#!/usr/bin/env python3

import sys
import re
import datetime
from collections import defaultdict
from operator import add

import matplotlib.pyplot as plt

#dir = "/Users/Jan/Downloads/testresults_top_3/testresults_top"
dir = "/Users/Jan/Downloads/toptest/"




def readData(dir):
    pattern_data_date = re.compile("top - ([0-9\:]+)")
    pattern_msghand = re.compile(".* ([0-9\.]+) +[0-9\.]+ +[0-9\.\:]+ +bitcoin-msghand")
    pattern_net = re.compile(".* ([0-9\.]+) +[0-9\.]+ +[0-9\.\:]+ +bitcoin-net")

    pattern_datum = re.compile(".* ([0-9\:]+) CDT 2018")
    pattern_nodes = re.compile("^([0-9]+)$")

    data_msghand = defaultdict(lambda: 0)
    data_net = defaultdict(lambda: 0)
    entries = defaultdict(lambda: 0)
    bursts_msghand = defaultdict(lambda: 0)
    bursts_net = defaultdict(lambda: 0)
    dates = []
    nodes = []

    with open(dir + "/date_concount","r") as file:
        for line in file:
            match = pattern_datum.match(line)
            if match:
                dates.append(datetime.datetime.strptime(match.group(1), '%H:%M:%S'))

            match = pattern_nodes.match(line)
            if match:
                nodes.append(int(match.group(1)))

    timeno = 0
    with open(dir + "/topout","r") as file:
        for line in file:
            match = pattern_net.match(line)
            if match:
                data_net[round(nodes[timeno], -1)] += float(match.group(1))
                entries[round(nodes[timeno], -1)] += 1
                if float(match.group(1)) >= 99.9:
                    bursts_net[round(nodes[timeno], -1)] += 1

            match = pattern_msghand.match(line)
            if match:
                data_msghand[round(nodes[timeno], -1)] += float(match.group(1))
                if float(match.group(1)) >= 99.9:
                    bursts_msghand[round(nodes[timeno], -1)] += 1

            match = pattern_data_date.match(line)
            if match:
                dat = datetime.datetime.strptime(match.group(1), '%H:%M:%S')
                # data_date.append(datetime.datetime.strptime(match.group(1), '%H:%M:%S'))
                if timeno + 1 < len(dates) and dat >= dates[timeno+1]:
                    timeno += 1

    for key,entry in entries.items():
        data_net[key] /= entry
        data_msghand[key] /= entry
        bursts_msghand[key] /= entry
        bursts_net[key] /= entry

    return {"net": data_net, "msghand": data_msghand, "netburst": bursts_net, "msgburst": bursts_msghand}

data1 = readData(dir + "toptest_1")
data2 = readData(dir + "toptest_2")
data3 = readData(dir + "toptest_3")

data_net = defaultdict(lambda: 0)
data_msghand = defaultdict(lambda: 0)
burst_net = defaultdict(lambda: 0)
burst_msg = defaultdict(lambda: 0)

for key in sorted(set(list(data1['net'].keys()) + list(data2['net'].keys()) + list(data3['net'].keys()))):
    nums = 0
    if key in data1['net']:
        data_net[key] += data1['net'][key]
        data_msghand[key] += data1['msghand'][key]
        burst_msg[key] += data1['msgburst'][key]
        burst_net[key] += data1['netburst'][key]
        nums += 1
    if key in data2['net']:
        data_net[key] += data2['net'][key]
        data_msghand[key] += data2['msghand'][key]
        burst_msg[key] += data2['msgburst'][key]
        burst_net[key] += data2['netburst'][key]
        nums += 1
    if key in data3['net']:
        data_net[key] += data3['net'][key]
        data_msghand[key] += data3['msghand'][key]
        burst_msg[key] += data3['msgburst'][key]
        burst_net[key] += data3['netburst'][key]
        nums += 1
    data_net[key] /= nums
    data_msghand[key] /= nums
    burst_net[key] /= nums
    burst_msg[key] /= nums



lists = sorted(data_net.items())  # sorted by key, return a list of tuples
x, y = zip(*lists)
plt.plot(x, y, label='bitcoin-net')

lists = sorted(data_msghand.items())  # sorted by key, return a list of tuples
x, y = zip(*lists)
plt.plot(x, y, label='bitcoin-msghand')
plt.ylabel('cpu core usage [%]')
plt.xlabel('connections')
plt.yticks(range(0,100,5))
#plt.axvline(x=section1,color='g')
#plt.axvline(x=section2,color='g')
plt.legend()
plt.savefig('/Users/Jan/Downloads/cpuUsage.eps', format='eps', dpi=1000)
plt.show()


lists = sorted(burst_net.items())  # sorted by key, return a list of tuples
x, y = zip(*lists)
plt.plot(x, y, label='bitcoin-net')

lists = sorted(burst_msg.items())  # sorted by key, return a list of tuples
x, y = zip(*lists)
plt.plot(x, y, label='bitcoin-msghand')
plt.ylabel('bursts')
plt.xlabel('connections')
#plt.axvline(x=section1,color='g')
#plt.axvline(x=section2,color='g')
plt.legend()
plt.savefig('/Users/Jan/Downloads/bursts.eps', format='eps', dpi=1000)
plt.show()


# with open(dir + "/topout","r") as file:
#     for line in file:
#         match = pattern_net.match(line)
#         if match:
#             data_net.append(float(match.group(1)))
#
#         match = pattern_msghand.match(line)
#         if match:
#             data_msghand.append(float(match.group(1)))
#
#         match = pattern_data_date.match(line)
#         if match:
#             data_date.append(datetime.datetime.strptime(match.group(1), '%H:%M:%S'))
#
# date0 = data_date[0]
# data_date[:] = [(dat - date0).total_seconds() for dat in data_date]
#
# dates = []
# nodes = []
# with open(dir + "/date_concount","r") as file:
#     for line in file:
#         match = pattern_datum.match(line)
#         if match:
#             dates.append(datetime.datetime.strptime(match.group(1), '%H:%M:%S'))
#
#         match = pattern_nodes.match(line)
#         if match:
#             nodes.append(int(match.group(1)))
#
#
# date0 = dates[0]
# dates[:] = [(dat - date0).total_seconds() for dat in dates]
# numAggregate = 100
#
# print("data length:" ,len(data_msghand), " ", len(data_net))
#
# #section1 = (datetime.datetime.strptime("18:28:00", '%H:%M:%S')-date0).total_seconds()
# section2 = (datetime.datetime.strptime("20:00:00", '%H:%M:%S')-date0).total_seconds()
#
# data_net_aggregated = []
# for i in range(numAggregate,len(data_net)):
#     data_net_aggregated.append(sum(data_net[i-numAggregate:i])/numAggregate)
#
# data_msghand_aggregated = []
# for i in range(numAggregate,len(data_msghand)):
#     data_msghand_aggregated.append(sum(data_msghand[i-numAggregate:i])/numAggregate)
#
#
# plt.plot(data_date[0:len(data_msghand_aggregated)], data_msghand_aggregated, label='msghandler')
# plt.plot(data_date[0:len(data_net_aggregated)], data_net_aggregated, label='net')
# plt.ylabel('cpu load[%]')
# plt.xlabel('time[s]')
# plt.yticks(range(0,100,5))
# #plt.axvline(x=section1,color='g')
# #plt.axvline(x=section2,color='g')
# plt.legend()
# plt.show()
#
# # data_msghand_aggregated = []
# # for i in range(0,int(len(data_msghand)/numAggregate)-1):
# #     data_msghand_aggregated.append(sum(data_msghand[i*numAggregate:i*numAggregate+5])/numAggregate)
# #
# # numAggregate = 200
# # data_msghand_max = []
# # for i in range(numAggregate,len(data_msghand)):
# #     data_msghand_max.append(max(data_msghand[i-numAggregate:i]))
# # data_msghand_min = []
# # for i in range(numAggregate,len(data_msghand)):
# #     data_msghand_min.append(min(data_msghand[i-numAggregate:i]))
#
#
#
#
# #data_net_aggregated = []
# #for i in range(0,int(len(data_net)/numAggregate)-1):
# #    data_net_aggregated.append(sum(data_net[i*numAggregate:i*numAggregate+5])/numAggregate)
#
# numAggregate = 1000
#
# data_net_aggregated = []
# for i in range(numAggregate,len(data_net)):
#     data_net_aggregated.append(sum(i > 99 for i in data_net[i-numAggregate:i]))
#
# data_msghand_aggregated = []
# for i in range(numAggregate,len(data_msghand)):
#     data_msghand_aggregated.append(sum(i > 99 for i in data_msghand[i-numAggregate:i]))
#
# plt.plot(data_date[0:len(data_msghand_aggregated)], data_msghand_aggregated, label='msghandler')
# plt.plot(data_date[0:len(data_net_aggregated)], data_net_aggregated, label='net')
# #plt.plot(data_date[0:len(data_msghand_max)], data_msghand_max, "-")
# #plt.plot(data_date[0:len(data_msghand_min)], data_msghand_min, "-")
# #plt.plot(data_msghand_min, "-")
# #plt.plot(data_net_aggregated, label='net')
# plt.ylabel('bursts per second')
# plt.xlabel('time[s]')
# #plt.axvline(x=section1, color='g')
# #plt.axvline(x=section2, color='g')
# plt.legend()
# plt.show()
# plt.plot(dates,nodes)
# plt.ylabel('connected nodes')
# plt.xlabel('time[s]')
# #plt.axvline(x=section1,color='g')
# #plt.axvline(x=section2,color='g')
# plt.show()
#
#
# #file = open("/Users/Jan/Downloads/testresults_top 3/nodes.csv", "w")
# #file.write("time;nodes\n")
# #for i in range(0, len(nodes)):
# #    file.write(str(dates[i]) + ";" + str(nodes[i]) +"\n")
# #file.close()