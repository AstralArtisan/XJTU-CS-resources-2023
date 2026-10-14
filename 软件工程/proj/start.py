"""一键启动前后端服务并打开浏览器。

用法：在项目根目录执行
    python start.py
"""

import atexit
import os
import sys
import time
import threading
import webbrowser
from http.server import SimpleHTTPRequestHandler, HTTPServer

BACKEND_HOST = "127.0.0.1"
BACKEND_PORT = 8000
FRONTEND_HOST = "127.0.0.1"
FRONTEND_PORT = 5173
FRONTEND_DIR = os.path.join(os.path.dirname(__file__), "src", "frontend")

_servers = []


def _shutdown_all():
    for srv in _servers:
        try:
            srv.shutdown()
        except Exception:
            pass


atexit.register(_shutdown_all)


def start_backend():
    root = os.path.dirname(__file__)
    src_dir = os.path.join(root, "src")
    if src_dir not in sys.path:
        sys.path.insert(0, src_dir)

    from backend.database.db import init_db
    from backend.main import AppHandler
    from http.server import ThreadingHTTPServer

    init_db()
    server = ThreadingHTTPServer((BACKEND_HOST, BACKEND_PORT), AppHandler)
    _servers.append(server)
    print(f"[后端] http://{BACKEND_HOST}:{BACKEND_PORT}")
    server.serve_forever()


def start_frontend():
    os.chdir(FRONTEND_DIR)
    server = HTTPServer((FRONTEND_HOST, FRONTEND_PORT), SimpleHTTPRequestHandler)
    _servers.append(server)
    print(f"[前端] http://{FRONTEND_HOST}:{FRONTEND_PORT}")
    server.serve_forever()


def main():
    print("=" * 44)
    print("  西交食堂评价系统 — 一键启动")
    print("=" * 44)

    t_back = threading.Thread(target=start_backend, daemon=True)
    t_front = threading.Thread(target=start_frontend, daemon=True)

    t_back.start()
    t_front.start()

    time.sleep(1.5)
    url = f"http://{FRONTEND_HOST}:{FRONTEND_PORT}"
    print(f"\n正在打开浏览器: {url}\n按 Ctrl+C 停止服务。\n")
    webbrowser.open(url)

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n已停止。")
        os._exit(0)


if __name__ == "__main__":
    main()
