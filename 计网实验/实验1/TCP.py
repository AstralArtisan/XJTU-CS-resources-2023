from __future__ import annotations

import argparse
import socket
import sys
from datetime import datetime
from pathlib import Path
from typing import TextIO


DEFAULT_CLIENT_HOST = "202.117.10.144"
DEFAULT_CLIENT_PORT = 8888
DEFAULT_SERVER_BIND = "0.0.0.0"
DEFAULT_SERVER_PORT = 9000
DEFAULT_TIMEOUT = 10.0
MIN_REQUIRED_LINES = 5
BUFFER_SIZE = 4096
BASE_DIR = Path(__file__).resolve().parent
LOG_DIR = BASE_DIR / "logs"


class SessionLogger:
    def __init__(self, role: str) -> None:
        LOG_DIR.mkdir(exist_ok=True)
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.path = LOG_DIR / f"{role}_{timestamp}.txt"
        self._file: TextIO = self.path.open("a", encoding="utf-8")

    def log(self, message: str) -> None:
        time_text = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        self._file.write(f"[{time_text}] {message}\n")
        self._file.flush()

    def close(self) -> None:
        if not self._file.closed:
            self._file.close()


def validate_port(value: str) -> int:
    try:
        port = int(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("端口必须是整数。") from exc
    if not 1 <= port <= 65535:
        raise argparse.ArgumentTypeError("端口必须在 1 到 65535 之间。")
    return port


def validate_timeout(value: str) -> float:
    try:
        timeout = float(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("超时时间必须是数字。") from exc
    if timeout <= 0:
        raise argparse.ArgumentTypeError("超时时间必须大于 0。")
    return timeout


def print_and_log(logger: SessionLogger, message: str) -> None:
    print(message)
    logger.log(message)


def format_socket_error(exc: OSError) -> str:
    if isinstance(exc, socket.timeout):
        return "连接或接收数据超时。"
    if isinstance(exc, socket.gaierror):
        return "无法解析主机地址，请检查主机名或 IP。"
    if isinstance(exc, ConnectionRefusedError):
        return "连接被拒绝，请确认目标服务器已启动且端口正确。"
    if isinstance(exc, ConnectionResetError):
        return "连接被对方重置。"
    if exc.errno == 10060:
        return "连接超时，目标主机没有及时响应。"
    if exc.errno == 10061:
        return "目标端口未开放或服务未启动。"
    if exc.errno == 10065:
        return "无法到达目标主机，请检查网络连接。"
    return f"网络错误：{exc}"


def decode_line(raw_line: bytes) -> str:
    if raw_line.endswith(b"\r"):
        raw_line = raw_line[:-1]
    return raw_line.decode("utf-8")


def receive_line(sock: socket.socket, buffer: bytes) -> tuple[str | None, bytes]:
    while True:
        if b"\n" in buffer:
            raw_line, _, buffer = buffer.partition(b"\n")
            return decode_line(raw_line), buffer

        chunk = sock.recv(BUFFER_SIZE)
        if not chunk:
            if buffer:
                return decode_line(buffer), b""
            return None, b""
        buffer += chunk


def handle_client_session(
    client_sock: socket.socket,
    client_address: tuple[str, int],
    logger: SessionLogger,
) -> None:
    buffer = b""
    while True:
        try:
            chunk = client_sock.recv(BUFFER_SIZE)
        except socket.timeout:
            print_and_log(logger, f"客户端 {client_address} 长时间无数据，连接关闭。")
            return
        except ConnectionResetError:
            print_and_log(logger, f"客户端 {client_address} 异常断开。")
            return

        if not chunk:
            if buffer:
                try:
                    message = decode_line(buffer)
                except UnicodeDecodeError:
                    print_and_log(logger, f"客户端 {client_address} 发送了无法解码的残留数据。")
                    return
                print_and_log(logger, f"from {client_address}: {message}")
            print_and_log(logger, f"客户端 {client_address} 已断开连接。")
            return

        buffer += chunk
        while b"\n" in buffer:
            raw_line, _, buffer = buffer.partition(b"\n")
            try:
                message = decode_line(raw_line)
            except UnicodeDecodeError:
                print_and_log(logger, f"客户端 {client_address} 发送的消息不是 UTF-8，连接关闭。")
                return

            print_and_log(logger, f"from {client_address}: {message}")
            try:
                client_sock.sendall(raw_line + b"\n")
            except OSError as exc:
                print_and_log(logger, f"向客户端 {client_address} 回传消息失败：{format_socket_error(exc)}")
                return


def run_server(args: argparse.Namespace) -> int:
    logger = SessionLogger("server")
    print_and_log(logger, f"日志文件：{logger.path}")

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_sock:
            server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server_sock.bind((args.bind, args.port))
            server_sock.listen()
            print_and_log(logger, f"TCP 服务器已启动，监听地址：{args.bind}:{args.port}")
            print_and_log(logger, f"可在另一个终端执行：netstat -ano | findstr :{args.port}")

            while True:
                try:
                    client_sock, client_address = server_sock.accept()
                except KeyboardInterrupt:
                    print_and_log(logger, "收到 Ctrl+C，服务器停止监听。")
                    return 0

                with client_sock:
                    client_sock.settimeout(args.timeout)
                    print_and_log(logger, f"客户端已连接：{client_address}")
                    handle_client_session(client_sock, client_address, logger)
    except OSError as exc:
        print_and_log(logger, f"服务器启动失败：{format_socket_error(exc)}")
        return 1
    finally:
        logger.close()


def run_client(args: argparse.Namespace) -> int:
    logger = SessionLogger("client")
    print_and_log(logger, f"日志文件：{logger.path}")
    print_and_log(logger, f"准备连接 TCP 服务：{args.host}:{args.port}")
    print_and_log(logger, f"可在另一个终端执行：netstat -ano | findstr :{args.port}")

    buffer = b""
    line_count = 0

    try:
        with socket.create_connection((args.host, args.port), timeout=args.timeout) as client_sock:
            client_sock.settimeout(args.timeout)
            local_ip, local_port = client_sock.getsockname()
            remote_ip, remote_port = client_sock.getpeername()
            print_and_log(
                logger,
                f"连接成功：本地 {local_ip}:{local_port} -> 远端 {remote_ip}:{remote_port}",
            )
            print_and_log(logger, "请输入至少 5 行内容。第 1 行请填写：姓名，学号，班级。")

            while True:
                prompt = "第1行（姓名，学号，班级）> " if line_count == 0 else f"第{line_count + 1}行> "
                try:
                    text = input(prompt)
                except EOFError:
                    text = ""

                if not text.strip():
                    if line_count < MIN_REQUIRED_LINES:
                        remaining = MIN_REQUIRED_LINES - line_count
                        print_and_log(logger, f"至少还需要输入 {remaining} 行，当前不能结束。")
                        continue
                    print_and_log(logger, "输入结束，客户端准备断开连接。")
                    return 0

                try:
                    payload = f"{text}\n".encode("utf-8")
                except UnicodeEncodeError:
                    print_and_log(logger, "消息编码失败，请重新输入。")
                    continue

                client_sock.sendall(payload)
                line_count += 1
                print_and_log(logger, f"发送: {text}")

                response, buffer = receive_line(client_sock, buffer)
                if response is None:
                    print_and_log(logger, "服务器已关闭连接，未收到完整响应。")
                    return 1

                print_and_log(logger, f"接收: {response}")
    except KeyboardInterrupt:
        print_and_log(logger, "收到 Ctrl+C，客户端主动结束。")
        return 0
    except UnicodeDecodeError:
        print_and_log(logger, "接收到的响应不是合法的 UTF-8 文本。")
        return 1
    except OSError as exc:
        print_and_log(logger, f"客户端运行失败：{format_socket_error(exc)}")
        return 1
    finally:
        logger.close()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="TCP 客户端/服务端实验程序")
    subparsers = parser.add_subparsers(dest="mode", required=True)

    client_parser = subparsers.add_parser("client", help="连接远端 TCP 服务并逐行交互")
    client_parser.add_argument("--host", default=DEFAULT_CLIENT_HOST, help="远端服务器地址")
    client_parser.add_argument(
        "--port",
        default=DEFAULT_CLIENT_PORT,
        type=validate_port,
        help="远端服务器端口",
    )
    client_parser.add_argument(
        "--timeout",
        default=DEFAULT_TIMEOUT,
        type=validate_timeout,
        help="连接和接收超时时间（秒）",
    )
    client_parser.set_defaults(func=run_client)

    server_parser = subparsers.add_parser("server", help="启动本地 TCP 回显服务器")
    server_parser.add_argument("--bind", default=DEFAULT_SERVER_BIND, help="服务器监听地址")
    server_parser.add_argument(
        "--port",
        default=DEFAULT_SERVER_PORT,
        type=validate_port,
        help="服务器监听端口",
    )
    server_parser.add_argument(
        "--timeout",
        default=DEFAULT_TIMEOUT,
        type=validate_timeout,
        help="客户端空闲超时时间（秒）",
    )
    server_parser.set_defaults(func=run_server)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
