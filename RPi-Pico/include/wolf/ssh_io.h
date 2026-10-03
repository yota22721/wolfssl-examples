#ifndef WOLF_PICO_SSH_IO_H
#define WOLF_PICO_SSH_IO_H

#include <wolfssh/ssh.h>

int wolfSshBsdIORecv(WOLFSSH* ssh, void* buf, word32 sz, void* ctx);
int wolfSshBsdIOSend(WOLFSSH* ssh, void* buf, word32 sz, void* ctx);

#endif
