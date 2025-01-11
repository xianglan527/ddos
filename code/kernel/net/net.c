#include "net.h"

#include "arp.h"
#include "ether.h"
#include "exmsg.h"
#include "icmpv4.h"
#include "ipv4.h"
#include "loop.h"
#include "netif.h"
#include "nettool.h"
#include "pktbuf.h"
#include "sock.h"
#include "raw.h"

int net_init(void){
    dbg_info(DBG_INIT, "init net...");
    tools_init();
    exmsg_init();
    pktbuf_init();
    netif_init();
    loop_init();
    ether_init();
    arp_init();
    ipv4_init();
    icmpv4_init();
    sockets_init();
    raws_init();
    return NET_OK;
}

int net_start(void){
    exmsg_start();
    dbg_info(DBG_INIT, "net is running.");
    return NET_OK;
}