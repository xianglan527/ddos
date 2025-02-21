#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * @brief TCP 回显客户端程序
 *
 * @param ip   服务器 IP
 * @param port 服务器端口
 * @return 成功返回 0，失败返回 -1
 */
int tcp_echo_client_start(const char* ip, int port) {
    printf("TCP echo client, ip: %s, port: %d\n", ip, port);
    printf("Enter quit to exit\n");

    // 创建套接字，使用流式传输，即 TCP
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("socket creation failed");
        return -1;
    }

    // 设置服务器地址和端口
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);  // 转换 IP 字符串

    // 连接到服务器
    if (connect(s, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect error");
        close(s);
        return -1;
    }

    // 进入循环，读取用户输入并发送至服务器，然后接收回显
    char buf[128];
    printf(">>");
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        // 如果输入 "quit" 则退出
        if (strncmp(buf, "quit", 4) == 0) { break; }

        // 发送数据到服务器（包含换行在内）
        size_t total_len = strlen(buf);
        if (send(s, buf, total_len, 0) < 0) {
            perror("send error");
            close(s);
            return -1;
        }

        // 接收服务器回显数据
        memset(buf, 0, sizeof(buf));
        ssize_t size = recv(s, buf, sizeof(buf) - 1, 0);
        if (size < 0) {
            perror("recv error");
            close(s);
            return -1;
        } else if (size == 0) {
            // 服务器关闭连接
            printf("Server closed the connection.\n");
            break;
        }
        buf[size] = '\0';  // 确保字符串以 '\0' 结束

        // 显示接收到的回显数据
        printf("%s", buf);
        printf(">>");
    }

    // 关闭套接字
    close(s);
    return 0;
}

/**
 * @brief 主函数
 */
int main(int argc, char* argv[]) {
    // 默认的服务器 IP 和端口
    const char* default_ip = "192.168.8.27";
    int default_port = 5050;

    // 如果命令行参数不够，输出使用帮助
    if (argc < 2) {
        printf("Usage: %s <server_ip> [port]\n", argv[0]);
        printf("Default IP: %s, Default Port: %d\n", default_ip, default_port);
        // 没有指定时，就会使用默认地址和端口继续执行
    }

    // 获取 IP 地址，若未提供则使用默认值
    const char* ip = (argc > 1) ? argv[1] : default_ip;

    // 获取端口号，若未提供则使用默认值
    int port = default_port;
    if (argc > 2) {
        port = atoi(argv[2]);
        if (port <= 0 || port > 65535) {
            printf("Invalid port number. Port must be between 1 and 65535.\n");
            return -1;
        }
    }

    // 启动 TCP echo 客户端
    return tcp_echo_client_start(ip, port);
}
