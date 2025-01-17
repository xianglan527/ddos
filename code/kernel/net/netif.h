#ifndef __NET_NETIF_H__
#define __NET_NETIF_H__

#include "error.h"
#include "ipaddr.h"
#include "list.h"
#include "net_config.h"
#include "sem.h"
#include "types.h"
#include "virtio-net.h"
#include "pktbuf.h"
#include "mbox.h"

#define NETIF_MBOX_ID_NULL (-1)

typedef enum netif_type {
    NETIF_TYPE_NONE = 0,
    NETIF_TYPE_ETHER,
    NETIF_TYPE_LOOP,
    NETIF_TYPE_SIZE,
} Netif_type;

typedef struct netif Netif;

typedef struct link_layer Link_layer;
struct link_layer {
    Netif_type type;
    int (*open)(Netif *netif);
    void (*close)(Netif *netif);
    int (*in)(Netif *netif, Pktbuf *buf);
    int (*out)(Netif *netif, Ipaddr *dest, Pktbuf *buf);
};

typedef struct netif_hwaddr {
    uint8_t len;
    uint8_t addr[NETIF_HWADDR_SIZE];
} Netif_hwaddr;

typedef struct netif Netif;

typedef struct netif_ops Netif_ops;
struct netif_ops {
    int (*open)(Netif *netif, void *data);
    void (*close)(Netif *netif);
    int (*xmit)(Netif *netif);
};

typedef struct netif Netif;
struct netif {
    Virtio_net *net;
    char netif_name[64];
    Netif_hwaddr hwaddr;
    Ipaddr ipaddr;
    Ipaddr netmask;
    Ipaddr gateway;
    volatile enum {
        NETIF_CLOSED,
        NETIF_OPENED,
        NETIF_ACTIVE,
    } state;
    Netif_type type;
    size_t mtu;
    Netif_ops *ops;
    void *ops_data;
    Link_layer *link_layer;
    volatile int tx_mbox_id;
    volatile int rx_mbox_id;
    Sem sem;
    List_entry netif_link;
};
#define le2netif(le) to_struct((le), Netif, netif_link)

#define dump_ip(module, msg, ip)   {if (module >= DBG_LEVEL_INFO) dump_ip_buf(msg, (ip)->a_addr); }

static void lock_netif(Netif *netif) { down(&netif->sem); }

static void unlock_netif(Netif *netif) { up(&netif->sem); }

int netif_init(void);
Netif *netif_open(char *dev_name, Netif_ops *ops, void *ops_data);
int netif_set_addr(Netif *netif, Ipaddr *ip, Ipaddr *netmask, Ipaddr *gateway);
int netif_set_hwaddr(Netif *netif, uint8_t *hwaddr, int len);
int netif_set_active(Netif *netif);
int netif_set_inactive(Netif *netif, bool force);
void dump_mac(char *msg, uint8_t *mac);
void dump_ip_buf(char *msg, uint8_t *ip);
void netif_list_dump();
void netif_set_default(Netif *netif);
int netif_close(Netif *netif);
int netif_register_layer(int type, Link_layer *layer);

int netif_put_out(Netif *netif, Pktbuf *buf, long tmo);
int netif_put_in(Netif *netif, Pktbuf *pktbuf, long tmo);
Pktbuf *netif_get_out(Netif *netif, long tmo);
Pktbuf *netif_get_in(Netif *netif, long tmo);
int netif_out(Netif *netif, Ipaddr *ipaddr, Pktbuf *buf);
Netif *netif_get_default(void);
#endif