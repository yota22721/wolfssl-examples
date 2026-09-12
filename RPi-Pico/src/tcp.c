/* tcp.c
 *
 * Copyright (C) 2006-2025 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

#include <stdio.h>
#include "pico/cyw43_arch.h"
#include "arch_freertos.h"

#include "wolfip.h"
#include "bsd_socket.h"

#include "wolf/tcp.h"

static struct wolfIP *ipstack = NULL;

struct wolfIP* tcp_get_ipstack(void)
{
    return ipstack;
}

static int pico_nic_send(struct wolfIP_ll_dev *ll, void *buf, uint32_t len)
{
    int ret;

    (void)ll;

    if (buf == NULL || len == 0U)
        return -1;

    ret = cyw43_send_ethernet(&cyw43_state, CYW43_ITF_STA, len,
                              (const void *)buf, false);
    if (ret != 0)
        return ret;

    return (int)len;
}
static void print_netinfo(void)
{
    if (ipstack != NULL) {
        ip4 ip, mask, gw;
        char buf[16];
        wolfIP_ipconfig_get(ipstack, &ip, &mask, &gw);
        iptoa(ip, buf);
        printf("IP Addr: %s\n", buf);
        iptoa(mask, buf);
        printf("Netmask: %s\n", buf);
        iptoa(gw, buf);
        printf("Gateway: %s\n", buf);
    }
    else {
        printf("Network interface not found.\n");
    }
}

int tcp_initThread(void)
{
    int ret;

    wolfIP_init_static(&ipstack);
    if (ipstack == NULL) {
        fprintf(stderr, "ERROR: failed to initialize wolfIP\n");
        return -1;
    }

    struct wolfIP_ll_dev *dev = wolfIP_getdev(ipstack);
    if (dev == NULL) {
        fprintf(stderr, "ERROR: failed to get wolfIP network device\n");
        return -1;
    }
    dev->send = pico_nic_send;
    ret = cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, dev->mac);
    if (ret != 0) {
        fprintf(stderr, "ERROR: failed to read CYW43 MAC address (%d)\n", ret);
        return ret;
    }

    ip4 ip   = atoip4("192.168.10.79");
    ip4 mask = atoip4("255.255.255.0");
    ip4 gw   = atoip4("192.168.10.1");
    wolfIP_ipconfig_set(ipstack, ip, mask, gw);

    ret = wolfip_freertos_socket_init(ipstack, CYW43_TASK_PRIORITY, 1024);
    if (ret != 0) {
        fprintf(stderr, "ERROR: failed to initialize wolfIP sockets (%d)\n", ret);
        return ret;
    }

    print_netinfo();
    printf("wolfIP initialized\n");
    return 0;
}
