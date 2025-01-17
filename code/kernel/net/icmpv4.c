#include "icmpv4.h"

#include "proc.h"
#include "protocol.h"
#include "stdio.h"
#include "nettool.h"
#include "raw.h"

extern Spinlock print_struct_lock;

#if DBG_DISP_ENABLED(DBG_ICMP)
static void dump_icmp_packet(char *title, Icmpv4_pkt *pkt) {
    acquire(&print_struct_lock);
    cprintf("--------------- %s ------------------ \n", title);
    cprintf("type: %d\n", pkt->hdr.type);
    cprintf("code: %d\n", pkt->hdr.code);
    cprintf("checksum: %x\n", x_ntohs(pkt->hdr.checksum));
    cprintf("------------------------------------- \n");
    release(&print_struct_lock);
}
#else
#define dump_icmp_packet(title, packet)
#endif 

int icmpv4_init(void){
    dbg_info(DBG_ICMP, "init icmp");
    dbg_info(DBG_ICMP, "done");
    return NET_OK;
}

static int is_pkt_ok(Icmpv4_pkt *pkt, size_t size, Pktbuf *buf) {
    if (size <= sizeof(Icmpv4_hdr)) {
        dbg_warning(DBG_ICMP, "size error: %d", size);
        return -E_NET_SIZE;
    }
    uint16_t checksum = pktbuf_checksum16(buf, size, 0, 1);
    if (checksum != 0) {
        dbg_warning(DBG_ICMP, "Bad checksum %0x(correct is: %0x)\n", pkt->hdr.checksum, checksum);
        return -E_NET_CHECKSUM;
    }
    return NET_OK;
}

static int icmpv4_out(Ipaddr *dest, Ipaddr *src, Pktbuf *buf) {
    Icmpv4_pkt *pkt = (Icmpv4_pkt *)pktbuf_data(buf);
    pktbuf_reset_acc(buf);
    pkt->hdr.checksum = pktbuf_checksum16(buf, buf->total_size, 0, 1);
    dump_icmp_packet("icmp reply", pkt);
    return ipv4_out(NET_PROTOCOL_ICMPv4, dest, src, buf);
}

static int icmpv4_echo_reply(Ipaddr *dest, Ipaddr *src, Pktbuf *buf) {
    Icmpv4_pkt *pkt = (Icmpv4_pkt *)pktbuf_data(buf);
    pkt->hdr.type = ICMPv4_ECHO_REPLY;
    pkt->hdr.checksum = 0;
    return icmpv4_out(dest, src, buf);
}

int icmpv4_in(Ipaddr *src_ip, Ipaddr *netif_ip, Pktbuf *buf) {
    dbg_info(DBG_ICMP, "icmp in !\n");
    Ipv4_pkt *ip_pkt = (Ipv4_pkt *)pktbuf_data(buf);
    int iphdr_size = ip_pkt->hdr.shdr * 4;  // ip包头和icmp包头连续
    int ret = pktbuf_set_cont(buf, sizeof(Icmpv4_hdr) + iphdr_size);
    if (ret < 0) {
        dbg_error(DBG_ICMP, "set icmp cont failed");
        return ret;
    }
    ip_pkt = (Ipv4_pkt *)pktbuf_data(buf);
    Icmpv4_pkt *icmp_pkt = (Icmpv4_pkt *)(pktbuf_data(buf) + iphdr_size);
    pktbuf_seek(buf, iphdr_size);
    if ((ret = is_pkt_ok(icmp_pkt, buf->total_size, buf)) != NET_OK) {
        dbg_warning(DBG_ICMP, "icmp pkt error.drop it. ret=%d", ret);
        return ret;
    }
    // 根据类型做不同的处理
    switch (icmp_pkt->hdr.type) {
        case ICMPv4_ECHO_REQUEST: {
#if DBG_DISP_ENABLED(DBG_ICMP)
            dump_ip(DBG_ICMP, "icmp request, ip:", src_ip);
#endif
            ret = pktbuf_remove_header(buf, iphdr_size);
            if (ret < 0) {
                dbg_error(DBG_IP, "remove ip header failed. ret = %d\n", ret);
                return -E_NET_SIZE;
            }
            pktbuf_reset_acc(buf);
            return icmpv4_echo_reply(src_ip, netif_ip, buf);
        }
        default: {
            ret = raw_in(buf);
            if (ret < 0) {
                dbg_warning(DBG_ICMP, "raw in failed.");
                return ret;
            }
            return NET_OK;
        }
    }
}

int icmpv4_out_unreach(Ipaddr *dest_addr, Ipaddr *src, uint8_t code, Pktbuf *ip_buf){
    size_t copy_size = ipv4_hdr_size((Ipv4_pkt *)pktbuf_data(ip_buf)) + 576;
    if (copy_size > ip_buf->total_size) { copy_size = ip_buf->total_size; }
    Pktbuf *new_buf = pktbuf_alloc(copy_size + sizeof(Icmpv4_hdr) + 4);
    if (new_buf == nullptr) {
        dbg_warning(DBG_ICMP, "alloc buf failed");
        return -E_NET_NONE;
    }
    int ret = pktbuf_set_cont(new_buf, sizeof(Icmpv4_pkt));
    if (ret < 0) {
        dbg_error(DBG_ICMP, "set cont faile.");
        return -E_NET_SIZE;
    }
    Icmpv4_pkt *pkt = (Icmpv4_pkt *)pktbuf_data(new_buf);
    pkt->hdr.type = ICMPv4_UNREACH;
    pkt->hdr.code = code;
    pkt->hdr.checksum = 0;
    pkt->reverse = 0;
    pktbuf_reset_acc(ip_buf);
    pktbuf_seek(new_buf, sizeof(Icmpv4_hdr) + 4); 
    ret = pktbuf_copy(new_buf, ip_buf, copy_size);
    // pktbuf_free(ip_buf);
    if (ret < 0) {
        dbg_error(DBG_ICMP, "copy ip buf failed. ret = %d", ret);
        pktbuf_free(new_buf);
        return ret;
    }
    ret = icmpv4_out(dest_addr, src, new_buf);
    if (ret < 0) {
        dbg_error(DBG_ICMP, "send icmp unreach failed.");
        pktbuf_free(new_buf);
        return ret;
    }
    return NET_OK;
}