/* main_interactive.c - interactive wolfCrypt PUF demo over UART
 *
 * Captures the real power-on SRAM at reset, reports whether it is a usable
 * PUF source, and then offers an interactive menu over the UART:
 *
 *   1  enroll and show identity / derived key / helper size
 *   2  noise sweep - inject a known number of bit flips per codeword and
 *      show where BCH stops correcting (the "correction cliff")
 *   3  derive two unrelated keys from the same silicon (HKDF context)
 *   4  dump the helper data, which is public
 *   5  reconstruct from the stored helper and compare to enrollment
 *   r  soft reboot
 *
 * The noise sweep needs controllable error counts, which real SRAM cannot
 * provide, so the captured power-on pattern is replayed through
 * wc_PufSetTestData with a known number of flips applied. The bits are real
 * silicon; only the extra noise is synthetic.
 *
 * Lines are terminated with an explicit \r\n: the host behavioral test
 * (host_test/driver.py) matches literal CRLF, and on the hardware UART the
 * extra CR added by _write() is harmless.
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

#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/wolfcrypt/puf.h>
#include <wolfssl/wolfcrypt/error-crypt.h>

/* This demo drives PUF APIs added after the v5.9.2 stable release
 * (WC_PUF_RAW_STRIDE_BITS, wc_PufCheckSram, wc_PufGetParams,
 * wc_PufGetProfileId, wc_PufGetHelperData), so INTERACTIVE=1 needs wolfSSL
 * master. The one-shot example still builds against the stable release. */
#ifndef WC_PUF_RAW_STRIDE_BITS
#error "INTERACTIVE=1 requires wolfSSL master (post-v5.9.2 PUF API)"
#endif
#include <stdio.h>
#include <string.h>
#include <stdint.h>

extern void hal_init(void);
extern int  uart_getc(void);
extern void uart_drain(void);

static unsigned int helper_sum(const uint8_t* d, uint32_t len);
static unsigned int helper_sum_cont(unsigned int sum, const uint8_t* d,
    uint32_t len);

/* Raw power-on SRAM. NOLOAD section: startup must not zero it. Non-static
 * so the host behavioral test harness can seed it before main() runs. */
__attribute__((section(".puf_sram")))
volatile uint8_t puf_sram_region[WC_PUF_RAW_BYTES];

/* Snapshot taken before anything else can disturb the region. */
static uint8_t  g_raw[WC_PUF_RAW_BYTES];
static uint8_t  g_work[WC_PUF_RAW_BYTES];
static uint8_t  g_helper[WC_PUF_HELPER_BYTES];
static uint8_t  g_id[WC_PUF_ID_SZ];
static int      g_enrolled = 0;
/* The noise sweep injects exact flip counts against the readout the helper
 * was enrolled from, so it needs an enrollment taken from THIS boot's g_raw -
 * a blob loaded from a previous boot has an unknown natural flip baseline. */
static int      g_freshEnroll = 0;
static int      g_rawHealthy = 0;
static int      g_onesPct = 0;

/* Derived keys are never written to the UART by default: the console is an
 * unauthenticated physical interface. make SHOW_KEYS=1 opts in for lab use. */
static void print_key(const char* label, const uint8_t* key, uint32_t len)
{
#ifdef PUF_DEMO_SHOW_KEYS
    uint32_t i;
    printf("%s", label);
    for (i = 0; i < 16u && i < len; i++)
        printf("%02x", key[i]);
    printf("\r\n");
#else
    printf("%s%u bytes derived OK (not shown; build SHOW_KEYS=1 to "
           "display)\r\n", label, (unsigned int)len);
    (void)key;
#endif
}

static void print_hex(const char* label, const uint8_t* d, uint32_t len)
{
    uint32_t i;
    printf("%s", label);
    for (i = 0; i < len; i++)
        printf("%02x", d[i]);
    printf("\r\n");
}

static int ones_percent(const uint8_t* d, uint32_t len)
{
    uint32_t i;
    int b, ones = 0;
    for (i = 0; i < len; i++) {
        for (b = 0; b < 8; b++) {
            if (d[i] & (1u << b))
                ones++;
        }
    }
    return (int)((ones * 100u) / (len * 8u));
}

/* Flip 'flips' bits inside each codeword-sized stride of the pattern. */
static void add_noise(uint8_t* d, int flips)
{
    int cw, f, bit;
    int stride = WC_PUF_RAW_STRIDE_BITS;
    for (cw = 0; cw < WC_PUF_NUM_CODEWORDS; cw++) {
        for (f = 0; f < flips; f++) {
            bit = cw * stride + (f * 7) + 3;
            if ((bit / 8) < (int)WC_PUF_RAW_BYTES)
                d[bit / 8] ^= (uint8_t)(1u << (bit % 8));
        }
    }
}

/* Load a pattern into a fresh context and read it in. */
static int load_ctx(wc_PufCtx* ctx, const uint8_t* pattern)
{
    int ret = wc_PufInit(ctx);
    if (ret != 0)
        return ret;
    ret = wc_PufSetTestData(ctx, pattern, WC_PUF_RAW_BYTES);
    if (ret != 0)
        return ret;
    return wc_PufReadSram(ctx, pattern, WC_PUF_RAW_BYTES);
}

static int require_healthy(void)
{
    if (!g_rawHealthy) {
        printf("  the power-on readout failed the health check, so this is\r\n"
               "  disabled - deriving from anything else would produce a\r\n"
               "  device-independent key. Power-cycle the board (a warm\r\n"
               "  reset leaves old data in SRAM) and try again.\r\n");
        return 0;
    }
    return 1;
}

static void do_enroll(void)
{
    wc_PufCtx ctx;
    uint8_t key[WC_PUF_KEY_SZ];
    int ret;

    if (!require_healthy())
        return;
    ret = load_ctx(&ctx, g_raw);
    if (ret != 0) {
        printf("  readout rejected: %d\r\n", ret);
        wc_PufZeroize(&ctx);
        return;
    }
    ret = wc_PufEnroll(&ctx);
    if (ret != 0) {
        printf("  enroll failed: %d\r\n", ret);
        wc_PufZeroize(&ctx);
        return;
    }
    ret = wc_PufGetHelperData(&ctx, g_helper, sizeof(g_helper));
    if (ret == 0)
        ret = wc_PufGetIdentity(&ctx, g_id, sizeof(g_id));
    if (ret == 0)
        ret = wc_PufDeriveKey(&ctx, (const byte*)"nv-integrity", 12,
            key, sizeof(key));
    if (ret != 0) {
        printf("  enroll failed: %d\r\n", ret);
        wc_ForceZero(key, sizeof(key));
        wc_PufZeroize(&ctx);
        return;
    }

    printf("  enrolled from this boot's power-on SRAM readout\r\n");
    print_hex("  identity    : ", g_id, 16);
    print_key("  derived key : ", key, sizeof(key));
    printf("  helper data : %d bytes, stored in the clear\r\n",
        (int)sizeof(g_helper));
    g_enrolled = 1;
    g_freshEnroll = 1;

    wc_ForceZero(key, sizeof(key));
    wc_PufZeroize(&ctx);
}

static void do_sweep(void)
{
    wc_PufCtx ctx;
    uint8_t id[WC_PUF_ID_SZ];
    int flips, ret, m, n, k, t, cw;

    if (!g_freshEnroll) {
        printf("  run [1] enroll first - the sweep needs a helper enrolled\r\n"
               "  from this boot's readout so the injected flip counts are\r\n"
               "  exact (a loaded blob has an unknown natural flip baseline)\r\n");
        return;
    }
    wc_PufGetParams(&m, &n, &k, &t, &cw);
    printf("  BCH(%d,%d,t=%d), %d codewords - correcting up to %d flips per "
           "%d-bit codeword\r\n", n, k, t, cw, t, n);
    printf("  flips/codeword   result\r\n");

    for (flips = 0; flips <= t + 3; flips++) {
        XMEMCPY(g_work, g_raw, sizeof(g_work));
        add_noise(g_work, flips);
        ret = load_ctx(&ctx, g_work);
        if (ret == 0)
            ret = wc_PufReconstruct(&ctx, g_helper, sizeof(g_helper));
        if (ret == 0)
            ret = wc_PufGetIdentity(&ctx, id, sizeof(id));

        printf("  %2d               ", flips);
        if (ret != 0) {
            printf("rejected (%d) - fails closed", ret);
        }
        else if (XMEMCMP(id, g_id, sizeof(id)) == 0) {
            printf("identity matches");
        }
        else {
            printf("WRONG KEY - would be a bug");
        }
        if (flips == t)
            printf("   <= t, the limit");
        printf("\r\n");
        wc_PufZeroize(&ctx);
    }
}

static void do_two_keys(void)
{
    wc_PufCtx ctx;
    uint8_t k1[WC_PUF_KEY_SZ], k2[WC_PUF_KEY_SZ];
    int ret;

    if (!g_enrolled) {
        printf("  run [1] enroll first\r\n");
        return;
    }
    if (!require_healthy())
        return;
    ret = load_ctx(&ctx, g_raw);
    if (ret == 0)
        ret = wc_PufReconstruct(&ctx, g_helper, sizeof(g_helper));
    if (ret != 0) {
        printf("  need an enrollment first ([1]), rc=%d\r\n", ret);
        wc_PufZeroize(&ctx);
        return;
    }
    ret = wc_PufDeriveKey(&ctx, (const byte*)"nv-integrity", 12,
        k1, sizeof(k1));
    if (ret == 0)
        ret = wc_PufDeriveKey(&ctx, (const byte*)"device-identity", 15,
            k2, sizeof(k2));
    if (ret != 0) {
        printf("  key derivation failed: %d\r\n", ret);
        wc_ForceZero(k1, sizeof(k1));
        wc_ForceZero(k2, sizeof(k2));
        wc_PufZeroize(&ctx);
        return;
    }
    printf("  same silicon, same helper data, two HKDF contexts:\r\n");
    print_key("  \"nv-integrity\"    : ", k1, sizeof(k1));
    print_key("  \"device-identity\" : ", k2, sizeof(k2));
    printf("  unrelated keys - one PUF backs as many as you need\r\n");
    wc_ForceZero(k1, sizeof(k1));
    wc_ForceZero(k2, sizeof(k2));
    wc_PufZeroize(&ctx);
}

static void do_dump_helper(void)
{
    uint32_t i;
    unsigned int sum;

    if (!g_enrolled) {
        printf("  run [1] enroll first\r\n");
        return;
    }
    printf("  Public recovery blob: device identity, %d bytes of helper\r\n"
           "  data, then a 2-byte checksum over both. Triple-click the\r\n"
           "  single line below and copy it. After a power cycle, [5]\r\n"
           "  pastes it back and checks itself, so there is nothing to\r\n"
           "  write down.\r\n\r\n",
        (int)WC_PUF_HELPER_BYTES);
    for (i = 0; i < (uint32_t)WC_PUF_ID_SZ; i++) {
        printf("%02x", g_id[i]);
    }
    for (i = 0; i < (uint32_t)WC_PUF_HELPER_BYTES; i++) {
        printf("%02x", g_helper[i]);
    }
    sum = helper_sum(g_id, (uint32_t)WC_PUF_ID_SZ);
    sum = helper_sum_cont(sum, g_helper, (uint32_t)WC_PUF_HELPER_BYTES);
    printf("%04x\r\n\r\n", sum);
    printf("  none of this is secret - it reveals nothing about the key, and\r\n"
           "  on another die it reconstructs nothing\r\n");
}

/* Small checksum so a mangled paste is reported as such rather than surfacing
 * as a confusing reconstruct failure. The blob checksum covers the identity
 * and the helper data together. */
static unsigned int helper_sum_cont(unsigned int sum, const uint8_t* d,
    uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++) {
        sum = ((sum << 5) ^ (sum >> 11) ^ d[i]) & 0xFFFFu;
    }
    return sum;
}

static unsigned int helper_sum(const uint8_t* d, uint32_t len)
{
    return helper_sum_cont(0xFFFFu, d, len);
}

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Read helper data back in as pasted hex and reconstruct from it. The helper
 * is public, so it can be carried out of the device and back in over the wire.
 * Pasting it after a power cycle shows the key rebuilt from silicon that has
 * just been re-read, with nothing secret ever leaving the part. */
static uint8_t g_blob[WC_PUF_ID_SZ + WC_PUF_HELPER_BYTES + 2];

static void do_load_helper(void)
{
    wc_PufCtx ctx;
    uint8_t id[WC_PUF_ID_SZ];
    uint8_t k1[WC_PUF_KEY_SZ], k2[WC_PUF_KEY_SZ];
    unsigned int sum, expect;
    int c, v, hi = -1, ret, match;
    uint32_t n = 0;

    if (!require_healthy())
        return;
    printf("  paste the recovery blob from [4]; q aborts.\r\n");
    printf("  nothing is echoed while pasting.\r\n");

    /* Terminator-free: a triple-click selection carries no trailing newline,
     * so finish as soon as the blob is complete. Whitespace is ignored;
     * anything else non-hex means the selection caught prose, so discard and
     * resynchronise rather than shifting the stream by a nibble.
     *
     * g_blob is only a staging buffer: nothing is committed to the enrolled
     * state (g_id / g_helper / g_enrolled) until the checksum verifies, the
     * reconstruct succeeds, AND the identity matches. Every failure path
     * leaves any previous enrollment untouched. */
    while (n < (uint32_t)sizeof(g_blob)) {
        c = uart_getc();
        if (c == 'q' || c == 'Q' || c == 27) {
            printf("  aborted\r\n");
            return;
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            continue;
        v = hexval(c);
        if (v < 0) {
            n = 0;
            hi = -1;
            continue;
        }
        if (hi < 0) {
            hi = v;
        }
        else {
            g_blob[n++] = (uint8_t)((hi << 4) | v);
            hi = -1;
        }
    }

    /* Verify the trailing checksum (over identity + helper) before anything
     * else, so a mangled paste is reported as exactly that. */
    sum = helper_sum(g_blob, (uint32_t)WC_PUF_ID_SZ);
    sum = helper_sum_cont(sum, g_blob + WC_PUF_ID_SZ,
        (uint32_t)WC_PUF_HELPER_BYTES);
    expect = ((unsigned int)g_blob[WC_PUF_ID_SZ + WC_PUF_HELPER_BYTES] << 8) |
              (unsigned int)g_blob[WC_PUF_ID_SZ + WC_PUF_HELPER_BYTES + 1];
    if (sum != expect) {
        printf("  checksum mismatch (got %04x, blob says %04x) - the paste\r\n"
               "  was mangled; nothing was changed, copy the line again\r\n",
            sum, expect);
        return;
    }
    printf("  loaded identity + %d bytes of helper data, checksum %04x OK\r\n",
        (int)WC_PUF_HELPER_BYTES, sum);

    ret = load_ctx(&ctx, g_raw);
    if (ret == 0)
        ret = wc_PufReconstruct(&ctx, g_blob + WC_PUF_ID_SZ,
            WC_PUF_HELPER_BYTES);
    if (ret == 0)
        ret = wc_PufGetIdentity(&ctx, id, sizeof(id));
    if (ret == 0)
        ret = wc_PufDeriveKey(&ctx, (const byte*)"nv-integrity", 12,
            k1, sizeof(k1));
    if (ret == 0)
        ret = wc_PufDeriveKey(&ctx, (const byte*)"device-identity", 15,
            k2, sizeof(k2));
    if (ret != 0) {
        printf("  reconstruct failed: %d\r\n", ret);
        printf("  either the blob is from a different part, or the readout\r\n"
               "  drifted past the correction budget; nothing was changed\r\n");
    }
    else {
        match = (XMEMCMP(id, g_blob, WC_PUF_ID_SZ) == 0);
        print_hex("  identity now      : ", id, 16);
        print_hex("  identity enrolled : ", g_blob, 16);
        printf("\r\n  >>> %s <<<\r\n\r\n", match ?
            "SAME KEY, REBUILT FROM SILICON AFTER POWER LOSS" :
            "MISMATCH - this blob does not belong to this part");
        if (match) {
            print_key("  \"nv-integrity\"    : ", k1, sizeof(k1));
            print_key("  \"device-identity\" : ", k2, sizeof(k2));
            /* Commit only now: verified, reconstructed, and matching. */
            XMEMCPY(g_id, g_blob, WC_PUF_ID_SZ);
            XMEMCPY(g_helper, g_blob + WC_PUF_ID_SZ, WC_PUF_HELPER_BYTES);
            g_enrolled = 1;
            /* Not enrolled from this boot's readout - the sweep stays off. */
            g_freshEnroll = 0;
        }
        else {
            printf("  nothing was changed\r\n");
        }
    }
    wc_ForceZero(k1, sizeof(k1));
    wc_ForceZero(k2, sizeof(k2));
    wc_PufZeroize(&ctx);
}

static void menu(void)
{
    printf("\r\n  [1] enroll and show identity / key / helper\r\n");
    printf("  [2] noise sweep - the correction cliff\r\n");
    printf("  [3] two keys from one PUF\r\n");
    printf("  [4] dump the public recovery blob (identity + helper)\r\n");
    printf("  [5] paste the blob back after a power cycle, and verify\r\n");
    printf("  [r] reboot (soft reset - SRAM is NOT re-randomised)\r\n");
    printf("  [?] this menu\r\n");
}

int main(void)
{
    int m, n, k, t, cw, c;

    /* Snapshot the power-on SRAM before anything else can touch it. */
    XMEMCPY(g_raw, (const void*)puf_sram_region, sizeof(g_raw));

    hal_init();
    c = wolfCrypt_Init();
    if (c != 0) {
        printf("ERROR: wolfCrypt_Init failed: %d\r\n", c);
        for (;;) { }
    }

    g_onesPct = ones_percent(g_raw, sizeof(g_raw));
    g_rawHealthy = (wc_PufCheckSram(g_raw, sizeof(g_raw), NULL) == 0);

    wc_PufGetParams(&m, &n, &k, &t, &cw);
    printf("\r\n=== wolfCrypt PUF - interactive demo ===\r\n");
    printf("  profile     : BCH(%d,%d,t=%d) over GF(2^%d), %d codewords, "
           "id 0x%08lX\r\n", n, k, t, m, cw,
           (unsigned long)wc_PufGetProfileId());
    printf("  power-on SRAM readout: %d bytes, %d%% ones -> %s\r\n",
        (int)sizeof(g_raw), g_onesPct,
        g_rawHealthy ? "inside the health band" :
                       "REJECTED by the health band");
    if (!g_rawHealthy) {
        printf("  this region has no usable power-on entropy on this boot,\r\n"
               "  so enrollment and key derivation are disabled - deriving\r\n"
               "  from anything else would produce a device-independent\r\n"
               "  key. Power-cycle the board (a warm reset leaves old data\r\n"
               "  in SRAM) and try again.\r\n");
    }
    else {
        printf("  the readout passed the SRAM health checks; only a genuine\r\n"
               "  power cycle establishes that it is fresh power-on entropy\r\n");
    }

    /* Drop any line noise latched in the receiver before prompting. */
    uart_drain();

    menu();

    for (;;) {
        printf("\r\n> ");
        c = uart_getc();
        printf("%c\r\n", (char)c);
        switch (c) {
            case '1': do_enroll();       break;
            case '2': do_sweep();        break;
            case '3': do_two_keys();     break;
            case '4': do_dump_helper();  break;
            case '5': do_load_helper();  break;
            case 'r':
            case 'R':
                printf("  rebooting...\r\n\r\n");
                /* AIRCR: VECTKEY 0x5FA | SYSRESETREQ */
                *(volatile uint32_t*)0xE000ED0Cu = 0x05FA0004u;
                for (;;) { }
            default:  menu();            break;
        }
    }
}
