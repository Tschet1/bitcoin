#!/usr/bin/env python3
from datetime import datetime
from scapy.all import *
import socket
import select
from joblib import Parallel, delayed
import multiprocessing
from time import sleep
num_cores = multiprocessing.cpu_count()

conf.L3socket
conf.L3socket=L3RawSocket

class node():
    socket = None

    def __init__(self,i):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(('localhost', 0))
        self.handshake()
        print(i)
    
    def handshake(self):
        print("send con")
        send(IP(dst="localhost", src="localhost") / UDP(dport=55555, sport=self.socket.getsockname()[1]) / (
        bytes([0x43, 0x4f, 0x4e, 0x00]) + int(self.socket.getsockname()[1]).to_bytes(2,"big") + bytes([0x7f, 0x00, 0x00, 0x01])))
    
    def receiveINV(self):
        print("wait for inv\n")
        self.socket.recvfrom(1024)
        print(datetime.now())
        print("received INV\n")


#nodes = Parallel(n_jobs=10)(delayed(node)() for i in range(1,10000))
print(datetime.now())
nodes = []
for i in range(1,10001):
    nodes.append(node(i))
    sleep(0.005)

print(datetime.now())

for n in nodes:
    n.receiveINV()
