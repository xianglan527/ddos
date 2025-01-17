#include "netif.h"

#include "assert.h"
#include "ether.h"
#include "exmsg.h"
#include "ipv4.h"
#include "mbox.h"
#include "net.h"
#include "proc.h"
#include "protocol.h"
#include "slab.h"
#include "spinlock.h"
#include "stdio.h"
#include "string.h"

List_entry netif_list;
Spinlock netif_list_lock;
Netif *netif_default;
static Link_layer *link_layers[NETIF_TYPE_SIZE];

extern Spinlock print_struct_lock;

void dump_mac(char *msg, uint8_t *mac) {
    if (msg) { cprintf("%s", msg); }
    cprintf("%02x-%02x-%02x-%02x-%02x-%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void dump_ip_buf(char *msg, uint8_t *ip) {
    if (msg) { cprintf("%s", msg); }
    if (ip) {
        cprintf("%d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    } else {
        cprintf("0.0.0.0\n");
    }
}

void netif_list_dump() {
    acquire(&netif_list_lock);
    acquire(&print_struct_lock);
    cprintf("\nnetif_list dump....................................\n\n");
    List_entry *le;
    size_t count = 0;
    list_for_each(le, &netif_list) {
        Netif *netif = le2netif(le);
        cprintf("state :");
        switch (netif->state) {
            case NETIF_CLOSED: cprintf(" %s ", "closed"); break;
            case NETIF_OPENED: cprintf(" %s ", "opened"); break;
            case NETIF_ACTIVE: cprintf(" %s ", "active"); break;
            default: break;
        }
        cprintf("  type :");
        switch (netif->type) {
            case NETIF_TYPE_NONE: cprintf(" %s ", "none"); break;
            case NETIF_TYPE_ETHER: cprintf(" %s ", "ether"); break;
            case NETIF_TYPE_LOOP: cprintf(" %s ", "loop"); break;
            default: break;
        }
        cprintf("  mtu : %d\n", netif->mtu);
        dump_mac(" mac:", netif->hwaddr.addr);
        dump_ip_buf(" ip:", netif->ipaddr.a_addr);
        dump_ip_buf(" netmask:", netif->netmask.a_addr);
        dump_ip_buf(" gateway:", netif->gateway.a_addr);
        cprintf("\n\n");
    }
    cprintf("\nend of netif_list dump.............................\n");
    release(&print_struct_lock);
    release(&netif_list_lock);
}

int netif_init(void) {
    dbg_info(DBG_NETIF, "init netif");
    list_init(&netif_list);
    initlock(&netif_list_lock, "netif_list_lock");
    netif_default = nullptr;
    memset((void *)link_layers, 0, sizeof(link_layers));
    dbg_info(DBG_NETIF, "init done.\n");
    return NET_OK;
}

int netif_register_layer(int type, Link_layer *layer) {
    if ((type < 0) || (type >= NETIF_TYPE_SIZE)) {
        dbg_error(DBG_NETIF, "type error: %d", type);
        return -E_NET_PARAM;
    }
    if (link_layers[type]) {
        dbg_error(DBG_NETIF, "link layer: %d exist", type);
        return -E_NET_PARAM;
    }
    link_layers[type] = layer;
    return NET_OK;
}

static Link_layer *netif_get_layer(int type) {
    if ((type < 0) || (type >= NETIF_TYPE_SIZE)) { return nullptr; }
    return link_layers[type];
}

Netif *netif_open(char *dev_name, Netif_ops *ops, void *ops_data) {
    Netif *netif = kmalloc(sizeof(*netif));
    assert(netif != nullptr);
    ipaddr_set_any(&netif->ipaddr);
    ipaddr_set_any(&netif->netmask);
    ipaddr_set_any(&netif->gateway);
    netif->mtu = 0;
    netif->type = NETIF_TYPE_NONE;
    if (dev_name) strncpy(netif->netif_name, dev_name, NETIF_NAME_SIZE);
    netif->netif_name[NETIF_NAME_SIZE - 1] = '\0';
    netif->ops = ops;
    netif->ops_data = ops_data;
    memset(&netif->hwaddr, 0, sizeof(netif->hwaddr));
    sem_init(&netif->sem, 1);
    int ret = ops->open(netif, ops_data);
    assert(ret == NET_OK && netif->type != NETIF_TYPE_NONE);
    netif->tx_mbox_id = NETIF_MBOX_ID_NULL;
    netif->rx_mbox_id = NETIF_MBOX_ID_NULL;
    netif->state = NETIF_OPENED;
    netif->link_layer = netif_get_layer(netif->type);
    if (!netif->link_layer && (netif->type != NETIF_TYPE_LOOP)) {
        panic("no link layer. netif name: %s", dev_name);
    }
    acquire(&netif_list_lock);
    list_add(&netif_list, &netif->netif_link);
    release(&netif_list_lock);
    // netif_list_dump();
    return netif;
}

int netif_set_addr(Netif *netif, Ipaddr *ip, Ipaddr *netmask, Ipaddr *gateway) {
    lock_netif(netif);
    ipaddr_copy(&netif->ipaddr, ip ? ip : ipaddr_get_any());
    ipaddr_copy(&netif->netmask, netmask ? netmask : ipaddr_get_any());
    ipaddr_copy(&netif->gateway, gateway ? gateway : ipaddr_get_any());
    unlock_netif(netif);
    return NET_OK;
}

int netif_set_hwaddr(Netif *netif, uint8_t *hwaddr, int len) {
    lock_netif(netif);
    memcpy(netif->hwaddr.addr, hwaddr, len);
    netif->hwaddr.len = len;
    unlock_netif(netif);
    return NET_OK;
}

static void netif_set_default_nolock(Netif *netif) {
    if (!ipaddr_is_any(&netif->gateway)) {
        if (netif_default) { rt_remove(ipaddr_get_any(), ipaddr_get_any()); }
        rt_add(ipaddr_get_any(), ipaddr_get_any(), &netif->gateway, netif);
    }
    netif_default = netif;
}

void netif_set_default(Netif *netif) {
    acquire(&netif_list_lock);
    netif_set_default_nolock(netif);
    release(&netif_list_lock);
}

int netif_set_active(Netif *netif) {
    lock_netif(netif);
    if (netif->state != NETIF_OPENED) {
        dbg_error(DBG_NETIF, "netif is not opened");
        unlock_netif(netif);
        return -E_NET_STATE;
    }
    assert(netif->tx_mbox_id == NETIF_MBOX_ID_NULL);
    netif->tx_mbox_id = ipc_mbox_init(RX_TX_RX_MSG_CNT);
    assert(netif->rx_mbox_id == NETIF_MBOX_ID_NULL);
    netif->rx_mbox_id = ipc_mbox_init(RX_TX_RX_MSG_CNT);
    netif->state = NETIF_ACTIVE;
    unlock_netif(netif);
    if (netif->link_layer) {
        int ret = netif->link_layer->open(netif);
        if (ret < 0) {
            dbg_error(DBG_NETIF, "active error.");
            return ret;
        }
    }
    Ipaddr ip = ipaddr_get_net(&netif->ipaddr, &netif->netmask);
    rt_add(&ip, &netif->netmask, ipaddr_get_any(), netif);
    ipaddr_set_all_1(&ip);
    rt_add(&netif->ipaddr, &ip, ipaddr_get_any(), netif);
    acquire(&netif_list_lock);
    if (!netif_default && (netif->type != NETIF_TYPE_LOOP)) { netif_set_default_nolock(netif); }
    release(&netif_list_lock);
    // netif_list_dump();
    return NET_OK;
}

int netif_set_inactive(Netif *netif, bool force) {
    if (force) {
        netif->state = NETIF_OPENED;
        if (netif->link_layer) { netif->link_layer->close(netif); }
        ipc_mbox_free(netif->tx_mbox_id);
        netif->tx_mbox_id = NETIF_MBOX_ID_NULL;
        ipc_mbox_free(netif->rx_mbox_id);
        netif->rx_mbox_id = NETIF_MBOX_ID_NULL;
        acquire(&netif_list_lock);
        if (netif_default == netif) { netif_default = nullptr; }
        release(&netif_list_lock);
        return NET_OK;
    }
    lock_netif(netif);
    if (netif->state != NETIF_ACTIVE) {
        dbg_error(DBG_NETIF, "netif is not actived");
        unlock_netif(netif);
        return -E_NET_STATE;
    }
    assert(ipc_mbox_free(netif->tx_mbox_id) == 0);
    netif->tx_mbox_id = NETIF_MBOX_ID_NULL;
    assert(ipc_mbox_free(netif->rx_mbox_id) == 0);
    netif->rx_mbox_id = NETIF_MBOX_ID_NULL;
    netif->state = NETIF_OPENED;
    if (netif->link_layer) { netif->link_layer->close(netif); }
    unlock_netif(netif);
    Ipaddr ip = ipaddr_get_net(&netif->ipaddr, &netif->netmask);
    rt_remove(&ip, &netif->netmask);
    rt_remove(&netif->ipaddr, &netif->netmask);
    acquire(&netif_list_lock);
    if (netif_default == netif) {
        netif_default = nullptr;
        rt_remove(ipaddr_get_any(), ipaddr_get_any());
    }
    release(&netif_list_lock);
    // netif_list_dump();
    return NET_OK;
}

int netif_close(Netif *netif) {
    lock_netif(netif);
    if (netif->state == NETIF_ACTIVE) {
        dbg_error(DBG_NETIF, "netif(%s) is active, close failed.", netif->netif_name);
        unlock_netif(netif);
        return -E_NET_STATE;
    }
    assert(netif->tx_mbox_id == NETIF_MBOX_ID_NULL);
    assert(netif->rx_mbox_id == NETIF_MBOX_ID_NULL);
    netif->ops->close(netif);
    netif->state = NETIF_CLOSED;
    unlock_netif(netif);
    acquire(&netif_list_lock);
    list_del(&netif->netif_link);
    release(&netif_list_lock);
    return NET_OK;
}

int netif_put_out(Netif *netif, Pktbuf *pktbuf, long tmo) {
    lock_netif(netif);
    if (netif->state != NETIF_ACTIVE) {
        dbg_error(DBG_NETIF, "netif is not actived");
        unlock_netif(netif);
        return -E_NET_STATE;
    }
    assert(netif->tx_mbox_id != NETIF_MBOX_ID_NULL);
    Mboxbuf __buf, *buf = &__buf;
    buf->size = sizeof(Pktbuf *);
    buf->len = buf->size;
    buf->data = kmalloc(sizeof(Pktbuf *));
    memcpy(buf->data, &pktbuf, sizeof(void *));
    int ret = ipc_mbox_send(netif->tx_mbox_id, buf, tmo);
    assert(ret == 0 || ret == -E_TIMEOUT || ret == -1 || ret == -E_MBX_FULL);
    if (ret == -E_TIMEOUT || ret == -E_MBX_FULL) {
        dbg_warning(DBG_NETIF, "%s tx mbox full", netif->netif_name);
        unlock_netif(netif);
        return ret;
    }
    if (ret == -1) {
        dbg_error(DBG_NETIF, "%s tx mbox failed", netif->netif_name);
        unlock_netif(netif);
        return ret;
    }
    kfree(buf->data);
    unlock_netif(netif);
    return NET_OK;
}

int netif_put_in(Netif *netif, Pktbuf *pktbuf, long tmo) {
    lock_netif(netif);
    if (netif->state != NETIF_ACTIVE) {
        dbg_error(DBG_NETIF, "netif is not actived");
        unlock_netif(netif);
        return -E_NET_STATE;
    }
    assert(netif->rx_mbox_id != NETIF_MBOX_ID_NULL);
    Mboxbuf __buf, *buf = &__buf;
    buf->size = sizeof(Pktbuf *);
    buf->len = buf->size;
    buf->data = kmalloc(sizeof(Pktbuf *));
    memcpy(buf->data, &pktbuf, sizeof(void *));
    int ret = ipc_mbox_send(netif->rx_mbox_id, buf, tmo);
    assert(ret == 0 || ret == -E_TIMEOUT || ret == -1 || ret == -E_MBX_FULL);
    if (ret == -E_TIMEOUT || ret == -E_MBX_FULL) {
        dbg_warning(DBG_NETIF, "%s rx mbox full", netif->netif_name);
        unlock_netif(netif);
        return ret;
    }
    if (ret == -1) {
        dbg_error(DBG_NETIF, "%s rx mbox failed", netif->netif_name);
        unlock_netif(netif);
        return ret;
    }
    kfree(buf->data);
    unlock_netif(netif);

    Msg_mbox *mbox;
    size_t rx_mbox_slot;
    assert((mbox = get_mbox(netif->rx_mbox_id)) != nullptr);
    acquire(&mbox->msg_mbox_lock);
    rx_mbox_slot = mbox->slots;
    release(&mbox->msg_mbox_lock);
    if (rx_mbox_slot == 1) { 
        exmsg_netif_in(netif); 
    }
    return NET_OK;
}

Pktbuf *netif_get_out(Netif *netif, long tmo) {
    lock_netif(netif);
    if (netif->state != NETIF_ACTIVE) {
        dbg_error(DBG_NETIF, "netif is not actived");
        unlock_netif(netif);
        return nullptr;
    }
    assert(netif->tx_mbox_id != NETIF_MBOX_ID_NULL);
    Mboxbuf __buf, *buf = &__buf;
    buf->size = sizeof(Pktbuf *);
    buf->len = buf->size;
    buf->data = kmalloc(sizeof(Pktbuf *));
    int ret = ipc_mbox_recv(netif->tx_mbox_id, buf, tmo);
    assert(ret == 0 || ret == -E_TIMEOUT || ret == -1 || ret == -E_MBX_EMPTY);
    if (ret == -E_TIMEOUT || ret == -E_MBX_EMPTY) {
        dbg_warning(DBG_NETIF, "%s tx mbox empty", netif->netif_name);
        unlock_netif(netif);
        return nullptr;
    }
    if (ret == -1) {
        dbg_error(DBG_NETIF, "%s tx mbox failed", netif->netif_name);
        unlock_netif(netif);
        return nullptr;
    }
    assert(buf->len == buf->size);
    Pktbuf *pktbuf = (*(Pktbuf **)(buf->data));
    pktbuf_reset_acc(pktbuf);
    kfree(buf->data);
    unlock_netif(netif);
    return pktbuf;
}

Pktbuf *netif_get_in(Netif *netif, long tmo) {
    lock_netif(netif);
    if (netif->state != NETIF_ACTIVE) {
        dbg_error(DBG_NETIF, "netif is not actived");
        unlock_netif(netif);
        return nullptr;
    }
    assert(netif->rx_mbox_id != NETIF_MBOX_ID_NULL);
    Mboxbuf __buf, *buf = &__buf;
    buf->size = sizeof(Pktbuf *);
    buf->len = buf->size;
    buf->data = kmalloc(sizeof(Pktbuf *));
    int ret = ipc_mbox_recv(netif->rx_mbox_id, buf, -1);
    assert(ret == 0 || ret == -E_TIMEOUT || ret == -1 || ret == -E_MBX_EMPTY);
    if (ret == -E_TIMEOUT || ret == -E_MBX_EMPTY) {
        dbg_warning(DBG_NETIF, "%s rx mbox empty", netif->netif_name);
        unlock_netif(netif);
        return nullptr;
    }
    if (ret == -1) {
        dbg_error(DBG_NETIF, "%s rx mbox failed", netif->netif_name);
        unlock_netif(netif);
        return nullptr;
    }
    assert(buf->len == buf->size);
    Pktbuf *pktbuf = (*(Pktbuf **)(buf->data));
    pktbuf_reset_acc(pktbuf);
    kfree(buf->data);
    unlock_netif(netif);
    return pktbuf;
}

int netif_out(Netif *netif, Ipaddr *ipaddr, Pktbuf *buf) {
    int ret;
    if (netif->link_layer) {
        // ret = ether_raw_out(netif, NET_PROTOCOL_ARP, ether_broadcast_addr(), buf);
        ret = netif->link_layer->out(netif, ipaddr, buf);
        if (ret < 0) {
            dbg_warning(DBG_NETIF, "netif link out error: %d", ret);
            return ret;
        }
        return NET_OK;
    } else {
        ret = netif_put_out(netif, buf, -1);
        if (ret != 0) {
            dbg_warning(DBG_NETIF, "send to netif queue failed. err: %d", ret);
            return ret;
        }
        return netif->ops->xmit(netif);
    }
}

Netif *netif_get_default(void) { return netif_default; }
