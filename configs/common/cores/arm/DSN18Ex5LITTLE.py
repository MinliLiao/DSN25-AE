#-----------------------------------------------------------------------
#                Paradox little core from
# ParaDox: Eliminating Voltage Margins via Heterogeneous Fault Tolerance
# HPCA 2021
#-----------------------------------------------------------------------

from m5.objects import *

#-----------------------------------------------------------------------
#                ex5 LITTLE core (based on the ARM Cortex-A7)
#-----------------------------------------------------------------------

# Simple ALU Instructions have a latency of 3
class DSN18Ex5LITTLE_Simple_Int(MinorDefaultIntFU):
    opList = [ OpDesc(opClass='IntAlu', opLat=4) ]

# Complex ALU instructions have a variable latencies
class DSN18Ex5LITTLE_Complex_IntMul(MinorDefaultIntMulFU):
    opList = [ OpDesc(opClass='IntMult', opLat=7) ]

class DSN18Ex5LITTLE_Complex_IntDiv(MinorDefaultIntDivFU):
    opList = [ OpDesc(opClass='IntDiv', opLat=9) ]

# Floating point and SIMD instructions
class DSN18Ex5LITTLE_FP(MinorDefaultFloatSimdFU):
    opList = [ OpDesc(opClass='SimdAdd', opLat=6),
               OpDesc(opClass='SimdAddAcc', opLat=4),
               OpDesc(opClass='SimdAlu', opLat=4),
               OpDesc(opClass='SimdCmp', opLat=1),
               OpDesc(opClass='SimdCvt', opLat=3),
               OpDesc(opClass='SimdMisc', opLat=3),
               OpDesc(opClass='SimdMult',opLat=4),
               OpDesc(opClass='SimdMultAcc',opLat=5),
               OpDesc(opClass='SimdShift',opLat=3),
               OpDesc(opClass='SimdShiftAcc', opLat=3),
               OpDesc(opClass='SimdSqrt', opLat=9),
               OpDesc(opClass='SimdFloatAdd',opLat=8),
               OpDesc(opClass='SimdFloatAlu',opLat=6),
               OpDesc(opClass='SimdFloatCmp', opLat=6),
               OpDesc(opClass='SimdFloatCvt', opLat=6),
               OpDesc(opClass='SimdFloatDiv', opLat=20, pipelined=False),
               OpDesc(opClass='SimdFloatMisc', opLat=6),
               OpDesc(opClass='SimdFloatMult', opLat=15),
               OpDesc(opClass='SimdFloatMultAcc',opLat=6),
               OpDesc(opClass='SimdFloatSqrt', opLat=17),
               OpDesc(opClass='FloatAdd', opLat=8),
               OpDesc(opClass='FloatCmp', opLat=6),
               OpDesc(opClass='FloatCvt', opLat=6),
               OpDesc(opClass='FloatDiv', opLat=15, pipelined=False),
               OpDesc(opClass='FloatSqrt', opLat=33),
               OpDesc(opClass='FloatMult', opLat=6) ]

# Load/Store Units
class DSN18Ex5LITTLE_MemFU(MinorDefaultMemFU):
    opList = [ OpDesc(opClass='MemRead',opLat=1),
               OpDesc(opClass='MemWrite',opLat=1) ]

# Misc Unit
class DSN18Ex5LITTLE_MiscFU(MinorDefaultMiscFU):
    opList = [ OpDesc(opClass='IprAccess',opLat=1),
               OpDesc(opClass='InstPrefetch',opLat=1) ]

# Functional Units for this CPU
class DSN18Ex5LITTLE_FUP(MinorFUPool):
    funcUnits = [DSN18Ex5LITTLE_Simple_Int(), DSN18Ex5LITTLE_Simple_Int(),
        DSN18Ex5LITTLE_Complex_IntMul(), DSN18Ex5LITTLE_Complex_IntDiv(),
        DSN18Ex5LITTLE_FP(), DSN18Ex5LITTLE_MemFU(),
        DSN18Ex5LITTLE_MiscFU()]

class DSN18Ex5LITTLE(ArmMinorCPU):
    # Inherit the doc string from the module to avoid repeating it
    # here.
    # __doc__ = __doc__

    executeFuncUnits = DSN18Ex5LITTLE_FUP()

    fetch1FetchLimit = 1
    decodeInputWidth = 1
    executeCommitLimit = 1
    executeInputWidth = 1
    executeIssueLimit = 1
    executeMaxAccessesInMemory = 1
    executeLSQMaxStoreBufferStoresPerCycle = 1
    executeLSQTransfersQueueSize = 1
    executeLSQStoreBufferSize = 1

class DSN18Ex5LITTLE_ICache(Cache):
    data_latency = 1
    tag_latency = 1
    response_latency = 1
    mshrs = 6
    tgts_per_mshr = 8
    size = '2kB'
    assoc = 2
    # No prefetcher, this is handled by the core

class DSN18Ex5LITTLE_DCache(Cache):
    data_latency = 1
    tag_latency = 1
    response_latency = 1
    mshrs = 6
    tgts_per_mshr = 8
    size = '2kB'
    assoc = 4
    write_buffers = 4

class DSN18Ex5LITTLE_L2(Cache):
    data_latency = 2
    tag_latency = 2
    response_latency = 2
    mshrs = 6
    tgts_per_mshr = 8
    size = '16kB'
    assoc = 4
    write_buffers = 4
    # Simple stride prefetcher
    prefetch_on_access = True
    prefetcher = StridePrefetcher(degree=1, latency = 1)
    tags = BaseSetAssoc()
    replacement_policy = RandomRP()
