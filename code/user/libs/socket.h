#ifndef __LIBS_SOCKET_H__
#define __LIBS_SOCKET_H__

#include "stdarg.h"
#include "types.h"

typedef enum protocol {
    NET_PROTOCOL_ARP = 0x0806,
    NET_PROTOCOL_IPv4 = 0x0800,
    NET_PROTOCOL_ICMPv4 = 0x1,
    NET_PROTOCOL_UDP = 0x11,
    NET_PROTOCOL_TCP = 0x06,
} Protocol;

typedef enum socket_type {
    SOCK_NONE = 0,
    SOCK_RAW,
    SOCK_UDP,
    SOCK_TCP,
} Socket_type;

#define SOCK_DGRAM SOCK_UDP
#define SOCK_STREAM SOCK_TCP

#define NET_OK 0

#define AF_INET 0

#define IPPROTO_ICMP NET_PROTOCOL_ICMPv4

#define IPPROTO_UDP NET_PROTOCOL_UDP

#define IPPROTO_TCP NET_PROTOCOL_TCP

#define INADDR_ANY 0

#define SOL_SOCKET 0

#define SOL_TCP 1

#define SO_RCVTIMEO 1
#define SO_SNDTIMEO 2
#define SO_KEEPALIVE 3
#define TCP_KEEPIDLE 4
#define TCP_KEEPINTVL 5
#define TCP_KEEPCNT 6

#define IPV4_ADDR_BROADCAST 0xFFFFFFFF
#define IPV4_ADDR_SIZE 4

typedef struct timeval {
    int tv_sec;
    int tv_usec;
} Timeval;

typedef struct hostent {
    char* h_name;
    char** h_aliases;
    int h_addrtype;
    int h_length;
    char** h_addr_list;
} Hostent;

#define PING_BUFFER_SIZE 4096
#define PING_DEFAULT_ID 0x200

#pragma pack(1)

typedef struct in_addr {
    union {
        struct {
            uint8_t addr0;
            uint8_t addr1;
            uint8_t addr2;
            uint8_t addr3;
        };
        uint8_t addr_array[IPV4_ADDR_SIZE];
        uint32_t s_addr;
    };
} In_addr;

typedef struct sockaddr {
    uint8_t sa_len;
    uint8_t sa_family;
    uint8_t sa_data[14];
} Sockaddr;

typedef struct sockaddr_in {
    uint8_t sin_len;
    uint8_t sin_family;
    uint16_t sin_port;
    In_addr sin_addr;
    char sin_zero[8];
} Sockaddr_in;

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

typedef struct icmpHdr {
    uint8_t type;       // 类型
    uint8_t code;       // 代码
    uint16_t checksum;  // ICMP报文的校验和
    uint16_t id;        // 标识符
    uint16_t seq;       // 序号
} IcmpHdr;

typedef struct echoReq {
    IcmpHdr echo_hdr;
    clock_t time;
    char buf[PING_BUFFER_SIZE];
} EchoReq;

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

typedef struct ipaddr Ipaddr;
struct ipaddr {
    enum {
        IPADDR_V4,
    } type;
    union {
        uint32_t q_addr;
        uint8_t a_addr[IPV4_ADDR_SIZE];
    };
};

typedef uint32_t in_addr_t;
typedef struct hostent_extra {
    in_addr_t* addr_tbl[2];
    in_addr_t addr;
    char name[1];
} Hostent_extra;

static inline uint16_t swap_u16(uint16_t v) {
    uint16_t r = ((v & 0xFF) << 8) | ((v >> 8) & 0xFF);
    return r;
}

static inline uint32_t swap_u32(uint32_t v) {
    uint32_t r = (((v >> 0) & 0xFF) << 24) | (((v >> 8) & 0xFF) << 16) | (((v >> 16) & 0xFF) << 8) |
                 (((v >> 24) & 0xFF) << 0);
    return r;
}

#define NET_ENDIAN_LITTLE 1

#if NET_ENDIAN_LITTLE  // 小端模式，需要转换
uint16_t swap_u16(uint16_t v);
uint32_t swap_u32(uint32_t v);
#define htons(v) swap_u16(v)
#define ntohs(v) swap_u16(v)
#define htonl(v) swap_u32(v)
#define ntohl(v) swap_u32(v)

#else
#define htons(v) (v)
#define ntohs(v) (v)
#define htonl(v) (v)
#define ntohl(v) (v)
#endif

uint16_t checksum(void* buf, uint16_t len);
char* inet_ntoa(In_addr in);
uint32_t inet_addr(char* str);
int inet_pton(int family, char* strptr, void* addrptr);
char* inet_ntop(int family, void* addrptr, char* strptr, size_t len);
void ping_run(Ping* ping, char* dest, size_t count, size_t size, size_t interval);
#endif
