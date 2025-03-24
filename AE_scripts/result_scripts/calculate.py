import sys
import argparse
import math

parser = argparse.ArgumentParser(description='Collect statistics')
parser.add_argument('--stat', type=str, action='store',required=True,
                    help='the name of the statstics to output')
parser.add_argument('--op', type=str,
                    choices=['add','sub','mult','div','normalize','average','geomean','high','low','crossGeomean','sum','max'],
                    required=True,
                    help='calculation operation on statistics')
parser.add_argument('--base', type=str, action='store', default='Baseline',
                    help='the name of config used as base for normalization')
parser.add_argument('--csv', type=str, action='store',
                    help='comma separated list of csv files containing data')
parser.add_argument('--include', type=str, action='store',
                    help='stat prefix name and comma seperated list of configs to include in high/low/crossGeomean calculations, e.g. "newStat,config1,config2"')
parser.add_argument('--exclude', type=str, action='store',
                    help='comma seperated list of configs to exclude in the output')

args = parser.parse_args()
if (args.op in ['add','sub','mult','div']) and (len(args.csv.split(",")) != 2):
    print(args.op + " requires 2 input files, " + str(len(args.csv.split(","))) + " given")
    sys.exit()
elif (args.op in ['normalize','average','geomean','high','low','crossGeomean']) and (len(args.csv.split(",")) != 1):
    print(args.op + " requires 1 input file, " + str(len(args.csv.split(","))) + " given")
    sys.exit()
elif (args.op in ['sum','max']) and (len(args.csv.split(",")) <= 1):
    print(args.op + " requires more than 1 input file, " + str(len(args.csv.split(","))) + " given")
    sys.exit()

def parse_csv(filename):
    stats = dict()
    with open(filename,'r') as f:
        first = True
        stat_name = ''
        benchmarks = []
        for line in f:
            if first:
                stats_name = line.replace("\n","").split(",")[0]
                benchmarks = line.replace("\n","").split(",")[1:]
                for benchmark in benchmarks:
                    stats[benchmark] = dict()
                first = False
            else:
                for i in range(len(benchmarks)):
                    stats[benchmarks[i]][line.split(",")[0]] = eval(line.replace("\n","").split(",")[i+1])
    return stats

stats_A = parse_csv(args.csv.split(",")[0])
benchmarks = list(stats_A.keys())
configs = list(stats_A[benchmarks[0]].keys())
    
if args.op in ['add','sub','mult','div']:
    stats_B = parse_csv(args.csv.split(",")[1])
    stats = dict()
    for benchmark in benchmarks:
        stats[benchmark] = dict()
        for config in configs:
            if args.op in ["add"]:
                stats[benchmark][config] = stats_A[benchmark][config] + stats_B[benchmark][config]
            elif args.op in ["sub"]:
                stats[benchmark][config] = stats_A[benchmark][config] - stats_B[benchmark][config]
            elif args.op in ["mult"]:
                stats[benchmark][config] = stats_A[benchmark][config] * stats_B[benchmark][config]
            else: # div
                if stats_A[benchmark][config] == 0:
                    stats[benchmark][config] = 0 # to avoid error out when both A and B are 0
                else:   
                    stats[benchmark][config] = stats_A[benchmark][config] / stats_B[benchmark][config]
elif args.op in ['average','geomean']:
    stats = stats_A
    stats[args.op] = dict()
    for config in configs:
        if args.op in ["average"]:
            result = math.fsum([stats[benchmark][config] for benchmark in benchmarks])/len(benchmarks)
        elif args.op in ["geomean"]:
            result = math.prod([stats[benchmark][config] for benchmark in benchmarks])
            if result != 0:
                result = math.pow(result, 1/len(benchmarks))
        stats[args.op][config] = result
    benchmarks.append(args.op)
elif args.op in ['high','low','crossGeomean']:
    stats = stats_A
    included_configs = configs
    config_prefix = ""
    if args.include:
        config_prefix = args.include.split(",")[0]
        included_configs = args.include.split(",")[1:]
        if len(set(included_configs)) != len(included_configs):
            print(args.include + " contain repeated configs")
            sys.exit()
        if not (set(configs) > set(included_configs)):
            print(args.include + " contain configs not in file")
            sys.exit()
    for benchmark in benchmarks:
        if args.op in ["high"]:
            result = max([stats[benchmark][config] for config in included_configs])
        elif args.op in ["low"]:
            result = min([stats[benchmark][config] for config in included_configs])
        elif args.op in ["crossGeomean"]:
            result = math.prod([stats[benchmark][config] for config in included_configs])
            if result != 0:
                result = math.pow(result, 1/len(included_configs))
        stats[benchmark][config_prefix+("geomean" if args.op in ["crossGeomean"] else args.op)] = result
    configs.append(config_prefix+("geomean" if args.op in ["crossGeomean"] else args.op))
elif args.op in ['sum','max']:
    num_input_files = len(args.csv.split(","))
    stats_array = [parse_csv(args.csv.split(",")[i]) for i in range(num_input_files)]
    stats = dict()
    for benchmark in benchmarks:
        stats[benchmark] = dict()
        for config in configs:
            if args.op in ["sum"]:
                stats[benchmark][config] = sum([stats_array[i][benchmark][config] for i in range(num_input_files)])
            else: # max
                stats[benchmark][config] = max([stats_array[i][benchmark][config] for i in range(num_input_files)])
else: # normalize
    stats = dict()
    for benchmark in benchmarks:
        stats[benchmark] = dict()
        for config in configs:
            stats[benchmark][config] = stats_A[benchmark][config] / stats_A[benchmark][args.base]

# Generate csv
stat_matrix = []
line = [args.stat]
line.extend(benchmarks)
stat_matrix.append(line)
for config in configs:
    if args.exclude:
        if not (set(configs) > set(args.exclude.split(","))):
            print(args.exclude + " contain configs not in file")
            sys.exit()
        if config in args.exclude.split(","):
            continue
    line = [config]
    line.extend([str(stats[benchmark][config]) for benchmark in benchmarks])
    stat_matrix.append(line)
stat_csv = "\n".join(",".join(line) for line in stat_matrix)
print(stat_csv)