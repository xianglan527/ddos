#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int tcp_echo_server_start(int port) {
    // 创建 TCP 套接字
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("socket creation failed");
        return -1;
    }

    // 绑定本地地址和端口
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // 监听所有网卡
    server_addr.sin_port = htons(port);        // 端口号，注意大小端转换

    if (bind(s, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind error");
        close(s);
        return -1;
    }

    // 开始监听
    if (listen(s, 5) < 0) {
        perror("listen error");
        close(s);
        return -1;
    }

    printf("TCP echo server started, listening on port %d\n", port);

    while (1) {
        // 等待客户端连接
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client = accept(s, (struct sockaddr*)&client_addr, &addr_len);
        if (client < 0) {
            perror("accept error");
            break;  // 发生错误时跳出循环
        }

        // 打印客户端IP和端口
        printf("TCP echo server: connected IP: %s, Port: %d\n", inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        // 接收并回显数据
        char buf[256];
        ssize_t size;
        while ((size = recv(client, buf, sizeof(buf), 0)) > 0) {
            // 当收到 "quit" 时，退出循环（可选功能，看实际需求）
            if (strncmp(buf, "quit", 4) == 0) {
                printf("Received 'quit' command, closing this connection...\n");
                break;
            }
            // 回显给客户端
            ssize_t sent = send(client, buf, size, 0);
            if (sent < 0) {
                perror("send error");
                break;
            }
        }
        close(client);
    }

    // 关闭服务器套接字
    close(s);
    return 0;
}

int main(int argc, char* argv[]) {
    // 默认端口
    int default_port = 5050;

    // 如果命令行参数不够，输出使用帮助
    if (argc < 2) {
        printf("Usage: %s [port]\n", argv[0]);
        printf("Default Port: %d\n", default_port);
    }

    // 根据输入设置端口
    int port = default_port;
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
        perror("fork failed");
        return -1;
    } else if (pid == 0) {
        // 子进程：执行 TCP 回显服务器
        printf("Child process started for TCP server...\n");
        return tcp_echo_server_start(port);
    } else {
        // 父进程：等待子进程结束
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            printf("Child process terminated with exit code %d\n", WEXITSTATUS(status));
        }
    }

    return 0;
}
