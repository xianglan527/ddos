#include "tcp.h"

#include "ipv4.h"
#include "nettool.h"
#include "protocol.h"
#include "rand.h"
#include "slab.h"
#include "sock.h"
#include "socket.h"
#include "stdio.h"
#include "tcp_buf.h"
#include "tcp_out.h"
#include "tcp_state.h"

static List_entry tcp_list;
Spinlock tcp_list_lock;

List_entry tcp_clean_list;
Spinlock tcp_clean_list_lock;
extern Spinlock print_struct_lock;

static int tcp_close(Socket* socket);
static int tcp_connect(Socket* socket, Sockaddr* addr, socklen_t len);
static int tcp_send(Socket* socket, void* buf, size_t len, int flags, ssize_t* result_len);
static int tcp_recv(Socket* socket, void* buf, size_t len, int flags, ssize_t* result_len);
static int tcp_setopt(Socket* socket, int level, int optname, char* optval, int optlen);
static int tcp_bind(Socket* socket, Sockaddr* addr, socklen_t len);
static int tcp_listen(Socket* socket, int backlog);
static int tcp_accept(Socket* socket, Sockaddr* addr, socklen_t* len, Socket** client);
static void tcp_destory(Socket* socket);

#if DBG_DISP_ENABLED(DBG_TCP)
void tcp_show_info(char* msg, Socket* socket) {
    Tcp* tcp = skop_info(socket, tcp);
    cprintf("%s: %s\n", msg, (tcp->state < TCP_STATE_MAX) ? tcp_state_name(tcp->state) : "UNKNOWN");
    cprintf("    local port: %u, remote port: %u\n", socket->local_port, socket->remote_port);
    cprintf("    snd.una: %u, snd.nxt: %u\n", tcp->snd.una, tcp->snd.nxt);
}

void tcp_display_pkt(char* msg, Tcp_hdr* tcp_hdr, Pktbuf* buf) {
    acquire(&print_struct_lock);
    cprintf("%s\n", msg);
    cprintf("    sport: %u, dport: %u\n", tcp_hdr->sport, tcp_hdr->dport);
    cprintf("    seq: %u, ack: %u, win: %d\n", tcp_hdr->seq, tcp_hdr->ack, tcp_hdr->win);
    cprintf("    flags:");
    if (tcp_hdr->f_syn) { cprintf(" syn"); }
    if (tcp_hdr->f_rst) { cprintf(" rst"); }
    if (tcp_hdr->f_ack) { cprintf(" ack"); }
    if (tcp_hdr->f_psh) { cprintf(" push"); }
    if (tcp_hdr->f_fin) { cprintf(" fin"); }
    cprintf("\n    len=%d", buf->total_size - tcp_hdr_size(tcp_hdr));
    cprintf("\n");
    release(&print_struct_lock);
}

void tcp_show_list(void) {
    int idx = 0;
    List_entry* le;
    acquire(&print_struct_lock);
    acquire(&tcp_list_lock);
    cprintf("\n-------------dump tcp list ---------- \n");
    list_for_each(le, &tcp_list) {
        Socket* socket = le2socket(le);
        Tcp* tcp = skop_info(socket, tcp);
        cprintf("[%d]\n", idx++);
        cprintf("\ttcp address is %p\n", tcp);
        tcp_show_info("", socket);
    }
    cprintf("\n-------------dump tcp list end ---------- \n");
    release(&tcp_list_lock);
    release(&print_struct_lock);
}
#endif

int tcps_init(void) {
    dbg_info(DBG_TCP, "tcp init.");
    list_init(&tcp_list);
    initlock(&tcp_list_lock, "tcp_clean_list_lock");
    list_init(&tcp_clean_list);
    initlock(&tcp_clean_list_lock, "tcp_clean_list_lock");
    dbg_info(DBG_TCP, "init done.");
    return NET_OK;
}

int tcp_init(Socket* socket, int family, int protocol) {
    static Sock_ops tcp_ops = {
        .connect = tcp_connect,
        .close = tcp_close,
        .send = tcp_send,
        .recv = tcp_recv,
        .setopt = tcp_setopt,
        .bind = tcp_bind,
        .listen = tcp_listen,
        .accept = tcp_accept,
        .destroy = tcp_destory,
    };
    Tcp* tcp = skop_info(socket, tcp);
    int ret = socket_init(socket, family, protocol, &tcp_ops);
    if (ret < 0) {
        dbg_error(DBG_TCP, "init tcp failed.");
        return ret;
    }
    tcp->snd.una = tcp->snd.nxt = tcp->snd.iss = 0;
    tcp->snd.ostate = TCP_OSTATE_IDLE;
    tcp->snd.rto = TCP_INIT_RTO;           
    tcp->snd.rexmit_max = TCP_INT_RETRIES;  
    tcp->rcv.nxt = tcp->rcv.iss = 0;
    tcp->snd.buf.data = nullptr;
    tcp->rcv.buf.data = nullptr;
    tcp->mss = 0;
    tcp->state = TCP_STATE_CLOSED;
    tcp->flags.keep_enable = 0;
    tcp->conn.keep_idle = TCP_KEEPALIVE_TIME;
    tcp->conn.keep_intvl = TCP_KEEPALIVE_INTVL;
    tcp->conn.keep_cnt = TCP_KEEPALIVE_PROBES;
    tcp->conn.keep_retry = 0;
    tcp->conn.keep_timer = nullptr;
    tcp->parent = nullptr;
    tcp->flags.inactive = 0;
    assert(list_empty(&socket->socket_link));
    acquire(&tcp_list_lock);
    list_add(&tcp_list, &socket->socket_link);
    release(&tcp_list_lock);
    // display_tcp_list();
    return NET_OK;
}

void tcp_free(Tcp* tcp) {
    Socket* socket = info2sk(tcp, tcp);
    tcp_kill_all_timers(tcp);
    if (tcp->snd.buf.data != nullptr) {
        kfree(tcp->snd.buf.data);
        tcp->snd.buf.data = nullptr;
    }
    if (tcp->rcv.buf.data != nullptr) {
        kfree(tcp->rcv.buf.data);
        tcp->rcv.buf.data = nullptr;
    }
    tcp->state = TCP_STATE_FREE;
    acquire(&tcp_list_lock);
    list_del(&socket->socket_link);
    release(&tcp_list_lock);
}

void tcp_clear_parent(Tcp* tcp) {
    List_entry* le;
    list_for_each(le, &tcp_list) {
        Socket* socket = le2socket(le);
        Tcp* child = skop_info(socket, tcp);
        if (child->parent == tcp) { tcp_free(tcp); }
    }
}

static int tcp_close(Socket* socket) {
    Tcp* tcp = skop_info(socket, tcp);
    dbg_info(DBG_TCP, "closing tcp: state = %s", tcp_state_name(tcp->state));
    switch (tcp->state) {
        case TCP_STATE_CLOSED:
            dbg_info(DBG_TCP, "tcp already closed");
            tcp_free(tcp);
            return NET_OK;
        case TCP_STATE_LISTEN:
            tcp_clear_parent(tcp);
            tcp_abort(tcp, -E_NET_CLOSE);
            tcp_free(tcp);
            return NET_OK;
        case TCP_STATE_SYN_RECVD:
        case TCP_STATE_SYN_SENT:
            tcp_abort(tcp, -E_NET_CLOSE);
            tcp_free(tcp);
            return NET_OK;
        case TCP_STATE_CLOSE_WAIT:
            tcp_send_fin(tcp);
            tcp_set_state(tcp, TCP_STATE_LAST_ACK);
            return NET_NEED_WAIT;
        case TCP_STATE_ESTABLISHED:
            tcp_send_fin(tcp);
            tcp_set_state(tcp, TCP_STATE_FIN_WAIT_1);
            return NET_NEED_WAIT;
        default:
            dbg_error(DBG_TCP, "tcp state error[%s]: send is not allowed", tcp_state_name(tcp->state));
            return -E_NET_STATE;
    }
}

static int alloc_port(void) {
#if 1  // NET_DBG
    int search_idx = rand() % 1000 + NET_PORT_DYN_START;
#else
    static int search_idx = NET_PORT_DYN_START;
#endif
    acquire(&tcp_list_lock);
    for (int i = NET_PORT_DYN_START; i < NET_PORT_DYN_END; i++) {
        List_entry* le;
        list_for_each(le, &tcp_list) {
            Socket* socket = le2socket(le);
            if (socket->local_port == search_idx) { break; }
        }
        int port = search_idx++;
        if (search_idx >= NET_PORT_DYN_END) { search_idx = NET_PORT_DYN_START; }
        if (le == &tcp_list) {
            release(&tcp_list_lock);
            return port;
        }
    }
    release(&tcp_list_lock);
    return -1;
}

static uint32_t tcp_get_iss(void) {
    static uint32_t seq = 0;
    seq += seq == 0 ? 32435 : 305;
    return seq;
}

static int tcp_init_connect(Tcp* tcp) {
    Rentry* rt = rt_find(&info2sk(tcp, tcp)->remote_ip);
    if (rt->netif->mtu == 0 || !ipaddr_is_any(&rt->next_hop)) {
        tcp->mss = TCP_DEFAULT_MSS;
    } else {
        tcp->mss = rt->netif->mtu - sizeof(Ipv4_hdr) - sizeof(Tcp_hdr);
    }
    uint8_t* snd_buf_data = (uint8_t*)kmalloc(TCP_SBUF_SIZE);
    assert(snd_buf_data != nullptr);
    tcp_buf_init(&tcp->snd.buf, snd_buf_data, TCP_SBUF_SIZE);
    uint8_t* rcv_buf_data = (uint8_t*)kmalloc(TCP_RBUF_SIZE);
    assert(rcv_buf_data != nullptr);
    tcp_buf_init(&tcp->rcv.buf, rcv_buf_data, TCP_RBUF_SIZE);
    tcp->snd.iss = tcp_get_iss();
    tcp->snd.una = tcp->snd.nxt = tcp->snd.iss;
    tcp->rcv.nxt = 0;
    return NET_OK;
}

static int tcp_connect(Socket* socket, Sockaddr* addr, socklen_t len) {
    Tcp* tcp = skop_info(socket, tcp);
    if (tcp->state != TCP_STATE_CLOSED) {
        dbg_error(DBG_TCP, "tcp is not closed. connect is not allowed");
        return -E_NET_STATE;
    }
    Sockaddr_in* addr_in = (Sockaddr_in*)addr;
    ipaddr_from_buf(&socket->remote_ip, (uint8_t*)&addr_in->sin_addr.s_addr);
    socket->remote_port = x_ntohs(addr_in->sin_port);
    if (socket->local_port == NET_PORT_EMPTY) {
        int port = alloc_port();
        if (port == -1) {
            dbg_error(DBG_TCP, "alloc port failed.");
            return -E_NET_NONE;
        }
        socket->local_port = port;
    }
    if (ipaddr_is_any(&socket->local_ip)) {
        Rentry* rt = rt_find(&socket->remote_ip);
        if (rt == nullptr) {
            dbg_error(DBG_TCP, "no route to dest");
            return -E_NET_UNREACH;
        }
        ipaddr_copy(&socket->local_ip, &rt->netif->ipaddr);
    }
    int ret;
    if ((ret = tcp_init_connect(tcp)) < 0) {
        dbg_error(DBG_TCP, "init conn failed.");
        return ret;
    }
    if ((ret = tcp_send_syn(tcp)) < 0) {
        dbg_error(DBG_TCP, "send syn failed.");
        return ret;
    }
    tcp_set_state(tcp, TCP_STATE_SYN_SENT);
    return NET_NEED_WAIT;
}

// Tcp* tcp_find(Ipaddr* local_ip, uint16_t local_port, Ipaddr* remote_ip, uint16_t remote_port) {
//     acquire(&tcp_list_lock);
//     List_entry* le;
//     Tcp *match_tcp = nullptr;
//     list_for_each(le, &tcp_list) {
//         Socket* s = le2socket(le);
//         if ((s->local_port == local_port) && ipaddr_is_equal(&s->remote_ip, remote_ip) &&
//             (s->remote_port == remote_port)) {
//             if (ipaddr_is_any(&s->local_ip) || ipaddr_is_equal(&s->local_ip, local_ip)) {
//                 release(&tcp_list_lock);
//                 return skop_info(s, tcp);
//             }
//             // if (ipaddr_is_equal(&s->local_ip, local_ip)) {
//             //     release(&tcp_list_lock);
//             //     return skop_info(s, tcp);
//             // } else if (ipaddr_is_any(&s->local_ip)) {
//             //     match_tcp = skop_info(s, tcp);
//             // }
//         }
//         Tcp *tcp = skop_info(s, tcp);
//         if ((tcp->state == TCP_STATE_LISTEN) && (s->local_port == local_port)) {
//             if (ipaddr_is_equal(&s->local_ip, local_ip)) {
//                 release(&tcp_list_lock);
//                 return skop_info(s, tcp);
//             } else if (ipaddr_is_any(&s->local_ip)) {
//                 match_tcp = skop_info(s, tcp);
//             }
//         }
//     }
//     release(&tcp_list_lock);
//     return match_tcp;
// }

Tcp* tcp_find(Ipaddr* local_ip, uint16_t local_port, Ipaddr* remote_ip, uint16_t remote_port) {
    acquire(&tcp_list_lock);
    List_entry* le;
    Tcp* match_tcp = nullptr;
    list_for_each(le, &tcp_list) {
        Socket* s = le2socket(le);
        if ((s->local_port == local_port) && ipaddr_is_equal(&s->remote_ip, remote_ip) &&
            (s->remote_port == remote_port)) {
            if (ipaddr_is_equal(&s->local_ip, local_ip)) {
                release(&tcp_list_lock);
                return skop_info(s, tcp);
            } else if (ipaddr_is_any(&s->local_ip)) {
                match_tcp = skop_info(s, tcp);
            }
        }
    }
    if (match_tcp == nullptr) {
        list_for_each(le, &tcp_list) {
            Socket* s = le2socket(le);
            Tcp* tcp = skop_info(s, tcp);
            if ((tcp->state == TCP_STATE_LISTEN) && (s->local_port == local_port)) {
                if (ipaddr_is_equal(&s->local_ip, local_ip)) {
                    release(&tcp_list_lock);
                    return skop_info(s, tcp);
                } else if (ipaddr_is_any(&s->local_ip)) {
                    match_tcp = skop_info(s, tcp);
                }
            }
        }
    }
    release(&tcp_list_lock);
    return match_tcp;
}

int tcp_abort(Tcp* tcp, int ret) {
    tcp_kill_all_timers(tcp);
    tcp_set_state(tcp, TCP_STATE_CLOSED);
    sock_wakeup(info2sk(tcp, tcp), WT_SOCK_ALL, ret);
    return NET_OK;
}

static size_t tcp_write_sndbuf(Tcp* tcp, uint8_t* buf, size_t len) {
    size_t free_cnt = tcp_buf_free_cnt(&tcp->snd.buf);
    if (free_cnt == 0) { return 0; }
    size_t wr_len = (len > free_cnt) ? free_cnt : len;
    tcp_buf_write_send(&tcp->snd.buf, buf, wr_len);
    return wr_len;
}

static int tcp_send(Socket* socket, void* buf, size_t len, int flags, ssize_t* result_len) {
    Tcp* tcp = skop_info(socket, tcp);
    switch (tcp->state) {
        case TCP_STATE_CLOSED: dbg_error(DBG_TCP, "tcp closed: send is not allowed"); return -E_NET_CLOSE;
        case TCP_STATE_FIN_WAIT_1:
        case TCP_STATE_FIN_WAIT_2:
        case TCP_STATE_CLOSING:
        case TCP_STATE_TIME_WAIT:
        case TCP_STATE_LAST_ACK:
            dbg_error(DBG_TCP, "tcp closed[%s]: send is not allowed", tcp_state_name(tcp->state));
            return -E_NET_CLOSE;
        case TCP_STATE_ESTABLISHED:
        case TCP_STATE_CLOSE_WAIT: {
            break;
        }
        case TCP_STATE_LISTEN:
        case TCP_STATE_SYN_RECVD:
        case TCP_STATE_SYN_SENT:
        default:
            dbg_error(DBG_TCP, "tcp state error[%s]: send is not allowed", tcp_state_name(tcp->state));
            return -E_NET_STATE;
    }
    size_t size = tcp_write_sndbuf(tcp, (uint8_t*)buf, len);
    if (size == 0) {
        *result_len = 0;
        return NET_NEED_WAIT;
    } else {
        *result_len = size;
        tcp_out_event(tcp, TCP_OEVENT_SEND);
        // tcp_transmit(tcp);
        return NET_OK;
    }
}

void tcp_read_options(Tcp* tcp, Tcp_hdr* tcp_hdr) {
    uint8_t* opt_start = (uint8_t*)tcp_hdr + sizeof(Tcp_hdr);
    uint8_t* opt_end = opt_start + (tcp_hdr_size(tcp_hdr) - sizeof(Tcp_hdr));
    if (opt_end <= opt_start) { return; }
    while (opt_start < opt_end) {
        Tcp_opt_mss* opt = (Tcp_opt_mss*)opt_start;
        switch (opt_start[0]) {
            case TCP_OPT_MSS: {
                if (opt->length == 4) {
                    uint16_t mss = x_ntohs(opt->mss);
                    if (tcp->mss > mss) { tcp->mss = mss; }
                } else {
                    opt_start++;
                }
                opt_start += opt->length;
                break;
            }
            case TCP_OPT_NOP: {
                opt_start++;
                break;
            }
            case TCP_OPT_END: {
                return;
            }
            default: {
                opt_start++;
                break;
            }
        }
    }
}

static int tcp_recv(Socket* socket, void* buf, size_t len, int flags, ssize_t* result_len) {
    Tcp* tcp = skop_info(socket, tcp);
    int need_wait = NET_NEED_WAIT;
    switch (tcp->state) {
        case TCP_STATE_LAST_ACK:
        case TCP_STATE_CLOSED: dbg_error(DBG_TCP, "tcp closed"); return -E_NET_CLOSE;
        case TCP_STATE_CLOSE_WAIT:
        case TCP_STATE_CLOSING: need_wait = NET_OK_CLOSE_WAIT; break;
        case TCP_STATE_FIN_WAIT_1:
        case TCP_STATE_FIN_WAIT_2:
        case TCP_STATE_ESTABLISHED: break;
        case TCP_STATE_LISTEN:
        case TCP_STATE_SYN_SENT:
        case TCP_STATE_SYN_RECVD:
        case TCP_STATE_TIME_WAIT:
        default: dbg_error(DBG_TCP, "tcp state error"); return -E_NET_STATE;
    }
    *result_len = 0;
    size_t cnt = tcp_buf_read_rcv(&tcp->rcv.buf, buf, len);
    if (cnt > 0) {
        *result_len = cnt;
        return NET_OK;
    }
    return need_wait;
}

size_t tcp_rcv_window(Tcp* tcp) {
    size_t window = tcp_buf_free_cnt(&tcp->rcv.buf);
    return window;
}

static int tcp_setopt(Socket* socket, int level, int optname, char* optval, int optlen) {
    int ret = sock_setopt(socket, level, optname, optval, optlen);
    if (ret == NET_OK) {
        return NET_OK;
    } else if ((ret < 0) && (ret != -E_NET_NOT_SUPPORT)) {
        return ret;
    }
    Tcp* tcp = skop_info(socket, tcp);
    if (level == SOL_SOCKET) {
        if (optlen != sizeof(int)) {
            dbg_error(DBG_TCP, "param size error");
            return -E_NET_PARAM;
        }
        tcp_keepalive_start(tcp, *(int*)optval);
        return NET_OK;
    } else if (level == SOL_TCP) {
        switch (optname) {
            case TCP_KEEPIDLE:
                if (optlen != sizeof(int)) {
                    dbg_error(DBG_TCP, "param size error");
                    return -E_NET_PARAM;
                }
                tcp->conn.keep_idle = *(int*)optval;
                tcp_keepalive_restart(tcp);
                return NET_OK;
            case TCP_KEEPINTVL:
                if (optlen != sizeof(int)) {
                    dbg_error(DBG_TCP, "param size error");
                    return -E_NET_PARAM;
                }
                tcp->conn.keep_intvl = *(int*)optval;
                tcp_keepalive_restart(tcp);
                return NET_OK;
            case TCP_KEEPCNT:
                if (optlen != sizeof(int)) {
                    dbg_error(DBG_TCP, "param size error");
                    return -E_NET_PARAM;
                }
                tcp->conn.keep_cnt = *(int*)optval;
                tcp_keepalive_restart(tcp);
                return NET_OK;
            default: dbg_error(DBG_TCP, "unknowm param"); break;
        }
    }
    return -E_NET_PARAM;
}

// void timer_func_test() {
//     Timer* t0 = timer_func_add(timer0_func, "t0", 200, 0);
//     Timer* t1 = timer_func_add(timer1_func, "t1", 500, TIMER_RELOAD);
//     Timer* t2 = timer_func_add(timer2_func, "t2", 500, TIMER_RELOAD);
//     Timer* t3 = timer_func_add(timer3_func, "t3", 1000, TIMER_RELOAD);
//     del_func_timer(t1);
// }

// void timer1_func(Timer* timer) {
//     static int count = 1;
//     cprintf("this is %s: %d\n", (char*)timer->arg, count++);
// }

void tcp_keepalive_tmo(Timer* timer) {
    Tcp* tcp = (Tcp*)timer->arg;
    if (++tcp->conn.keep_retry <= tcp->conn.keep_cnt) {
        tcp_send_keepalive(tcp);
        if (tcp->conn.keep_timer) del_func_timer(tcp->conn.keep_timer);
        tcp->conn.keep_timer = nullptr;
        tcp->conn.keep_timer = timer_func_add(tcp_keepalive_tmo, tcp, tcp->conn.keep_intvl * 1000, 0);
        dbg_info(DBG_TCP, "tcp keepalive tmo, retrying: %d", tcp->conn.keep_retry);
    } else {
        tcp_send_reset_for_tcp(tcp);
        tcp_abort(tcp, -E_NET_CLOSE);
        dbg_error(DBG_TCP, "tcp keepalive tmo, give up");
    }
}

static void keepalive_start_timer(Tcp* tcp) {
    tcp->conn.keep_retry = 0;
    tcp->conn.keep_timer = timer_func_add(tcp_keepalive_tmo, tcp, tcp->conn.keep_idle * 1000, 0);
    dbg_info(DBG_TCP, "tcp keepalive enabled.");
}

void tcp_keepalive_start(Tcp* tcp, bool run) {
    if (tcp->flags.keep_enable && !run) {
        if (tcp->conn.keep_timer) del_func_timer(tcp->conn.keep_timer);
        tcp->conn.keep_timer = nullptr;
        dbg_info(DBG_TCP, "keepalive disabled");
    } else if (!tcp->flags.keep_enable && run) {
        keepalive_start_timer(tcp);
    }
    tcp->flags.keep_enable = run;
}

void tcp_keepalive_restart(Tcp* tcp) {
    if (tcp->flags.keep_enable) {
        if (tcp->conn.keep_timer) del_func_timer(tcp->conn.keep_timer);
        tcp->conn.keep_timer = nullptr;
        keepalive_start_timer(tcp);
        tcp->conn.keep_retry = 0;
    }
}

void tcp_kill_all_timers(Tcp* tcp) {
    if (tcp->conn.keep_timer) {
        del_func_timer(tcp->conn.keep_timer);
        tcp->conn.keep_timer = nullptr;
    }
    if (tcp->snd.snd_timer) {
        del_func_timer(tcp->snd.snd_timer);
        tcp->snd.snd_timer = nullptr;
    }
}

static int tcp_bind(Socket* socket, Sockaddr* addr, socklen_t len) {
    Tcp* tcp = skop_info(socket, tcp);
    if (tcp->state != TCP_STATE_CLOSED) {
        dbg_error(DBG_TCP, "tcp is not closed. connect is not allowed");
        return -E_NET_STATE;
    }
    Sockaddr_in* addr_in = (Sockaddr_in*)addr;
    if (x_ntohs(addr_in->sin_port) == NET_PORT_EMPTY) {
        dbg_error(DBG_TCP, "port is emptry");
        return -E_NET_PARAM;
    }
    if (socket->local_port != NET_PORT_EMPTY) {
        dbg_error(DBG_TCP, "already binded.");
        return -E_NET_BIND;
    }
    Ipaddr local_ip;
    ipaddr_from_buf(&local_ip, (uint8_t*)&addr_in->sin_addr.addr_array);
    if (!ipaddr_is_any(&local_ip)) {
        Rentry* rt = rt_find(&local_ip);
        if (rt == nullptr) {
            dbg_error(DBG_TCP, "ipaddr error, no netif has this ip");
            return -E_NET_PARAM;
        }
        if (!ipaddr_is_equal(&local_ip, &rt->netif->ipaddr)) {
            dbg_error(DBG_TCP, "ipaddr error");
            return -E_NET_PARAM;
        }
    }
    int port = x_ntohs(addr_in->sin_port);
    tcp = nullptr;
    List_entry* le;
    acquire(&tcp_list_lock);
    list_for_each(le, &tcp_list) {
        Socket* s = le2socket(le);
        if (s == socket || s->remote_port != NET_PORT_EMPTY) { continue; }
        if (ipaddr_is_equal(&socket->local_ip, &local_ip) && (socket->local_port == port)) {
            tcp = skop_info(s, tcp);
            break;
        }
    }
    release(&tcp_list_lock);
    if (tcp) {
        dbg_error(DBG_TCP, "port already used!");
        return -E_NET_BIND;
    } else {
        ipaddr_copy(&socket->local_ip, &local_ip);
        socket->local_port = x_ntohs(addr_in->sin_port);
    }
    return NET_OK;
}

static int tcp_listen(Socket* socket, int backlog) {
    Tcp* tcp = skop_info(socket, tcp);
    if (backlog <= 0) {
        dbg_error(DBG_TCP, "backlog(%d) <= 0", backlog);
        return -E_NET_PARAM;
    }
    if (tcp->state != TCP_STATE_CLOSED) {
        dbg_error(DBG_TCP, "tcp is not closed. listen is not allowed");
        return -E_NET_STATE;
    }
    //   tcp->state = TCP_STATE_LISTEN;
    tcp_set_state(tcp, TCP_STATE_LISTEN);
    tcp->conn.backlog = backlog;
    return NET_OK;
}

static int tcp_accept(Socket* socket, Sockaddr* addr, socklen_t* len, Socket** client) {
    List_entry* le;
    acquire(&tcp_list_lock);
    list_for_each(le, &tcp_list) {
        Socket* s = le2socket(le);
        Tcp* tcp = skop_info(s, tcp);
        if ((socket == s) || (tcp->parent != skop_info(socket, tcp)) || tcp->state != TCP_STATE_ESTABLISHED) {
            continue;
        }
        if (tcp->flags.inactive) {
            Sockaddr_in* addr_in = (Sockaddr_in*)addr;
            memset(addr_in, 0, sizeof(Sockaddr_in));
            addr_in->sin_family = AF_INET;
            addr_in->sin_port = x_htons(s->remote_port);
            ipaddr_to_buf(&s->remote_ip, (uint8_t*)&addr_in->sin_addr.addr_array);
            if (len) { *len = sizeof(Sockaddr_in); }
            tcp->flags.inactive = 0;
            *client = s;
            release(&tcp_list_lock);
            return NET_OK;
        }
    }
    release(&tcp_list_lock);
    return NET_NEED_WAIT;
}

int tcp_backlog_count(Tcp* tcp) {
    int count = 0;
    List_entry* le;
    acquire(&tcp_list_lock);
    list_for_each(le, &tcp_list) {
        Socket* s = le2socket(le);
        Tcp* child = skop_info(s, tcp);
        if ((child->parent == tcp) && (child->flags.inactive)) { count++; }
    }
    release(&tcp_list_lock);
    return count;
}

Tcp* tcp_create_child(Tcp* parent, Tcp_seg* seg) {
    Socket* parent_socket = info2sk(parent, tcp);
    Socket* child_socket = new_socket();
    assert(child_socket != nullptr);
    child_socket->sk_type = parent_socket->sk_type;
    int ret = tcp_init(child_socket, parent_socket->family, parent_socket->protocol);
    Tcp* child = skop_info(child_socket, tcp);
    if (ret < 0) {
        dbg_error(DBG_TCP, "no child tcp");
        return nullptr;
    }
    ipaddr_copy(&child_socket->local_ip, &seg->local_ip);
    ipaddr_copy(&child_socket->remote_ip, &seg->remote_ip);
    child_socket->local_port = seg->hdr->dport;
    child_socket->remote_port = seg->hdr->sport;
    child->parent = parent;
    child->flags.irs_valid = 1;
    child->flags.inactive = 1;
    child->conn.backlog = 0;
    tcp_init_connect(child);
    child->rcv.iss = seg->seq;
    child->rcv.nxt = child->rcv.iss + 1;
    tcp_read_options(child, seg->hdr);
    return child;
}

static void tcp_destory(Socket* socket) {
    Tcp* tcp = skop_info(socket, tcp);
    tcp_kill_all_timers(tcp);
    if (tcp->state != TCP_STATE_TIME_WAIT) { tcp_free(tcp); }
}

void add_clean_tcp_list(Tcp* tcp) {
    acquire(&tcp_clean_list_lock);
    list_add(&tcp_clean_list, &info2sk(tcp, tcp)->socket_link);
    release(&tcp_clean_list_lock);
}

void do_clean_tcp_list(void) {
    List_entry* le;
    // int pp = list_count(&tcp_clean_list);
    acquire(&tcp_clean_list_lock);
    while ((le = list_next(&tcp_clean_list)) != &tcp_clean_list) {
        Socket* s = le2socket(le);
        Tcp* tcp = skop_info(s, tcp);
        assert(s->state == SOCKET_STATE_OPENED && tcp->state == TCP_STATE_FREE);
        list_del(&s->socket_link);
        socket_close(s->id);
    }
    release(&tcp_clean_list_lock);
}