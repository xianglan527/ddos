#include "udp.h"

#include "dns.h"
#include "ipv4.h"
#include "nettool.h"
#include "protocol.h"
#include "sock.h"
#include "socket.h"
#include "stdio.h"

static List_entry udp_list;
Spinlock udp_list_lock;
extern Spinlock print_struct_lock;

int udp_sendto(Socket* socket, void* buf, size_t len, int flags, Sockaddr* dest, socklen_t dest_len,
                      ssize_t* result_len);
int udp_recvfrom(Socket* socket, void* buf, size_t len, int flags, Sockaddr* src, socklen_t* addr_len,
                        ssize_t* result_len);
static int udp_close(Socket* socket);
static int udp_connect(Socket* socket, Sockaddr* addr, socklen_t len);
int udp_bind(Socket* socket, Sockaddr* addr, socklen_t len);

#if DBG_DISP_ENABLED(DBG_UDP)
    static void display_udp_packet(Udp_pkt* pkt) {
    acquire(&print_struct_lock);
    cprintf("UDP packet:\n");
    cprintf("source Port:%d\n", pkt->hdr.src_port);
    cprintf("dest Port: %d\n", pkt->hdr.dest_port);
    cprintf("length: %d bytes\n", pkt->hdr.total_len);
    cprintf("checksum:  %04x\n", pkt->hdr.checksum);
    release(&print_struct_lock);
}

static void display_udp_list(void) {
    int idx = 0;
    List_entry* le;
    acquire(&print_struct_lock);
    acquire(&udp_list_lock);
    cprintf("\n-------------dump udp list ---------- \n");
    list_for_each(le, &udp_list) {
        Socket* socket = le2socket(le);
        Udp* udp = skop_info(socket, udp);
        cprintf("[%d]\n", idx++);
        cprintf("\tudp address is %p\n", udp);
        dump_ip_buf("\tlocal:", (uint8_t*)&socket->local_ip.a_addr);
        cprintf("\tlocal port: %d\n", socket->local_port);
        dump_ip_buf("\tremote:", (uint8_t*)&socket->remote_ip.a_addr);
        cprintf("\tremote port: %d\n", socket->remote_port);
        cprintf("\n");
    }
    cprintf("\n-------------dump udp list end ---------- \n");
    release(&udp_list_lock);
    release(&print_struct_lock);
}
#else
#define display_udp_packet(packet)
#define display_udp_list()
#endif

int udps_init(void) {
    dbg_info(DBG_UDP, "udp init.");
    list_init(&udp_list);
    initlock(&udp_list_lock, "udp_list_lock");
    dbg_info(DBG_UDP, "init done.");
    return NET_OK;
}

static bool is_port_used(int port) {
    List_entry* le;
    list_for_each(le, &udp_list) {
        Socket* socket = le2socket(le);
        if (socket->local_port == port) { return true; }
    }
    return false;
}

static int alloc_port(Socket* socket) {
    acquire(&udp_list_lock);
    static int search_index = NET_PORT_DYN_START;
    if (search_index > NET_PORT_DYN_END) { search_index = NET_PORT_DYN_END; }
    for (int i = NET_PORT_DYN_START; i < NET_PORT_DYN_END; i++) {
        int port = search_index++;
        if (!is_port_used(port)) {
            socket->local_port = port;
            release(&udp_list_lock);
            return NET_OK;
        }
    }
    return -E_NET_NONE;
    release(&udp_list_lock);
}

int udp_init(Socket* socket, int family, int protocol) {
    static Sock_ops udp_ops = {
        .setopt = sock_setopt,
        .sendto = udp_sendto,
        .recvfrom = udp_recvfrom,
        .connect = udp_connect,
        .close = udp_close,
        .send = sock_send,
        .recv = sock_recv,
        .bind = udp_bind,
    };
    Udp* udp = skop_info(socket, udp);
    int ret = socket_init(socket, family, protocol, &udp_ops);
    if (ret < 0) {
        dbg_error(DBG_UDP, "init udp failed.");
        return ret;
    }
    list_init(&udp->recv_pkt_list);
    assert(list_empty(&socket->socket_link));
    acquire(&udp_list_lock);
    list_add(&udp_list, &socket->socket_link);
    release(&udp_list_lock);
    display_udp_list();
    return NET_OK;
}

int udp_sendto(Socket* socket, void* buf, size_t len, int flags, Sockaddr* dest, socklen_t dest_len,
                      ssize_t* result_len) {
    Ipaddr dest_ip;
    Sockaddr_in* addr = (Sockaddr_in*)dest;
    ipaddr_from_buf(&dest_ip, addr->sin_addr.addr_array);
    if (!ipaddr_is_any(&socket->remote_ip) && !ipaddr_is_equal(&dest_ip, &socket->remote_ip)) {
        dbg_error(DBG_UDP, "dest is incorrect");
        return -E_NET_CONNECTED;
    }
    uint16_t dport = x_ntohs(addr->sin_port);
    if (socket->remote_port && socket->remote_port != dport) {
        dbg_error(DBG_UDP, "remote port error");
        return -E_NET_MATCH;
    }
    if (!socket->local_port && ((socket->ret = alloc_port(socket)) < 0)) {
        dbg_error(DBG_UDP, "no port avaliable");
        return -E_NET_NONE;
    }
    Pktbuf* pktbuf = pktbuf_alloc(len);
    if (!pktbuf) {
        dbg_error(DBG_UDP, "no buffer");
        return -E_NO_MEM;
    }
    int ret = pktbuf_write(pktbuf, (uint8_t*)buf, len);
    if (ret < 0) {
        dbg_error(DBG_UDP, "copy data error");
        goto end_sendto;
    }
    ret = udp_out(&dest_ip, dport, &socket->local_ip, socket->local_port, pktbuf);
    if (ret < 0) {
        dbg_error(DBG_UDP, "send error");
        goto end_sendto;
    }
    if (result_len) { *result_len = (ssize_t)len; }
    return NET_OK;
end_sendto:
    pktbuf_free(pktbuf);
    return ret;
}

int udp_out(Ipaddr* dest, uint16_t dport, Ipaddr* src, uint16_t sport, Pktbuf* buf) {
    dbg_info(DBG_UDP, "send an udp packet!");
    if (!src || ipaddr_is_any(src)) {
        Rentry* rt = rt_find(dest);
        if (rt == nullptr) {
            dump_ip(DBG_UDP, "no route to dest: ", dest);
            return -E_NET_MATCH;
        }
        src = &rt->netif->ipaddr;
    }
    int ret = pktbuf_add_header(buf, sizeof(Udp_hdr), 1);
    if (ret < 0) {
        dbg_error(DBG_UDP, "add header failed. ret = %d", ret);
        return -E_NET_SIZE;
    }
    Udp_hdr* udp_hdr = (Udp_hdr*)pktbuf_data(buf);
    udp_hdr->src_port = x_htons(sport);
    udp_hdr->dest_port = x_htons(dport);
    udp_hdr->total_len = x_htons(buf->total_size);
    udp_hdr->checksum = checksum_peso(src->a_addr, dest->a_addr, NET_PROTOCOL_UDP, buf);
    ret = ipv4_out(NET_PROTOCOL_UDP, dest, src, buf);
    if (ret < 0) {
        dbg_error(DBG_UDP, "udp out error, ret = %d", ret);
        return ret;
    }
    return ret;
}

static Udp* udp_find(Ipaddr* src_ip, uint16_t sport, Ipaddr* dest_ip, uint16_t dport) {
    if (dport == 0) { return nullptr; }
    int score, best_score = 0;
    List_entry* le;
    Udp* udp = nullptr;
    Socket* best_socket = nullptr;
    acquire(&udp_list_lock);
    list_for_each(le, &udp_list) {
        score = 0;
        Socket* socket = le2socket(le);
        if (socket->local_port != dport) { continue; }
        if (!ipaddr_is_any(&socket->local_ip) && !ipaddr_is_equal(&socket->local_ip, dest_ip)) { continue; }
        if (!ipaddr_is_any(&socket->remote_ip) && !ipaddr_is_equal(&socket->remote_ip, src_ip)) { continue; }
        if (socket->remote_port && (socket->remote_port != sport)) { continue; }
        if (!ipaddr_is_any(&socket->local_ip) && ipaddr_is_equal(&socket->local_ip, dest_ip)) { score++; }
        if (!ipaddr_is_any(&socket->remote_ip) && ipaddr_is_equal(&socket->remote_ip, src_ip)) { score++; }
        if (socket->remote_port && (socket->remote_port == sport)) { score++; }
        if (score >= best_score) {
            best_socket = socket;
            best_score = score;
        }
    }
    release(&udp_list_lock);
    if (best_socket != nullptr) { udp = skop_info(best_socket, udp); }
    return udp;
}

static int is_pkt_ok(Udp_pkt *pkt, int size) {
    if ((size < sizeof(Udp_hdr)) || (size < pkt->hdr.total_len)) {
        dbg_error(DBG_UDP, "udp packet size incorrect: %d!", size);
        return -E_NET_SIZE;
    }
    return NET_OK;
}

int udp_in(Pktbuf* buf, Ipaddr* src_ip, Ipaddr* dest_ip) {
    dbg_info(DBG_UDP, "Recv a udp packet!");
    int iphdr_size = ipv4_hdr_size((Ipv4_pkt*)pktbuf_data(buf));
    int ret = pktbuf_set_cont(buf, sizeof(Udp_hdr) + iphdr_size);
    if (ret < 0) {
        dbg_error(DBG_UDP, "set udp cont failed");
        return ret;
    }
    Ipv4_pkt* ip_pkt = (Ipv4_pkt*)pktbuf_data(buf);
    Udp_pkt* udp_pkt = (Udp_pkt*)((uint8_t*)ip_pkt + iphdr_size);
    uint16_t local_port = x_ntohs(udp_pkt->hdr.dest_port);
    uint16_t remote_port = x_ntohs(udp_pkt->hdr.src_port);
    // Ipaddr ip;
    // ipaddr_from_str(&ip, "192.168.8.27");
    // if (ipaddr_is_equal(src_ip, &ip)) {
    //     cprintf("yyyyyyyyyyyy\n");
    // }
    Udp* udp = (Udp*)udp_find(src_ip, remote_port, dest_ip, local_port);
    if (!udp) {
        dbg_warning(DBG_UDP, "no udp for this packet");
        return -E_NET_UNREACH;
    }
    pktbuf_remove_header(buf, iphdr_size);
    udp_pkt = (Udp_pkt *)pktbuf_data(buf);
    if (udp_pkt->hdr.checksum) {
        pktbuf_reset_acc(buf);
        if (checksum_peso(dest_ip->a_addr, src_ip->a_addr, NET_PROTOCOL_UDP, buf)) {
            dbg_warning(DBG_UDP, "udp check sum incorrect");
            return -E_NET_CHECKSUM;
        }
    }

    udp_pkt = (Udp_pkt *)(pktbuf_data(buf));
    udp_pkt->hdr.src_port = x_ntohs(udp_pkt->hdr.src_port);
    udp_pkt->hdr.dest_port = x_ntohs(udp_pkt->hdr.dest_port);
    udp_pkt->hdr.total_len = x_ntohs(udp_pkt->hdr.total_len);
    if ((ret = is_pkt_ok(udp_pkt, buf->total_size)) < 0) {
        // dbg_error(DBG_UDP, "udp packet error");
        return ret;
    }
    display_udp_packet(udp_pkt);
    static_assert(sizeof(Udp_hdr) < sizeof(Udp_from));
    // pktbuf_remove_header(buf, (size_t)(sizeof(Udp_hdr) - sizeof(Udp_from)));
    pktbuf_add_header(buf, sizeof(Udp_from) - sizeof(Udp_hdr), false);
    pktbuf_set_cont(buf, sizeof(Udp_from));
    pktbuf_reset_acc(buf);
    Udp_from* from = (Udp_from *)pktbuf_data(buf);
    from->port = remote_port;
    ipaddr_copy(&from->from, src_ip);
    if (list_count(&udp->recv_pkt_list) < UDP_MAX_RECV) {
        list_add(&udp->recv_pkt_list, &buf->sock_recv_link);
        Socket* socket = info2sk(udp, udp);
        if(dns_is_arrive(udp)){
            dns_in();
        }else{
            sock_wakeup(socket, WT_SOCK_READ, NET_OK);
        }
    } else {
        dbg_warning(DBG_UDP, "queue full, drop pkt");
        pktbuf_free(buf);
    }
    return NET_OK;
}

int udp_recvfrom(Socket* socket, void* buf, size_t len, int flags, Sockaddr* src, socklen_t* addr_len,
                        ssize_t* result_len) {
    Udp* udp = skop_info(socket, udp);
    if (list_empty(&udp->recv_pkt_list)) {
        *result_len = 0;
        return NET_NEED_WAIT;
    }
    List_entry* le = list_next(&udp->recv_pkt_list);
    list_del(le);
    Pktbuf* pktbuf = le2sock_recv(le);
    Udp_from* from = (Udp_from *)pktbuf_data(pktbuf);
    Sockaddr_in* addr = (Sockaddr_in*)src;
    memset(addr, 0, sizeof(Sockaddr));
    addr->sin_family = AF_INET;
    addr->sin_port = x_htons(from->port);
    ipaddr_to_buf(&from->from, addr->sin_addr.addr_array);
    pktbuf_remove_header(pktbuf, sizeof(Udp_from));
    size_t size = (pktbuf->total_size > len) ? len : pktbuf->total_size;
    pktbuf_reset_acc(pktbuf);
    int ret = pktbuf_read(pktbuf, buf, size);
    if (ret < 0) {
        pktbuf_free(pktbuf);
        dbg_error(DBG_UDP, "pktbuf read error");
        return ret;
    }
    pktbuf_free(pktbuf);
    if(result_len)
    *result_len = size;
    return NET_OK;
}

static int udp_close(Socket* socket) {
    Udp* udp = skop_info(socket, udp);
    List_entry* le;
    while ((le = list_next(&udp->recv_pkt_list)) != &udp->recv_pkt_list) {
        list_del(le);
        Pktbuf* buf = le2sock_recv(le);
        pktbuf_free(buf);
    }
    acquire(&udp_list_lock);
    list_del_init(&socket->socket_link);
    release(&udp_list_lock);
    display_udp_list();
    return NET_OK;
}

static int udp_connect(Socket* socket, Sockaddr* addr, socklen_t len){
    sock_connect(socket, addr, len);
    display_udp_list();
    return NET_OK;
}

int udp_bind(Socket* socket, Sockaddr* addr, socklen_t len){
    Sockaddr_in* addr_in = (Sockaddr_in *)addr;
    if (socket->local_port != NET_PORT_EMPTY) {
        dbg_error(DBG_UDP, "already binded.");
        return -E_NET_BIND;
    }

    Ipaddr local_ip;
    ipaddr_from_buf(&local_ip, (uint8_t*)&addr_in->sin_addr.addr_array);
    int port = x_ntohs(addr_in->sin_port);
    Udp *udp = nullptr;
    List_entry* le;
    acquire(&udp_list_lock);
    list_for_each(le, &udp_list) {
        Socket* s = le2socket(le);
        if(s == socket){
            continue;
        }
        if (ipaddr_is_equal(&socket->local_ip, &local_ip) && (socket->local_port == port)) {
            udp = skop_info(s, udp);
            break;
        }
    }
    release(&udp_list_lock);
    if (udp) {
        dbg_error(DBG_UDP, "port already used!");
        return -E_NET_BIND;
    } else {
        sock_bind(socket, addr, len);
    }
    display_udp_list();
    return NET_OK;
}
