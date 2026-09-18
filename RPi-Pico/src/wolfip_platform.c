#include "pico/rand.h"
#include "wolfip.h"
#include "cyw43.h"

#include "wolf/tcp.h"

static volatile uint8_t cyw43_link_up[2];

struct pbuf;
uint16_t pbuf_copy_partial(const struct pbuf *p, void *dataptr, uint16_t len, uint16_t offset)
{
    (void)p;
    (void)dataptr;
    (void)len;
    (void)offset;
    return 0;
}

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

void cyw43_cb_tcpip_init(cyw43_t *self, int itf)
{
    (void)self;
    if (itf >= 0 && itf < (int)(sizeof(cyw43_link_up) /
            sizeof(cyw43_link_up[0]))) {
        cyw43_link_up[itf] = 0;
    }
}

void cyw43_cb_tcpip_deinit(cyw43_t *self, int itf)
{
    (void)self;
    if (itf >= 0 && itf < (int)(sizeof(cyw43_link_up) /
            sizeof(cyw43_link_up[0]))) {
        cyw43_link_up[itf] = 0;
    }
}

void cyw43_cb_tcpip_set_link_up(cyw43_t *self, int itf)
{
    (void)self;
    if (itf >= 0 && itf < (int)(sizeof(cyw43_link_up) /
            sizeof(cyw43_link_up[0]))) {
        cyw43_link_up[itf] = 1;
    }
}

void cyw43_cb_tcpip_set_link_down(cyw43_t *self, int itf)
{
    (void)self;
    if (itf >= 0 && itf < (int)(sizeof(cyw43_link_up) /
            sizeof(cyw43_link_up[0]))) {
        cyw43_link_up[itf] = 0;
    }
}

int cyw43_tcpip_link_status(cyw43_t *self, int itf)
{
    if (itf >= 0 && itf < (int)(sizeof(cyw43_link_up) /
            sizeof(cyw43_link_up[0])) && cyw43_link_up[itf]) {
        return CYW43_LINK_UP;
    }
    return cyw43_wifi_link_status(self, itf);
}
uint32_t wolfIP_getrandom(void)
{
    return get_rand_32();
}
