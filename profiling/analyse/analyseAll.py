import re
from copy import deepcopy
import os
import sys
import datetime
import pickle
from tabulate import tabulate
from functools import reduce
from collections import defaultdict

timedrift = 5
#timedrift = 22

if len(sys.argv) != 2:
    print("Usage: ", sys.argv[0]," path_to_testfolder")
    exit(1)
folder = sys.argv[1]

pattern_received = re.compile("^.* ([0-9:]+) received: .*\(([0-9]+) bytes\) peer=([0-9]+)")
pattern_sent = re.compile("^.* ([0-9:]+) sending .*\(([0-9]+) bytes\) peer=([0-9]+)")

pattern_datum = re.compile("[a-zA-Z]+ +[a-zA-Z]+ +[0-9]+ +([0-9:]+) [A-Z]+ 2018")
pattern_nodes_count = re.compile("^([0-9]+)$")

pattern_proc_entry = re.compile("^ *([0-9\.]+) +[0-9\.]+ +[0-9]+ +src/bitcoind +[0-9]+ ([a-zA-Z-]+)$")

pattern_measure = re.compile("^.* ([0-9:]+) MEASURE: ProcessMessage: ([0-9\.]*) bytes ([0-9\.]*)ms")
pattern_measure_socket = re.compile("^.* ([0-9:]+) MEASURE: SocketHandling: ([0-9\.]*)ms")

pattern_testname = re.compile(".*testresults_([0-9]+)_[0-9]+")

pattern_vmstat = re.compile("^ *[0-9]+ +[0-9]+ +([0-9]+) +([0-9]+) +([0-9]+) +([0-9]+) +([0-9]+) +([0-9]+) +([0-9]+) +([0-9]+) +([0-9]+) +([0-9]+) +([0-9]+) +([0-9]+) +([0-9]+) +([0-9]+) +([0-9]+) +[0-9\-]+ +([0-9:]+)")

pattern_ping = re.compile("^[0-9\-]+ +([0-9:]+) +THROUGHPUT: +PING: +([0-9]+)")
pattern_pong = re.compile("^[0-9\-]+ +([0-9:]+) +THROUGHPUT: +PONG: +([0-9]+)")

#top
pattern_data_date = re.compile("top - ([0-9\:]+)")
pattern_thread = re.compile(".* ([0-9\.]+) +[0-9\.]+ +[0-9\.\:]+ +(bitcoin[a-z\-]*)")


class ProcEntry():
    def __init__(self):
        self.datum = ""
        self.node_count = 0
        self.data = []

class Entry():
    def __init__(self):
        self.nodes = 0
        self.datum = ""
        self.rx = 0
        self.tx = 0
        self.measure_total = 0
        self.measure_points = 0
        self.measure_max = 0
        self.measure_socket_total = 0
        self.measure_socket_points = 0
        self.measure_bytes = 0
        self.bytes_sent_by_5 = 0
        self.bytes_received_by_5 = 0

def save_obj(obj, name):
    with open(folder + '/obj/'+ name + '.pkl', 'wb') as f:
        pickle.dump(obj, f, pickle.HIGHEST_PROTOCOL)

def load_obj(name):
    with open(folder + '/obj/' + name + '.pkl', 'rb') as f:
        return pickle.load(f)

if not os.path.exists(folder + "/obj"):
    os.makedirs(folder + "/obj")

if os.path.exists(folder + "/obj/tests.pkl"):
    tests = load_obj("tests")
else:
    tests = {}

    # analyse the number of experiments
    for path in os.walk(folder):
        regexRes = pattern_testname.match(path[0])
        if not regexRes:
            continue
        tests[int(regexRes.group(1))] = {}
        path = path[0]

        # analyse concount
        tests[int(regexRes.group(1))]["concount"] = []
        currentEntry = None
        with open(path+"/date_concount", "r") as file:
            for line in file:
                splited_line = pattern_datum.match(line)
                if splited_line:
                    if currentEntry is not None:
                        tests[int(regexRes.group(1))]["concount"].append(currentEntry)
                    currentEntry = ProcEntry()
                    currentEntry.datum = datetime.datetime.strptime(splited_line.group(1), '%H:%M:%S')
                    continue

                splited_line = pattern_nodes_count.match(line)
                if splited_line and currentEntry is not None:
                    currentEntry.node_count = splited_line.group(1)
                    continue

                splited_line = pattern_proc_entry.match(line)
                if splited_line and currentEntry is not None:
                    currentEntry.data.append(splited_line.group(1))
                    continue

        tests[int(regexRes.group(1))]["start"] = tests[int(regexRes.group(1))]["concount"][0].datum
        tests[int(regexRes.group(1))]["end"] = tests[int(regexRes.group(1))]["concount"][-1].datum

        # analyse vmstat
        tests[int(regexRes.group(1))]["vmstat"] = []
        with open(path + "/vmstat.log", "r") as file:
            for line in file:
                splited_line = pattern_vmstat.match(line)
                if splited_line:
                    entry = {}
                    entry["swap"] = splited_line.group(1)
                    entry["free"] = splited_line.group(2)
                    entry["buff"] = splited_line.group(3)
                    entry["cache"] = splited_line.group(4)
                    entry["swapin"] = splited_line.group(5)
                    entry["swapout"] = splited_line.group(6)
                    entry["diskr"] = splited_line.group(7)
                    entry["diskw"] = splited_line.group(8)
                    entry["interrupts"] = splited_line.group(9)
                    entry["cs"] = splited_line.group(10)
                    entry["user"] = splited_line.group(11)
                    entry["sys"] = splited_line.group(12)
                    entry["idle"] = splited_line.group(13)
                    entry["iowait"] = splited_line.group(14)
                    entry["stolen"] = splited_line.group(15)
                    entry["time"] = datetime.datetime.strptime(splited_line.group(16), '%H:%M:%S')
                    tests[int(regexRes.group(1))]["vmstat"].append(entry)

        # analyse top
        currentEntry = None
        tests[int(regexRes.group(1))]["top"] = []
        with open(path + "/top.log", "r", encoding="utf-8") as file:
            for line in file:
                match = pattern_thread.match(line)
                if match:
                    currentEntry[match.group(2)] = float(match.group(1))

                match = pattern_data_date.match(line)
                if match:
                    if currentEntry == None:
                        currentEntry = {}
                    else:
                        tests[int(regexRes.group(1))]["top"].append(currentEntry)
                        currentEntry = {}
                    currentEntry['date'] = match.group(1)

    currentEntry = None
    with open(folder + "/debug.log","r") as file:
        inExperiment = None
        for line in file:
            # check if this is in our experiment time
            if len(line) < 10:
                continue
            date_tmp = datetime.datetime.strptime(line[11:19], '%H:%M:%S')
            date = date_tmp - datetime.timedelta(hours=timedrift)
            if date.date().year < date_tmp.date().year:
                date = date + datetime.timedelta(days=1)

            if inExperiment is None:
                for key,test in tests.items():
                    if test["start"] <= date <= test["end"]:
                        inExperiment = test
                        inExperiment["log"] = []
                        break
            if inExperiment is None:
                continue

            # check if experiment has finished
            if date > inExperiment["end"]:
                inExperiment = None
                continue

            # analyse data
            splited_line = pattern_received.match(line)
            if splited_line:
                if currentEntry == None:
                    currentEntry = Entry()
                    currentEntry.datum = splited_line.group(1)
                elif currentEntry.datum != splited_line.group(1):
                    inExperiment["log"].append(currentEntry)
                    currentEntry = deepcopy(currentEntry)
                    currentEntry.measure_total = 0
                    currentEntry.measure_points = 0
                    currentEntry.measure_max = 0
                    currentEntry.measure_bytes = 0
                    currentEntry.datum = splited_line.group(1)

                currentEntry.rx += int(splited_line.group(2))
                continue

            splited_line = pattern_sent.match(line)
            if splited_line:
                if currentEntry == None:
                    currentEntry = Entry()
                    currentEntry.datum = splited_line.group(1)
                elif currentEntry.datum != splited_line.group(1):
                    inExperiment["log"].append(currentEntry)
                    currentEntry = deepcopy(currentEntry)
                    currentEntry.measure_total = 0
                    currentEntry.measure_points = 0
                    currentEntry.measure_max = 0
                    currentEntry.measure_bytes = 0
                    currentEntry.datum = splited_line.group(1)

                currentEntry.tx += int(splited_line.group(2))
                continue

            splited_line = pattern_measure.match(line)
            if splited_line:
                if currentEntry == None:
                    currentEntry = Entry()
                    currentEntry.datum = splited_line.group(1)
                elif currentEntry.datum != splited_line.group(1):
                    inExperiment["log"].append(currentEntry)
                    currentEntry = deepcopy(currentEntry)
                    currentEntry.measure_total = 0
                    currentEntry.measure_points = 0
                    currentEntry.measure_max = 0
                    currentEntry.measure_bytes = 0
                    currentEntry.datum = splited_line.group(1)

                currentEntry.measure_points += 1
                currentEntry.measure_total += float(splited_line.group(3))
                currentEntry.measure_max = max(currentEntry.measure_max, float(splited_line.group(2)))
                currentEntry.measure_bytes += float(splited_line.group(2))
                continue

    # analyse pingpong
    with open(folder + "/pingpong.log","r") as file:
        inExperiment = None
        ping = 0
        for line in file:
            if len(line) < 10:
                continue
            date_tmp = datetime.datetime.strptime(line[11:19], '%H:%M:%S')
            date = date_tmp - datetime.timedelta(hours=timedrift)
            if date.date().year < date_tmp.date().year:
                date = date + datetime.timedelta(days=1)

            # check if this is in our experiment time
            if inExperiment is None:
                for key,test in tests.items():
                    if test["start"] <= date <= test["end"]:
                        inExperiment = test
                        test["pingpong"] = []
                        break
            if inExperiment is None:
                continue

            # check if experiment has finished
            if date > inExperiment["end"]:
                inExperiment = None
                ping = 0
                continue

            if ping == 0:
                splited_line = pattern_ping.match(line)
                if splited_line:
                    ping = int(splited_line.group(2))

            else:
                splited_line = pattern_pong.match(line)
                if splited_line:
                    entry = {}
                    entry["date"] = datetime.datetime.strptime(splited_line.group(1), '%H:%M:%S')
                    entry["rtt"] = int(splited_line.group(2)) - ping
                    test["pingpong"].append(entry)
                    ping = 0
    print("fertig")

    save_obj(tests, "tests")

# analyse stuff
with open(folder + "/report.txt", "w") as file:
    file.write("\n")
    file.write("\n")
    file.write("#### connections: ####\n")
    data=[]
    for key,test in sorted(tests.items()):
        cons = [int(a.node_count) for a in test['concount']]
        data.append([key, sum(cons) / float(len(cons))])
    file.write(tabulate(data, headers=['test', 'connections']))
    save_obj(data, "cons")

    for measure in ["swap","free","buff","cache","swapin","swapout","diskr","diskw","interrupts","cs","user","sys","idle","iowait","stolen"]:
        file.write("\n")
        file.write("\n")
        file.write("#### " + measure + ": ####\n")
        data = []

        for key, test in sorted(tests.items()):
            cons = [int(a[measure]) for a in test['vmstat'][1:]]
            data.append([key, sum(cons) / float(len(cons) + 0.0001)])
        file.write(tabulate(data, headers=['test', measure]))
        save_obj(data, measure)

    file.write("\n")
    file.write("\n")
    file.write("#### Threads: ####\n")
    tmp_data = {}
    data = defaultdict(list)
    elementCount = {}
    for key,test in sorted(tests.items()):
        elementCount[key] = float(len(test['top']))
        tmp_data[key] = reduce(lambda x, y: dict((k, int(v) + y[k]) if not isinstance(v, str) else ('date', "") for k, v in sorted(x.items())), test['top'])
        tmp_data[key]["_nodes"] = key
    for key, value in sorted(tmp_data.items()):
        for subkey, subvalue in sorted(value.items()):
            if subkey is "date":
                continue
            elif subkey is "_nodes":
                data["nodes"].append(subvalue)
            else:
                data[subkey].append(subvalue/elementCount[key])

    file.write(tabulate(data,headers="keys"))
    save_obj(data, "threads")

    file.write("\n")
    file.write("\n")
    file.write("#### throughput: ####\n")
    data = []
    for key, test in sorted(tests.items()):
        if not "log" in test:
            continue
        tx = test["log"][-1].tx - test["log"][0].tx
        rx = test["log"][-1].rx - test["log"][0].rx
        total = tx + rx
        time = (datetime.datetime.strptime(test["log"][-1].datum, '%H:%M:%S') - datetime.datetime.strptime(test["log"][0].datum, '%H:%M:%S')).total_seconds()

        if time < 0:
            time += 24*60*60

        prevElem = test["log"][0]
        max_rx = 0
        max_tx = 0
        for el in test["log"][1:]:
            max_tx = max(max_tx, el.tx - prevElem.tx)
            max_rx = max(max_rx, el.rx - prevElem.rx)
            prevElem = el

        data.append([key, int(tx)/float(time), max_tx, int(rx) / float(time), max_rx, int(total) / float(time)])
    file.write(tabulate(data, headers=['test', "tx", "max tx", "rx", "max rx", "total"]))
    save_obj(data, "throughput")


    file.write("\n")
    file.write("\n")
    file.write("#### RTT: ####\n")
    data = []
    for key, test in sorted(tests.items()):
        if not "pingpong" in test:
            continue
        cons = [int(a["rtt"]) for a in test['pingpong']]
        data.append([key, sum(cons) / float(len(cons))])
    file.write(tabulate(data, headers=['test', "RTT"]))
    save_obj(data, "rtt")


