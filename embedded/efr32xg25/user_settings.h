/* user_settings.h
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

/* wolfCrypt configuration for the EFR32xG25 Secure Element crypto callback
 * port. Used by both the library and the application, as required. */

#ifndef WOLFSSL_XG25_USER_SETTINGS_H
#define WOLFSSL_XG25_USER_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Silicon Labs Secure Element -------------------------------------- */
/* Routes operations through the crypto callback framework by devId. This one
 * define also turns on WOLF_CRYPTO_CB, registers the device from
 * wolfCrypt_Init() and points WC_USE_DEVID at it, so the stock wolfCrypt test
 * and benchmark exercise the hardware. */
#define WOLFSSL_SILABS_CRYPTOCB

/* Optional: TLS 1.3 handshake and record layer test. Adds a full handshake and
 * application data exchange, both peers on the device, everything routed to
 * the Secure Element - buffers at arbitrary offsets and one cipher object
 * reused across many records, which is where alignment or carried-state
 * problems show up. See tls13_test.c.
 *
 * Turning this on links the TLS sources, which are already in the project and
 * compile to nothing while it is off.
 */
#if 0
    #define WOLFSSL_XG25_TLS13
#endif

/* ---- Platform ---------------------------------------------------------- */
#ifdef WOLFSSL_XG25_TLS13
    #define WOLFSSL_TLS13
    #define WOLFSSL_NO_TLS12
    #define HAVE_TLS_EXTENSIONS
    #define HAVE_SUPPORTED_CURVES
    #define WOLFSSL_USER_IO
    #define NO_DH
    /* RSA stays compiled in for the wolfCrypt test vectors, and TLS 1.3
     * requires PSS for any RSA signature. */
    #define WC_RSA_PSS
#else
    /* This project exercises wolfCrypt only; no TLS is linked. */
    #define WOLFCRYPT_ONLY
#endif
#define NO_OLD_TLS
#define WOLFSSL_GENERAL_ALIGNMENT 4
/* newlib-nano has no strcasecmp/strncasecmp; use wolfCrypt's own. */
#define USE_WOLF_STRCASECMP
#define USE_WOLF_STRNCASECMP
#define SINGLE_THREADED
#define NO_FILESYSTEM
#define NO_WRITEV
#define WOLFSSL_NO_SOCK
#define NO_MAIN_DRIVER
#define BENCH_EMBEDDED
#define WOLFSSL_SMALL_STACK
#define WOLFSSL_USER_CURRTIME
#define USE_CERT_BUFFERS_256

/* Optional: Thumb2 assembly for the symmetric and hash software paths. The SE
 * handles these algorithms, so this only changes what the software fallback
 * costs - and what the benchmark's SW column reports. The thumb2-*-asm_c.c
 * sources are already in the project and compile to nothing while it is off.
 *
 * WOLFSSL_ARM_ARCH=7 selects the ARMv7-M/Thumb2 encodings the Cortex-M33 runs;
 * WOLFSSL_ARMASM_NO_HW_CRYPTO is required because the M33 has no ARMv8
 * cryptography extensions and no NEON.
 */
#if 0
    #define WOLFSSL_ARMASM
    #define WOLFSSL_ARMASM_THUMB2
    #define WOLFSSL_ARMASM_INLINE
    #define WOLFSSL_ARMASM_NO_HW_CRYPTO
    #define WOLFSSL_ARMASM_NO_NEON
    #define WOLFSSL_ARM_ARCH 7
#endif

/* ---- Hardening ---------------------------------------------------------
 * What ./configure --enable-harden turns on, and its default for good reason:
 * timing-attack resistance on the paths handling private keys, at some
 * software cost. The SE is unaffected, but anything it declines falls back to
 * these software implementations.
 *
 * TFM_TIMING_RESISTANT applies only to the fastmath backend, which this
 * configuration does not use (see WOLFSSL_SP_MATH_ALL below), but is set so
 * the setting survives a change of math backend.
 */
#define ECC_TIMING_RESISTANT
#define WC_RSA_BLINDING
#define TFM_TIMING_RESISTANT

/* Cortex-M33 single precision assembly for the software fallback paths. */
#define WOLFSSL_SP_MATH_ALL
#define WOLFSSL_HAVE_SP_RSA
#define WOLFSSL_HAVE_SP_ECC
#define WOLFSSL_SP_ARM_CORTEX_M_ASM
#define WOLFSSL_SP_SMALL

/* ---- Algorithms offloaded to the SE ------------------------------------ */
#define HAVE_AES_ECB
#define WOLFSSL_AES_DIRECT
#define WOLFSSL_AES_COUNTER
#define HAVE_AESGCM
#define HAVE_AESCCM
#define WOLFSSL_AES_192
#define WOLFSSL_AES_256
#define WOLFSSL_CMAC
#define WOLFSSL_SHA224
#define WOLFSSL_SHA384
#define WOLFSSL_SHA512
#define HAVE_HKDF
#define HAVE_PBKDF2
#define HAVE_CHACHA
#define HAVE_POLY1305
#define HAVE_ECC
#define ECC_SHAMIR
#define HAVE_ECC384
#define HAVE_ECC521
#define ALT_ECC_SIZE

/* ---- Trimmed away to fit and to keep the run short --------------------- */
#define NO_DSA
#define NO_DES3
#define NO_RC4
#define NO_MD4
#define NO_MD5
#define NO_PSK
#define NO_PWDBASED_TEST_LONG
#define WOLFSSL_NO_SHAKE128
#define WOLFSSL_NO_SHAKE256

/* The Series 2 High Security Engine has no RSA hardware. RSA stays in
 * software; leave it on for the test vectors but keep the key size modest. */
#define RSA_MAX_SIZE 2048

#ifdef __cplusplus
}
#endif

#endif /* WOLFSSL_XG25_USER_SETTINGS_H */
