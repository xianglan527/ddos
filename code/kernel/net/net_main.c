#include "assert.h"
#include "debug.h"
#include "exmsg.h"
#include "ipv4.h"
#include "net.h"
#include "net_api.h"
#include "net_device.h"
#include "pktbuf.h"
#include "printf.h"
#include "proc.h"
#include "socket.h"
#include "stdio.h"
#include "string.h"

extern Netif_ops netdev_ops;
extern List_entry nets_list;
extern uint64_t sys_gettime(void);

typedef struct {
    char *name;
    char *ip;
    char *mask;
    char *gw;
} NetConfig;

char *br0_ip = BR0_IP;
char *br0_gw = BR0_GATEWAY;
char *br0_netmask = BR0_NETMASK;

char *friend0_ip = BR0_IP;

// NetConfig net_configs[] = {
//     {"net0", "192.168.74.3", "255.255.255.0", "192.168.74.1"},
//     {"net1", "192.168.74.4", "255.255.255.0", "192.168.74.1"},
// };

// NetConfig *get_net_config(const char *net_name) {
//     for (int i = 0; i < sizeof(net_configs) / sizeof(NetConfig); i++) {
//         if (net_configs[i].name && strcmp(net_configs[i].name, net_name) == 0) { return &net_configs[i]; }
//     }
//     return nullptr;
// }

int net_device_init(void) {
    List_entry *le;
    int index = 3;
    list_for_each(le, &nets_list) {
        Virtio_net *net = le2virtio_net(le);
        Netif *netif = netif_open(nullptr, &netdev_ops, net);
        assert(netif != nullptr);
        snprintf(netif->netif_name, sizeof(netif->netif_name), "%sif", net->net_name);
        // NetConfig *config = get_net_config(net->net_name);
        // if (config == nullptr) {
        //     warn("No configuration found for %s\n", net->net_name);
        //     return -E_NET_CONFIG;
        // }
        Ipaddr ip, mask, gw;
        ipaddr_from_str(&ip, br0_gw);
        // ipaddr_from_str(&ip, br0_ip);
        ipaddr_from_str(&mask, br0_netmask);
        // ipaddr_from_str(&gw, br0_gw);
        ipaddr_from_str(&gw, br0_ip);
        // ipaddr_from_str(&gw, br0_gw);
        ip.a_addr[IPV4_ADDR_SIZE - 1] += index++;
        // ipaddr_from_str(&ip, br0_ip);
        netif_set_addr(netif, &ip, &mask, &gw);
        netif_set_active(netif);
        netif_list_dump();
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
        break;
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

void timer_func_test() {
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
    Hostent hent, *result;
    int err;
    if (gethostbyname_r(dest, &hent, buf, sizeof(buf), &result, &err) < 0) {
        cprintf("resolve name %s failed\n", dest);
        return;
    }
    int sk = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sk < 0) {
        cprintf("Socket creation error, please run as root.\n");
        return;
    }

    // Set destination address
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = *(uint32_t *)hent.h_addr_list[0];  // inet_addr(dest);

    // Print target information
    inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
    cprintf("Trying to ping %s [%s]\n", dest, buf);

#define USE_CONNECT
#ifdef USE_CONNECT
    connect(sk, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
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
        ssize_t send_len = send(sk, (char *)&ping->req, total_size, 0);
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
        if (i < count - 1) { do_sleep(interval); }
    }
    closesocket(sk);
}

void socket_raw_test() {
    // char *dest = "192.168.0.1";
    // char *dest = friend0_ip;
    // char *dest = br0_gw;
    char *dest = "8.8.8.8";
    size_t count = 4;       // Default count
    size_t size = 56;       // Default data size
    size_t interval = 500;  // Default interval
    if (size > PING_BUFFER_SIZE) {
        cprintf("The size is too big, should be < %d\n", PING_BUFFER_SIZE);
        return;
    }
    Ping ping;
    memset(&ping, 0, sizeof(ping));
    // ping_run(&ping, dest, count, size, interval);
    // ping_run(&ping, "baidu.com", count, size, interval);
    ping_run(&ping, "baidu.com", count, size, interval);
    ping_run(&ping, "qq.com", count, size, interval);
    return;
}
/////////////////////////////////////////////////////////////

void udp_echo_server_start(void *arg) {
    int port = *(int *)arg;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        cprintf("socket creation failed");
        return;
    }
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    // local_addr.sin_addr.s_addr = inet_addr("192.168.8.4");
    local_addr.sin_port = htons(port);
    if (bind(s, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        cprintf("bind error");
        closesocket(s);
        return;
    }
    char buf[256];
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        ssize_t size = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&client_addr, &addr_len);
        if (size < 0) {
            cprintf("recvfrom error");
            break;
        }
        cprintf("UDP echo server: connected IP: %s, Port: %d\n", inet_ntoa(client_addr.sin_addr),
                ntohs(client_addr.sin_port));

        if (strncmp(buf, "quit", 4) == 0) { break; }
        size = sendto(s, buf, size, 0, (struct sockaddr *)&client_addr, addr_len);
        if (size < 0) {
            cprintf("sendto error");
            break;
        }
    }
    closesocket(s);
    return;
}

void udp_echo_server() {
    static int default_port = 5050;
    kernel_thread_init(udp_echo_server_start, &default_port);
}

int udp_echo_client_start(char *ip, int port) {
    cprintf("udp echo client, ip: %s, port: %d\n", ip, port);
    cprintf("Enter quit to exit\n");
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        cprintf("open socket error\n");
        return -1;
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);
    connect(s, (struct sockaddr *)&server_addr, sizeof(server_addr));
    cprintf(">>");
    char buf[128];
    while (getstring(buf, sizeof(buf)) != 0) {
        // cprintf("11111111111\n");
        if (strncmp(buf, "quit", 4) == 0) { break; }
        size_t total_len = strlen(buf);
        ssize_t size = send(s, buf, total_len, 0);
        // ssize_t size = sendto(s, buf, total_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (size < 0) {
            cprintf("send error\n");
            closesocket(s);
            return -1;
        }
        memset(buf, 0, sizeof(buf));
        // struct sockaddr_in remote_addr;
        // socklen_t addr_len = sizeof(remote_addr);
        // cprintf("2222222222\n");
        // size = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&remote_addr, &addr_len);
        size = recv(s, buf, sizeof(buf), 0);
        if (size < 0) {
            cprintf("recv error\n");
            closesocket(s);
            return -1;
        }
        buf[sizeof(buf) - 1] = '\0';  // 确保字符串以 '\0' 结束
        cprintf("%s", buf);
        cprintf(">>\n");
        // cprintf("33333333333\n");
    }
    closesocket(s);
    return 0;
}

// 主函数
int udp_echo_client() {
    char *ip = "127.0.0.1";
    //    char *ip = "192.168.8.27";
    int port = 5050;
    return udp_echo_client_start(ip, port);
}

void udp_echo_test() {
    udp_echo_server();
    udp_echo_client();
}
//////////////////////////////////////////////////////////////
void tcp_echo_server_start(void *arg) {
    int port = *(int *)arg;

    // 1. 创建 TCP Socket（SOCK_STREAM）
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        cprintf("socket creation failed\n");
        return;
    }

    // 2. 绑定端口
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    // local_addr.sin_addr.s_addr = inet_addr("192.168.8.4"); // 如需要指定IP请自行解注并设置
    local_addr.sin_port = htons(port);
    if (bind(s, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        cprintf("bind error\n");
        closesocket(s);
        return;
    }

    // 3. 监听端口
    if (listen(s, 2) < 0) {
        cprintf("listen error\n");
        closesocket(s);
        return;
    }
    cprintf("TCP echo server listening on port %d\n", port);

    while (1) {
        // 4. 接收连接
        struct sockaddr_in client_addr;
        client_addr.sin_port = 0;
        socklen_t addr_len = sizeof(client_addr);
        int client_sock = 0;
        client_sock = accept(s, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock < 0) {
            cprintf("accept error\n");
            break;
        }
        cprintf("TCP echo server: connected IP: %s, Port: %d\n", inet_ntoa(client_addr.sin_addr),
                ntohs(client_addr.sin_port));

        // 5. 接收并回显
        char buf[4096];
        // getstring(buf, sizeof(buf));
        while (1) {
            memset(buf, 0, sizeof(buf));
            ssize_t size = recv(client_sock, buf, sizeof(buf) - 1, 0);
            if (size <= 0) {
                // size < 0 是出错，size == 0 是客户端关闭连接
                cprintf("recv error or connection closed\n");
                break;
            }
            // recv(client_sock, buf, sizeof(buf) - 1, 0);
            // 判断是否输入 "quit"
            if (strncmp(buf, "quit", 4) == 0) { break; }

            // 回显
            ssize_t send_size = send(client_sock, buf, size, 0);
            if (send_size < 0) {
                cprintf("send error\n");
                break;
            }
        }
        // 关闭本次连接
        closesocket(client_sock);
        // 如果只想接受一次连接就退出，可在此处 break
        // break;
    }

    // 关闭监听socket
    closesocket(s);
    return;
}

void tcp_echo_server() {
    static int default_port = 5050;
    kernel_thread_init(tcp_echo_server_start, &default_port);
}

int tcp_echo_client_start(char *ip, int port) {
    cprintf("tcp echo client, ip: %s, port: %d\n", ip, port);
    cprintf("Enter quit to exit\n");

    // 1. 创建 TCP Socket
    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {
        cprintf("open socket error\n");
        return -1;
    }

    // 2. 连接服务器
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);
#if 1    
    while(connect(s, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0);
#else
    // if (connect(s, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    //     cprintf("connect error\n");
    //     closesocket(s);
    //     return -1;
    // }
# endif
    int keepalive = 1;
    int keepidle = 5;  
    int keepinterval = 1; 
    int keepcount = 10;   
    setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalive, sizeof(keepalive));
    setsockopt(s, SOL_TCP, TCP_KEEPIDLE, (void *)&keepidle, sizeof(keepidle));
    setsockopt(s, SOL_TCP, TCP_KEEPINTVL, (void *)&keepinterval, sizeof(keepinterval));
    setsockopt(s, SOL_TCP, TCP_KEEPCNT, (void *)&keepcount, sizeof(keepcount));
    char buf[4096];
    cprintf(">>");
#if 0
    getstring(buf, sizeof(buf));
    closesocket(s);
    cprintf("11111111\n");
    return 0;
#endif

#if 0
    char sbuf[4096];
    for (int i = 0; i < sizeof(sbuf); i++) { sbuf[i] = 'a' + i % 26; }
    for (int i = 0; i < 2; i++) {
        ssize_t size = send(s, sbuf, sizeof(sbuf), 0);
        cprintf("............index is %d   size is %ld\n", i, size);
        if (size < 0) {
            cprintf("send error: size=%d\n", (int)size);
            break;
        }
    }
    cprintf("11111111\n");
    // getstring(buf, sizeof(buf));
    closesocket(s);
    return 0;
#endif

#if 0
    ssize_t total_size = 0;
    ssize_t rcv_size;
    while ((rcv_size = recv(s, buf, sizeof(buf), 0)) > 0) {
        // cprintf("4444444444\n");
        // do_sleep(200);
        // cprintf("5555555555\n");
        buf[rcv_size] = '\0';  // 确保字符串以 '\0' 结束
        cprintf("rcv_size is %ld\n %s\n", rcv_size, buf);
        total_size += rcv_size;
    }
    cprintf("333333333\n");
    if (rcv_size < 0) {
        // 接收完毕
        cprintf("rcv file size: %ld\n", total_size);
        closesocket(s);
        return -1;
    }
    cprintf("rcv file size: %ld\n", total_size);
    cprintf("rcv file ok\n");
    closesocket(s);
    return 0;

#endif


    while (getstring(buf, sizeof(buf)) != 0) {
        // if (strncmp(buf, "quit", 4) == 0) { break; }
 
        ssize_t size;
#if 1
        size_t total_len = strlen(buf);
        size = send(s, buf, total_len, 0);
        if (size < 0) {
            cprintf("send error\n");
            closesocket(s);
            return -1;
        }
#endif
#if 0
        for (int i = 0; i < sizeof(buf); i++) { buf[i] = 'a' + i % 26; }
        for (int i = 0; i < 1; i++) {
            ssize_t snd_size = send(s, buf, sizeof(buf), 0);
            cprintf("............index is %d   snd_size is %ld\n", i, snd_size);
            if (snd_size < 0) {
                cprintf("send error: snd_size=%d\n", (int)snd_size);
                break;
            }
        }
#endif
        // 接收回显的数据
        memset(buf, 0, sizeof(buf));
        size = recv(s, buf, sizeof(buf) - 1, 0);
        // cprintf("22222222\n");
        if (size < 0) {
            cprintf("recv error\n");
            closesocket(s);
            return -1;
        } else if (size == 0) {
            // 服务器端关闭连接
            cprintf("connection closed by server\n");
            break;
        }
        buf[size] = '\0';  // 确保字符串以 '\0' 结束

        cprintf("%s", buf);
        if (strncmp(buf, "quit", 4) == 0) { break; }
        cprintf(">>\n");
    }

    closesocket(s);
    // cprintf("11111111\n");
    return 0;
}

int tcp_echo_client() {
    char *ip = "127.0.0.1";
    // char* ip = "192.168.8.27";
    // char *ip = "192.168.8.28";
    int port = 5050;
    return tcp_echo_client_start(ip, port);
}

void tcp_echo_test() {
    tcp_echo_server();
    do_sleep(3000);
    tcp_echo_client();
}
//////////////////////////////////////////////////////////////
int test_func_arg = 0x1234;
int test_func(Func_msg *msg) {
    msg->err = 0x1234;
    cprintf("hello, 1234: %x\n", *(int *)msg->param);
    return NET_OK;
}

void exmsg_func_test() { exmsg_func_exec(test_func, (void *)&test_func_arg); }

//////////////////////////////////////////////////////////////

void net_test() {
    // pktbuf_test();
    // timer_func_test();
    //  exmsg_func_test();
    // socket_raw_test();
    // udp_echo_test();
    tcp_echo_test();
}

void net_main(void *arg) {
    cprintf("br0 IP: %s\n", br0_ip);
    cprintf("br0 Gateway: %s\n", br0_gw);
    cprintf("br0 netmask: %s\n", br0_netmask);
    assert(memcmp(br0_ip, "0.0.0.0", sizeof("0.0.0.0")) != 0);
    assert(memcmp(br0_gw, "0.0.0.0", sizeof("0.0.0.0")) != 0);
    assert(memcmp(br0_netmask, "0.0.0.0", sizeof("0.0.0.0")) != 0);
    net_init();
    net_device_init();
    net_start();
    // dbg_info(DBG_TEST, "info");
    // dbg_warning(DBG_TEST, "warning");
    // dbg_error(DBG_TEST, "error");
#ifdef PRINT_NET_TEST
    net_test();
#endif
    // while (1);
}