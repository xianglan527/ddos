#ifndef __NET_IPV4_H__
#define __NET_IPV4_H__

#include "error.h"
#include "ipaddr.h"
#include "net.h"
#include "net_config.h"
#include "netif.h"
#include "pktbuf.h"
#include "types.h"

#define NET_VERSION_IPV4 4
#define NET_IP_DEF_TTL 64

#pragma pack(1)

typedef struct ipv4_hdr {
    union {
        struct {
#if NET_ENDIAN_LITTLE
            uint16_t shdr : 4;
            uint16_t version : 4;
            uint16_t tos : 8;
#else
            uint16_t version : 4;
            uint16_t shdr : 4;
            uint16_t tos : 8;
#endif
        };
        uint16_t shdr_all;
    };
    uint16_t total_len;  // Total length
    uint16_t id;  // Identifier, used to distinguish different datagrams, can be used for IP fragmentation and
                  // reassembly
    union {
        struct {
#if NET_ENDIAN_LITTLE
            uint16_t offset : 13;   // Fragment offset, in 8-byte units, starting from 0
            uint16_t more : 1;      // More fragments follow, 1 if more, 0 if this is the last fragment
            uint16_t disable : 1;   // 1 - Fragmentation is not allowed, 0 - Fragmentation is allowed
            uint16_t reserved : 1;  // Reserved, must be 0
#else
            uint16_t reserved : 1;  // Reserved, must be 0
            uint16_t disable : 1;   // 1 - Fragmentation is not allowed, 0 - Fragmentation is allowed
            uint16_t more : 1;      // More fragments follow, 1 if more, 0 if this is the last fragment
            uint16_t offset : 13;   // Fragment offset, in 8-byte units, starting from 0
#endif
        };
        uint16_t frag_all;
    };
    uint8_t ttl;            // Time to live, decremented by 1 at each router hop; discarded when it reaches 0
    uint8_t protocol;       // Upper layer protocol
    uint16_t hdr_checksum;  // Header checksum
    uint8_t src_ip[IPV4_ADDR_SIZE];   // Source IP address
    uint8_t dest_ip[IPV4_ADDR_SIZE];  // Destination IP address
} Ipv4_hdr;

typedef struct ipv4_pkt {
    Ipv4_hdr hdr;
    uint8_t data[1];
} Ipv4_pkt;

#pragma pack()

typedef struct ip_frag {
    Ipaddr ip;
    uint16_t id;
    size_t tmo;
    List_entry ip_frag_buf_list;
    Spinlock ip_frag_buf_list_lock;
    List_entry ip_frag_link;
} Ip_frag;

#define le2ip_frag(le) to_struct((le), Ip_frag, ip_frag_link)

typedef struct ip_frag_head {
    Spinlock ip_frag_list_lock;
    size_t count;
    List_entry ip_frag_list;
} Ip_frag_head;

static inline size_t ipv4_hdr_size(Ipv4_pkt *pkt) { return pkt->hdr.shdr * 4; }

static inline void set_header_size(Ipv4_pkt *pkt, size_t size) { pkt->hdr.shdr = (uint16_t)(size / 4); }

static inline size_t get_data_size(Ipv4_pkt *pkt) { return pkt->hdr.total_len - ipv4_hdr_size(pkt); }

static inline uint16_t get_frag_start(Ipv4_pkt *pkt) { return pkt->hdr.offset * 8; }

static inline uint16_t get_frag_end(Ipv4_pkt *pkt) { return get_frag_start(pkt) + get_data_size(pkt); }

int ipv4_init(void);
int ipv4_in(Netif *netif, Pktbuf *buf);
int ipv4_out(uint8_t protocol, Ipaddr *dest, Ipaddr *src, Pktbuf *buf);
#endif