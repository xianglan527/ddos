#include "socket.h"

#include "exmsg.h"
#include "stdio.h"

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
        // cprintf("jjjjjjjjj\n");
        ret = sock_wait_enter(req.wait, req.wait_tmo);
        // cprintf("kkkkkkkkk\n");
        if (ret < 0) {
            dbg_error(DBG_SOCKET, "recv failed %d.", ret);
            return ret;
        }
    }
}

int setsockopt(int sockfd, int level, int optname,char *optval, int optlen){
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
        return ret;
    }
    // ret = sock_wait_enter(req.wait, req.wait_tmo);
    // if (ret < 0) {
    //     dbg_error(DBG_SOCKET, "close failed %d.", ret);
    //     return ret;
    // }
    return NET_OK;
}

int connect(int sid, Sockaddr *addr, socklen_t len){
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
        dbg_error(DBG_SOCKET, "try connect failed: %d", ret);
        return ret;
    }
    return NET_OK;
}

ssize_t send(int sockfd, void* buf, size_t len, int flags){
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

ssize_t recv(int sockfd, void* buf, size_t len, int flags){
    if (!len || !buf) {
        dbg_error(DBG_SOCKET, "addr size or len error");
        return -E_NET_PARAM;
    }
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
        ret = sock_wait_enter(req.wait, req.wait_tmo);
        if (ret < 0) {
            dbg_error(DBG_SOCKET, "recv failed %d.", ret);
            return ret;
        }
    }
}

int bind(int sid, Sockaddr* addr, socklen_t len){
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