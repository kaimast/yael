#! /usr/bin/python3

from math import sqrt
from subprocess import Popen, PIPE
from time import sleep

NUM_CLIENTS=50

server = Popen(["./yael-benchmark-server", "localhost"])

sleep(0.1)

clients = []
latencies = []

for _ in range(NUM_CLIENTS):
    c = Popen(["./yael-benchmark-client", "localhost"], stdout=PIPE)
    clients.append(c)

for c in clients:
    out, err = c.communicate()
 
    for line in out.splitlines():
        lat = float(line)
        latencies.append(lat)

server.kill()

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
