#include "net.h"
#include "exmsg.h"

int net_init(void){
    return NET_OK;
}

int net_start(void){
    exmsg_start();
    return NET_OK;
}