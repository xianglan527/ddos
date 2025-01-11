#include "socket.h"

#include "exmsg.h"

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
        return -1;
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
        ret = sock_wait_enter(req.wait, req.wait_tmo);
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