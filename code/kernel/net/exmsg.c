#include "exmsg.h"
#include "net.h"
#include "proc.h"
#include "stdio.h"
#include "mbox.h"
#include "slab.h"

Proc *net_work_thread = nullptr;
int net_work_thread_mbox_id = -1;

int exmsg_netif_in(void){
    static int i = 0;
    Mboxbuf __buf, *buf = &__buf;
    buf->size = sizeof(Exmsg);
    buf->len = buf->size;
    buf->data = kmalloc(sizeof(Exmsg));
    (*(Exmsg *)(buf->data)).id = i++;
    assert(net_work_thread_mbox_id != -1);
    int ret = ipc_mbox_send(net_work_thread_mbox_id, buf, 0);
    assert(ret == 0);
    kfree(buf->data);
    return 0;
}

int exmsg_init(void) {
    return NET_OK;
}

static void work_thread(void *arg){
    cprintf("%s exmsg is running....\n", (char *)arg);
    Mboxbuf __buf, *buf = &__buf;
    buf->size = sizeof(Exmsg);
    buf->len = buf->size;
    buf->data = kmalloc(sizeof(Exmsg));
    while(1){
        int ret = ipc_mbox_recv(net_work_thread_mbox_id, buf, 0);
        assert(buf->len == buf->size);
        Exmsg msg = (*(Exmsg *)(buf->data));
        cprintf("work_thead recv data is %d\n", msg.id);
        do_sleep(100);
    }
}

int exmsg_start(void){
    net_work_thread_mbox_id = ipc_mbox_init(EXMSG_MSG_CNT);
    net_work_thread = kernel_thread_init(work_thread, "work_thread");
    return NET_OK;
}