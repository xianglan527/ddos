#include "assert.h"
#include "debug.h"
#include "net.h"
#include "net_device.h"
#include "pktbuf.h"
#include "string.h"
#include "printf.h"
#include "proc.h"
#include "stdio.h"
#include "ipv4.h"
#include "exmsg.h"
#include "socket.h"
#include "stdio.h"
#include "net_api.h"

extern Netif_ops netdev_ops;
extern List_entry nets_list;
extern uint64_t sys_gettime(void);

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

        // ipaddr_from_str(&dest, "192.168.74.255");
        // buf = pktbuf_alloc(32);
        // pktbuf_fill(buf, 0xA5, buf->total_size);
        // netif_out(netif, &dest, buf);

        // buf = pktbuf_alloc(32);
        // pktbuf_fill(buf, 0xA5, buf->total_size);

        // Ipaddr src;
        // ipaddr_from_str(&dest, friend0_ip);
        // ipaddr_from_str(&src, config->ip);
        // ipv4_out(0, &dest, &src, buf);
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

//////////////////////////////////////////////////////////////
#define PING_BUFFER_SIZE 4096  
#define PING_DEFAULT_ID 0x200

#pragma pack(1)

/**
 * 自定义的IP包头
 * 没有使用协议栈自带的，目的是让ping的代码与所用的协议栈无关
 */
typedef struct ipHdr {
    uint8_t shdr : 4;       // 首部长，低4字节
    uint8_t version : 4;    // 版本号
    uint8_t tos;            // 服务类型
    uint16_t total_len;     // 总长度
    uint16_t id;            // 标识符，用于区分不同的数据报, 可用于ip数据报分片与重组
    uint16_t frag;          // 分片偏移与标志位
    uint8_t ttl;            // 存活时间，每台路由器转发时减1，减到0时，该包被丢弃
    uint8_t protocol;       // 上层协议
    uint16_t hdr_checksum;  // 首部校验和
    uint8_t src_ip[4];      // 源IP
    uint8_t dest_ip[4];     // 目标IP
} IpHdr;

/**
 * 回显请求与应答的包头
 */
typedef struct icmpHdr {
    uint8_t type;       // 类型
    uint8_t code;       // 代码
    uint16_t checksum;  // ICMP报文的校验和
    uint16_t id;        // 标识符
    uint16_t seq;       // 序号
} IcmpHdr;

/**
 * 请求包，不含IP包头
 */
typedef struct echoReq {
    IcmpHdr echo_hdr;
    clock_t time;
    char buf[PING_BUFFER_SIZE];
} EchoReq;

/**
 * 响应包，包含IP包头
 */
typedef struct echoReply {
    IpHdr iphdr;
    IcmpHdr echo_hdr;
    clock_t time;
    char buf[PING_BUFFER_SIZE];
} EchoReply;

#pragma pack()

typedef struct ping {
    EchoReq req;      // 请求包，不含IP包头
    EchoReply reply;  // 响应包，含IP包头
} Ping;

static uint16_t checksum(void *buf, uint16_t len) {
    uint16_t *curr_buf = (uint16_t *)buf;
    uint32_t sum = 0;
    while (len > 1) {
        sum += *curr_buf++;
        len -= 2;
    }
    if (len > 0) { sum += *(uint8_t *)curr_buf; }
    // Handle carry
    uint16_t high;
    while ((high = sum >> 16) != 0) { sum = high + (sum & 0xFFFF); }
    return (uint16_t)~sum;
}

void ping_run(Ping *ping, char *dest, size_t count, size_t size, size_t interval) {
    static uint16_t start_id = PING_DEFAULT_ID;  // Record an initial ID, continuously increment
    char buf[512];

    int sk = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sk < 0) {
        cprintf("Socket creation error, please run as root.\n");
        return;
    }

    // Set destination address
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = inet_addr(dest);

    // Print target information
    inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
    cprintf("Trying to ping %s [%s]\n", dest, buf);

// #define USE_CONNECT
#ifdef USE_CONNECT
    connect(sk, (const struct sockaddr *)&addr, sizeof(struct sockaddr_in));
#endif

    struct timeval tmo;
    tmo.tv_sec = 5;
    tmo.tv_usec = 0;
    setsockopt(sk, SOL_SOCKET, SO_RCVTIMEO, (char *)&tmo, sizeof(tmo));

    // Fill the data to be sent
    size -= sizeof(clock_t);
    size_t fill_size = (size > PING_BUFFER_SIZE) ? PING_BUFFER_SIZE : size;
    for (size_t i = 0; i < fill_size; i++) { ping->req.buf[i] = (uint8_t)i; }

    // Calculate the total length to send (header + payload)
    size_t total_size = sizeof(IcmpHdr) + fill_size;

    // Loop to send 'count' times
    for (size_t i = 0, seq = 0; i < count; i++, seq++) {
        // Construct ICMP Echo
        ping->req.echo_hdr.id = start_id++;
        ping->req.echo_hdr.seq = (uint16_t)seq;
        ping->req.echo_hdr.type = 8;  // ICMP Echo request
        ping->req.echo_hdr.code = 0;
        ping->req.echo_hdr.checksum = 0;

        // Record send time
        ping->req.time = sys_gettime();

        // Recalculate checksum
        ping->req.echo_hdr.checksum = checksum(&ping->req, (uint16_t)total_size);

        // Send
#ifdef USE_CONNECT
        ssize_t send_len = send(sk, (const char *)&ping->req, total_size, 0);
#else
        ssize_t send_len =
            sendto(sk, (char *)&ping->req, total_size, 0, (struct sockaddr *)&addr, sizeof(addr));
#endif
        if (send_len < 0) {
            cprintf("Failed to send ping request.\n");
            break;
        }

        // Receive reply
        do {
            memset(&ping->reply, 0, sizeof(ping->reply));
#ifdef USE_CONNECT
            ssize_t recv_len = recv(sk, (char *)&ping->reply, sizeof(ping->reply), 0);
#else
            socklen_t addr_len = sizeof(addr);
            ssize_t recv_len = recvfrom(sk, (char *)&ping->reply, sizeof(ping->reply), 0,
                                        (struct sockaddr *)&addr, &addr_len);
#endif
            if (recv_len < 0) {
                cprintf("Ping reply timeout.\n");
                break;
            }
            // Check if it matches
            if ((ping->req.echo_hdr.id == ping->reply.echo_hdr.id) &&
                (ping->req.echo_hdr.seq == ping->reply.echo_hdr.seq)) {
                // Note to use %zu for size_t
                cprintf("Received reply, sequence=%lu\n", seq);
                break;
            }
        } while (1);
        if (size > 0) {
            // Compare the received data part, if different the packet is corrupted, skip and continue ping()
            int recv_size = size;
            if (recv_size < (int)(sizeof(IpHdr) + sizeof(IcmpHdr))) {
                cprintf("Received packet is too small, ignoring.\n");
                continue;
            }
            int data_size = size - sizeof(IpHdr) - sizeof(IcmpHdr) - sizeof(clock_t);
            if (data_size > 0 && memcmp(ping->req.buf, ping->reply.buf, data_size) != 0) {
                cprintf("Received data is incorrect.\n");
                continue;
            }
            // Display response information
            IpHdr *iphdr = &ping->reply.iphdr;
            int send_size = fill_size + sizeof(clock_t);
            if (recv_size == send_size) {
                cprintf("Reply from %s: bytes = %d", inet_ntoa(addr.sin_addr), send_size);
            } else {
                cprintf("Reply from %s: bytes = %d (sent = %d)", inet_ntoa(addr.sin_addr), recv_size,
                       send_size);
            }
            // Calculate time difference (milliseconds)
            int diff_ms = (sys_gettime() - ping->req.time);
            if (diff_ms < 1) {
                cprintf(", time <1ms, TTL=%d\n", iphdr->ttl);
            } else {
                cprintf(", time=%dms, TTL=%d\n", diff_ms, iphdr->ttl);
            }
        }
        if (i < count - 1) {
            do_sleep(interval);
        }
    }
    closesocket(sk);
}

void socket_raw_test(){
    char *dest = "192.168.74.2";
    size_t count = 4;     // Default count
    size_t size = 56;     // Default data size
    size_t interval = 500;  // Default interval
    if (size > PING_BUFFER_SIZE) {
        cprintf("The size is too big, should be < %d\n", PING_BUFFER_SIZE);
        return;
    }
    Ping ping;
    memset(&ping, 0, sizeof(ping));
    ping_run(&ping, dest, count, size, interval);
    return;
}

//////////////////////////////////////////////////////////////

void net_test() { 
    // pktbuf_test();
    // timer_func_test();
    socket_raw_test();
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
    int test_func_arg = 0x1234;
    exmsg_func_exec(test_func, (void *)&test_func_arg);
    while (1);
}