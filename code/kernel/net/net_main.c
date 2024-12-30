#include "assert.h"
#include "debug.h"
#include "net.h"
#include "net_device.h"
#include "pktbuf.h"
#include "string.h"
#include "printf.h"
#include "proc.h"
#include "stdio.h"

extern Netif_ops netdev_ops;
extern List_entry nets_list;

typedef struct {
    char *name;
    char *ip;
    char *mask;
    char *gw;
} NetConfig;

NetConfig net_configs[] = {
    {"net0", "192.168.74.3", "255.255.255.0", "192.168.74.1"},
    {"net1", "192.168.74.4", "255.255.255.0", "192.168.74.1"},
};

char *friend0_ip = "192.168.74.2";

NetConfig *get_net_config(const char *net_name) {
    for (int i = 0; i < sizeof(net_configs) / sizeof(NetConfig); i++) {
        if (net_configs[i].name && strcmp(net_configs[i].name, net_name) == 0) { return &net_configs[i]; }
    }
    return nullptr;
}

int net_device_init(void) {
    List_entry *le;
    list_for_each(le, &nets_list) {
        Virtio_net *net = le2virtio_net(le);
        Netif *netif = netif_open(nullptr, &netdev_ops, net);
        assert(netif != nullptr);
        snprintf(netif->netif_name, sizeof(netif->netif_name), "%sif", net->net_name);
        NetConfig *config = get_net_config(net->net_name);
        if (config == nullptr) {
            warn("No configuration found for %s\n", net->net_name);
            return -E_NET_CONFIG;
        }
        Ipaddr ip, mask, gw;
        ipaddr_from_str(&ip, config->ip);
        ipaddr_from_str(&mask, config->mask);
        ipaddr_from_str(&gw, config->gw);
        netif_set_addr(netif, &ip, &mask, &gw);
        netif_set_active(netif);
        Pktbuf *buf = pktbuf_alloc(32);
        pktbuf_fill(buf, 0x53, 32);

        Ipaddr dest;
        ipaddr_from_str(&dest, friend0_ip);
        netif_out(netif, &dest, buf);

        ipaddr_from_str(&dest, "192.168.74.255");
        buf = pktbuf_alloc(32);
        pktbuf_fill(buf, 0xA5, buf->total_size);
        netif_out(netif, &dest, buf);
    }
    return NET_OK;
}
#define DBG_TEST DBG_LEVEL_INFO
// #define DBG_TEST    DBG_LEVEL_WARNING
// #define DBG_TEST    DBG_LEVEL_ERROR
// #define DBG_TEST    DBG_LEVEL_NONE

void pktbuf_test(void) {
    Pktbuf *buf = pktbuf_alloc(2000);
    pktbuf_free(buf);
    buf = pktbuf_alloc(2000);
    for (int i = 0; i < 16; i++) { pktbuf_add_header(buf, 66, true); }
    for (int i = 0; i < 10; i++) { pktbuf_remove_header(buf, 88); }
    for (int i = 0; i < 16; i++) { pktbuf_add_header(buf, 66, false); }
    for (int i = 0; i < 10; i++) { pktbuf_remove_header(buf, 88); }
    pktbuf_free(buf);

    buf = pktbuf_alloc(0);
    pktbuf_resize(buf, 32);
    pktbuf_resize(buf, 527);
    pktbuf_resize(buf, 4922);
    pktbuf_resize(buf, 1921);
    pktbuf_resize(buf, 288);
    pktbuf_resize(buf, 32);
    pktbuf_resize(buf, 0);
    pktbuf_free(buf);

    buf = pktbuf_alloc(689);
    Pktbuf *sbuf = pktbuf_alloc(892);
    pktbuf_join(buf, sbuf);
    pktbuf_free(buf);

    buf = pktbuf_alloc(32);
    pktbuf_join(buf, pktbuf_alloc(4));
    pktbuf_join(buf, pktbuf_alloc(16));
    pktbuf_join(buf, pktbuf_alloc(54));
    pktbuf_join(buf, pktbuf_alloc(32));
    pktbuf_join(buf, pktbuf_alloc(38));
    pktbuf_set_cont(buf, 44);
    pktbuf_set_cont(buf, 60);
    pktbuf_set_cont(buf, 64);
    pktbuf_set_cont(buf, 128);
    pktbuf_set_cont(buf, 176);
    // pktbuf_set_cont(buf, 513);
    pktbuf_free(buf);

    buf = pktbuf_alloc(1024);
    pktbuf_join(buf, pktbuf_alloc(4));
    pktbuf_join(buf, pktbuf_alloc(16));
    pktbuf_join(buf, pktbuf_alloc(54));
    pktbuf_join(buf, pktbuf_alloc(32));
    pktbuf_join(buf, pktbuf_alloc(38));
    pktbuf_join(buf, pktbuf_alloc(512));
    pktbuf_reset_acc(buf);
    static uint16_t temp[1000];
    for (int i = 0; i < 1000; i++) { temp[i] = i; }
    pktbuf_write(buf, (uint8_t *)temp, buf->total_size);
    static uint16_t read_temp[1000];
    memset(read_temp, 0, sizeof(read_temp));
    pktbuf_reset_acc(buf);
    pktbuf_read(buf, (uint8_t *)read_temp, buf->total_size);
    assert(memcmp(temp, read_temp, buf->total_size) == 0);

    memset(read_temp, 0, sizeof(read_temp));
    pktbuf_seek(buf, 85 * 2);
    pktbuf_read(buf, (uint8_t *)read_temp, 512);
    assert(memcmp(temp + 85, read_temp, 512) == 0);

    Pktbuf *dest = pktbuf_alloc(1024);
    pktbuf_seek(buf, 200);
    pktbuf_seek(dest, 300);
    pktbuf_copy(dest, buf, 522);

    memset(read_temp, 0, sizeof(read_temp));
    pktbuf_seek(dest, 300);
    pktbuf_read(dest, (uint8_t *)read_temp, 522);
    assert(memcmp(temp + 100, read_temp, 512) == 0);

    pktbuf_seek(dest, 0);
    pktbuf_fill(dest, 53, dest->total_size);
    memset(read_temp, 0, sizeof(read_temp));
    pktbuf_seek(dest, 0);
    pktbuf_read(dest, (uint8_t *)read_temp, dest->total_size);
    for (int i = 0; i < dest->total_size; i++) { assert(((uint8_t *)read_temp)[i] == 53); }

    pktbuf_free(dest);
    pktbuf_free(buf);
}

void timer0_func(Timer *timer) {
    static int count = 1;
    cprintf("this is %s: %d\n", (char *)timer->arg, count++);
}

void timer1_func(Timer *timer) {
    static int count = 1;
    cprintf("this is %s: %d\n", (char *)timer->arg, count++);
}

void timer2_func(Timer *timer) {
    static int count = 1;
    cprintf("this is %s: %d\n", (char *)timer->arg, count++);
}

void timer3_func(Timer *timer) {
    static int count = 1;
    cprintf("this is %s: %d\n", (char *)timer->arg, count++);
}

void timer_func_test(){
    Timer *t0 = timer_func_add(timer0_func, "t0", 200, 0);
    Timer *t1 = timer_func_add(timer1_func, "t1", 500, TIMER_RELOAD);
    Timer *t2 = timer_func_add(timer2_func, "t2", 500, TIMER_RELOAD);
    Timer *t3 = timer_func_add(timer3_func, "t3", 1000, TIMER_RELOAD);
    del_func_timer(t1);
}

void net_test() { 
    pktbuf_test();
    timer_func_test();
}

void net_main(void *arg) {
    net_init();
    net_device_init();
    net_start();
    // dbg_info(DBG_TEST, "info");
    // dbg_warning(DBG_TEST, "warning");
    // dbg_error(DBG_TEST, "error");
#ifdef PRINT_NET_TEST
    net_test();
#endif
    while (1);
}