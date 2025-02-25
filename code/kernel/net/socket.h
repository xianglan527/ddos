#ifndef __NET_SOCKET_H__
#define __NET_SOCKET_H__

#include "error.h"
#include "net_config.h"
#include "types.h"
#include "nettool.h"
#include "ipaddr.h"
#include "sock.h"

#pragma pack(1)

typedef struct in_addr {
    union {
        struct {
            uint8_t addr0;
            uint8_t addr1;
            uint8_t addr2;
            uint8_t addr3;
        };
        uint8_t addr_array[IPV4_ADDR_SIZE];
        uint32_t s_addr;
    };
}In_addr;

typedef struct sockaddr {
    uint8_t sa_len;
    uint8_t sa_family;
    uint8_t sa_data[14];
} Sockaddr;

typedef struct sockaddr_in {
    uint8_t sin_len;            
    uint8_t sin_family;      
    uint16_t sin_port;          
    In_addr sin_addr;     
    char sin_zero[8];  
}Sockaddr_in;

#pragma pack()

typedef struct hostent {
    char* h_name;     
    char** h_aliases;   
    int h_addrtype;      
    int h_length;       
    char** h_addr_list;  
}Hostent;

#define h_addr h_addr_list[0]

typedef uint32_t in_addr_t;
typedef struct hostent_extra {
    in_addr_t* addr_tbl[2];  
    in_addr_t addr;          
    char name[1];            
} Hostent_extra;

int socket(int family, int type, int protocol);
ssize_t sendto(int sockfd, void* buf, size_t len, int flags, Sockaddr* dest, socklen_t dest_len);
ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags, Sockaddr* dest, socklen_t* dest_len);
int setsockopt(int sockfd, int level, int optname, char* optval, int optlen);
int closesocket(int sockfd);
int connect(int sockfd, Sockaddr *addr, socklen_t len);
ssize_t send(int sockfd, void* buf, size_t len, int flags);
ssize_t recv(int sockfd, void* buf, size_t len, int flags);
int bind(int sockfd, Sockaddr* addr, socklen_t len);
int accept(int sockfd, Sockaddr* addr, socklen_t* len);
int listen(int sockfd, int backlog);
int gethostbyname_r(char* name, Hostent* ret, char* buf, size_t buflen, Hostent** result, int* h_errnop);
#endif