from scapy.all import ARP, sniff

seen: dict[str, str] = {}


def on_packet(packet) -> None:
    arp = packet[ARP]
    if arp.op != 2:
        return

    ip_addr = arp.psrc
    mac_addr = arp.hwsrc.lower()
    if seen.get(ip_addr) == mac_addr:
        return

    seen[ip_addr] = mac_addr
    print(f"{ip_addr}\t{mac_addr}", flush=True)


def main() -> None:
    print("Sniffing ARP replies... Press Ctrl+C to stop.")
    print("IP\t\tMAC")
    sniff(filter="arp", prn=on_packet, store=False)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
