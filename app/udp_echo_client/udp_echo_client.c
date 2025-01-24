#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "udp_echo_client.h"


int udp_echo_client_start(const char* ip, int port) {
    printf("udp echo client, ip: %s, port: %d\n", ip, port);
    printf("Enter quit to exit\n");

    // 创建套接字，使用UDP协议
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);  
    if (s < 0) {
        printf("open socket error\n");
        return -1;
    }

    // 设置服务器地址和端口
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip); 
    server_addr.sin_port = htons(port);           // 端口号，注意大小端转换

    connect(s, (const struct sockaddr*)&server_addr, sizeof(server_addr));
    // 循环，读取用户输入并发送到服务器
    printf(">>");
    char buf[128];
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        // 如果输入 "quit" 则退出
        if (strncmp(buf, "quit", 4) == 0) { break; }

        // 发送数据到服务器
        size_t total_len = strlen(buf);
        ssize_t size = send(s, buf, total_len, 0);
        // ssize_t size = sendto(s, buf, total_len, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
        if (size < 0) {
            printf("send error\n");
            close(s);
            return -1;
        }

        // 接收回显数据
        memset(buf, 0, sizeof(buf));
        // struct sockaddr_in remote_addr;
        // socklen_t addr_len = sizeof(remote_addr);
        // size = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&remote_addr, &addr_len);
        size = recv(s, buf, sizeof(buf), 0);
        if (size < 0) {
            printf("recv error\n");
            close(s);
            return -1;
        }
        buf[sizeof(buf) - 1] = '\0';  // 确保字符串以 '\0' 结束

        // 显示接收到的回显数据
        printf("%s", buf);
        printf(">>");
    }

    // 关闭套接字
    close(s);
    return 0;
}

// 主函数
int main(int argc, char* argv[]) {
    // 默认的服务器 IP 和端口
    // const char* default_ip = "127.0.0.1";
    const char* default_ip = "192.168.8.27";
    int default_port = 5050;

    // 如果命令行参数不够，输出使用帮助
    if (argc < 2) {
        printf("Usage: %s <server_ip> [port]\n", argv[0]);
        printf("Default IP: %s, Default Port: %d\n", default_ip, default_port);
        // return -1;
    }
    const char* ip = NULL;
    if (argc > 1) { ip = argv[1]; }
    ip = default_ip;

    // 获取 IP 地址，若未提供则使用默认值
    // const char* ip = argv[1];
    int port = default_port;  // 默认端口号

    // 如果提供了端口号，解析并赋值
    if (argc > 2) {
        port = atoi(argv[2]);
        if (port <= 0 || port > 65535) {
            printf("Invalid port number. Port must be between 1 and 65535.\n");
            return -1;
        }
    }

    // 调用 UDP echo 客户端函数开始通信
    return udp_echo_client_start(ip, port);
}
