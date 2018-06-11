import matplotlib.pyplot as plt
from tabulate import tabulate
from operator import add

import pickle


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


def load_obj(folder, name):
    with open(folder + '/obj/' + name + '.pkl', 'rb') as f:
        return pickle.load(f)


#folder1 = "/Users/Jan/Downloads/testresults_pingpong2"
#folder2 = "/Users/Jan/Downloads/testresults_pingpong3"
#folder3 = "/Users/Jan/Downloads/testresults_pingpong4"

folder1 = "/Users/Jan/Downloads/dev2/testresults_f1"
folder2 = "/Users/Jan/Downloads/dev2/testresults_f2"
folder3 = "/Users/Jan/Downloads/dev2/testresults_f3"

results = ["idle", "user", "buff", "cache", "cons", "cs", "diskr", "diskw", "free", "interrupts", "iowait", "rtt", "swap", "sys"]

tests = load_obj(folder1, "tests")

cons1 = load_obj(folder1, "cons")
cons2 = load_obj(folder2, "cons")
cons3 = load_obj(folder2, "cons")

for what in results:
    data1 = load_obj(folder1, what)
    data2 = load_obj(folder2, what)
    data3 = load_obj(folder3, what)

    print(what)

    numexperiments = min(6, len(data1), len(data2), len(data3))
    data1[:] = [x[1] for x in data1]
    data2[:] = [x[1] for x in data2]
    data3[:] = [x[1] for x in data3]

    #data1[:] = [x[1] / data1[0][1] * 100 for x in data1]
    #data2[:] = [x[1] / data2[0][1] * 100 for x in data2]
    #data3[:] = [x[1] / data3[0][1] * 100 for x in data3]

    out = map(add, data1[0:numexperiments], data2[0:numexperiments])
    out = list(map(add, out, data3[0:numexperiments]))
    print(tabulate([[x / 3 for x in out]], headers=[str(x[0]) for x in cons1[0:numexperiments]]))

    #plt.plot([x[0] for x in cons1[0:numexperiments]], [x / 3 for x in out])
    #plt.ylabel(what)
    #plt.xlabel('connections')
    # plt.legend()
    #plt.show()

print("total")
total = [0, 0, 0, 0, 0, 0]
for folder in [folder1, folder2, folder3]:
    sys = load_obj(folder, "sys")
    user = load_obj(folder, "user")
    total = list(map(add, total, map(add, [x[1] for x in sys], [x[1] for x in user])))

total = [x / 3 for x in total]
print(total)


#
# data1 = load_obj(folder1, "threads")
# data2 = load_obj(folder2, "threads")
# data3 = load_obj(folder3, "threads")
#
# res = {}
#
# for key in data1:
#     if key == "nodes":
#         continue
#
#     tmp = map(add, data1[key], data2[key])
#     res[key] = [x/3 for x in list(map(add,tmp, data3[key]))]
#
#     plt.plot([int(x[0]) for x in cons1], res[key], label=key, marker='x')
#     plt.ylabel("CPU core usage [%]")
#     plt.yticks(range(0, 101, 10))
#     plt.xlabel('connections')
#     plt.legend()
# plt.savefig('/Users/Jan/Downloads/threads.eps', format='eps', dpi=1000)
# plt.show()
# print(res)










