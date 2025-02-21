#ifndef __NET_SOCK_H__
#define __NET_SOCK_H__

#include "error.h"
#include "exmsg.h"
#include "ipaddr.h"
#include "list.h"
#include "net_config.h"
#include "nettool.h"
#include "raw.h"
#include "sem.h"
#include "spinlock.h"
#include "types.h"
#include "udp.h"
#include "wait.h"
#include "tcp.h"
#include "protocol.h"

#define AF_INET 0

// #define SOCK_RAW 1

#define IPPROTO_ICMP NET_PROTOCOL_ICMPv4

#define IPPROTO_UDP NET_PROTOCOL_UDP

#define IPPROTO_TCP NET_PROTOCOL_TCP

#define INADDR_ANY 0

#define SOL_SOCKET 0

#define SOL_TCP 1

#define SO_RCVTIMEO 1
#define SO_SNDTIMEO 2
#define SO_KEEPALIVE 3
#define TCP_KEEPIDLE 4
#define TCP_KEEPINTVL 5
#define TCP_KEEPCNT 6

#define NET_PORT_EMPTY  0

typedef struct sockaddr Sockaddr;

typedef struct sock_wait {
    int ret;
    // int waiting;
    Sem sem;
    uint32_t wait_state;
} Sock_wait;

static inline size_t sock_wait_count(Sock_wait* wait) {
    acquire(&wait->sem.sem_lock);
    size_t count = list_count(&wait->sem.wait_queue.wait_head);
    release(&wait->sem.sem_lock);
    return count;
}

typedef struct socket Socket;

typedef struct sock_ops {
    int (*close)(Socket* socket);
    int (*sendto)(Socket* socket, void* buf, size_t len, int flags, Sockaddr* dest, socklen_t dest_len,
                  ssize_t* result_len);
    int (*recvfrom)(Socket* socket, void* buf, size_t len, int flags, Sockaddr* src, socklen_t* addr_len,
                    ssize_t* result_len);
    int (*setopt)(Socket* socket, int level, int optname, char* optval, int optlen);
    int (*connect)(Socket* socket, Sockaddr* addr, socklen_t len);
    void (*destroy)(Socket* socket);
    int (*send)(Socket* socket, void* buf, size_t len, int flags, ssize_t* result_len);
    int (*recv)(Socket* socket, void* buf, size_t len, int flags, ssize_t* result_len);
    int (*bind)(Socket* socket, Sockaddr* addr, socklen_t len);
    int (*listen)(Socket* socket, int backlog);
    int (*accept)(Socket* socket, Sockaddr* addr, socklen_t* len, Socket** client);
} Sock_ops;

typedef enum socket_type {
    SOCK_NONE = 0,
    SOCK_RAW,
    SOCK_UDP,
    SOCK_TCP,
} Socket_type;

typedef struct socket {
    union {
        Raw __raw_info;
        Udp __udp_info;
        Tcp __tcp_info;
    } sk_info;
    int id;
    enum {
        SOCKET_STATE_CLOSED,
        SOCKET_STATE_OPENED,
    } state;
    Socket_type sk_type;
    Ipaddr local_ip;
    Ipaddr remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    Sock_ops* ops;
    int family;
    int protocol;
    int ret;
    int rcv_tmo;
    int snd_tmo;
    int close_tmo;
    int conn_tmo;
    List_entry socket_link;
    // Spinlock socket_lock;
    Sock_wait snd_wait;
    Sock_wait rcv_wait;
    Sock_wait conn_wait;
    Sock_wait close_wait;
    // List_entry socket_link;
} Socket;

#define le2socket(le) to_struct((le), Socket, socket_link)

#define SOCK_none SOCK_NONE
#define SOCK_raw SOCK_RAW
#define SOCK_udp SOCK_UDP
#define SOCK_tcp SOCK_TCP

#define __sk_type(TYPE) SOCK_##TYPE

#define check_sk_type(sk, type) ((sk)->sk_type == __sk_type(type) && __sk_type(type) != SOCK_NONE)

#define __skop_info(_sk, type)                                                                      \
    ({                                                                                              \
        Socket* __sk = (_sk);                                                                       \
        assert(__sk != nullptr && __sk->state != SOCKET_STATE_CLOSED && check_sk_type(__sk, type)); \
        &(__sk->sk_info.__##type##_info);                                                           \
    })

#define skop_info(sk, type) __skop_info(sk, type)

#define info2sk(info, type) to_struct((info), Socket, sk_info.__##type##_info)

#define SOCKET_P_PAGE (PGSIZE / sizeof(Socket))
#define MAX_SOCKET_PAGES ((MAX_SOCKET_NUM + SOCKET_P_PAGE - 1) / SOCKET_P_PAGE)

typedef struct socket_head {
    Spinlock socket_list_lock;
    size_t count;
    List_entry socket_list;
} Socket_head;

typedef struct sock_create {
    int family;
    int protocol;
    Socket_type type;
} Sock_create;

typedef struct sock_data {
    uint8_t* buf;
    size_t len;
    int flags;
    Sockaddr* addr;
    socklen_t* addr_len;
    ssize_t comp_len;
} Sock_data;

typedef struct sock_opt {
    int level;
    int optname;
    char* optval;
    int optlen;
} Sock_opt;

typedef struct sock_conn {
    Sockaddr* addr;
    socklen_t len;
} Sock_conn;

typedef struct sock_bind {
    Sockaddr* addr;
    socklen_t len;
} Sock_bind;

typedef struct sock_listen {
    int backlog;
} Sock_listen;

typedef struct sock_accept {
    Sockaddr* addr;
    socklen_t* len;
    int client_fd;
} Sock_accept;

typedef struct sock_req {
    Sock_wait* wait;
    ulong wait_tmo;
    int sockfd;
    union {
        Sock_create create;
        Sock_data data;
        Sock_opt opt;
        Sock_conn conn;
        Sock_bind bind;
        Sock_listen listen;
        Sock_accept accept;
    };
} Sock_req;

typedef struct socket_info Socket_info;
typedef struct socket_info {
    int protocol;
    int (*init)(Socket* socket, int family, int protocol);
} Socket_info;

typedef struct timeval {
    int tv_sec;
    int tv_usec;
} Timeval;

// int socket_alloc(void);
int sockets_init(void);
Socket *new_socket(void);
int socket_close(int id);
Socket *get_socket(int id);
void socket_cleanup(void);
int sock_create_req_in(Func_msg* api_msg);
int socket_init(Socket* socket, int family, int protocol, Sock_ops* ops);
int sock_sendto_req_in(Func_msg* api_msg);
int sock_recvfrom_req_in(Func_msg* api_msg);
int sock_wait_init(Sock_wait* wait);
void sock_wait_destory(Sock_wait* wait);
void sock_wait_add(Sock_wait* wait, ulong tmo, Sock_req* req);
int sock_wait_enter(Sock_wait* wait, ulong tmo);
void sock_wait_leave(Sock_wait* wait, int ret);
void sock_wakeup(Socket* socket, uint32_t type, int ret);
int sock_setopt(Socket* socket, int level, int optname, char* optval, int optlen);
int sock_setsockopt_req_in(Func_msg* api_msg);
int sock_close_req_in(Func_msg* api_msg);
int sock_connect_req_in(Func_msg* api_msg);
int sock_connect(Socket* socket, Sockaddr* addr, socklen_t len);
int sock_send_req_in(Func_msg* api_msg);
int sock_send(Socket* socket, void* buf, size_t len, int flags, ssize_t* result_len);
int sock_recv_req_in(Func_msg* api_msg);
int sock_recv(Socket* socket, void* buf, size_t len, int flags, ssize_t* result_len);
int sock_bind_req_in(Func_msg* api_msg);
int sock_bind(Socket* socket, Sockaddr* addr, socklen_t len);
int sock_listen_req_in(Func_msg* api_msg);
int sock_accept_req_in(Func_msg* api_msg);
int sock_destroy_req_in(Func_msg* api_msg);
#endif