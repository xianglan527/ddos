#include "net.h"
#include "exmsg.h"
#include "pktbuf.h"

int net_init(void){
    dbg_info(DBG_INIT, "init net...");
    exmsg_init();
    pktbuf_init();
    return NET_OK;
}

int net_start(void){
    exmsg_start();
    dbg_info(DBG_INIT, "net is running.");
    return NET_OK;
}