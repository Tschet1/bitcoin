#!/usr/bin/env python3

from scapy.all import *
import sys
import socket

packets = rdpcap('block.pcapng')

socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
socket.bind(('localhost', 8081))

port=int(sys.argv[1])

while True:
    data, addr = socket.recvfrom(1024) # buffer size is 1024 bytes
    num=int.from_bytes(data, byteorder="little")
    #if num>10000:
    #    port=num
    #    continue
    print("\n\nSend fragment " + str(num) + " to port " + str(port) + " \n")

    # create message
    try:
        msg = IP(dst="127.0.0.1", src="127.0.0.1")/UDP(dport=port, sport=8080)/(bytes([0x42, 0x4c, 0x4b]) + num.to_bytes(2,"little"))/packets[num][Raw]
        send(msg)
    except:
        print("Unexpected error:", sys.exc_info()[0])
