#ifndef __NET_ARP_H__
#define __NET_ARP_H__

#include "error.h"
#include "ether.h"
#include "list.h"
#include "net.h"
#include "net_config.h"
#include "netif.h"
#include "pktbuf.h"
#include "protocol.h"
#include "spinlock.h"
#include "types.h"

#define ARP_HW_ETHER 0x1  // Ethernet type
#define ARP_REQUEST 0x1   // ARP request packet
#define ARP_REPLY 0x2     // ARP reply packet

#pragma pack(1)
typedef struct arp_pkt {
    // The type and len fields are used to make the packet compatible with different hardware layers and
    // protocol layers. In our case, we only support MAC to IP conversion, so these are hardcoded.
    uint16_t htype;                        // Hardware type
    uint16_t ptype;                        // Protocol type
    uint8_t hlen;                          // Hardware address length
    uint8_t plen;                          // Protocol address length
    uint16_t opcode;                       // Request/Response
    uint8_t send_haddr[ETH_HWA_SIZE];      // Sender's hardware address
    uint8_t send_paddr[IPV4_ADDR_SIZE];    // Sender's protocol address
    uint8_t target_haddr[ETH_HWA_SIZE];    // Target's hardware address
    uint8_t target_paddr[IPV4_ADDR_SIZE];  // Target's protocol address
} Arp_pkt;

#pragma pack()

typedef struct arp_entry Arp_entry;
struct arp_entry {
    uint8_t paddr[IPV4_ADDR_SIZE];
    uint8_t haddr[ETH_HWA_SIZE];
    enum {
        NET_ARP_FREE = 0x1234,
        NET_ARP_RESOLVED,
        NET_ARP_WAITING,
    } state;
    size_t tmo;
    size_t retry;
    Netif* netif;
    List_entry arp_entry_link;
    List_entry buf_list;
    Spinlock buf_list_lock;
};

typedef struct arp_entry_head {
    Spinlock arp_entry_list_lock;
    size_t count;
    List_entry arp_entry_list;
} Arp_entry_head;

#define le2arp_entry(le) to_struct((le), Arp_entry, arp_entry_link)

int arp_init(void);
int arp_make_request(Netif* netif, Ipaddr* pro_addr);
int arp_make_gratuitous(Netif* netif);
int arp_in(Netif* netif, Pktbuf* buf);
int arp_make_reply(Netif* netif, Pktbuf* buf);
int arp_resolve(Netif* netif, Ipaddr* ipaddr, Pktbuf* buf);
void arp_clear(Netif* netif);
uint8_t* arp_find(Netif* netif, Ipaddr* ip);
void arp_update_from_ipbuf(Netif* netif, Pktbuf* pkt);
#endif