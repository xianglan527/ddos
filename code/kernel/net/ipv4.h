#ifndef __NET_IPV4_H__
#define __NET_IPV4_H__

#include "error.h"
#include "net_config.h"
#include "types.h"
#include "net.h"
#include "ipaddr.h"
#include "netif.h"
#include "pktbuf.h"

#define NET_VERSION_IPV4 4
#define NET_IP_DEF_TTL  64

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
    uint16_t frag_all;

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

static inline size_t ipv4_hdr_size(Ipv4_pkt *pkt) {
    return pkt->hdr.shdr * 4;  
}

static inline void set_header_size(Ipv4_pkt *pkt, size_t size) { pkt->hdr.shdr = (uint16_t)(size / 4); }

int ipv4_init(void);
int ipv4_in(Netif *netif, Pktbuf *buf);
int ipv4_out(uint8_t protocol, Ipaddr *dest, Ipaddr *src, Pktbuf *buf);
#endif