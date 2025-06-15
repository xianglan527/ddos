#include "socket.h"

#include "exmsg.h"
#include "stdio.h"
#include "dns.h"

int socket(int family, int type, int protocol) {
    Sock_req req;
    req.wait = nullptr;
    req.wait_tmo = 0;
    req.sockfd = -1;
    req.create.family = family;
    req.create.type = type;
    req.create.protocol = protocol;
    int ret = exmsg_func_exec(sock_create_req_in, &req);
    if (ret < 0) {
        dbg_error(DBG_SOCKET, "create sock failed: %d.", ret);
        return ret;
    }
    return req.sockfd;
}

ssize_t sendto(int sockfd, void* buf, size_t len, int flags, Sockaddr* dest, socklen_t dest_len) {
    if ((dest_len != sizeof(Sockaddr)) || !len) {
        dbg_error(DBG_SOCKET, "addr size or len error");
        return -E_NET;
    }
    if (dest->sa_family != AF_INET) {
        dbg_error(DBG_SOCKET, "family error");
        return -E_NET;
    }
    ssize_t send_size = 0;
    uint8_t* start = (uint8_t*)buf;
    while (len) {
        Sock_req req;
        req.wait = nullptr;
        // req.wait_tmo = 0;
        req.sockfd = sockfd;
        req.data.buf = start;
        req.data.len = len;
        req.data.flags = flags;
        req.data.addr = dest;
        req.data.addr_len = &dest_len;
        req.data.comp_len = 0;
        int ret = exmsg_func_exec(sock_sendto_req_in, &req);
        if (ret < 0) {
            dbg_error(DBG_SOCKET, "write failed.");
            return -E_NET;
        }
        if (req.wait && (ret = sock_wait_enter(req.wait, req.wait_tmo))) {
            if (ret < 0) {
                dbg_error(DBG_SOCKET, "send failed %d.", ret);
                return ret;
            }
        }
        len -= req.data.comp_len;
        send_size += (ssize_t)req.data.comp_len;
        start += req.data.comp_len;
    }
    return send_size;
}

ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags, Sockaddr* dest, socklen_t* dest_len) {
    if (!len || !dest_len || !dest) {
        dbg_error(DBG_SOCKET, "addr size or len error");
        return -E_NET_PARAM;
    }
    Socket* socket = get_socket(sockfd);
    assert(socket != nullptr);
    while (1) {
        Sock_req req;
        req.wait = nullptr;
        // req.wait_tmo = 0;
        req.sockfd = sockfd;
        req.data.buf = buf;
        req.data.len = len;
        req.data.flags = flags;
        req.data.addr = dest;
        req.data.addr_len = dest_len;
        req.data.comp_len = 0;
        int ret = exmsg_func_exec(sock_recvfrom_req_in, &req);
        if (ret < 0) {
            dbg_error(DBG_SOCKET, "write failed.");
            return -E_NET;
        }
        if (req.data.comp_len) { return (ssize_t)req.data.comp_len; }
        if (ret == NET_OK_CLOSE_WAIT && req.data.comp_len == 0) { return 0; }
        ret = sock_wait_enter(req.wait, req.wait_tmo);
        if (ret == -E_NET_CLOSE) {
            dbg_warning(DBG_SOCKET, "remote close");
        } else if (ret < 0) {
            dbg_error(DBG_SOCKET, "recv failed %d.", ret);
            return ret;
        }
    }
}

int setsockopt(int sockfd, int level, int optname, char* optval, int optlen) {
    if (!optval || !optlen) {
        dbg_error(DBG_SOCKET, "param error %d", -E_NET_PARAM);
        return -E_NET_PARAM;
    }
    Sock_req req;
    req.wait = nullptr;
    req.sockfd = sockfd;
    req.opt.level = level;
    req.opt.optname = optname;
    req.opt.optval = optval;
    req.opt.optlen = optlen;
    int ret = exmsg_func_exec(sock_setsockopt_req_in, &req);
    if (ret < 0) {
        dbg_error(DBG_SOCKET, "setopt: %d", ret);
        return ret;
    }
    return NET_OK;
}

int closesocket(int sockfd) {
    Sock_req req;
    req.wait = 0;
    req.sockfd = sockfd;
    int ret = exmsg_func_exec(sock_close_req_in, &req);
    if (ret < 0) {
        dbg_error(DBG_SOCKET, "try close failed %d, force delete.", ret);
        exmsg_func_exec(sock_destroy_req_in, &req);
        socket_close(sockfd);
        return ret;
    }
    // ret = sock_wait_enter(req.wait, req.wait_tmo);
    // if (ret < 0) {
    //     dbg_error(DBG_SOCKET, "close failed %d.", ret);
    //     return ret;
    // }
    if (ret == NET_NEED_WAIT && req.wait) {
        sock_wait_enter(req.wait, req.wait_tmo);
        exmsg_func_exec(sock_destroy_req_in, &req);
    }
    Socket *socket = get_socket(sockfd);
    assert(socket != nullptr);
    if (!(socket->sk_type == SOCK_TCP && skop_info(socket, tcp)->state == TCP_STATE_TIME_WAIT)){
        socket_close(sockfd);
    }
    return NET_OK;
}

int connect(int sid, Sockaddr* addr, socklen_t len) {
    if ((len != sizeof(Sockaddr)) || !addr) {
        dbg_error(DBG_SOCKET, "addr size or len error");
        return -E_NET;
    }
    if (addr->sa_family != AF_INET) {
        dbg_error(DBG_SOCKET, "family error");
        return -E_NET;
    }
    Sock_req req;
    req.wait = 0;
    req.sockfd = sid;
    req.conn.addr = addr;
    req.conn.len = len;
    int ret = exmsg_func_exec(sock_connect_req_in, &req);
    if (ret < 0) {
        dbg_error(DBG_SOCKET, "try connect failed: %e", ret);
        return ret;
    }
    if (req.wait && (ret = sock_wait_enter(req.wait, req.wait_tmo))) {
        if (ret < 0) {
            dbg_error(DBG_SOCKET, "connect failed %e.", ret);
            return ret;
        }
    }
    return NET_OK;
}

ssize_t send(int sockfd, void* buf, size_t len, int flags) {
    ssize_t send_size = 0;
    uint8_t* start = (uint8_t*)buf;
    while (len) {
        Sock_req req;
        req.wait = nullptr;
        // req.wait_tmo = 0;
        req.sockfd = sockfd;
        req.data.buf = start;
        req.data.len = len;
        req.data.flags = flags;
        req.data.comp_len = 0;
        int ret = exmsg_func_exec(sock_send_req_in, &req);
        if (ret < 0) {
            dbg_error(DBG_SOCKET, "write failed.");
            return -E_NET;
        }
        if (req.wait && (ret = sock_wait_enter(req.wait, req.wait_tmo))) {
            if (ret < 0) {
                dbg_error(DBG_SOCKET, "send failed %d.", ret);
                return ret;
            }
        }
        len -= req.data.comp_len;
        send_size += (ssize_t)req.data.comp_len;
        start += req.data.comp_len;
    }
    return send_size;
}

ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
    if (!len || !buf) {
        dbg_error(DBG_SOCKET, "addr size or len error");
        return -E_NET_PARAM;
    }
    Socket* socket = get_socket(sockfd);
    assert(socket != nullptr);
    while (1) {
        Sock_req req;
        req.wait = nullptr;
        // req.wait_tmo = 0;
        req.sockfd = sockfd;
        req.data.buf = buf;
        req.data.len = len;
        req.data.flags = flags;
        req.data.comp_len = 0;
        int ret = exmsg_func_exec(sock_recv_req_in, &req);
        if (ret < 0) {
            dbg_error(DBG_SOCKET, "write failed.");
            return -E_NET;
        }
        if (req.data.comp_len) { return (ssize_t)req.data.comp_len; }
        if (ret == NET_OK_CLOSE_WAIT && req.data.comp_len == 0) { return 0; }
        ret = sock_wait_enter(req.wait, req.wait_tmo);
        if (ret == -E_NET_CLOSE) {
            dbg_warning(DBG_SOCKET, "remote cloase");
        } else if (ret < 0) {
            dbg_error(DBG_SOCKET, "recv failed %d.", ret);
            return ret;
        }
    }
}

int bind(int sid, Sockaddr* addr, socklen_t len) {
    if ((len != sizeof(Sockaddr)) || !addr) {
        dbg_error(DBG_SOCKET, "addr size or len error");
        return -E_NET;
    }
    if (addr->sa_family != AF_INET) {
        dbg_error(DBG_SOCKET, "family error");
        return -E_NET;
    }
    Sock_req req;
    req.wait = 0;
    req.sockfd = sid;
    req.bind.addr = addr;
    req.bind.len = len;
    int ret = exmsg_func_exec(sock_bind_req_in, &req);
    if (ret < 0) {
        dbg_error(DBG_SOCKET, "bind failed: %d", ret);
        return ret;
    }
    return NET_OK;
}

int accept(int sockfd, Sockaddr* addr, socklen_t* len) {
    if (!addr || !len) {
        dbg_error(DBG_SOCKET, "addr len error");
        return -E_NET_PARAM;
    }
    Socket* socket = get_socket(sockfd);
    assert(socket != nullptr);
    while (1) {
        Sock_req req;
        req.wait = nullptr;
        req.sockfd = sockfd;
        req.accept.addr = addr;
        req.accept.len = len;
        req.accept.client_fd = -1;
        int ret = exmsg_func_exec(sock_accept_req_in, &req);
        if (ret < 0) {
            dbg_error(DBG_SOCKET, "setopt: %d", ret);
            return ret;
        }
        if (req.accept.client_fd >= 0) {
            dbg_info(DBG_TCP, "get new connection");
            return req.accept.client_fd;
        }
        if (req.wait && (ret = sock_wait_enter(req.wait, req.wait_tmo))) {
            if (ret < 0) {
                dbg_error(DBG_SOCKET, "connect failed %d.", ret);
                return ret;
            }
        }
    }
}

int listen(int sockfd, int backlog) {
    Sock_req req;
    req.wait = 0;
    req.sockfd = sockfd;
    req.listen.backlog = backlog;
    int ret = exmsg_func_exec(sock_listen_req_in, &req);
    if (ret < 0) {
        dbg_error(DBG_SOCKET, "setopt:", ret);
        return ret;
    }
    return NET_OK;
}

int gethostbyname_r(char* name, Hostent* ret, char* buf, size_t len, Hostent** result, int* err){
    if (!name || !ret || !buf || !len || !result) {
        dbg_error(DBG_SOCKET, "param error");
        *err = -E_NET_PARAM;
        return -1;
    }
    if (len < (strlen(name) + sizeof(Hostent_extra))) {
        dbg_error(DBG_SOCKET, "size too samll: %d", len);
        *err = -E_NET_PARAM;
        return -1;
    }
    int internal_err;
    if (!err) { err = &internal_err; }
    size_t name_len = strlen(name);             
    if (len < (sizeof(Hostent_extra) + name_len)) {  
        dbg_error(DBG_SOCKET, "buf too small");
        *err = -E_NET_PARAM;
        return -1;
    }
    Dns_req* dns_req = dns_alloc_req();
    strncpy(dns_req->domain_name, name, DNS_DOMAIN_NAME_MAX);
    ipaddr_set_any(&dns_req->ipaddr);
    dns_req->wait_sem.valid = 0;
    int e = exmsg_func_exec(dns_req_in, dns_req);
    if (e < 0) {
        dbg_error(DBG_SOCKET, "get host failed:", e);
        *err = e;
        goto dns_req_error;
    }
    if (dns_req->wait_sem.valid && (e = sem_down(&dns_req->wait_sem, WT_DNS, 0/*DNS_REQ_TMO*/)) < 0) {
        dbg_error(DBG_SOCKET, "wait sem failed.");
        if (e == -E_TIMEOUT) { *err = -E_NET_TMO; }
        *err = -1;
        goto dns_req_error;
    }
    if (dns_req->ret < 0) {
        dbg_error(DBG_SOCKET, "dns resolve failed.");
        *err = dns_req->ret;
        goto dns_req_error;
    }
    Hostent_extra* extra = (Hostent_extra*)buf;
    extra->addr = dns_req->ipaddr.q_addr;
    strncpy(extra->name, name, len - sizeof(Hostent_extra));  
    buf[len - 1] = '\0';                                        
    ret->h_name = extra->name;
    ret->h_aliases = (char**)0;
    ret->h_addrtype = AF_INET;  // IPv4地址
    ret->h_length = 4;          // IPv4，4字节地址
    ret->h_addr_list = (char**)extra->addr_tbl;
    ret->h_addr_list[0] = (char*)&extra->addr;
    ret->h_addr_list[1] = (char*)0;
    *result = ret;
    *err = 0;
    dns_free_req(dns_req);
    return NET_OK;
dns_req_error:
    dns_free_req(dns_req);
    return -1;
}