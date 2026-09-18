#ifndef WOLFSSH_PICO_PORT_H
#define WOLFSSH_PICO_PORT_H

#include <stdint.h>

#include "bsd_socket.h"

#define sockaddr wolfIP_sockaddr
#define sockaddr_in wolfIP_sockaddr_in

#ifndef INADDR_ANY
    #define INADDR_ANY 0U
#endif
#ifndef IPPROTO_TCP
    #define IPPROTO_TCP 6
#endif
#ifndef SOL_SOCKET
    #define SOL_SOCKET WOLFIP_SOL_SOCKET
#endif
#ifndef SO_REUSEADDR
    #define SO_REUSEADDR 2
#endif
#ifndef SO_NOSIGPIPE
    #define SO_NOSIGPIPE 0x1022
#endif

#ifndef htons
    #define htons(value) ee16(value)
#endif
#ifndef ntohs
    #define ntohs(value) ee16(value)
#endif

struct hostent {
    char* h_name;
    int h_length;
    char** h_addr_list;
};

static inline struct hostent* gethostbyname(const char* name)
{
    (void)name;
    return NULL;
}

static inline uint32_t inet_addr(const char* address)
{
    return atoip4(address);
}

#endif
