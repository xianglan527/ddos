import socket
import time

TCP_IP = "192.168.8.27"
# TCP_IP = "127.0.0.1"
TCP_PORT = 5050

# 创建 TCP 套接字
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 0)

# 绑定到指定 IP 和端口
server_socket.bind((TCP_IP, TCP_PORT))
# 开始监听，最大排队连接数设为 5
server_socket.listen(5)

print(f"TCP 服务器正在监听 {TCP_IP}:{TCP_PORT}...")

while True:
    # 阻塞等待，直到有客户端连接进来
    client_socket, addr = server_socket.accept()
    print(f"接收到来自 {addr} 的连接")

    time.sleep(2)
    # client_socket.shutdown(socket.SHUT_RDWR)  # 发送 FIN 包
    client_socket.close()  # 彻底关闭
    print(f"已主动关闭与 {addr} 的连接")
    break
    # 不断接收客户端发送的数据，直到对方关闭连接
    # while True:
    #     data = client_socket.recv(1024)
    #     if not data:
    #         # 如果收到空数据，表示客户端已关闭连接
    #         print(f"客户端 {addr} 已断开连接")
    #         client_socket.close()
    #         break

    #     print(f"接收到来自 {addr} 的数据: {data}")

    #     # 回显数据给客户端
    #     client_socket.sendall(data)
    #     print(f"已回传数据到 {addr}")

server_socket.close()
print("服务器已关闭。")