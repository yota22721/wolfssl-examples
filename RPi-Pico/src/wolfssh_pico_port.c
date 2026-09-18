#include <sys/select.h>

int select(int nfds, fd_set* readSet, fd_set* writeSet, fd_set* errorSet,
           struct timeval* timeout)
{
    (void)nfds;
    (void)writeSet;
    (void)errorSet;
    (void)timeout;

    return readSet != NULL ? 1 : 0;
}
