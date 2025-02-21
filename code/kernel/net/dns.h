#ifndef __NET_DNS_H__
#define __NET_DNS_H__
#include "error.h"
#include "exmsg.h"
#include "list.h"
#include "net.h"
#include "net_config.h"
#include "netif.h"
#include "sem.h"
#include "spinlock.h"
#include "types.h"
#include "udp.h"

// DNS error types
#define DNS_ERR_NONE 0       // No error
#define DNS_ERR_FORMAT 1     // Format error, the query cannot be interpreted
#define DNS_ERR_SERV_FAIL 2  // Server failure, processing error on the server
#define DNS_ERR_NXMOMAIN 3   // Nonexistent domain, referenced an unknown domain
#define DNS_ERR_NOTIMP 4     // Not implemented, the request is not supported on the server
#define DNS_ERR_REFUSED 5    // Refused: the server does not want to provide an answer

#pragma pack(1)

typedef struct dns_hdr {
    uint16_t id;  // Transaction ID
    union {
        uint16_t all;
#if NET_ENDIAN_LITTLE
        struct {
            uint16_t rcode : 4;   // Response code
            uint16_t cd : 1;      // Disable security checks (1)
            uint16_t ad : 1;      // Information is authorized (1)
            uint16_t z : 1;       // Reserved, must be 0
            uint16_t ra : 1;      // Recursion available (1)
            uint16_t rd : 1;      // Recursion desired (1), 0 allows iterative query
            uint16_t tc : 1;      // Truncation (1), used when UDP exceeds 512 bytes, returning only 512 bytes
            uint16_t aa : 1;      // Authoritative answer
            uint16_t opcode : 4;  // Operation code (default 0)
            uint16_t qr : 1;
        };
#else
        struct {
            uint16_t qr : 1;
            uint16_t opcode : 4;  // Operation code (default 0)
            uint16_t aa : 1;      // Authoritative answer
            uint16_t tc : 1;      // Truncation (1), used when UDP exceeds 512 bytes, returning only 512 bytes
            uint16_t rd : 1;      // Recursion desired (1), 0 allows iterative query
            uint16_t ra : 1;      // Recursion available (1)
            uint16_t z : 1;       // Reserved, must be 0
            uint16_t ad : 1;      // Information is authorized (1)
            uint16_t cd : 1;      // Disable security checks (1)
            uint16_t rcode : 4;   // Response code
        };
#endif
    } flags;
    uint16_t qdcount;  // Number of questions/sections
    uint16_t ancount;  // Number of answers/records
    uint16_t nscount;  // Number of authority records/updates
    uint16_t arcount;  // Number of additional records
} Dns_hdr;

#define DNS_QUERY_CLASS_INET 1  

#define DNS_QUERY_TYPE_A 1  

typedef struct dns_qfield {
    uint16_t type;   
    uint16_t class; 
} Dns_qfield;

typedef struct dns_afield {
    uint16_t type;     
    uint16_t class;   
    uint32_t ttl;       
    uint16_t rd_len;    
    uint16_t rdata[1]; 
} Dns_afield;

#pragma pack()

typedef struct dns_req {
    char domain_name[DNS_DOMAIN_NAME_MAX]; 
    int ret;
    Ipaddr ipaddr;    
    Sem wait_sem;
    uint16_t query_id;
    uint32_t retry_tmo;  
    uint32_t retry_cnt;  
    List_entry dns_req_link;
} Dns_req;

#define le2dns_req(le) to_struct((le), Dns_req, dns_req_link)

typedef struct dns_entry {
    Ipaddr ipaddr;
    int ttl;
    char domain_name[DNS_DOMAIN_NAME_MAX];
    List_entry dns_entry_link;
} Dns_entry;

#define le2dns_entry(le) to_struct((le), Dns_entry, dns_entry_link)

Dns_req* dns_alloc_req(void);
void dns_free_req(Dns_req* req);
int dns_req_in(Func_msg* msg);
void dnss_init(void);
void dns_in(void);
int dns_is_arrive(Udp* udp);
#endif