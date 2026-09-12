/* wolfip_platform.c */
#include "pico/rand.h"
#include "wolfip.h"
#include "cyw43.h"
#include "cyw43_stats.h"

#include "wolf/tcp.h"

struct pbuf;
uint16_t pbuf_copy_partial(const struct pbuf *p, void *dataptr, uint16_t len, uint16_t offset)
{
    (void)p;
    (void)dataptr;
    (void)len;
    (void)offset;
    return 0;
}

/* Forward raw Ethernet frames received by CYW43 to wolfIP. */
void cyw43_cb_process_ethernet(void *cb_data, int itf, size_t len,
                               const uint8_t *buf)
{
    (void)cb_data;
    (void)itf;
    struct wolfIP *s = tcp_get_ipstack();
    if (s) {
        wolfIP_recv(s, (void *)buf, (uint32_t)len);
    }
}

/* The CYW43 driver requires these callbacks when lwIP is disabled. */
void cyw43_cb_tcpip_init(cyw43_t *self, int itf)
{
    (void)self;
    (void)itf;
}

void cyw43_cb_tcpip_deinit(cyw43_t *self, int itf)
{
    (void)self;
    (void)itf;
}

void cyw43_cb_tcpip_set_link_up(cyw43_t *self, int itf)
{
    (void)self;
    (void)itf;
}

void cyw43_cb_tcpip_set_link_down(cyw43_t *self, int itf)
{
    (void)self;
    (void)itf;
}

int cyw43_tcpip_link_status(cyw43_t *self, int itf)
{
    int s = cyw43_wifi_link_status(self, itf);
    if (s == CYW43_LINK_JOIN)
        return CYW43_LINK_UP;
    return s;
}
uint32_t wolfIP_getrandom(void)
{
    return get_rand_32();
}
