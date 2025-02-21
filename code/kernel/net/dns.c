#include "dns.h"

#include "net_api.h"
#include "nettool.h"
#include "slab.h"
#include "sock.h"
#include "socket.h"
#include "stdio.h"
#include "udp.h"
#include "proc.h"

static List_entry dns_req_list;
Spinlock dns_req_list_lock;

static List_entry dns_entry_list;
Spinlock dns_entry_list_lock;

static uint8_t working_buf[DNS_WORKING_BUF_SIZE];
static Udp* dns_udp;

static uint16_t id;

static Timer *entry_update_timer = nullptr;

extern int udp_sendto(Socket* socket, void* buf, size_t len, int flags, Sockaddr* dest, socklen_t dest_len,
                      ssize_t* result_len);
extern int udp_init(Socket* socket, int family, int protocol);
extern int udp_recvfrom(Socket* socket, void* buf, size_t len, int flags, Sockaddr* src, socklen_t* addr_len,
                        ssize_t* result_len);
extern Spinlock print_struct_lock;

static void dns_req_fail_nolock(Dns_req* req, int ret);

#if DBG_DISP_ENABLED(DBG_DNS)

static void show_entry_list(void) {
    int idx = 0;
    acquire(&dns_entry_list_lock);
    acquire(&print_struct_lock);
    cprintf("----------------dump dns entry list----------\n");
    List_entry* le;
    list_for_each(le, &dns_entry_list) {
        Dns_entry* entry = le2dns_entry(le);
        if (ipaddr_is_any(&entry->ipaddr)) { continue; }
        cprintf("%d: %s ttl(%d)", idx++, entry->domain_name, entry->ttl);
        dump_ip_buf(" ip: \n", entry->ipaddr.a_addr);
    }
    cprintf("----------------dump dns entry list end----------\n");
    release(&print_struct_lock);
    release(&dns_entry_list_lock);
}

static void show_req_list(void) {
    int idx = 0;
    acquire(&dns_req_list_lock);
    acquire(&print_struct_lock);
    cprintf("----------------dump dns req list----------\n");
    List_entry* le;
    list_for_each(le, &dns_req_list) {
        Dns_req* req = le2dns_req(le);
        cprintf("%d: name(%s) query(%d) retry tmo : %d, retry_cnt : % d ",
                idx++, req->domain_name, req->query_id,
                req->retry_tmo, req->retry_cnt);
        dump_ip_buf(" ip: \n", req->ipaddr.a_addr);
    }
    cprintf("----------------dump dns req list end----------\n");
    release(&print_struct_lock);
    release(&dns_req_list_lock);
}
#else
#define show_entry_list()
#define show_req_list()
#endif

Dns_req* dns_alloc_req(void) {
    Dns_req* req = kmalloc(sizeof(Dns_req));
    assert(req != nullptr);
    list_init(&req->dns_req_link);
    return req;
}

void dns_free_req(Dns_req* req) {
    if (!list_empty(&req->dns_req_link)) {
        acquire(&dns_req_list_lock);
        list_del_init(&req->dns_req_link);
        release(&dns_req_list_lock);
    }
    kfree(req);
}

Dns_entry* dns_entry_find(char* domain_name) {
    List_entry* le;
    acquire(&dns_entry_list_lock);
    list_for_each(le, &dns_entry_list) {
        Dns_entry* entry = le2dns_entry(le);
        assert(!ipaddr_is_any(&entry->ipaddr));
        if ((stricmp(domain_name, entry->domain_name) == 0)) {
            dbg_info(DBG_DNS, "found dns entry: %s %s", entry->domain_name, domain_name);
            release(&dns_entry_list_lock);
            return entry;
        };
    }
    release(&dns_entry_list_lock);
    return nullptr;
}

static void dns_entry_init(Dns_entry* entry, char* domain_name, int ttl, Ipaddr* ipaddr) {
    ipaddr_copy(&entry->ipaddr, ipaddr);
    entry->ttl = ttl;
    strncpy(entry->domain_name, domain_name, DNS_DOMAIN_NAME_MAX - 1);
    entry->domain_name[DNS_DOMAIN_NAME_MAX - 1] = '\0';
}

static void dns_entry_insert(char* domain_name, int ttl, Ipaddr* ipaddr) {
    acquire(&dns_entry_list_lock);
    Dns_entry* new = nullptr;
    Dns_entry* oldest = nullptr;
    if(list_count(&dns_entry_list) >= DNS_ENTRY_SIZE){
        List_entry* le;
        list_for_each(le, &dns_entry_list) {
            Dns_entry* entry = le2dns_entry(le);
            assert(!ipaddr_is_any(&entry->ipaddr));
            if ((oldest == nullptr) || (entry->ttl < oldest->ttl)) { oldest = entry; }
        }
        assert(oldest != nullptr);
        dns_entry_init(oldest, domain_name, ttl, ipaddr);
    }else{
        new = kmalloc(sizeof(Dns_entry));
        assert(new != nullptr);
        dns_entry_init(new, domain_name, ttl, ipaddr);
        list_add(&dns_entry_list, &new->dns_entry_link);
    }
    release(&dns_entry_list_lock);
    show_entry_list();
}

static void dns_entry_free_nolock (Dns_entry * entry) {
    // acquire(&dns_entry_list_lock);
    if(!list_empty(&entry->dns_entry_link)){
        list_del_init(&entry->dns_entry_link);
    }
    // release(&dns_entry_list_lock);
    kfree(entry);
}

static void dns_req_add(Dns_req* req) {
    req->query_id = ++id;
    req->ret = NET_OK;
    req->retry_tmo = DNS_QUERY_RETRY_TMO;
    req->retry_cnt = DNS_QUERY_RETRY_CNT;
    ipaddr_set_any(&req->ipaddr);
    acquire(&dns_req_list_lock);
    list_add(&dns_req_list, &req->dns_req_link);
    release(&dns_req_list_lock);
}

static uint8_t* add_query_field(char* domain_name, char* buf, size_t size) {
    if (size < (sizeof(Dns_qfield) + strlen(domain_name) + 2)) {
        dbg_error(DBG_DNS, "no enough space for query: %s", domain_name);
        return nullptr;
    }
    char* name_buf = buf;
    name_buf[0] = '.';
    strcpy(name_buf + 1, domain_name);
    char* c = name_buf;
    while (*c) {
        if (*c == '.') {
            char* dot = c++;
            while (*c && (*c != '.')) { c++; }
            *dot = (uint8_t)(c - dot - 1);
        } else {
            c++;
        }
    }
    *c++ = '\0';
    Dns_qfield* f = (Dns_qfield*)c;
    f->class = htons(DNS_QUERY_CLASS_INET);
    f->type = htons(DNS_QUERY_TYPE_A);
    return (uint8_t*)f + sizeof(Dns_qfield);
}

static int dns_send_query(Dns_req* req) {
    Dns_hdr* dns_hdr = (Dns_hdr*)working_buf;
    dns_hdr->id = htons(req->query_id);
    dns_hdr->flags.all = 0;
    dns_hdr->flags.rd = 1;
    dns_hdr->flags.all = htons(dns_hdr->flags.all);
    dns_hdr->qdcount = htons(1);
    dns_hdr->ancount = 0;
    dns_hdr->nscount = 0;
    dns_hdr->arcount = 0;
    uint8_t* buf = working_buf + sizeof(Dns_hdr);
    buf = add_query_field(req->domain_name, (char*)buf, sizeof(working_buf) - sizeof(Dns_hdr));
    if (!buf) {
        dbg_error(DBG_DNS, "add query question failed.");
        return -E_NO_MEM;
    }
    Sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(DNS_PORT_DEFAULT);
    dest.sin_addr.s_addr = inet_addr("8.8.8.8");
    // dest.sin_addr.s_addr = inet_addr(BR0_IP);
    return udp_sendto(info2sk(dns_udp, udp), working_buf, buf - working_buf, 0, (Sockaddr*)&dest,
                      sizeof(dest), nullptr);
}

int dns_req_in(Func_msg* msg) {
    Dns_req* dns_req = (Dns_req*)msg->param;
    size_t name_len = strlen(dns_req->domain_name);
    if (name_len >= DNS_DOMAIN_NAME_MAX) {
        dbg_error(DBG_DNS, "domain name too long: %d > %d", name_len, DNS_DOMAIN_NAME_MAX);
        return -E_NET_PARAM;
    }
    if (strcmp(dns_req->domain_name, "localhost") == 0) {
        ipaddr_from_str(&dns_req->ipaddr, "127.0.0.1");
        dns_req->ret = NET_OK;
        return dns_req->ret;
    }
    Ipaddr ipaddr;
    if (ipaddr_from_str(&ipaddr, dns_req->domain_name) == NET_OK) {
        ipaddr_copy(&dns_req->ipaddr, &ipaddr);
        dns_req->ret = NET_OK;
        return dns_req->ret;
    }
    Dns_entry* entry = dns_entry_find(dns_req->domain_name);
    if (entry) {
        ipaddr_copy(&dns_req->ipaddr, &entry->ipaddr);
        dns_req->ret = NET_OK;
        return dns_req->ret;
    }
    sem_init(&dns_req->wait_sem, 0);
    dns_req_add(dns_req);
    int ret = dns_send_query(dns_req);
    if (ret < 0) {
        dbg_error(DBG_DNS, "send dns query failed. ret=%d", ret);
        goto dns_req_end;
    }
    return NET_OK;
dns_req_end:
    if (dns_req->wait_sem.valid) {
        wakeup_queue(&dns_req->wait_sem.wait_queue, WT_INTERAUPTED, 1);
        dns_req->wait_sem.valid = 0;
    }
    return ret;
}

static void dns_update_tmo(Timer* timer) {
    acquire(&dns_entry_list_lock);
    List_entry* le;
    // while ((le = list_next(&dns_entry_list)) != &dns_entry_list){
    list_for_each(le, &dns_entry_list) {
        Dns_entry* entry = le2dns_entry(le);
        if (ipaddr_is_any(&entry->ipaddr)) { continue; }
        if (!entry->ttl || (--entry->ttl == 0)) {
            le = list_next(le);
            dns_entry_free_nolock(entry);
            if(le == &dns_entry_list){
                break;
            }
        }
    }
    release(&dns_entry_list_lock);
    acquire(&dns_req_list_lock);
    // List_entry* le;
    list_for_each(le, &dns_req_list) {
        Dns_req* req = le2dns_req(le);
        if (--req->retry_tmo == 0) {
            if (--req->retry_cnt == 0) {
                le = list_next(le);
                dns_req_fail_nolock(req, -E_NET_TMO);
                if (le == &dns_req_list) { break; }
            } else {
                req->retry_tmo = DNS_QUERY_RETRY_TMO;
                dns_send_query(req);
            }
        }
    }
    release(&dns_req_list_lock);
    show_entry_list();
}

void dnss_init(void) {
    dbg_info(DBG_DNS, "DNS init");
    list_init(&dns_req_list);
    initlock(&dns_req_list_lock, "dns_req_list_lock");
    list_init(&dns_entry_list);
    initlock(&dns_entry_list_lock, "dns_entry_list_lock");
    Socket* dns_socket = new_socket();
    assert(dns_socket != nullptr);
    dns_socket->sk_type = SOCK_UDP;
    udp_init(dns_socket, AF_INET, IPPROTO_UDP);
    dns_udp = skop_info(dns_socket, udp);
    entry_update_timer = timer_func_add(dns_update_tmo, nullptr, DNS_UPDATE_PERIOID, TIMER_RELOAD);
    dbg_info(DBG_DNS, "DNS init");
}

static char* domain_name_cmp(char* domain_name, char* name, size_t size) {
    char* src = domain_name;
    char* dest = name;
    while (*src) {
        int cnt = *dest++;
        for (int i = 0; i < cnt; i++) {
            if (*dest++ != *src++) { return nullptr; }
        }
        if (*src == '\0') {
            break;
        } else if ((*src++ != '.')) {
            return nullptr;
        }
    }
    return (dest >= (name + size)) ? nullptr : dest + 1;
}

static uint8_t* domain_name_skip(uint8_t* name, size_t size) {
    uint8_t* c = name;
    uint8_t* end = name + size;
    while (*c && (c < end)) {
        if ((*c & 0xc0) == 0xc0) {
            c += 2;
            goto skip_end;
        } else {
            c += *c;
        }
    }
    if (*c == '\0') { c++; }
skip_end:
    return c >= end ? nullptr : c;
}

static void dns_req_remove(Dns_req* req, int ret) {
    if (!list_empty(&req->dns_req_link)) {
        acquire(&dns_req_list_lock);
        list_del_init(&req->dns_req_link);
        release(&dns_req_list_lock);
    }
    req->ret = ret;
    if (ret < 0) { ipaddr_set_any(&req->ipaddr); }
    if (req->wait_sem.valid) {
        sem_up(&req->wait_sem, WT_DNS);
        req->wait_sem.valid = 0;
    }
    show_req_list();
}

static void dns_req_remove_nolock(Dns_req* req, int ret) {
    if (!list_empty(&req->dns_req_link)) {
        list_del_init(&req->dns_req_link);
    }
    req->ret = ret;
    if (ret < 0) { ipaddr_set_any(&req->ipaddr); }
    if (req->wait_sem.valid) {
        sem_up(&req->wait_sem, WT_DNS);
        req->wait_sem.valid = 0;
    }
    show_req_list();
}

static void dns_req_fail(Dns_req* req, int ret) { dns_req_remove(req, ret); }

static void dns_req_fail_nolock(Dns_req* req, int ret) { dns_req_remove_nolock(req, ret); }

void dns_in(void) {
    ssize_t rcv_len;
    Sockaddr_in src;
    socklen_t addr_len;
    int ret = udp_recvfrom(info2sk(dns_udp, udp), working_buf, sizeof(working_buf), 0, (Sockaddr*)&src,
                           &addr_len, &rcv_len);
    if (ret < 0) {
        dbg_error(DBG_DNS, "rcv udp error: %d", ret);
        return;
    }
    uint8_t* rcv_start = working_buf;
    uint8_t* rcv_end = working_buf + rcv_len;
    Dns_hdr* dns_hdr = (Dns_hdr*)rcv_start;
    dns_hdr->id = ntohs(dns_hdr->id);
    dns_hdr->flags.all = ntohs(dns_hdr->flags.all);
    dns_hdr->qdcount = ntohs(dns_hdr->qdcount);
    dns_hdr->ancount = ntohs(dns_hdr->ancount);
    dns_hdr->nscount = ntohs(dns_hdr->nscount);
    dns_hdr->arcount = ntohs(dns_hdr->arcount);
    ret = NET_OK;
    List_entry* le;
    list_for_each(le, &dns_req_list) {
        Dns_req* req = le2dns_req(le);
        if (req->query_id != dns_hdr->id) { continue; }
        if (dns_hdr->flags.qr == 0) {
            dbg_warning(DBG_DNS, "not a responsed");
            goto req_failure;
        }
        if (dns_hdr->flags.tc == 1) {
            dbg_warning(DBG_DNS, "response truncated");
            goto req_failure;
        }
        if (dns_hdr->flags.ra == 0) {
            dbg_warning(DBG_DNS, "recursion not supported");
            goto req_failure;
        }
        switch (dns_hdr->flags.rcode) {
            case DNS_ERR_NONE: break;
            case DNS_ERR_NOTIMP: dbg_warning(DBG_DNS, "server reply: not support"); goto req_failure;
            case DNS_ERR_REFUSED: dbg_warning(DBG_DNS, "server reply: refused"); goto req_failure;
            case DNS_ERR_SERV_FAIL: dbg_warning(DBG_DNS, "server reply: server failure"); goto req_failure;
            case DNS_ERR_NXMOMAIN: dbg_warning(DBG_DNS, "server reply: domain not exist"); goto req_failure;
            case DNS_ERR_FORMAT: dbg_warning(DBG_DNS, "server reply: format error"); goto req_failure;
            default: dbg_warning(DBG_DNS, "unknow error"); goto req_failure;
        }
        if (dns_hdr->qdcount == 1) {
            rcv_start += sizeof(Dns_hdr);
            rcv_start = (uint8_t*)domain_name_cmp(req->domain_name, (char*)rcv_start, rcv_end - rcv_start);
            if (rcv_start == nullptr) {
                dbg_warning(DBG_DNS, "domain name not match");
                goto req_failure;
            }
            if (rcv_start + sizeof(Dns_qfield) > rcv_end) {
                dbg_warning(DBG_DNS, "size error");
                goto req_failure;
            }
            Dns_qfield* qf = (Dns_qfield*)rcv_start;
            if (qf->class != htons(DNS_QUERY_CLASS_INET)) {
                dbg_warning(DBG_DNS, "query class not inet");
                goto req_failure;
            }

            if (qf->type != htons(DNS_QUERY_TYPE_A)) {
                dbg_warning(DBG_DNS, "query class not inet");
                goto req_failure;
            }
            rcv_start += sizeof(Dns_qfield);
        }
        if (dns_hdr->ancount < 1) {
            dbg_warning(DBG_DNS, "no answer");
            goto req_failure;
        }
        for (int i = 0; (i < dns_hdr->ancount) && (rcv_start < rcv_end); i++) {
            rcv_start = domain_name_skip(rcv_start, rcv_end - rcv_start);
            if (rcv_start == nullptr) {
                dbg_warning(DBG_DNS, "size error");
                goto req_failure;
            }
            if (rcv_start + sizeof(Dns_afield) > rcv_end) {
                dbg_warning(DBG_DNS, "size error");
                goto req_failure;
            }
            Dns_afield* af = (Dns_afield*)rcv_start;
            if ((af->class == htons(DNS_QUERY_CLASS_INET)) && (af->type == htons(DNS_QUERY_TYPE_A)) &&
                (af->rd_len == htons(IPV4_ADDR_SIZE))) {
                ipaddr_from_buf(&req->ipaddr, (uint8_t*)af->rdata);
                dns_entry_insert(req->domain_name, ntohl(af->ttl), &req->ipaddr);
                dbg_info(DBG_DNS, "recv dns A type: %s", req->domain_name);
                dump_ip(DBG_DNS, "ipaddr:", &req->ipaddr);
                dns_req_remove(req, NET_OK);
                return;
            }
            rcv_start += sizeof(Dns_afield) + htons(af->rd_len) - 2;
        }
    req_failure:
        ret = -E_NET;
        dns_req_fail(req, ret);
        return;
    }
}

int dns_is_arrive(Udp* udp) { return udp == dns_udp; }