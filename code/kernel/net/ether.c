#include "ether.h"

#include "netif.h"
#include "pktbuf.h"
#include "protocol.h"
#include "stdio.h"
#include "nettool.h"
#include "arp.h"
#include "ipv4.h"

extern Spinlock print_struct_lock;

#if DBG_DISP_ENABLED(DBG_ETHER)
void ether_pkt_dump(char* title, Ether_pkt* pkt, size_t size) {
    Ether_hdr* hdr = (Ether_hdr *)pkt;
    acquire(&print_struct_lock);
    cprintf("\nEther_pkt %s dump....................................\n\n", title);
    cprintf("\tlen: %lu bytes\n", size);
    dump_mac("\tdest:", hdr->dest);
    dump_mac("\tsrc:", hdr->src);
    cprintf("\ttype: %04x - ", x_ntohs(hdr->protocol));
    switch (x_ntohs(hdr->protocol)) {
        case NET_PROTOCOL_ARP: cprintf("ARP\n"); break;
        case NET_PROTOCOL_IPv4: cprintf("IP\n"); break;
        default: cprintf("Unknown\n"); break;
    }
    cprintf("\n");
    cprintf("\nend of Ether_pkt %s dump.............................\n", title);
    release(&print_struct_lock);
}
#else
#define ether_pkt_dump(title, pkt, size)
#endif

static int ether_open(Netif* netif) { return arp_make_gratuitous(netif); }

static void ether_close(Netif* netif) {
    return arp_clear(netif);
}

static int is_pkt_ok(Ether_pkt* frame, size_t total_size) {
    if (total_size > (sizeof(Ether_hdr) + ETH_MTU)) {
        dbg_warning(DBG_ETHER, "frame size too big: %d", total_size);
        return -E_NET_SIZE;
    }
    if (total_size < (sizeof(Ether_hdr))) {
        dbg_warning(DBG_ETHER, "frame size too small: %d", total_size);
        return -E_NET_SIZE;
    }
    return NET_OK;
}

static int ether_in(Netif* netif, Pktbuf* buf) {
    dbg_info(DBG_ETHER, "ether in:");
    pktbuf_set_cont(buf, sizeof(Ether_hdr));
    Ether_pkt* pkt = (Ether_pkt*)pktbuf_data(buf);
    int ret;
    if ((ret = is_pkt_ok(pkt, buf->total_size)) != NET_OK) {
        dbg_error(DBG_ETHER, "ether pkt error");
        return ret;
    }
    ether_pkt_dump("ether in", pkt, buf->total_size);

    switch (x_ntohs(pkt->hdr.protocol)) {
        case NET_PROTOCOL_ARP: {
            ret = pktbuf_remove_header(buf, sizeof(Ether_hdr));
            if (ret < 0) {
                dbg_error(DBG_ETHER, "remove ether header failed, packet size: %d", buf->total_size);
                return -E_NET_SIZE;
            }
            return arp_in(netif, buf);
        }
        case NET_PROTOCOL_IPv4: {
            arp_update_from_ipbuf(netif, buf);
            ret = pktbuf_remove_header(buf, sizeof(Ether_hdr));
            if (ret < 0) {
                dbg_error(DBG_ETHER, "remove ethernet header failed, packet size: %d", buf->total_size);
                return -E_NET_SIZE;
            }
            ret = ipv4_in(netif, buf);
            if (ret < 0) {
                dbg_warning(DBG_ETHER, "process in buf failed. ret=%d", ret);
                return ret;
            }
            break;
        }
        default: dbg_warning(DBG_ETHER, "unknown packet, ignore it."); return -E_NET_NOT_SUPPORT;
    }

    return NET_OK; 
}

static int ether_out(Netif* netif, Ipaddr* dest, Pktbuf* buf) { 
   if (ipaddr_is_equal(&netif->ipaddr, dest)){
       return ether_raw_out(netif, NET_PROTOCOL_IPv4, (uint8_t*)netif->hwaddr.addr, buf);
   }
   const uint8_t* hwaddr = arp_find(netif, dest);
   if (hwaddr) {
       return ether_raw_out(netif, NET_PROTOCOL_IPv4, hwaddr, buf);
   }else{
       return arp_resolve(netif, dest, buf);
   }
}

int ether_init(void) {
    static Link_layer link_layer = {
        .type = NETIF_TYPE_ETHER,
        .open = ether_open,
        .close = ether_close,
        .in = ether_in,
        .out = ether_out,
    };
    dbg_info(DBG_ETHER, "init ether");
    int ret = netif_register_layer(NETIF_TYPE_ETHER, &link_layer);
    if (ret < 0) {
        dbg_info(DBG_ETHER, "error = %d", ret);
        return ret;
    }
    dbg_info(DBG_ETHER, "done.");
    return NET_OK;
}

uint8_t* ether_broadcast_addr(void){
    static uint8_t broadcast_addr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    return broadcast_addr;
}

int ether_raw_out(Netif* netif, Protocol protocol, const uint8_t* dest, Pktbuf* buf){
    int ret;
    size_t size = buf->total_size;
    if (size < ETH_DATA_MIN) {
        dbg_info(DBG_ETHER, "resize from %d to %lu", size, (size_t)ETH_DATA_MIN);
        ret = pktbuf_resize(buf, ETH_DATA_MIN);
        if (ret < 0) {
            dbg_error(DBG_ETHER, "resize failed: %d", ret);
            return ret;
        }
        pktbuf_reset_acc(buf);
        pktbuf_seek(buf, size);
        pktbuf_fill(buf, 0, ETH_DATA_MIN - size);
    }
    ret = pktbuf_add_header(buf, sizeof(Ether_hdr), true);
    if (ret < 0) {
        dbg_error(DBG_ETHER, "add header failed: %d", ret);
        return -E_NET_SIZE;
    }
    Ether_pkt* pkt = (Ether_pkt*)pktbuf_data(buf);
    memcpy(pkt->hdr.dest, dest, ETH_HWA_SIZE);
    memcpy(pkt->hdr.src, netif->hwaddr.addr, ETH_HWA_SIZE);
    pkt->hdr.protocol = x_htons(protocol);
    ether_pkt_dump("ether out", pkt, size);
    if (memcmp(netif->hwaddr.addr, dest, ETH_HWA_SIZE) == 0) {
       return netif_put_in(netif, buf, -1);
    } else{
        ret = netif_put_out(netif, buf, -1);
        if (ret < 0) {
            dbg_warning(DBG_ETHER, "put pkt out failed: %d", ret);
            return ret;
        }
        return netif->ops->xmit(netif);
    }
}