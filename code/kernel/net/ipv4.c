#include "ipv4.h"

#include "nettool.h"
#include "proc.h"
#include "protocol.h"
#include "stdio.h"
#include "icmpv4.h"

extern Spinlock print_struct_lock;

static uint16_t packet_id = 0;

#if DBG_DISP_ENABLED(DBG_IP)
static void dump_ip_packet(Ipv4_pkt *pkt) {
    Ipv4_hdr *ip_hdr = (Ipv4_hdr *)&pkt->hdr;
    acquire(&print_struct_lock);
    cprintf("--------------- ip ------------------ \n");
    cprintf("    Version:%d\n", ip_hdr->version);
    cprintf("    Header len:%d bytes\n", ipv4_hdr_size(pkt));
    cprintf("    Totoal len: %d bytes\n", ip_hdr->total_len);
    cprintf("    Id:%d\n", ip_hdr->id);
    cprintf("    TTL: %d\n", ip_hdr->ttl);
    cprintf("    Protocol: %d\n", ip_hdr->protocol);
    cprintf("    Header checksum: 0x%04x\n", ip_hdr->hdr_checksum);
    dump_ip_buf("    src ip:", ip_hdr->dest_ip);
    cprintf("\n");
    dump_ip_buf("    dest ip:", ip_hdr->src_ip);
    cprintf("\n");
    cprintf("--------------- ip end ------------------ \n");
    release(&print_struct_lock);
}
#else
#define dump_ip_packet(pkt)
#endif

int ipv4_init(void) {
    dbg_info(DBG_IP, "init ip\n");

    dbg_info(DBG_IP, "done.");
    return NET_OK;
}

static void iphdr_ntohs(Ipv4_pkt *pkt) {
    pkt->hdr.total_len = x_ntohs(pkt->hdr.total_len);
    pkt->hdr.id = x_ntohs(pkt->hdr.id);
    pkt->hdr.frag_all = x_ntohs(pkt->hdr.frag_all);
}

static void iphdr_htons(Ipv4_pkt *pkt) {
    pkt->hdr.total_len = x_htons(pkt->hdr.total_len);
    pkt->hdr.id = x_htons(pkt->hdr.id);
    pkt->hdr.frag_all = x_ntohs(pkt->hdr.frag_all);
}

static int is_pkt_ok(Ipv4_pkt *pkt, int size) {
    if (pkt->hdr.version != NET_VERSION_IPV4) {
        dbg_warning(DBG_IP, "invalid ip version, only support ipv4!\n");
        return -E_NET_NOT_SUPPORT;
    }
    int hdr_len = ipv4_hdr_size(pkt);
    if (hdr_len < sizeof(Ipv4_hdr)) {
        dbg_warning(DBG_IP, "IPv4 header error: %d!", hdr_len);
        return -E_NET_SIZE;
    }
    int total_size = x_ntohs(pkt->hdr.total_len);
    if ((total_size < sizeof(Ipv4_hdr)) || (size < total_size)) {
        dbg_warning(DBG_IP, "ip packet size error: %d!\n", total_size);
        return -E_NET_SIZE;
    }
    if (pkt->hdr.hdr_checksum) {
        uint16_t c = checksum16((uint16_t *)pkt, hdr_len, 0, 1);
        if (c != 0) {
            dbg_warning(DBG_IP, "Bad checksum: %0x(correct is: %0x)\n", pkt->hdr.hdr_checksum, c);
            return -E_NET_DATA;
        }
    }
    return NET_OK;
}

static int ip_normal_in(Netif *netif, Pktbuf *buf, Ipaddr *src, Ipaddr *dest) {
    Ipv4_pkt *pkt = (Ipv4_pkt *)pktbuf_data(buf);
    dump_ip_packet(pkt);
    switch (pkt->hdr.protocol) {
        case NET_PROTOCOL_ICMPv4: {
            int ret = icmpv4_in(src, &netif->ipaddr, buf);
            if (ret < 0) {
                dbg_warning(DBG_IP, "icmp in failed.\n");
                return ret;
            }
            return NET_OK;
        }
        case NET_PROTOCOL_UDP:
            iphdr_htons(pkt);
            icmpv4_out_unreach(src, &netif->ipaddr, ICMPv4_UNREACH_PORT, buf); 
            break;
        case NET_PROTOCOL_TCP: break;
        default: dbg_warning(DBG_IP, "unknown protocol %d, drop it.\n", pkt->hdr.protocol); break;
    }
    return -E_NET_MATCH;
}

int ipv4_in(Netif *netif, Pktbuf *buf){
    dbg_info(DBG_IP, "IP in!\n");
    int ret = pktbuf_set_cont(buf, sizeof(Ipv4_hdr));
    if (ret < 0) {
        dbg_error(DBG_IP, "adjust header failed. ret=%d\n", ret);
        return ret;
    }
    Ipv4_pkt *pkt = (Ipv4_pkt *)pktbuf_data(buf);
    if (is_pkt_ok(pkt, buf->total_size) != NET_OK) {
        dbg_warning(DBG_IP, "packet is broken. drop it.\n");
        return ret;
    }
    iphdr_ntohs(pkt);
    ret = pktbuf_resize(buf, pkt->hdr.total_len);
    if (ret < 0) {
        dbg_error(DBG_IP, "ip packet resize failed. ret=%d\n", ret);
        return ret;
    }
    Ipaddr dest_ip, src_ip;
    ipaddr_from_buf(&dest_ip, pkt->hdr.dest_ip);
    ipaddr_from_buf(&src_ip, pkt->hdr.src_ip);
    if (!ipaddr_is_match(&dest_ip, &netif->ipaddr, &netif->netmask)) {
        dbg_warning(DBG_IP, "ipaddr not match\n");
        return -E_NET_MATCH;
    }
    ret = ip_normal_in(netif, buf, &src_ip, &dest_ip);
    return ret;
}

int ipv4_out(uint8_t protocol, Ipaddr *dest, Ipaddr *src, Pktbuf *buf){
    dbg_info(DBG_IP, "send an ip packet.\n");
    int ret = pktbuf_add_header(buf, sizeof(Ipv4_hdr), 1);
    if (ret < 0) {
        dbg_error(DBG_IP, "no enough space for ip header, curr size: %d\n", buf->total_size);
        return -E_NET_SIZE;
    }
    Ipv4_pkt *pkt = (Ipv4_pkt *)pktbuf_data(buf);
    pkt->hdr.shdr_all = 0;
    pkt->hdr.version = NET_VERSION_IPV4;
    set_header_size(pkt, sizeof(Ipv4_hdr));
    pkt->hdr.total_len = buf->total_size;
    pkt->hdr.id = packet_id++;  
    pkt->hdr.frag_all = 0;    
    pkt->hdr.ttl = NET_IP_DEF_TTL;
    pkt->hdr.protocol = protocol;
    pkt->hdr.hdr_checksum = 0;
    ipaddr_to_buf(src, pkt->hdr.src_ip);
    ipaddr_to_buf(dest, pkt->hdr.dest_ip);
    iphdr_htons(pkt);
    pktbuf_reset_acc(buf);
    pkt->hdr.hdr_checksum = pktbuf_checksum16(buf, ipv4_hdr_size(pkt), 0, 1);
    dump_ip_packet(pkt);
    ret = netif_out(netif_get_default(), dest, buf);
    if (ret < 0) {
        dbg_warning(DBG_IP, "send ip packet failed. error = %d\n", ret);
        return ret;
    }

    return NET_OK;
}