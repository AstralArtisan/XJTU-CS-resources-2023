import socket


def main():
    host = "0.0.0.0"
    port = 9999

    try:
        # 创建TCP套接字
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        # 允许端口快速复用，避免程序刚关闭又重启时端口被占用
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        # 绑定IP和端口
        server_socket.bind((host, port))

        # 开始监听
        server_socket.listen(5)
        print(f"服务器正在监听 {host}:{port} ...")
        print("等待客户端连接...")

        while True:
            # 接受客户端连接
            client_socket, client_addr = server_socket.accept()
            print(f"\n有客户端连接成功：IP={client_addr[0]} 端口={client_addr[1]}")

            try:
                while True:
                    # 接收客户端消息
                    data = client_socket.recv(4096)

                    if not data:
                        print("客户端已断开连接。")
                        break

                    message = data.decode("utf-8", errors="ignore")
                    print(f"收到客户端消息：{message}")

                    # 将消息原样返回给客户端
                    client_socket.send(data)

            except Exception as e:
                print("与客户端通信时发生错误：", e)

            finally:
                client_socket.close()
                print("本次客户端连接已关闭，继续等待下一个客户端。")

    except Exception as e:
        print("服务器运行出错：", e)

    finally:
        try:
            server_socket.close()
        except:
            pass


if __name__ == "__main__":
    main()