#include "net.h"
#include "exmsg.h"
#include "pktbuf.h"
#include "netif.h"
#include "loop.h"
#include "ether.h"
#include "nettool.h"
#include "arp.h"

int net_init(void){
    dbg_info(DBG_INIT, "init net...");
    tools_init();
    exmsg_init();
    pktbuf_init();
    netif_init();
    loop_init();
    ether_init();
    arp_init();
    return NET_OK;
}

int net_start(void){
    exmsg_start();
    dbg_info(DBG_INIT, "net is running.");
    return NET_OK;
}