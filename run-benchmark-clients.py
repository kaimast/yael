#! /usr/bin/python3

from math import sqrt
from subprocess import Popen, PIPE
from time import sleep

import argparse

parser = argparse.ArgumentParser()
parser.add_argument("num_clients", type=int)
parser.add_argument("server_name", type=str);

args = parser.parse_args()

clients = []
latencies = []

for _ in range(args.num_clients):
    c = Popen(["./yael-benchmark-client", args.server_name], stdout=PIPE)
    clients.append(c)

for c in clients:
    out, err = c.communicate()
 
    for line in out.splitlines():
        lat = float(line)
        latencies.append(lat)

lat_sum = 0

for lat in latencies:
    lat_sum += lat

lat_mean = lat_sum / len(latencies)

var_sum = 0

for lat in latencies:
    var_sum += (lat - lat_mean) * (lat - lat_mean)

lat_dev = sqrt( var_sum / len(latencies) )

print("Mean: " + str(lat_mean))
print("Deviation: " + str(lat_dev))
