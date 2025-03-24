import random
import argparse

parser = argparse.ArgumentParser(description='Generate random values for error injection.')
parser.add_argument('--num_vals', type=int,
                    help='Number of random value pairs to generate')
parser.add_argument('--add_to_file', type=str, action='store',
                    help='the filename of existing random errors to add to without duplication')

Num_OpClass = 54
Num_bits_reg = 64
Num_bits_vec = 2048
existing_rand_sets = dict()
args = parser.parse_args()
Num_randPairs = args.num_vals
if args.add_to_file:
    with open(args.add_to_file) as f:
        for line in f:
            errType = line.split(" ")[1]
            if errType not in existing_rand_sets:
                existing_rand_sets[errType] = set()
                startIDs[errType] = 1
            existing_rand_sets[errType].add(" ".join(line.split(" ")[2:]))

opClasses = ["No_OpClass", "Branch", "IntAlu", "IntMult", "IntDiv", "FloatAdd", "FloatCmp", "FloatCvt", "FloatMult", "FloatMultAcc", "FloatDiv", "FloatMisc", "FloatSqrt", "SimdAdd", "SimdAddAcc", "SimdAlu", "SimdCmp", "SimdCvt", "SimdMisc", "SimdMult", "SimdMultAcc", "SimdShift", "SimdShiftAcc", "SimdDiv", "SimdSqrt", "SimdReduceAdd", "SimdReduceAlu", "SimdReduceCmp", "SimdFloatAdd", "SimdFloatAlu", "SimdFloatCmp", "SimdFloatCvt", "SimdFloatDiv", "SimdFloatMisc", "SimdFloatMult", "SimdFloatMultAcc", "SimdFloatSqrt", "SimdFloatReduceCmp", "SimdFloatReduceAdd", "SimdAes", "SimdAesMix", "SimdSha1Hash", "SimdSha1Hash2", "SimdSha256Hash", "SimdSha256Hash2", "SimdShaSigma2", "SimdShaSigma3", "SimdPredAlu", "MemRead", "MemWrite", "FloatMemRead", "FloatMemWrite", "IprAccess", "InstPrefetch"]
opClasses_in_benchmarks = ["Branch", "IntAlu", "IntMult", "IntDiv", "FloatAdd", "FloatCmp", "FloatCvt", "FloatMult", "FloatMultAcc", "FloatDiv", "FloatMisc", "SimdAdd", "SimdAddAcc", "SimdAlu", "SimdCmp", "SimdCvt", "SimdMisc", "SimdShift", "SimdFloatAdd", "SimdFloatAlu", "SimdFloatCmp", "SimdFloatDiv", "SimdFloatMult", "SimdFloatMultAcc", "MemRead", "MemWrite"]
benchmarks = ["bwaves", "gcc", "mcf", "deepsjeng", "leela", "exchange2", "xz", "wrf", "cam4", "pop2", "imagick", "nab", "fotonik3d", "roms", "perlbench", "x264", "xalancbmk", "omnetpp", "cactuBSSN", "lbm"]
opClasses_per_benchmarks = dict()
opClasses_per_benchmarks["bwaves"] = ["Branch", "IntAlu", "IntMult", "IntDiv", "FloatAdd", "FloatMult", "FloatMultAcc", "FloatDiv", "FloatMisc", "MemRead", "MemWrite"]
opClasses_per_benchmarks["gcc"] = ["Branch", "IntAlu", "IntMult", "IntDiv", "FloatMisc", "SimdAdd", "SimdAlu", "SimdCmp", "SimdMisc", "MemRead", "MemWrite"]
opClasses_per_benchmarks["mcf"] = ["Branch", "IntAlu", "IntMult", "IntDiv", "MemRead", "MemWrite"]
opClasses_per_benchmarks["deepsjeng"] = ["Branch", "IntAlu", "IntMult", "IntDiv", "FloatMisc", "SimdAdd", "SimdAlu", "SimdMisc", "MemRead", "MemWrite"]
opClasses_per_benchmarks["leela"] = ["Branch", "IntAlu", "IntMult", "IntDiv", "FloatAdd", "FloatCmp", "FloatCvt", "FloatDiv", "FloatMisc", "SimdAdd", "SimdAlu", "SimdCmp", "SimdCvt", "SimdMisc", "MemRead", "MemWrite"]
opClasses_per_benchmarks["exchange2"] = ["Branch", "IntAlu", "IntMult", "SimdAdd", "SimdAlu", "SimdMisc", "MemRead", "MemWrite"]
opClasses_per_benchmarks["xz"] = ["Branch", "IntAlu", "SimdAdd", "SimdAlu", "SimdShift", "MemRead", "MemWrite"]
opClasses_per_benchmarks["wrf"] = ["Branch", "IntAlu", "IntMult", "FloatAdd", "FloatCmp", "FloatCvt", "FloatMult", "FloatMultAcc", "FloatDiv", "FloatMisc", "SimdAdd", "SimdAlu", "SimdCvt", "SimdMisc", "SimdFloatAdd", "SimdFloatAlu", "SimdFloatCmp", "SimdFloatDiv", "SimdFloatMult", "SimdFloatMultAcc", "MemRead", "MemWrite"]
opClasses_per_benchmarks["cam4"] = ["Branch", "IntAlu", "IntMult", "IntDiv", "FloatAdd", "FloatCmp", "FloatCvt", "FloatMult", "FloatMultAcc", "FloatDiv", "FloatMisc", "SimdAlu", "SimdMisc", "SimdFloatAdd", "SimdFloatAlu", "SimdFloatCmp", "SimdFloatDiv", "SimdFloatMult", "SimdFloatMultAcc", "MemRead", "MemWrite"]
opClasses_per_benchmarks["pop2"] = ["Branch", "IntAlu", "IntMult", "MemRead", "MemWrite"]
opClasses_per_benchmarks["imagick"] = ["Branch", "IntAlu", "IntMult", "FloatAdd", "FloatCmp", "FloatCvt", "FloatMult", "FloatDiv", "FloatMisc", "SimdCvt", "SimdMisc", "MemRead", "MemWrite"]
opClasses_per_benchmarks["nab"] = ["Branch", "IntAlu", "IntMult", "IntDiv", "FloatAdd", "FloatCmp", "FloatMult", "FloatMisc", "MemRead", "MemWrite"]
opClasses_per_benchmarks["fotonik3d"] = ["Branch", "IntAlu", "IntMult", "IntDiv", "SimdMisc", "MemRead", "MemWrite"]
opClasses_per_benchmarks["roms"] = ["Branch", "IntAlu", "FloatAdd", "FloatCvt", "FloatMult", "FloatMultAcc", "FloatDiv", "FloatMisc", "MemRead", "MemWrite"]
opClasses_per_benchmarks["perlbench"] = ["Branch", "IntAlu", "IntMult", "MemRead", "MemWrite"]
opClasses_per_benchmarks["x264"] = ["Branch", "IntAlu", "IntMult", "FloatMisc", "SimdAdd", "SimdAddAcc", "SimdAlu", "SimdMisc", "MemRead", "MemWrite"]
opClasses_per_benchmarks["xalancbmk"] = ["Branch", "IntAlu", "IntMult", "IntDiv", "FloatMisc", "SimdMisc", "MemRead", "MemWrite"]
opClasses_per_benchmarks["omnetpp"] = ["Branch", "IntAlu", "IntMult", "FloatCmp", "FloatCvt", "FloatMult", "FloatMisc", "SimdAlu", "SimdCmp", "SimdCvt", "SimdMisc", "MemRead", "MemWrite"]
opClasses_per_benchmarks["cactuBSSN"] = ["Branch", "IntAlu", "FloatAdd", "FloatCvt", "FloatMult", "FloatMultAcc", "FloatDiv", "FloatMisc", "SimdMisc", "MemRead", "MemWrite"]
opClasses_per_benchmarks["lbm"] = ["Branch", "IntAlu", "FloatAdd", "FloatMult", "FloatDiv", "FloatMisc", "MemRead", "MemWrite"]

# bench_op_dict = dict()
# for bench in benchmarks:
#     bench_op_dict[bench] = dict()
#     for opClass in opClasses:
#         bench_op_dict[bench][opClass] = 0

# with open("/local/scratch/ml2076/gem5-paramedox_m5out/m5out_10000000/tmp", "r") as f:
#     for line in f:
#         for i,opclass in enumerate(opClasses):
#             # if i == 0:
#             #     continue
#             # print("grep \"switch_cpus.commit.committedInstType_0::" + opclass + " * [1-9].*#\" *baseline*/stats.txt >> tmp")
#             for bench in benchmarks:
#                 if bench in line and opclass in line:
#                     bench_op_dict[bench][opclass] = 1
# # lines = [["opClass"]]
# # for benchmark in benchmarks:
# #     lines[0].append(benchmark)
# # for opclass in opClasses:
# #     lines.append([opclass])
# #     for bench in benchmarks:
#         # lines[-1].append(str(bench_op_dict[bench][opclass]))
# # print("\n".join([" ".join(line) for line in lines]))
# lines = []
# for bench in benchmarks:
#     lines.append([bench])
#     for opclass in opClasses:
#         if bench_op_dict[bench][opclass] == 1:
#             lines[-1].append(opclass)
# print("\n".join([" ".join(line) for line in lines]))        

def gen_randOpClass(include_list, exclude_list):
    opClass = 0
    while opClasses[opClass] not in include_list or opClasses[opClass] in exclude_list:
        opClass = random.randint(0, len(opClasses) - 1)
    return opClass  

def gen_randBitId(nbits): 
    return random.randint(0, nbits - 1)

def gen_randStuckAtBit():
    return random.randint(0,1)

def get_randOpBitPair(include_list=opClasses_in_benchmarks, exclude_list=["Branchd"]):
    opClass = gen_randOpClass(include_list, exclude_list)
    vecRegOpList = range(5, 48)
    if opClass in vecRegOpList:
        bitId = gen_randBitId(Num_bits_vec)
    else:
        bitId = gen_randBitId(Num_bits_reg)
    # opClass 2 has multiple destination regs, 3 control regs (but not always)
    if opClass == 2:
        # most errors in reg 0, some in one of the 3 control regs
        rand_val = random.randint(0,3) # 0 for int, 1-3 for CC 
        if rand_val > 0: # 1 for first CC reg, 2 for second, 3 for third
            # Add offset to be able to easily get opClass back with mod but still determine which reg to insert error
            opClass = opClass + rand_val*(len(opClasses))
    # opClass 6 has multiple destination regs, 4 control regs for some instructions, 1 vec reg for some instructions
    elif opClass == 6:
        rand_val = random.randint(0,5) # 0 for misc, 1-4 for CC, 5 for vec
        opClass = opClass + rand_val*(len(opClasses))
        if rand_val != 5:
            bitId = gen_randBitId(Num_bits_reg) # reg types are not vec reg except 1 vec reg case
    # opClass has multiple destination regs, 1 misc 1 vec
    elif opClass in [5,8,9,10,25,26,27,29,31,32]:
        rand_val = random.randint(0,1) # 0 for misc, 1 for vec
        opClass = opClass + rand_val*(len(opClasses))
        if rand_val == 0:
            bitId = gen_randBitId(Num_bits_reg) # reg types are not vec reg except 1 vec reg case
    # opClass has multiple destination regs, misc, vec or int
    elif opClass in [7,11]:
        rand_val = random.randint(0,2) # 0 for misc, 1 for vec, 2 for int
        opClass = opClass + rand_val*(len(opClasses))
        if rand_val == 2:
            bitId = gen_randBitId(Num_bits_reg) # reg type is not vec reg
    # opClass has multiple types of destination regs, vec or int
    elif opClass in [18]:
        rand_val = random.randint(1,2) # 1 for vec, 2 for int
        opClass = opClass + rand_val*(len(opClasses))
        if rand_val == 2:
            bitId = gen_randBitId(Num_bits_reg) # reg type is not vec reg
    return opClass, bitId

def count_perBenchOp(perBenchOpCount, newOp):
    enough = True
    for bench in benchmarks:
        if newOp in opClasses_per_benchmarks[bench]:
            perBenchOpCount[bench] += 1
        if perBenchOpCount[bench] < Num_randPairs:
            enough = False
    return enough

rand_set = set()
for i in range(Num_randPairs):
#     # Excluding branch, doesn't have destination register in some benchmarks
#     # Excluding memread/write for now, lsl probably a better place to insert error on those
#     opClass, bitId = get_randOpBitPair(["Branch", "IntAlu", "IntMult", "MemRead", "MemWrite"],["Branch", "MemRead", "MemWrite"])
#     opClass, bitId = get_randOpBitPair(opClasses_in_benchmarks,["Branch", "IntAlu", "IntMult", "MemRead", "MemWrite"])
    opClass, bitId = get_randOpBitPair(["MemRead", "MemWrite"],[])
    errStr = str(opClass) + " " + str(bitId) + " " + str(gen_randStuckAtBit()) + " (" + str(opClass//Num_OpClass) + "/" + str(opClass%Num_OpClass) + ")"
    if errStr not in existing_rand_sets["FUdest"]:
        rand_set.add(errStr)
startIDs = dict()
if "FUdest" not in existing_rand_sets:
    startIDs["FUdest"] = 1
else:
    startIDs["FUdest"] = len(existing_rand_sets["FUdest"]) + 1
print("\n".join([str(i+startIDs["FUdest"]) + ": FUdest " + rand_vals for i,rand_vals in enumerate(rand_set)]))

# perBenchOpCount = dict()
# for bench in benchmarks:
#     perBenchOpCount[bench] = 0
# while not count_perBenchOp(perBenchOpCount, opClasses[opClass]):
#     opClass, bitId = get_randOpBitPair()
#     rand_str = str(opClass) + " " + str(bitId) + " " + str(gen_randStuckAtBit())
#     rand_list.append(rand_str)
# print("\n".join([str(i) + ": FUdest " + rand_vals for i,rand_vals in enumerate(rand_list)]))

# for i in range(1, Num_randPairs + 1):
#     opClass, bitId = get_randOpBitPair()
#     print(str(i) + ": FUdest " + str(opClass) + " " + str(bitId) + " " + str(gen_randStuckAtBit()))