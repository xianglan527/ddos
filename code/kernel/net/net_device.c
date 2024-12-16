#include "net_device.h"

#include "net.h"
#include "proc.h"
#include "stdio.h"
#include "mbox.h"
#include "exmsg.h"

static void recv_thread(void* arg) {
    cprintf("recv thread is running...\n");

    while (1) { 
        exmsg_netif_in();
        do_sleep(100); 
    }
}


static void xmit_thread(void* arg) {
    cprintf("xmit thread is running...\n");

    while (1) { do_sleep(100); }
}

int net_tx_rx_create(void){
    kernel_thread_init(recv_thread, nullptr);
    kernel_thread_init(xmit_thread, nullptr);
    return NET_OK;
}