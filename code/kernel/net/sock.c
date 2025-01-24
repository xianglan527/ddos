#include "sock.h"

#include "socket.h"
#include "ipv4.h"

static Socket *socket_map[MAX_SOCKET_PAGES];
static List_entry free_socket_list;
static Spinlock sockets_lock;

extern int raw_init(Socket *socket, int family, int protocol);
extern int udp_init(Socket *socket, int family, int protocol);

int sockets_init(void) {
    for (int i = 0; i < MAX_SOCKET_PAGES; i++) { socket_map[i] = nullptr; }
    initlock(&sockets_lock, "sockets_lock");
    list_init(&free_socket_list);
    return NET_OK;
}

static Socket *get_socket(int id) {
    if (id >= 0 && id < MAX_SOCKET_NUM) {
        int i = id / SOCKET_P_PAGE, j = id % SOCKET_P_PAGE;
        if (socket_map[i] != nullptr) {
            Socket *socket = socket_map[i] + j;
            if (socket->state == SOCKET_STATE_OPENED) { return socket; }
        }
    }
    return nullptr;
}

static void socket_free(Socket *socket) {
    assert(wait_queue_empty(&socket->snd_wait.sem.wait_queue) && socket->snd_wait.sem.valid == 0);
    assert(wait_queue_empty(&socket->rcv_wait.sem.wait_queue) && socket->snd_wait.sem.valid == 0);
    assert(wait_queue_empty(&socket->conn_wait.sem.wait_queue) && socket->snd_wait.sem.valid == 0);
    socket->state = SOCKET_STATE_CLOSED;
    socket->sk_type = SOCK_NONE;
    acquire(&sockets_lock);
    assert(list_empty(&socket->socket_link));
    list_add_before(&free_socket_list, &socket->socket_link);
    release(&sockets_lock);
}

static Socket *new_socket(void) {
    Socket *socket = nullptr;
    acquire(&sockets_lock);
    if (list_empty(&free_socket_list)) {
        int i, id;
        for (i = 0; i < MAX_SOCKET_PAGES; i++) {
            if (socket_map[i] == nullptr) { break; }
        }
        if (i == MAX_SOCKET_PAGES) { goto out; }
        Page *page = AllocPage();
        if (page == nullptr) { goto out; }
        id = i * SOCKET_P_PAGE;
        socket = socket_map[i] = (Socket *)page2kva(page);
        for (i = 0; i < SOCKET_P_PAGE; i++, id++, socket++) {
            socket->id = id;
            socket->state = SOCKET_STATE_CLOSED;
            list_init(&socket->socket_link);
            sock_wait_init(&socket->snd_wait);
            sock_wait_init(&socket->rcv_wait);
            sock_wait_init(&socket->conn_wait);
            socket->snd_wait.wait_state = WT_SOCK_WRITE;
            socket->rcv_wait.wait_state = WT_SOCK_READ;
            socket->conn_wait.wait_state = WT_SOCK_CONN;
            initlock(&socket->socket_lock, "socket_lock");
            list_add_before(&free_socket_list, &socket->socket_link);
        }
    }
    assert(!list_empty(&free_socket_list));
    socket = le2socket(list_next(&free_socket_list));
    list_del_init(&socket->socket_link);
    socket->state = SOCKET_STATE_OPENED;
    socket->sk_type = SOCK_NONE;
out:
    release(&sockets_lock);
    return socket;
}

static inline int get_index(Socket *socket) {
    assert(socket->state == SOCKET_STATE_OPENED);
    return socket->id;
}

static int socket_alloc() {
    Socket *socket;
    int ret = -E_NO_MEM;
    if ((socket = new_socket()) != nullptr) { ret = socket->id; }
    return ret;
}

static int socket_close(int id) {
    Socket *socket;
    if ((socket = get_socket(id)) == nullptr) { return -E_INVAL; }
    acquire(&socket->socket_lock);
    sock_wait_destory(&socket->snd_wait);
    sock_wait_destory(&socket->rcv_wait);
    sock_wait_destory(&socket->conn_wait);
    release(&socket->socket_lock);
    socket_free(socket);
    return 0;
}

void socket_cleanup(void) {
    if (sockets_lock.name == nullptr) return;
    acquire(&sockets_lock);
    int i, j;
    for (i = 0; i < MAX_SOCKET_PAGES; i++) {
        Socket *socket;
        if ((socket = socket_map[i]) != nullptr) {
            for (j = 0; j < SOCKET_P_PAGE; j++, socket++) {
                if (socket->state != SOCKET_STATE_CLOSED) { break; }
            }
            if (j != SOCKET_P_PAGE) { continue; }
            socket = socket_map[i];
            for (j = 0; j < SOCKET_P_PAGE; j++, socket++) { list_del(&socket->socket_link); }
            socket = socket_map[i], socket_map[i] = nullptr;
            FreePage(kva2page((uintptr_t)socket));
        }
    }
    release(&sockets_lock);
}

int sock_create_req_in(Func_msg *api_msg) {
    Socket_info sock_tbl[] = {
        [SOCK_RAW] =
            {
                .protocol = 0,
                .init = raw_init,
            },
        [SOCK_UDP] =
            {
                .protocol = IPPROTO_UDP,
                .init = udp_init,
            },
    };
    Sock_req *req = (Sock_req *)api_msg->param;
    Sock_create *param = &req->create;
    int ret = socket_alloc();
    if (ret < 0) {
        dbg_error(DBG_SOCKET, "no socket");
        return ret;
    }
    req->sockfd = ret;
    Socket *socket = get_socket(req->sockfd);
    if ((param->type < 0) || (param->type >= sizeof(sock_tbl) / sizeof(sock_tbl[0]))) {
        dbg_error(DBG_SOCKET, "unknown type: %d", param->type);
        socket_close(req->sockfd);
        return -E_NET_PARAM;
    }
    socket->sk_type = param->type;
    Socket_info *info = sock_tbl + param->type;
    if (param->protocol == 0) { param->protocol = info->protocol; }
    ret = info->init(socket, param->family, param->protocol);
    if (ret < 0) {
        dbg_error(DBG_SOCKET, "init socket failed, type: %d", param->type);
        socket_close(req->sockfd);
        return ret;
    }
    return NET_OK;
}

int socket_init(Socket *socket, int family, int protocol, Sock_ops *ops) {
    socket->protocol = protocol;
    socket->ops = ops;
    socket->family = family;
    ipaddr_set_any(&socket->local_ip);
    ipaddr_set_any(&socket->remote_ip);
    socket->local_port = 0;
    socket->remote_port = 0;
    socket->ret = NET_OK;
    socket->rcv_tmo = 0;
    socket->snd_tmo = 0;
    list_init(&socket->socket_link);
    return NET_OK;
}

int sock_sendto_req_in(Func_msg *api_msg) {
    Sock_req *req = (Sock_req *)api_msg->param;
    Socket *s = get_socket(req->sockfd);
    if (!s) {
        dbg_error(DBG_SOCKET, "param error: socket = %d.", s);
        return -E_NET_PARAM;
    }
    Sock_data *data = (Sock_data *)&req->data;
    if (!s->ops->sendto) {
        dbg_error(DBG_SOCKET, "this function is not implemented");
        return -E_NET_NOT_SUPPORT;
    }
    int ret = s->ops->sendto(s, data->buf, data->len, data->flags, data->addr, *data->addr_len,
                             &req->data.comp_len);
    if (ret == NET_NEED_WAIT) {
        assert(s->snd_wait.sem.valid == true);
        sock_wait_add(&s->snd_wait, s->snd_tmo, req);
    }
    return ret;
}

int sock_recvfrom_req_in(Func_msg *api_msg) {
    Sock_req *req = (Sock_req *)api_msg->param;
    Socket *s = get_socket(req->sockfd);
    if (!s) {
        dbg_error(DBG_SOCKET, "param error: socket = %d.", s);
        return -E_NET_PARAM;
    }
    Sock_data *data = (Sock_data *)&req->data;
    if (!s->ops->recvfrom) {
        dbg_error(DBG_SOCKET, "this function is not implemented");
        return -E_NET_NOT_SUPPORT;
    }
    int ret = s->ops->recvfrom(s, data->buf, data->len, data->flags, data->addr, data->addr_len,
                               &req->data.comp_len);
    if (ret == NET_NEED_WAIT) {
        assert(s->rcv_wait.sem.valid == true);
        sock_wait_add(&s->rcv_wait, s->rcv_tmo, req);
    }
    return ret;
}

int sock_wait_init(Sock_wait *wait) {
    // wait->waiting = 0;
    sem_init(&wait->sem, 0);
    wait->ret = NET_OK;
    // wait->wait_state = wait_state;
    return NET_OK;
}

void sock_wait_destory(Sock_wait *wait) {
    acquire(&wait->sem.sem_lock);
    wait->sem.valid = 0;
    wakeup_queue(&wait->sem.wait_queue, WT_INTERAUPTED, 1);
    release(&wait->sem.sem_lock);
}

void sock_wait_add(Sock_wait *wait, ulong tmo, Sock_req *req) {
    req->wait = wait;
    req->wait_tmo = tmo;
}

int sock_wait_enter(Sock_wait *wait, ulong tmo) {
    int ret = 0;
    if ((ret = sem_down(&wait->sem, wait->wait_state, tmo)) < 0) { return ret; }
    return wait->ret;
}

void sock_wait_leave(Sock_wait *wait, int ret) {
    sem_up(&wait->sem, wait->wait_state);
    wait->ret = ret;
}

void sock_wakeup(Socket *socket, uint32_t type, int ret) {
    if (type & WT_SOCK_CONN) { sock_wait_leave(&socket->conn_wait, ret); }
    if (type & WT_SOCK_WRITE) { sock_wait_leave(&socket->snd_wait, ret); }
    if (type & WT_SOCK_READ) { sock_wait_leave(&socket->rcv_wait, ret); }
    socket->ret = ret;
}

int sock_setopt(Socket *socket, int level, int optname, char *optval, int optlen) {
    if (level != SOL_SOCKET) {
        dbg_error(DBG_SOCKET, "unknow level: %d", level);
        return -E_NET_NOT_SUPPORT;
    }
    switch (optname) {
        case SO_RCVTIMEO:
        case SO_SNDTIMEO: {
            if (optlen != sizeof(Timeval)) {
                dbg_error(DBG_SOCKET, "time size error");
                return -E_NET_PARAM;
            }
            Timeval *time = (Timeval *)optval;
            int time_ms = time->tv_sec * 1000 + time->tv_usec / 1000;
            if (optname == SO_RCVTIMEO) {
                socket->rcv_tmo = time_ms;
                return NET_OK;
            } else if (optname == SO_SNDTIMEO) {
                socket->snd_tmo = time_ms;
                return NET_OK;
            } else {
                return -E_NET_PARAM;
            }
        }
        default: break;
    }
    return -E_NET_PARAM;
}

int sock_setsockopt_req_in(Func_msg *api_msg) {
    Sock_req *req = (Sock_req *)api_msg->param;
    Socket *s = get_socket(req->sockfd);
    if (!s) {
        dbg_error(DBG_SOCKET, "param error: socket = %d.", s);
        return -E_NET_PARAM;
    }
    Sock_opt *opt = (Sock_opt *)&req->data;
    return s->ops->setopt(s, opt->level, opt->optname, opt->optval, opt->optlen);
}

int sock_close_req_in(Func_msg *api_msg) {
    Sock_req *req = (Sock_req *)api_msg->param;
    Socket *s = get_socket(req->sockfd);
    if (!s) {
        dbg_error(DBG_SOCKET, "param error: socket = %d.", s);
        return -E_NET_PARAM;
    }
    int err = s->ops->close(s);
    socket_close(s->id);
    return err;
}

int sock_connect(Socket *socket, Sockaddr *addr, socklen_t len) {
    Sockaddr_in *remote = (Sockaddr_in *)addr;
    ipaddr_from_buf(&socket->remote_ip, remote->sin_addr.addr_array);
    socket->remote_port = x_ntohs(remote->sin_port);
    return NET_OK;
}

int sock_connect_req_in(Func_msg *api_msg) {
    Sock_req *req = (Sock_req *)api_msg->param;
    Socket *s = get_socket(req->sockfd);
    if (!s) {
        dbg_error(DBG_SOCKET, "param error: socket = %d.", s);
        return -E_NET_PARAM;
    }
    Sock_conn *conn = (Sock_conn *)&req->conn;
    int ret = s->ops->connect(s, conn->addr, conn->len);
    if (ret == NET_NEED_WAIT) {
        assert(s->rcv_wait.sem.valid == true);
        sock_wait_add(&s->conn_wait, s->rcv_tmo, req);
    }
    return ret;
}

int sock_send_req_in(Func_msg *api_msg) {
    Sock_req *req = (Sock_req *)api_msg->param;
    Socket *s = get_socket(req->sockfd);
    if (!s) {
        dbg_error(DBG_SOCKET, "param error: socket = %d.", s);
        return -E_NET_PARAM;
    }
    Sock_data *data = (Sock_data *)&req->data;
    if (!s->ops->send) {
        dbg_error(DBG_SOCKET, "this function is not implemented");
        return -E_NET_NOT_SUPPORT;
    }
    int ret = s->ops->send(s, data->buf, data->len, data->flags, &req->data.comp_len);
    if (ret == NET_NEED_WAIT) {
        assert(s->snd_wait.sem.valid == true);
        sock_wait_add(&s->snd_wait, s->snd_tmo, req);
    }
    return ret;
}

int sock_send(Socket *socket, void *buf, size_t len, int flags, ssize_t *result_len) {
    if (ipaddr_is_any(&socket->remote_ip)) {
        dbg_error(DBG_RAW, "dest ip is empty.");
        return -E_NET_PARAM;
    }
    Sockaddr_in dest;
    dest.sin_family = socket->family;
    dest.sin_port = x_htons(socket->remote_port);
    ipaddr_to_buf(&socket->remote_ip, (uint8_t *)&dest.sin_addr);
    return socket->ops->sendto(socket, buf, len, flags, (Sockaddr *)&dest, sizeof(dest), result_len);
}

int sock_recv(Socket *socket, void *buf, size_t len, int flags, ssize_t *result_len) {
    if (ipaddr_is_any(&socket->remote_ip)) {
        dbg_error(DBG_RAW, "dest ip is empty.");
        return -E_NET_PARAM;
    }
    Sockaddr src;
    socklen_t addr_len;
    return socket->ops->recvfrom(socket, buf, len, flags, &src, &addr_len, result_len);
}

int sock_recv_req_in(Func_msg *api_msg) {
    Sock_req *req = (Sock_req *)api_msg->param;
    Socket *s = get_socket(req->sockfd);
    if (!s) {
        dbg_error(DBG_SOCKET, "param error: socket = %d.", s);
        return -E_NET_PARAM;
    }
    Sock_data *data = (Sock_data *)&req->data;
    if (!s->ops->recvfrom) {
        dbg_error(DBG_SOCKET, "this function is not implemented");
        return -E_NET_NOT_SUPPORT;
    }
    int ret = s->ops->recv(s, data->buf, data->len, data->flags, &req->data.comp_len);
    if (ret == NET_NEED_WAIT) {
        assert(s->rcv_wait.sem.valid == true);
        sock_wait_add(&s->rcv_wait, s->rcv_tmo, req);
    }
    return ret;
}

int sock_bind_req_in(Func_msg *api_msg) {
    Sock_req *req = (Sock_req *)api_msg->param;
    Socket *s = get_socket(req->sockfd);
    if (!s) {
        dbg_error(DBG_SOCKET, "param error: socket = %d.", s);
        return -E_NET_PARAM;
    }
    Sock_bind *bind = (Sock_bind *)&req->bind;
    return s->ops->bind(s, bind->addr, bind->len);
}

int sock_bind(Socket *socket, Sockaddr *addr, socklen_t len) {
    Ipaddr local_ip;
    Sockaddr_in *local = (Sockaddr_in *)addr;
    ipaddr_from_buf(&local_ip, local->sin_addr.addr_array);
    if (!ipaddr_is_any(&local_ip)) {
        Rentry *rt = rt_find(&local_ip);
        if (!rt ||!ipaddr_is_equal(&rt->netif->ipaddr, &local_ip)) {
            dbg_error(DBG_SOCKET, "addr error");
            return -E_NET_PARAM;
        }
    }
    ipaddr_copy(&socket->local_ip, &local_ip);
    socket->local_port = x_ntohs(local->sin_port);
    return NET_OK;
}