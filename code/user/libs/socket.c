#include "socket.h"
#include "printf.h"
#include "error.h"
#include "string.h"
#include "user.h"

uint16_t checksum(void *buf, uint16_t len) {
    uint16_t *curr_buf = (uint16_t *)buf;
    uint32_t sum = 0;
    while (len > 1) {
        sum += *curr_buf++;
        len -= 2;
    }
    if (len > 0) { sum += *(uint8_t *)curr_buf; }
    // Handle carry
    uint16_t high;
    while ((high = sum >> 16) != 0) { sum = high + (sum & 0xFFFF); }
    return (uint16_t)~sum;
}

#define IPV4_STR_SIZE 16

static int ipaddr_from_str(Ipaddr *dest, char *str) {
    if (!dest || !str) { return -E_NET_PARAM; }
    dest->q_addr = 0;
    char c;
    uint8_t *p = dest->a_addr;
    uint8_t sub_addr = 0;
    while ((c = *str++) != '\0') {
        if ((c >= '0') && (c <= '9')) {
            sub_addr = sub_addr * 10 + c - '0';
        } else if (c == '.') {
            *p++ = sub_addr;
            sub_addr = 0;
        } else {
            return -E_NET_PARAM;
        }
    }
    *p++ = sub_addr;
    return NET_OK;
}

char *inet_ntoa(In_addr in) {
    static char buf[IPV4_STR_SIZE];
    memset(buf, 0, IPV4_STR_SIZE);
    snprintf(buf, IPV4_STR_SIZE, "%d.%d.%d.%d", in.addr0, in.addr1, in.addr2, in.addr3);
    return buf;
}

uint32_t inet_addr(char *str) {
    if (!str) { return INADDR_ANY; }
    Ipaddr ipaddr;
    ipaddr_from_str(&ipaddr, str);
    return ipaddr.q_addr;
}

int inet_pton(int family, char *strptr, void *addrptr) {
    if ((family != AF_INET) || !strptr || !addrptr) { return -E_NET_NOT_SUPPORT; }
    In_addr *addr = (In_addr *)addrptr;
    Ipaddr dest;
    ipaddr_from_str(&dest, strptr);
    addr->s_addr = dest.q_addr;
    return NET_OK;
}

char *inet_ntop(int family, void *addrptr, char *strptr, size_t len) {
    if ((family != AF_INET) || !addrptr || !strptr || !len) { return nullptr; }
    In_addr *addr = (In_addr *)addrptr;
    char buf[IPV4_STR_SIZE];
    memset(buf, 0, IPV4_STR_SIZE);
    snprintf(buf, IPV4_STR_SIZE, "%d.%d.%d.%d", addr->addr0, addr->addr1, addr->addr2, addr->addr3);
    strncpy(strptr, buf, len - 1);
    strptr[len - 1] = '\0';
    return strptr;
}

void ping_run(Ping *ping, char *dest, size_t count, size_t size, size_t interval) {
    static uint16_t start_id = PING_DEFAULT_ID;  // Record an initial ID, continuously increment
    char buf[512];
    Hostent hent, *result;
    int err;
    if (gethostbyname_r(dest, &hent, buf, sizeof(buf), &result, &err) < 0) {
        printf("resolve name %s failed\n", dest);
        return;
    }
    int sk = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sk < 0) {
        printf("Socket creation error, please run as root.\n");
        return;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = *(uint32_t *)hent.h_addr_list[0];  // inet_addr(dest);
    inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
    printf("Trying to ping %s [%s]\n", dest, buf);

#define USE_CONNECT
#ifdef USE_CONNECT
    connect(sk, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
#endif

    struct timeval tmo;
    tmo.tv_sec = 5;
    tmo.tv_usec = 0;
    setsockopt(sk, SOL_SOCKET, SO_RCVTIMEO, (char *)&tmo, sizeof(tmo));

    // Fill the data to be sent
    size -= sizeof(clock_t);
    size_t fill_size = (size > PING_BUFFER_SIZE) ? PING_BUFFER_SIZE : size;
    for (size_t i = 0; i < fill_size; i++) { ping->req.buf[i] = (uint8_t)i; }

    // Calculate the total length to send (header + payload)
    size_t total_size = sizeof(IcmpHdr) + fill_size;

    // Loop to send 'count' times
    for (size_t i = 0, seq = 0; i < count; i++, seq++) {
        // Construct ICMP Echo
        ping->req.echo_hdr.id = start_id++;
        ping->req.echo_hdr.seq = (uint16_t)seq;
        ping->req.echo_hdr.type = 8;  // ICMP Echo request
        ping->req.echo_hdr.code = 0;
        ping->req.echo_hdr.checksum = 0;

        // Record send time
        ping->req.time = gettime();

        // Recalculate checksum
        ping->req.echo_hdr.checksum = checksum(&ping->req, (uint16_t)total_size);

        // Send
#ifdef USE_CONNECT
        ssize_t send_len = send(sk, (char *)&ping->req, total_size, 0);
#else
        ssize_t send_len =
            sendto(sk, (char *)&ping->req, total_size, 0, (struct sockaddr *)&addr, sizeof(addr));
#endif
        if (send_len < 0) {
            printf("Failed to send ping request.\n");
            break;
        }

        // Receive reply
        do {
            memset(&ping->reply, 0, sizeof(ping->reply));
#ifdef USE_CONNECT
            ssize_t recv_len = recv(sk, (char *)&ping->reply, sizeof(ping->reply), 0);
#else
            socklen_t addr_len = sizeof(addr);
            ssize_t recv_len = recvfrom(sk, (char *)&ping->reply, sizeof(ping->reply), 0,
                                        (struct sockaddr *)&addr, &addr_len);
#endif
            if (recv_len < 0) {
                printf("Ping reply timeout.\n");
                break;
            }
            // Check if it matches
            if ((ping->req.echo_hdr.id == ping->reply.echo_hdr.id) &&
                (ping->req.echo_hdr.seq == ping->reply.echo_hdr.seq)) {
                // Note to use %zu for size_t
                printf("Received reply, sequence=%lu\n", seq);
                break;
            }
        } while (1);
        if (size > 0) {
            // Compare the received data part, if different the packet is corrupted, skip and continue ping()
            int recv_size = size;
            if (recv_size < (int)(sizeof(IpHdr) + sizeof(IcmpHdr))) {
                printf("Received packet is too small, ignoring.\n");
                continue;
            }
            int data_size = size - sizeof(IpHdr) - sizeof(IcmpHdr) - sizeof(clock_t);
            if (data_size > 0 && memcmp(ping->req.buf, ping->reply.buf, data_size) != 0) {
                printf("Received data is incorrect.\n");
                continue;
            }
            // Display response information
            IpHdr *iphdr = &ping->reply.iphdr;
            int send_size = fill_size + sizeof(clock_t);
            if (recv_size == send_size) {
                printf("Reply from %s: bytes = %d", inet_ntoa(addr.sin_addr), send_size);
            } else {
                printf("Reply from %s: bytes = %d (sent = %d)", inet_ntoa(addr.sin_addr), recv_size,
                       send_size);
            }
            // Calculate time difference (milliseconds)
            int diff_ms = (gettime() - ping->req.time);
            if (diff_ms < 1) {
                printf(", time <1ms, TTL=%d\n", iphdr->ttl);
            } else {
                printf(", time=%dms, TTL=%d\n", diff_ms, iphdr->ttl);
            }
        }
        if (i < count - 1) { sleep(interval); }
    }
    closesocket(sk);
}