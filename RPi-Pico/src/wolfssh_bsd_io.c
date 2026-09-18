#include "bsd_socket.h"
#include <wolfssh/error.h>
#include "wolf/ssh.h"

int wolfSshBsdIORecv(WOLFSSH* ssh, void* buf, word32 sz, void* ctx)
{
    int received;
    int socketFd;

    (void)ssh;

    if (buf == NULL || ctx == NULL)
        return WS_CBIO_ERR_GENERAL;

    socketFd = *(int*)ctx;
    received = recv(socketFd, buf, sz, 0);
    if (received > 0)
        return received;
    if (received == 0)
        return WS_CBIO_ERR_CONN_CLOSE;
    if (socket_last_error() == WOLFIP_EAGAIN)
        return WS_CBIO_ERR_WANT_READ;

    return WS_CBIO_ERR_GENERAL;
}

int wolfSshBsdIOSend(WOLFSSH* ssh, void* buf, word32 sz, void* ctx)
{
    int sent;
    int socketFd;

    (void)ssh;

    if (buf == NULL || ctx == NULL)
        return WS_CBIO_ERR_GENERAL;

    socketFd = *(int*)ctx;
    sent = send(socketFd, buf, sz, 0);
    if (sent >= 0)
        return sent;
    if (socket_last_error() == WOLFIP_EAGAIN)
        return WS_CBIO_ERR_WANT_WRITE;

    return WS_CBIO_ERR_GENERAL;
}
