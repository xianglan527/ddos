import socket
import signal
import sys
import time

TCP_IP   = "192.168.0.27"
TCP_PORT = 5050

server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
# 缩小 recv 缓冲区，更容易填满
# server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 2048)
server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096 * 2)
server_socket.bind((TCP_IP, TCP_PORT))
server_socket.listen(5)
print(f"TCP 服务器正在监听 {TCP_IP}:{TCP_PORT}…")

total_data_once = 0
total_data = 0
current_client = None

def handle_sigint(signum, frame):
    print("\n捕获到 Ctrl+C，正在关闭…")
    if current_client:
        current_client.close()
    server_socket.close()
    sys.exit(0)

signal.signal(signal.SIGINT, handle_sigint)

while True:
    client_socket, addr = server_socket.accept()
    current_client = client_socket
    print(f"接收到来自 {addr} 的连接")

    while True:
        data = client_socket.recv(256)
        if not data:
            print(f"{addr} 已断开")
            client_socket.close()
            break

        total_data_once += len(data)
        total_data += len(data)
        print(f"总共累计接收 {total_data} 单次累计接收 {total_data_once} bytes: {data!r}")

        try:
            client_socket.sendall(data)
            print(f"已回传 {len(data)} bytes 给 {addr}")
        except BrokenPipeError:
            print(f"无法回传，客户端 {addr} 已关闭连接")
            client_socket.close()
            break

        # 累计到 2KB，就模拟不读数据 1 秒，触发对端零窗口探测
        if total_data_once >= 2048:
            print(">>> 模拟零窗口：暂停读取 1 秒 <<<")
            time.sleep(1)
            print(">>> 模拟结束，继续读取 <<<")
            total_data_once = 0

        # 客户端发 "quit" 时关闭连接
        if data.strip() == b"quit":
            print(f"收到 'quit'，断开 {addr}")
            client_socket.close()
            break
