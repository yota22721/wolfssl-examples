/* harness.c - host-side stand-ins for the interactive PUF demo's HAL so the
 * menu logic, blob parsing, and fail-closed paths can be exercised without
 * hardware. The UART becomes stdio; the PUF region is seeded with a balanced
 * deterministic pattern before main() runs, or left all-zero (which fails the
 * Hamming-weight health band) when PUF_HOST_UNHEALTHY is set.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/wolfcrypt/puf.h>

extern volatile uint8_t puf_sram_region[WC_PUF_RAW_BYTES];

__attribute__((constructor))
static void seed_region(void)
{
    uint32_t x = 0x12345678u;
    unsigned int i;

    if (getenv("PUF_HOST_UNHEALTHY") != NULL) {
        return; /* all-zero: rejected by the health band */
    }
    for (i = 0; i < (unsigned int)WC_PUF_RAW_BYTES; i++) {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        puf_sram_region[i] = (uint8_t)x;
    }
}

void hal_init(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
}

int uart_getc(void)
{
    int c = getchar();
    if (c == EOF) {
        exit(0);
    }
    return c;
}

void uart_drain(void)
{
}

int custom_rand_gen_block(unsigned char* output, unsigned int sz)
{
    static uint32_t x = 0xA5A5A5A5u;
    unsigned int i;

    for (i = 0; i < sz; i++) {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        output[i] = (unsigned char)x;
    }
    return 0;
}
