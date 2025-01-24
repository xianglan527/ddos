#include "udp_echo_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>  // 用于等待子进程结束
#include <time.h>
#include <unistd.h>

// 服务器运行的函数
int udp_echo_server_start(int port) {
    // 创建UDP套接字
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        perror("socket creation failed");
        return -1;
    }
    // 连接的服务地址和端口
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port);           // 端口号，注意大小端转换
    // 绑定本地地址和端口
    if (bind(s, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("bind error");
        close(s);  // 关闭套接字
        return 0;
    }
    while (1) {
        struct sockaddr_in client_addr;
        char buf[256];
        // 接受来自客户端的数据包
        socklen_t addr_len = sizeof(client_addr);
        ssize_t size = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&client_addr, &addr_len);
        if (size < 0) {
            perror("recvfrom error");
            break;  // 发生错误时跳出循环
        }
   
        // 打印客户端的IP和端口
        printf("UDP echo server: connected IP: %s, Port: %d\n", inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));
               
        if (strncmp(buf, "quit", 4) == 0) { break; }
        // 将接收到的数据发回客户端
        size = sendto(s, buf, size, 0, (struct sockaddr *)&client_addr, addr_len);
        if (size < 0) {
            perror("sendto error");
            break;  // 发生错误时跳出循环
        }
    }
    // 关闭套接字
    close(s);
    return 0;
}

int main(int argc, char* argv[]) {
    int default_port = 5050;
    // 如果命令行参数不够，输出使用帮助
    if (argc < 2) {
        printf("Usage: %s [port]\n", argv[0]);
        printf("Default Port: %d\n",  default_port);
        // return -1;
    }
    int port = default_port;  // 默认端口号
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            printf("Invalid port number. Port must be between 1 and 65535.\n");
            return -1;
        }
    }
    // 创建子进程
    pid_t pid = fork();

    if (pid < 0) {
        // 如果 fork 失败
        perror("Fork failed");
        return -1;
    } else if (pid == 0) {
        // 子进程执行服务器
        printf("Child process started for UDP server...\n");
        return udp_echo_server_start(port);  // 子进程启动 UDP 服务器
    } else {
        // 父进程继续执行，等待子进程结束
        int status;
        waitpid(pid, &status, 0);  // 等待子进程结束
        if (WIFEXITED(status)) {
            printf("Child process terminated with exit code %d\n", WEXITSTATUS(status));
        }
    }
    return 0;
}
