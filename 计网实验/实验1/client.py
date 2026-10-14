import socket


def main():
    # 用户输入目标IP和端口
    server_ip = input("请输入服务器IP（直接回车默认 202.117.10.144）：").strip()
    if server_ip == "":
        server_ip = "202.117.10.144"

    port_str = input("请输入服务器端口（直接回车默认 8888）：").strip()
    if port_str == "":
        server_port = 8888
    else:
        server_port = int(port_str)

    try:
        # 创建TCP套接字
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        # 连接服务器
        client_socket.connect((server_ip, server_port))
        print(f"已成功连接到服务器 {server_ip}:{server_port}")
        print("请输入要发送的内容，输入 quit 结束。")

        while True:
            message = input("请输入：")

            if message.lower() == "quit":
                print("客户端结束运行。")
                break

            # 发送数据
            client_socket.send(message.encode("utf-8"))

            # 接收服务器返回的数据
            data = client_socket.recv(4096)

            # 如果服务器关闭，recv会收到空字节串
            if not data:
                print("服务器已关闭连接。")
                break

            print("收到服务器回复：", data.decode("utf-8", errors="ignore"))

    except Exception as e:
        print("发生错误：", e)

    finally:
        # 关闭套接字
        try:
            client_socket.close()
        except:
            pass

if __name__ == "__main__":
    main()