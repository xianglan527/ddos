#include "exmsg.h"

#include "error.h"
#include "ipv4.h"
#include "mbox.h"
#include "net.h"
#include "net_config.h"
#include "proc.h"
#include "slab.h"
#include "stdio.h"
#include "string.h"

Proc *net_work_thread = nullptr;
int net_work_thread_mbox_id = -1;
static int msg_i = 0;

int test_func(Func_msg *msg) {
    msg->err = 0x1234;
    cprintf("hello, 1234: %x\n", *(int *)msg->param);
    return NET_OK;
}

int exmsg_netif_in(Netif *netif) {
    Mboxbuf __buf, *buf = &__buf;
    buf->size = sizeof(Exmsg);
    buf->len = buf->size;
    buf->data = kmalloc(sizeof(Exmsg));
    Exmsg *msg = (Exmsg *)(buf->data);
    msg->id = msg_i++;
    msg->type = NET_EXMSG_NETIF_IN;
    msg->netif.netif = netif;
    assert(net_work_thread_mbox_id != -1);
    int ret = ipc_mbox_send(net_work_thread_mbox_id, buf, -1);
    assert(ret == 0 || ret == -E_MBX_FULL || ret == -1);
    if (ret < 0){
        if(ret == -E_MBX_FULL){
            dbg_warning(DBG_MSG, "work thread mbox full");
        }
        else{
            dbg_error(DBG_MSG, "send work thread mbox failed");
        }
        kfree(buf->data);
        return ret;
    }
    kfree(buf->data);
    return ret;
}

static int do_netif_in(Exmsg *msg) {
    Netif *netif = msg->netif.netif;
    Pktbuf *buf;
    int ret;
    while ((buf = netif_get_in(netif, -1))) {
        dbg_info(DBG_MSG, "recv a packet");
        if (netif->link_layer) {
            ret = netif->link_layer->in(netif, buf);
            if (ret < 0) {
                pktbuf_free(buf);
                dbg_warning(DBG_MSG, "netif in failed. err=%d", ret);
            }
        } else {
            ret = ipv4_in(netif, buf);
            if (ret < 0) {
                pktbuf_free(buf);
                dbg_warning(DBG_MSG, "netif in failed. err=%d", ret);
            };
        }
    }
    return NET_OK;
}

int exmsg_init(void) {
    net_work_thread_mbox_id = ipc_mbox_init(EXMSG_MSG_CNT);
    return NET_OK;
}

int exmsg_func_exec(Exmsg_func func, void *param) {
    Func_msg func_msg;
    func_msg.proc = myproc();
    func_msg.func = func;
    func_msg.param = param;
    func_msg.err = NET_OK;
    sem_init(&func_msg.wait_sem, 0);
    Mboxbuf __buf, *buf = &__buf;
    buf->size = sizeof(Exmsg);
    buf->len = buf->size;
    buf->data = kmalloc(sizeof(Exmsg));
    Exmsg *msg = (Exmsg *)(buf->data);
    msg->id = msg_i++;
    msg->type = NET_EXMSG_FUN;
    msg->func = &func_msg;
    dbg_info(DBG_MSG, "1.begin call func: %p", func);
    assert(net_work_thread_mbox_id != -1);
    int ret = ipc_mbox_send(net_work_thread_mbox_id, buf, 0);
    assert(ret == 0 || ret == -1);
    if (ret < 0) {
        dbg_warning(DBG_MSG, "send func msg to work thread mbox failed");
        kfree(buf->data);
        return ret;
    }
    down(&func_msg.wait_sem);
    dbg_info(DBG_MSG, "4.end call func: %p", func);
    kfree(buf->data);
    return func_msg.err;
}

static int do_func(Func_msg *func_msg) {
    dbg_info(DBG_MSG, "2.calling func");
    func_msg->err = func_msg->func(func_msg);
    up(&func_msg->wait_sem);
    dbg_info(DBG_MSG, "3.func exec complete");
    return NET_OK;
}

static void work_thread(void *arg) {
    cprintf("%s exmsg is running....\n", (char *)arg);
    Mboxbuf __buf, *buf = &__buf;
    buf->size = sizeof(Exmsg);
    buf->len = buf->size;
    buf->data = kmalloc(sizeof(Exmsg));
    while (1) {
        int ret = ipc_mbox_recv(net_work_thread_mbox_id, buf, -1);
        assert(ret == 0 || ret == -E_TIMEOUT || ret == -E_MBX_EMPTY);
        if (ret == -E_TIMEOUT || ret == -E_MBX_EMPTY) {
            do_sleep(50);
            continue;
        }
        assert(buf->len == buf->size);
        Exmsg *msg = (Exmsg *)(buf->data);
        dbg_info(DBG_MSG, "recieve a msg(%p): %d : %d", msg, msg->id, msg->type);
        switch (msg->type) {
            case NET_EXMSG_NETIF_IN:  // 网络接口消息
                do_netif_in(msg);
                break;
            case NET_EXMSG_FUN: 
                do_func(msg->func);
                break;
            default: break;
        }
        memset(buf->data, 0, buf->size);
    }
}

int exmsg_start(void) {
    net_work_thread = kernel_thread_init(work_thread, "work_thread");
    return NET_OK;
}

