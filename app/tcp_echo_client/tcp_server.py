import socket
import time

TCP_IP = "192.168.8.27"
# TCP_IP = "127.0.0.1"
TCP_PORT = 5050

# 创建 TCP 套接字
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# 绑定到指定 IP 和端口
server_socket.bind((TCP_IP, TCP_PORT))

# 开始监听，最大排队连接数设为 5
server_socket.listen(5)

print(f"TCP 服务器正在监听 {TCP_IP}:{TCP_PORT}...")

# 用于统计接收到的数据总大小
total_data_received = 0

while True:
    # 阻塞等待，直到有客户端连接进来
    client_socket, addr = server_socket.accept()
    print(f"接收到来自 {addr} 的连接")
    
    # 不断接收客户端发送的数据，直到对方关闭连接
    while True:
        data = client_socket.recv(1024)
        if not data:
            # 如果收到空数据，表示客户端已关闭连接
            print(f"客户端 {addr} 已断开连接")
            client_socket.close()
            break

        # 累加接收到的数据的大小
        total_data_received += len(data)

        print(f"接收到来自 {addr} 的数据: {data}")
        print(f"当前接收到的数据总大小: {total_data_received} 字节")

        # 如果接收到 "quit" 字符串，则关闭连接
        if data.decode('utf-8') == "quit":
            print(f"收到 'quit'，关闭连接 {addr}")
            client_socket.close()
            break

        # 回显数据给客户端
        # client_socket.sendall(data)
        # print(f"已回传数据到 {addr}")
