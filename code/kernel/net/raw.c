#include "raw.h"

#include "ipv4.h"
#include "slab.h"
#include "sock.h"
#include "socket.h"
#include "stdio.h"

static List_entry raw_list;
Spinlock raw_list_lock;
extern Spinlock print_struct_lock;

static int raw_sendto(Socket* socket, void* buf, size_t len, int flags, Sockaddr* dest, socklen_t dest_len,
                      ssize_t* result_len);
static int raw_recvfrom(Socket* socket, void* buf, size_t len, int flags, Sockaddr* src, socklen_t* addr_len,
                        ssize_t* result_len);
static int raw_close(Socket* socket);
static int raw_connect(Socket* socket, Sockaddr* addr, socklen_t len);
static int raw_bind(Socket* socket, Sockaddr* addr, socklen_t len);

#if DBG_DISP_ENABLED(DBG_RAW)
    static void display_raw_list(void) {
    int idx = 0;
    List_entry* le;
    acquire(&print_struct_lock);
    cprintf("\n-------------dump raw list ---------- \n");
    list_for_each(le, &raw_list) {
        Socket *socket = le2socket(le);
        Raw *raw = skop_info(socket, raw);
        cprintf("[%d]\n", idx++);
        cprintf("\traw address is %p\n", raw);
        dump_ip_buf("\tlocal:", (uint8_t*)&socket->local_ip.a_addr);
        dump_ip_buf("\tremote:", (uint8_t*)&socket->remote_ip.a_addr);
        cprintf("\n");
    }
    cprintf("\n-------------dump raw list end ---------- \n");
    release(&print_struct_lock);
}
#else
#define display_raw_list()
#endif

int raw_init(Socket* socket, int family, int protocol) {
    static Sock_ops raw_ops = {
        .sendto = raw_sendto,
        .recvfrom = raw_recvfrom,
        .setopt = sock_setopt,
        .close = raw_close,
        .connect = raw_connect,
        .bind = raw_bind,
        .send = sock_send,  
        .recv = sock_recv,
    };
    // socket->sk_type = SOCK_RAW;
    Raw* raw = skop_info(socket, raw);
    int ret = socket_init(socket, family, protocol, &raw_ops);
    if (ret < 0) {
        dbg_error(DBG_RAW, "init raw failed.");
        return ret;
    }
    list_init(&raw->recv_pkt_list);
    assert(list_empty(&socket->socket_link));
    acquire(&raw_list_lock);
    list_add(&raw_list, &socket->socket_link);
    release(&raw_list_lock);
    display_raw_list();
    return NET_OK;
}

int raws_init(void) {
    dbg_info(DBG_RAW, "raw init.");
    list_init(&raw_list);
    initlock(&raw_list_lock, "raw_list_lock");
    dbg_info(DBG_RAW, "init done.");
    return NET_OK;
}

static int raw_sendto(Socket* socket, void* buf, size_t len, int flags, Sockaddr* dest, socklen_t dest_len,
                      ssize_t* result_len) {
    Ipaddr dest_ip;
    Sockaddr_in* addr = (Sockaddr_in*)dest;
    ipaddr_from_buf(&dest_ip, addr->sin_addr.addr_array);
    if (!ipaddr_is_any(&socket->remote_ip) && !ipaddr_is_equal(&dest_ip, &socket->remote_ip)) {
        dbg_error(DBG_RAW, "dest is incorrect");
        return -E_NET_CONNECTED;
    }
    Pktbuf* pktbuf = pktbuf_alloc(len);
    if (!pktbuf) {
        dbg_error(DBG_RAW, "no buffer");
        return -E_NO_MEM;
    }
    int ret = pktbuf_write(pktbuf, (uint8_t*)buf, len);
    if (ret < 0) {
        dbg_error(DBG_RAW, "copy data error");
        goto end_sendto;
    }
    ret = ipv4_out(socket->protocol, &dest_ip, &socket->local_ip, pktbuf);
    if (ret < 0) {
        dbg_error(DBG_RAW, "send error");
        goto end_sendto;
    }
    *result_len = (ssize_t)len;
    return NET_OK;
end_sendto:
    pktbuf_free(pktbuf);
    return ret;
}

static int raw_recvfrom(Socket* socket, void* buf, size_t len, int flags, Sockaddr* src, socklen_t* addr_len,
                        ssize_t* result_len) {
    Raw* raw = skop_info(socket, raw);
    if (list_empty(&raw->recv_pkt_list)) {
        *result_len = 0;
        return NET_NEED_WAIT;
    }
    List_entry* le = list_next(&raw->recv_pkt_list);
    list_del(le);
    Pktbuf* pktbuf = le2sock_recv(le);
    Ipv4_hdr* iphdr = (Ipv4_hdr*)pktbuf_data(pktbuf);
    Sockaddr_in* addr = (Sockaddr_in*)src;
    memset(addr, 0, sizeof(Sockaddr));
    addr->sin_family = AF_INET;
    addr->sin_port = 0;
    memcpy(&addr->sin_addr, iphdr->src_ip, IPV4_ADDR_SIZE);
    size_t size = (pktbuf->total_size > len) ? len : pktbuf->total_size;
    pktbuf_reset_acc(pktbuf);
    int ret = pktbuf_read(pktbuf, buf, size);
    if (ret < 0) {
        pktbuf_free(pktbuf);
        dbg_error(DBG_RAW, "pktbuf read error");
        return ret;
    }
    pktbuf_free(pktbuf);
    *result_len = size;
    return NET_OK;
}

static Raw* raw_find(Ipaddr* src, Ipaddr* dest, int protocol) {
    List_entry* le;
    Raw* raw = nullptr;
    acquire(&raw_list_lock);
    list_for_each(le, &raw_list) {
        Socket* socket = le2socket(le);
        if (socket->protocol && (socket->protocol != protocol)) { continue; }
        if (!ipaddr_is_any(&socket->local_ip) && !ipaddr_is_equal(&socket->local_ip, dest)) { continue; }
        if (!ipaddr_is_any(&socket->remote_ip) && !ipaddr_is_equal(&socket->remote_ip, src)) { continue; }
        raw = skop_info(socket, raw);
        break;
    }
    release(&raw_list_lock);
    return raw;
}

int raw_in(Pktbuf* pktbuf) {
    Ipv4_hdr* iphdr = (Ipv4_hdr*)pktbuf_data(pktbuf);
    Ipaddr src, dest;
    ipaddr_from_buf(&dest, iphdr->dest_ip);
    ipaddr_from_buf(&src, iphdr->src_ip);
    Raw* raw = raw_find(&src, &dest, iphdr->protocol);
    if (raw == nullptr) {
        dbg_warning(DBG_RAW, "no raw for this packet");
        return -E_NET_MATCH;
    }
    if (list_count(&raw->recv_pkt_list) < RAW_MAX_RECV) {
        list_add(&raw->recv_pkt_list, &pktbuf->sock_recv_link);
        Socket* socket = info2sk(raw, raw);
        sock_wakeup(socket, WT_SOCK_READ, NET_OK);
    } else {
        pktbuf_free(pktbuf);
    }
    return NET_OK;
}

static int raw_close(Socket* socket) { 
    Raw *raw = skop_info(socket, raw);
    List_entry* le;
    while ((le = list_next(&raw->recv_pkt_list)) != &raw->recv_pkt_list) {
        list_del(le);
        Pktbuf* buf = le2sock_recv(le);
        pktbuf_free(buf);
    }
    acquire(&raw_list_lock);
    list_del_init(&socket->socket_link);
    release(&raw_list_lock);
    display_raw_list();
    return NET_OK;
}

static int raw_connect(Socket* socket, Sockaddr* addr, socklen_t len) {
    sock_connect(socket, addr, len);
    display_raw_list();
    return NET_OK;
}

static int raw_bind(Socket* socket, Sockaddr* addr, socklen_t len) {
    int ret = sock_bind(socket, addr, len);
    display_raw_list();
    return ret;
}
