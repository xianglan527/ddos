import socket
import time
import signal
import sys

TCP_IP = "192.168.0.27"
TCP_PORT = 5050

# 创建 TCP 套接字
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
# 绑定到指定 IP 和端口
server_socket.bind((TCP_IP, TCP_PORT))
# 开始监听，最大排队连接数设为 5
server_socket.listen(5)

print(f"TCP 服务器正在监听 {TCP_IP}:{TCP_PORT}...")

def 构造4KB字符串():
    """构造一个4KB的字符串，由 'abcdefghijklmnopqrstuvwxyz' 循环填充"""
    base_str = "abcdefghijklmnopqrstuvwxyz"
    result = ""
    while len(result) < 4096:
        result += base_str
    return result[:4096]  # 截取前 4096 个字符，确保大小为 4KB

# 构造发送的消息内容（4KB）
message = 构造4KB字符串()
msg_bytes = message.encode('utf-8')
chunk_size = 128  # 每次发送 128 字节

current_client = None

def handle_sigint(signum, frame):
    print("\n捕获到 Ctrl+C，正在关闭…")
    if current_client:
        current_client.close()
    server_socket.close()
    sys.exit(0)

signal.signal(signal.SIGINT, handle_sigint)

while True:
    # 阻塞等待客户端连接
    client_socket, addr = server_socket.accept()
    current_client = client_socket
    print(f"接收到来自 {addr} 的连接")

    # 禁用 Nagle 算法，避免小包合并（可选）
    client_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    # 发送 4 次，每次发送 4096 字节（每次分片为 128 字节发送）
    for i in range(4):
        print(f"开始第 {i+1} 次数据发送")
        for j in range(0, len(msg_bytes), chunk_size):
            chunk = msg_bytes[j:j+chunk_size]
            client_socket.sendall(chunk)
            print(f"  已发送第 {j // chunk_size + 1} 个 128 字节块")

    client_socket.close()
    print(f"与 {addr} 的连接已关闭")
