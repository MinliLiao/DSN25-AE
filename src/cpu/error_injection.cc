/* Authors: Lionel Zoubritsky
 *          Sam Ainsworth
 */

#include <bitset>
#include "cpu/error_injection.hh"
#include "cpu/minor/pipeline.hh"
#include "cpu/o3/dyn_inst.hh"
#include "cpu/o3/fu_pool.hh"
#include "debug/ErrorInjection.hh"

namespace gem5 {

using namespace TheISA;
int errorinjection::numberOfErroneousWrites = 0;
int errorinjection::numberOfErroneousReads = 0;
int errorinjection::numberOfErroneousArchStates = 0;
int errorinjection::numberOfErroneousTCStates = 0;
int errorinjection::numberOfErroneousOpClass = 0;
std::vector<uint64_t> errorinjection::lapses(0,0);
std::vector<bool> errorinjection::hasInjectedError(1,false);
std::vector<bool> errorinjection::unchangedInjectedError(1,false);

unsigned errorinjection::seed = 258958529; // std::chrono::system_clock::now().time_since_epoch().count();
std::default_random_engine generator(errorinjection::seed);
std::uniform_int_distribution<int> distrib64(0,63);
std::uniform_int_distribution<int> distrib32(0,31);
std::independent_bits_engine<std::default_random_engine, 64, uint64_t> indepGenerator64(errorinjection::seed);
std::independent_bits_engine<std::default_random_engine, 32, uint32_t> indepGenerator32(errorinjection::seed);
std::uniform_real_distribution<double> disdouble;


int errorinjection::undetectedErrors = 0;
int errorinjection::falsePositives = 0;
int errorinjection::cptOnlyUnchangedInjections = 0;
uint64_t errorinjection::unchangedInjections = 0;
uint64_t errorinjection::changedInjections = 0;

double errorinjection::loadstoreErrRate = 0;
double errorinjection::TCStateErrRate = 0;
double errorinjection::UniversalOpErrRate = 0;
double errorinjection::floatOpErrRate = 0;
double errorinjection::intOpErrRate = 0;
double errorinjection::ALUOpErrRate = 0;
unsigned errorinjection::hardErrBit = 0;
unsigned errorinjection::hardErrStructId = 0;
errorinjection::hardErrStructTypes errorinjection::hardErrStructType = errorinjection::hardErrStructTypes::None;
bool errorinjection::hardErrorStuckAt1 = false;
unsigned errorinjection::hardErrorCores = 0;
std::map<OpClass, std::set<std::string>> errorinjection::multiDestOpClasses;
std::map<OpClass, std::set<std::string>> errorinjection::destRegOpClasses;

bool errorinjection::exitOnErr = false;

void errorinjection::setErrRates(std::vector<double> errRates) {
    loadstoreErrRate = errRates[0];
    TCStateErrRate = errRates[1];
    UniversalOpErrRate = errRates[2];
    floatOpErrRate = errRates[3];
    intOpErrRate = errRates[4];
    ALUOpErrRate = errRates[5];
}

bool checkBitSet(unsigned x, unsigned nthBit) {
    return ((1 << nthBit) & x) != 0;
}

void errorinjection::setHardErr(unsigned errBit, unsigned errStID, std::string errStType, unsigned stuckAt, unsigned errMain, unsigned numMains, unsigned numCheckersPerMain, bool exit_on_error) {
    assert(errMain < ((unsigned)1<<numMains));
    // assert(errStID < enums::Num_OpClass);
    hardErrBit = errBit;
    hardErrStructId = errStID;
    if (errStType == "FUdest" ) {
        hardErrStructType = hardErrStructTypes::FUdest;
    } else if (errStType == "LSLentry" ) {
        hardErrStructType = hardErrStructTypes::LSLentry;
    } else if (errStType == "RF" ) {
        hardErrStructType = hardErrStructTypes::RF;
    }
    assert(stuckAt == 0 || stuckAt == 1);
    hardErrorStuckAt1 = (stuckAt == 1);
    hardErrorCores = 0;
    for (unsigned i = 0; i < numMains; ++i) {
        if (checkBitSet(errMain, i)) {
            for (unsigned j = 0; j < numCheckersPerMain; j++) {
                hardErrorCores |= 1 << (i*numCheckersPerMain+j);
            }
        }
    }
    exitOnErr = exit_on_error;
    std::cout << "Hard error type " << errStType << " id " << hardErrStructId;
    std::cout << " bit " << hardErrBit << " stuckAt " << stuckAt;
    std::cout << " for main cores " << std::hex << errMain << std::dec;
    std::cout << " inject on checkers " << std::hex << hardErrorCores << std::dec << std::endl;
}

int getNumTargetFU(int mainCPUID) {
    // Set up the err rate based on FU number
    BaseCPU * baseMain = loadstorelogentry::allCPUMeta[mainCPUID].baseCPU;
    auto O3Main = dynamic_cast<o3::CPU *>(baseMain);
    auto MinorMain = dynamic_cast<MinorCPU *>(baseMain);
    int numTargetFU = 0;
    if (O3Main) {
        int numFU = O3Main->getIEW()->fuPool->size();
        for (int i=0; i<numFU; i++) {
            if (O3Main->getIEW()->fuPool->getFUbyId(i)->provides((OpClass)(errorinjection::hardErrStructId%OpClass::Num_OpClass))) {
                numTargetFU++;
            }
        }
        // For X2, numTargetFU is 0 if hardErrStructId is among:
        // 0 (No_OpClass), 9 (FloatMultAcc), 11 (FloatMisc), 23 (SimdDiv), 
        // 34 to 47 (SimdReduceAdd to SimdPredAlu), 50 (FloatMemRead), 
        // 51 (FloatMemWrite), 53 (InstPrefetch)
        // However, there are 9 (FloatMultAcc), 11 (FloatMisc) instructions in 
        // benchmarks, so assuming numTargetFU of 1 in these cases
        if ((OpClass)(errorinjection::hardErrStructId%OpClass::Num_OpClass) == OpClass::FloatMultAcc ||
            (OpClass)(errorinjection::hardErrStructId%OpClass::Num_OpClass) == OpClass::FloatMisc) {
            numTargetFU = 1;
        }
    } else if (MinorMain) {
        int numFU = MinorMain->pipeline->getExecute()->getFuncUnits()->size();
        for (int i=0; i<numFU; i++) {
            if (MinorMain->pipeline->getExecute()->getFuncUnits()->at(i)->provides((OpClass)(errorinjection::hardErrStructId%OpClass::Num_OpClass))) {
                numTargetFU++;
            }
        }
    } else {
      assert(false);
    }
    assert(numTargetFU > 0);
    return numTargetFU;
}

int roll_dice(int size) {
  std::uniform_int_distribution<int> distrib(0,size-1);
  return distrib(generator);
}

template <class T>
T compromise_uint(T x, int idx_bit) {
  return x ^ ((T)1<<idx_bit);
}

template <class T>
T compromise_uint(T x) {
  return compromise_uint(x, roll_dice(8*sizeof(T)));
}

uint64_t compromise_uint64(uint64_t x) {
  return compromise_uint<uint64_t>(x, distrib64(generator));
}
uint32_t compromise_uint32(uint32_t x) {
  return compromise_uint<uint32_t>(x, distrib32(generator));
}

template <class T>
void compromise_array_pos(T* x, int pos) {
  int idx_bit = roll_dice(8*sizeof(T));
  x[pos] ^= ((T)1<<idx_bit);
}

template <class T>
T stuckAt_uint(T x, int idx_bit, bool stuckAt1) {
  T stuckAtVal = stuckAt1? 1 : 0;
  return ((x & (~((T)1<<idx_bit))) | (stuckAtVal<<idx_bit));
}

#define SET_NEXT_ERROR_INJECTION(id, error_rate)                             \
      std::geometric_distribution<int64_t> distributionError(1.0/error_rate);    \
      nextErrorInjection[id] = 1 + distributionError(generator);             \
 //     std::cout << "Voltage: " << loadstorelogentry::voltage[0] << " Error rate: " << UNIVERSAL_ERRORRATE << " Next error: " << nextErrorInjection[id] << "\n"; 

#define PREPARE_DISTRIBUTION(id, domain_size, error_rate)      		         \
      static std::vector<int64_t> nextErrorInjection(domain_size,-1);                     \
      static std::vector<int64_t> actualDelay(domain_size,0);                        \
      if(nextErrorInjection.size() != domain_size) { nextErrorInjection.resize(domain_size); actualDelay.resize(domain_size);  } \
      if (nextErrorInjection[0]==-1 || errordetection::voltage_reset[id])    \
        for (int i = 0; i < domain_size; i++) {                              \
          SET_NEXT_ERROR_INJECTION(i, error_rate)                            \
        }                                                                    \
      errordetection::voltage_reset[id] = false;							 \
      actualDelay[id]++;                                                     \
      /*nextErrorInjection[id] += errorinjection::hasInjectedError[checkerID]; //commented out to allow multiple errors per checkpoint */\
      if (--nextErrorInjection[id]>0) return;                                \
      assert(nextErrorInjection[id]==0);                                     \
      errorinjection::hasInjectedError[checkerID] = true;                    \
      SET_NEXT_ERROR_INJECTION(id, error_rate)                               \
      assert(nextErrorInjection[id]>=0);                                     \
      errorinjection::lapses.push_back(actualDelay[id]);                     \
      actualDelay[id] = 0;                                                   \

#define _If_SHOULD_INJECT_ERROR_MAIN(error_rate)                         \
      int checkerID = loadstorelogentry::mainCPUMeta[cpuID].current_segment_to_fill; \
      PREPARE_DISTRIBUTION(cpuID, NUMBEROFMAINCORES, error_rate)

#define _If_SHOULD_INJECT_ERROR_CHECKER(error_rate) \
      PREPARE_DISTRIBUTION(checkerID, (NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE), error_rate)



#ifdef DEBUG_ERRROR_INJECTION
#define IF_SHOULD_INJECT_ERROR_MAIN(error_rate, name) \
      _If_SHOULD_INJECT_ERROR_MAIN(error_rate)        \
      std::cout << " < Injected error for checker core " << checkerID << " at time " << loadstorelogentry::mainCPUMeta[cpuID].timestamp << " on " << name << std::endl;
#define IF_SHOULD_INJECT_ERROR_CHECKER(error_rate, name) \
      _If_SHOULD_INJECT_ERROR_CHECKER(error_rate)        \
      std::cout << " < Injected error for checker core " << checkerID << " at local time " << loadstorelogentry::checkerCPUMeta[checkerID].timestamps << " (global time: " << loadstorelogentry::mainCPUMeta[checkerID/NUMBEROFCHECKERCORESPERCORE].timestamp << ") on " << name << std::endl;
#else
#define IF_SHOULD_INJECT_ERROR_MAIN(error_rate, name) \
       _If_SHOULD_INJECT_ERROR_MAIN(error_rate)

#define IF_SHOULD_INJECT_ERROR_CHECKER(error_rate, name) \
       _If_SHOULD_INJECT_ERROR_CHECKER(error_rate)

#endif



void compromise_loadstorelogentry(int cpuID, void* ll) {
  // Change implementation with the model

#ifdef LOADSTORE_ERRORRATE

  loadstorelogentry* l = (loadstorelogentry*)ll;
  if (!l->load) return;
  IF_SHOULD_INJECT_ERROR_MAIN(LOADSTORE_ERRORRATE, (l->load?"load":"store"));

  // if (l->load) errorinjection::numberOfErroneousReads++;
  // else         errorinjection::numberOfErroneousWrites++;
  errorinjection::numberOfErroneousReads++;

  if (l->data.empty()) {
    #ifdef DEBUG_ERRROR_INJECTION
    std::cout << "Compromised empty data!" << std::endl;
    #endif
    l->data.push_back(1);
  } else {
    // static int i = 0;
    // int idx = 0;
    // if (i%4==0) idx = 1;
    // else if (i%4==1) idx = 3;
    // else if (i%4==2) idx = 7;
    // else if (i%4==3) idx = 0;
    // ++i;
    // Try 1 3 7 0 on bitcount

    int idx_data = roll_dice(l->data.size());
    compromise_array_pos<uint8_t>(l->data.data(), idx_data);
  }
#endif
}


void corrupt_value_reg(ThreadContext* tc, regSafeEntry e) {
  switch(e.reg_class) {
    case IntRegClass:
      tc->setIntRegFlat(e.idx, compromise_uint64(e.value));
    break;
/*
    case VecRegClass:
      tc->setFloatRegBitsFlat(e.idx, compromise_uint32((uint32_t)e.value));
    break;
*/
    case CCRegClass:
      tc->setCCReg(e.idx, compromise_uint64(e.value));
    break;

    case MiscRegClass:
      tc->setMiscRegNoEffect(e.idx, compromise_uint64(e.value));
    break;

    default:
      std::cout << "!! Unsupported modified register" << std::endl;
  }
}


void compromise_architectural_state(int cpuID, void* mm, int reg_type, int idx) {

assert(0 && "not yet reimplemented\n");
/*
#ifndef ARCHSTATE_ERRORRATE
return;
#endif


#ifdef ARCHSTATE_ERRORRATE
  IF_SHOULD_INJECT_ERROR_MAIN(ARCHSTATE_ERRORRATE, "architectural state")
#endif


  errorinjection::numberOfErroneousArchStates++;
  miniContext* m = (miniContext*)mm;

  if (reg_type==-1) reg_type = roll_dice(5);
  switch (reg_type) {
    case 0: {
      if (idx==-1) idx = roll_dice(int_reg::NumRegs);
      compromise_array_pos<IntReg>(m->intRegs, idx);
    }
    break;

    case 1: {
      if (idx==-1) idx = roll_dice(NumFloatRegs);
      assert(sizeof(float)>=4); // Since we're converting it to an uint32_t
      uint32_t fl = *((uint32_t*)&(m->floatRegs[idx]));
      *((uint32_t*)&(m->floatRegs[idx])) = compromise_uint32(fl);
    }
    break;

    case 2: {
      if (idx==-1) idx = roll_dice(NumCCRegs);
      compromise_array_pos<CCReg>(m->ccRegs, idx);
    }
    break;

    case 3: {
      if (idx==-1) idx = roll_dice(NumMiscRegs);
      compromise_array_pos<MiscReg>(m->miscRegs, idx);
    }
    break;

    case 4: {
      assert(idx==-1);
      uint8_t nextItstate = m->pcState.nextItstate();
      m->pcState.nextItstate(compromise_uint<uint8_t>(nextItstate));
    }
    break;

    default: assert(reg_type >= 0 && reg_type < 5);
  }
*/
}

void compromise_thread_context_state(int checkerID, int reg_type, int idx) {
#ifdef TCSTATE_ERRORRATE

//TODO: Lionel's implementation only compromises reg_type 2. Not sure why...

  IF_SHOULD_INJECT_ERROR_CHECKER(TCSTATE_ERRORRATE, "tc state")

  errorinjection::numberOfErroneousTCStates++;
  ThreadContext* tc = loadstorelogentry::allCPUMeta[checkerID+NUMBEROFMAINCORES].baseCPU->getContext(0);

  if (reg_type==-1) reg_type = roll_dice(4); // will be roll_dice(5) when fixed
  switch (reg_type) {
 /*   case 0: {

      if (idx==-1) idx = roll_dice(int_reg::NumRegs);
      IntReg x = tc->readIntRegFlat(idx);
      tc->setIntRegFlat(idx, compromise_uint64(x));
    }
    break;

    case 1: {
      if (idx==-1) idx = roll_dice(NumFloatRegs);
      FloatRegBits x = tc->readFloatRegBitsFlat(idx);
      tc->setFloatRegBitsFlat(idx, compromise_uint32(x));
    }
    break;
*/
    case 2: {
#ifdef ISA_HAS_CC_REGS
      if (idx==-1) idx = roll_dice(NumCCRegs);
      CCRegClass x =  tc->getReg(idx);
      RegId reg(CCRegClass, idx);
      tc->setReg(reg,  compromise_uint64(x)));
#endif
    }
    break;
/*
    case 3: {
      if (idx==-1) idx = roll_dice(NumMiscRegs);
      MiscReg x = tc->readMiscRegNoEffect(idx);
      tc->setMiscRegNoEffect(idx, compromise_uint64(x));
    }
    break;

    case 4: { // Does not have any effect...
      assert(idx==-1);
      TheISA::PCState oldpc = tc->pcState();
      TheISA::PCState pc = tc->pcState();
      pc.set(12345678);
      // pc.instNPC(compromise_uint<uint8_t>(pc.instNPC()));
      // pc.nextItstate((uint8_t)123);
      tc->pcStateNoRecord(pc);
      assert(tc->pcState()!=oldpc);
    }
    break;
*/
    default: assert(reg_type == 2); //assert(reg_type >= 0 && reg_type < 5);
  }

#endif
}

bool extract_value_reg(ThreadContext* tc, RegClassType reg_class, uint64_t idx, RegVal &val) {
    bool extracted = false;
    switch(reg_class) {
      case IntRegClass: 
      case FloatRegClass:
      case VecElemClass:
      case CCRegClass: {
        val = (uint64_t) tc->getReg(RegId(reg_class, idx));
        extracted = true;
        break;
      }
      case VecRegClass: {
        assert(0 && "not yet implemented VecReg"); break;
      }
      case VecPredRegClass: {
        assert(0 && "not yet implemented VecPredReg"); break;
      }
      case MiscRegClass: {
        val = (uint64_t) tc->readMiscRegNoEffect(idx);
        extracted = true;
        break;
      }
      case InvalidRegClass: {
        std::cout << "!! Unsupported modified register InvalidReg" << std::endl;
        break;
      }
      default: {
        std::cout << "!! Unsupported modified register" << std::endl;
        break;
      }
    }
    return extracted;
}

bool extract_value_vecReg(ThreadContext* tc, RegClassType reg_class, uint64_t idx, TheISA::VecRegContainer &val) {
    bool extracted = false;
    switch(reg_class) {
      case VecRegClass: {
        tc->getReg(RegId(reg_class, idx), &val);
        extracted = true;
        break;
      }
      case IntRegClass: 
      case FloatRegClass:
      case VecElemClass:
      case CCRegClass:
      case MiscRegClass: {
        assert(0 && "Wrong reg class, not VecReg"); break;
      }
      case VecPredRegClass: 
        assert(0 && "not yet implemented VecPredReg"); break;
      case InvalidRegClass: {
        std::cout << "!! Unsupported modified register InvalidReg" << std::endl;
        break;
      }
      default: {
        std::cout << "!! Unsupported modified register" << std::endl;
        break;
      }
    }
    return extracted;
}

bool modify_value_reg(ThreadContext* tc, RegClassType reg_class, uint64_t idx, uint64_t new_val) {
    bool changed = false;
    switch(reg_class) {
      case IntRegClass:
      case FloatRegClass:
      case VecElemClass:
      case CCRegClass:
        tc->setReg(RegId(reg_class, idx), new_val);
        changed = true;
        break;
      case VecRegClass:
        assert(0 && "not yet implemented VecReg"); break;
      case VecPredRegClass: 
        assert(0 && "not yet implemented VecPredReg"); break;
      case MiscRegClass:
        tc->setMiscRegNoEffect(idx, new_val);
        changed = true;
        break;
      case InvalidRegClass:
        std::cout << "!! Unsupported modified register InvalidReg" << std::endl;
        break;
      default: {
            std::cout << "!! Unsupported modified register" << std::endl;
        }
        break;
    }
    return changed;
}

bool modify_value_vecReg(ThreadContext* tc, RegClassType reg_class, uint64_t idx, TheISA::VecRegContainer &new_val) {
    bool changed = false;
    switch(reg_class) {
      case IntRegClass:
      case FloatRegClass:
      case VecElemClass:
      case CCRegClass:
      case MiscRegClass:
        assert(0 && "Wrong reg class, not VecReg"); break;
      case VecRegClass:{
        tc->setReg(RegId(reg_class, idx), &new_val);
        changed = true;
        break;
      }
      case VecPredRegClass: 
        assert(0 && "not yet implemented VecPredReg"); break;
      case InvalidRegClass:
        std::cout << "!! Unsupported modified register InvalidReg" << std::endl;
        break;
      default: {
            std::cout << "!! Unsupported modified register" << std::endl;
        }
        break;
    }
    return changed;
}

void randomise_value_reg(ThreadContext* tc, RegClassType reg_class, uint64_t idx) {
  modify_value_reg(tc, reg_class, idx, indepGenerator64());
}

bool stuckAt_reg(ThreadContext* tc, RegClassType reg_class, uint64_t idx, uint64_t orig_val, int idx_bit, bool stuckAt1) {
    uint64_t new_val = stuckAt_uint<uint64_t>(orig_val, idx_bit, stuckAt1);
    bool changed = (orig_val != new_val);
    if (changed) {
        changed = modify_value_reg(tc, reg_class, idx, new_val);
    }
    return changed;
}

bool stuckAt_vecReg(ThreadContext* tc, RegClassType reg_class, uint64_t idx, TheISA::VecRegContainer orig_vecVal, int idx_bit, bool stuckAt1) {
    unsigned elem_id = idx_bit/64;
    unsigned elem_bit = idx_bit%64;
    uint64_t orig_val = orig_vecVal.as<uint64_t>()[elem_id];
    uint64_t new_val = stuckAt_uint<uint64_t>(orig_val, elem_bit, stuckAt1);
    bool changed = (orig_val != new_val);
    if (changed) {
        orig_vecVal.as<uint64_t>()[elem_id] = new_val;
        changed = modify_value_vecReg(tc, reg_class, idx, orig_vecVal);
    }
    return changed;
}

bool stuckAt_addr(uint64_t& addr, int idx_bit, bool stuckAt1) {
    uint64_t new_val = stuckAt_uint<uint64_t>(addr, idx_bit, stuckAt1);
    bool changed = (addr != new_val);
    addr = new_val;
    return changed;
}

bool extract_old_value_reg_o3(o3::DynInstPtr inst, uint64_t idx, RegVal &val) {
  bool extracted = false;
  const PhysRegIdPtr reg = inst->renamedDestIdx(idx);
  const RegId& original_dest_reg = inst->staticInst->destRegIdx(idx);
  switch (original_dest_reg.classValue()) {
    case IntRegClass:
    case FloatRegClass:
    case CCRegClass:
    case VecElemClass:
      val = inst->cpu->getReg(reg);
      extracted = true;
      break;
    case VecRegClass:
      {
          // TheISA::VecRegContainer val;
          // cpu->getReg(prev_phys_reg, &val);
          std::cout << "!! Unsupported extract register VecReg" << std::endl;
          assert(0 && "not yet implemented");
      }
      break;
    case VecPredRegClass:
      {
          // TheISA::VecPredRegContainer val;
          // cpu->getReg(prev_phys_reg, &val);
          std::cout << "!! Unsupported extract register VecPredReg" << std::endl;
          assert(0 && "not yet implemented");
      }
      break;
    case InvalidRegClass:
      {
          std::cout << "!! Unsupported extract register InvalidReg" << std::endl;
      }
      break;
    case MiscRegClass:
      {
          std::cout << "!! Unsupported extract register MiscReg" << std::endl;
      }
      // no need to forward misc reg values
      break;
    default:
      panic("Unknown register class: %d",
              (int)original_dest_reg.classValue());
  }
  return extracted;
}

bool modify_value_reg_o3(o3::DynInstPtr inst, uint64_t idx, uint64_t new_val) {
  bool changed = true;
  const PhysRegIdPtr reg = inst->renamedDestIdx(idx);
  const RegId& original_dest_reg = inst->staticInst->destRegIdx(idx);
  switch (original_dest_reg.classValue()) {
    case IntRegClass:
    case FloatRegClass:
    case CCRegClass:
    case VecElemClass: {
        inst->cpu->setReg(reg, new_val);
      }
      break;
    case VecRegClass:
      {
          // TheISA::VecRegContainer val;
          // cpu->getReg(prev_phys_reg, &val);
          // setRegOperand(staticInst.get(), idx, &val);
          std::cout << "!! Unsupported modified register VecReg" << std::endl;
          assert(0 && "not yet implemented");
          changed = false;
      }
      break;
    case VecPredRegClass:
      {
          // TheISA::VecPredRegContainer val;
          // cpu->getReg(prev_phys_reg, &val);
          // setRegOperand(staticInst.get(), idx, &val);
          std::cout << "!! Unsupported modified register VecPredReg" << std::endl;
          assert(0 && "not yet implemented");
          changed = false;
      }
      break;
    case InvalidRegClass:
      {
          std::cout << "!! Unsupported modified register InvalidReg" << std::endl;
          changed = false;
      }
      break;
    case MiscRegClass:
      {
        std::cout << "!! Unsupported modified register MiscReg" << std::endl;
        changed = false;
      }
      // no need to forward misc reg values
      break;
    default:
      panic("Unknown register class: %d",
              (int)original_dest_reg.classValue());
  }
  return changed;
}
bool stuckAt_reg_o3(o3::DynInstPtr inst, uint64_t idx, uint64_t orig_val, int idx_bit, bool stuckAt1) {
  uint64_t new_val = stuckAt_uint<uint64_t>(orig_val, idx_bit, stuckAt1);
  bool changed = (orig_val != new_val);
  if (changed) {
    changed = modify_value_reg_o3(inst, idx, new_val);
  }
  return changed;
}

void save_modified_regs(StaticInstPtr staticInst, ThreadContext* tc, regSafe* safe) {
    unsigned int num_dest_regs = staticInst->numDestRegs();
    for (unsigned int dest_reg = 0; dest_reg < num_dest_regs; dest_reg++) {
        RegClassType reg_class = staticInst->destRegIdx(dest_reg).classValue();
        if (reg_class == VecRegClass || reg_class == VecPredRegClass) {
            TheISA::VecRegContainer value;
            if (extract_value_vecReg(tc, reg_class, staticInst->destRegIdx(dest_reg).index(), value)) {
                safe->push_back(regSafeEntry(reg_class, staticInst->destRegIdx(dest_reg).index(), value));
            }
        } else {
            uint64_t value;
            if (extract_value_reg(tc, reg_class, staticInst->destRegIdx(dest_reg).index(), value)) {
                safe->push_back(regSafeEntry(reg_class, staticInst->destRegIdx(dest_reg).index(), value));
            }
        }
    }
}

void save_modified_regs_o3(o3::DynInstPtr inst, regSafe* safe) {
    unsigned int num_dest_regs = inst->numDestRegs();
    for (unsigned int dest_reg = 0; dest_reg < num_dest_regs; dest_reg++) {
        RegClassType reg_class = inst->staticInst->destRegIdx(dest_reg).classValue();
        uint64_t value;
        if (extract_old_value_reg_o3(inst, dest_reg, value)) {
            safe->push_back(regSafeEntry(reg_class, dest_reg, value));
        }
    }
}

// std::vector<std::string> allNames = std::vector<std::string>();

bool injectFUdestError(int checkerID) {
    assert(checkerID < NUMBEROFCHECKERCORESPERCORE*NUMBEROFMAINCORES);
    return errorinjection::hardErrStructType == errorinjection::hardErrStructTypes::FUdest // should inject FUdest error
      && checkBitSet(errorinjection::hardErrorCores, checkerID) // should inject error on this checker core
      && loadstorelogentry::checkerCPUMeta[checkerID].timestamps > 10; // do not inject error on the first 10 checkpoints
}

bool shouldInjectError(double errRate) {
  double rand_val = disdouble(generator);
  return (rand_val > 0) && (rand_val < errRate);
}

void stuckAt_stats(int checkerID, bool changed) {
    assert(checkerID < NUMBEROFCHECKERCORESPERCORE*NUMBEROFMAINCORES);
    if (changed) { errorinjection::changedInjections++; }
    else { 
        errorinjection::unchangedInjections++; 
        if (!errorinjection::hasInjectedError[checkerID] && !errorinjection::unchangedInjectedError[checkerID]) {
            errorinjection::unchangedInjectedError[checkerID] = true;
        }
    }
    errorinjection::hasInjectedError[checkerID] = errorinjection::hasInjectedError[checkerID] || changed;
    if (errorinjection::hasInjectedError[checkerID] && errorinjection::unchangedInjectedError[checkerID]) {
        errorinjection::unchangedInjectedError[checkerID] = false;
    }
}

bool stuckAt_instruction_result(int checkerID, StaticInstPtr staticInst, ThreadContext* tc, regSafe* before_instr, int idx_bit, bool stuckAt1) {
    assert(checkerID < NUMBEROFCHECKERCORESPERCORE*NUMBEROFMAINCORES);
    assert(injectFUdestError(checkerID));
    bool error_inserted = false;
    if (shouldInjectError(errorinjection::UniversalOpErrRate/getNumTargetFU(checkerID/NUMBEROFCHECKERCORESPERCORE))) {
        bool modified = false;
        for (regSafeEntry e : *before_instr) {
            if (e.reg_class == VecRegClass || e.reg_class == VecPredRegClass) {
                assert(e.vecReg);
                TheISA::VecRegContainer newValue;
                if (extract_value_vecReg(tc, e.reg_class, e.idx, newValue) && e.vecVal != newValue) {
                    modified = true;
                    break;
                }
            } else {
                uint64_t newValue;
                if (extract_value_reg(tc, e.reg_class, e.idx, newValue) && e.value != newValue) {
                    modified = true;
                    break;
                }
            }
        }

        if (modified) {
            for (unsigned i = 0; i < before_instr->size(); i++) {
                regSafeEntry e = before_instr->at(i);
                if (errorinjection::hardErrStructType == errorinjection::hardErrStructTypes::FUdest) {
                    if ((errorinjection::hardErrStructId % OpClass::Num_OpClass) == OpClass::IntAlu) {
                        // sometimes insert error in calculation, sometimes in CC
                        if (errorinjection::hardErrStructId > OpClass::Num_OpClass) { // insert in CC
                            if (e.reg_class != CCRegClass) continue; // Do not insert if not CC
                            else if (before_instr->at(0).reg_class == CCRegClass // All regs are CC
                                    && i+1 != errorinjection::hardErrStructId/OpClass::Num_OpClass) {
                                continue;   
                            } else if (i != errorinjection::hardErrStructId/OpClass::Num_OpClass) continue;
                        } else if (e.reg_class == CCRegClass) continue; // skip if not insert in CC && register is CC
                    } else if ((errorinjection::hardErrStructId % OpClass::Num_OpClass) == OpClass::FloatCmp) {
                        // sometimes insert error in CC, sometimes in value
                        if (errorinjection::hardErrStructId/OpClass::Num_OpClass == 5 && e.reg_class != VecRegClass) 
                            continue; // Insert in result vec only
                        else if (errorinjection::hardErrStructId > OpClass::Num_OpClass) { // Insert in CC
                            if (e.reg_class != CCRegClass) continue;
                            else if (i != errorinjection::hardErrStructId/OpClass::Num_OpClass) continue;
                        } else if (e.reg_class == CCRegClass || e.reg_class == VecRegClass) continue;
                    } else {
                        // insert error in different destination register types
                        std::set<OpClass> multi_reg_types {
                            OpClass::FloatAdd,OpClass::FloatCvt,OpClass::FloatMult,
                            OpClass::FloatMultAcc,OpClass::FloatDiv,OpClass::FloatMisc,
                            OpClass::SimdMisc,OpClass::SimdFloatAdd,OpClass::SimdFloatAlu,
                            OpClass::SimdFloatCmp,OpClass::SimdFloatDiv,OpClass::SimdFloatMult,
                            OpClass::SimdFloatMultAcc};
                        if ((multi_reg_types.find((OpClass)(errorinjection::hardErrStructId % OpClass::Num_OpClass)) != multi_reg_types.end())
                            && ((errorinjection::hardErrStructId/OpClass::Num_OpClass == 0 && e.reg_class != MiscRegClass)
                                || (errorinjection::hardErrStructId/OpClass::Num_OpClass == 1 && e.reg_class != VecRegClass)
                                || (errorinjection::hardErrStructId/OpClass::Num_OpClass == 2 && e.reg_class != IntRegClass))) {
                            continue; // skip incorrect reg class
                        }
                    }
                }
                
                if (e.reg_class == VecRegClass || e.reg_class == VecPredRegClass) {
                    TheISA::VecRegContainer origValue;
                    if (extract_value_vecReg(tc, e.reg_class, e.idx, origValue)) {
                        bool changed = stuckAt_vecReg(tc, e.reg_class, e.idx, origValue, idx_bit, stuckAt1);
                        if (changed) { errorinjection::changedInjections++; }
                        else { errorinjection::unchangedInjections++; }
                        error_inserted = error_inserted || changed;
                        if (changed) {
                            TheISA::VecRegContainer newValue;
                            extract_value_vecReg(tc, e.reg_class, e.idx, newValue);
                            std::stringstream orig_ss, new_ss;
                            orig_ss << origValue;
                            new_ss << newValue;
                            DPRINTF(ErrorInjection, 
                                    "Minor Error injected on reg_class %i, "
                                    "reg_id %i, bit %i, stuckAt %i, "
                                    "orig_val %s, new_val %s, "
                                    "inst %s\n",
                                    e.reg_class, e.idx, idx_bit, stuckAt1,
                                    orig_ss.str(), new_ss.str(),
                                    staticInst->getName());
                        } else if (!errorinjection::hasInjectedError[checkerID] && !errorinjection::unchangedInjectedError[checkerID]) {
                            errorinjection::unchangedInjectedError[checkerID] = true;
                        }
                    }
                } else {
                    uint64_t origValue;
                    if (extract_value_reg(tc, e.reg_class, e.idx, origValue)) {
                        if (idx_bit > 63 && !(e.reg_class == VecRegClass || e.reg_class == VecPredRegClass)) {
                            DPRINTF(ErrorInjection, "Opclass %d regClass %d had bit id > 63\n",
                                    staticInst->opClass(), e.reg_class);
                            idx_bit = idx_bit % 64;
                        }
                        bool changed = stuckAt_reg(tc, e.reg_class, e.idx, origValue, idx_bit, stuckAt1);
                        if (changed) { errorinjection::changedInjections++; }
                        else { errorinjection::unchangedInjections++; }
                        error_inserted = error_inserted || changed;
                        if (changed) {
                            uint64_t newValue;
                            extract_value_reg(tc, e.reg_class, e.idx, newValue);
                            DPRINTF(ErrorInjection, 
                                    "Minor Error injected on reg_class %i, "
                                    "reg_id %i, bit %i, stuckAt %i, "
                                    "orig_val %ld, new_val %ld, "
                                    "inst %s\n",
                                    e.reg_class, e.idx, idx_bit, stuckAt1,
                                    origValue, newValue,
                                    staticInst->getName());
                        } else if (!errorinjection::hasInjectedError[checkerID] && !errorinjection::unchangedInjectedError[checkerID]) {
                            errorinjection::unchangedInjectedError[checkerID] = true;
                        }
                    }
                }
                errorinjection::hasInjectedError[checkerID] = errorinjection::hasInjectedError[checkerID] || error_inserted;
                if (errorinjection::hasInjectedError[checkerID] && errorinjection::unchangedInjectedError[checkerID]) {
                    errorinjection::unchangedInjectedError[checkerID] = false;
                }
            }
        }
    }
    return error_inserted;
}

bool stuckAt_instruction_result_o3(int checkerID, o3::DynInstPtr inst, regSafe* before_instr, int idx_bit, bool stuckAt1) {
    assert(injectFUdestError(checkerID));
    bool error_inserted = false;
    if (shouldInjectError(errorinjection::UniversalOpErrRate/getNumTargetFU(checkerID/NUMBEROFCHECKERCORESPERCORE))) {
        bool modified = false;
        for (regSafeEntry e : *before_instr) {
            uint64_t newValue;
            if (extract_old_value_reg_o3(inst, e.idx, newValue) && e.value != newValue) {
                modified = true;
                break;
            }
        }

        if (modified) {
            for (regSafeEntry e : *before_instr) {
                uint64_t origValue;
                if (extract_old_value_reg_o3(inst, e.idx, origValue)) {
                    error_inserted = error_inserted || stuckAt_reg_o3(inst, e.idx, origValue, idx_bit, stuckAt1);
                    if (error_inserted && !errorinjection::hasInjectedError[checkerID]) {
                        std::cout << "O3 Error injected" << std::endl;
                    }
                    errorinjection::hasInjectedError[checkerID] = errorinjection::hasInjectedError[checkerID] || error_inserted;
                }
            }
        }
    }
    return error_inserted;
}

void compromise_instruction_result(int checkerID, StaticInstPtr staticInst, ThreadContext* tc, regSafe* before_instr) {
#ifdef OPCLASS_ERRORRATE

  // std::string name = staticInst->getName();
  // bool flag = true;
  // for (int i = 0; i < allNames.size(); i++) {
  //   if (allNames[i] == name) {
  //     flag = false;
  //     break;
  //   }
  // }
  // if (flag) {
    // allNames.push_back(name);
    // std::cout << "\n" << name << " -- " << staticInst->opClass() << std::endl;

  OpClass opclass = staticInst->opClass();
  if (!(OPCLASS_TARGET)) return;

  IF_SHOULD_INJECT_ERROR_CHECKER(OPCLASS_ERRORRATE, "op class")

  bool modified = false;
  for (regSafeEntry e : *before_instr) {
    uint64_t newValue = extract_value_reg(tc, e.reg_class, e.idx);
    if (e.value != newValue) {
      modified = true;
      break;
    }
  }

  if (modified) {
    errorinjection::numberOfErroneousOpClass++;
    for (regSafeEntry e : *before_instr) {
      randomise_value_reg(tc, e.reg_class, e.idx);
    }
  } else {
    nextErrorInjection.at(checkerID) = 1;
    actualDelay.at(checkerID) = errorinjection::lapses.back();
    errorinjection::lapses.pop_back();
    errorinjection::hasInjectedError.at(checkerID) = false;
#ifdef DEBUG_ERRROR_INJECTION
    std::cout << " --> cancelled (no modified register)" << std::endl;
#endif
  }

  // if (!(name[0]=='b' && name[1]!='i' && name[1]!='k')) {
  //   std::cout << "\n" << name << std::endl;
  //   std::cout << "- - - - - - - - - " << std::endl;
  //   std::cout << m->pcState << std::endl;
  //   std::cout << " .   .   .   .   .   .   .   ." << std::endl;
  //   std::cout << tc->pcState() << std::endl;
  // }
#endif
}
}
