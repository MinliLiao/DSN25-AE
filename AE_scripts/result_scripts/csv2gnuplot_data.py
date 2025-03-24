import sys
import argparse

parser = argparse.ArgumentParser(description='Transform csv to gnuplot .data file')
parser.add_argument('--filename', type=str, action='store',required=True,
                    help='the filename of the csv file to transform')
parser.add_argument('--oppMode', type=str,
                    choices=["With","Without","Both"],
                    required=True,
                    help='whether to have results with opportunistic mode or without or both')
parser.add_argument('--noTrans', action='store_true',
                    help='if the data from the csv file needs to be transposed')
parser.add_argument('--noBase', action='store_true',
                    help='Skip baseline results')
parser.add_argument('--groupByConfig', action='store_true',
                    help='Add blank lines to separage results of different checker core count and core type')
parser.add_argument('--filterByCore', type=str,
                    choices=["X2","A510"],
                    help='whether to have results of only X2 or A510 core or not')

args = parser.parse_args()

def parse_csv(filename):
    stats = dict()
    with open(filename,'r') as f:
        first = True
        stat_name = ''
        benchmarks = []
        for line in f:
            if first:
                stat_name = line.replace("\n","").split(",")[0]
                benchmarks = line.replace("\n","").split(",")[1:]
                for benchmark in benchmarks:
                    stats[benchmark] = dict()
                first = False
            else:
                for i in range(len(benchmarks)):
                    if args.oppMode in ["With"] and "opp" not in line.split(",")[0]:
                        continue
                    elif args.oppMode in ["Without"] and "opp" in line.split(",")[0]:
                        continue
                    stats[benchmarks[i]][line.split(",")[0]] = line.replace("\n","").split(",")[i+1]
    return stat_name, stats

stat_name, stats = parse_csv(args.filename)
benchmarks = list(stats.keys())
configs = list(stats[benchmarks[0]].keys())

if args.noBase:
    if "Baseline" in configs:
        configs.remove("Baseline")
if args.filterByCore:
    to_remove_configs = []
    for config in configs:
        if args.filterByCore not in config:
            to_remove_configs.append(config)
    for config in to_remove_configs:
        configs.remove(config)
if args.noTrans:
    data_str = [["# " + stat_name + "," + ",".join(benchmarks)]]
    for config in configs:
        data_str.append([config.replace("@", "\\\\@").replace("_","\\\\_")])
    for benchmark in benchmarks:
        for i, config in enumerate(configs):
            data_str[i+1].append(stats[benchmark][config])
else:
    data_str = [["# " + stat_name + "," + ",".join(configs)]]
    for benchmark in benchmarks:
        data_str.append([benchmark])
    for config in configs:
        for i, benchmark in enumerate(benchmarks):
            data_str[i+1].append(stats[benchmark][config])
if args.groupByConfig:
    lastConfig = ""
    i = 1
    for config in configs:
        data_str[i][0] = config.split("@")[1].replace("_","\\\\_")
        if lastConfig not in [""] and lastConfig.split("@")[0] not in [config.split("@")[0]]:
            data_str.insert(i, "")
            i += 1
        i += 1
        lastConfig = config
            

print("\n".join(" ".join(line) for line in data_str))


