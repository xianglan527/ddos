#include "loop.h"
#include "assert.h"
#include "debug.h"

static int loop_open(Netif* netif, void* ops_data) {
    netif->type = NETIF_TYPE_LOOP;
    return NET_OK;
}

static void loop_close(Netif* netif) {}

static int loop_xmit(Netif* netif) {
    Pktbuf* pktbuf = netif_get_out(netif, 10);
    if(pktbuf){
        return netif_put_in(netif, pktbuf, 10);
    }
    return NET_OK;
}

static Netif_ops loop_driver = {
    .open = loop_open,
    .close = loop_close,
    .xmit = loop_xmit,
};

int loop_init(void){
    dbg_info(DBG_NETIF, "init loop");
    Netif *netif = netif_open("loop", &loop_driver, nullptr);
    assert(netif != nullptr);
    Ipaddr ip, mask;
    ipaddr_from_str(&ip, "127.0.0.1");
    ipaddr_from_str(&mask, "255.0.0.0");
    netif_set_addr(netif, &ip, &mask, nullptr);
    netif_set_active(netif);
    dbg_info(DBG_NETIF, "init done");
    return NET_OK;
}