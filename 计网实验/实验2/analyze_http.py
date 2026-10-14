import os
import subprocess
import sys

# 如果 tshark 不在 PATH 中，把这里改成完整路径
# TSHARK = r"C:\Program Files\Wireshark\tshark.exe"
TSHARK = "tshark"

# ====== 按你的实际文件名修改这里 ======
TARGETS = [
    {
        "name": "国内",
        "pcap": "hit2.pcapng",
        "domain": "www.hit.edu.cn",
        "root_domain": "hit.edu.cn",
        "server_ip": "202.118.254.135",
    },
    {
        "name": "国外",
        "pcap": "customs2.pcapng",
        "domain": "www.customs.gov.mm",
        "root_domain": "customs.gov.mm",
        "server_ip": "150.95.82.190",
    },
    {
        "name": "南半球",
        "pcap": "ucv2.pcapng",
        "domain": "www.ucv.ve",
        "root_domain": "ucv.ve",
        "server_ip": "190.169.30.8",
    },
]
# ======================================


def run_tshark(pcap, display_filter, fields):
    cmd = [TSHARK, "-r", pcap, "-Y", display_filter, "-T", "fields"]
    for f in fields:
        cmd += ["-e", f]
    cmd += ["-E", "separator=\t", "-E", "quote=n", "-E", "occurrence=f"]

    try:
        out = subprocess.check_output(
            cmd, text=True, encoding="utf-8", errors="ignore"
        )
    except FileNotFoundError:
        print("错误：找不到 tshark，请把 TSHARK 改成 tshark.exe 的完整路径。")
        sys.exit(1)
    except subprocess.CalledProcessError as e:
        print(f"tshark 执行失败：{e}")
        return []

    rows = []
    for line in out.splitlines():
        parts = line.split("\t")
        if len(parts) < len(fields):
            parts += [""] * (len(fields) - len(parts))
        rows.append(parts)
    return rows


def safe_float(x):
    try:
        return float(x)
    except:
        return None


def to_ms(seconds):
    return round(seconds * 1000, 3)


def analyze_target(target):
    pcap = target["pcap"]
    domain = target["domain"]
    root_domain = target["root_domain"]
    server_ip = target["server_ip"]

    result = {
        "domain": domain,
        "server_ip": server_ip,
        "rtt_ms": None,
        "dns_count": 0,
        "page_load_ms": None,
        "request_object_count": 0,
        "tcp_conn_count": 0,
        "note": "",
    }

    if not os.path.exists(pcap):
        result["note"] = f"抓包文件不存在: {pcap}"
        return result

    # 1) 与该网站相关的 DNS 请求数
    dns_queries_related = run_tshark(
        pcap,
        f'dns.flags.response == 0 and dns.qry.name contains "{root_domain}"',
        ["frame.number", "frame.time_epoch", "dns.id", "dns.qry.name"],
    )
    result["dns_count"] = len(dns_queries_related)

    first_dns_query_time = None
    if dns_queries_related:
        first_dns_query_time = safe_float(dns_queries_related[0][1])

    # 2) 统计 HTTP 连接数：统计到该服务器 80 端口的初始 SYN，对 stream 去重
    syn_rows = run_tshark(
        pcap,
        f'tcp.flags.syn == 1 and tcp.flags.ack == 0 and ip.dst == {server_ip} and tcp.dstport == 80',
        ["frame.number", "frame.time_epoch", "tcp.stream"],
    )

    stream_to_syns = {}
    for row in syn_rows:
        frame_no, t, stream = row
        tt = safe_float(t)
        if stream == "" or tt is None:
            continue
        stream_to_syns.setdefault(stream, []).append(tt)

    result["tcp_conn_count"] = len(stream_to_syns)

    # 3) RTT：对每个 stream，取“最后一个 SYN -> 后续第一个 SYN,ACK”
    #    取第一个能成功算出的 stream 的 RTT
    for stream, syn_times in sorted(stream_to_syns.items(), key=lambda x: min(x[1])):
        syn_times = sorted(syn_times)
        synack_rows = run_tshark(
            pcap,
            f'tcp.flags.syn == 1 and tcp.flags.ack == 1 and ip.src == {server_ip} and tcp.srcport == 80 and tcp.stream == {stream}',
            ["frame.number", "frame.time_epoch", "tcp.stream"],
        )

        synack_times = []
        for row in synack_rows:
            tt = safe_float(row[1])
            if tt is not None:
                synack_times.append(tt)

        if not synack_times:
            continue

        first_synack = min(synack_times)

        # 取 first_synack 之前最后一个 SYN
        candidate_syns = [t for t in syn_times if t <= first_synack]
        if not candidate_syns:
            continue

        last_syn = max(candidate_syns)
        result["rtt_ms"] = to_ms(first_synack - last_syn)
        break

    # 4) 请求对象数：HTTP 请求数
    #    优先按 IP + 80 端口统计
    http_requests = run_tshark(
        pcap,
        f'http.request and ip.dst == {server_ip} and tcp.dstport == 80',
        ["frame.number", "frame.time_epoch", "http.request.method", "http.host", "http.request.uri"],
    )
    result["request_object_count"] = len(http_requests)

    # 5) 页面加载时间
    #    有缓存时可能 DNS 次数为 0，所以起点按以下优先级：
    #    first DNS query -> first HTTP request -> first SYN
    start_time = None
    if first_dns_query_time is not None:
        start_time = first_dns_query_time
    elif http_requests:
        start_time = safe_float(http_requests[0][1])
    elif syn_rows:
        start_time = safe_float(syn_rows[0][1])

    # 终点：与该服务器 80 端口相关的最后一个带数据的 TCP 包
    http_payload_packets = run_tshark(
        pcap,
        f'ip.addr == {server_ip} and tcp.port == 80 and tcp.len > 0',
        ["frame.number", "frame.time_epoch"],
    )

    end_time = None
    if http_payload_packets:
        end_time = safe_float(http_payload_packets[-1][1])

    if start_time is not None and end_time is not None and end_time >= start_time:
        result["page_load_ms"] = to_ms(end_time - start_time)

    # 6) 如果这几个关键值都没有，给出说明
    if result["tcp_conn_count"] == 0 and result["request_object_count"] == 0:
        result["note"] = "未检测到到该 IP:80 的明文 HTTP 访问，可能实际走的是 HTTPS 或未抓到完整流量。"

    return result


def fmt(x):
    return str(x) if x is not None else "未测得"


def main():
    print("====== 带缓存情况下不同网站访问情况统计 ======\n")

    all_results = []
    for target in TARGETS:
        print(f"正在分析：{target['name']} - {target['pcap']}")
        res = analyze_target(target)
        all_results.append((target["name"], res))

    print("\n====== 结果汇总 ======\n")
    for name, res in all_results:
        print(f"--- {name} ---")
        print(f"域名: {res['domain']}")
        print(f"IP 地址: {res['server_ip']}")
        print(f"RTT(ms): {fmt(res['rtt_ms'])}")
        print(f"DNS 次数: {res['dns_count']}")
        print(f"页面加载时间(ms): {fmt(res['page_load_ms'])}")
        print(f"请求对象数: {res['request_object_count']}")
        print(f"TCP 连接数: {res['tcp_conn_count']}")
        if res["note"]:
            print(f"说明: {res['note']}")
        print()

    # 方便你抄表，再输出成一行格式
    print("====== 便于填表的简表 ======\n")
    for name, res in all_results:
        print(
            f"{name}: "
            f"RTT={fmt(res['rtt_ms'])}, "
            f"DNS次数={res['dns_count']}, "
            f"页面加载时间={fmt(res['page_load_ms'])}, "
            f"请求对象数={res['request_object_count']}, "
            f"TCP连接数={res['tcp_conn_count']}"
        )


if __name__ == "__main__":
    main()