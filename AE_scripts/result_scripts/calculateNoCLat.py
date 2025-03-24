import sys
import argparse
import math
import pprint

parser = argparse.ArgumentParser(description='Collect statistics')
parser.add_argument('--op', type=str,
                    choices=['1M2C', '1M4C', '1M12C', '1M16C', '2M', '4M', '4x4i', '4x4o'],
                    required=True,
                    help='operation to perform')
parser.add_argument('--csv', type=str, action='store', required=True,
                    help='comma separated list of csv files containing data')
parser.add_argument('--slowNoC', action='store_true',
                    help='Use the 128b width 1.5GHz clock NoC')
parser.add_argument('--slowNoCEstimate', type=str, action='store',
                    help='csv files containing estimated slowNoC cycles')
parser.add_argument('--hashed', action='store_true',
                    help='The LSL entries are hashed to reduce message size')
parser.add_argument('--coreClockRate', type=int, action='store', default=3000000000, 
                    help="""Clock rate of main cores to calculate the number of 
                        cycles from sim seconds.""")
parser.add_argument('--NoCClockRate', type=int, action='store', default=2000000000, 
                    help="""Clock rate of NoC to calculate service rate.""")
parser.add_argument('--NoCWidth', type=int, action='store', default=256, 
                    help="""NoC data transfer width in bits.""")
parser.add_argument('--num_mains', type=int, action='store', default=None, 
                    help="""Number of main cores.""")
parser.add_argument('--num_checkers_per_main', type=int, action='store', default=None, 
                    help="""Number of checker cores per main core.""")

LSLEntryWidth = 128 # Size of LSL entry in bits (average)
LSLMsgWidth = 512 # Size of LSL data per push in bits
respMsgWidth = 512 # Size of LLC response data per response in bits

args = parser.parse_args()
if (args.slowNoC): # 128-bit wide, 1.5GHz clock NoC
    args.NoCWidth = 128
    args.NoCClockRate = 1500000000
if args.hashed:
    LSLEntryWidth = 64 # Only address + size in hashed mode

LSLEntryPerMsg = LSLMsgWidth/LSLEntryWidth # Number of LSL entry per push
# number of packets generated per transfer
message_size = math.ceil(LSLMsgWidth/args.NoCWidth)
response_size = math.ceil(respMsgWidth/args.NoCWidth) + 1 # 1 extra for address etc
# number of packets that can be processed per main core cycle
service_rate = args.NoCClockRate/args.coreClockRate

if args.op in ['4M']:
    args.num_mains = 4
elif args.op in ['2M']:
    args.num_mains = 2
elif args.op in ['1M2C', '1M4C', '1M12C', '1M16C']:
    args.num_mains = 1
    if args.op in ['1M4C']:
        args.num_checkers_per_main = 4
    elif args.op in ['1M12C']:
        args.num_checkers_per_main = 12
    elif args.op in ['1M16C']:
        args.num_checkers_per_main = 16
elif args.op in ['4x4i', '4x4o'] and not args.num_mains:
    print(args.op + " requires specifying the number of main cores with --num_mains option.")
    sys.exit()
elif args.op in ['4x4i', '4x4o'] and args.num_mains < 1:
    print(args.op + " requires at least 1 main core, " + str(args.num_mains) + " given")
    sys.exit()
elif args.op in ['4x4o'] and not args.num_checkers_per_main:
    print(args.op + " requires specifying the number of checker cores per main core with --num_checkers_per_main option.")
    sys.exit()
elif args.op in ['4x4o'] and args.num_checkers_per_main < 1:
    print(args.op + " requires at least 1 checker core per main core, " + args.num_checkers_per_main + " given")
    sys.exit()

if (args.num_mains == 1) and args.hashed and (len(args.csv.split(",")) != 5):
    print(args.op + " with " + str(args.num_mains) + " main core(s) requires 5 input files, " + str(len(args.csv.split(","))) + " given")
    sys.exit()
elif (args.num_mains == 1) and (not args.hashed) and (len(args.csv.split(",")) != 3):
    print(args.op + " with " + str(args.num_mains) + " main core(s) requires 3 input files, " + str(len(args.csv.split(","))) + " given")
    sys.exit()
elif (args.num_mains == 2) and (len(args.csv.split(",")) != 5):
    print(args.op + " with " + str(args.num_mains) + " main core(s) requires 5 input files, " + str(len(args.csv.split(","))) + " given")
    sys.exit()
elif (args.num_mains == 4) and (len(args.csv.split(",")) != 9):
    print(args.op + " with " + str(args.num_mains) + " main core(s) requires 9 input files, " + str(len(args.csv.split(","))) + " given")
    sys.exit()

num_acc_input_per_core = 2
if args.hashed:
    num_acc_input_per_core = 4
if ("numCycles" not in args.csv.split(",")[0]) and ("simSeconds" not in args.csv.split(",")[0]):
    print ("first csv input file should be number of cycles or simulated seconds, " + args.csv.split(",")[0] + " given")
    sys.exit()
elif (args.num_mains == 1):   
    if "L1DAcc" not in args.csv.split(",")[1]:
        print ("second csv input file should be number of L1 data cache accesses, " + args.csv.split(",")[1] + " given")
        sys.exit()
    elif "LLCAcc" not in args.csv.split(",")[2]:
        print ("third csv input file should be number of LLC accesses, " + args.csv.split(",")[2] + " given")
        sys.exit()
    elif args.hashed and "L1DReadAcc" not in args.csv.split(",")[3]:
        print ("fourth csv input file should be number of L1D read accesses, " + args.csv.split(",")[3] + " given")
        sys.exit()
    elif args.hashed and "L1DSwapAcc" not in args.csv.split(",")[4]:
        print ("fifth csv input file should be number of L1D swap accesses, " + args.csv.split(",")[4] + " given")
        sys.exit()
else: 
    for i in range(args.num_mains):
        if ("L1DAcc_m"+str(i)) not in args.csv.split(",")[i*num_acc_input_per_core+1]:
            print (str(i*num_acc_input_per_core+1) + "th csv input file should be number of L1 data cache accesses on the " + str(i+1) + "th main core, " + args.csv.split(",")[i*num_acc_input_per_core+1] + " given")
            sys.exit()
        elif ("LLCAcc_m"+str(i)) not in args.csv.split(",")[i*num_acc_input_per_core+2]:
            print (str(i*num_acc_input_per_core+2) + "th csv input file should be number of LLC accesse on the " + str(i+1) + "th main core, " + args.csv.split(",")[i*num_acc_input_per_core+2] + " given")
            sys.exit()
        elif args.hashed and ("L1DReadAcc_m"+str(i)) not in args.csv.split(",")[i*num_acc_input_per_core+3]:
            print (str(i*num_acc_input_per_core+3) + "th csv input file should be number of L1D read accesses on the " + str(i+1) + "th main core, " + args.csv.split(",")[i*num_acc_input_per_core+3] + " given")
            sys.exit()
        elif args.hashed and ("L1DSwapAcc_m"+str(i)) not in args.csv.split(",")[i*num_acc_input_per_core+4]:
            print (str(i*num_acc_input_per_core+4) + "th csv input file should be number of L1D swap accesses on the " + str(i+1) + "th main core, " + args.csv.split(",")[i*num_acc_input_per_core+4] + " given")
            sys.exit()

if args.slowNoC and (not args.hashed) and (not args.slowNoCEstimate):
    print("slowNoC estimate file is required for slowNoC without hashed")
    sys.exit()

# Layout of mesh with 4 cache slices, cache slices are named after their closest core
if args.op in ['4x4i', '4x4o']:
    cache_slices_dist = [[],[],[],[]]
    cache_slices_dist[0] = [0, [1,2], 3] # Cache slice close to core 0 is 0 hops away from slice 0, 1 hop away from slices 1 and 2, 2 hops away from slice 3
    cache_slices_dist[1] = [1, [0,3], 2]
    cache_slices_dist[2] = [2, [0,3], 1]
    cache_slices_dist[3] = [3, [1,2], 0]

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

# Read input files
stats_numCycles = parse_csv(args.csv.split(",")[0])
stats_L1DAcc_list = [parse_csv(args.csv.split(",")[i*num_acc_input_per_core+1]) for i in range(args.num_mains)]
stats_LLCAcc_list = [parse_csv(args.csv.split(",")[i*num_acc_input_per_core+2]) for i in range(args.num_mains)]

benchmarks = list(stats_numCycles.keys())
configs = list(stats_numCycles[benchmarks[0]].keys())  
if (len(configs) != 1) or (configs[0] not in ["Baseline"]):
    print("Should only use Baseline results", configs)
    sys.exit()
if args.slowNoC and not args.hashed:
    estimated_checked_cycles = dict()
    for benchmark in benchmarks:
        if (args.op in ['1M2C']):
            estimated_checked_cycles[benchmark] = parse_csv(args.slowNoCEstimate)[benchmark]["X2_slowNoC"]
        elif (args.op in ['4x4i']):
            estimated_checked_cycles[benchmark] = parse_csv(args.slowNoCEstimate)[benchmark][args.op]
        elif (args.op in ['4x4o']):
            estimated_checked_cycles[benchmark] = parse_csv(args.slowNoCEstimate)[benchmark][args.op + str(args.num_checkers_per_main) + "c"]
        elif args.op in ['1M12C', '1M16C']:
            print("slowNoC does not support " + args.op + " yet")
            sys.exit()
        else:
            estimated_checked_cycles[benchmark] = parse_csv(args.slowNoCEstimate)[benchmark]["A510_slowNoC"]
if args.hashed:
    stats_L1DRead_list = [parse_csv(args.csv.split(",")[i*num_acc_input_per_core+3]) for i in range(args.num_mains)]
    stats_L1DSwap_list = [parse_csv(args.csv.split(",")[i*num_acc_input_per_core+4]) for i in range(args.num_mains)]
    # Adjust L1DAcc size for calculations later, expecting each L1DAcc generating 16B entry originally
    for i in range(args.num_mains):
        for benchmark in benchmarks:
            if (stats_L1DAcc_list[i][benchmark][configs[0]] - stats_L1DRead_list[i][benchmark][configs[0]] - stats_L1DSwap_list[i][benchmark][configs[0]] ) < 0:
                print(benchmark + " L1DAcc size smaller than read+swap size")
                sys.exit()
            # Loads and swaps, 8B read data. Stores do not generate per access data traffic
            stats_L1DAcc_list[i][benchmark][configs[0]] = \
                (stats_L1DRead_list[i][benchmark][configs[0]] + stats_L1DSwap_list[i][benchmark][configs[0]])
# Aggregate input into more easy to use format
stats = dict()
for benchmark in benchmarks:
    stats[benchmark] = dict()
    if "simSeconds" in args.csv.split(",")[0]:
        # calculate number of cycles from simulated seconds
        stats[benchmark]["numCycles"] = stats_numCycles[benchmark][configs[0]]*args.coreClockRate
    else:
        stats[benchmark]["numCycles"] = stats_numCycles[benchmark][configs[0]]
    if args.slowNoC and not args.hashed: # need to use estimated cycles to join MM1 model with simulation result
        stats[benchmark]["numCycles_checked"] = estimated_checked_cycles[benchmark]
    else:
        stats[benchmark]["numCycles_checked"] = stats[benchmark]["numCycles"]
    stats[benchmark]["L1DAcc"] = stats_L1DAcc_list[0][benchmark][configs[0]]
    stats[benchmark]["LLCAcc"] = stats_LLCAcc_list[0][benchmark][configs[0]]

    for i in range(args.num_mains):   
        stats[benchmark]["L1DAcc_m" + str(i)] = stats_L1DAcc_list[i][benchmark][configs[0]]
        stats[benchmark]["LLCAcc_m" + str(i)] = stats_LLCAcc_list[i][benchmark][configs[0]]
    # Calculate number of LSL packets and number of LLC response packets
    stats[benchmark]["LSLPKT"] = (stats[benchmark]["L1DAcc"] / LSLEntryPerMsg) * message_size
    stats[benchmark]["LLCRESP"] = stats[benchmark]["LLCAcc"] * response_size

    for i in range(args.num_mains):
        stats[benchmark]["LSLPKT_m" + str(i)] = (stats[benchmark]["L1DAcc_m" + str(i)] / LSLEntryPerMsg) * message_size
        stats[benchmark]["LLCRESP_m" + str(i)] = stats[benchmark]["LLCAcc_m" + str(i)] * response_size       
    # print("Basic stats from input:")
    # pprint.pp(stats)
    # Calculate traffic on different link/routers
    if args.op in ['4x4i', '4x4o']:
        # All the end to end demand traffic routes
        traffics = []
        for i in range(args.num_mains):
            traffics.extend(["base_M" + str(i) + "toLLC" + str(i), 
                             "checked_M" + str(i) + "toLLC" + str(i), 
                             "base_LLC" + str(i) + "toM" + str(i), 
                             "checked_LLC" + str(i) + "toM" + str(i)]) # traffic to and from each main core to it's own LLC slice
            traffics.extend(["base_M" + str(i) + "toLLC" + str(j) for j in cache_slices_dist[i][1]]) # base traffic from each main core to each adjacent LLC slice
            traffics.extend(["base_M" + str(i) + "toLLC" + str(cache_slices_dist[i][2]) + "viaLLC" + str(j) for j in cache_slices_dist[i][1]]) # base traffic from each main core to diagonal LLC slice via each adjacent LLC slice
            traffics.extend(["checked_M" + str(i) + "toLLC" + str(j) for j in cache_slices_dist[i][1]]) # checked traffic from each main core to each adjacent LLC slice
            traffics.extend(["checked_M" + str(i) + "toLLC" + str(cache_slices_dist[i][2]) + "viaLLC" + str(j) for j in cache_slices_dist[i][1]]) # checked traffic from each main core to diagonal LLC slice via each adjacent LLC slice
            traffics.extend(["base_LLC" + str(j) + "toM" + str(i) for j in cache_slices_dist[i][1]]) # base traffic from each adjacent LLC slice to each main core
            traffics.extend(["base_LLC" + str(cache_slices_dist[i][2]) + "viaLLC" + str(j) + "toM" + str(i) for j in cache_slices_dist[i][1]]) # base traffic from diagonal LLC slice via each adjacent LLC slice to each main core
            traffics.extend(["checked_LLC" + str(j) + "toM" + str(i) for j in cache_slices_dist[i][1]]) # checked traffic from each adjacent LLC slice to each main core
            traffics.extend(["checked_LLC" + str(cache_slices_dist[i][2]) + "viaLLC" + str(j) + "toM" + str(i) for j in cache_slices_dist[i][1]]) # checked traffic from diagonal LLC slice via each adjacent LLC slice to each main core
        # print("Added traffic routes:")
        # pprint.pp(traffics)
        # Calculate amount of traffic on each link
        base_MxtoMxR = [stats[benchmark]["LLCAcc_m" + str(i)] for i in range(args.num_mains)]  # Main to Main_router link
        checked_MxtoMxR = [stats[benchmark]["LLCAcc_m" + str(i)] + stats[benchmark]["LSLPKT_m" + str(i)] for i in range(args.num_mains)]  # Main to Main_router link with LSL traffic
        MxRtoMx = [stats[benchmark]["LLCRESP_m" + str(i)] for i in range(args.num_mains)] # Main_router to Main link, no difference between base and check since no LSL traffic
        # 1/4 of each main cores' LLC traffic from each slice
        LLCxtoLLCxR = [sum(stats[benchmark]["LLCRESP_m" + str(i)] for i in range(args.num_mains))/4 for i in range(4)] # LLC slice to attached router
        LLCxRtoLLCx = [sum(stats[benchmark]["LLCAcc_m" + str(i)] for i in range(args.num_mains))/4 for i in range(4)] # Attached router to LLC slice
        # Calculate traffic on links between routers with cache slices attached
        LLCxRtoLLCyR_matrix = [[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0]]
        # print("Added common traffics:")
        # print("base_MxtoMxR: ", base_MxtoMxR, ", checked_MxtoMxR: ", checked_MxtoMxR, ", MxRtoMx: ", MxRtoMx, ", LLCxtoLLCxR: ", LLCxtoLLCxR, ", LLCxRtoLLCx: ", LLCxRtoLLCx)
        for x in range(4):
            for y in cache_slices_dist[x][1]: # adjacent slices
                # LLC request traffics
                if x < args.num_mains: # There are requests from core x to other slices
                    LLCxRtoLLCyR_matrix[x][y] += stats[benchmark]["LLCAcc_m" + str(x)] * (1/4 + 1/8) # 1/4 to slice y, 1/8 passing to the core diagonal to x
                if cache_slices_dist[y][2] < args.num_mains: # There are requests from core diagonal to y to slice y passing through x
                    LLCxRtoLLCyR_matrix[x][y] += stats[benchmark]["LLCAcc_m" + str(cache_slices_dist[y][2])] * 1/8 # 1/8 passing to slice y
                # LLC response traffics
                if y < args.num_mains: # There are demand traffic response to core y
                    LLCxRtoLLCyR_matrix[x][y] += stats[benchmark]["LLCRESP_m" + str(y)] * (1/4 + 1/8) # 1/4 from slice x to core y, 1/8 from the slice diagonal to y to core y passing through x
                if cache_slices_dist[x][2] < args.num_mains: # There are demand traffic response to core diaconal to x
                    LLCxRtoLLCyR_matrix[x][y] += stats[benchmark]["LLCRESP_m" + str(cache_slices_dist[x][2])] * 1/8 # 1/8 from slice x to core diagonal from x passing through core y
        # print("LLCxRtoLLCyR_matrix: ")
        # pprint.pp(LLCxRtoLLCyR_matrix)
        base_LLCxR = [LLCxtoLLCxR[i] + sum([LLCxRtoLLCyR_matrix[j][i] for j in cache_slices_dist[i][1]]) for i in range(4)]
        checked_LLCxR = [LLCxtoLLCxR[i] + sum([LLCxRtoLLCyR_matrix[j][i] for j in cache_slices_dist[i][1]]) for i in range(4)]
        if args.op in ['4x4i']: # MxR is local LLCxR
            # Calculate traffic going through routers 
            base_MxR = []
            checked_MxR = []
            for i in range(args.num_mains): # LLCxR has main core attached
                base_LLCxR[i] += base_MxtoMxR[i]
                checked_LLCxR[i] += checked_MxtoMxR[i]
                base_MxR.append(base_LLCxR[i])
                checked_MxR.append(checked_LLCxR[i])
            # print("Added config specific traffic:") 
            # print("base_MxR: ", base_MxR, ",checked_MxR: ", checked_MxR, ", base_LLCxR: ", base_LLCxR, ", checked_LLCxR: ", checked_LLCxR)              
        else: # local LLCR is 1 hop away with 1 checker on it
            base_MxRtoLLCxR = base_MxtoMxR
            checked_MxRtoLLCxR = [stats[benchmark]["LLCAcc_m" + str(i)] + stats[benchmark]["LSLPKT_m" + str(i)]/args.num_checkers_per_main for i in range(args.num_mains)]
            LLCxRtoMxR = MxRtoMx
            # Calculate traffic going through routers
            base_MxR = [base_MxtoMxR[i] + LLCxRtoMxR[i] for i in range(args.num_mains)]
            checked_MxR = [checked_MxtoMxR[i] + LLCxRtoMxR[i] for i in range(args.num_mains)]
            for i in range(args.num_mains):
                base_LLCxR[i] += base_MxRtoLLCxR[i]
                checked_LLCxR[i] += checked_MxRtoLLCxR[i]
            # print("Added config specific traffic:")
            # print("base_MxRtoLLCxR: ", base_MxRtoLLCxR, ",checked_MxRtoLLCxR: ", checked_MxRtoLLCxR, ", LLCxRtoMxR: ", LLCxRtoMxR, ", base_MxR: ", base_MxR, ", checked_MxR: ", checked_MxR, ", base_LLCxR: ", base_LLCxR, ", checked_LLCxR: ", checked_LLCxR)
        # Populate each end to end traffic demand traffic routes with traffic per link and router
        for i in range(args.num_mains):
            stats[benchmark]["base_M" + str(i) + "toLLC" + str(i)] = [base_MxtoMxR[i], base_LLCxR[i], LLCxRtoLLCx[i]]
            stats[benchmark]["checked_M" + str(i) + "toLLC" + str(i)] = [checked_MxtoMxR[i], checked_LLCxR[i], LLCxRtoLLCx[i]]
            stats[benchmark]["base_LLC" + str(i) + "toM" + str(i)] = [LLCxtoLLCxR[i], base_LLCxR[i], MxRtoMx[i]]
            stats[benchmark]["checked_LLC" + str(i) + "toM" + str(i)] = [LLCxtoLLCxR[i], checked_LLCxR[i], MxRtoMx[i]]
            for j in cache_slices_dist[i][1]: # adjacent slices
                stats[benchmark]["base_M" + str(i) + "toLLC" + str(j)] = [base_MxtoMxR[i], base_LLCxR[i], LLCxRtoLLCyR_matrix[i][j], base_LLCxR[j], LLCxRtoLLCx[j]]
                stats[benchmark]["checked_M" + str(i) + "toLLC" + str(j)] = [checked_MxtoMxR[i], checked_LLCxR[i], LLCxRtoLLCyR_matrix[i][j], checked_LLCxR[j], LLCxRtoLLCx[j]]
                stats[benchmark]["base_LLC" + str(j) + "toM" + str(i)] = [LLCxtoLLCxR[j], base_LLCxR[j], LLCxRtoLLCyR_matrix[j][i], base_LLCxR[i], MxRtoMx[i]]
                stats[benchmark]["checked_LLC" + str(j) + "toM" + str(i)] = [LLCxtoLLCxR[j], checked_LLCxR[j], LLCxRtoLLCyR_matrix[j][i], checked_LLCxR[i], MxRtoMx[i]]
                k = cache_slices_dist[i][2] # diagonal slice
                stats[benchmark]["base_M" + str(i) + "toLLC" + str(k) + "viaLLC" + str(j)] = [base_MxtoMxR[i], base_LLCxR[i], LLCxRtoLLCyR_matrix[i][j], base_LLCxR[j], LLCxRtoLLCyR_matrix[j][k], base_LLCxR[k], LLCxRtoLLCx[k]]
                stats[benchmark]["checked_M" + str(i) + "toLLC" + str(k) + "viaLLC" + str(j)] = [checked_MxtoMxR[i], checked_LLCxR[i], LLCxRtoLLCyR_matrix[i][j], checked_LLCxR[j], LLCxRtoLLCyR_matrix[j][k], checked_LLCxR[k], LLCxRtoLLCx[k]]
                stats[benchmark]["base_LLC" + str(k) + "viaLLC" + str(j) + "toM" + str(i)] = [LLCxtoLLCxR[k], base_LLCxR[k], LLCxRtoLLCyR_matrix[k][j], base_LLCxR[j], LLCxRtoLLCyR_matrix[j][i], base_LLCxR[i], MxRtoMx[i]]
                stats[benchmark]["checked_LLC" + str(k) + "viaLLC" + str(j) + "toM" + str(i)] = [LLCxtoLLCxR[k], checked_LLCxR[k], LLCxRtoLLCyR_matrix[k][j], checked_LLCxR[j], LLCxRtoLLCyR_matrix[j][i], checked_LLCxR[i], MxRtoMx[i]]
            if args.op in ['4x4o']: # has an extra link and an extra router compared to 4x4i
                for traffic in traffics:
                    if "base_M" + str(i) in traffic:
                        stats[benchmark][traffic].extend([base_MxR[i], base_MxRtoLLCxR[i]])
                    elif "checked_M" + str(i) in traffic:
                        stats[benchmark][traffic].extend([checked_MxR[i], checked_MxRtoLLCxR[i]])
                    elif "base_LLC" in traffic and "toM" + str(i) in traffic:
                        stats[benchmark][traffic].extend([LLCxRtoMxR[i], base_MxR[i]])
                    elif "checked_LLC" in traffic and "toM" + str(i) in traffic:
                        stats[benchmark][traffic].extend([LLCxRtoMxR[i], checked_MxR[i]])
        # print("Added per route traffic:")
        # pprint.pp(stats)
        # Calculate the average delay of each end to end demand traffic route
        for traffic in traffics :
            total_cycles = stats[benchmark]["numCycles"]
            if "check" in traffic:
                total_cycles = stats[benchmark]["numCycles_checked"]
            stats[benchmark][traffic + "_avgDelay"] = math.fsum([1 / (service_rate - (pkts / total_cycles)) for pkts in stats[benchmark][traffic]])
            assert max(stats[benchmark][traffic])/total_cycles < service_rate, benchmark + ": " + traffic + " has too much packets (" + str(stats[benchmark][traffic]) + ") that the NoC cannot process in time (" + str(total_cycles) + " main core cycles with " + str(service_rate) + " service rate)."
        # Weighted round-trip delay and delta delay
        for i in range(args.num_mains):
            # 1/4 demand traffic to and from local slice
            stats[benchmark]["base_M" + str(i) + "_avgDelay"] = (stats[benchmark]["base_M" + str(i) + "toLLC" + str(i) + "_avgDelay"] + stats[benchmark]["base_LLC" + str(i) + "toM" + str(i) + "_avgDelay"]) / 4 
            stats[benchmark]["checked_M" + str(i) + "_avgDelay"] = (stats[benchmark]["checked_M" + str(i) + "toLLC" + str(i) + "_avgDelay"] + stats[benchmark]["checked_LLC" + str(i) + "toM" + str(i) + "_avgDelay"]) / 4 
            for j in cache_slices_dist[i][1]: # adjacent slices
                # 1/4 demand traffic to and from each of the adjacent slices
                stats[benchmark]["base_M" + str(i) + "_avgDelay"] += (stats[benchmark]["base_M" + str(i) + "toLLC" + str(j) + "_avgDelay"] + stats[benchmark]["base_LLC" + str(j) + "toM" + str(i) + "_avgDelay"]) / 4
                stats[benchmark]["checked_M" + str(i) + "_avgDelay"] += (stats[benchmark]["checked_M" + str(i) + "toLLC" + str(j) + "_avgDelay"] + stats[benchmark]["checked_LLC" + str(j) + "toM" + str(i) + "_avgDelay"]) / 4
                k = cache_slices_dist[i][2] # diagonal slice
                # 1/8 demand traffic to and from diagonal slice through each of the adjacent slices
                stats[benchmark]["base_M" + str(i) + "_avgDelay"] += (stats[benchmark]["base_M" + str(i) + "toLLC" + str(k) + "viaLLC" + str(j) + "_avgDelay"] + stats[benchmark]["base_LLC" + str(k) + "viaLLC" + str(j) + "toM" + str(i) + "_avgDelay"]) / 8
                stats[benchmark]["checked_M" + str(i) + "_avgDelay"] += (stats[benchmark]["checked_M" + str(i) + "toLLC" + str(k) + "viaLLC" + str(j) + "_avgDelay"] + stats[benchmark]["checked_LLC" + str(k) + "viaLLC" + str(j) + "toM" + str(i) + "_avgDelay"]) / 8
            # delta of weighted round-trip delay
            stats[benchmark]["delta_M" + str(i) + "_avgDelay"] = stats[benchmark]["checked_M" + str(i) + "_avgDelay"] - stats[benchmark]["base_M" + str(i) + "_avgDelay"]
        # Geomean of delay among cores
        stats[benchmark]["delta_avgDelay"] = math.ceil(math.pow(math.prod([stats[benchmark]["delta_M" + str(i) + "_avgDelay"] for i in range(args.num_mains)]), 1/args.num_mains)) # ceil of geomean of all cores
        # print("Added calculated delay:")
        # pprint.pp(stats)
    else:
        base_MtoMR = stats[benchmark]["LLCAcc"]  # Main to Main_router link
        check_MtoMR = stats[benchmark]["LLCAcc"] + stats[benchmark]["LSLPKT"]  # Main to Main_router link with LSL traffic
        MRtoM = stats[benchmark]["LLCRESP"] # Main_router to Main link, no difference between base and check since no LSL traffic
        base_MRtoLLCR = base_MtoMR # Main_router to LLC_router link
        LLCRtoMR = MRtoM # LLC_router to Main_router link
        base_MR = base_MtoMR + LLCRtoMR # Main_router has traffic from Main and LLC_router
        check_MR = check_MtoMR + LLCRtoMR # Main_router has traffic from Main and LLC_router
        if args.op in ['2M']: # Stats for the second main core
            base_M1toM1R = stats[benchmark]["LLCAcc_m1"]  # Main to Main_router link
            check_M1toM1R = stats[benchmark]["LLCAcc_m1"] + stats[benchmark]["LSLPKT_m1"]  # Main to Main_router link with LSL traffic
            M1RtoM1 = stats[benchmark]["LLCRESP_m1"] # Main_router to Main link, no difference between base and check since no LSL traffic
            base_M1RtoLLCR = base_M1toM1R # Main_router to LLC_router link
            LLCRtoM1R = M1RtoM1 # LLC_router to Main_router link
            base_M1R = base_M1toM1R + LLCRtoM1R # Main_router has traffic from Main and LLC_router
            check_M1R = check_M1toM1R + LLCRtoM1R # Main_router has traffic from Main and LLC_router
        if args.op in ['4M']: # Stats for the second main core onward
            base_MxtoMxR = [stats[benchmark]["LLCAcc_m" + str(i)] for i in range(args.num_mains)]  # Main to Main_router link
            check_MxtoMxR = [stats[benchmark]["LLCAcc_m" + str(i)] + stats[benchmark]["LSLPKT_m" + str(i)] for i in range(args.num_mains)]  # Main to Main_router link with LSL traffic
            MxRtoMx = [stats[benchmark]["LLCRESP_m" + str(i)] for i in range(args.num_mains)] # Main_router to Main link, no difference between base and check since no LSL traffic
            base_MxRtoLLCR = base_MxtoMxR # Main_router to LLC_router link
            LLCRtoMxR = MxRtoMx # LLC_router to Main_router link
            base_MxR = [base_MxtoMxR[i] + LLCRtoMxR[i] for i in range(args.num_mains)] # Main_router has traffic from Main and LLC_router
            check_MxR = [check_MxtoMxR[i] + LLCRtoMxR[i] for i in range(args.num_mains)] # Main_router has traffic from Main and LLC_router
        if args.op in ['1M2C','1M4C', '1M12C', '1M16C']:    
            if args.op in ['1M2C']:
                check_MRtoLLCR = base_MtoMR # Main_router to LLC_router link, no LSL traffic
            else:
                check_MRtoLLCR = base_MtoMR + (stats[benchmark]["LSLPKT"] / args.num_checkers_per_main) # Main_router to LLC_router link with LSL traffic
            # Stats for LLC
            LLCtoLLCR = LLCRtoMR  # LLC to LLC_router link
            LLCRtoLLC = base_MRtoLLCR  # LLC_router to LLC link
            base_LLCR = base_MRtoLLCR + LLCtoLLCR # LLC_router has traffic from Main_router and LLC
            check_LLCR = check_MRtoLLCR + LLCtoLLCR # LLC_router has traffic from Main_router and LLC
        elif args.op in ['2M']: # '2M'
            check_MRtoLLCR = base_MtoMR # Main_router to LLC_router link, no LSL traffic
            check_M1RtoLLCR = base_M1toM1R # Main_router to LLC_router link, no LSL traffic
            # Stats for LLC, has traffic to/from both cores
            LLCRtoLLC = base_MRtoLLCR + base_M1RtoLLCR  # LLC_router to LLC link
            LLCtoLLCR = LLCRtoMR + LLCRtoM1R # LLC to LLC_router link
            base_LLCR = base_MRtoLLCR + base_M1RtoLLCR + LLCtoLLCR # LLC_router has traffic from Main_router and LLC
            check_LLCR = check_MRtoLLCR + check_M1RtoLLCR + LLCtoLLCR # LLC_router has traffic from Main_router and LLC
        else: # '4M'
            check_MRtoLLCR = base_MtoMR # Main_router to LLC_router link, no LSL traffic
            check_MxRtoLLCR = base_MxtoMxR # Main_router to LLC_router link, no LSL traffic
            # Stats for LLC, has traffic to/from all cores
            LLCRtoLLC = sum(base_MxRtoLLCR)  # LLC_router to LLC link
            LLCtoLLCR = sum(LLCRtoMxR) # LLC to LLC_router link
            base_LLCR = sum(base_MxRtoLLCR) + LLCtoLLCR # LLC_router has traffic from Main_router and LLC
            check_LLCR = sum(check_MxRtoLLCR) + LLCtoLLCR # LLC_router has traffic from Main_router and LLC

        # Main to LLC traffic: [Main to Main_router link, Main_router, Main_router to LLC_router link, LLC_router, LLC_router to LLC link]
        # LLC to Main traffic: [Main_router to Main link, Main_router, LLC_router to Main_router link, LLC_router, LLC to LLC_router link]
        traffics = ["base_MtoLLC", "check_MtoLLC", "base_LLCtoM", "check_LLCtoM"]
        stats[benchmark]["base_MtoLLC"] = [base_MtoMR, base_MR, base_MRtoLLCR, base_LLCR, LLCRtoLLC]
        stats[benchmark]["check_MtoLLC"] = [check_MtoMR, check_MR, check_MRtoLLCR, check_LLCR, LLCRtoLLC]
        stats[benchmark]["base_LLCtoM"] = [MRtoM, base_MR, LLCRtoMR, base_LLCR, LLCtoLLCR]
        stats[benchmark]["check_LLCtoM"] = [MRtoM, check_MR, LLCRtoMR, check_LLCR, LLCtoLLCR]
        if args.op in ['2M']:
            traffics.extend(["base_M1toLLC", "check_M1toLLC", "base_LLCtoM1", "check_LLCtoM1"])
            stats[benchmark]["base_M1toLLC"] = [base_M1toM1R, base_M1R, base_M1RtoLLCR, base_LLCR, LLCRtoLLC]
            stats[benchmark]["check_M1toLLC"] = [check_M1toM1R, check_M1R, check_M1RtoLLCR, check_LLCR, LLCRtoLLC]
            stats[benchmark]["base_LLCtoM1"] = [M1RtoM1, base_M1R, LLCRtoM1R, base_LLCR, LLCtoLLCR]
            stats[benchmark]["check_LLCtoM1"] = [M1RtoM1, check_M1R, LLCRtoM1R, check_LLCR, LLCtoLLCR]
        if args.op in ['4M']:
            for i in range(args.num_mains):
                traffics.extend(["base_M" + str(i) + "toLLC", "check_M" + str(i) + "toLLC", "base_LLCtoM" + str(i), "check_LLCtoM" + str(i)])
                stats[benchmark]["base_M" + str(i) + "toLLC"] = [base_MxtoMxR[i], base_MxR[i], base_MxRtoLLCR[i], base_LLCR, LLCRtoLLC]
                stats[benchmark]["check_M" + str(i) + "toLLC"] = [check_MxtoMxR[i], check_MxR[i], check_MxRtoLLCR[i], check_LLCR, LLCRtoLLC]
                stats[benchmark]["base_LLCtoM" + str(i)] = [MxRtoMx[i], base_MxR[i], LLCRtoMxR[i], base_LLCR, LLCtoLLCR]
                stats[benchmark]["check_LLCtoM" + str(i)] = [MxRtoMx[i], check_MxR[i], LLCRtoMxR[i], check_LLCR, LLCtoLLCR]
        # Calculate the average delay from Main to LLC traffic and LLC to Main traffic
        for traffic in traffics :
            stats[benchmark][traffic + "_avgDelay"] = math.fsum([1 / (service_rate - (pkts / stats[benchmark]["numCycles"])) for pkts in stats[benchmark][traffic]])
            if "check" in traffic:
                stats[benchmark][traffic + "_avgDelay"] = math.fsum([1 / (service_rate - (pkts / stats[benchmark]["numCycles_checked"])) for pkts in stats[benchmark][traffic]])
        # Calculate the round-trip delay and delta delay
        stats[benchmark]["base_avgDelay"] = stats[benchmark]["base_MtoLLC_avgDelay"] + stats[benchmark]["base_LLCtoM_avgDelay"]
        stats[benchmark]["check_avgDelay"] = stats[benchmark]["check_MtoLLC_avgDelay"] + stats[benchmark]["check_LLCtoM_avgDelay"]
        if args.op in ['1M2C','1M4C', '1M12C', '1M16C']:
            stats[benchmark]["delta_avgDelay"] = math.ceil(stats[benchmark]["check_avgDelay"] - stats[benchmark]["base_avgDelay"])
        elif args.op in ['2M']: # '2M'
            stats[benchmark]["base_avgDelay_m1"] = stats[benchmark]["base_M1toLLC_avgDelay"] + stats[benchmark]["base_LLCtoM1_avgDelay"]
            stats[benchmark]["check_avgDelay_m1"] = stats[benchmark]["check_M1toLLC_avgDelay"] + stats[benchmark]["check_LLCtoM1_avgDelay"]
            stats[benchmark]["delta_avgDelay_m0"] = stats[benchmark]["check_avgDelay"] - stats[benchmark]["base_avgDelay"]
            stats[benchmark]["delta_avgDelay_m1"] = stats[benchmark]["check_avgDelay_m1"] - stats[benchmark]["base_avgDelay_m1"]
            stats[benchmark]["delta_avgDelay"] = math.ceil(math.pow(stats[benchmark]["delta_avgDelay_m0"] * stats[benchmark]["delta_avgDelay_m1"], 1/2)) # ceil of geomean of two cores
        else: # '4M'
            for i in range(args.num_mains):
                stats[benchmark]["base_avgDelay_m" + str(i)] = stats[benchmark]["base_M" + str(i) + "toLLC_avgDelay"] + stats[benchmark]["base_LLCtoM" + str(i) + "_avgDelay"]
                stats[benchmark]["check_avgDelay_m" + str(i)] = stats[benchmark]["check_M" + str(i) + "toLLC_avgDelay"] + stats[benchmark]["check_LLCtoM" + str(i) + "_avgDelay"]
                stats[benchmark]["delta_avgDelay_m" + str(i)] = stats[benchmark]["check_avgDelay_m" + str(i)] - stats[benchmark]["base_avgDelay_m" + str(i)]
            stats[benchmark]["delta_avgDelay"] = math.ceil(math.pow(math.prod([stats[benchmark]["delta_avgDelay_m" + str(i)] for i in range(args.num_mains)]), 1/args.num_mains)) # ceil of geomean of all cores

# Generate NocLat output file
for benchmark in benchmarks:
    if benchmark in ["bc", "bfs", "cc", "pr"]:
        print(benchmark + "_roi: " + str(stats[benchmark]["delta_avgDelay"]))
    else:
        print(benchmark + ": " + str(stats[benchmark]["delta_avgDelay"]))