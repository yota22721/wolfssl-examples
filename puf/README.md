# wolfCrypt SRAM PUF Example

Bare-metal example demonstrating SRAM PUF (Physically Unclonable Function)
on Cortex-M targets (tested on NUCLEO-H563ZI).

## Overview

SRAM PUF exploits the random power-on state of SRAM memory cells to derive
device-unique cryptographic keys. Each chip has a unique SRAM "fingerprint"
caused by manufacturing variations.

This example demonstrates:

1. **Enrollment** - Read raw SRAM, generate helper data using BCH(127,64,t=10)
   error-correcting codes
2. **Reconstruction** - Re-read (noisy) SRAM, use helper data to recover the
   same stable bits despite bit flips (corrects up to 10 per 127-bit codeword)
3. **Key derivation** - Use HKDF-SHA256 to derive a 256-bit cryptographic key
4. **Device identity** - SHA-256 hash of stable bits serves as a unique device ID

## Building

### Requirements

- `arm-none-eabi-gcc` toolchain

### Test Mode (default)

Uses synthetic SRAM data -- runs on any target, no real hardware needed:

```bash
make
```

### Real Hardware Mode

Test mode is the default and is selected by the Makefile (not the
header). To build for the real SRAM PUF on hardware, override the
`PUF_TEST` variable:

```bash
make PUF_TEST=0
```

This drops the `-DWOLFSSL_PUF_TEST` define and includes `puf_sram_region`
(placed in the `.puf_sram` NOLOAD section) so `wc_PufReadSram()` reads
the real power-on SRAM contents.

**Only a real power cycle gives a real readout.** A warm reset - the reset
button, a debugger reset, or `-rst` after flashing - leaves SRAM holding
whatever the previous image left there. That stale content can still pass the
Hamming-weight health band, so the example will happily enroll from it and
report a plausible-looking identity that has nothing to do with the silicon.
Pull power (or unplug USB) between enrollment and reconstruction when you want
to exercise the PUF itself.

Measured on a NUCLEO-H563ZI: a cold-boot readout is about 51-52% ones, well
inside the default 35-65% band, and reconstruction recovers the enrolled
identity unchanged across a physical power cycle - so this part's SRAM noise
stays within the BCH t=10 correction budget. Immediately after a warm reset the
same board reported 20% ones and was correctly rejected with `PUF_READ_E`.

### Interactive Mode

The interactive demo has a host-side behavioral test: `host_test/` builds
`main_interactive.c` against a stdio HAL and `driver.py` drives the menu end
to end (enrollment, the sweep gate and correction cliff, blob dump and
recovery, checksum and identity-mismatch rejection, paste abort, and the
fail-closed unhealthy-readout path). CI runs it on every change; locally:
`make -C host_test WOLFSSL_ROOT=/path/to/wolfssl run`.

```bash
make INTERACTIVE=1
```

Requires wolfSSL master (the demo uses PUF APIs added after v5.9.2; the build
stops with a clear `#error` on older trees). Output goes to
`Build-interactive/` so switching modes never reuses stale objects.

Derived keys are never written to the UART by default - the console is an
unauthenticated physical interface, so the demo prints a "derived OK (not
shown)" status instead. For lab work where seeing the key bytes matters,
`make INTERACTIVE=1 SHOW_KEYS=1` opts in explicitly.

If the power-on readout fails the Hamming-weight health band (which is what a
warm reset looks like, since SRAM keeps the previous image's data), the demo
fails closed: enrollment, reconstruction, and key derivation are disabled
until a genuine power cycle provides a real readout. Nothing is ever derived
from a substitute pattern.

Builds `main_interactive.c` instead of the one-shot example: a UART menu that
captures the real power-on SRAM at reset, reports whether it passed the readout
health band, and then lets you drive the extractor a step at a time.

```
=== wolfCrypt PUF - interactive demo ===
  profile     : BCH(127,64,t=10) over GF(2^7), 16 codewords, id 0x38500010
  power-on SRAM readout: 256 bytes, 44% ones -> inside the health band

  [1] enroll and show identity / key / helper
  [2] noise sweep - the correction cliff
  [3] two keys from one PUF
  [4] dump the public recovery blob (identity + helper)
  [5] paste the blob back after a power cycle, and verify
  [r] reboot (soft reset - SRAM is NOT re-randomised)
```

Option 2 is the interesting one: it injects a known number of bit flips per
codeword and shows exactly where BCH stops correcting.

```
  flips/codeword   result
   9               identity matches
  10               identity matches   <= t, the limit
  11               rejected (-1012) - fails closed
```

Controlled error counts are not something real SRAM can provide, so the captured
power-on pattern is replayed through `wc_PufSetTestData()` with the flips
applied - the bits are real silicon, only the extra noise is synthetic. That is
why `INTERACTIVE=1` implies `PUF_TEST=1`.

Options 4 and 5 show what helper data is for, across a real power cycle and with
no non-volatile storage involved. `4` prints one line holding the device
identity, the helper data, and a trailing checksum over both. Copy it, power-cycle the
board, then paste it back with `5`: it verifies the checksum (a mangled
paste is reported as such and changes nothing), reconstructs from freshly
re-read silicon, and compares against the identity carried in the blob, so the
board reports the result itself rather than leaving you to compare hex by eye. Nothing secret
leaves the part - the helper is public, which is why it can travel out over the
wire and back in again.

The reader ignores whitespace, needs no trailing newline, and discards its
accumulation if it sees any non-hex text, so a selection that catches the
surrounding prose still loads correctly. `q` aborts.

Note that a soft reset does **not** re-randomise SRAM. Only a real power cycle
produces a fresh power-on readout.

### Output

Build output is placed in `./Build/`:
- `puf_example.elf` - Loadable ELF binary
- `puf_example.hex` - Intel HEX for flash programmers

## Flashing

### OpenOCD (NUCLEO-H563ZI)

```bash
openocd -f interface/stlink.cfg -f target/stm32h5x.cfg \
    -c "program Build/puf_example.elf verify reset exit"
```

When multiple ST-Links are connected, specify the serial number:

```bash
openocd -f interface/stlink.cfg \
    -c "adapter serial <YOUR_SERIAL>" \
    -f target/stm32h5x.cfg \
    -c "program Build/puf_example.elf verify reset exit"
```

## UART Output

Connect to the board's UART (typically 115200 baud) to see output:

```
--- wolfCrypt SRAM PUF Example ---

PUF initialized.
Mode: TEST (synthetic SRAM data)

Enrollment complete.
Identity (enrollment): 3ad99904f92897bad1a21bc9cbc3ab8f2dc4bc40dfe6e161c741f98ef8dd7e01
Derived key (enrollment): aa8573f70a3253ca567500bdcd610face6a140e5fc68047e02d3f13958dcc480

--- Simulating power cycle (noisy SRAM) ---

Reconstruction complete (BCH corrected noisy bits).
Identity (reconstructed): 3ad99904f92897bad1a21bc9cbc3ab8f2dc4bc40dfe6e161c741f98ef8dd7e01
PASS: Identity matches after reconstruction.
Derived key (reconstructed): aa8573f70a3253ca567500bdcd610face6a140e5fc68047e02d3f13958dcc480
PASS: Derived key matches after reconstruction.

--- PUF example complete ---
```

## Customizing for Your MCU

### Linker Script

Edit `linker.ld` to match your MCU's memory map:

```ld
MEMORY
{
    FLASH   (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K  /* your flash size */
    RAM     (rwx) : ORIGIN = 0x20000000, LENGTH = 636K   /* total - PUF_RAM */
    PUF_RAM (rw)  : ORIGIN = 0x2009F000, LENGTH = 4K     /* end of SRAM */
}
```

The `PUF_RAM` region must be at the end of SRAM and marked `NOLOAD` so the
startup code does not zero it.

### Architecture

Edit the `ARCHFLAGS` in `Makefile`:

```makefile
ARCHFLAGS = -mcpu=cortex-m33 -mthumb
```

## API Usage

```c
wc_PufCtx ctx;
uint8_t helperData[WC_PUF_HELPER_BYTES];
uint8_t key[WC_PUF_KEY_SZ];

/* First boot: Enroll */
wc_PufInit(&ctx);
wc_PufReadSram(&ctx, sram_addr, sram_size);
wc_PufEnroll(&ctx);
memcpy(helperData, ctx.helperData, WC_PUF_HELPER_BYTES);
/* Store helperData to flash/NVM (it is NOT secret) */

/* Subsequent boots: Reconstruct */
wc_PufInit(&ctx);
wc_PufReadSram(&ctx, sram_addr, sram_size);
wc_PufReconstruct(&ctx, helperData, WC_PUF_HELPER_BYTES);
wc_PufDeriveKey(&ctx, info, infoSz, key, sizeof(key));

/* Always zeroize when done */
wc_PufZeroize(&ctx);
```

## Security Notes

- **Helper data is public** - It does not reveal the key. Safe to store
  unencrypted in flash or transmit over the network.
- **SRAM must not be accessed before PUF read** - Any read or write to the
  PUF SRAM region before `wc_PufReadSram()` will corrupt the power-on entropy.
- **Production RNG** - This example wires wolfCrypt's RNG through
  `CUSTOM_RAND_GENERATE_BLOCK` (in `user_settings.h`) to
  `custom_rand_gen_block()` in `stm32.c`, which uses the STM32H5 RNG
  peripheral over HSI48. When porting to another MCU, replace the
  implementation behind `custom_rand_gen_block()` (or remap
  `CUSTOM_RAND_GENERATE_BLOCK` to your platform's hardware RNG hook).

## Reproducing on the m33mu Emulator

The m33mu Cortex-M33 emulator can simulate cold-boot SRAM and seeded
noise so the BCH reconstruction path can be exercised without rebooting
real hardware:

```bash
# Deterministic SRAM, boot 0 - enrolls and reconstructs cleanly
m33mu --puf-seed 0xDEADBEEF --puf-cold-boot 0 Build/puf_example.elf

# Same seed/boot with 2 bit flips per 127-bit codeword - within BCH(t=10)
m33mu --puf-seed 0xDEADBEEF --puf-cold-boot 0 --puf-noise 2 \
      Build/puf_example.elf
```

Identity must match between the enrollment and reconstruction prints as
long as noise stays within the BCH correction budget (10 flips per
127-bit codeword; safe margin 2-4).
