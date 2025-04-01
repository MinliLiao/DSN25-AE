 /* Authors: Sam Ainsworth
 *          Lionel Zoubritzky
 */


#ifndef __LOADSTORELOGENTRY_HH__
#define __LOADSTORELOGENTRY_HH__

#include <algorithm>
#include <fstream>
#include <queue>
#include <vector>

#include "arch/isa.hh"
#include "cpu/base.hh"
#include "cpu/minor/cpu.hh"
#include "cpu/o3/regfile.hh"
#include "cpu/thread_context.hh"
#include "mem/cache/base.hh"
#include "mem/packet.hh"
#include "mem/request.hh"

#define logsize (loadstorelogentry::lslSize) // 4096 for paraverser, 384 for paradox size
#define TIMEOUT (loadstorelogentry::cptTimeout) // 5000 for paraverser, static timeout or initial and max timeout with AIMD
#define AIMD (loadstorelogentry::timeoutAIMD) // true for additive increase multiplicative decrease auto adjustment of dynamic timeout
#define STORESIZE loadstorelogentry::actualstoresize  //3 for recovery.
#define AUTOCOMMIT (loadstorelogentry::actualstoresize != 3)


#define STARTVOLTAGE 1.1
#define BASEVOLTAGE 1.1
#define AIMDSCALE 0.75
#define AIMDDIFF 0.0001

#define END_ON_THREAD_EXIT 0

#define SIZEOFLOCKEDMEMSEGMENT 100
#define LOCKEDMEMLOGSIZE (NUMBEROFCHECKERCORESPERCORE*SIZEOFLOCKEDMEMSEGMENT)

#define NUMBEROFHISTOENTRIES 500

#define NUMBEROFAIMDHISTOENTRIES ((TIMEOUT/10))

#define BIGBUCKETLIMIT 20480000

#ifndef PARAGLIDER
#define PARAGLIDER 0
#define PARADVFS 0
#define LOGERRORS 0
#define FULL_ROLLBACK 1
#endif


#define NUMBEROFMAINCORES (loadstorelogentry::actualnumberofmaincores)
#define NUMBEROFCHECKERCORESPERCORE (loadstorelogentry::numCheckers())


namespace gem5
{



using namespace TheISA;

struct lockAddressBufferEntry
{
    Addr requestPC;
    Addr address;
    bool visited;

    lockAddressBufferEntry(Addr pc, Addr addr) : requestPC(pc), address(addr), visited(false) {}
    lockAddressBufferEntry() : requestPC(0), address(0), visited(true) {}
};

struct histoEntry
{
    uint64_t number;
    uint64_t mean;
    uint64_t minimum;
    uint64_t maximum;

    void add_entry(uint64_t time) {
        mean += time;
        number++;

        if (time<minimum) minimum = time;
        if (time>maximum) maximum = time;

    }

    static histoEntry merge (histoEntry entry1, histoEntry entry2) {
        histoEntry nh;
        nh.number = entry1.number + entry2.number;
        nh.maximum = (entry2.maximum > entry1.maximum) ? entry2.maximum : entry1.maximum;
        nh.minimum = (entry2.minimum > entry1.minimum) ? entry1.minimum : entry2.minimum;
        nh.mean = entry1.mean + entry2.mean;

        return nh;
    }

    histoEntry() {
        number = mean = maximum = 0;
        minimum = (uint64_t)-1;
    }
};


struct miniContext
{

   std::vector<RegVal> vecRegs;
   std::vector<RegVal> intRegs;

    std::vector<RegVal> ccRegs;


    std::vector<RegVal> miscRegs;

    std::vector<TheISA::VecRegContainer> vc;

    TheISA::PCState pcState;

    RegVal CPSR;

    bool initialized;
    bool checked;
    bool set;

    miniContext() : vecRegs(NumVecRegs * NumVecElemPerVecReg), intRegs(int_reg::NumRegs), ccRegs(cc_reg::NumRegs),miscRegs(NUM_MISCREGS), vc(NumVecRegs,TheISA::VecRegContainer()){
        initialized = false;
        checked = true;
        set = false;


        
    }

};

miniContext m_serialize(ThreadContext *tc);

bool m_identical(ThreadContext *tc, miniContext m);

void m_copyRegs(ThreadContext *tc, miniContext m);


namespace errordetection {
    void print_times();
    void detectError(int id);
    void detectErrorCommit(int id);

    extern uint64_t numberOfDetectedErroneousWrites;
        extern uint64_t numberOfDetectedErroneousReads;
        extern uint64_t numberOfDetectedErroneousArchStates;
        extern uint64_t numberOfCorrectCheckpoints;
#if LOGERRORS
        extern std::vector<uint64_t> memoryRecoveryTime;
        extern std::vector<uint64_t> voltageSwitchTime;
        extern std::vector<double> voltageSwitchPoints;

#endif
                extern uint64_t detected_errors;
        extern uint64_t total_memory_recoveries;

        extern uint64_t min_memory_recoveries;
        extern uint64_t max_memory_recoveries;

        extern uint64_t total_rollback_recovery;

        extern uint64_t min_rollback_recovery;
        extern uint64_t max_rollback_recovery;

        extern std::vector<bool> voltage_reset;//[NUMBEROFMAINCORES];
}



class loadstorelogentry
{

    public:
    
    struct AllCPUMeta
    {
            BaseCPU* baseCPU;
    };
    
    struct MainCPUMeta
    {
        int timeout;
        int current_segment_to_fill;//[NUMBEROFMAINCORES];
        int current_entry = 0;//[NUMBEROFMAINCORES];
        int current_size = 0;//[NUMBEROFMAINCORES];
	bool waiting_to_checkpoint = false;//[NUMBEROFMAINCORES];
	bool ready = false;//[NUMBEROFMAINCORES];
        uint64_t timestamp = 1;//[NUMBEROFMAINCORES];
        uint64_t committed_timestamp = 0;//[NUMBEROFMAINCORES];
        // List of timestamps that committed before a previous timestamp
        // (when out of order checker core checkpoint commit is allowed with minorCommitBypass) 
        std::map<uint64_t, miniContext> earlyCommittedTimestamps;

        miniContext committed_context = miniContext();//[NUMBEROFMAINCORES];
        miniContext previousThreadContext = miniContext();//[NUMBEROFMAINCORES];
        bool mainCoreErroneous = false;
        
        double voltage = STARTVOLTAGE;
        double highestVoltageError = 0;
        double highestRecentVoltageError = 0;
        
        int lastChecker = 0;//[NUMBEROFMAINCORES];
        
        uint64_t numberOfEntries = 0;//[NUMBEROFMAINCORES];
        uint64_t numberOfReads = 0;//[NUMBEROFMAINCORES];

        // The loadstorelog sequence number associated with the instructon
        // recorded in the first entry of the current segment
        int64_t startingSeqNum = 1;
                
        uint64_t startingTickTmp = 0;//[NUMBEROFMAINCORES];
        uint64_t errorTick = 0;//[NUMBEROFMAINCORES];
                

        std::vector<uint64_t> should_cache_wait;//[NUMBEROFMAINCORES][NUMBEROFMAINCORES];
        
        MainCPUMeta(): timeout(TIMEOUT), should_cache_wait(1,0) {}
    };
   
    
    struct CheckerCPUMeta
    {
        int entryIndices = 0;
        uint64_t timestamps = 1;
        uint64_t committedInstructions = 0;
        uint64_t currentCommittedInstructions = 0;
        // Number of cycles where the checker core couldn't commit at least 1 
        // instruction due to the limit on currentCommittedInstructions
        uint64_t checkerLSLStallCycles = 0;
        // Tick when the main core starts to fill in the checkpoint segment
        uint64_t mainStartingTick = 0;
        // Tick when the checker core first wakes up to check
        uint64_t checkerStartWakeupTick = 0;
        // Tick when the checker core first fetch after wake up
        uint64_t checkerStartFetchTick = 0;
        // Tick when the checker core finishes cache access for the first fetch
        uint64_t checkerStartFetchAccCompleteTick = 0;
        // Tick when the checker core starts to commit
        uint64_t checkerStartCommitTick = 0;
        // Tick when the checker core commits the last instruction in the
        // checkpoint segment
        uint64_t checkerLastCommitTick = 0;
        // Tick when the checker core finished draining the pipeline after 
        // completing a checkpoint segment. Not reset between segments, holds 
        // the value for the previous checkpoint segment before the first wake 
        // up, then reset to 0
        uint64_t checkerDrainDoneTick = 0; // Set with first signalDrainDone
        bool segmentFree = true;
        bool readyToCommit = false;
        bool interrupted = false;
        bool erroneous = false;
        
        bool copyingRegister = false; // Whether the checker core is switching to a checkpoint already stored in the extra slots 
        bool activeChecker = false;//[NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE];
        bool aboutToValidate = false;//[NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE];
        bool hasSyscall = false;//[NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE];
        uint64_t checkpoint_cachelines = 0;//[NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE];
        uint64_t checkpoint_entries = 0;//[NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE];
        
        // The loadstorelog sequence number associated with the instructon
        // recorded in the first entry of the current segment
        int64_t startingSeqNum = 1;

        uint64_t startingTick = 0;//[NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE];
        uint64_t startingCheckTick = 0;//[NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE];
        uint64_t expectedSetCommittedInsts = 0;
        
        miniContext startingContext = miniContext();
        miniContext expectedFinalContext= miniContext();
        
        std::vector<loadstorelogentry> entries;
        
         /* When the data size requested by the checker is different from that
         * logged, it means an instruction from the main core has been split
         * into several for the minor cores. This will be referred to as a midop.
         * The address offset for the next lookup corresponding to the same data
         * is stored in dataAddressOffset.
         * midopEntryIndex stores the entry index of the data corresponding to
         * this delayed lookup, as some regular lookups may happen in between.
         */
        Addr dataAddressOffset = 0;//[NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE]
        int midopEntryIndex = 0;//[NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE];
        
        /* Hash calculated from the main core run */
        std::array<uint64_t,8> expectedHash = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
        /* Hash calcualted from the checker core run, should match expectedHash */
        std::array<uint64_t,8> hash = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
        /* Chunk of 512-bit data used by each iteration of SHA-256, in 8*8-byte words */
        std::array<uint64_t,8> expectedHash_chunk = {0,0,0,0,0,0,0,0};
        std::array<uint64_t,8> hash_chunk = {0,0,0,0,0,0,0,0};
        /* Index for the next 8-byte word to fill in the chunk of data used for next iteration of hash */
        uint64_t expectedHash_chunk_index = 0;
        uint64_t hash_chunk_index = 0;
        /* Constants used in the SHA-256 hash */
        std::array<uint64_t,64> hash_k = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        /* Temporary entry used for the checker to merge micro-ops */
        loadstorelogentry * last_entry = nullptr;
        /* The total size of the hashed message in number of bits */
        int expectedHash_message_size = 0;
        int hash_message_size = 0;
        
        CheckerCPUMeta() : entries(logsize) {}
        /** Calculate the expected hash from the main core execution
         *  Input is the index to LSL entry that is being written
         */
        void calcExpectedHash(int current_entry);
        /** Calculate the hash from the checker core execution
         *  Input is the content of the committed instruction in form of loadstorelog entry
        */
        void calcCheckedHash(loadstorelogentry l);
        /** Calculate the hash
         *  Input is either the "expectedHash" for main core execution, or 
         *  "hash" for the checker core execution
        */
        void calcHash(std::array<uint64_t, 8>& in_hash);
        /** Initialize the expectedHash_chunk and expectedHash_chunk_index,  
         *  prepare for the next iteration of expectedHash calculation
        */
        void initExpectedHashCalc();
        /** Initialize the hash_chunk and hash_chunk_index, prepare for 
         *  the next iteration of hash calculation
        */
        void initHashCalc();
        /** Initialize all hash structures, prepare for next segment of hash */
        void initHash();
        
    };
    
        static std::vector<AllCPUMeta> allCPUMeta;    
        static std::vector<MainCPUMeta> mainCPUMeta;
        static std::vector<CheckerCPUMeta> checkerCPUMeta;
        // Keep track of the end address of the last access in a macro op
        // Next micro op access should start at this address if accesses are consecutive
        static std::vector<Addr> last_macro_addrs; 
        static std::vector<std::map<std::string, std::array<int,2>>> mainStaticInstsChecked;
        static std::vector<std::map<uint64_t, std::array<int,2>>> mainPCsChecked;
        static std::map<uint64_t, std::string> mainPCStaticInstsMap;
        
        static std::vector<uint64_t> blocked_lines;//[513];  //number of l1 cache lines + 1
private:
        static int actualnumberofcheckercores;
public:
        static int actualnumberofmaincores;

        static int actualstoresize;

        static bool timeoutAIMD;
        static int cptTimeout;
        static int lslSize;
        static bool minorCommitBypass;
        static int num_checkSlot_per_checker;
        static bool useHash;
        
        static int numCheckers() {
         return actualnumberofcheckercores;
        }
    
    	static void initCoreCount(int mains, int checkers, int extra_slot_per_checker, bool hashed, std::vector<double> errRates);
        static void initTimeout(bool AIMDoff, int in_cptTimeout, int in_lslSize);
        static void initMinorCommitBypass(bool in_minorCommitBypass);
    	static void initCacheSets(int l1sets);
        bool load;
        bool isSC;
        Addr addr;
        std::vector<uint8_t> data;
        std::vector<uint8_t> oldData;
        uint64_t flags;
        uint64_t extra_data;
        bool valid;
        uint64_t time;
        uint64_t pc;
        uint64_t microPC;
        std::string inst_name;
        // Unique sequence number for each macro-op accessing the loadstorelog,
        // -1 is invalid
        int64_t seqNum;

        static std::vector<histoEntry> histoEntries;
        static histoEntry bigBucket;

        static uint64_t maxHistoSize;
        // Histogram for checkpoint length in ticks
        static std::vector<histoEntry> cptLenHistoEntries;
        static histoEntry cptLenBigBucket;
        static uint64_t cptLenMaxHistoSize;
        // Histogram for checker checkpoint length in cycles
        static std::vector<histoEntry> checkerCptLenHistoEntries;
        static histoEntry checkerCptLenBigBucket;
        static uint64_t checkerCptLenMaxHistoSize;
        // Histogram for checkpoints' first fetch to commit delay in cycles
        static std::vector<histoEntry> cptFirstFetchToCommitDelayHistoEntries;
        static histoEntry cptFirstFetchToCommitDelayBigBucket;
        static uint64_t cptFirstFetchToCommitDelayMaxHistoSize;

        static std::vector<uint64_t> aimdHistoentries;
        // Histogram of the checkpoint size in number of committed instructions
        static std::vector<uint64_t> cptLengthHistoentries;
        // Histogram of number of committed instructions before the first LSL
        // entry in each checkpointing segment
        static std::vector<uint64_t> lengthToFirstLSLHistoentries;
        // Histogram of number of committed instructions after the last LSL 
        // entry in each checkpointing segment
        static std::vector<uint64_t> lengthFromLastLSLHistoentries;
        
        static uint64_t meanTime;
        static uint64_t maxTime;
        static uint64_t minTime;
        static uint64_t times;
        static uint64_t totalCommittedInstructions;
        static uint64_t checkedCommittedInstructions;
        // The number of instructions main core committed when the checker core
        // starts to check
        static uint64_t checkStartDelayInstructions;
        // The number of instructions on checker cores that still needs to be 
        // checked when the main core sets the final context
        static uint64_t checkDelayCommittedInstructions;
        // The number of ticks between the main starting to fill the new 
        // segement and the checker first wakes up in the new segment
        static uint64_t cptStartDelayTicks;
        // The number of ticks between the main starting to fill the new 
        // segement and the main core takes the checkpoint
        static uint64_t cptLenTicks;
        // The number of ticks between the checker first wakes up in the new 
        // segement and the checker starting to fetch new instructions
        static uint64_t cptCheckerStartToFetchDelayTicks;
        // The number of ticks between the checker starting to fetch new 
        // instructions and the checker finishes icache access in fetch
        static uint64_t cptCheckerFirstFetchTransAccDelayTicks;
        // The number of ticks between the checker starting to fetch new 
        // instructions and the checker starting to commit new instructions
        static uint64_t cptCheckerFirstFetchToCommitDelayTicks;
        // The number of ticks between the checker first wakes up in the new 
        // segement and the checker starting to commit new instructions
        static uint64_t cptCheckerStartToCommitDelayTicks;
        // The number of ticks between the checker starting to commit new 
        // instructions and the checker commits the last instruction
        static uint64_t cptCheckerFirstToLastCommitDelayTicks;
        // The number of ticks between the checker commits the last instruction
        // and the checker finishes draining
        static uint64_t cptCheckerLastCommitToDrainDoneDelayTicks;
        // The number of ticks between the checker finishes draining and
        // starting to check on the next segment (idle waiting for work)
        static uint64_t cptCheckerDrainDoneToStartDelayTicks;
        // The number of cycles that the main core stalls due to taking a 
        // checkpoint
        static uint64_t checkpointingCycles;
        // The number of cycles that the main core stalls due to no checker 
        // core available
        static uint64_t noCheckerCycles;
        // The number of cycles that the main core stalls due to dirty data 
        // eviction from L1 data cache
        static uint64_t blockingWaitCycles;

        static bool debugFlag;

        static void printHisto(std::ofstream * outfile, 
            uint64_t& maxHistoSize = loadstorelogentry::maxHistoSize, 
            std::vector<histoEntry>& histoEntries = loadstorelogentry::histoEntries, 
            histoEntry& bigBucket = loadstorelogentry::bigBucket) {
            std::cout << "size : number : min : max : mean" << std::endl;
            *outfile << "size : number : min : max : mean" << std::endl;
            for (int x=0; x<NUMBEROFHISTOENTRIES; x++) {
                histoEntry h = histoEntries[x];
                std::cout << (x*maxHistoSize) / NUMBEROFHISTOENTRIES << " : " << h.number << " : " << ((h.minimum == (uint64_t) -1)? 0ul : h.minimum) << " : " << h.maximum << " : " << (h.number==0? 0ul : h.mean/h.number) << std::endl;
            }
            for (int x=0; x<NUMBEROFHISTOENTRIES; x++) {
                histoEntry h = histoEntries[x];
                *outfile << (x*maxHistoSize) / NUMBEROFHISTOENTRIES << " : " << h.number << " : " << ((h.minimum == (uint64_t) -1)? 0ul : h.minimum) << " : " << h.maximum << " : " << (h.number==0? 0ul : h.mean/h.number) << std::endl;
            }
                              histoEntry h = bigBucket;
                     std::cout << "bigbucket : " << h.number << " : " << ((h.minimum == (uint64_t) -1)? 0ul : h.minimum) << " : " << h.maximum << " : " << (h.number==0? 0ul : h.mean/h.number) << std::endl;
                    *outfile << "bigbucket : " << h.number << " : " << ((h.minimum == (uint64_t) -1)? 0ul : h.minimum) << " : " << h.maximum << " : " << (h.number==0? 0ul : h.mean/h.number) << std::endl;


            std::cout << "blocked lines" << std::endl << "number : frequency" << std::endl;
            *outfile << "blocked lines" << std::endl << "number : frequency" << std::endl;

            for (int x=0; x<513; x++) {
                    std::cout << x << " : " << blocked_lines[x] << std::endl;
                    *outfile << x << " : " << blocked_lines[x] << std::endl;
            }

        }

        static void addToHisto (uint64_t time, 
            uint64_t& maxHistoSize = loadstorelogentry::maxHistoSize, 
            std::vector<histoEntry>& histoEntries = loadstorelogentry::histoEntries, 
            histoEntry& bigBucket = loadstorelogentry::bigBucket) 
        {

            if (time > BIGBUCKETLIMIT) {
                //std::cout << "bigbucket" << time << " at cycle " << curTick() << "\n";
                bigBucket.add_entry(time);
                return;
            }

            while (maxHistoSize <= time) {
                maxHistoSize *=2;
                for (int x=0; x<(NUMBEROFHISTOENTRIES); x+=2) {
                    histoEntries[x/2] = histoEntry::merge(histoEntries[x],histoEntries[x+1]);
                }
                for (int x=NUMBEROFHISTOENTRIES/2; x<NUMBEROFHISTOENTRIES; x++) {
                    histoEntries[x] = histoEntry();
                }


            }

	    assert(time < maxHistoSize);
            uint64_t index = (NUMBEROFHISTOENTRIES*time) / maxHistoSize;
	    assert(index < NUMBEROFHISTOENTRIES);
            histoEntries[index].add_entry(time);
        }


        static void printAimdHisto(std::ofstream* outfile) {

            std::cout << "AIMD Histogram" << std::endl;
            std::cout << "size : number" << std::endl;
            *outfile << "AIMD Histogram" << std::endl;
            *outfile << "size : number" << std::endl;


            for (int x=0; x<NUMBEROFAIMDHISTOENTRIES+1; x++) {
                uint64_t i = aimdHistoentries[x];
                std::cout << (x*TIMEOUT) / NUMBEROFAIMDHISTOENTRIES << " : " << i  << std::endl;
                *outfile << (x*TIMEOUT) / NUMBEROFAIMDHISTOENTRIES << " : " << i  << std::endl;

            }
        }
        static void printCptLengthHisto(std::ofstream* outfile) {
            *outfile << "cpt length Histogram" << std::endl;
            *outfile << "size : number" << std::endl;
            for (int x=0; x<NUMBEROFAIMDHISTOENTRIES+1; x++) {
                uint64_t i = cptLengthHistoentries[x];
                *outfile << (x*TIMEOUT) / NUMBEROFAIMDHISTOENTRIES << " : " << i  << std::endl;

            }
            for (int x = NUMBEROFAIMDHISTOENTRIES+1; x < cptLengthHistoentries.size(); x++) {
                uint64_t i = cptLengthHistoentries[x];
                if (i > 0) {
                    *outfile << (x*TIMEOUT) / NUMBEROFAIMDHISTOENTRIES << " : " << i  << std::endl;
                }
            }
            *outfile << "length to first LSL entry Histogram" << std::endl;
            *outfile << "size : number" << std::endl;
            for (int x=0; x<NUMBEROFAIMDHISTOENTRIES+1; x++) {
                uint64_t i = lengthToFirstLSLHistoentries[x];
                *outfile << (x*TIMEOUT) / NUMBEROFAIMDHISTOENTRIES << " : " << i  << std::endl;

            }
            for (int x = NUMBEROFAIMDHISTOENTRIES+1; x < lengthToFirstLSLHistoentries.size(); x++) {
                uint64_t i = lengthToFirstLSLHistoentries[x];
                if (i > 0) {
                    *outfile << (x*TIMEOUT) / NUMBEROFAIMDHISTOENTRIES << " : " << i  << std::endl;
                }
            }
            *outfile << "length from last LSL entry Histogram" << std::endl;
            *outfile << "size : number" << std::endl;
            for (int x=0; x<NUMBEROFAIMDHISTOENTRIES+1; x++) {
                uint64_t i = lengthFromLastLSLHistoentries[x];
                *outfile << (x*TIMEOUT) / NUMBEROFAIMDHISTOENTRIES << " : " << i  << std::endl;

            }
            for (int x = NUMBEROFAIMDHISTOENTRIES+1; x < lengthFromLastLSLHistoentries.size(); x++) {
                uint64_t i = lengthFromLastLSLHistoentries[x];
                if (i > 0) {
                    *outfile << (x*TIMEOUT) / NUMBEROFAIMDHISTOENTRIES << " : " << i  << std::endl;
                }
            }
        }


        static void addToAIMDHisto (uint64_t histo) {
            assert(histo <= TIMEOUT);
            assert(((histo*NUMBEROFAIMDHISTOENTRIES) / TIMEOUT) <= NUMBEROFAIMDHISTOENTRIES);
            aimdHistoentries[(histo*NUMBEROFAIMDHISTOENTRIES) / TIMEOUT]++;
        }

        static void addToCptLengthHisto (uint64_t histo) {
            if (histo <= TIMEOUT) {
                assert(((histo*NUMBEROFAIMDHISTOENTRIES) / TIMEOUT) <= NUMBEROFAIMDHISTOENTRIES);
            } else {
                assert(((histo*NUMBEROFAIMDHISTOENTRIES) / TIMEOUT) <= cptLengthHistoentries.size());
            }
            // cptLengthSequence.push_back(histo);
            cptLengthHistoentries[(histo*NUMBEROFAIMDHISTOENTRIES) / TIMEOUT]++;
        }

        static void addToLengthToFirstLSLHisto (uint64_t histo) {
            if (histo <= TIMEOUT) {
                assert(((histo*NUMBEROFAIMDHISTOENTRIES) / TIMEOUT) <= NUMBEROFAIMDHISTOENTRIES);
            } else {
                assert(((histo*NUMBEROFAIMDHISTOENTRIES) / TIMEOUT) <= lengthToFirstLSLHistoentries.size());
            }
            lengthToFirstLSLHistoentries[(histo*NUMBEROFAIMDHISTOENTRIES) / TIMEOUT]++;
        }

        static void addToLengthFromLastLSLHisto (uint64_t histo) {
            if (histo <= TIMEOUT) {
                assert(((histo*NUMBEROFAIMDHISTOENTRIES) / TIMEOUT) <= NUMBEROFAIMDHISTOENTRIES);
            } else {
                assert(((histo*NUMBEROFAIMDHISTOENTRIES) / TIMEOUT) <= lengthFromLastLSLHistoentries.size());
            }
            lengthFromLastLSLHistoentries[(histo*NUMBEROFAIMDHISTOENTRIES) / TIMEOUT]++;
        }

        static bool allocate_little_for_big(int mainCPUID);
        static void commit_minor_checkpoint(BaseCPU* cpu);
        static void copy_main_registers_to_checker(BaseCPU* cpu, int checkerCoreId);
        static void updateMainComparisonContexts(BaseCPU* cpu);
        static void updateMainContexts(BaseCPU* cpu);
        static int mainCPURollback(BaseCPU* cpu);
        static bool checkerCheckIfShouldSleep(BaseCPU* cpu);
        static void checkerWakeup(int x);
        static void mainDoCheckpoint(BaseCPU* cpu, bool wasSyscall);
        static bool mainShouldBlock(BaseCPU* cpu, bool should_wait, bool should_commit);
        static bool isMainReady(int checkerCPUID);
        static void recStartCommitStats(BaseCPU* cpu);
        static int getMainID(int checkerCPUID);
        static bool isMainCore(int cpuID);
        static bool isCheckerCore(int cpuID);
        static bool checkedMainStaticInst(
            std::map<std::string, std::array<int, 2>>::iterator it);
        static std::pair<std::map<std::string, std::array<int, 2>>::iterator,
                         bool>
        addMainStaticInst(int cpuID, std::string instName);
        static int getNumCheckedStaticInsts(int cpuID);
        static std::string printStaticInsts(int cpuID);
        static std::pair<std::map<uint64_t, std::array<int, 2>>::iterator,
                         bool>
        addMainPC(int cpuID, uint64_t pc, std::string instName);
        static bool checkedMainPC(
            std::map<uint64_t, std::array<int, 2>>::iterator it);
        static std::string printPCs(int cpuID);

        static void not_found_sleep(int id); 


        loadstorelogentry (bool isLoad, bool isStoreConditional, Addr address, uint8_t* ld_data, uint8_t size, uint64_t secondary_data, uint64_t t, uint64_t progc, std::string name, unsigned flagz, uint64_t uprogc, int64_t loadstorelogSeqNum) {
            load = isLoad;
            flags = flagz;
            isSC = isStoreConditional;

            if (ld_data) {
              assert(size!=0);
              data.assign(ld_data, &ld_data[size]);
            }
            addr = address;
            extra_data = secondary_data;
            valid = true;
            time = t;
            pc = progc;
            microPC = uprogc;
            inst_name = name;
            assert(loadstorelogSeqNum > 0); // Was initialized to 1 and should increase
            seqNum = loadstorelogSeqNum;
        }

        loadstorelogentry () {
            load = false;
            isSC = false;
            addr = 0;
            valid = false;
            extra_data = 0;
            time = 0;
            pc = 0;
            microPC = 0;
            inst_name = "";
            seqNum = -1;
        }

            static void dumpLocalLogState(int id) {
               // Used for debugging
               std::cout << "\nLocal state of the loadstorelog:" << std::endl;
               for (int diff = (checkerCPUMeta.at(id).entryIndices > 2 ? -3 : -checkerCPUMeta.at(id).entryIndices); 
                                           diff <= 3 && checkerCPUMeta.at(id).entryIndices+diff < logsize; diff++) {
                  loadstorelogentry l = checkerCPUMeta.at(id).entries[checkerCPUMeta.at(id).entryIndices+diff];
                  std::cout << "\tAt " << diff << "(+ " << checkerCPUMeta.at(id).entryIndices <<  "): "
                                 << (l.valid?"":"[Invalid]") << " address = " << std::hex << l.addr
                                 << ", status = " << (l.load?'r':'w')
                                 << " -> (" << l.data.size() << ") [";
                  for (int i=0; i < l.data.size(); ++i) {
                     std::cout << (uint64_t)l.data[i] << (i==l.data.size()-1?"], ":", ");
                  }
                  std::cout << "extra_data = " << l.extra_data;
                  std::cout << std::endl;
               }
               std::cout << std::endl;
            }



        bool compare_data(PacketPtr pkt, int id) {
          // assert(pkt->req->getSize() <= data.size()-checkerCPUMeta.at(id).dataAddressOffset);
          if (pkt->req->getSize() > data.size()-checkerCPUMeta.at(id).dataAddressOffset) {
            return false;
          }

          /* dczva implementation on the main core does not actually zero the
           * entire cache line as it should so we add a special case here.
           */
          if (inst_name[0]=='d' && inst_name[1]=='c') {
            if (debugFlag) std::cout << inst_name << " instruction encountered" << std::endl;
            if (inst_name=="dc zva") {
              for (int i=0; i<pkt->req->getSize(); ++i) {
                if (pkt->getPtr<uint8_t>()[i] != 0) return false;
              }
              return true;
            }
          }

          // Compare the data stored and the one written by the checker core.
          for (int i=0; i<pkt->req->getSize(); ++i) {
            if (pkt->isMaskedWrite() && !pkt->req->getByteEnable()[i]) {
                continue;
            }
            if (pkt->getConstPtr<uint8_t>()[i] != data.data()[i+checkerCPUMeta.at(id).dataAddressOffset])
              return false;
          }
          return true;
        }

  static bool do_write(loadstorelogentry l, int cpuID, bool newline, bool mayMergeMicroop, uint64_t currentCommittedInstructions);
  static bool do_read(PacketPtr pkt, ThreadContext* tc);
  bool do_read(PacketPtr pkt, int id);
  static bool try_read(PacketPtr pkt, ThreadContext* tc);
};

void add_cpu(BaseCPU* cpu, int cpuID);


}
#endif
