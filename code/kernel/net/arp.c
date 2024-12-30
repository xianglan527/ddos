#include "arp.h"
#include "nettool.h"
#include "stdio.h"
#include "assert.h"
#include "slab.h"
#include "proc.h"

extern Spinlock print_struct_lock;
Arp_entry_head arp_entry_head;

static uint8_t empty_hwaddr[] = {0, 0, 0, 0, 0, 0};
Timer* arp_cache_timer;

#define to_scan_cnt(tmo) (tmo / ARP_TIMER_TMO)

#if DBG_DISP_ENABLED(DBG_ARP)

static void cache_free_nolock(Arp_entry* entry);

static void __dump_arp_entry(int index, Arp_entry* entry) {
    cprintf("%d: ", index); 
    dump_ip_buf(" ip:", entry->paddr);
    dump_mac(" mac:", entry->haddr);
    cprintf(" tmo: %lu, retry: %lu, %s, buf: %lu\n", entry->tmo, entry->retry,
                entry->state == NET_ARP_RESOLVED ? "stable" : "pending", list_count(&entry->buf_list));
}

static void dump_arp_tbl(void) {
    acquire(&arp_entry_head.arp_entry_list_lock);
    acquire(&print_struct_lock);
    cprintf("\n------------- ARP table start ---------- \n");
    List_entry* le;
    int index = 0;
    list_for_each(le, &arp_entry_head.arp_entry_list) {
        Arp_entry* entry = le2arp_entry(le);
        if (entry->state == NET_ARP_FREE) { continue; }
        __dump_arp_entry(index++, entry);
    }
    cprintf("------------- ARP table end ---------- \n");
    release(&print_struct_lock);
    release(&arp_entry_head.arp_entry_list_lock);
}

static void dump_arp_pkt(Arp_pkt* packet) {
    uint16_t opcode = x_ntohs(packet->opcode);
    acquire(&print_struct_lock);
    cprintf("--------------- arp_pkt dump start ------------------\n");
    cprintf("htype:%x\n", x_ntohs(packet->htype));
    cprintf("ptype:%x\n", x_ntohs(packet->ptype));
    cprintf("hlen: %x\n", packet->hlen);
    cprintf("plen:%x\n", packet->plen);
    cprintf("type:%04x  ", opcode);
    switch (opcode) {
        case ARP_REQUEST:
            cprintf("request\n");
            break;
        case ARP_REPLY: cprintf("reply\n"); break;
        default: cprintf("unknown\n"); break;
    }
    dump_ip_buf("sender:", packet->send_paddr);
    dump_mac("mac:", packet->send_haddr);
    cprintf("\n");
    dump_ip_buf("target:", packet->target_paddr);
    dump_mac("mac:", packet->target_haddr);
    cprintf("\n");
    cprintf("--------------- arp_pkt dump end ------------------ \n");
    release(&print_struct_lock);
}

#else
#define __dump_arp_entry(entry)
#define dump_arp_tbl()
#define dump_arp_pkt(packet)
#endif

static void arp_cache_tmo(Timer* timer) {
    int changed_cnt = 0;
    List_entry* le;
    acquire(&arp_entry_head.arp_entry_list_lock);
    if(list_empty(&arp_entry_head.arp_entry_list)){
        release(&arp_entry_head.arp_entry_list_lock);
        return;
    }
    le = list_next(&arp_entry_head.arp_entry_list);
    while (le != &arp_entry_head.arp_entry_list) {
        Arp_entry *entry = le2arp_entry(le);
        le = list_next(le);
        if(--entry->tmo > 0){
            continue;
        }
        changed_cnt++;
        switch(entry->state){
            case NET_ARP_RESOLVED:{
                dbg_info(DBG_ARP, "stable to pending:");
                // __dump_arp_entry(0, entry);
                Ipaddr ipaddr;
                ipaddr_from_buf(&ipaddr, entry->paddr);
                memset(entry->haddr, 0, sizeof(entry->haddr));
                entry->state = NET_ARP_WAITING;
                entry->tmo = to_scan_cnt(ARP_ENTRY_PENDING_TMO);
                entry->retry = to_scan_cnt(ARP_ENTRY_RETRY_CNT);
                release(&arp_entry_head.arp_entry_list_lock);
                arp_make_request(entry->netif, &ipaddr);
                acquire(&arp_entry_head.arp_entry_list_lock);
                break;
            }
            case NET_ARP_WAITING: {
                if (--entry->retry == 0) {
                    dbg_info(DBG_ARP, "pending tmo, free it.");
                    // __dump_arp_entry(0, entry);
                    cache_free_nolock(entry);
                } else {
                    dbg_info(DBG_ARP, "pending tmo, send request.");
                    // __dump_arp_entry(0, entry);
                    Ipaddr ipaddr;
                    ipaddr_from_buf(&ipaddr, entry->paddr);
                    entry->tmo = to_scan_cnt(ARP_ENTRY_PENDING_TMO);
                    release(&arp_entry_head.arp_entry_list_lock);
                    arp_make_request(entry->netif, &ipaddr);
                    acquire(&arp_entry_head.arp_entry_list_lock);
                }
                break;
            }
            default: {
                dbg_error(DBG_ARP, "unknown arp state.");
                // __dump_arp_entry(0, entry);
                break;
            }
        }
    }
    release(&arp_entry_head.arp_entry_list_lock);
    if (changed_cnt) {
        dbg_info(DBG_ARP, "%d arp entry changed.", changed_cnt);
        dump_arp_tbl();
    }
}

int arp_init(void){
    list_init(&arp_entry_head.arp_entry_list);
    initlock(&arp_entry_head.arp_entry_list_lock, "arp_entry_list_lock");
    arp_entry_head.count = 0;
    arp_cache_timer = timer_func_add(arp_cache_tmo, "arp_cache_timer", ARP_TIMER_TMO * 1000, TIMER_RELOAD);
    assert(arp_cache_timer != nullptr);
    return NET_OK;
}

static void cache_clear_all(Arp_entry* entry) {
    acquire(&print_struct_lock);
    dbg_info(DBG_ARP, "clear %d packet:", list_count(&entry->buf_list));
#if DBG_DISP_ENABLED(DBG_ARP)
    dump_ip_buf("ip:", entry->paddr);
    dump_mac("mac:", entry->haddr);
#endif
    release(&print_struct_lock);
    List_entry* le;
    acquire(&entry->buf_list_lock);
    while ((le = list_next(&entry->buf_list)) != &entry->buf_list) {
        list_del(le);
        Pktbuf* buf = le2pktbuf_wait(le);
        pktbuf_free(buf);
    }
    release(&entry->buf_list_lock);
}

static void cache_free_nolock(Arp_entry* entry) {
    cache_clear_all(entry);
    // acquire(&arp_entry_head.arp_entry_list_lock);
    list_del(&entry->arp_entry_link);
    arp_entry_head.count--;
    // release(&arp_entry_head.arp_entry_list_lock);
    kfree(entry);
}

static Arp_entry* cache_alloc_nolock(bool force) {
    List_entry* le;
    // acquire(&arp_entry_head.arp_entry_list_lock);   
    if(arp_entry_head.count == ARP_CACHE_SIZE){
        if(force == false){
            // release(&arp_entry_head.arp_entry_list_lock);
            return nullptr;
        }
        assert(!list_empty(&arp_entry_head.arp_entry_list));
        le = list_prev(&arp_entry_head.arp_entry_list);
        Arp_entry* entry = le2arp_entry(le);
        cache_free_nolock(entry);
    }
    Arp_entry *entry = kmalloc(sizeof(Arp_entry));
    assert(entry != nullptr);
    entry->state = NET_ARP_FREE;
    list_init(&entry->buf_list);
    initlock(&entry->buf_list_lock, "buf_list_lock");
    list_add(&arp_entry_head.arp_entry_list, &entry->arp_entry_link);
    arp_entry_head.count++;
    // release(&arp_entry_head.arp_entry_list_lock);
    return entry;
}

static int cache_send_all(Arp_entry* entry) {
#if DBG_DISP_ENABLED(DBG_ARP)
    acquire(&print_struct_lock);
    dbg_info(DBG_ARP, "send %lu packet:", list_count(&entry->buf_list));
    dump_ip_buf("ip:", entry->paddr);
    dump_mac("mac:", entry->haddr);
    release(&print_struct_lock);
#endif
    List_entry* le;
    acquire(&entry->buf_list_lock);
    while ((le = list_next(&entry->buf_list)) != &entry->buf_list) {
        Pktbuf* buf = le2pktbuf_wait(le);
        list_del(le);
        int ret = ether_raw_out(entry->netif, NET_PROTOCOL_IPv4, entry->haddr, buf);
        if (ret < 0) {
            pktbuf_free(buf);
        }
    }
    release(&entry->buf_list_lock);
    return NET_OK;
}

static Arp_entry* cache_find_nolock(uint8_t* ip) {
    List_entry *le;
    list_for_each(le, &arp_entry_head.arp_entry_list) {
         Arp_entry* entry = le2arp_entry(le);
         if (memcmp(ip, entry->paddr, IPV4_ADDR_SIZE) == 0) {
             if (list_next(&arp_entry_head.arp_entry_list) != &entry->arp_entry_link) {
                 list_del(&entry->arp_entry_link);
                 list_add_after(&arp_entry_head.arp_entry_list, &entry->arp_entry_link);
             }
             return entry;
         }
    }
    return nullptr;
}

static void cache_entry_set(Arp_entry* entry, uint8_t* hwaddr, uint8_t* proaddr, Netif* netif,
                            int state) {
    memcpy(entry->haddr, hwaddr, ETH_HWA_SIZE);
    memcpy(entry->paddr, proaddr, IPV4_ADDR_SIZE);
    entry->state = state;
    entry->netif = netif;
    if (state == NET_ARP_RESOLVED) {
        entry->tmo = to_scan_cnt(ARP_ENTRY_STABLE_TMO);  
    } else {
        entry->tmo = to_scan_cnt(ARP_ENTRY_PENDING_TMO); 
    }
    entry->retry = ARP_ENTRY_RETRY_CNT;
}

static int cache_insert(Netif* netif, uint8_t* pro_addr, uint8_t* hw_addr, bool force){
    acquire(&arp_entry_head.arp_entry_list_lock);
    Arp_entry* entry = cache_find_nolock(pro_addr);
    if(!entry){
        entry = cache_alloc_nolock(force);
        if (!entry) {
#if DBG_DISP_ENABLED(DBG_ARP)
            dump_ip_buf("alloc failed! sender ip:", pro_addr);
#endif
            return -E_NET_NONE;
        }
        cache_entry_set(entry, hw_addr, pro_addr, netif, NET_ARP_RESOLVED);
        list_del(&entry->arp_entry_link);
        list_add_after(&arp_entry_head.arp_entry_list, &entry->arp_entry_link);
#if DBG_DISP_ENABLED(DBG_ARP)
        dump_ip_buf("insert an entry, sender ip:", pro_addr);
#endif
    }else{
#if DBG_DISP_ENABLED(DBG_ARP)
        acquire(&print_struct_lock);
        dump_ip_buf("update an entry, sender ip:", pro_addr);
        dump_mac("sender mac:", hw_addr);
        release(&print_struct_lock);
#endif
        cache_entry_set(entry, hw_addr, pro_addr, netif, NET_ARP_RESOLVED);
        if (list_next(&arp_entry_head.arp_entry_list) != &entry->arp_entry_link){
            list_del(&entry->arp_entry_link);
            list_add_after(&arp_entry_head.arp_entry_list, &entry->arp_entry_link);
        }
        int ret = cache_send_all(entry);
        if (ret < 0) {
            dbg_error(DBG_ARP, "send packet in entry failed. err = %d", ret);
            return ret;
        }
    }
    release(&arp_entry_head.arp_entry_list_lock);
    dump_arp_tbl();
    return NET_OK;
}

int arp_make_request(Netif* netif, Ipaddr* pro_addr){
    Pktbuf* buf = pktbuf_alloc(sizeof(Arp_pkt));
    if (buf == NULL) {
        dump_ip(DBG_ARP, "allocate arp packet failed. ip:", pro_addr);
        return -E_NET_NONE;
    }
    pktbuf_set_cont(buf, sizeof(Arp_pkt));
    Arp_pkt* arp_packet = (Arp_pkt *)pktbuf_data(buf);
    arp_packet->htype = x_htons(ARP_HW_ETHER);
    arp_packet->ptype = x_htons(NET_PROTOCOL_IPv4);
    arp_packet->hlen = ETH_HWA_SIZE;
    arp_packet->plen = IPV4_ADDR_SIZE;
    arp_packet->opcode = x_htons(ARP_REQUEST);
    memcpy(arp_packet->send_haddr, netif->hwaddr.addr, ETH_HWA_SIZE);
    ipaddr_to_buf(&netif->ipaddr, arp_packet->send_paddr);
    memset(arp_packet->target_haddr, 0, ETH_HWA_SIZE);
    ipaddr_to_buf(pro_addr, arp_packet->target_paddr);
    dump_arp_pkt(arp_packet);
    int ret = ether_raw_out(netif, NET_PROTOCOL_ARP, ether_broadcast_addr(), buf);
    if (ret < 0) { pktbuf_free(buf); }
    return ret;
}

int arp_make_gratuitous(Netif* netif) { 
    return arp_make_request(netif, &netif->ipaddr);
}

static int is_pkt_ok(Arp_pkt* arp_packet, uint16_t size, Netif* netif) {
    if (size < sizeof(Arp_pkt)) {
        dbg_warning(DBG_ARP, "packet size error: %d < %d", size, (int)sizeof(Arp_pkt));
        return -E_NET_SIZE;
    }
    if ((x_ntohs(arp_packet->htype) != ARP_HW_ETHER) || (arp_packet->hlen != ETH_HWA_SIZE) ||
        (x_ntohs(arp_packet->ptype) != NET_PROTOCOL_IPv4) || (arp_packet->plen != IPV4_ADDR_SIZE)) {
        dbg_warning(DBG_ARP, "packet incorrect");
        return -E_NET_NOT_SUPPORT;
    }
    uint16_t opcode = x_ntohs(arp_packet->opcode);
    if ((opcode != ARP_REQUEST) && (opcode != ARP_REPLY)) {
        dbg_warning(DBG_ARP, "unknown opcode=%d", arp_packet->opcode);
        return -E_NET_NOT_SUPPORT;
    }
    return NET_OK;
}

int arp_in(Netif* netif, Pktbuf* buf){
    dbg_info(DBG_ARP, "arp in");
    int ret = pktbuf_set_cont(buf, sizeof(Arp_pkt));
    if (ret < 0) { return ret; }
    Arp_pkt* arp_packet = (Arp_pkt*)pktbuf_data(buf);
    dump_arp_pkt(arp_packet);
    if (is_pkt_ok(arp_packet, buf->total_size, netif) != NET_OK) { return ret; }
    Ipaddr target_ip;
    ipaddr_from_buf(&target_ip, arp_packet->target_paddr);
    if (ipaddr_is_equal(&target_ip, &netif->ipaddr)){
        dbg_info(DBG_ARP, "received an arp for me, force update.");
        cache_insert(netif, arp_packet->send_paddr, arp_packet->send_haddr, 1);
        if (x_ntohs(arp_packet->opcode) == ARP_REQUEST) {
            dbg_info(DBG_ARP, "arp is request. try to send reply");
            return arp_make_reply(netif, buf);
        }
    }else{
        dbg_info(DBG_ARP, "received an arp not for me, try to update.");
        cache_insert(netif, arp_packet->send_paddr, arp_packet->send_haddr, 0);
    }
    dump_arp_tbl();
    pktbuf_free(buf);
    return NET_OK;
}

int arp_make_reply(Netif* netif, Pktbuf* buf){
    Arp_pkt *arp_packet = (Arp_pkt *)pktbuf_data(buf);
    arp_packet->opcode = x_htons(ARP_REPLY);
    memcpy(arp_packet->target_haddr, arp_packet->send_haddr, ETH_HWA_SIZE);
    memcpy(arp_packet->target_paddr, arp_packet->send_paddr, IPV4_ADDR_SIZE);
    memcpy(arp_packet->send_haddr, netif->hwaddr.addr, ETH_HWA_SIZE);
    ipaddr_to_buf(&netif->ipaddr, arp_packet->send_paddr);
    // dump_arp_pkt(arp_packet);
    return ether_raw_out(netif, NET_PROTOCOL_ARP, arp_packet->target_haddr, buf);
}

int arp_resolve(Netif* netif, Ipaddr* ipaddr, Pktbuf* buf){
    uint8_t pro_addr[IPV4_ADDR_SIZE];
    ipaddr_to_buf(ipaddr, pro_addr);
    acquire(&arp_entry_head.arp_entry_list_lock);
    Arp_entry* entry = cache_find_nolock(pro_addr);
    if(entry){
         dbg_info(DBG_ARP, "found an arp entry.");
         if (entry->state == NET_ARP_RESOLVED) {
             int ret = ether_raw_out(entry->netif, NET_PROTOCOL_IPv4, entry->haddr, buf);
             return ret;
         }
         assert(entry->state == NET_ARP_WAITING);
         acquire(&entry->buf_list_lock);
         if(list_count(&entry->buf_list) <= ARP_MAX_PKT_WAIT){
             dbg_info(DBG_ARP, "insert packet to arp entry");
             list_add_before(&entry->buf_list, &buf->pktbuf_wait_link);
         } else {
             dbg_warning(DBG_ARP, "too many waiting. ignore it");
             return -E_NET_FULL;
         }
         release(&entry->buf_list_lock);
         release(&arp_entry_head.arp_entry_list_lock);
         dump_arp_tbl();
         return NET_OK;
    }else{
        dump_ip(DBG_ARP, "make arp request, ip:", ipaddr);
        entry = cache_alloc_nolock(1);
        assert(entry != nullptr);
        cache_entry_set(entry, empty_hwaddr, pro_addr, netif, NET_ARP_WAITING);
        list_del(&entry->arp_entry_link);
        list_add_after(&arp_entry_head.arp_entry_list, &entry->arp_entry_link);
        dbg_info(DBG_ARP, "insert packet to arp");
        acquire(&entry->buf_list_lock);
        list_add_before(&entry->buf_list, &buf->pktbuf_wait_link);
        release(&entry->buf_list_lock);
        release(&arp_entry_head.arp_entry_list_lock);
        dump_arp_tbl();
        return arp_make_request(netif, ipaddr);
    }
}

void arp_clear(Netif* netif){
    List_entry* le;
    acquire(&arp_entry_head.arp_entry_list_lock);
    le = list_next(&arp_entry_head.arp_entry_list);
    while (le != &arp_entry_head.arp_entry_list) {
        Arp_entry *entry = le2arp_entry(le);
        le = list_next(le);
        if(entry->netif == netif){
            cache_free_nolock(entry);
        }
    }
    release(&arp_entry_head.arp_entry_list_lock);
}

uint8_t* arp_find(Netif* netif, Ipaddr* ip){
    if (ipaddr_is_local_broadcast(ip) || ipaddr_is_direct_broadcast(ip, &netif->netmask)) {
        return ether_broadcast_addr();
    }
    acquire(&arp_entry_head.arp_entry_list_lock);
    Arp_entry *entry = cache_find_nolock(ip->a_addr);
    release(&arp_entry_head.arp_entry_list_lock);
    if (entry && (entry->state == NET_ARP_RESOLVED)) { return entry->haddr; }
    return nullptr;
}