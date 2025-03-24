#-----------------------------------------------------------------------
#                Paradox little core from
# ParaDox: Eliminating Voltage Margins via Heterogeneous Fault Tolerance
# HPCA 2021
#-----------------------------------------------------------------------

from m5.objects import *



class DSN18MinorCPU(ArmMinorCPU):
    # Inherit the doc string from the module to avoid repeating it
    # here.
    __doc__ = __doc__

    fetch1FetchLimit = 1
    decodeInputWidth = 1
    executeCommitLimit = 1
    executeInputWidth = 1
    executeIssueLimit = 1
    executeMaxAccessesInMemory = 1
    executeLSQMaxStoreBufferStoresPerCycle = 1
    executeLSQTransfersQueueSize = 1
    executeLSQStoreBufferSize = 1

class DSN18MinorCPU_ICache(Cache):
    data_latency = 1
    tag_latency = 1
    response_latency = 1
    mshrs = 6
    tgts_per_mshr = 8
    size = '2kB'
    assoc = 2
    # No prefetcher, this is handled by the core

class DSN18MinorCPU_DCache(Cache):
    data_latency = 1
    tag_latency = 1
    response_latency = 1
    mshrs = 6
    tgts_per_mshr = 8
    size = '2kB'
    assoc = 4
    write_buffers = 4

class DSN18MinorCPU_L2(Cache):
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
