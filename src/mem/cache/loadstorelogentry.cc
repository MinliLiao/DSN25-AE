/* Authors: Sam Ainsworth
*
*/

#include "mem/cache/loadstorelogentry.hh"

#include "base/output.hh"
#include "cpu/error_injection.hh"
#include "cpu/o3/cpu.hh"
#include "cpu/minor/cpu.hh"
#include "debug/LoadStoreLogChecker.hh"
#include "debug/LoadStoreLogDebugFlag.hh"
#include "debug/LoadStoreLogSeqNum.hh"
#include "debug/LoadStoreLogSleepGuard.hh"
#include "debug/LoadStoreLogSwap.hh"
#include "debug/MinorStrictLdStOrder.hh"

//#include "sim/syscall_emul.hh"
#include <iostream>

#include "sim/syscalllog.hh"

namespace gem5 {

void loadstorelogentry::initCacheSets(int sets) {
std::cout << "Number of cache sets: " << sets << "\n";
   blocked_lines.resize(sets+1);
   actualstoresize=3;
   
}

int loadstorelogentry::actualnumberofmaincores = 1;
int loadstorelogentry::actualstoresize = 2;
int loadstorelogentry::actualnumberofcheckercores = 0;
bool loadstorelogentry::timeoutAIMD = true;
int loadstorelogentry::cptTimeout = 5000;
int loadstorelogentry::lslSize = 4096;
bool loadstorelogentry::minorCommitBypass = false;
bool loadstorelogentry::useHash = false;
int loadstorelogentry::num_checkSlot_per_checker = 1;

void loadstorelogentry::initCoreCount(int mains, int checkers, int extra_slot_per_checker, bool hashed, std::vector<double> errRates) {

   std::cout<< "Configuring "<< mains << " main cores and " << checkers << " checkers\n";

   actualnumberofmaincores=mains;
   checkers = checkers / mains; // # of checkers per core
   actualnumberofcheckercores=checkers;
   num_checkSlot_per_checker = 1 + extra_slot_per_checker;
   useHash = hashed;
   debugFlag = (debug::LoadStoreLogDebugFlag) ? true : false;

   syscalllogentry::resizeLogs(mains,checkers);
   
   allCPUMeta.resize(mains+mains*checkers,AllCPUMeta());
   mainCPUMeta.resize(mains,MainCPUMeta());
   checkerCPUMeta.resize(num_checkSlot_per_checker*mains*checkers,CheckerCPUMeta());
   last_macro_addrs.resize(checkerCPUMeta.size(),0);
   mainStaticInstsChecked.resize(mains,
                                 std::map<std::string, std::array<int, 2>>());
   mainPCsChecked.resize(mains,
                         std::map<uint64_t, std::array<int, 2>>());

   assert(checkerCPUMeta.size() ==
          num_checkSlot_per_checker*NUMBEROFMAINCORES * NUMBEROFCHECKERCORESPERCORE);
   
   errordetection::voltage_reset.resize(mains,false);

   for(int x=0; x<mains; x++) {
      mainCPUMeta[x].current_segment_to_fill = x*checkers;
      mainCPUMeta[x].lastChecker = x*checkers;
      mainCPUMeta[x].should_cache_wait.resize(mains, 0);
   }
   
   

   errorinjection::hasInjectedError.resize(mains*checkers, false);
   errorinjection::unchangedInjectedError.resize(mains*checkers, false);
   errorinjection::setErrRates(errRates);
  
   for(int z=0; z<num_checkSlot_per_checker*mains*checkers;z++) {
     checkerCPUMeta[z].entries.resize(logsize,loadstorelogentry());
   }
}

void 
loadstorelogentry::initTimeout(bool AIMDoff, int in_cptTimeout, int in_lslSize) 
{
    timeoutAIMD = !AIMDoff;
    cptTimeout = in_cptTimeout;
    lslSize = in_lslSize;
    std::cout << "AIMD " << AIMD << ", TIMEOUT " << TIMEOUT << ", logsize " << logsize << std::endl;
    aimdHistoentries.resize(NUMBEROFAIMDHISTOENTRIES+64,0);
    cptLengthHistoentries.resize(NUMBEROFAIMDHISTOENTRIES+64,0);
    lengthToFirstLSLHistoentries.resize(NUMBEROFAIMDHISTOENTRIES+64,0);
    lengthFromLastLSLHistoentries.resize(NUMBEROFAIMDHISTOENTRIES+64,0);
    histoEntries.resize(NUMBEROFHISTOENTRIES+64,histoEntry());
    cptLenHistoEntries.resize(NUMBEROFHISTOENTRIES+64,histoEntry());
    checkerCptLenHistoEntries.resize(NUMBEROFHISTOENTRIES+64,histoEntry());
    cptFirstFetchToCommitDelayHistoEntries.resize(NUMBEROFHISTOENTRIES+64,histoEntry());
}

void
loadstorelogentry::initMinorCommitBypass(bool in_minorCommitBypass) 
{
    minorCommitBypass = in_minorCommitBypass;
    std::cout << "minorCommitBypass " << minorCommitBypass << std::endl;
}

bool
loadstorelogentry::isMainCore(int cpuID)
{
    return cpuID >= 0 && cpuID < allCPUMeta.size() &&
           allCPUMeta[cpuID].baseCPU->isMain();
}

bool
loadstorelogentry::isCheckerCore(int cpuID)
{
    return cpuID >= 0 && cpuID < allCPUMeta.size() &&
           allCPUMeta[cpuID].baseCPU->isChecker();
}

bool
loadstorelogentry::checkedMainStaticInst(
    std::map<std::string, std::array<int, 2>>::iterator it)
{
    bool newCheck = false;
    newCheck = (it->second[1] == 0);
    it->second[1]++;
    return newCheck;
}
std::pair<std::map<std::string, std::array<int, 2>>::iterator, bool>
loadstorelogentry::addMainStaticInst(int cpuID, std::string instName)
{
    auto result = mainStaticInstsChecked[cpuID].insert(
        std::make_pair(instName, std::array<int, 2>({0, 0})));
    result.first->second[0]++;
    return result;
}
int
loadstorelogentry::getNumCheckedStaticInsts(int cpuID)
{
    return std::count_if(mainStaticInstsChecked[cpuID].begin(),
                         mainStaticInstsChecked[cpuID].end(),
                         [](auto x) { return x.second[1] > 0; });
}
std::string
loadstorelogentry::printStaticInsts(int cpuID)
{
    std::stringstream ss;
    ss << "static inst,# encountered,# checked" << std::endl;
    for (auto it : mainStaticInstsChecked[cpuID]) {
     ss << it.first << "," << it.second[0] << "," << it.second[1] << std::endl;
    }
    return ss.str();
}

bool
loadstorelogentry::checkedMainPC(
    std::map<uint64_t, std::array<int, 2>>::iterator it)
{
    bool newCheck = false;
    newCheck = (it->second[1] == 0);
    it->second[1]++;
    return newCheck;
}
std::pair<std::map<uint64_t, std::array<int, 2>>::iterator, bool>
loadstorelogentry::addMainPC(int cpuID, uint64_t pc, std::string instName)
{
    auto result = mainPCsChecked[cpuID].insert(
        std::make_pair(pc, std::array<int, 2>({0, 0})));
    result.first->second[0]++;
    auto check = mainPCStaticInstsMap.insert(std::make_pair(pc, instName));
    assert(!check.second || check.first->second == instName);
    return result;
}
std::string
loadstorelogentry::printPCs(int cpuID)
{
    std::stringstream ss;
    ss << "pc, static inst,# encountered,# checked" << std::endl;
    for (auto it : mainPCsChecked[cpuID]) {
     ss << it.first << "," << mainPCStaticInstsMap[it.first] << "," << it.second[0] << "," << it.second[1] << std::endl;
    }
    return ss.str();
}

uint64_t loadstorelogentry::meanTime=0;
uint64_t loadstorelogentry::maxTime=0;
uint64_t loadstorelogentry::minTime=(uint64_t)-1;
uint64_t loadstorelogentry::times=0;
uint64_t loadstorelogentry::totalCommittedInstructions = 0;
uint64_t loadstorelogentry::checkedCommittedInstructions = 0;
uint64_t loadstorelogentry::checkStartDelayInstructions = 0;
uint64_t loadstorelogentry::checkDelayCommittedInstructions = 0;
uint64_t loadstorelogentry::cptStartDelayTicks = 0;
uint64_t loadstorelogentry::cptLenTicks = 0;
uint64_t loadstorelogentry::cptCheckerStartToFetchDelayTicks = 0;
uint64_t loadstorelogentry::cptCheckerFirstFetchTransAccDelayTicks = 0;
uint64_t loadstorelogentry::cptCheckerFirstFetchToCommitDelayTicks = 0;
uint64_t loadstorelogentry::cptCheckerStartToCommitDelayTicks = 0;
uint64_t loadstorelogentry::cptCheckerFirstToLastCommitDelayTicks = 0;
uint64_t loadstorelogentry::cptCheckerLastCommitToDrainDoneDelayTicks = 0;
uint64_t loadstorelogentry::cptCheckerDrainDoneToStartDelayTicks = 0;
uint64_t loadstorelogentry::checkpointingCycles = 0;
uint64_t loadstorelogentry::noCheckerCycles = 0;
uint64_t loadstorelogentry::blockingWaitCycles = 0;


std::vector<histoEntry> loadstorelogentry::histoEntries;
std::vector<histoEntry> loadstorelogentry::cptLenHistoEntries;
std::vector<histoEntry> loadstorelogentry::checkerCptLenHistoEntries;
std::vector<histoEntry> loadstorelogentry::cptFirstFetchToCommitDelayHistoEntries;

// uint64_t loadstorelogentry::aimdHistoentries[NUMBEROFAIMDHISTOENTRIES+64] = {0};
std::vector<uint64_t> loadstorelogentry::aimdHistoentries;
std::vector<uint64_t> loadstorelogentry::cptLengthHistoentries;
std::vector<uint64_t> loadstorelogentry::lengthToFirstLSLHistoentries;
std::vector<uint64_t> loadstorelogentry::lengthFromLastLSLHistoentries;

uint64_t loadstorelogentry::maxHistoSize = 100000;
uint64_t loadstorelogentry::cptLenMaxHistoSize = 10000;
uint64_t loadstorelogentry::checkerCptLenMaxHistoSize = 100;
uint64_t loadstorelogentry::cptFirstFetchToCommitDelayMaxHistoSize = NUMBEROFHISTOENTRIES;

histoEntry loadstorelogentry::bigBucket = histoEntry();
histoEntry loadstorelogentry::cptLenBigBucket = histoEntry();
histoEntry loadstorelogentry::checkerCptLenBigBucket = histoEntry();
histoEntry loadstorelogentry::cptFirstFetchToCommitDelayBigBucket = histoEntry();

std::vector<loadstorelogentry::MainCPUMeta> loadstorelogentry::mainCPUMeta;
std::vector<loadstorelogentry::CheckerCPUMeta> loadstorelogentry::checkerCPUMeta;
std::vector<loadstorelogentry::AllCPUMeta> loadstorelogentry::allCPUMeta;

std::vector<uint64_t> loadstorelogentry::blocked_lines(513,0);

bool loadstorelogentry::debugFlag = false;
std::vector<Addr> loadstorelogentry::last_macro_addrs;
std::vector<std::map<std::string, std::array<int, 2>>>
    loadstorelogentry::mainStaticInstsChecked;
std::vector<std::map<uint64_t, std::array<int, 2>>>
    loadstorelogentry::mainPCsChecked;
std::map<uint64_t, std::string> loadstorelogentry::mainPCStaticInstsMap;


using namespace TheISA;
miniContext
m_serialize(ThreadContext *tc)
{

    miniContext m;

 for (int i = 0; i < int_reg::NumRegs; i++) {
        RegId reg(IntRegClass, i);
        m.intRegs.at(i) = tc->getRegFlat(reg);
    }

    for (int i = 0; i < cc_reg::NumRegs; i++) {
        RegId reg(CCRegClass, i);
         m.ccRegs.at(i) = tc->getReg(reg);
    }

    for (int i = 0; i < NUM_MISCREGS; i++)
         m.miscRegs.at(i) = tc->readMiscRegNoEffect(i);

    for (int i = 0; i < NumVecRegs; i++) {
        RegId reg(VecRegClass, i);
        tc->getRegFlat(reg, &(m.vc[i]));
    }

    for (int i = 0; i < NumVecRegs * NumVecElemPerVecReg; i++) {
        RegId reg(VecElemClass, i);
        m.vecRegs.at(i) = tc->getRegFlat(reg);
    }

    // setMiscReg "with effect" will set the misc register mapping correctly.
    // e.g. updateRegMap(val)
    m.CPSR = tc->readMiscRegNoEffect(MISCREG_CPSR);


    m.pcState = tc->pcState().as<TheISA::PCState>();
    m.initialized = true;
    m.checked = false;

    return m;
}

bool m_identical(ThreadContext *tc, const miniContext m)
{
    assert(m.initialized);
    assert(!m.checked);

    bool ret = true;

     for (int i = 0; i < int_reg::NumRegs; i++) {
             if (i==34) continue;//TODO: from old codebase. Needed here too?
        RegId reg(IntRegClass, i);
        ret &= m.intRegs.at(i) == tc->getRegFlat(reg);
        if(m.intRegs.at(i) != tc->getRegFlat(reg)) {std::cout << "int " << i << " " << m.intRegs.at(i) << " vs " << tc->getRegFlat(reg) << " \n";}
    }

//check FloatRegs
    for (int i = 0; i < NumVecRegs * NumVecElemPerVecReg; i++) {
        RegId reg(VecElemClass, i);
        ret &= m.vecRegs.at(i) == tc->getRegFlat(reg);
        if(m.vecRegs.at(i) != tc->getRegFlat(reg)) {std::cout << "vec " << i << "\n";}
    }


#ifdef ISA_HAS_CC_REGS
    for (int i = 0; i < NumCCRegs; ++i) {
         RegId reg(CCRegClass, i);
        ret &= m.ccRegs[i] == tc->getReg(reg);
    }
    // if (!ret) {
    //     std::cout << "/!\\ Different CC registers. ";
    //     for (int i = 0; i < NumCCRegs; ++i)
    //         if (m.ccRegs[i] != tc->readCCReg(i)) {
    //             std::cout << "(" << i << " ie " << TheISA::ccRegName[i] << ") " << m.ccRegs[i] << " != " << tc->readCCReg(i) << " ; ";
    //         }
    //     return ret;
    // }
#endif

    for (int i=0; i< NUM_MISCREGS; ++i) {
        if (i==19/* && (m.miscRegs[19] != tc->readMiscRegNoEffect(19))*/) { //TODO: from old codebase. Still needed?
            // miscRegs[19] contains the Load locked address. The main core
            // stores the physical address of the memory location, but checker
            // core doesn't get the proper physical address, so the value will
            // not match.
            continue;
        } else {
            ret &= m.miscRegs.at(i) == tc->readMiscRegNoEffect(i);
            if(m.miscRegs.at(i) != tc->readMiscRegNoEffect(i)) {std::cout << "misc " << i << "\n";}
        }
    }

    ret &= m.CPSR == tc->readMiscRegNoEffect(MISCREG_CPSR);


    // For PCState, the equality is overloaded in src/arch/*THEISA*/types.hh
    ret &= m.pcState == tc->pcState();

    return ret;
}

void m_copyRegs(ThreadContext *tc, miniContext m)
{

    for (int i = 0; i < int_reg::NumRegs; i++) {
        RegId reg(IntRegClass, i);
        tc->setRegFlat(reg, m.intRegs.at(i));
    }
    for (int i = 0; i < cc_reg::NumRegs; i++) {
        RegId reg(CCRegClass, i);
        tc->setReg(reg,  m.ccRegs[i]);
    }

    for (int i = 0; i < NUM_MISCREGS; i++)
        tc->setMiscRegNoEffect(i, m.miscRegs.at(i));

    for (int i = 0; i < NumVecRegs; i++) {
        RegId reg(VecRegClass, i);
        tc->setRegFlat(reg, &(m.vc[i]));
    }
    for (int i = 0; i < NumVecRegs * NumVecElemPerVecReg; i++) {
        RegId reg(VecElemClass, i);
        tc->setRegFlat(reg, m.vecRegs.at(i));
    }

    //not clear this really does anything, but it's from the version in src/arch/arm/utility.cc
    tc->setMiscReg(MISCREG_CPSR, m.CPSR);

    tc->pcState(m.pcState);

    // Invalidate the tlb misc register cache
    static_cast<MMU *>(tc->getMMUPtr())->invalidateMiscReg();


    // thread_num and cpu_id are deterministic from the config
}

namespace errordetection {
#if LOGERRORS

std::vector<uint64_t> detectionTime = std::vector<uint64_t>();
std::vector<uint64_t> detectionPoints = std::vector<uint64_t>();
std::vector<double> detectionVoltage = std::vector<double>();

std::vector<uint64_t> memoryRecoveryTime = std::vector<uint64_t>();
std::vector<uint64_t> voltageSwitchTime = std::vector<uint64_t>();
std::vector<double> voltageSwitchPoints = std::vector<double>();


#endif
uint64_t detected_errors = 0;
uint64_t total_memory_recoveries = 0;


uint64_t min_memory_recoveries = 0;
uint64_t max_memory_recoveries = 0;

uint64_t total_rollback_recovery = 0;

uint64_t min_rollback_recovery = 0;
uint64_t max_rollback_recovery = 0;


std::vector<bool> voltage_reset{false};//[NUMBEROFMAINCORES]=  {false};



uint64_t numberOfDetectedErroneousWrites = 0;
uint64_t numberOfDetectedErroneousReads = 0;
uint64_t numberOfDetectedErroneousArchStates = 0;
uint64_t numberOfCorrectCheckpoints = 0;
}

void errordetection::detectError(int id) {

#if 1
    assert(id < NUMBEROFMAINCORES * NUMBEROFCHECKERCORESPERCORE);
    std::cout << ">>> Detecting error"
              << (errorinjection::hasInjectedError[id] ? "" : " (false positive!)")
              << " for checker " << id << " at local time "
              << loadstorelogentry::checkerCPUMeta.at(id).timestamps << " (global time: "
              << loadstorelogentry::mainCPUMeta[id/NUMBEROFCHECKERCORESPERCORE].timestamp
              << ") insts " << loadstorelogentry::allCPUMeta[id+NUMBEROFMAINCORES].baseCPU->committedInstrs << std::endl;
#endif

    if (errorinjection::exitOnErr) {
        // Make sure that stats are printed before exiting
        errordetection::print_times();
    }
    assert(!loadstorelogentry::checkerCPUMeta.at(id).expectedFinalContext.checked || !loadstorelogentry::checkerCPUMeta.at(id).expectedFinalContext.set);
    if (!loadstorelogentry::checkerCPUMeta.at(id).expectedFinalContext.set) std::cout << "context not set yet\n";

    loadstorelogentry::checkerCPUMeta.at(id).expectedFinalContext.checked = true;
    assert(!errorinjection::exitOnErr);
    if (!errorinjection::hasInjectedError[id]) {
        errorinjection::falsePositives++;
        return;
    }
    errorinjection::hasInjectedError[id] = false;
    errorinjection::unchangedInjectedError[id] = false;

    loadstorelogentry::checkerCPUMeta.at(id).erroneous = true;




}

#if PARADVFS
static int ctr = 0;
#endif

void errordetection::detectErrorCommit(int id) {

#if LOGERRORS
    assert(id < NUMBEROFMAINCORES * NUMBEROFCHECKERCORESPERCORE);
    std::cout << ">>> Detecting errorcommit"
             << " for checker " << id << " at local time "
              << loadstorelogentry::checkerCPUMeta.at(id).timestamps << " (global time: "
              << loadstorelogentry::mainCPUMeta[id/NUMBEROFCHECKERCORESPERCORE].timestamp
              << ")" << std::endl;
#endif
    int mainCPUID = id/NUMBEROFCHECKERCORESPERCORE;
    uint64_t lastCorrectTick = loadstorelogentry::checkerCPUMeta.at(id).startingTick;

    uint64_t detectionTime = curTick() - lastCorrectTick;

    loadstorelogentry::mainCPUMeta[mainCPUID].errorTick = lastCorrectTick;
    errordetection::detected_errors++;
#if LOGERRORS
    errordetection::detectionTime.push_back(detectionTime);
    errordetection::detectionPoints.push_back(curTick());
#endif
    errordetection::total_rollback_recovery +=detectionTime;
    errordetection::min_rollback_recovery = std::min(detectionTime,errordetection::min_rollback_recovery);
    errordetection::min_rollback_recovery = errordetection::min_rollback_recovery==0?  detectionTime: errordetection::min_rollback_recovery;
    errordetection::max_rollback_recovery = std::max(detectionTime,errordetection::max_rollback_recovery);


#if PARAGLIDER
    loadstorelogentry::mainCPUMeta[mainCPUID].timeout = std::max((loadstorelogentry::mainCPUMeta[mainCPUID].timeout)>>1,5);
#if PARADVFS
    loadstorelogentry::mainCPUMeta[mainCPUID].highestVoltageError = std::max(loadstorelogentry::mainCPUMeta[mainCPUID].highestVoltageError,loadstorelogentry::mainCPUMeta[mainCPUID].voltage);
    loadstorelogentry::mainCPUMeta[mainCPUID].highestRecentVoltageError = std::max(loadstorelogentry::mainCPUMeta[mainCPUID].highestRecentVoltageError,loadstorelogentry::mainCPUMeta[mainCPUID].voltage);
    ctr++;
    if (ctr > 100) {
        ctr = 0;
        loadstorelogentry::mainCPUMeta[mainCPUID].highestRecentVoltageError =0;
    }
#if LOGERRORS
    errordetection::detectionVoltage.push_back(loadstorelogentry::voltage[mainCPUID]);
    std::cout << "Voltage: " << loadstorelogentry::voltage[mainCPUID];
    std::cout << "Highest Recent Error: " << loadstorelogentry::highestRecentVoltageError[mainCPUID];
#endif
    loadstorelogentry::mainCPUMeta[mainCPUID].voltage = BASEVOLTAGE - AIMDSCALE * (BASEVOLTAGE - loadstorelogentry::mainCPUMeta[mainCPUID].voltage);
#if LOGERRORS
    errordetection::detectionVoltage.push_back(loadstorelogentry::mainCPUMeta[mainCPUID].voltage);
    std::cout << "New Voltage: " << loadstorelogentry::mainCPUMeta[mainCPUID].voltage;
#endif
    errordetection::voltage_reset[mainCPUID] = true;

#endif
#endif
}


void errordetection::print_times() {
    if (loadstorelogentry::times==0) return;
    std::cout << "Delays: mean " << loadstorelogentry::meanTime/loadstorelogentry::times << " max " << loadstorelogentry::maxTime << " min " << loadstorelogentry::minTime << " ps\n";
    std::ofstream outfile;

    outfile.open(simout.resolve("staticInsts.txt"), std::ios::trunc);
    outfile << "Checked," << loadstorelogentry::getNumCheckedStaticInsts(0) << std::endl;
    outfile << "Total," << loadstorelogentry::mainStaticInstsChecked[0].size() << std::endl;
    outfile << loadstorelogentry::printStaticInsts(0);
    outfile << loadstorelogentry::printPCs(0);
    outfile.close();

    outfile.open(simout.resolve("delays.txt"), std::ios::trunc);
    outfile << "Delays: mean " << loadstorelogentry::meanTime/loadstorelogentry::times << " max " << loadstorelogentry::maxTime << " min " << loadstorelogentry::minTime << " ps\n";
    loadstorelogentry::printHisto(&outfile);
    outfile << "Histogram of cpt length in ticks" << std::endl;
    loadstorelogentry::printHisto(&outfile, loadstorelogentry::cptLenMaxHistoSize, loadstorelogentry::cptLenHistoEntries, loadstorelogentry::cptLenBigBucket);
    outfile << "Histogram of checker cpt length in cycles" << std::endl;
    loadstorelogentry::printHisto(&outfile, loadstorelogentry::checkerCptLenMaxHistoSize, loadstorelogentry::checkerCptLenHistoEntries, loadstorelogentry::checkerCptLenBigBucket);
    outfile << "Histogram for checkpoints' first fetch to commit delay in cycles" << std::endl;
    loadstorelogentry::printHisto(&outfile, loadstorelogentry::cptFirstFetchToCommitDelayMaxHistoSize, loadstorelogentry::cptFirstFetchToCommitDelayHistoEntries, loadstorelogentry::cptFirstFetchToCommitDelayBigBucket);
    if (AIMD)
      loadstorelogentry::printAimdHisto(&outfile);
    loadstorelogentry::printCptLengthHisto(&outfile);
    outfile << "checkedCommittedInstructions " << loadstorelogentry::checkedCommittedInstructions << ", totalCommittedInstructions " << loadstorelogentry::totalCommittedInstructions << ", checkDelayCommittedInstructions " << loadstorelogentry::checkDelayCommittedInstructions << ", checkStartDelayInstructions " << loadstorelogentry::checkStartDelayInstructions << std::endl;
    outfile << "cptLenTicks " << loadstorelogentry::cptLenTicks << ", cptStartDelayTicks " << loadstorelogentry::cptStartDelayTicks << ", cptCheckerStartToFetchDelayTicks " << loadstorelogentry::cptCheckerStartToFetchDelayTicks << ", cptCheckerFirstFetchTransAccDelayTicks " << loadstorelogentry::cptCheckerFirstFetchTransAccDelayTicks << ", cptCheckerFirstFetchToCommitDelayTicks " << loadstorelogentry::cptCheckerFirstFetchToCommitDelayTicks << ", cptCheckerStartToCommitDelayTicks " << loadstorelogentry::cptCheckerStartToCommitDelayTicks << ", cptCheckerFirstToLastCommitDelayTicks " << loadstorelogentry::cptCheckerFirstToLastCommitDelayTicks << ", cptCheckerLastCommitToDrainDoneDelayTicks " << loadstorelogentry::cptCheckerLastCommitToDrainDoneDelayTicks << ", cptCheckerDrainDoneToStartDelayTicks " << loadstorelogentry::cptCheckerDrainDoneToStartDelayTicks << ", cptCheckerStartToDrainDoneDelayTicks " << (loadstorelogentry::cptCheckerStartToCommitDelayTicks + loadstorelogentry::cptCheckerFirstToLastCommitDelayTicks + loadstorelogentry::cptCheckerLastCommitToDrainDoneDelayTicks) << std::endl;
    uint64_t numCpts = loadstorelogentry::cptLenBigBucket.number;
    for (auto entry: loadstorelogentry::cptLenHistoEntries) {
        numCpts += entry.number;
    }
    outfile << "numCpts " << numCpts << std::endl;
    if (numCpts > 0 && loadstorelogentry::allCPUMeta.size() > 0) { // At least 1 cpt and 1 main core
        outfile << "cptLen/cpt (main cycles) " << loadstorelogentry::allCPUMeta[0].baseCPU->ticksToCycles(loadstorelogentry::cptLenTicks/numCpts) << ", cptStartDelay/cpt (main cycles) " << loadstorelogentry::allCPUMeta[0].baseCPU->ticksToCycles(loadstorelogentry::cptStartDelayTicks/numCpts) << ", cptCheckerStartToFetchDelay/cpt (main cycles) " << loadstorelogentry::allCPUMeta[0].baseCPU->ticksToCycles(loadstorelogentry::cptCheckerStartToFetchDelayTicks/numCpts) << ", cptCheckerFirstFetchTransAccDelay/cpt (main cycles) " << loadstorelogentry::allCPUMeta[0].baseCPU->ticksToCycles(loadstorelogentry::cptCheckerFirstFetchTransAccDelayTicks/numCpts) << ", cptCheckerFirstFetchToCommitDelay/cpt (main cycles) " << loadstorelogentry::allCPUMeta[0].baseCPU->ticksToCycles(loadstorelogentry::cptCheckerFirstFetchToCommitDelayTicks/numCpts) << ", cptCheckerStartToCommitDelay/cpt (main cycles) " << loadstorelogentry::allCPUMeta[0].baseCPU->ticksToCycles(loadstorelogentry::cptCheckerStartToCommitDelayTicks/numCpts) << ", cptCheckerFirstToLastCommitDelay/cpt (main cycles) " << loadstorelogentry::allCPUMeta[0].baseCPU->ticksToCycles(loadstorelogentry::cptCheckerFirstToLastCommitDelayTicks/numCpts) << ", cptCheckerLastCommitToDrainDoneDelay/cpt (main cycles) " << loadstorelogentry::allCPUMeta[0].baseCPU->ticksToCycles(loadstorelogentry::cptCheckerLastCommitToDrainDoneDelayTicks/numCpts) << ", cptCheckerDrainDoneToStartDelay/cpt (main cycles) " << loadstorelogentry::allCPUMeta[0].baseCPU->ticksToCycles(loadstorelogentry::cptCheckerDrainDoneToStartDelayTicks/numCpts) << ", cptCheckerStartToDrainDoneDelay/cpt (main cycles) " << loadstorelogentry::allCPUMeta[0].baseCPU->ticksToCycles((loadstorelogentry::cptCheckerStartToCommitDelayTicks + loadstorelogentry::cptCheckerFirstToLastCommitDelayTicks + loadstorelogentry::cptCheckerLastCommitToDrainDoneDelayTicks)/numCpts) << std::endl;
    }
    if (numCpts > 0 && loadstorelogentry::allCPUMeta.size() > NUMBEROFMAINCORES) { // At least 1 cpt and checkers exist
        outfile << "cptLen/cpt (checker cycles) " << loadstorelogentry::allCPUMeta[NUMBEROFMAINCORES].baseCPU->ticksToCycles(loadstorelogentry::cptLenTicks/numCpts) << ", cptStartDelay/cpt (checker cycles) " << loadstorelogentry::allCPUMeta[NUMBEROFMAINCORES].baseCPU->ticksToCycles(loadstorelogentry::cptStartDelayTicks/numCpts) << ", cptCheckerStartToFetchDelay/cpt (checker cycles) " << loadstorelogentry::allCPUMeta[NUMBEROFMAINCORES].baseCPU->ticksToCycles(loadstorelogentry::cptCheckerStartToFetchDelayTicks/numCpts) << ", cptCheckerFirstFetchTransAccDelay/cpt (checker cycles) " << loadstorelogentry::allCPUMeta[NUMBEROFMAINCORES].baseCPU->ticksToCycles(loadstorelogentry::cptCheckerFirstFetchTransAccDelayTicks/numCpts) << ", cptCheckerFirstFetchToCommitDelay/cpt (checker cycles) " << loadstorelogentry::allCPUMeta[NUMBEROFMAINCORES].baseCPU->ticksToCycles(loadstorelogentry::cptCheckerFirstFetchToCommitDelayTicks/numCpts) << ", cptCheckerStartToCommitDelay/cpt (checker cycles) " << loadstorelogentry::allCPUMeta[NUMBEROFMAINCORES].baseCPU->ticksToCycles(loadstorelogentry::cptCheckerStartToCommitDelayTicks/numCpts) << ", cptCheckerFirstToLastCommitDelay/cpt (checker cycles) " << loadstorelogentry::allCPUMeta[NUMBEROFMAINCORES].baseCPU->ticksToCycles(loadstorelogentry::cptCheckerFirstToLastCommitDelayTicks/numCpts) << ", cptCheckerLastCommitToDrainDoneDelay/cpt (checker cycles) " << loadstorelogentry::allCPUMeta[NUMBEROFMAINCORES].baseCPU->ticksToCycles(loadstorelogentry::cptCheckerLastCommitToDrainDoneDelayTicks/numCpts) << ", cptCheckerDrainDoneToStartDelay/cpt (checker cycles) " << loadstorelogentry::allCPUMeta[NUMBEROFMAINCORES].baseCPU->ticksToCycles(loadstorelogentry::cptCheckerDrainDoneToStartDelayTicks/numCpts) << ", cptCheckerStartToDrainDoneDelay/cpt (checker cycles) " << loadstorelogentry::allCPUMeta[NUMBEROFMAINCORES].baseCPU->ticksToCycles((loadstorelogentry::cptCheckerStartToCommitDelayTicks + loadstorelogentry::cptCheckerFirstToLastCommitDelayTicks + loadstorelogentry::cptCheckerLastCommitToDrainDoneDelayTicks)/numCpts) << std::endl;
    }
    outfile << "checkpointingCycles " << loadstorelogentry::checkpointingCycles << ", noCheckerCycles " << loadstorelogentry::noCheckerCycles << ", blockingWaitCycles " << loadstorelogentry::blockingWaitCycles << std::endl;
    outfile << "checkerLSLStallCycles ";
    for (int i = 0; i < loadstorelogentry::checkerCPUMeta.size(); ++i) {
        outfile << loadstorelogentry::checkerCPUMeta[i].checkerLSLStallCycles << ",";
    }
    outfile << std::endl;
    outfile.close();

    std::cout << "Written packets: " << loadstorelogentry::mainCPUMeta[0].numberOfEntries << ". Read packets: " << loadstorelogentry::mainCPUMeta[0].numberOfReads << ".\n";

    outfile.open(simout.resolve("error_log.txt"), std::ios::trunc);


    outfile << "number_detected_errors: " << errordetection::detected_errors
            << "\nnumber_injected_errors: "
            << errorinjection::numberOfErroneousWrites
            + errorinjection::numberOfErroneousReads
            + errorinjection::numberOfErroneousArchStates
            + errorinjection::numberOfErroneousTCStates
            + errorinjection::numberOfErroneousOpClass
            << "\nwrite_injected_errors: " << errorinjection::numberOfErroneousWrites
            << "\nread_injected_errors: " << errorinjection::numberOfErroneousReads
            << "\narch_injected_errors: " << errorinjection::numberOfErroneousArchStates
            << "\ntcstate_injected_errors: " << errorinjection::numberOfErroneousTCStates
            << "\nopclass_injected_errors: " << errorinjection::numberOfErroneousOpClass
            << std::endl;
    // std::cout << "Error recovery penalty: "
    //         << errordetection::detectionTime.mean << " (detection) + "
    //         << errordetection::memoryRecoveryTime.mean << " (memory recovery) = "
    //         << errordetection::detectionTime.mean + errordetection::memoryRecoveryTime.mean
    //         << std::endl;

#if LOGERRORS


    outfile << "injection_lapses: ";
    for (int i = 0; i < errorinjection::lapses.size(); i++) {
        outfile << errorinjection::lapses[i] << ", ";
    }




    uint64_t penalty = 0;
    uint64_t mempenalty = 0;
    uint64_t rerunpenalty = 0;
#if PARADVFS
    outfile << "\ndetection_points: ";
    for (int i = 0; i < errordetection::detectionPoints.size(); i++) {
        penalty += errordetection::detectionPoints[i];
        outfile << errordetection::detectionPoints[i] << ", ";
    }
    outfile << "\ndetection_voltages: ";
    for (int i = 0; i < errordetection::detectionVoltage.size(); i++) {
        outfile << errordetection::detectionPoints[i/2] << " ";
        outfile << errordetection::detectionVoltage[i] << "\n";
    }
    outfile << "\nswitch_points: ";
    for (int i = 0; i < errordetection::voltageSwitchTime.size(); i++) {
        outfile << errordetection::voltageSwitchTime[i] << " ";
        outfile << errordetection::voltageSwitchPoints[i] << "\n";

    }
#endif

    outfile << "\ndetection_times: ";
    for (int i = 0; i < errordetection::detectionTime.size(); i++) {
        penalty += errordetection::detectionTime[i];
        rerunpenalty += errordetection::detectionTime[i];
        outfile << errordetection::detectionTime[i] << ", ";
    }


    outfile << "\nmemory_recovery: ";
    for (int i = 0; i < errordetection::memoryRecoveryTime.size(); i++) {
        penalty += 313*errordetection::memoryRecoveryTime[i];
        mempenalty +=313*errordetection::memoryRecoveryTime[i];
        outfile << errordetection::memoryRecoveryTime[i] << ", ";
    }
    outfile << "\ntotal_recovery_penalty: " << penalty << std::endl;
    outfile << "\nrerun_penalty: " << rerunpenalty << std::endl;

#endif


    outfile << "\nrerun_penalty: " << errordetection::total_rollback_recovery << std::endl;
    outfile << "\nmin_rerun_penalty: " <<  errordetection::min_rollback_recovery << std::endl;
    outfile << "\nmax_rerun_penalty: " <<  errordetection::max_rollback_recovery << std::endl;

    outfile << "\nmemory_recovery_penalty: " << errordetection::total_memory_recoveries*313ul << std::endl;
    outfile << "\nmin_memory_recovery_penalty: " << errordetection::min_memory_recoveries*313ul << std::endl;
    outfile << "\nmax_memory_recovery_penalty: " << errordetection::max_memory_recoveries*313ul << std::endl;

    outfile << "missed_errors: " << errorinjection::undetectedErrors << std::endl;
    outfile << "false_positives: " << errorinjection::falsePositives << std::endl;

    outfile << "cpt_only_unchanged_injection: " << errorinjection::cptOnlyUnchangedInjections << std::endl;
    outfile << "changed_injections: " << errorinjection::changedInjections << std::endl;
    outfile << "unchanged_injections: " << errorinjection::unchangedInjections << std::endl;
    outfile << "total_injections: " << (errorinjection::changedInjections + errorinjection::unchangedInjections) << std::endl;

#if PARADVFS
    for (int i=0; i< NUMBEROFMAINCORES; i++) {
        outfile << "\n current voltage: " << loadstorelogentry::mainCPUMeta[i].voltage;
        outfile << "\n highest voltage error: " <<  loadstorelogentry::mainCPUMeta[i].highestVoltageError;
    }
#endif

    outfile << "multiDestOpClasses: " << std::endl;
    for (auto op_inst: errorinjection::multiDestOpClasses) {
        outfile << op_inst.first << ":";
        for (auto inst_str: op_inst.second) {
            outfile << " " << inst_str;
        }
        outfile << std::endl;
    }

    outfile << "destRegOpClasses: " << std::endl;
    for (auto op_inst: errorinjection::destRegOpClasses) {
        outfile << op_inst.first << ":";
        for (auto reg_type: op_inst.second) {
            outfile << " " << reg_type;
        }
        outfile << std::endl;
    }

    outfile.close();

    assert(errorinjection::lapses.size() ==
           errorinjection::numberOfErroneousWrites
           + errorinjection::numberOfErroneousReads
           + errorinjection::numberOfErroneousArchStates
           + errorinjection::numberOfErroneousTCStates
           + errorinjection::numberOfErroneousOpClass);
}


void add_cpu(BaseCPU* cpu, int cpuID) {
    if (cpuID>=NUMBEROFCHECKERCORESPERCORE*NUMBEROFMAINCORES+NUMBEROFMAINCORES) return;

    loadstorelogentry::allCPUMeta[cpuID].baseCPU = cpu;
    std::cout << "got " << ((cpu->isChecker()) ? "checker " : "") << "cpu "
              << cpuID << std::endl;
}

      bool loadstorelogentry::do_read(PacketPtr pkt, int id) {

            if (!valid) {
              /* This can happen if an erroneous control flow executes a memory
               * operation that should not exist. */
              if (debugFlag) std::cout << "Invalid read for checker " << id << std::endl;
              return false;
            }

            // Second round means it was not the stored midop
            assert(id < NUMBEROFMAINCORES * NUMBEROFCHECKERCORESPERCORE);
            if (!debug::MinorStrictLdStOrder) {
                checkerCPUMeta.at(id).dataAddressOffset = 
                    pkt->req->getVaddr() - addr;
            }
            Addr offset = checkerCPUMeta.at(id).dataAddressOffset;

            if (debugFlag) {
                std::cout << pkt->print() << " address " <<  pkt->req->getVaddr() << ", PC " << pkt->req->getPC() << std::endl;
            }

            bool address_mismatch =
                (debug::MinorStrictLdStOrder) ?
                    // Should be an exact match when strictly in-order
                    addr + offset != pkt->req->getVaddr() :
                    // Address should be in range if not strictly in-order
                    !(pkt->req->getVaddr() >= addr && offset < data.size() &&
                      // Should be an exact match if not a micro-op
                      (data.size() != pkt->req->getSize() || offset == 0));
            bool size_mismatch = !address_mismatch && (offset + pkt->req->getSize() > data.size());
            if (address_mismatch || size_mismatch ||
                (load != pkt->isRead()  && !((pkt->isRead() && pkt->isLLSC()) || 
                                             (pkt->isRead() && pkt->isWrite())) // exclude access type error due to swap requests
                )) {
                    std::stringstream ss;
                 if (address_mismatch || size_mismatch) {
                   ss << "=====> Mismatching addresses (log: " << addr << "+" << data.size() << " ; pkt: " << pkt->req->getVaddr() << "+" << pkt->req->getSize() << ")";
                   if (load != pkt->isRead()) {
                     ss << " and status (log is a " << (load?'r':'w') << ")";
                   }
                 } else if (load != pkt->isRead() && !(pkt->isRead() && pkt->isWrite())) { // swap is both read and write
                   ss << "=====> Mismatching status (log is a " << (load?'r':'w') << ")";
                 } else {
                   std::cout << "=====> Unknown mismatch at time " << mainCPUMeta[id/NUMBEROFCHECKERCORESPERCORE].timestamp << std::endl;
                   std::cout << "  Addresses --> log: " << addr << " ; pkt: " << pkt->req->getVaddr();
                   std::cout << "  Status    --> log is a " << (load?'r':'w') << " ; pkt->isRead() is " << pkt->isRead() << " ; pkt->isWrite() is " << pkt->isWrite() << "\n" << std::endl;
                   dumpLocalLogState(id);
                   assert(false);
                 }
                 ss << " at time " << mainCPUMeta[id/NUMBEROFCHECKERCORESPERCORE].timestamp << ", seqNum " << pkt->req->getLdStLogSeqNum() << std::endl;
                 pkt->req->setLdStLogAccErrMsg(ss.str());
                 if (debugFlag) {
                   std::cout << ss.str();
                   dumpLocalLogState(id);
                 }

                return false;
            }

            uint64_t newTime = curTick() - time;

            minTime = std::min(minTime,newTime);
            maxTime = std::max(maxTime,newTime);

            addToHisto(newTime);

            meanTime += newTime;
            times++;

            if (pkt->isRead() && pkt->isWrite()) { // Swap commands are both read and write
                if (debug::LoadStoreLogSwap) {
                    DPRINTF(LoadStoreLogSwap, "PC: %x %x\n", 
                        pkt->req->getPC(), pc);
                    std::stringstream ss;
                    ss << "oldData: [";
                    for (int i=0; i<pkt->req->getSize(); ++i) {
                        ss << std::hex << (uint64_t) oldData.data()[offset+i];
                        if (i == pkt->req->getSize() - 1) {
                            ss << "] -> ";
                        } else {
                            ss << ", ";
                        }
                    }
                    ss << "data: [";
                    for (int i=0; i<pkt->req->getSize(); ++i) {
                        ss << std::hex << (uint64_t) data.data()[offset+i];
                        if (i == pkt->req->getSize() - 1) {
                            ss << "]" << std::endl;
                        } else {
                            ss << ", ";
                        }
                    }
                    DPRINTF(LoadStoreLogSwap, "%s", ss.str());
                }
                if (pkt->req->isAtomic()) { // Atomic swaps
                    // No write data attached to packet initially
                    // Atomic insts reads data first then atomic op on read data then write
                    // The AtomicOpFunctor itself includes the register value used
                    // pkt->deleteData();
                    pkt->setData(oldData.data() + offset); // reads old data
                    if (debug::LoadStoreLogSwap) {
                        std::stringstream ss;
                        ss << "Atomic swap read: [";
                        for (int i = 0; i < pkt->req->getSize(); ++i) {
                            ss << std::hex << (uint64_t) oldData.data()[offset + i];
                            if (i == pkt->req->getSize() - 1) {
                                ss << "]" << std::endl;
                            } else {
                                ss << ", ";
                            }
                        }
                        DPRINTF(LoadStoreLogSwap, "%s", ss.str());
                    }
                    // Written data is not available in the store buffer
                    // The loadstorelog seems to not capture the value after 
                    // atomic op happened in cache, the read data is stored in 
                    // data.data() instead
                    // Can't really check if data written is correct
                    // TODO: Fix this?
                } else { // Non-atomic swaps
                    // Looks like ARM doesn't have non-atomic swap
                    assert(false);
                    // Have write data attached to packets
                    if (debug::LoadStoreLogSwap) {
                        std::stringstream ss;
                        ss << "Swap write: [";
                        for (int i=0; i<pkt->req->getSize(); ++i) {
                            ss << std::hex << (uint64_t) pkt->getPtr<uint8_t>()[i];
                            if (i == pkt->req->getSize() - 1) {
                                ss << "]" << std::endl;
                            } else {
                                ss << ", ";
                            }
                        }
                        DPRINTF(LoadStoreLogSwap, "%s", ss.str());
                    }
                    // Check write content first
                    if (!compare_data(pkt, id)) {
                        if (debugFlag) {
                            std::cout << "Different writes at address " << addr << " and time " << mainCPUMeta[id/NUMBEROFCHECKERCORESPERCORE].timestamp << std::endl;
                            std::cout << "    Packet: (" << pkt->req->getSize() << ") [";
                            for (int i=0; i<pkt->req->getSize(); ++i) {
                            std::cout << (uint64_t)pkt->getPtr<uint8_t>()[i] << (i==pkt->req->getSize()-1?"]":", ");
                            }
                            dumpLocalLogState(id);
                        }

                        return false;
                    } 
                    // Then read
                    pkt->setData(oldData.data() + offset); // reads old data
                    if (debug::LoadStoreLogSwap) {
                        std::stringstream ss;
                        ss << "Swap read: [";
                        for (int i = 0; i < pkt->req->getSize(); ++i) {
                            ss << std::hex << (uint64_t) oldData.data()[offset + i];
                            if (i == pkt->req->getSize() - 1) {
                                ss << "]" << std::endl;
                            } else {
                                ss << ", ";
                            }
                        }
                        DPRINTF(LoadStoreLogSwap, "%s", ss.str());
                    }
                }
            } else if (pkt->isRead()) {
                pkt->setData(data.data()+offset);
                if (debug::LoadStoreLogChecker) {
                    std::stringstream ss;
                    ss << "read data: (" << pkt->req->getSize() << ") [";
                    for (int i = 0; i < pkt->req->getSize(); ++i) {
                        ss << (uint64_t)pkt->getPtr<uint8_t>()[i]
                           << (i == pkt->req->getSize() - 1 ? "]" : ", ");
                    }
                    DPRINTF(LoadStoreLogChecker, "Packet %s, %s\n",
                            pkt->print(), ss.str());
                    dumpLocalLogState(id);
                }
            } else if (isSC) {
                pkt->req->setExtraData(extra_data);
            }

            if (pkt->isWrite() && !pkt->isRead()) {
              if (!compare_data(pkt, id)) {
                if (debugFlag) {
                    std::cout << "Different writes at address " << addr << " and time " << mainCPUMeta[id/NUMBEROFCHECKERCORESPERCORE].timestamp << std::endl;
                    std::cout << "    Packet: (" << pkt->req->getSize() << ") [";
                    for (int i=0; i<pkt->req->getSize(); ++i) {
                    std::cout << (uint64_t)pkt->getPtr<uint8_t>()[i] << (i==pkt->req->getSize()-1?"]":", ");
                    }
                    dumpLocalLogState(id);
                }

                return false;
              }
            } else {
              // Packets here should only be reads or writes
              assert(pkt->isRead());
            }

            bool sizeDifference = (data.size() != pkt->req->getSize());

            if (sizeDifference) {
              if (debugFlag) std::cout << "Size difference (" << (pkt->isRead()?'r':'w') << ") : " << pkt->req->getSize() << " (requested) != " << data.size() << " (stored) for address " <<  pkt->req->getVaddr() << std::endl;
            }

            checkerCPUMeta.at(id).dataAddressOffset += pkt->req->getSize();
            if (checkerCPUMeta.at(id).dataAddressOffset > data.size()) {
                return false;
            } else if (debug::MinorStrictLdStOrder && 
                       checkerCPUMeta.at(id).dataAddressOffset == data.size())
            {
                // End of micro op or macro op, only works if micro op accesses are consecutive
                checkerCPUMeta.at(id).dataAddressOffset = 0;
            }

            return true;
        }

        bool loadstorelogentry::do_read(PacketPtr pkt, ThreadContext* tc) {

            int size_of_segment = logsize;

            int id = tc->contextId()-NUMBEROFMAINCORES;
            assert(id < NUMBEROFCHECKERCORESPERCORE*NUMBEROFMAINCORES);

            // if (debugFlag) std::cout << "Starting entryIndices[id] = " << entryIndices[id] << std::endl;

            DPRINTF(LoadStoreLogSeqNum, "instSeqNum %lld, entryIndex %d, "
                "startingSeqNum+entryIndex %lld\n",
                pkt->req->getLdStLogSeqNum(), 
                checkerCPUMeta.at(id).entryIndices, 
                checkerCPUMeta.at(id).startingSeqNum + 
                    checkerCPUMeta.at(id).entryIndices);

            int mainCPUID = id/NUMBEROFCHECKERCORESPERCORE;

            mainCPUMeta[mainCPUID].numberOfReads++;

            auto minorCPU =
                dynamic_cast<MinorCPU *>(allCPUMeta[tc->contextId()].baseCPU);
            if (minorCPU &&
                (allCPUMeta[tc->contextId()]
                         .baseCPU->getContext(0)
                         ->status() == ThreadContext::Status::Suspended ||
                 minorCPU->isDraining)) // drainState() is not set in Minor
            {
                /* This occasionnally happens, but it seems harmless. */
                return true;
            } else if (allCPUMeta[tc->contextId()]
                               .baseCPU->getContext(0)
                               ->status() ==
                           ThreadContext::Status::Suspended ||
                       allCPUMeta[tc->contextId()].baseCPU->drainState() ==
                           DrainState::Draining)
            {
                DPRINTF(LoadStoreLogChecker,
                        "Checker %d reading while "
                        "Suspended? %d, Draining? %d, at time %d.\n",
                        id,
                        (allCPUMeta[tc->contextId()]
                             .baseCPU->getContext(0)
                             ->status() == ThreadContext::Status::Suspended),
                        (allCPUMeta[tc->contextId()].baseCPU->drainState() ==
                         DrainState::Draining),
                        mainCPUMeta[mainCPUID].timestamp);
            }

            if (pkt->req->getLdStLogSeqNum() - 
                checkerCPUMeta.at(id).startingSeqNum < 
                checkerCPUMeta.at(id).entryIndices) 
            {
                DPRINTF(LoadStoreLogSeqNum, "Reordering found\n");
            }
            if (debug::MinorStrictLdStOrder) {
                // Check if loadstorelog sequence number is correct
                assert(pkt->req->getLdStLogSeqNum() - 
                    checkerCPUMeta.at(id).startingSeqNum == 
                    checkerCPUMeta.at(id).entryIndices);
            } else {
                checkerCPUMeta.at(id).entryIndices = 
                    pkt->req->getLdStLogSeqNum() - 
                    checkerCPUMeta.at(id).startingSeqNum;
            }

            bool accessed = false;

            bool foundAlready = true;
            assert(id < NUMBEROFMAINCORES * NUMBEROFCHECKERCORESPERCORE);
            

            /** Performing the actual read */
            if (!(checkerCPUMeta.at(id).entryIndices >= size_of_segment) && !checkerCPUMeta.at(id).entries[checkerCPUMeta.at(id).entryIndices].do_read(pkt, id)) {
              // Genuine lookup failure
              if (debugFlag) std::cout << "do_read failure on " << (checkerCPUMeta.at(id).entries[checkerCPUMeta.at(id).entryIndices].valid?"":"in") << "valid entry; packet (" << (pkt->isWrite()?"W":pkt->isRead()?"R":"unknown!") << ") at " << (uint64_t)pkt->req->getVaddr() << std::endl;
              foundAlready = false;
            }


            auto previousIndices = checkerCPUMeta.at(id).entryIndices;
            if (foundAlready) {
                if (debug::MinorStrictLdStOrder && 
                    checkerCPUMeta.at(id).dataAddressOffset == 0) 
                {
                    checkerCPUMeta.at(id).entryIndices++;
                }
                accessed = true;
            } else /** Dealing with lookup failures */
            if (pkt->req->isPrefetch()) {
                accessed = true;
                foundAlready = true;
                printf("prefetch %ld %s on %d, ignored\n", pkt->req->getVaddr(), (pkt->print()).c_str(), id);
                std::cout << pkt->cmdString() << "\n";
            } else if (pkt->isWrite()) {
                accessed = false;
                foundAlready = false;
            }
            else if (mainCPUMeta[mainCPUID].previousThreadContext.initialized) {
                assert(!pkt->req->isUncacheable());
                // std::cout << "Not found entry: ";
                // std::cout << "address = " << pkt->req->getVaddr() << " ; status is " << (pkt->isRead()?'r':'w') << std::endl;
                // if (entryIndices[id] < (size_of_segment-1) || entryIndices[id] < (size_of_segment-1) ) {
                //     printf("not found %ld %c on %d at address %lX, expected at %d, %ld %c addr %lX\n", pkt->req->getVaddr(), pkt->isRead()? 'r':'w', id,
                //             pkt->req->getPC(), entryIndices[id], entries[size_of_segment*id+entryIndices[id]].addr,
                //             entries[size_of_segment*id+entryIndices[id]].load? 'r':'w', entries[size_of_segment*id+entryIndices[id]].pc);
                //     std::cout << pkt->cmdString() << "\n";
                //     /*std::cout << pkt->cmdString() << "\n";
                //     for (int x=0; x<20; x++) {
                //             int y=std::min(std::max(entryIndices[id]+x-10,0),size_of_segment-1);
                //             printf("%d : %ld\n",y, entries[size_of_segment*id+y].addr);
                //     }
                //     getchar();*/
                // }
                // dumpLocalLogState(id);
            // } else {
            //   std::cout << "Not found entry with uninitialised previousThreadContext[mainCPUID="<<mainCPUID<<"]" << std::endl;
            //   std::cout << "address = " << pkt->req->getVaddr() << " ; status is " << (pkt->isRead()?'r':'w') << std::endl;
            //   dumpLocalLogState(id);
            }

            // The -1 in the following indices come from the entryIndices[id]++ above
            if (!foundAlready || !checkerCPUMeta.at(id).entries[previousIndices].valid || previousIndices >= size_of_segment || !mainCPUMeta[mainCPUID].previousThreadContext.initialized) {

              if (loadstorelogentry::debugFlag) {
                std::cout << ">>>>>!<<<<< sleeping checker cpu " << id << " at time " << mainCPUMeta[mainCPUID].timestamp << ", seqNum " << pkt->req->getLdStLogSeqNum() << " (packet time: " << checkerCPUMeta.at(id).entries[checkerCPUMeta.at(id).entryIndices].time << ") because ";
                if (foundAlready) {
                  if (previousIndices < size_of_segment) {
                    if (checkerCPUMeta.at(id).entries[previousIndices].valid) {
                      assert(!mainCPUMeta[mainCPUID].previousThreadContext.initialized);
                      std::cout << "unitialized previous thread context!" << std::endl;
                    } else { // This should never happen
                      std::cout << "invalid entry!" << std::endl;
                      std::cout << "  -> Entry index is " << previousIndices
                                << " = size_of_segment (" << size_of_segment
                                << ")*id (" << id
                                << ") ; accessed is " << accessed << std::endl;
                    }
                  } else std::cout << "entryIndices[id]-1 = " << previousIndices << " >= size_of_segment = " << size_of_segment << std::endl;
                } else std::cout << "not found entry!" << std::endl;
              }

                //suspend CPU, maybe reawaken O3.
                // not_found_sleep(id);
                // printf("setting not_found_sleep\n");
                pkt->req->setLdStLogAccErr();
            } else if (!accessed) { // If the cause is not one of the above
              if (loadstorelogentry::debugFlag) std::cout << ">>>!<<< sleeping checker cpu " << id << " at time " << mainCPUMeta[mainCPUID].timestamp << ", seqNum " << pkt->req->getLdStLogSeqNum() << " (packet time: " << checkerCPUMeta.at(id).entries[checkerCPUMeta.at(id).entryIndices].time << ") because not accessed entry" << std::endl;
                // printf("setting not_found_sleep2\n");
                pkt->req->setLdStLogAccErr();
                // not_found_sleep(id);
            }

            //if (debugFlag) std::cout << "ID " << id << " Ending entryIndices[id] = " << checkerCPUMeta.at(id).entryIndices  << "Chkpt: " << checkerCPUMeta.at(id).checkpoint_entries << std::endl;

            if (debug::MinorStrictLdStOrder &&
                checkerCPUMeta.at(id).entryIndices ==
                    checkerCPUMeta.at(id).checkpoint_entries)
            {
                // Cannot use whether we are accessing the last entry to
                // determine whether we should sleep with out-of-order accesses 
                // std::cout << "Sleepguarding " << id << " at
                // " << checkerCPUMeta.at(id).entryIndices  << std::endl;
                allCPUMeta[id+NUMBEROFMAINCORES].baseCPU->sleepGuardOn=true;
                DPRINTF(LoadStoreLogSleepGuard,
                        "do_read CPU %d sleepGuardOn set for CPU %d\n",
                        tc->contextId(), tc->contextId());
            }

            return accessed;

        }



        bool loadstorelogentry::do_write(loadstorelogentry l, int cpuID, bool newline, bool mayMergeMicroop, uint64_t currentCommittedInstructions) {
            int size_of_segment = logsize;
            bool merged = false;

            assert(cpuID < mainCPUMeta.size());
            assert(mainCPUMeta[cpuID].current_entry < size_of_segment);
            // If writing first entry of the segment, record number of committed instructions
            if (checkerCPUMeta[mainCPUMeta[cpuID].current_segment_to_fill].currentCommittedInstructions == 0) {
                if (currentCommittedInstructions == std::numeric_limits<uint64_t>::max()) {
                    // Reached mainDoCheckpoint without any LSL entry
                    addToLengthToFirstLSLHisto(checkerCPUMeta.at(mainCPUMeta[cpuID].current_segment_to_fill).committedInstructions);
                } else {
                    addToLengthToFirstLSLHisto(currentCommittedInstructions);
                }
            }
            checkerCPUMeta[mainCPUMeta[cpuID].current_segment_to_fill].currentCommittedInstructions = currentCommittedInstructions;
            if (mayMergeMicroop && mainCPUMeta[cpuID].current_entry > 0) {
                loadstorelogentry& last_entry = checkerCPUMeta[mainCPUMeta[cpuID].current_segment_to_fill].entries[mainCPUMeta[cpuID].current_entry -1 ];
                if (debugFlag) std::cerr << "load " << last_entry.load << " " << l.load << ", isSC" << last_entry.isSC << " " << l.isSC << ", PC " << last_entry.pc << "/" << last_entry.microPC << " " << l.pc << "/" << l.microPC << ", Addr " << last_entry.addr << " " << l.addr << ", size " << last_entry.data.size() << " " << l.data.size() << ", name " << last_entry.inst_name << " " << l.inst_name << std::endl;
                if(last_entry.pc == l.pc && last_entry.microPC < l.microPC) {
                    assert(last_entry.load == l.load);
                    assert(last_entry.isSC == l.isSC);
                    // assert(last_entry.inst_name == l.inst_name); inst_name may differ for micro-ops, using pc to identify the same instruction
                    assert(last_macro_addrs[mainCPUMeta[cpuID].current_segment_to_fill] == l.addr); // Assumption: micro op load/store accesses consecutive addresses in order
                    last_entry.microPC = l.microPC; // Update microPC for next comparison
                    last_macro_addrs[mainCPUMeta[cpuID].current_segment_to_fill] += l.data.size();
                    last_entry.data.insert( last_entry.data.end(), l.data.begin(), l.data.end() );
                    if (last_entry.oldData.size() > 0 || l.oldData.size() > 0) { // Assumption: oldData is consecutive where existent
                        last_entry.oldData.insert( last_entry.oldData.end(), l.oldData.begin(), l.oldData.end() );
                    }
                    merged = true;
                }
            } else {
                if (debugFlag) std::cerr << "load " << l.load << ", isSC " << l.isSC << ", PC " << l.pc << ", Addr " << l.addr << ", size "  << l.data.size() << ", name " << l.inst_name << std::endl;
            }
            if (!merged) {
                last_macro_addrs[mainCPUMeta[cpuID].current_segment_to_fill] = l.addr + l.data.size();

                checkerCPUMeta[mainCPUMeta[cpuID].current_segment_to_fill].entries[mainCPUMeta[cpuID].current_entry ] = l;
                if (useHash) {
                    /* calculate hash */
                    auto checkerID = mainCPUMeta[cpuID].current_segment_to_fill;
                    checkerCPUMeta[checkerID].calcExpectedHash(mainCPUMeta[cpuID].current_entry);
                }

                mainCPUMeta[cpuID].current_entry++;

                checkerCPUMeta[mainCPUMeta[cpuID].current_segment_to_fill].checkpoint_entries++;
            }
            if (mainCPUMeta[cpuID].current_segment_to_fill+NUMBEROFMAINCORES < allCPUMeta.size() &&
                !checkerCPUMeta[mainCPUMeta[cpuID].current_segment_to_fill].copyingRegister &&
                allCPUMeta[mainCPUMeta[cpuID].current_segment_to_fill+NUMBEROFMAINCORES].baseCPU->sleepGuardOn){
                //std::cout << "Desleepguarding " << current_segment_to_fill[cpuID] << " at " << checkpoint_entries[current_segment_to_fill[cpuID]] << std::endl;
                allCPUMeta[mainCPUMeta[cpuID].current_segment_to_fill+NUMBEROFMAINCORES].baseCPU->sleepGuardOn=false;
                DPRINTF(LoadStoreLogSleepGuard,
                        "do_write CPU %d sleepGuardOn unset for CPU %d\n",
                        cpuID,
                        mainCPUMeta[cpuID].current_segment_to_fill +
                            NUMBEROFMAINCORES);
                allCPUMeta[mainCPUMeta[cpuID].current_segment_to_fill+NUMBEROFMAINCORES].baseCPU->wakeup(0);
                if (checkerCPUMeta.at(mainCPUMeta[cpuID]
                    .current_segment_to_fill).checkerStartWakeupTick == 0) 
                { // First time wakeup
                    checkerCPUMeta.at(mainCPUMeta[cpuID]
                        .current_segment_to_fill).checkerStartWakeupTick
                        = curTick();
                    cptStartDelayTicks += 
                        checkerCPUMeta.at(mainCPUMeta[cpuID]
                            .current_segment_to_fill).checkerStartWakeupTick
                        - checkerCPUMeta.at(mainCPUMeta[cpuID]
                            .current_segment_to_fill).mainStartingTick;
                    if (checkerCPUMeta.at(mainCPUMeta[cpuID]
                        .current_segment_to_fill).checkerDrainDoneTick != 0) 
                    { // Not first checkpoint on the checker, was drained before
                        cptCheckerDrainDoneToStartDelayTicks += 
                            checkerCPUMeta.at(mainCPUMeta[cpuID]
                                .current_segment_to_fill).checkerStartWakeupTick
                            - checkerCPUMeta.at(mainCPUMeta[cpuID]
                                .current_segment_to_fill).checkerDrainDoneTick;
                        // Reset stat after use
                        checkerCPUMeta.at(mainCPUMeta[cpuID]
                            .current_segment_to_fill).checkerDrainDoneTick = 0;
                    }
                }
                if (!debug::MinorStrictLdStOrder) {
                    auto O3DcachePort = dynamic_cast<o3::LSQ::DcachePort *>(&(
                        allCPUMeta[mainCPUMeta[cpuID].current_segment_to_fill +
                                   NUMBEROFMAINCORES]
                            .baseCPU->getDataPort()));
                    auto MinorDcachePort = dynamic_cast<
                        MinorCPU::MinorCPUPort *>(&(
                        allCPUMeta[mainCPUMeta[cpuID].current_segment_to_fill +
                                   NUMBEROFMAINCORES]
                            .baseCPU->getDataPort()));
                    if (O3DcachePort) {
                        O3DcachePort->forceRetry();
                    } else if (MinorDcachePort) {
                        MinorDcachePort->forceRetry();
                    }
                    assert(O3DcachePort || MinorDcachePort);
                }
                checkerCPUMeta[mainCPUMeta[cpuID].current_segment_to_fill].activeChecker = true;
            }

            //std::cout << "Segment " <<  current_segment_to_fill[cpuID] << " ents " <<  checkpoint_entries[current_segment_to_fill[cpuID]] << std::endl;
            //if (loadstorelogentry::segmentFree[current_segment_to_fill[cpuID]]) std::cout<<"Free!"<<std::endl;
#if !PARAGLIDER
            mainCPUMeta[cpuID].current_size+= l.load? 2 : STORESIZE;
#else
            mainCPUMeta[cpuID].current_size+= l.load? 2 : 2 + newline? 9 : 0; //make conditional based on timestamp.
                        checkerCPUMeta[mainCPUMeta[cpuID].current_segment_to_fill].checkpoint_cachelines+= newline? 1 : 0;

#endif

#if !PARAGLIDER
            if (mainCPUMeta[cpuID].current_size >= size_of_segment-2 ) { //for maximal size fit.
#else
            if (mainCPUMeta[cpuID].current_size >= size_of_segment-10 ) { //for maximal size fit of cacheline plus write.
#endif
                return true;
            }
            return false;

        }

bool
loadstorelogentry::try_read(PacketPtr pkt, ThreadContext *tc)
{
    int id = tc->contextId() - NUMBEROFMAINCORES;
    assert(id < NUMBEROFCHECKERCORESPERCORE*NUMBEROFMAINCORES);
    // Current loadstorelog segment has been filled completely or
    // incoming packet accesses an entry that has already been filled
    bool should_read =
        checkerCPUMeta.at(id).expectedFinalContext.set ||
        (pkt->req->getLdStLogSeqNum() - checkerCPUMeta.at(id).startingSeqNum +
             1 < // +1 because last entry could be incomplete
         checkerCPUMeta.at(id).checkpoint_entries);
    assert(allCPUMeta[getMainID(tc->contextId())].baseCPU->isSleepGuarded ||
           (!allCPUMeta[getMainID(tc->contextId())].baseCPU->isSleepGuarded &&
            checkerCPUMeta.at(id).expectedFinalContext.set));
    if (!should_read && !allCPUMeta[tc->contextId()].baseCPU->sleepGuardOn) {
        DPRINTF(LoadStoreLogSleepGuard,
                "try_read CPU %d sleepGuardOn set for CPU %d, pkt %s\n",
                tc->contextId(), tc->contextId(), pkt->print());
        allCPUMeta[tc->contextId()].baseCPU->sleepGuardOn = true;
    }
    return should_read;
}

void 
loadstorelogentry::CheckerCPUMeta::calcExpectedHash(int current_entry)
{
    if (current_entry > 0) {
        auto const latest = entries[current_entry - 1]; // latest entry that just got confirmed that it was completely written
        int data_size = 0; // load only hashes in address and size, no data
        if (!latest.load) { // either store or swap, need to add in stored data
            if (latest.data.size() > 32) {
                std::cout << "Large data found, size " << latest.data.size() << "B" << " from inst " << latest.inst_name << std::endl;
            } else {
                // assert(latest.data.size() <= 32); // data size should be below 256 bits
                if (latest.data.size() > 0) { // atomic swap probably does not have data yet, the write data is difficult to get
                    data_size = ((latest.data.size() - 1) / 8) + 1; // in 64-bit chunks
                }
            }
        }
        if ((expectedHash_chunk_index + data_size + 1) > 8) // expectedHash_chunk does not have enough space for the next entry
        { 
            expectedHash_message_size += (8 - expectedHash_chunk_index) * 8; // padding within 512-bit chunks
            calcHash(expectedHash); // calculate for the current chunk
        }
        // std::cout << "hashing entry with addr " << std::hex << latest.addr << std::dec << ", size " << latest.data.size();
        expectedHash_chunk[expectedHash_chunk_index] = (latest.addr << 8) | (latest.data.size() & 0x0FF); // add address and size to hash
        expectedHash_chunk_index += 1;
        expectedHash_message_size += 8*8;
        if (!latest.load) { // either store or swap, need to add in data
            if (latest.data.size() > 0 && latest.data.size() <= 32) { // atomic swap probably does not have data yet, the write data is difficult to get
                // std::cout << ", data ";
                // for (auto n : latest.data) {
                //     std::cout << n << ",";
                // }
                assert(latest.data.size() + expectedHash_chunk_index * 8 <= expectedHash_chunk.size() * 8);
                std::memcpy(&expectedHash_chunk[expectedHash_chunk_index], latest.data.data(), latest.data.size());
                expectedHash_chunk_index += ((latest.data.size() - 1) / 8) + 1;
                expectedHash_message_size += (((latest.data.size() - 1) / 8) + 1) * 64;
            }
        }
        assert(expectedHash_chunk_index <= 8);
        // std::cout << std::endl;
    }
    if (!entries[current_entry].valid) { // final entry, need to work out padding
        if (expectedHash_chunk_index >= 7) { // Not enough space left for the padding, add the chunk to hash first
            expectedHash_message_size += (8 - expectedHash_chunk_index) * 8; // padding within 512-bit chunks
            calcHash(expectedHash); // calculate for the current chunk
        }
        expectedHash_chunk[expectedHash_chunk_index] = 1;
        expectedHash_chunk[expectedHash_chunk_index] = expectedHash_chunk[expectedHash_chunk_index] << 63;
        expectedHash_chunk[7] = expectedHash_message_size;
        expectedHash_chunk_index = 8;
        calcHash(expectedHash); // calculate for the final chunk
    }
}

void 
loadstorelogentry::CheckerCPUMeta::calcCheckedHash(loadstorelogentry const l) {
    if (last_entry) { // Check if the current commit should merge into the last entry
        // if (last_entry->seqNum == l.seqNum) { // Merging entry
            // assert(last_entry->pc == l.pc);
            // assert(last_entry->microPC < l.microPC); // microPC not accessible on minor
        if(l.microPC > 0 && last_entry->pc == l.pc && last_entry->microPC < l.microPC) {
            assert(last_entry->load == l.load);
            assert(last_entry->isSC == l.isSC);
            if (last_entry->addr + last_entry->data.size() != l.addr) {
                std::cerr << "Last addr " << std::hex << last_entry->addr << " + size " << std::dec << last_entry->data.size() << " (" << std::hex << last_entry->addr + last_entry->data.size() << ") not equal to current addr " << l.addr << " insts " << last_entry->inst_name << ", " << l.inst_name << ", load " << l.load << std::endl;
            }
            assert(last_entry->addr + last_entry->data.size() == l.addr);
            // last_entry->microPC = l.microPC; // microPC not accessible on minor
            last_entry->data.insert( last_entry->data.end(), l.data.begin(), l.data.end() );
        } else { // Not merging entries, load last_entry into hash_chunk
            assert(last_entry->valid);
            int data_size = 0; // load only hashes in address and size, no data
            if (!last_entry->load) { // either store or swap
                if (last_entry->data.size() <= 32) {
                    assert(last_entry->data.size() <= 32); // need to add in data
                    if (last_entry->data.size() > 0) { // atomic swap probably does not have data yet, the write data is difficult to get
                        data_size = ((last_entry->data.size() - 1) / 8) + 1; // in 64-bit chunks
                    }
                }
            }
            if ((hash_chunk_index + data_size + 1) > 8) // hash_chunk is full, cannot take in new entry in full
            { 
                hash_message_size += (8 - hash_chunk_index) * 8; // padding within 512-bit chunks
                calcHash(hash); // calculate for the current chunk
            }
            // std::cerr << "hashing entry with addr " << std::hex << last_entry->addr << std::dec << ", size " << last_entry->data.size();
            hash_chunk[hash_chunk_index] = (last_entry->addr << 8) | (last_entry->data.size() & 0x0FF); // add address and size to hash
            hash_chunk_index += 1;
            hash_message_size += 8*8;
            if (!last_entry->load) { // either store or swap, need to add in data
                if (last_entry->data.size() > 0 && last_entry->data.size() <= 32) { // atomic swap probably does not have data yet, the write data is difficult to get
                    // std::cerr << ", data ";
                    // for (auto n : last_entry->data) {
                    //     std::cerr << n << ",";
                    // }
                    assert(last_entry->data.size() + hash_chunk_index * 8 <= hash_chunk.size() * 8);
                    std::memcpy(&hash_chunk[hash_chunk_index], last_entry->data.data(), last_entry->data.size());
                    hash_chunk_index += ((last_entry->data.size() - 1) / 8) + 1;
                    hash_message_size += (((last_entry->data.size() - 1) / 8) + 1) * 64;
                }
            }
            assert(hash_chunk_index <= 8);
            // std::cerr << std::endl;
            // Copy the incoming loadstorelogentry to last_entry
            *last_entry = l;
        }
    } else { // Allocate a new last_entry (Should only happen on a new checkpoint)
        last_entry = new loadstorelogentry();
        // Copy the incoming loadstorelogentry to last_entry
        *last_entry = l;
    }
    // If the last_entry is the last of the segment, adjust padding and calculate final hash
    if (!last_entry->valid) { // final entry, need to work out padding
        if (hash_chunk_index >= 7) { // Not enough space left for the padding, add the chunk to hash first
            hash_message_size += (8 - hash_chunk_index) * 8; // padding within 512-bit chunks
            calcHash(hash); // calculate for the current chunk
        }
        hash_chunk[hash_chunk_index] = 1;
        hash_chunk[hash_chunk_index] = hash_chunk[hash_chunk_index] << 63;
        hash_chunk[7] = hash_message_size;
        calcHash(hash); // calculate for the final chunk
        delete last_entry;
        last_entry = nullptr;
    } 
}

void 
loadstorelogentry::CheckerCPUMeta::calcHash(std::array<uint64_t, 8>& in_hash) {
    uint64_t mask = 0x0FFFFFFFF; // mask for lower 32 bits
    // copied from pseudo code for each chunk from https://en.wikipedia.org/wiki/SHA-2#Implementations
    // copy chunk into first 16 words w[0..15] of the message schedule array

    // if (&in_hash == &hash) {
    //     for (auto i : hash_chunk) {
    //         std::cerr << std::hex << i << ",";
    //     }
    //     std::cerr << std::endl;
    // } else {
    //     for (auto i : expectedHash_chunk) {
    //         std::cout << std::hex << i << ",";
    //     }
    //     std::cout << std::endl;
    // }

    std::array<uint64_t, 64> w;
    for (int i = 0; i < 16; ++i) {
        if (&in_hash == &hash) {
            w[i] = (i%2 == 1)? (hash_chunk[i/2] & mask) : // lower 32 bits 
                               (hash_chunk[i/2] >> 32); // higher 32 bits
        } else {
            w[i] = (i%2 == 1)? (expectedHash_chunk[i/2] & mask) : // lower 32 bits 
                               (expectedHash_chunk[i/2] >> 32); // higher 32 bits
        }
    }
    // Extend the first 16 words into the remaining 48 words w[16..63] of the message schedule array:
    for (int i = 16; i < 64; ++i) {
        uint64_t s0 = ((((w[i-15] << 32) | w[i-15]) >> 7) & mask) ^ // (w[i-15] rightrotate  7) xor
                      ((((w[i-15] << 32) | w[i-15]) >> 18) & mask) ^ // (w[i-15] rightrotate 18) xor 
                      (w[i-15] >> 3); // (w[i-15] rightshift  3)
        uint64_t s1 = ((((w[i-2] << 32) | w[i-2]) >> 17) & mask) ^ // (w[i-2] rightrotate 17) xor 
                      ((((w[i-2] << 32) | w[i-2]) >> 19) & mask) ^ // (w[i-2] rightrotate 19) xor
                      (w[i-2] >> 10); // (w[i-2] rightshift 10)
        w[i] = (w[i-16] + s0 + w[i-7] + s1) & mask;
    }
    // Initialize working variables to current hash value:
    uint64_t a = in_hash[0];
    uint64_t b = in_hash[1];
    uint64_t c = in_hash[2];
    uint64_t d = in_hash[3];
    uint64_t e = in_hash[4];
    uint64_t f = in_hash[5];
    uint64_t g = in_hash[6];
    uint64_t h = in_hash[7];
    for (int i = 0; i < 64; ++i) {
        uint64_t S1 = ((((e << 32) | e) >> 6) & mask) ^ // (e rightrotate 6) xor 
                      ((((e << 32) | e) >> 11) & mask) ^ // (e rightrotate 11) xor 
                      ((((e << 32) | e) >> 25) & mask); // (e rightrotate 25)
        uint64_t ch = (e & f) ^ ((~e & mask) & g); // (e and f) xor ((not e) and g)
        uint64_t temp1 = (h + S1 + ch + hash_k[i] + w[i]) & mask;
        uint64_t S0 = ((((a << 32) | a) >> 2) & mask) ^ // (a rightrotate 2) xor 
                      ((((a << 32) | a) >> 13) & mask) ^ // (a rightrotate 13) xor 
                      ((((a << 32) | a) >> 22) & mask); // (a rightrotate 22)
        uint64_t maj = (a & b) ^ (a & c) ^ (b & c); // (a and b) xor (a and c) xor (b and c)
        uint64_t temp2 = (S0 + maj) & mask;
        h = g;
        g = f;
        f = e;
        e = (d + temp1) & mask;
        d = c;
        c = b;
        b = a;
        a = (temp1 + temp2) & mask;
    }
    in_hash[0] = (in_hash[0] + a) & mask;
    in_hash[1] = (in_hash[1] + b) & mask;
    in_hash[2] = (in_hash[2] + c) & mask;
    in_hash[3] = (in_hash[3] + d) & mask;
    in_hash[4] = (in_hash[4] + e) & mask;
    in_hash[5] = (in_hash[5] + f) & mask;
    in_hash[6] = (in_hash[6] + g) & mask;
    in_hash[7] = (in_hash[7] + h) & mask;
    // Reset hash_chunk for the next chunck
    if (&in_hash == &hash) {
        initHashCalc();
        // std::cerr << "checker chunk of size " << hash_message_size << std::endl;
    } else {
        initExpectedHashCalc();
        // std::cout << "main chunk of size " << expectedHash_message_size << std::endl;
    }
}

void 
loadstorelogentry::CheckerCPUMeta::initExpectedHashCalc() {
    expectedHash_chunk.fill(0);
    expectedHash_chunk_index = 0;
}

void 
loadstorelogentry::CheckerCPUMeta::initHashCalc() {
    hash_chunk.fill(0);
    hash_chunk_index = 0;
}

void 
loadstorelogentry::CheckerCPUMeta::initHash() {
    // std::cout << "init hash" << std::endl;
    // std::cerr << "init hash" << std::endl;
    expectedHash = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    hash = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    hash_message_size = 0;
    expectedHash_message_size = 0;
    initExpectedHashCalc();
    initHashCalc();
}
}
