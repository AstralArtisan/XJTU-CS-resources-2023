from scapy.all import get_if_list

for i in get_if_list():
    print(i)