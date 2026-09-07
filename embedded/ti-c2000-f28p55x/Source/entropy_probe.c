/* entropy_probe.c
 *
 * Copyright (C) 2006-2026 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/* Raw entropy-source characterization for the TMS320F28P550SJ.
 *
 * `make ENTROPY_PROBE=1`.  A MEASUREMENT tool, not part of the RNG: it dumps
 * unconditioned samples over SCI so a host can estimate min-entropy before
 * anything is wired into wc_GenerateSeed().
 *
 * Candidates: DCC oscillator jitter (a DCC counts PLL edges inside a window of
 * INTOSC cycles, leaving the drift between two independent oscillators in the
 * low bits) and ADC LSB noise (floating high-Z input, short acquisition so the
 * SAR never settles).
 *
 * Tagged hex output for the host analyzer:
 *   E0/E1 <N>  DCC1 INTOSC1 / DCC0 INTOSC2 window, PLL counted, N cycles
 *   E3 0       ADCC raw 12-bit results
 *   E4/E5/E6   packed LSB streams (8 samples per emitted octet)
 *   PROBE DONE
 */

#include <stdio.h>
#include <stdint.h>

#include "driverlib.h"
#include "device.h"

#ifdef WOLF_ENTROPY_PROBE

/* Counter1 counts down from 0xFFFFF, capping the window near 34,900 INTOSC
 * cycles at 300 MHz PLL / 10 MHz INTOSC; keep well under that. */
#define PROBE_CNT1_SEED   0xFFFFFUL

/* A sample taken after a DCC ERROR flag or a guard-loop timeout is not noise,
 * it is a misconfigured clock mux or a stalled counter.  Count both so a
 * capture with any nonzero total can be rejected rather than analysed. */
static uint32_t probeDccErrors;
static uint32_t probeDccTimeouts;
static uint32_t probeAdcTimeouts;
#define PROBE_SAMPLES     1024
/* printf over SCI dominates; keep the ADC set smaller. */
#define PROBE_ADC_SAMPLES 1024
#define PROBE_PER_LINE    16
/* Packed-LSB stream size.  32 KiB = 262144 bits per source, enough for the
 * MCV confidence bound to stop being the limiting factor. */
#define PROBE_PACKED_BYTES 32768UL

/* Window sweep (slow-clock cycles).  Entropy per sample grows with the window
 * while the rate falls, so the useful entropy rate peaks in the middle. */
static const uint32_t probeWindows[] = { 256UL, 1024UL, 4096UL };
#define PROBE_NUM_WINDOWS (sizeof(probeWindows) / sizeof(probeWindows[0]))


/* One DCC measurement, at register level: DCC_measureClockFrequency() uses
 * float32_t, which does not belong here. */
static uint32_t probe_dccSample(uint32_t base, DCC_Count0ClockSource src0,
                                DCC_Count1ClockSource src1, uint32_t window)
{
    uint32_t guard;

    DCC_clearErrorFlag(base);
    DCC_clearDoneFlag(base);
    DCC_disableModule(base);
    DCC_disableErrorSignal(base);
    DCC_disableDoneSignal(base);

    DCC_setCounter0ClkSource(base, src0);
    DCC_setCounter1ClkSource(base, src1);
    DCC_setCounterSeeds(base, window, DCC_VALIDSEED_MIN, PROBE_CNT1_SEED);
    DCC_enableSingleShotMode(base, DCC_MODE_COUNTER_ZERO);

    /* DONE only latches with the done/error signals enabled - driverlib's own
     * DCC_measureClockFrequency() does this and it is easy to miss. */
    DCC_enableErrorSignal(base);
    DCC_enableDoneSignal(base);

    DCC_enableModule(base);

    /* Bounded wait, scaled to the window, so a bad mux cannot hang.  Record
     * why we stopped: only a clean done-signal yields a usable sample. */
    for (guard = 0; guard < (window * 256UL) + 100000UL; guard++) {
        if (DCC_getSingleShotStatus(base) || DCC_getErrorStatus(base)) {
            break;
        }
    }
    if (DCC_getErrorStatus(base)) {
        probeDccErrors++;
    }
    else if (!DCC_getSingleShotStatus(base)) {
        probeDccTimeouts++;
    }

    return (PROBE_CNT1_SEED - (DCC_getCounter1Value(base) & PROBE_CNT1_SEED));
}


static void probe_dccInit(void)
{
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_DCC0);
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_DCC1);
    SysCtl_delay(100);
}


static void probe_adcInit(void)
{
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_ADCC);
    SysCtl_delay(100);

    ASysCtl_setAnalogReferenceInternal(ASYSCTL_ANAREF_INTREF_ADCC);

    ADC_setPrescaler(ADCC_BASE, ADC_CLK_DIV_4_0);
    ADC_setInterruptPulseMode(ADCC_BASE, ADC_PULSE_END_OF_CONV);
    ADC_enableConverter(ADCC_BASE);
    DEVICE_DELAY_US(1000);

    /* Short acquisition on a floating high-Z input: the SAR deliberately does
     * not settle, which is where the noise comes from. */
    ADC_setupSOC(ADCC_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_SW_ONLY,
                 ADC_CH_ADCIN0, 8U);
    ADC_setInterruptSource(ADCC_BASE, ADC_INT_NUMBER1, ADC_SOC_NUMBER0);
    ADC_enableInterrupt(ADCC_BASE, ADC_INT_NUMBER1);
    ADC_clearInterruptStatus(ADCC_BASE, ADC_INT_NUMBER1);
}


static uint16_t probe_adcSample(void)
{
    uint32_t guard;

    ADC_clearInterruptStatus(ADCC_BASE, ADC_INT_NUMBER1);
    ADC_forceSOC(ADCC_BASE, ADC_SOC_NUMBER0);

    for (guard = 0; guard < 1000000UL; guard++) {
        if (ADC_getInterruptStatus(ADCC_BASE, ADC_INT_NUMBER1)) {
            break;
        }
    }
    if (guard >= 1000000UL) {
        probeAdcTimeouts++;
    }

    return ADC_readResult(ADCCRESULT_BASE, ADC_SOC_NUMBER0);
}


static void probe_dumpDcc(const char* tag, uint32_t base,
                          DCC_Count0ClockSource src0,
                          DCC_Count1ClockSource src1, uint32_t window)
{
    uint32_t i;

    for (i = 0; i < PROBE_SAMPLES; i++) {
        if ((i % PROBE_PER_LINE) == 0) {
            printf("\r\n%s %lu ", tag, (unsigned long)window);
        }
        printf("%05lx ",
               (unsigned long)probe_dccSample(base, src0, src1, window));
    }
    printf("\r\n");
}


/* Packed LSB stream.  A useful min-entropy estimate needs far more samples
 * than a 115200 UART can carry one hex count at a time, so the bit extraction
 * happens on-target: 8 samples per emitted octet.  This is also the stream a
 * real entropy source consumes, so it is the right thing to assess. */
static void probe_dumpPackedDcc(const char* tag, uint32_t base,
                                DCC_Count0ClockSource src0,
                                DCC_Count1ClockSource src1, uint32_t window,
                                uint32_t nbytes)
{
    uint32_t i;
    int b;
    uint16_t acc;

    for (i = 0; i < nbytes; i++) {
        if ((i % 32U) == 0U) {
            printf("\r\n%s %lu ", tag, (unsigned long)window);
        }
        acc = 0U;
        for (b = 0; b < 8; b++) {
            acc = (uint16_t)(acc |
                (uint16_t)((probe_dccSample(base, src0, src1, window) & 1U)
                           << b));
        }
        printf("%02x ", (unsigned int)(acc & 0xFFU));
    }
    printf("\r\n");
}


static void probe_dumpPackedAdc(const char* tag, uint32_t nbytes)
{
    uint32_t i;
    int b;
    uint16_t acc;

    for (i = 0; i < nbytes; i++) {
        if ((i % 32U) == 0U) {
            printf("\r\n%s 0 ", tag);
        }
        acc = 0U;
        for (b = 0; b < 8; b++) {
            acc = (uint16_t)(acc |
                (uint16_t)((probe_adcSample() & 1U) << b));
        }
        printf("%02x ", (unsigned int)(acc & 0xFFU));
    }
    printf("\r\n");
}


void entropy_probe_run(void)
{
    uint32_t w;
    uint32_t i;

    printf("\r\n=== ENTROPY PROBE ===\r\n");
    /* %lu, not %d: int is 16 bits here and PROBE_PACKED_BYTES is 32768. */
    printf("SYSCLK %lu Hz, samples/config %lu, packed stream %lu octets\r\n",
           (unsigned long)DEVICE_SYSCLK_FREQ, (unsigned long)PROBE_SAMPLES,
           (unsigned long)PROBE_PACKED_BYTES);

    probe_dccInit();
    probe_adcInit();

    for (w = 0; w < PROBE_NUM_WINDOWS; w++) {
        probe_dumpDcc("E0", DCC1_BASE, DCC_COUNT0SRC_INTOSC1,
                      DCC_COUNT1SRC_PLL, probeWindows[w]);
        probe_dumpDcc("E1", DCC0_BASE, DCC_COUNT0SRC_INTOSC2,
                      DCC_COUNT1SRC_PLL, probeWindows[w]);
    }

    for (i = 0; i < PROBE_ADC_SAMPLES; i++) {
        if ((i % PROBE_PER_LINE) == 0) {
            printf("\r\nE3 0 ");
        }
        printf("%05lx ", (unsigned long)probe_adcSample());
    }
    printf("\r\n");

    /* Packed LSB streams for the real min-entropy assessment. */
    probe_dumpPackedDcc("E4", DCC1_BASE, DCC_COUNT0SRC_INTOSC1,
                        DCC_COUNT1SRC_PLL, 256UL, PROBE_PACKED_BYTES);
    probe_dumpPackedDcc("E5", DCC0_BASE, DCC_COUNT0SRC_INTOSC2,
                        DCC_COUNT1SRC_PLL, 256UL, PROBE_PACKED_BYTES);
    probe_dumpPackedAdc("E6", PROBE_PACKED_BYTES);

    printf("\r\nDCC errors %lu, DCC timeouts %lu, ADC timeouts %lu\r\n",
           (unsigned long)probeDccErrors, (unsigned long)probeDccTimeouts,
           (unsigned long)probeAdcTimeouts);
    printf("\r\nPROBE DONE\r\n");
}

#endif /* WOLF_ENTROPY_PROBE */
