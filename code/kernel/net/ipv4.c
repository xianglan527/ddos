#include "ipv4.h"

#include "icmpv4.h"
#include "nettool.h"
#include "proc.h"
#include "protocol.h"
#include "raw.h"
#include "slab.h"
#include "stdio.h"
#include "udp.h"
#include "tcp_in.h"

extern Spinlock print_struct_lock;

static uint16_t packet_id = 0;

Ip_frag_head ip_frag_head;

Timer *frag_timer;

List_entry rt_list;
Spinlock rt_list_lock;

static void frag_free_nolock(Ip_frag *frag);

#if DBG_DISP_ENABLED(DBG_IP)
static void dump_ip_packet(Ipv4_pkt *pkt) {
    Ipv4_hdr *ip_hdr = (Ipv4_hdr *)&pkt->hdr;
    acquire(&print_struct_lock);
    cprintf("--------------- ip ------------------ \n");
    cprintf("    Version:%d\n", ip_hdr->version);
    cprintf("    Header len:%d bytes\n", ipv4_hdr_size(pkt));
    cprintf("    Totoal len: %d bytes\n", ip_hdr->total_len);
    cprintf("    Id:%d\n", ip_hdr->id);
    cprintf("    Frag offset: 0x%04x\n", ip_hdr->offset);
    cprintf("    More frag: %d\n", ip_hdr->more);
    cprintf("    TTL: %d\n", ip_hdr->ttl);
    cprintf("    Protocol: %d\n", ip_hdr->protocol);
    cprintf("    Header checksum: 0x%04x\n", ip_hdr->hdr_checksum);
    dump_ip_buf("    src ip:", ip_hdr->src_ip);
    cprintf("\n");
    dump_ip_buf("    dest ip:", ip_hdr->dest_ip);
    cprintf("\n");
    cprintf("--------------- ip end ------------------ \n");
    release(&print_struct_lock);
}

static void dump_ip_frag_buf_list(Ip_frag *ip_frag) {
    List_entry *le;
    int index = 0;
    acquire(&ip_frag->ip_frag_buf_list_lock);
    list_for_each(le, &ip_frag->ip_frag_buf_list) {
        Pktbuf *buf = le2pktbuf_ip_frag(le);
        Ipv4_pkt *pkt = (Ipv4_pkt *)pktbuf_data(buf);
        cprintf("\t\tB%d[%d - %d], ", index++, get_frag_start(pkt), get_frag_end(pkt) - 1);
    }
    release(&ip_frag->ip_frag_buf_list_lock);
}

static void dump_ip_frags(void) {
    int index = 0;
    acquire(&ip_frag_head.ip_frag_list_lock);
    acquire(&print_struct_lock);
    List_entry *le;
    cprintf("\n------------- ip_frag list start ---------- \n");
    list_for_each(le, &ip_frag_head.ip_frag_list) {
        Ip_frag *frag = le2ip_frag(le);
        cprintf("[%d]:\n", index++);
        dump_ip_buf("\tip:", frag->ip.a_addr);
        cprintf("\tid: %d\n", frag->id);
        cprintf("\ttmo: %lu\n", frag->tmo);
        cprintf("\tbufs: %lu\n", list_count(&frag->ip_frag_buf_list));
        cprintf("\tbufs:\n");
        dump_ip_frag_buf_list(frag);
        cprintf("\n");
    }
    cprintf("\n------------- ip_frag list end ---------- \n");
    release(&print_struct_lock);
    release(&ip_frag_head.ip_frag_list_lock);
}

#else
#define dump_ip_packet(pkt)
#define dump_ip_frag_buf_list(ip_frag)
#define dump_ip_frags()
#endif

#if DBG_DISP_ENABLED(DBG_IP)
void dump_rt_nlist(void) {
    acquire(&rt_list_lock);
    acquire(&print_struct_lock);
    cprintf("\n------------- dump route table start ---------- \n");
    int index = 0;
    List_entry *le;
    list_for_each(le, &rt_list) {
        Rentry *rentry = le2rentry(le);
        cprintf("%d: ", index++);
        dump_ip(DBG_IP, "net:", &rentry->net);
        cprintf("\t");
        dump_ip(DBG_IP, "mask:", &rentry->mask);
        cprintf("\t");
        dump_ip(DBG_IP, "next_hop:", &rentry->next_hop);
        cprintf("\t");
        cprintf("netif name: %s", rentry->netif->netif_name);
        cprintf("\n");
    }
    cprintf("------------- dump route table end ---------- \n");
    release(&print_struct_lock);
    release(&rt_list_lock);
}
#else
#define dump_rt_nlist()
#endif

static void frag_tmo(Timer *timer) {
    int changed_cnt = 0;
    List_entry *le;
    acquire(&ip_frag_head.ip_frag_list_lock);
    if (list_empty(&ip_frag_head.ip_frag_list)) {
        release(&ip_frag_head.ip_frag_list_lock);
        return;
    }
    le = list_next(&ip_frag_head.ip_frag_list);
    while (le != &ip_frag_head.ip_frag_list) {
        Ip_frag *frag = le2ip_frag(le);
        le = list_next(le);
        if (--frag->tmo == 0) {
            changed_cnt++;
            frag_free_nolock(frag);
        }
    }
    release(&ip_frag_head.ip_frag_list_lock);
    if (changed_cnt) {
        dbg_info(DBG_IP, "%d ip frag list changed.", changed_cnt);
        dump_ip_frags();
    }
}

static int frag_init(void) {
    list_init(&ip_frag_head.ip_frag_list);
    initlock(&ip_frag_head.ip_frag_list_lock, "ip_frag_list_lock");
    ip_frag_head.count = 0;
    frag_timer = timer_func_add(frag_tmo, "frag_timer", IP_FRAG_SCAN_PERIOD * 1000, TIMER_RELOAD);
    assert(frag_timer != nullptr);
    return NET_OK;
}

int ipv4_init(void) {
    dbg_info(DBG_IP, "init ip\n");
    int ret = frag_init();
    if (ret < 0) {
        dbg_error(DBG_IP, "failed. ret = %d", ret);
        return ret;
    }
    rts_init();
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

static void frag_free_buf_list_nolock(Ip_frag *frag) {
    List_entry *le;
    while ((le = list_next(&frag->ip_frag_buf_list)) != &frag->ip_frag_buf_list) {
        list_del(le);
        Pktbuf *buf = le2pktbuf_ip_frag(le);
        pktbuf_free(buf);
    }
}

static void frag_free_buf_list(Ip_frag *frag) {
    acquire(&frag->ip_frag_buf_list_lock);
    frag_free_buf_list_nolock(frag);
    release(&frag->ip_frag_buf_list_lock);
}

static Ip_frag *frag_alloc_nolock(void) {
    List_entry *le;
    Ip_frag *frag = nullptr;
    if (ip_frag_head.count == IP_FRAGS_MAX_NR) {
        le = list_prev(&ip_frag_head.ip_frag_list);
        frag = le2ip_frag(le);
        frag_free_buf_list(frag);
        list_del(le);
        return frag;
    }
    frag = kmalloc(sizeof(Ip_frag));
    assert(frag != nullptr);
    list_init(&frag->ip_frag_buf_list);
    initlock(&frag->ip_frag_buf_list_lock, "ip_frag_buf_list_lock");
    list_add_after(&ip_frag_head.ip_frag_list, &frag->ip_frag_link);
    ip_frag_head.count++;
    return frag;
}

static Ip_frag *frag_alloc(void) {
    Ip_frag *frag = nullptr;
    acquire(&ip_frag_head.ip_frag_list_lock);
    frag = frag_alloc_nolock();
    release(&ip_frag_head.ip_frag_list_lock);
    return frag;
}

static void frag_free_nolock(Ip_frag *frag) {
    frag_free_buf_list(frag);
    list_del(&frag->ip_frag_link);
    ip_frag_head.count--;
    kfree(frag);
}

static void frag_free(Ip_frag *frag) {
    acquire(&ip_frag_head.ip_frag_list_lock);
    frag_free_nolock(frag);
    release(&ip_frag_head.ip_frag_list_lock);
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
        uint16_t c = checksum16(0, (uint16_t *)pkt, hdr_len, 0, 1);
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
        case NET_PROTOCOL_UDP: {
            int ret = udp_in(buf, src, dest);
            if (ret < 0) {
                dbg_warning(DBG_IP, "udp in error. ret = %d\n", ret);
                if (ret == -E_NET_UNREACH) {
                    iphdr_htons(pkt);
                    icmpv4_out_unreach(src, &netif->ipaddr, ICMPv4_UNREACH_PORT, buf);
                }
                return ret;
            }
            return NET_OK;
        }
        case NET_PROTOCOL_TCP: {
            pktbuf_remove_header(buf, ipv4_hdr_size(pkt));
            int ret = tcp_in(buf, src, dest);
            if (ret < 0) {
                dbg_warning(DBG_IP, "udp in error. ret = %d\n", ret);
                return ret;
            }
            return NET_OK;
        }
        default: {
            dbg_warning(DBG_IP, "unknown protocol %d, drop it.\n", pkt->hdr.protocol);
            int ret = raw_in(buf);
            if (ret < 0) { dbg_warning(DBG_IP, "raw in error. ret = %d\n", ret); }
            return ret;
        }
    }
    return -E_NET_MATCH;
}

static Ip_frag *frag_find_nolock(Ipaddr *ip, uint16_t id) {
    List_entry *le;
    list_for_each(le, &ip_frag_head.ip_frag_list) {
        Ip_frag *frag = le2ip_frag(le);
        if (ipaddr_is_equal(ip, &frag->ip) && (id == frag->id)) {
            if (list_next(&ip_frag_head.ip_frag_list) != &frag->ip_frag_link) {
                list_del(&frag->ip_frag_link);
                list_add_after(&ip_frag_head.ip_frag_list, &frag->ip_frag_link);
            }
            return frag;
        }
    }
    return nullptr;
}

static void frag_add(Ip_frag *frag, Ipaddr *ip, uint16_t id) {
    ipaddr_copy(&frag->ip, ip);
    frag->tmo = IP_FRAG_TMO / IP_FRAG_SCAN_PERIOD;
    frag->id = id;
    // list_init(&frag->ip_frag_buf_list);
    // list_add_after(&ip_frag_head.ip_frag_list, &frag->ip_frag_link);
}

static int frag_insert(Ip_frag *frag, Pktbuf *buf, Ipv4_pkt *pkt) {
    int ret = NET_OK;
    acquire(&frag->ip_frag_buf_list_lock);
    if (list_count(&frag->ip_frag_buf_list) >= IP_FRAG_MAX_BUF_NR) {
        dbg_warning(DBG_IP, "too many buf on frag. drop it.\n");
        ret = -E_NET_FULL;
    }
    release(&frag->ip_frag_buf_list_lock);
    if (ret != NET_OK) {
        frag_free(frag);
        return ret;
    }
    List_entry *le;
    acquire(&frag->ip_frag_buf_list_lock);
    list_for_each(le, &frag->ip_frag_buf_list) {
        Pktbuf *cur_buf = le2pktbuf_ip_frag(le);
        Ipv4_pkt *cur_pkt = (Ipv4_pkt *)pktbuf_data(cur_buf);
        uint16_t cur_start = get_frag_start(cur_pkt);
        if (get_frag_start(pkt) == cur_start) {
            release(&frag->ip_frag_buf_list_lock);
            return -E_NET_EXIST;
        } else if (get_frag_end(pkt) <= cur_start) {
            list_add_before(le, &buf->ip_frag_link);
            release(&frag->ip_frag_buf_list_lock);
            return NET_OK;
        }
    }
    list_add(&frag->ip_frag_buf_list, &buf->ip_frag_link);
    release(&frag->ip_frag_buf_list_lock);
    return NET_OK;
}

static bool frag_is_all_arrived(Ip_frag *frag) {
    size_t offset = 0;
    Ipv4_pkt *pkt = nullptr;
    List_entry *le;
    acquire(&frag->ip_frag_buf_list_lock);
    list_for_each(le, &frag->ip_frag_buf_list) {
        Pktbuf *buf = le2pktbuf_ip_frag(le);
        pkt = (Ipv4_pkt *)pktbuf_data(buf);
        size_t cur_offset = get_frag_start(pkt);
        if (cur_offset != offset) {
            release(&frag->ip_frag_buf_list_lock);
            return false;
        }
        offset += get_data_size(pkt);
    }
    release(&frag->ip_frag_buf_list_lock);
    return pkt ? !pkt->hdr.more : false;
}

static Pktbuf *frag_join(Ip_frag *frag) {
    List_entry *le;
    Pktbuf *target = nullptr;
    Pktbuf *cur_buf = nullptr;
    acquire(&frag->ip_frag_buf_list_lock);
    if (list_count(&frag->ip_frag_buf_list) == 0) {
        release(&frag->ip_frag_buf_list_lock);
        frag_free(frag);
        dbg_warning(DBG_IP, "ip frag buf list is empty. \n");
        return nullptr;
    } else {
        le = list_next(&frag->ip_frag_buf_list);
        target = le2pktbuf_ip_frag(le);
        while ((le = list_next(&target->ip_frag_link)) != &frag->ip_frag_buf_list) {
            cur_buf = le2pktbuf_ip_frag(le);
            Ipv4_pkt *pkt = (Ipv4_pkt *)pktbuf_data(cur_buf);
            pktbuf_remove_header(cur_buf, ipv4_hdr_size(pkt));
            list_del(&cur_buf->ip_frag_link);
            pktbuf_join(target, cur_buf);
        }
        release(&frag->ip_frag_buf_list_lock);
        Pktbuf *ret_buf = pktbuf_alloc(target->total_size);
        assert(ret_buf != nullptr);
        pktbuf_reset_acc(target);
        int ret = pktbuf_copy(ret_buf, target, target->total_size);
        assert(ret == NET_OK);
        frag_free(frag);
        return ret_buf;
    }
}

static int ip_frag_in(Netif *netif, Pktbuf *buf, Ipaddr *src, Ipaddr *dest) {
    Ipv4_pkt *cur = (Ipv4_pkt *)pktbuf_data(buf);
    // static int count = 0;
    // if(count != 0){
    //     return -E_NET;
    // }
    // count++;
    acquire(&ip_frag_head.ip_frag_list_lock);
    Ip_frag *frag = frag_find_nolock(src, cur->hdr.id);
    if (!frag) {
        frag = frag_alloc_nolock();
        frag_add(frag, src, cur->hdr.id);
    }
    release(&ip_frag_head.ip_frag_list_lock);
    int ret = frag_insert(frag, buf, cur);
    if (ret < 0) {
        dbg_warning(DBG_IP, "frag insert failed.");
        return ret;
    }
    if (frag_is_all_arrived(frag)) {
        Pktbuf *full_buf = frag_join(frag);
        if (full_buf) {
            int ret = ip_normal_in(netif, full_buf, src, dest);
            if (ret < 0) {
                dbg_warning(DBG_IP, "ip frag in error. ret=%d\n", ret);
                pktbuf_free(full_buf);
                return NET_OK;
            }
        }
    }
    dump_ip_frags();
    return NET_OK;
}

int ipv4_in(Netif *netif, Pktbuf *buf) {
    dbg_info(DBG_IP, "IP in!\n");
    // if (strncmp(netif->netif_name, "net0if", 6) == 0) cprintf("xxxxxxxxxxxxxxx\n");
    int ret = pktbuf_set_cont(buf, sizeof(Ipv4_hdr));
    if (ret < 0) {
        dbg_error(DBG_IP, "adjust header failed. ret=%d\n", ret);
        return ret;
    }
    Ipv4_pkt *pkt = (Ipv4_pkt *)pktbuf_data(buf);
    if (is_pkt_ok(pkt, buf->total_size) != NET_OK) {
        dbg_error(DBG_IP, "packet is broken. drop it.\n");
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
    // dump_ip_buf("src ip:", src_ip.a_addr);
    // Ipaddr ip;
    // ipaddr_from_str(&ip, "192.168.8.27");
    // if (ipaddr_is_equal(&src_ip, &ip)) { 
    //     cprintf("yyyyyyyyyyyy\n"); 
    // }
    if (!ipaddr_is_match(&dest_ip, &netif->ipaddr, &netif->netmask)) {
        dbg_warning(DBG_IP, "ipaddr not match\n");
        return -E_NET_MATCH;
    }
    if (pkt->hdr.offset || pkt->hdr.more) {
        ret = ip_frag_in(netif, buf, &src_ip, &dest_ip);
    } else {
        ret = ip_normal_in(netif, buf, &src_ip, &dest_ip);
    }
    return ret;
}

int ip_frag_out(uint8_t protocol, Ipaddr *dest, Ipaddr *src, Pktbuf *buf, Ipaddr *next, Netif *netif) {
    dbg_info(DBG_IP, "frag send an ip packet.\n");
    pktbuf_reset_acc(buf);
    size_t offset = 0;
    size_t total = buf->total_size;
    while (total) {
        size_t cur_size = total;
        if (cur_size > netif->mtu - sizeof(Ipv4_hdr)) { cur_size = netif->mtu - sizeof(Ipv4_hdr); }
        if (cur_size < total) { cur_size &= ~0x7; }
        Pktbuf *dest_buf = pktbuf_alloc(cur_size + sizeof(Ipv4_hdr));
        assert(dest_buf != nullptr);
        Ipv4_pkt *pkt = (Ipv4_pkt *)pktbuf_data(dest_buf);
        pkt->hdr.shdr_all = 0;
        pkt->hdr.version = NET_VERSION_IPV4;
        set_header_size(pkt, sizeof(Ipv4_hdr));
        pkt->hdr.total_len = dest_buf->total_size;
        pkt->hdr.id = packet_id;
        pkt->hdr.frag_all = 0;
        pkt->hdr.ttl = NET_IP_DEF_TTL;
        pkt->hdr.protocol = protocol;
        pkt->hdr.hdr_checksum = 0;
        // ipaddr_to_buf(src, pkt->hdr.src_ip);
        if (!src || ipaddr_is_any(src)) {
            ipaddr_to_buf(&netif->ipaddr, pkt->hdr.src_ip);
        } else {
            ipaddr_to_buf(src, pkt->hdr.src_ip);
        }
        ipaddr_to_buf(dest, pkt->hdr.dest_ip);
        pkt->hdr.offset = offset >> 3;
        pkt->hdr.more = total > cur_size;
        pktbuf_seek(dest_buf, sizeof(Ipv4_hdr));
        int ret = pktbuf_copy(dest_buf, buf, cur_size);
        if (ret < 0) {
            dbg_error(DBG_IP, "frag copy failed. error = %d.\n", ret);
            pktbuf_free(dest_buf);
            return ret;
        }
        iphdr_htons(pkt);
        pktbuf_seek(dest_buf, 0);
        pkt->hdr.hdr_checksum = pktbuf_checksum16(dest_buf, ipv4_hdr_size(pkt), 0, 1);
        dump_ip_packet(pkt);
        ret = netif_out(netif, next, dest_buf);
        if (ret < 0) {
            dbg_warning(DBG_IP, "ip send retor. ret = %d\n", ret);
            pktbuf_free(dest_buf);
            return ret;
        }
        total -= cur_size;
        offset += cur_size;
    }
    packet_id++;
    pktbuf_free(buf);
    return NET_OK;
}

int ipv4_out(uint8_t protocol, Ipaddr *dest, Ipaddr *src, Pktbuf *buf) {
    dbg_info(DBG_IP, "send an ip packet.\n");
    int ret = NET_OK;

    Rentry *rt = rt_find(dest);
    if (rt == nullptr) {
        dbg_error(DBG_IP, "send failed. no route.");
        return -E_NET_MATCH;
    }
    Ipaddr next_hop;
    if (ipaddr_is_any(&rt->next_hop)) {
        ipaddr_copy(&next_hop, dest);
    } else {
        ipaddr_copy(&next_hop, &rt->next_hop);
    }

    // Netif *netif = netif_get_default();
    Netif *netif = rt->netif;
    if (netif->mtu && ((buf->total_size + sizeof(Ipv4_hdr)) > netif->mtu)) {
        ret = ip_frag_out(protocol, dest, src, buf, &next_hop, netif);
        if (ret < 0) {
            dbg_warning(DBG_IP, "send ip frag packet failed. error = %d\n", ret);
            return ret;
        }
        return NET_OK;
    }
    ret = pktbuf_add_header(buf, sizeof(Ipv4_hdr), 1);
    if (ret < 0) {
        dbg_error(DBG_IP, "no enough space for ip header, cur size: %d\n", buf->total_size);
        return -E_NET_SIZE;
    }
    Ipv4_pkt *pkt = (Ipv4_pkt *)pktbuf_data(buf);
    pkt->hdr.shdr_all = 0;
    pkt->hdr.version = NET_VERSION_IPV4;
    set_header_size(pkt, sizeof(Ipv4_hdr));
    pkt->hdr.total_len = (uint16_t)buf->total_size;
    pkt->hdr.id = packet_id++;
    pkt->hdr.frag_all = 0;
    pkt->hdr.ttl = NET_IP_DEF_TTL;
    pkt->hdr.protocol = protocol;
    pkt->hdr.hdr_checksum = 0;
    // ipaddr_to_buf(src, pkt->hdr.src_ip);
    if (!src || ipaddr_is_any(src)) {
        ipaddr_to_buf(&netif->ipaddr, pkt->hdr.src_ip);
    } else {
        ipaddr_to_buf(src, pkt->hdr.src_ip);
    }
    ipaddr_to_buf(dest, pkt->hdr.dest_ip);
    iphdr_htons(pkt);
    pktbuf_reset_acc(buf);
    pkt->hdr.hdr_checksum = pktbuf_checksum16(buf, ipv4_hdr_size(pkt), 0, 1);
    dump_ip_packet(pkt);
    ret = netif_out(netif, &next_hop, buf);
    if (ret < 0) {
        dbg_warning(DBG_IP, "send ip packet failed. error = %d\n", ret);
        return ret;
    }
    return NET_OK;
}

void rts_init(void) {
    list_init(&rt_list);
    initlock(&rt_list_lock, "rt_list_lock");
}

static bool rt_has_exist(Ipaddr *net, Ipaddr *mask) {
    bool has_exist = false;
    acquire(&rt_list_lock);
    List_entry *le;
    list_for_each(le, &rt_list) {
        Rentry *rentry = le2rentry(le);
        if (ipaddr_is_equal(&rentry->net, net) && ipaddr_is_equal(&rentry->mask, mask)) {
            has_exist = true;
            break;
        }
    }
    release(&rt_list_lock);
    return has_exist;
}

void rt_add(Ipaddr *net, Ipaddr *mask, Ipaddr *next_hop, Netif *netif) {
    if (rt_has_exist(net, mask)) {
        dbg_warning(DBG_IP, "The RT entry has existed.");
        return;
    }
    Rentry *rentry = (Rentry *)kmalloc(sizeof(Rentry));
    assert(rentry != nullptr);
    ipaddr_copy(&rentry->net, net);
    ipaddr_copy(&rentry->mask, mask);
    ipaddr_copy(&rentry->next_hop, next_hop);
    rentry->netif = netif;
    rentry->mask_1_cnt = ipaddr_1_cnt(mask);
    acquire(&rt_list_lock);
    if (list_count(&rt_list) == IP_RTABLE_SIZE) {
        dbg_warning(DBG_IP, "The RT entry is full, and remove the last entry from the linked list.");
        assert(!list_empty(&rt_list));
        list_del(rt_list.prev);
        kfree(le2rentry(rt_list.prev));
    }
    list_add_after(&rt_list, &rentry->rentry_link);
    release(&rt_list_lock);
    dump_rt_nlist();
}

void rt_remove(Ipaddr *net, Ipaddr *mask) {
    acquire(&rt_list_lock);
    List_entry *le;
    while ((le = list_next(&rt_list)) != &rt_list) {
        Rentry *rentry = le2rentry(le);
        if (ipaddr_is_equal(&rentry->net, net) && ipaddr_is_equal(&rentry->mask, mask)) {
            acquire(&print_struct_lock);
            dbg_info(DBG_IP, "remove a route info:");
            dump_ip(DBG_IP, "net:", net);
            dump_ip(DBG_IP, "mask:", mask);
            cprintf("\n");
            release(&print_struct_lock);
            list_del(le);
            kfree(rentry);
            break;
        }
    }
    release(&rt_list_lock);
    dump_rt_nlist();
}

Rentry *rt_find(Ipaddr *ip) {
    Rentry *find_rentry = nullptr;
    acquire(&rt_list_lock);
    List_entry *le;
    list_for_each(le, &rt_list) {
        Rentry *rentry = le2rentry(le);
        Ipaddr net = ipaddr_get_net(ip, &rentry->mask);
        if (!ipaddr_is_equal(&net, &rentry->net)) { continue; }
        if (!find_rentry || (find_rentry->mask_1_cnt < rentry->mask_1_cnt)) { find_rentry = rentry; }
    }
    release(&rt_list_lock);
    return find_rentry;
}