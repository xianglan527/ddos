import socket

UDP_IP = "192.168.8.27"
# UDP_IP = "127.0.0.1"
UDP_PORT = 5050

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

sock.bind((UDP_IP, UDP_PORT))

print(f"服务器正在监听 {UDP_IP}:{UDP_PORT}...")

while True:
    # 接收数据
    data, addr = sock.recvfrom(1024)  # buffer size is 1024 bytes
    print(f"接收到来自 {addr} 的数据: {data}")

    # 将数据回传给客户端
    sock.sendto(data, addr)
    print(f"已回传数据到 {addr}")
