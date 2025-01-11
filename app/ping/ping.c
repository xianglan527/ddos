#include "ping.h"  // Assume ping.h contains definitions for Ping, IcmpHdr, IpHdr, etc.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Calculate checksum
static uint16_t checksum(void* buf, uint16_t len) {
    uint16_t* curr_buf = (uint16_t*)buf;
    uint32_t sum = 0;
    while (len > 1) {
        sum += *curr_buf++;
        len -= 2;
    }
    if (len > 0) { sum += *(uint8_t*)curr_buf; }
    // Handle carry
    uint16_t high;
    while ((high = sum >> 16) != 0) { sum = high + (sum & 0xFFFF); }
    return (uint16_t)~sum;
}

// Send and receive ping
void ping_run(Ping* ping, const char* dest, size_t count, size_t size, size_t interval) {
    static uint16_t start_id = PING_DEFAULT_ID;  // Record an initial ID, continuously increment
    char buf[512];

    int sk = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sk < 0) {
        printf("Socket creation error, please run as root.\n");
        return;
    }

    // Set destination address
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = inet_addr(dest);

    // Print target information
    inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
    printf("Trying to ping %s [%s]\n", dest, buf);

#define USE_CONNECT
#ifdef USE_CONNECT
    connect(sk, (const struct sockaddr*)&addr, sizeof(struct sockaddr_in));
#endif

    struct timeval tmo;
    tmo.tv_sec = 5;
    tmo.tv_usec = 0;
    setsockopt(sk, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tmo, sizeof(tmo));

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
        ping->req.time = clock();

        // Recalculate checksum
        ping->req.echo_hdr.checksum = checksum(&ping->req, (uint16_t)total_size);

        // Send
#ifdef USE_CONNECT
        ssize_t send_len = send(sk, (const char*)&ping->req, total_size, 0);
#else
        ssize_t send_len =
            sendto(sk, (const char*)&ping->req, total_size, 0, (struct sockaddr*)&addr, sizeof(addr));
#endif
        if (send_len < 0) {
            printf("Failed to send ping request.\n");
            break;
        }

        // Receive reply
        do {
            memset(&ping->reply, 0, sizeof(ping->reply));
#ifdef USE_CONNECT
            ssize_t recv_len = recv(sk, (char*)&ping->reply, sizeof(ping->reply), 0);
#else
            socklen_t addr_len = sizeof(addr);
            ssize_t recv_len =
                recvfrom(sk, (char*)&ping->reply, sizeof(ping->reply), 0, (struct sockaddr*)&addr, &addr_len);
#endif
            if (recv_len < 0) {
                printf("Ping reply timeout.\n");
                break;
            }
            // Check if it matches
            if ((ping->req.echo_hdr.id == ping->reply.echo_hdr.id) &&
                (ping->req.echo_hdr.seq == ping->reply.echo_hdr.seq)) {
                // Note to use %zu for size_t
                printf("Received reply, sequence=%zu\n", seq);
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
            IpHdr* iphdr = &ping->reply.iphdr;
            int send_size = fill_size + sizeof(clock_t);
            if (recv_size == send_size) {
                printf("Reply from %s: bytes = %d", inet_ntoa(addr.sin_addr), send_size);
            } else {
                printf("Reply from %s: bytes = %d (sent = %d)", inet_ntoa(addr.sin_addr), recv_size,
                       send_size);
            }

            // Calculate time difference (milliseconds)
            int diff_ms = (clock() - ping->req.time) * 1000 / CLOCKS_PER_SEC;
            if (diff_ms < 1) {
                printf(", time <1ms, TTL=%d\n", iphdr->ttl);
            } else {
                printf(", time=%dms, TTL=%d\n", diff_ms, iphdr->ttl);
            }
        }

        // Sleep for 'interval' seconds before sending next ping
        if (i < count - 1) {
            // sleep() parameter is unsigned int
            sleep((unsigned int)interval);
        }
    }

    close(sk);
}

/**
 * Example main function
 * Usage:
 *    ./ping_test <destination_ip> [count] [size] [interval]
 * Default number of pings: 4, data size: 56, interval: 1 second
 */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("usage: %s <destination_ip> [count] [size] [interval]\n", argv[0]);
        return -1;
    }

    // Parse arguments
    char* dest = argv[1];
    size_t count = 4;     // Default count
    size_t size = 56;     // Default data size
    size_t interval = 1;  // Default interval

    if(argv > 1) {dest = "127.0.0.1";}
    if (argc > 2) { count = (size_t)atoi(argv[2]); }
    if (argc > 3) { size = (size_t)atoi(argv[3]); }
    if (argc > 4) { interval = (size_t)atoi(argv[4]); }

    // Declare Ping structure to store request and reply data

    if (size > PING_BUFFER_SIZE) {
        printf("The size is too big, should be < %d\n", PING_BUFFER_SIZE);
        return 0;
    }
    Ping ping;
    memset(&ping, 0, sizeof(ping));

    // Call ping_run to execute
    ping_run(&ping, dest, count, size, interval);

    return 0;
}
