#include "net.h"
#include "net_device.h"
#include "debug.h"

int net_device_init(void){
    net_tx_rx_create();
    return NET_OK;
}
#define DBG_TEST DBG_LEVEL_INFO
// #define DBG_TEST    DBG_LEVEL_WARNING
// #define DBG_TEST    DBG_LEVEL_ERROR
// #define DBG_TEST    DBG_LEVEL_NONE

void net_main(void *arg){
    net_init();
    net_start();
    net_device_init();
    dbg_info(DBG_TEST, "info");
    dbg_warning(DBG_TEST, "warning");
    dbg_error(DBG_TEST, "error");
    while(1);
}