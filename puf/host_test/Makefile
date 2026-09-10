# Host behavioral test for the interactive PUF demo: builds
# main_interactive.c with a stdio HAL (harness.c) and drives the menu with
# driver.py, covering enrollment, the noise sweep, blob dump/recovery,
# checksum and identity-mismatch rejection, abort, and the fail-closed
# unhealthy-readout path. Requires wolfSSL master (same as INTERACTIVE=1).
WOLFSSL_ROOT ?= ../../../wolfssl
CC ?= gcc
CFLAGS = -Wall -Og -g -DWOLFSSL_USER_SETTINGS -DWOLFSSL_PUF_TEST
CFLAGS += -I.. -I$(WOLFSSL_ROOT)

SRC = ../main_interactive.c harness.c
SRC += $(WOLFSSL_ROOT)/wolfcrypt/src/puf.c
SRC += $(WOLFSSL_ROOT)/wolfcrypt/src/sha256.c
SRC += $(WOLFSSL_ROOT)/wolfcrypt/src/kdf.c
SRC += $(WOLFSSL_ROOT)/wolfcrypt/src/hmac.c
SRC += $(WOLFSSL_ROOT)/wolfcrypt/src/hash.c
SRC += $(WOLFSSL_ROOT)/wolfcrypt/src/memory.c
SRC += $(WOLFSSL_ROOT)/wolfcrypt/src/wc_port.c
SRC += $(WOLFSSL_ROOT)/wolfcrypt/src/error.c
SRC += $(WOLFSSL_ROOT)/wolfcrypt/src/misc.c
SRC += $(WOLFSSL_ROOT)/wolfcrypt/src/logging.c
SRC += $(WOLFSSL_ROOT)/wolfcrypt/src/random.c
SRC += $(WOLFSSL_ROOT)/wolfcrypt/src/sp_int.c
SRC += $(WOLFSSL_ROOT)/wolfcrypt/src/sha3.c

puf_host_test: $(SRC) ../user_settings.h
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

run: puf_host_test
	python3 driver.py ./puf_host_test

clean:
	rm -f puf_host_test

.PHONY: run clean
