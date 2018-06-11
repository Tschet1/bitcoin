#!/usr/bin/env python3
from scapy.all import *

#client tests
# CTR
def testCTR():
    print("Check using wireshark. Should try to create a connection to 127.0.0.5:1234")
    msg = IP(dst="127.0.0.1", src="127.0.0.1")/UDP(dport=8333, sport=8080)/(bytes([0x43, 0x54, 0x52, 0x7f, 0x00, 0x00, 0x05, 0x00, 0x04, 0xd2]))
    send(msg)

#master tests
# CON
def testCON():
    print("Check using wireshark. Should try to create a connection to 127.0.0.5:1234")
    msg = IP(dst="127.0.0.1", src="127.0.0.1")/UDP(dport=8333, sport=8080)/(bytes([0x43, 0x4f, 0x4e, 0x7f, 0x00, 0x00, 0x05, 0x00, 0x04, 0xd2]))
    send(msg)
