# Makefile for wolfCrypt SRAM PUF bare-metal example
#
# Target: Cortex-M (tested on NUCLEO-H563ZI)
# Usage:  make            - build puf_example.elf and puf_example.hex
#         make clean      - remove build artifacts
#         make PUF_TEST=0 - build for real hardware SRAM (no test mode)
#
# Copyright (C) 2006-2026 wolfSSL Inc.

# Toolchain
TOOLCHAIN ?= arm-none-eabi-
CC = $(TOOLCHAIN)gcc
LD = $(TOOLCHAIN)gcc
AR = $(TOOLCHAIN)ar
OBJCOPY = $(TOOLCHAIN)objcopy
OBJDUMP = $(TOOLCHAIN)objdump
SIZE = $(TOOLCHAIN)size
NM = $(TOOLCHAIN)nm

# wolfSSL root (relative to this directory)
WOLFSSL_ROOT ?= ../../wolfssl

# Build output. Each configuration gets its own directory so switching
# INTERACTIVE / PUF_TEST between invocations can never relink stale objects
# from the previous configuration (the default build stays in ./Build).
BIN = puf_example

# PUF test mode (default on): synthetic SRAM data for testing without hardware.
# Set PUF_TEST=0 to build for real hardware SRAM.
PUF_TEST ?= 1

# Set INTERACTIVE=1 to build the UART menu demo (main_interactive.c) instead of
# the one-shot example. It captures the real power-on SRAM and then replays it
# through wc_PufSetTestData so a known number of bit flips can be injected, so
# it needs the test hooks compiled in: PUF_TEST is forced on (override beats a
# contradictory PUF_TEST=0 on the command line). Requires wolfSSL master (the
# demo uses PUF APIs added after v5.9.2; the build stops with a clear #error
# on older trees).
INTERACTIVE ?= 0
ifeq ($(INTERACTIVE),1)
override PUF_TEST := 1
endif

# The interactive demo never writes derived keys to the UART by default (the
# console is unauthenticated). SHOW_KEYS=1 opts in for lab use.
SHOW_KEYS ?= 0

# Architecture
ARCHFLAGS = -mcpu=cortex-m33 -mthumb -mabi=aapcs

# Compiler flags
CFLAGS = $(ARCHFLAGS) -std=gnu99 -Wall -Os
CFLAGS += -ffunction-sections -fdata-sections -fno-builtin
CFLAGS += -DWOLFSSL_USER_SETTINGS
CFLAGS += -I. -I$(WOLFSSL_ROOT)

ifeq ($(PUF_TEST),1)
CFLAGS += -DWOLFSSL_PUF_TEST
endif

BUILD_SUFFIX =
ifeq ($(INTERACTIVE),1)
BUILD_SUFFIX := $(BUILD_SUFFIX)-interactive
ifeq ($(SHOW_KEYS),1)
CFLAGS += -DPUF_DEMO_SHOW_KEYS
BUILD_SUFFIX := $(BUILD_SUFFIX)-showkeys
endif
endif
ifeq ($(PUF_TEST),0)
BUILD_SUFFIX := $(BUILD_SUFFIX)-hw
endif
BUILD_DIR ?= ./Build$(BUILD_SUFFIX)

# Linker flags
LDFLAGS = $(ARCHFLAGS)
LDFLAGS += --specs=nosys.specs --specs=nano.specs
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=$(BUILD_DIR)/$(BIN).map
LDFLAGS += -T./linker.ld

# Math lib
LIBS = -lm

# Source files
ifeq ($(INTERACTIVE),1)
SRC_C  = main_interactive.c
else
SRC_C  = main.c
endif
SRC_C += startup.c
SRC_C += stm32.c

# wolfCrypt sources needed for PUF
SRC_C += $(WOLFSSL_ROOT)/wolfcrypt/src/puf.c
SRC_C += $(WOLFSSL_ROOT)/wolfcrypt/src/sha256.c
SRC_C += $(WOLFSSL_ROOT)/wolfcrypt/src/kdf.c
SRC_C += $(WOLFSSL_ROOT)/wolfcrypt/src/hmac.c
SRC_C += $(WOLFSSL_ROOT)/wolfcrypt/src/hash.c
SRC_C += $(WOLFSSL_ROOT)/wolfcrypt/src/memory.c
SRC_C += $(WOLFSSL_ROOT)/wolfcrypt/src/wc_port.c
SRC_C += $(WOLFSSL_ROOT)/wolfcrypt/src/error.c
SRC_C += $(WOLFSSL_ROOT)/wolfcrypt/src/misc.c
SRC_C += $(WOLFSSL_ROOT)/wolfcrypt/src/logging.c
SRC_C += $(WOLFSSL_ROOT)/wolfcrypt/src/random.c
SRC_C += $(WOLFSSL_ROOT)/wolfcrypt/src/sp_int.c
SRC_C += $(WOLFSSL_ROOT)/wolfcrypt/src/sha3.c

# Object files
FILENAMES_C = $(notdir $(SRC_C))
OBJS_C = $(addprefix $(BUILD_DIR)/, $(FILENAMES_C:.c=.o))
vpath %.c $(dir $(SRC_C))

# Targets
.PHONY: all clean

all: $(BUILD_DIR) $(BUILD_DIR)/$(BIN).hex
	@echo ""
	$(SIZE) $(BUILD_DIR)/$(BIN).elf

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c
	@echo "Compiling: $(notdir $<)"
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/$(BIN).elf: $(OBJS_C)
	@echo "Linking: $(notdir $@)"
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo ""
	$(NM) -n $@ > $(BUILD_DIR)/$(BIN).sym
	$(OBJDUMP) -S $@ > $(BUILD_DIR)/$(BIN).disasm

$(BUILD_DIR)/$(BIN).hex: $(BUILD_DIR)/$(BIN).elf
	@echo "Generating HEX: $(notdir $@)"
	$(OBJCOPY) -O ihex $< $@

clean:
	rm -rf ./Build ./Build-interactive ./Build-interactive-showkeys ./Build-hw
	$(MAKE) -C host_test clean
