#!/usr/bin/env python3
import datetime
from scapy.all import *
import socket
from joblib import Parallel, delayed
import multiprocessing
num_cores = multiprocessing.cpu_count()
import sys

s = conf.L3socket(iface='h2-eth0')

class node():
    socket = None

    def __init__(self, s):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(('', 0))
        self.handshake(s)

    def handshake(self, s):
        print("send rey")
        s.send(IP(dst="10.0.0.10", src="10.0.1.10") / UDP(dport=10, sport=self.socket.getsockname()[1]) / (
                    bytes([0x52, 0x45, 0x59, 0x10, 0x00, 0x00])))
        data, addr = self.socket.recvfrom(1024)
        print(data)

        s.send(IP(dst="10.0.0.10", src="10.0.1.10") / UDP(dport=10, sport=self.socket.getsockname()[1]) / (
                data[:3] + bytes([data[3] + 16]) + data[4:]))

    def receiveINV(self):
        print("wait for inv on port ", self.socket.getsockname()[0], " ", self.socket.getsockname()[1])
        sys.stdout.flush()
        self.socket.recvfrom(1024)
        print(datetime.datetime.now())
        print("received INV\n")

nodes = Parallel(n_jobs=10)(delayed(node)(s) for i in range(1,1001))
print(datetime.datetime.now())
sys.stdout.flush()
for n in nodes:
    n.receiveINV()
