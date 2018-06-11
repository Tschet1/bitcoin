#!/bin/env python3
import re
import sys
import datetime
from collections import defaultdict

if len(sys.argv) != 2:
    print("Usage: ", sys.argv[0]," path_to_testfolder")
    exit(1)
file = sys.argv[1]

pattern = re.compile("^t([0-9]) = ([0-9:.]+)$")

times = defaultdict(lambda: 0)
count = 0

with open(file,"r") as ofile:
    for line in ofile:
        match = pattern.match(line)
        if match:
            ttime = datetime.datetime.strptime(match.group(2), '%H:%M:%S.%f')
            if int(match.group(1)) == 0:
                times["0"] = ttime
                count += 1
            else:
                times[match.group(1)] += (ttime - times["0"]).total_seconds()

print("t1=" + str(times["1"]/count))
print("t2=" + str(times["2"]/count))
print("t3=" + str(times["3"]/count))
print("t4=" + str(times["4"]/count))
print("t5=" + str(times["5"]/count))
print("t6=" + str(times["6"]/count))
print("t7=" + str(times["7"]/count))
print("t8=" + str(times["8"]/count))
print("t9=" + str(times["9"]/count))
with open(file, 'r') as f:
    lines = f.read().splitlines()
    last_line = lines[-1]
    print(last_line)
