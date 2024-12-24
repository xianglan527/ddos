#include "exmsg.h"
#include "net.h"
#include "proc.h"
#include "stdio.h"
#include "mbox.h"
#include "slab.h"
#include "error.h"
#include "net_config.h"
#include "string.h"

Proc *net_work_thread = nullptr;
int net_work_thread_mbox_id = -1;

int exmsg_netif_in(Netif *netif){
    static int i = 0;
    Mboxbuf __buf, *buf = &__buf;
    buf->size = sizeof(Exmsg);
    buf->len = buf->size;
    buf->data = kmalloc(sizeof(Exmsg));
    Exmsg *msg = (Exmsg *)(buf->data);
    msg->id = i++;
    msg->type = NET_EXMSG_NETIF_IN;
    msg->netif.netif = netif;
    assert(net_work_thread_mbox_id != -1);
    int ret = ipc_mbox_send(net_work_thread_mbox_id, buf, 10);
    assert(ret == 0 || ret == -E_TIMEOUT);
    if(ret == -E_TIMEOUT){
        dbg_warning(DBG_MSG, "work thread mbox full");
        return ret;
    }
    kfree(buf->data);
    return NET_OK;
}

static int do_netif_in(Exmsg *msg) {
    Netif *netif = msg->netif.netif;
    Pktbuf *buf;
    int ret;
    while ((buf = netif_get_in(netif, 10))) {
        dbg_info(DBG_MSG, "recv a packet");
        if(netif->link_layer){
            ret = netif->link_layer->in(netif, buf);
            if(ret < 0){
                pktbuf_free(buf);
                dbg_warning(DBG_MSG, "netif in failed. err=%d", ret);
            }
        }else{
            pktbuf_free(buf);
        }
    }
    return NET_OK;
}

int exmsg_init(void) {
    net_work_thread_mbox_id = ipc_mbox_init(EXMSG_MSG_CNT);
    return NET_OK;
}

static void work_thread(void *arg){
    cprintf("%s exmsg is running....\n", (char *)arg);
    Mboxbuf __buf, *buf = &__buf;
    buf->size = sizeof(Exmsg);
    buf->len = buf->size;
    buf->data = kmalloc(sizeof(Exmsg));
    while(1){
        int ret = ipc_mbox_recv(net_work_thread_mbox_id, buf, 5);
        assert(ret == 0 || ret == -E_TIMEOUT);
        if(ret == -E_TIMEOUT) {
            // do_sleep(50);
            continue;
        }
        assert(buf->len == buf->size);
        Exmsg *msg = (Exmsg *)(buf->data);
        dbg_info(DBG_MSG, "recieve a msg(%p): %d : %d", msg, msg->id, msg->type);
        switch (msg->type) {
            case NET_EXMSG_NETIF_IN:  // 网络接口消息
                do_netif_in(msg);
                break;
        }
        memset(buf->data, 0, buf->size);
    }
}

int exmsg_start(void){
    net_work_thread = kernel_thread_init(work_thread, "work_thread");
    return NET_OK;
}