import socket
import time

TCP_IP = "192.168.8.27"
TCP_PORT = 5050

# 创建 TCP 套接字
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
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
    # 截取前4096个字符，确保大小为4KB
    return result[:4096]

# 构造发送的消息内容（4KB）
message = 构造4KB字符串()

while True:
    # 阻塞等待，直到有客户端连接进来
    client_socket, addr = server_socket.accept()
    print(f"接收到来自 {addr} 的连接")
    
    # 发送构造的4KB字符串
    for i in range(1):
        client_socket.sendall(message.encode('utf-8'))
        print(f"已向 {addr} 发送第 {i+1} 次数据")
        # 如有需要可以添加延时
        time.sleep(0.1)
    
    client_socket.close()
    print(f"与 {addr} 的连接已关闭")
