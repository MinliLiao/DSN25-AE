#ifndef ERROR_INJECTION_H
#define ERROR_INJECTION_H

#include <chrono>
#include <cmath>
#include <map>
#include <random>
#include <set>

#include "cpu/reg_class.hh"
#include "cpu/thread_context.hh"
#include "mem/cache/loadstorelogentry.hh"

namespace gem5 {
// #define DEBUG_ERRROR_INJECTION // Uncomment to get precise information on error injection and detection

// For each of the following error rates constant definition, simply uncomment
// the line to inject errors at the desired point.

#if PARADVFS
#define UNIVERSAL_ERRORRATE (std::max(1.0,2*(60.0 * 3200.0 * 1000.0 * 1000.0) / pow(6.0,(50.0*(1.0-loadstorelogentry::voltage[0])))))
//TODO: really, should be based on core (not voltage[0], but voltage[n]), but no need for now.
//0.5/60 * 6^(50*(1-voltage)) errors per second.
#else
//#define UNIVERSAL_ERRORRATE 1000
#endif

#ifdef UNIVERSAL_ERRORRATE
    #define LOADSTORE_ERRORRATE UNIVERSAL_ERRORRATE // Inject errors on load
    #define ARCHSTATE_ERRORRATE UNIVERSAL_ERRORRATE  // Corrupt starting architectural states
    #define TCSTATE_ERRORRATE UNIVERSAL_ERRORRATE  // Corrupt random architectural states
    #define OPCLASS_ERRORRATE UNIVERSAL_ERRORRATE // Inject errors on specific operation class

// In case OPCLASS_ERRORRATE is defined, one of the following should be as well:
    #define OPCLASS_ERRORRATE_UNIVERSAL
#endif

//#define LOADSTORE_ERRORRATE 5000 // Inject errors on load
// #define ARCHSTATE_ERRORRATE 400   // Corrupt starting architectural states
// #define TCSTATE_ERRORRATE 50000   // Corrupt random architectural states
// #define OPCLASS_ERRORRATE 1 // Inject errors on specific operation class

// In case OPCLASS_ERRORRATE is defined, one of the following should be as well:
// #define OPCLASS_ERRORRATE_ALU
// #define OPCLASS_ERRORRATE_INT
// #define OPCLASS_ERRORRATE_FLOAT


#ifdef OPCLASS_ERRORRATE_FLOAT
#define OPCLASS_ERRORRATE OPCLASS_ERRORRATE_FLOAT
#define OPCLASS_TARGET (opclass >= 4 && opclass < 12)  // The targeted opclass (see OpClass() in FuncUnit.py)
#endif
#ifdef OPCLASS_ERRORRATE_INT
#define OPCLASS_ERRORRATE OPCLASS_ERRORRATE_INT
#define OPCLASS_TARGET (opclass >= 1 && opclass < 4)
#endif
#ifdef OPCLASS_ERRORRATE_ALU
#define OPCLASS_ERRORRATE OPCLASS_ERRORRATE_ALU
#define OPCLASS_TARGET (opclass == 1)
#endif
#ifdef OPCLASS_ERRORRATE_UNIVERSAL
#define OPCLASS_TARGET (opclass >= 1 && opclass < 12)
#endif


class errorinjection
{
public:
    enum hardErrStructTypes{
        None, FUdest, LSLentry, RF
    };
    static std::vector<bool> hasInjectedError;//[NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE];
    static std::vector<bool> unchangedInjectedError;
    static int numberOfErroneousWrites;
    static int numberOfErroneousReads;
    static int numberOfErroneousArchStates;
    static int numberOfErroneousTCStates;
    static int numberOfErroneousOpClass;

    static int undetectedErrors;
    static int falsePositives;
    static int cptOnlyUnchangedInjections;
    static uint64_t unchangedInjections;
    static uint64_t changedInjections;

    static double loadstoreErrRate;
    static double TCStateErrRate;
    static double UniversalOpErrRate;
    static double floatOpErrRate;
    static double intOpErrRate;
    static double ALUOpErrRate;
    static unsigned hardErrBit;
    static unsigned hardErrStructId;
    static hardErrStructTypes hardErrStructType;
    static bool hardErrorStuckAt1;
    static unsigned hardErrorCores;
    static std::map<OpClass, std::set<std::string>> multiDestOpClasses;
    static std::map<OpClass, std::set<std::string>> destRegOpClasses;

    static std::vector<uint64_t> lapses;

    static unsigned seed;
    static std::default_random_engine generator;

    static bool exitOnErr;

    static void setErrRates(std::vector<double> errRates);
    static void setHardErr(unsigned errBit, unsigned errStID, 
        std::string errStType, unsigned stuckAt, unsigned errMain, 
        unsigned numMains, unsigned numCheckersPerMain, bool exit_on_error);
};

struct regSafeEntry
{
    RegClassType reg_class;
    RegIndex idx;
    uint64_t value;
    TheISA::VecRegContainer vecVal;
    bool vecReg;

    regSafeEntry(RegClassType r, RegIndex i, uint64_t v) :
        reg_class(r), idx(i), value(v), vecReg(false)
    { }
    regSafeEntry(RegClassType r, RegIndex i, TheISA::VecRegContainer v) :
        reg_class(r), idx(i), vecVal(v), vecReg(true)
    { }

    regSafeEntry() : reg_class(MiscRegClass), idx(0), value(0), vecReg(false)
    { }
};

typedef std::vector<regSafeEntry> regSafe;

void save_modified_regs(StaticInstPtr staticInst, ThreadContext* tc, regSafe* safe);
void save_modified_regs_o3(o3::DynInstPtr inst, regSafe* safe);

void compromise_loadstorelogentry(int cpuID, void* l);

void compromise_architectural_state(int cpuID, void* mm, int reg_type=-1, int idx=-1);

void compromise_thread_context_state(int checkerID, int reg_type=-1, int idx=-1);

void compromise_instruction_result(int checkerID, StaticInstPtr inst, ThreadContext* thread, regSafe* before_instr);
bool injectFUdestError(int checkerID);
bool stuckAt_addr(uint64_t& addr, int idx_bit, bool stuckAt1);
void stuckAt_stats(int checkerID, bool changed);
bool stuckAt_instruction_result(int checkerID, StaticInstPtr staticInst, ThreadContext* tc, regSafe* before_instr, int idx_bit, bool stuckAt1);
bool stuckAt_instruction_result_o3(int checkerID, o3::DynInstPtr inst, regSafe* before_instr, int idx_bit, bool stuckAt1);

}
#endif // ERROR_INJECTION_H
