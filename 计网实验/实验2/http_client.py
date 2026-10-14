import socket
import sys
import time
from html.parser import HTMLParser
from urllib.parse import urlparse, urljoin

USER_AGENT = "Zhang Zhaoyuan"


class EmbeddedObjectParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.urls = []

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)

        if tag == "img" and "src" in attrs:
            self.urls.append(attrs["src"])

        elif tag == "script" and "src" in attrs:
            self.urls.append(attrs["src"])

        elif tag == "iframe" and "src" in attrs:
            self.urls.append(attrs["src"])

        elif tag == "source" and "src" in attrs:
            self.urls.append(attrs["src"])

        elif tag == "link":
            rel = attrs.get("rel", "").lower()
            href = attrs.get("href")
            if href and "stylesheet" in rel:
                self.urls.append(href)


def recv_all(sock):
    data = b""
    while True:
        part = sock.recv(4096)
        if not part:
            break
        data += part
    return data


def parse_headers(header_bytes):
    lines = header_bytes.decode("iso-8859-1").split("\r\n")
    status_line = lines[0]
    headers = {}

    for line in lines[1:]:
        if ": " in line:
            k, v = line.split(": ", 1)
            headers[k.lower()] = v

    parts = status_line.split(" ", 2)
    status_code = int(parts[1]) if len(parts) >= 2 else 0
    return status_code, headers


def decode_chunked(body):
    result = b""
    i = 0
    while True:
        j = body.find(b"\r\n", i)
        size_line = body[i:j]
        size = int(size_line.decode().split(";", 1)[0], 16)
        if size == 0:
            break
        i = j + 2
        result += body[i:i + size]
        i += size + 2
    return result


def fetch_once(url):
    parsed = urlparse(url)
    host = parsed.hostname
    port = parsed.port or 80
    path = parsed.path if parsed.path else "/"
    if parsed.query:
        path += "?" + parsed.query

    dns_start = time.perf_counter()
    addr_info = socket.getaddrinfo(host, port, socket.AF_INET, socket.SOCK_STREAM)
    dns_end = time.perf_counter()

    ip = addr_info[0][4][0]

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    tcp_start = time.perf_counter()
    sock.connect((ip, port))
    tcp_end = time.perf_counter()

    host_header = host if port == 80 else f"{host}:{port}"
    request = (
        f"GET {path} HTTP/1.1\r\n"
        f"Host: {host_header}\r\n"
        f"User-Agent: {USER_AGENT}\r\n"
        f"Accept-Encoding: identity\r\n"
        f"Connection: close\r\n"
        f"\r\n"
    )

    http_start = time.perf_counter()
    sock.sendall(request.encode())
    raw_response = recv_all(sock)
    http_end = time.perf_counter()

    sock.close()

    header_bytes, body = raw_response.split(b"\r\n\r\n", 1)
    status_code, headers = parse_headers(header_bytes)

    if headers.get("transfer-encoding", "").lower() == "chunked":
        body = decode_chunked(body)

    return {
        "url": url,
        "host": host,
        "ip": ip,
        "status": status_code,
        "headers": headers,
        "body": body,
        "dns_ms": (dns_end - dns_start) * 1000,
        "tcp_ms": (tcp_end - tcp_start) * 1000,
        "http_ms": (http_end - http_start) * 1000,
    }


def fetch_with_redirect(url, max_redirect=3):
    records = []
    current_url = url

    for _ in range(max_redirect + 1):
        result = fetch_once(current_url)
        records.append(result)

        if result["status"] in (301, 302, 303, 307, 308):
            location = result["headers"].get("location")
            if not location:
                break
            next_url = urljoin(current_url, location)
            if not next_url.startswith("http://"):
                break
            current_url = next_url
        else:
            break

    return records


def unique_keep_order(urls):
    seen = set()
    result = []
    for u in urls:
        if u not in seen:
            seen.add(u)
            result.append(u)
    return result


def main():
    target = sys.argv[1]

    if not target.startswith("http://"):
        target = "http://" + target

    all_records = []

    # 下载主页面
    main_records = fetch_with_redirect(target)
    all_records.extend(main_records)
    final_main = main_records[-1]

    html = final_main["body"].decode(errors="ignore")

    # 解析内嵌对象
    parser = EmbeddedObjectParser()
    parser.feed(html)

    object_urls = []
    for u in parser.urls:
        abs_url = urljoin(final_main["url"], u)
        if abs_url.startswith("http://"):
            object_urls.append(abs_url)

    object_urls = unique_keep_order(object_urls)

    # 下载所有内嵌对象
    for obj_url in object_urls:
        obj_records = fetch_with_redirect(obj_url)
        all_records.extend(obj_records)

    # 输出结果
    print("===== Download Result =====")
    print(f"Main Page: {final_main['url']}")
    print(f"Embedded Object Count: {len(object_urls)}")
    print()

    total_dns = 0.0
    total_tcp = 0.0
    total_http = 0.0

    for i, r in enumerate(all_records, 1):
        total_dns += r["dns_ms"]
        total_tcp += r["tcp_ms"]
        total_http += r["http_ms"]

        print(f"[{i}] {r['url']}")
        print(f"    IP: {r['ip']}")
        print(f"    Status: {r['status']}")
        print(f"    DNS time: {r['dns_ms']:.3f} ms")
        print(f"    TCP connect time: {r['tcp_ms']:.3f} ms")
        print(f"    Full HTTP response time: {r['http_ms']:.3f} ms")
        print()

    print("===== Total =====")
    print(f"Total DNS time: {total_dns:.3f} ms")
    print(f"Total TCP connect time: {total_tcp:.3f} ms")
    print(f"Total Full HTTP response time: {total_http:.3f} ms")


if __name__ == "__main__":
    main()