/* ssh.h
 *
 * Copyright (C) 2006-2026 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WOLF_PICO_SSH_H
#define WOLF_PICO_SSH_H

#include <wolfssh/ssh.h>

int wolfSshBsdIORecv(WOLFSSH* ssh, void* buf, word32 sz, void* ctx);
int wolfSshBsdIOSend(WOLFSSH* ssh, void* buf, word32 sz, void* ctx);

#endif /* WOLF_PICO_SSH_H */
