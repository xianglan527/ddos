#ifndef __NET_TCP_H__
#define __NET_TCP_H__

#include "error.h"
#include "net_config.h"
#include "nettool.h"
#include "types.h"
#include "list.h"
#include "pktbuf.h"
#include "spinlock.h"
#include "tcp_buf.h"
#include "proc.h"

#define TCP_OPT_END 0  // Option End
#define TCP_OPT_NOP 1  // No-Operation, used for padding
#define TCP_OPT_MSS 2  // Maximum Segment Size

#define TCP_SEQ_LE(a, b) ((int32_t)(a) - (int32_t)(b) <= 0)
#define TCP_SEQ_LT(a, b) ((int32_t)(a) - (int32_t)(b) < 0)

typedef struct socket Socket;

#pragma pack(1)

typedef struct tcp_hdr {
    uint16_t sport;  // Source port
    uint16_t dport;  // Destination port
    uint32_t seq;    // Sequence number
    uint32_t ack;    // Acknowledgment number
    union {
        uint16_t flags;
#if NET_ENDIAN_LITTLE
        struct {
            uint16_t resv : 4;   // Reserved
            uint16_t shdr : 4;   // Header length
            uint16_t f_fin : 1;  // FIN flag: Indicates the sender has finished sending data, terminating the
                                 // connection
            uint16_t f_syn : 1;  // SYN flag: Synchronize sequence numbers to initiate a connection
            uint16_t f_rst : 1;  // RST flag: Reset the connection
            uint16_t f_psh : 1;  // PSH flag: Push function - receiver should pass the data to the application
                                 // as soon as possible
            uint16_t f_ack : 1;  // ACK flag: Acknowledgment field is significant
            uint16_t f_urg : 1;  // URG flag: Urgent pointer field is significant
            uint16_t f_ece : 1;  // ECE flag: ECN Echo - sender received a congestion notification
            uint16_t f_cwr : 1;  // CWR flag: Congestion Window Reduced - sender is limiting its sending rate
        };
#else
        struct {
            uint16_t shdr : 4;   // Header length
            uint16_t resv : 4;   // Reserved
            uint16_t f_cwr : 1;  // CWR flag: Congestion Window Reduced - sender is limiting its sending rate
            uint16_t f_ece : 1;  // ECE flag: ECN Echo - sender received a congestion notification
            uint16_t f_urg : 1;  // URG flag: Urgent pointer field is significant
            uint16_t f_ack : 1;  // ACK flag: Acknowledgment field is significant
            uint16_t f_psh : 1;  // PSH flag: Push function - receiver should pass the data to the application
                                 // as soon as possible
            uint16_t f_rst : 1;  // RST flag: Reset the connection
            uint16_t f_syn : 1;  // SYN flag: Synchronize sequence numbers to initiate a connection
            uint16_t f_fin : 1;  // FIN flag: Indicates the sender has finished sending data, terminating the
                                 // connection
        };
#endif
    };
    uint16_t win;  // Window size, implements flow control. Window scaling option can support larger values
    uint16_t checksum;  // Checksum
    uint16_t urgptr;    // Urgent pointer
} Tcp_hdr;

typedef struct tcp_pkt {
    Tcp_hdr hdr;
    uint8_t data[1];
} Tcp_pkt;

typedef struct tcp_opt_mss {
    uint8_t kind;
    uint8_t length;
    union {
        uint16_t mss;
    };
} Tcp_opt_mss;

#pragma pack()

typedef struct tcp_seg {
    Ipaddr local_ip;   
    Ipaddr remote_ip;  
    Tcp_hdr *hdr;      
    Pktbuf *buf;      
    uint32_t data_len;   
    uint32_t seq;     
    uint32_t seq_len;  
} Tcp_seg;

typedef enum tcp_state {
    TCP_STATE_FREE = 0, 
    TCP_STATE_CLOSED,
    TCP_STATE_LISTEN,
    TCP_STATE_SYN_SENT,
    TCP_STATE_SYN_RECVD,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2,
    TCP_STATE_CLOSING,
    TCP_STATE_TIME_WAIT,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_LAST_ACK,

    TCP_STATE_MAX,
} Tcp_state;

typedef enum tcp_ostate {
    TCP_OSTATE_IDLE,    
    TCP_OSTATE_SENDING,  
    TCP_OSTATE_REXMIT,  

    TCP_OSTATE_MAX,
} Tcp_ostate;

typedef struct tcp Tcp;
struct tcp {
    Tcp *parent;
    struct {
        uint32_t syn_out : 1;
        uint32_t fin_out : 1;
        uint32_t irs_valid : 1;
        uint32_t fin_in : 1;
        uint32_t keep_enable : 1;
        uint32_t inactive : 1;
    } flags;
    Tcp_state state;
    uint16_t mss;
    struct {
        int keep_idle;          
        int keep_intvl;       
        int keep_cnt;      
        int keep_retry;     
        Timer *keep_timer;
        int backlog;
    } conn;
    struct {
        Tcp_buf buf;
        uint32_t una;  // Starting sequence number of the unacknowledged region
        uint32_t nxt;  // Next sequence number to be sent
        uint32_t iss;  // Initial send sequence number
        Timer *snd_timer;
        int rto;
        int rexmit_cnt; 
        int rexmit_max;
        Tcp_ostate ostate; 
    } snd;
    struct {
        Tcp_buf buf;
        uint32_t nxt;  // Next expected sequence number to receive
        uint32_t iss;  // Initial receive sequence number
    } rcv;
};

static inline size_t tcp_hdr_size(Tcp_hdr *hdr) { return hdr->shdr * 4; }

static inline void tcp_set_hdr_size(Tcp_hdr *hdr, size_t size) { hdr->shdr = (uint16_t)(size / 4); }

#if DBG_DISP_ENABLED(DBG_TCP)
void tcp_show_info(char *msg, Socket *socket);
void tcp_display_pkt(char *msg, Tcp_hdr *tcp_hdr, Pktbuf *buf);
void tcp_show_list(void);
#else
#define tcp_show_info(msg, tcp)
#define tcp_display_pkt(msg, hdr, buf)
#define tcp_show_list()
#endif

int tcps_init(void);
Tcp *tcp_find(Ipaddr *local_ip, uint16_t local_port, Ipaddr *remote_ip, uint16_t remote_port);
int tcp_abort(Tcp *tcp, int ret);
void tcp_free(Tcp *tcp);
void tcp_read_options(Tcp *tcp, Tcp_hdr *tcp_hdr);
size_t tcp_rcv_window(Tcp *tcp);
void tcp_keepalive_start(Tcp *tcp, bool run);
void tcp_keepalive_restart(Tcp *tcp);
void tcp_kill_all_timers(Tcp *tcp);
int tcp_backlog_count(Tcp *tcp);
Tcp *tcp_create_child(Tcp *parent, Tcp_seg *seg);
void add_clean_tcp_list(Tcp *tcp);
void do_clean_tcp_list(void);
#endif