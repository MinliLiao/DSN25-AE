/*
 * Copyright (c) 2011-2012, 2014, 2016, 2017, 2019-2020 ARM Limited
 * Copyright (c) 2013 Advanced Micro Devices, Inc.
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2004-2006 The Regents of The University of Michigan
 * Copyright (c) 2011 Regents of the University of California
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "cpu/o3/cpu.hh"

#include "cpu/error_injection.hh"

#include "config/the_isa.hh"
#include "cpu/activity.hh"
#include "cpu/checker/cpu.hh"
#include "cpu/checker/thread_context.hh"
#include "cpu/o3/dyn_inst.hh"
#include "cpu/o3/limits.hh"
#include "cpu/o3/thread_context.hh"
#include "cpu/simple_thread.hh"
#include "cpu/thread_context.hh"
#include "debug/Activity.hh"
#include "debug/Drain.hh"
#include "debug/O3AsChecker.hh"
#include "debug/O3LdStLogSeqNum.hh"
#include "debug/O3CPU.hh"
#include "debug/Quiesce.hh"
#include "enums/MemoryMode.hh"
#include "sim/cur_tick.hh"
#include "sim/full_system.hh"
#include "sim/process.hh"
#include "sim/stat_control.hh"
#include "sim/system.hh"
#include "mem/cache/loadstorelogentry.hh"

namespace gem5
{

struct BaseCPUParams;

namespace o3
{

CPU::CPU(const BaseO3CPUParams &params)
    : BaseCPU(params),
      mmu(params.mmu),
      tickEvent([this]{ tick(); }, "O3CPU tick",
                false, Event::CPU_Tick_Pri),
      threadExitEvent([this]{ exitThreads(); }, "O3CPU exit threads",
                false, Event::CPU_Exit_Pri),
#ifndef NDEBUG
      instcount(0),
#endif
      removeInstsThisCycle(false),
      lastPC(0), lastMicroPC(-1), commitSeqNum(0), 
      squashedLdStLogSeqNumCount(0),
      fetch(this, params),
      decode(this, params),
      rename(this, params),
      iew(this, params),
      commit(this, params),

      regFile(params.numPhysIntRegs,
              params.numPhysFloatRegs,
              params.numPhysVecRegs,
              params.numPhysVecPredRegs,
              params.numPhysCCRegs,
              params.isa[0]->regClasses()),

      freeList(name() + ".freelist", &regFile),

      rob(this, params),

      scoreboard(name() + ".scoreboard", regFile.totalNumPhysRegs()),

      isa(numThreads, NULL),

      timeBuffer(params.backComSize, params.forwardComSize),
      fetchQueue(params.backComSize, params.forwardComSize),
      decodeQueue(params.backComSize, params.forwardComSize),
      renameQueue(params.backComSize, params.forwardComSize),
      iewQueue(params.backComSize, params.forwardComSize),
      activityRec(name(), NumStages,
                  params.backComSize + params.forwardComSize,
                  params.activity),

      globalSeqNum(1),
      system(params.system),
      lastRunningCycle(curCycle()),
      _isChecked(params.isChecked), _wasChecked(params.isChecked), _isStored(params.isStored), 
      _canContinueUnchecked(params.canContinueUnchecked),
      _sampledCheck(params.samplePeriod),
      lastSample(0),
      drainEvent([this]{ drain(); }, "O3CPU drain",
                false, Event::CPU_Tick_Pri),
      currentFilledCheckerId(INVALID_CPUID),
      cpuStats(this)
{
    BaseCPU::isSleepGuarded = (params.isSleepGuarded);
    fatal_if(FullSystem && params.numThreads > 1,
            "SMT is not supported in O3 in full system mode currently.");

    fatal_if(!FullSystem && params.numThreads < params.workload.size(),
            "More workload items (%d) than threads (%d) on CPU %s.",
            params.workload.size(), params.numThreads, name());

    if (!params.switched_out) {
        _status = Running;
    } else {
        _status = SwitchedOut;
    }

    if (params.checker) {
        BaseCPU *temp_checker = params.checker;
        checker = dynamic_cast<Checker<DynInstPtr> *>(temp_checker);
        checker->setIcachePort(&fetch.getInstPort());
        checker->setSystem(params.system);
    } else {
        checker = NULL;
    }

    if (!FullSystem) {
        thread.resize(numThreads);
        tids.resize(numThreads);
    }

    // The stages also need their CPU pointer setup.  However this
    // must be done at the upper level CPU because they have pointers
    // to the upper level CPU, and not this CPU.

    // Set up Pointers to the activeThreads list for each stage
    fetch.setActiveThreads(&activeThreads);
    decode.setActiveThreads(&activeThreads);
    rename.setActiveThreads(&activeThreads);
    iew.setActiveThreads(&activeThreads);
    commit.setActiveThreads(&activeThreads);

    // Give each of the stages the time buffer they will use.
    fetch.setTimeBuffer(&timeBuffer);
    decode.setTimeBuffer(&timeBuffer);
    rename.setTimeBuffer(&timeBuffer);
    iew.setTimeBuffer(&timeBuffer);
    commit.setTimeBuffer(&timeBuffer);

    // Also setup each of the stages' queues.
    fetch.setFetchQueue(&fetchQueue);
    decode.setFetchQueue(&fetchQueue);
    commit.setFetchQueue(&fetchQueue);
    decode.setDecodeQueue(&decodeQueue);
    rename.setDecodeQueue(&decodeQueue);
    rename.setRenameQueue(&renameQueue);
    iew.setRenameQueue(&renameQueue);
    iew.setIEWQueue(&iewQueue);
    commit.setIEWQueue(&iewQueue);
    commit.setRenameQueue(&renameQueue);

    commit.setIEWStage(&iew);
    rename.setIEWStage(&iew);
    rename.setCommitStage(&commit);

    ThreadID active_threads;
    if (FullSystem) {
        active_threads = 1;
    } else {
        active_threads = params.workload.size();

        if (active_threads > MaxThreads) {
            panic("Workload Size too large. Increase the 'MaxThreads' "
                  "constant in cpu/o3/limits.hh or edit your workload size.");
        }
    }

    // Make Sure That this a Valid Architeture
    assert(numThreads);
    const auto &regClasses = params.isa[0]->regClasses();

    assert(params.numPhysIntRegs >=
            numThreads * regClasses.at(IntRegClass).numRegs());
    assert(params.numPhysFloatRegs >=
            numThreads * regClasses.at(FloatRegClass).numRegs());
    assert(params.numPhysVecRegs >=
            numThreads * regClasses.at(VecRegClass).numRegs());
    assert(params.numPhysVecPredRegs >=
            numThreads * regClasses.at(VecPredRegClass).numRegs());
    assert(params.numPhysCCRegs >=
            numThreads * regClasses.at(CCRegClass).numRegs());

    // Just make this a warning and go ahead anyway, to keep from having to
    // add checks everywhere.
    warn_if(regClasses.at(CCRegClass).numRegs() == 0 &&
            params.numPhysCCRegs != 0,
            "Non-zero number of physical CC regs specified, even though\n"
            "    ISA does not use them.");

    rename.setScoreboard(&scoreboard);
    iew.setScoreboard(&scoreboard);

    // Setup the rename map for whichever stages need it.
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        isa[tid] = dynamic_cast<TheISA::ISA *>(params.isa[tid]);
        commitRenameMap[tid].init(regClasses, &regFile, &freeList);
        renameMap[tid].init(regClasses, &regFile, &freeList);
    }

    // Initialize rename map to assign physical registers to the
    // architectural registers for active threads only.
    for (ThreadID tid = 0; tid < active_threads; tid++) {
        for (auto type = (RegClassType)0; type <= CCRegClass;
                type = (RegClassType)(type + 1)) {
            for (RegIndex ridx = 0; ridx < regClasses.at(type).numRegs();
                    ++ridx) {
                // Note that we can't use the rename() method because we don't
                // want special treatment for the zero register at this point
                RegId rid = RegId(type, ridx);
                PhysRegIdPtr phys_reg = freeList.getReg(type);
                renameMap[tid].setEntry(rid, phys_reg);
                commitRenameMap[tid].setEntry(rid, phys_reg);
            }
        }
    }

    rename.setRenameMap(renameMap);
    commit.setRenameMap(commitRenameMap);
    rename.setFreeList(&freeList);

    // Setup the ROB for whichever stages need it.
    commit.setROB(&rob);

    lastActivatedCycle = 0;

    DPRINTF(O3CPU, "Creating O3CPU object.\n");

    // Setup any thread state.
    thread.resize(numThreads);

    for (ThreadID tid = 0; tid < numThreads; ++tid) {
        if (FullSystem) {
            // SMT is not supported in FS mode yet.
            assert(numThreads == 1);
            thread[tid] = new ThreadState(this, 0, NULL);
        } else {
            if (tid < params.workload.size()) {
                DPRINTF(O3CPU, "Workload[%i] process is %#x", tid,
                        thread[tid]);
                thread[tid] = new ThreadState(this, tid, params.workload[tid]);
            } else {
                //Allocate Empty thread so M5 can use later
                //when scheduling threads to CPU
                Process* dummy_proc = NULL;

                thread[tid] = new ThreadState(this, tid, dummy_proc);
            }
        }

        gem5::ThreadContext *tc;

        // Setup the TC that will serve as the interface to the threads/CPU.
        auto *o3_tc = new ThreadContext;

        tc = o3_tc;

        // If we're using a checker, then the TC should be the
        // CheckerThreadContext.
        if (params.checker) {
            tc = new CheckerThreadContext<ThreadContext>(o3_tc, checker);
        }

        o3_tc->cpu = this;
        o3_tc->thread = thread[tid];

        // Give the thread the TC.
        thread[tid]->tc = tc;

        // Add the TC to the CPU's list of TC's.
        threadContexts.push_back(tc);
    }

    // O3CPU always requires an interrupt controller.
    if (!params.switched_out && interrupts.empty()) {
        fatal("O3CPU %s has no interrupt controller.\n"
              "Ensure createInterruptController() is called.\n", name());
    }
}

void
CPU::regProbePoints()
{
    BaseCPU::regProbePoints();

    ppInstAccessComplete = new ProbePointArg<PacketPtr>(
            getProbeManager(), "InstAccessComplete");
    ppDataAccessComplete = new ProbePointArg<
        std::pair<DynInstPtr, PacketPtr>>(
                getProbeManager(), "DataAccessComplete");

    fetch.regProbePoints();
    rename.regProbePoints();
    iew.regProbePoints();
    commit.regProbePoints();
}

CPU::CPUStats::CPUStats(CPU *cpu)
    : statistics::Group(cpu),
      ADD_STAT(timesIdled, statistics::units::Count::get(),
               "Number of times that the entire CPU went into an idle state "
               "and unscheduled itself"),
      ADD_STAT(idleCycles, statistics::units::Cycle::get(),
               "Total number of cycles that the CPU has spent unscheduled due "
               "to idling"),
      ADD_STAT(quiesceCycles, statistics::units::Cycle::get(),
               "Total number of cycles that CPU has spent quiesced or waiting "
               "for an interrupt"),
      ADD_STAT(committedInsts, statistics::units::Count::get(),
               "Number of Instructions Simulated"),
      ADD_STAT(committedOps, statistics::units::Count::get(),
               "Number of Ops (including micro ops) Simulated"),
      ADD_STAT(uncheckedCommittedInsts, statistics::units::Count::get(),
               "Number of Instructions Simulated that were not checked by a "
               "checker core"),
      ADD_STAT(uncheckedCommittedOps, statistics::units::Count::get(),
               "Number of Ops (including micro ops) Simulated that were not "
               "checked by a "
               "checker core"),
      ADD_STAT(committedStaticInsts, statistics::units::Count::get(),
               "Number of type of Static Instructions Simulated"),
      ADD_STAT(checkedCommittedStaticInsts, statistics::units::Count::get(),
               "Number of type of Static Instructions Simulated that were checked by "
               "a checker core"),
      ADD_STAT(uncheckedCommittedStaticInsts, statistics::units::Count::get(),
               "Number of type of Static Instructions Simulated that were not checked "
               "by a checker core"),
      ADD_STAT(committedPCs, statistics::units::Count::get(),
               "Number of Static Instructions Simulated"),
      ADD_STAT(checkedCommittedPCs, statistics::units::Count::get(),
               "Number of Static Instructions Simulated that were checked by "
               "a checker core"),
      ADD_STAT(uncheckedCommittedPCs, statistics::units::Count::get(),
               "Number of Static Instructions Simulated that were not checked "
               "by a checker core"),
      ADD_STAT(numSyscallCpts, statistics::units::Count::get(),
               "Number of checkpoints taken due to syscall"),
      ADD_STAT(numLSLFullCpts, statistics::units::Count::get(),
               "Number of checkpoints taken due to LSL full"),
      ADD_STAT(numTimeoutCpts, statistics::units::Count::get(),
               "Number of checkpoints taken due to committed instructions reach timeout"),
      ADD_STAT(numBlockingCpts, statistics::units::Count::get(),
               "Number of checkpoints taken due to blocking dirty block eviction"),
      ADD_STAT(numErrCpts, statistics::units::Count::get(),
               "Number of checkpoints taken due to erroneous main core"),
      ADD_STAT(numOtherCpts, statistics::units::Count::get(),
               "Number of checkpoints taken due to other unknown reasons"),
      ADD_STAT(numLSLLoadEntries, statistics::units::Count::get(),
               "Number of LSL entries for load instructions"),
      ADD_STAT(numLSLSwapEntries, statistics::units::Count::get(),
               "Number of LSL entries for swap instructions"),
      ADD_STAT(numLSLReadDataEntries, statistics::units::Count::get(),
               "Number of LSL entries with read data (load or swap)"),
      ADD_STAT(cpi,
               statistics::units::Rate<statistics::units::Cycle,
                                       statistics::units::Count>::get(),
               "CPI: Cycles Per Instruction"),
      ADD_STAT(totalCpi,
               statistics::units::Rate<statistics::units::Cycle,
                                       statistics::units::Count>::get(),
               "CPI: Total CPI of All Threads"),
      ADD_STAT(ipc,
               statistics::units::Rate<statistics::units::Count,
                                       statistics::units::Cycle>::get(),
               "IPC: Instructions Per Cycle"),
      ADD_STAT(totalIpc,
               statistics::units::Rate<statistics::units::Count,
                                       statistics::units::Cycle>::get(),
               "IPC: Total IPC of All Threads"),
      ADD_STAT(intRegfileReads, statistics::units::Count::get(),
               "Number of integer regfile reads"),
      ADD_STAT(intRegfileWrites, statistics::units::Count::get(),
               "Number of integer regfile writes"),
      ADD_STAT(fpRegfileReads, statistics::units::Count::get(),
               "Number of floating regfile reads"),
      ADD_STAT(fpRegfileWrites, statistics::units::Count::get(),
               "Number of floating regfile writes"),
      ADD_STAT(vecRegfileReads, statistics::units::Count::get(),
               "number of vector regfile reads"),
      ADD_STAT(vecRegfileWrites, statistics::units::Count::get(),
               "number of vector regfile writes"),
      ADD_STAT(vecPredRegfileReads, statistics::units::Count::get(),
               "number of predicate regfile reads"),
      ADD_STAT(vecPredRegfileWrites, statistics::units::Count::get(),
               "number of predicate regfile writes"),
      ADD_STAT(ccRegfileReads, statistics::units::Count::get(),
               "number of cc regfile reads"),
      ADD_STAT(ccRegfileWrites, statistics::units::Count::get(),
               "number of cc regfile writes"),
      ADD_STAT(miscRegfileReads, statistics::units::Count::get(),
               "number of misc regfile reads"),
      ADD_STAT(miscRegfileWrites, statistics::units::Count::get(),
               "number of misc regfile writes")
{
    // Register any of the O3CPU's stats here.
    timesIdled
        .prereq(timesIdled);

    idleCycles
        .prereq(idleCycles);

    quiesceCycles
        .prereq(quiesceCycles);

    // Number of Instructions simulated
    // --------------------------------
    // Should probably be in Base CPU but need templated
    // MaxThreads so put in here instead
    committedInsts
        .init(cpu->numThreads)
        .flags(statistics::total);

    committedOps
        .init(cpu->numThreads)
        .flags(statistics::total);

    uncheckedCommittedInsts
        .init(cpu->numThreads)
        .flags(statistics::total);

    uncheckedCommittedOps
        .init(cpu->numThreads)
        .flags(statistics::total);

    committedStaticInsts
        .init(cpu->numThreads)
        .flags(statistics::total);

    checkedCommittedStaticInsts
        .init(cpu->numThreads)
        .flags(statistics::total);
        
    uncheckedCommittedStaticInsts
        .flags(statistics::total);
    uncheckedCommittedStaticInsts = committedStaticInsts - checkedCommittedStaticInsts;

    committedPCs
        .init(cpu->numThreads)
        .flags(statistics::total);

    checkedCommittedPCs
        .init(cpu->numThreads)
        .flags(statistics::total);
        
    uncheckedCommittedPCs
        .flags(statistics::total);
    uncheckedCommittedPCs = committedPCs - checkedCommittedPCs;

    numSyscallCpts
        .init(cpu->numThreads)
        .flags(statistics::total);
        
    numLSLFullCpts
        .init(cpu->numThreads)
        .flags(statistics::total);
        
    numTimeoutCpts
        .init(cpu->numThreads)
        .flags(statistics::total);
        
    numBlockingCpts
        .init(cpu->numThreads)
        .flags(statistics::total);

    numErrCpts
        .init(cpu->numThreads)
        .flags(statistics::total);

    numOtherCpts
        .init(cpu->numThreads)
        .flags(statistics::total);
    
    numLSLLoadEntries
        .init(cpu->numThreads)
        .flags(statistics::total);

    numLSLSwapEntries
        .init(cpu->numThreads)
        .flags(statistics::total);
    
    numLSLReadDataEntries
        .flags(statistics::total);
    numLSLReadDataEntries = numLSLLoadEntries + numLSLSwapEntries;

    cpi
        .precision(6);
    cpi = cpu->baseStats.numCycles / committedInsts;

    totalCpi
        .precision(6);
    totalCpi = cpu->baseStats.numCycles / sum(committedInsts);

    ipc
        .precision(6);
    ipc = committedInsts / cpu->baseStats.numCycles;

    totalIpc
        .precision(6);
    totalIpc = sum(committedInsts) / cpu->baseStats.numCycles;

    intRegfileReads
        .prereq(intRegfileReads);

    intRegfileWrites
        .prereq(intRegfileWrites);

    fpRegfileReads
        .prereq(fpRegfileReads);

    fpRegfileWrites
        .prereq(fpRegfileWrites);

    vecRegfileReads
        .prereq(vecRegfileReads);

    vecRegfileWrites
        .prereq(vecRegfileWrites);

    vecPredRegfileReads
        .prereq(vecPredRegfileReads);

    vecPredRegfileWrites
        .prereq(vecPredRegfileWrites);

    ccRegfileReads
        .prereq(ccRegfileReads);

    ccRegfileWrites
        .prereq(ccRegfileWrites);

    miscRegfileReads
        .prereq(miscRegfileReads);

    miscRegfileWrites
        .prereq(miscRegfileWrites);
}

void
CPU::tick()
{
    DPRINTF(O3CPU, "\n\nO3CPU: Ticking main, O3CPU.\n");
    assert(!switchedOut());
    assert(drainState() != DrainState::Drained);
    if (isChecker() &&
        !loadstorelogentry::isMainReady(getContext(0)->contextId()))
    {
        drain();
    }

    ++baseStats.numCycles;
    updateCycleCounters(BaseCPU::CPU_STATE_ON);

//    activity = false;

    //Tick each of the stages
    fetch.tick();

    decode.tick();

    rename.tick();

    iew.tick();

    commit.tick();

    // Now advance the time buffers
    timeBuffer.advance();

    fetchQueue.advance();
    decodeQueue.advance();
    renameQueue.advance();
    iewQueue.advance();

    activityRec.advance();

    if (removeInstsThisCycle) {
        cleanUpRemovedInsts();
    }

    if (!tickEvent.scheduled()) {
        if (_status == SwitchedOut) {
            DPRINTF(O3CPU, "Switched out!\n");
            // increment stat
            lastRunningCycle = curCycle();
        } else if (!activityRec.active() || _status == Idle) {
            DPRINTF(O3CPU, "Idle!\n");
            lastRunningCycle = curCycle();
            cpuStats.timesIdled++;
        } else {
            schedule(tickEvent, clockEdge(Cycles(1)));
            DPRINTF(O3CPU, "Scheduling next tick!\n");
        }
    }

    if (!FullSystem)
        updateThreadPriority();

    tryDrain();
}

void
CPU::init()
{
    BaseCPU::init();

    for (ThreadID tid = 0; tid < numThreads; ++tid) {
        // Set noSquashFromTC so that the CPU doesn't squash when initially
        // setting up registers.
        thread[tid]->noSquashFromTC = true;
    }

    // Clear noSquashFromTC.
    for (int tid = 0; tid < numThreads; ++tid)
        thread[tid]->noSquashFromTC = false;

    commit.setThreads(thread);
}

void
CPU::startup()
{
    DPRINTF(O3AsChecker, "startup %s cpu %d\n",
            (isChecker() ? "checker" : "main"), getContext(0)->contextId());
    BaseCPU::startup();

    fetch.startupStage();
    decode.startupStage();
    iew.startupStage();
    rename.startupStage();
    commit.startupStage();
    if (isChecker()) {
        drain();
    }
}

void
CPU::activateThread(ThreadID tid)
{
    std::list<ThreadID>::iterator isActive =
        std::find(activeThreads.begin(), activeThreads.end(), tid);

    DPRINTF(O3CPU, "[tid:%i] Calling activate thread.\n", tid);
    assert(!switchedOut());

    if (isActive == activeThreads.end()) {
        DPRINTF(O3CPU, "[tid:%i] Adding to active threads list\n", tid);

        activeThreads.push_back(tid);
    }
}

void
CPU::deactivateThread(ThreadID tid)
{
    // hardware transactional memory
    // shouldn't deactivate thread in the middle of a transaction
    assert(!commit.executingHtmTransaction(tid));

    //Remove From Active List, if Active
    std::list<ThreadID>::iterator thread_it =
        std::find(activeThreads.begin(), activeThreads.end(), tid);

    DPRINTF(O3CPU, "[tid:%i] Calling deactivate thread.\n", tid);
    assert(!switchedOut());

    if (thread_it != activeThreads.end()) {
        DPRINTF(O3CPU,"[tid:%i] Removing from active threads list\n",
                tid);
        activeThreads.erase(thread_it);
    }

    fetch.deactivateThread(tid);
    commit.deactivateThread(tid);
}

Counter
CPU::totalInsts() const
{
    Counter total(0);

    ThreadID size = thread.size();
    for (ThreadID i = 0; i < size; i++)
        total += thread[i]->numInst;

    return total;
}

Counter
CPU::totalOps() const
{
    Counter total(0);

    ThreadID size = thread.size();
    for (ThreadID i = 0; i < size; i++)
        total += thread[i]->numOp;

    return total;
}

void
CPU::activateContext(ThreadID tid)
{
    assert(!switchedOut());

    // Needs to set each stage to running as well.
    activateThread(tid);

    // We don't want to wake the CPU if it is drained. In that case,
    // we just want to flag the thread as active and schedule the tick
    // event from drainResume() instead.

    assert(drainState()!= DrainState::Drained);

    if (drainState() == DrainState::Drained)
        return;

    // If we are time 0 or if the last activation time is in the past,
    // schedule the next tick and wake up the fetch unit
    if (lastActivatedCycle == 0 || lastActivatedCycle < curTick()) {
        scheduleTickEvent(Cycles(0));

        // Be sure to signal that there's some activity so the CPU doesn't
        // deschedule itself.
        activityRec.activity();
        fetch.wakeFromQuiesce();

        Cycles cycles(curCycle() - lastRunningCycle);
        // @todo: This is an oddity that is only here to match the stats
        if (cycles != 0)
            --cycles;
        cpuStats.quiesceCycles += cycles;

        lastActivatedCycle = curTick();

        _status = Running;

        BaseCPU::activateContext(tid);
    }
}

void
CPU::suspendContext(ThreadID tid)
{
    DPRINTF(O3CPU,"[tid:%i] Suspending Thread Context.\n", tid);
    assert(!switchedOut());

    deactivateThread(tid);

    // If this was the last thread then unschedule the tick event.
    if (activeThreads.size() == 0) {
        unscheduleTickEvent();
        lastRunningCycle = curCycle();
        _status = Idle;
    }

    DPRINTF(Quiesce, "Suspending Context\n");

    BaseCPU::suspendContext(tid);


    printf("thread %d suspended\n",getContext(0)->contextId());
    if (!isChecker()) {
        int cpuID = getContext(0)->contextId();
        loadstorelogentry::mainCPUMeta[cpuID].committed_timestamp =
            loadstorelogentry::mainCPUMeta[cpuID].timestamp;
    }
}

void
CPU::haltContext(ThreadID tid)
{
    //For now, this is the same as deallocate
    DPRINTF(O3CPU,"[tid:%i] Halt Context called. Deallocating\n", tid);
    assert(!switchedOut());

    deactivateThread(tid);
    removeThread(tid);

    // If this was the last thread then unschedule the tick event.
    if (activeThreads.size() == 0) {
        if (tickEvent.scheduled())
        {
            unscheduleTickEvent();
        }
        lastRunningCycle = curCycle();
        _status = Idle;
    }
    updateCycleCounters(BaseCPU::CPU_STATE_SLEEP);
}

void
CPU::insertThread(ThreadID tid)
{
    DPRINTF(O3CPU,"[tid:%i] Initializing thread into CPU");
    // Will change now that the PC and thread state is internal to the CPU
    // and not in the ThreadContext.
    gem5::ThreadContext *src_tc;
    if (FullSystem)
        src_tc = system->threads[tid];
    else
        src_tc = tcBase(tid);

    //Bind Int Regs to Rename Map
    const auto &regClasses = isa[tid]->regClasses();

    for (auto type = (RegClassType)0; type <= CCRegClass;
            type = (RegClassType)(type + 1)) {
        for (RegIndex idx = 0; idx < regClasses.at(type).numRegs(); idx++) {
            PhysRegIdPtr phys_reg = freeList.getReg(type);
            renameMap[tid].setEntry(RegId(type, idx), phys_reg);
            scoreboard.setReg(phys_reg);
        }
    }

    //Copy Thread Data Into RegFile
    //copyFromTC(tid);

    //Set PC/NPC/NNPC
    pcState(src_tc->pcState(), tid);

    src_tc->setStatus(gem5::ThreadContext::Active);

    activateContext(tid);

    //Reset ROB/IQ/LSQ Entries
    commit.rob->resetEntries();
}

void
CPU::removeThread(ThreadID tid)
{
    DPRINTF(O3CPU,"[tid:%i] Removing thread context from CPU.\n", tid);

    // Copy Thread Data From RegFile
    // If thread is suspended, it might be re-allocated
    // copyToTC(tid);


    // @todo: 2-27-2008: Fix how we free up rename mappings
    // here to alleviate the case for double-freeing registers
    // in SMT workloads.

    // clear all thread-specific states in each stage of the pipeline
    // since this thread is going to be completely removed from the CPU
    commit.clearStates(tid);
    fetch.clearStates(tid);
    decode.clearStates(tid);
    rename.clearStates(tid);
    iew.clearStates(tid);

    // Flush out any old data from the time buffers.
    for (int i = 0; i < timeBuffer.getSize(); ++i) {
        timeBuffer.advance();
        fetchQueue.advance();
        decodeQueue.advance();
        renameQueue.advance();
        iewQueue.advance();
    }

    // at this step, all instructions in the pipeline should be already
    // either committed successfully or squashed. All thread-specific
    // queues in the pipeline must be empty.
    assert(iew.instQueue.getCount(tid) == 0);
    assert(iew.ldstQueue.getCount(tid) == 0);
    assert(commit.rob->isEmpty(tid));

    // Reset ROB/IQ/LSQ Entries

    // Commented out for now.  This should be possible to do by
    // telling all the pipeline stages to drain first, and then
    // checking until the drain completes.  Once the pipeline is
    // drained, call resetEntries(). - 10-09-06 ktlim
/*
    if (activeThreads.size() >= 1) {
        commit.rob->resetEntries();
        iew.resetEntries();
    }
*/
}

Fault
CPU::getInterrupts()
{
    // Check if there are any outstanding interrupts
    return interrupts[0]->getInterrupt();
}

void
CPU::processInterrupts(const Fault &interrupt)
{
    // Check for interrupts here.  For now can copy the code that
    // exists within isa_fullsys_traits.hh.  Also assume that thread 0
    // is the one that handles the interrupts.
    // @todo: Possibly consolidate the interrupt checking code.
    // @todo: Allow other threads to handle interrupts.

    assert(interrupt != NoFault);
    interrupts[0]->updateIntrInfo();

    DPRINTF(O3CPU, "Interrupt %s being handled\n", interrupt->name());
    trap(interrupt, 0, nullptr);
}

void
CPU::trap(const Fault &fault, ThreadID tid, const StaticInstPtr &inst)
{
    // Pass the thread's TC into the invoke method.
    fault->invoke(threadContexts[tid], inst);
}

void
CPU::serializeThread(CheckpointOut &cp, ThreadID tid) const
{
    thread[tid]->serialize(cp);
}

void
CPU::unserializeThread(CheckpointIn &cp, ThreadID tid)
{
    thread[tid]->unserialize(cp);
}

void
CPU::reset() {
    assert(0 && "not used anymore\n");
}

bool
CPU::addToLoadStoreLog(DynInstPtr head_inst, Addr oldPc, bool wasSyscall) {
    assert(!isChecker());
    bool done_write = false;
    if (lastMicroPC >= 0) {
        if (!(oldPc == lastPC && lastMicroPC < head_inst->pcState().microPC()))
        {
            commitSeqNum++;
        }
    } else {
        commitSeqNum++;
    }
    lastPC = oldPc;
    lastMicroPC = head_inst->pcState().microPC();
    if (debug::O3LdStLogSeqNum) {
        std::string inst_str;
        head_inst->dump(inst_str);
        DPRINTF(O3LdStLogSeqNum,
                "inst: %s, inst seqnum: %lld, inst adjust seqnum: %lld, inst "
                "commit seqnum: %lld\n",
                inst_str, head_inst->getLdstlogSeqNum(),
                head_inst->getAdjustSeqNum(), commitSeqNum);
    }
    assert(head_inst->getAdjustSeqNum() == commitSeqNum || canContinueUnchecked());
    // The commit loadstorelog sequence number should be incrementing
    assert(head_inst->getLdstlogSeqNum() >= getLastCommitLdstlogSeq());

	                    uint64_t secondaryData = 0;
	                    int cpuID = getContext(0)->contextId();
                    if (head_inst->isStoreConditional()) {
                    //     std::cout << "Store conditional with secondaryData = " << head_inst->extraData << " => ";
                     //    head_inst->dump();
                        secondaryData = head_inst->extraData;
                        //Invert, as ARM ISA does so apparently compared to cache system?
                        // std::cout << "Extra value: " << secondaryData << "\n";
                    }

                    loadstorelogentry l (head_inst->isLoad(),head_inst->isStoreConditional(),head_inst->effAddr,head_inst->memData,head_inst->effSize,secondaryData,curTick(),oldPc,head_inst->staticInst->getName(),head_inst->memReqFlags, head_inst->pcState().microPC(), head_inst->getLdstlogSeqNum());
                    if(!head_inst->isLoad() && !head_inst->memData) { // Swap instructions are isAtomic but not isLoad
                        /* This means that the write has not been propagated to
                         * memory yet. The logentrydata field has been added to
                         * bypass the lsq and hold the write data directly.
                         * Probably not representative of a real hardware
                         * implementation though.
                         */
                        if (!head_inst->logentrydata.size()) {
                            // std::cout << "!$! Store instruction with no data (of size " << (int)head_inst->effSize << ") at time " << loadstorelogentry::timestamp[cpuID] << " -> ";
                            // head_inst->dump();
                            l.data.clear();
                            l.data.resize(0, 0);
                        } else {
                            l.data.assign(head_inst->logentrydata.data(), &(head_inst->logentrydata.data()[head_inst->effSize]));
#ifdef LOADSTORE_ERRORRATE
                            if(!(wasSyscall || loadstorelogentry::checkerCPUMeta[loadstorelogentry::mainCPUMeta[cpuID].current_segment_to_fill].hasSyscall))compromise_loadstorelogentry(cpuID, &l);

#endif
                            uint8_t oldData[head_inst->effSize];
                            bool newline = false;
                            Fault read = readMem(head_inst->effAddr, oldData, head_inst->effSize, head_inst->pcState().instAddr(), head_inst->memReqFlags, newline);
                            assert(read == NoFault);
                            l.oldData.assign(oldData, &(oldData[head_inst->effSize]));
#ifdef ROLLBACK_DEBUG
                            std::cout<< "Writing over " << head_inst->effAddr  << " data ";
                            for (auto i: oldData)
                                std::cout << i << ' ';

                            std::cout << "\n";
#endif

			    // Make the checker cores issue fake reqs
			    if (fakeReqsEnabled()){
			        unsigned filled_checker_id = loadstorelogentry::mainCPUMeta[cpuID].current_segment_to_fill + NUMBEROFMAINCORES;
			        if (currentFilledCheckerId != INVALID_CPUID
				    && currentFilledCheckerId != filled_checker_id){
				    // Disable fake reqs in previous checker core
				    if (system->cpuIsFakeReqIssueEventEnabled(currentFilledCheckerId))
                                        system->cpuSuspendFakeReqIssueEvent(currentFilledCheckerId);

				    // Save for later
				    currentFilledCheckerId = filled_checker_id;

				    // Enable fake reqs in new checker core
                                    system->cpuActivateFakeReqIssueEvent(filled_checker_id);
			        } else {
				    if (!system->cpuIsFakeReqIssueEventEnabled(filled_checker_id))
                                        system->cpuActivateFakeReqIssueEvent(filled_checker_id);
			        }
			    }

                            done_write = loadstorelogentry::do_write(l,cpuID,newline, 
                                                                     head_inst->pcState().microPC() > 0, committedInstrs); // Is at least the second micro-op
                            if (head_inst->isAtomic()) { // Swap instructions
                                cpuStats.numLSLSwapEntries[head_inst->threadNumber]++;
                            }
                        }
                        // std::cout << "Successful write on the logentry at time " << entry->time << std::endl;
                    } else {
#ifdef LOADSTORE_ERRORRATE
                            if(!(wasSyscall || loadstorelogentry::checkerCPUMeta[loadstorelogentry::mainCPUMeta[cpuID].current_segment_to_fill].hasSyscall))compromise_loadstorelogentry(cpuID, &l);
#endif

                        if(!head_inst->isLoad()) { // Swap instructions are isAtomic but not isLoad
                            assert(head_inst->oldData.data());
                            l.oldData.assign(head_inst->oldData.data(), &(head_inst->oldData.data()[head_inst->effSize]));
#ifdef ROLLBACK_DEBUG
                            std::cout<< "Writing over (2) " << head_inst->effAddr  << " data ";
                            for (auto i: head_inst->oldData)
                                std::cout << i << ' ';

                            std::cout << "\n";
#endif
                        }

  			// Make the checker cores issue fake reqs
			if (fakeReqsEnabled()){
			    unsigned filled_checker_id = loadstorelogentry::mainCPUMeta[cpuID].current_segment_to_fill + NUMBEROFMAINCORES;
			    if (currentFilledCheckerId != INVALID_CPUID 
			        && currentFilledCheckerId != filled_checker_id){
				// Disable fake reqs in previous checker core
				if (system->cpuIsFakeReqIssueEventEnabled(currentFilledCheckerId))
                                    system->cpuSuspendFakeReqIssueEvent(currentFilledCheckerId);

				// Save for later
				currentFilledCheckerId = filled_checker_id;

				// Enable fake reqs in new checker core
                                system->cpuActivateFakeReqIssueEvent(filled_checker_id);
			    } else {
				if (!system->cpuIsFakeReqIssueEventEnabled(filled_checker_id))
                                    system->cpuActivateFakeReqIssueEvent(filled_checker_id);
			    }
                        }

                        done_write = loadstorelogentry::do_write(l,cpuID,head_inst->newline, 
                                                                     head_inst->pcState().microPC() > 0, committedInstrs); // Is at least the second micro-op
                        if (head_inst->isLoad()) {
                            cpuStats.numLSLLoadEntries[head_inst->threadNumber]++;
                        } else if (head_inst->isAtomic()) { // Swap instructions
                            cpuStats.numLSLSwapEntries[head_inst->threadNumber]++;
                        }

                    }
                    // if (done_write) std::cout << "DONE WRITE at time " << loadstorelogentry::timestamp[cpuID] << std::endl;
	return done_write;
}


DrainState
CPU::drain()
{
    // Deschedule any power gating event (if any)
    deschedulePowerGatingEvent();

    if (isChecker()) {
        DPRINTF(O3AsChecker, "Draining\n");
    }

    DPRINTF(Drain, "Attempt to drain\n");
    // If the CPU isn't doing anything, then return immediately.
    if (switchedOut()){
	DPRINTF(Drain, "Switched out!!\n");
        return DrainState::Drained;
    }

    DPRINTF(Drain, "Draining...\n");

    // We only need to signal a drain to the commit stage as this
    // initiates squashing controls the draining. Once the commit
    // stage commits an instruction where it is safe to stop, it'll
    // squash the rest of the instructions in the pipeline and force
    // the fetch stage to stall. The pipeline will be drained once all
    // in-flight instructions have retired.
    commit.drain();

    // Wake the CPU and record activity so everything can drain out if
    // the CPU was not able to immediately drain.
    if (!isCpuDrained())  {
        // If a thread is suspended, wake it up so it can be drained
        for (auto t : threadContexts) {
            if (t->status() == gem5::ThreadContext::Suspended){
                DPRINTF(Drain, "Currently suspended so activate %i \n",
                        t->threadId());
                t->activate();
                // As the thread is now active, change the power state as well
                activateContext(t->threadId());
            }
        }

        wakeCPU();
        activityRec.activity();

        DPRINTF(Drain, "CPU not drained\n");

	// Check the drain state
	setIsDraining();

        return DrainState::Draining;
    } else {
        std::cout << "Already drained" << std::endl;

        DPRINTF(Drain, "CPU is already drained\n");
        if (tickEvent.scheduled())
            deschedule(tickEvent);

        // Flush out any old data from the time buffers.  In
        // particular, there might be some data in flight from the
        // fetch stage that isn't visible in any of the CPU buffers we
        // test in isCpuDrained().
        for (int i = 0; i < timeBuffer.getSize(); ++i) {
            timeBuffer.advance();
            fetchQueue.advance();
            decodeQueue.advance();
            renameQueue.advance();
            iewQueue.advance();
        }

        drainSanityCheck();
	// Make the CPU signal the drain is over
	signalDrainDone();
        return DrainState::Drained;
    }
}

extern void copyRegisters(BaseCPU* cpu, int checkerCoreId);

void
CPU::haveDrained()
{
    //drainSanityCheck();
}

bool
CPU::tryDrain()
{
    if (drainState() != DrainState::Draining || !isCpuDrained())
        return false;

    if (tickEvent.scheduled()){
	DPRINTF(Drain, "Descheduling tickEvent\n");
        deschedule(tickEvent);
    }

    DPRINTF(Drain, "CPU done draining, processing drain event\n");
    signalDrainDone();
    return true;
}

void
CPU::signalDrainDone()
{
    DPRINTF(Drain, "SignalDrainDone\n");

    // Change drain state
    setDrained();
    
    if (isChecker()) {
        // Should happen only once
        assert(loadstorelogentry::checkerCPUMeta[getContext(0)->contextId()
            -NUMBEROFMAINCORES].checkerDrainDoneTick == 0);
        // Should have been set if check started
        if (loadstorelogentry::checkerCPUMeta[getContext(0)->contextId()
            -NUMBEROFMAINCORES].checkerLastCommitTick > 0)
        {
            loadstorelogentry::checkerCPUMeta[getContext(0)->contextId()
                -NUMBEROFMAINCORES].checkerDrainDoneTick = curTick();
            loadstorelogentry::cptCheckerLastCommitToDrainDoneDelayTicks += 
                loadstorelogentry::checkerCPUMeta[getContext(0)->contextId()
                    -NUMBEROFMAINCORES].checkerDrainDoneTick
                - loadstorelogentry::checkerCPUMeta[getContext(0)->contextId()
                    -NUMBEROFMAINCORES].checkerLastCommitTick;
        }
        loadstorelogentry::commit_minor_checkpoint(this);
    }
    
    std::cout << curTick() << " Drained\n" << std::endl;
    if (!isChecker()) {
        int cpuID = getContext(0)->contextId();
        assert(loadstorelogentry::mainCPUMeta[cpuID].mainCoreErroneous);
        int remove = loadstorelogentry::mainCPURollback(this);
        if (remove > 0) {
            thread[0]->numInst -= remove;
            // thread[0]->numInsts -= remove;
            cpuStats.committedInsts[0] -= remove;
        }
    }

    DPRINTF(Drain, "SignalDrainDone end\n");
    
}

void
CPU::drainSanityCheck() const
{
    assert(isCpuDrained());
    fetch.drainSanityCheck();
    decode.drainSanityCheck();
    rename.drainSanityCheck();
    iew.drainSanityCheck();
    commit.drainSanityCheck();
}

bool
CPU::isCpuDrained() const
{
    bool drained(true);

    if (!instList.empty() || !removeList.empty()) {
        DPRINTF(Drain, "Main CPU structures not drained.\n");
        drained = false;
    }

    if (!fetch.isDrained()) {
        DPRINTF(Drain, "Fetch not drained.\n");
        drained = false;
    }

    if (!decode.isDrained()) {
        DPRINTF(Drain, "Decode not drained.\n");
        drained = false;
    }

    if (!rename.isDrained()) {
        DPRINTF(Drain, "Rename not drained.\n");
        drained = false;
    }

    if (!iew.isDrained()) {
        DPRINTF(Drain, "IEW not drained.\n");
        drained = false;
    }

    if (!commit.isDrained()) {
        DPRINTF(Drain, "Commit not drained.\n");
        drained = false;
    }

    return drained;
}

void CPU::commitDrained(ThreadID tid) { fetch.drainStall(tid); }

void
CPU::drainResume()
{
    if (switchedOut())
        return;

    setRunning();

    DPRINTF(Drain, "Resuming...\n");
    verifyMemoryMode();
    if (isChecker() &&
        !loadstorelogentry::isMainReady(getContext(0)->contextId())) {
        _status = Idle;
        for (ThreadID i = 0; i < thread.size(); i++) {
            if (thread[i]->status() == gem5::ThreadContext::Active) {
                DPRINTF(Drain, "Activating thread: %i\n", i);
                activateThread(i);
                _status = Running;
            }
        }
        DPRINTF(O3AsChecker, "Scheduling drain for next cycle\n");
        // Just switched over, the CPU is Idle
        schedule(drainEvent, nextCycle());
        // Reschedule any power gating event (if any)
        schedulePowerGatingEvent();
        return;
    }

    fetch.drainResume();
    commit.drainResume();

    _status = Idle;
    for (ThreadID i = 0; i < thread.size(); i++) {
        if (thread[i]->status() == gem5::ThreadContext::Active) {
            DPRINTF(Drain, "Activating thread: %i\n", i);
            activateThread(i);
            _status = Running;
        }
    }

    assert(!tickEvent.scheduled());
    if (_status == Running)
        schedule(tickEvent, nextCycle());

    // Reschedule any power gating event (if any)
    schedulePowerGatingEvent();
}


Fault
CPU::readMem(Addr addr, uint8_t *data, unsigned size, Addr pc,
                         Request::Flags flags, bool &newLine)
{
    Fault fault = NoFault;
    int fullSize = size;
    Addr secondAddr = roundDown(addr + size - 1, cacheLineSize());
    bool checked_flags = false;
    bool flags_match = true;

    RequestPtr memReq;


    if (secondAddr > addr)
        size = secondAddr - addr;

    // Need to account for multiple accesses like the Atomic and TimingSimple
    while (1) {
        memReq = std::make_shared<Request>(addr, size, flags, instRequestorId(),
                             pc, thread[0]->contextId());

        // translate to physical address
        fault = mmu->translateFunctional(memReq, thread[0]->getTC(), BaseMMU::Read);

        if (!checked_flags && fault == NoFault) {
            // flags_match = checkFlags(unverifiedReq, memReq->getVaddr(),
            //                         memReq->getPaddr(), memReq->getFlags());
            checked_flags = true;
        }

        // Now do the access
        if (fault == NoFault &&
                !memReq->getFlags().isSet(Request::NO_ACCESS)) {
            PacketPtr pkt = Packet::createRead(memReq);

            pkt->dataStatic(data);

            if (!(memReq->isUncacheable()/* || memReq->isMmappedIpr()*/)) {
                // Access memory to see if we have the same data
                iew.ldstQueue.getDataPort().sendFunctional(pkt);
            } else {
                // Assume the data is correct if it's an uncached access
                // memcpy(data, unverifiedMemData, size);
            }

            newLine = pkt->timestamp != loadstorelogentry::mainCPUMeta[getContext(0)->contextId()].timestamp;


            memReq = NULL;
            delete pkt;
        }

        if (fault != NoFault) {
            if (memReq->isPrefetch()) {
                fault = NoFault;
            }
            memReq = NULL;
            break;
        }


        //If we don't need to access a second cache line, stop now.
        if (secondAddr <= addr)
        {
            break;
        }


        // Setup for accessing next cache line
        data += size;
        size = addr + fullSize - secondAddr;
        addr = secondAddr;
    }

    if (!flags_match) {
        //  warn("%lli: Flags do not match CPU:%#x %#x %#x Checker:%#x %#x %#x\n",
        //      curTick(), unverifiedReq->getVaddr(), unverifiedReq->getPaddr(),
        //       unverifiedReq->getFlags(), addr, pAddr, flags);
        //handleError();
    }

    return fault;
}

Fault
CPU::writeMem(Addr addr, uint8_t *data, unsigned size, Addr pc,
                          Request::Flags flags)
{
    Fault fault = NoFault;
    int fullSize = size;
    Addr secondAddr = roundDown(addr + size - 1, cacheLineSize());
    bool checked_flags = false;
    bool flags_match = true;

    RequestPtr memReq;


    if (secondAddr > addr)
        size = secondAddr - addr;

    // Need to account for multiple accesses like the Atomic and TimingSimple
    while (1) {
        memReq = std::make_shared<Request>(addr, size, flags, instRequestorId(),
                             pc, thread[0]->contextId());

        // translate to physical address
        fault = mmu->translateFunctional(memReq, thread[0]->getTC(), BaseMMU::Write);

        if (!checked_flags && fault == NoFault) {
            // flags_match = checkFlags(unverifiedReq, memReq->getVaddr(),
            //                         memReq->getPaddr(), memReq->getFlags());
            checked_flags = true;
        }

        // Now do the access
        if (fault == NoFault &&
                !memReq->getFlags().isSet(Request::NO_ACCESS)) {
            PacketPtr pkt = Packet::createWrite(memReq);

            pkt->dataStatic(data);

            if (!(memReq->isUncacheable() /*|| memReq->isMmappedIpr()*/)) {
                // Access memory to see if we have the same data
                iew.ldstQueue.getDataPort().sendFunctional(pkt);
            } else {
                // Assume the data is correct if it's an uncached access
                // memcpy(data, unverifiedMemData, size);
            }

            memReq = NULL;
            delete pkt;
        }

        if (fault != NoFault) {
            if (memReq->isPrefetch()) {
                fault = NoFault;
            }
            memReq = NULL;
            break;
        }

        //If we don't need to access a second cache line, stop now.
        if (secondAddr <= addr)
        {
            break;
        }

        // Setup for accessing next cache line
        data += size;
        size = addr + fullSize - secondAddr;
        addr = secondAddr;
    }

    if (!flags_match) {
        //  warn("%lli: Flags do not match CPU:%#x %#x %#x Checker:%#x %#x %#x\n",
        //      curTick(), unverifiedReq->getVaddr(), unverifiedReq->getPaddr(),
        //       unverifiedReq->getFlags(), addr, pAddr, flags);
        //handleError();
    }

    return fault;
}

void
CPU::switchOut()
{
    DPRINTF(O3CPU, "Switching out\n");
    BaseCPU::switchOut();

    activityRec.reset();

    _status = SwitchedOut;

    if (checker)
        checker->switchOut();
}

void
CPU::takeOverFrom(BaseCPU *oldCPU)
{
    BaseCPU::takeOverFrom(oldCPU);

    fetch.takeOverFrom();
    decode.takeOverFrom();
    rename.takeOverFrom();
    iew.takeOverFrom();
    commit.takeOverFrom();

    assert(!tickEvent.scheduled());

    auto *oldO3CPU = dynamic_cast<CPU *>(oldCPU);
    if (oldO3CPU)
        globalSeqNum = oldO3CPU->globalSeqNum;

    lastRunningCycle = curCycle();
    _status = Idle;
}

void
CPU::verifyMemoryMode() const
{
    if (!system->isTimingMode()) {
        fatal("The O3 CPU requires the memory system to be in "
              "'timing' mode.\n");
    }
}

RegVal
CPU::readMiscRegNoEffect(int misc_reg, ThreadID tid) const
{
    return isa[tid]->readMiscRegNoEffect(misc_reg);
}

RegVal
CPU::readMiscReg(int misc_reg, ThreadID tid)
{
    cpuStats.miscRegfileReads++;
    return isa[tid]->readMiscReg(misc_reg);
}

void
CPU::setMiscRegNoEffect(int misc_reg, RegVal val, ThreadID tid)
{
    isa[tid]->setMiscRegNoEffect(misc_reg, val);
}

void
CPU::setMiscReg(int misc_reg, RegVal val, ThreadID tid)
{
    cpuStats.miscRegfileWrites++;
    isa[tid]->setMiscReg(misc_reg, val);
}

RegVal
CPU::getReg(PhysRegIdPtr phys_reg)
{
    switch (phys_reg->classValue()) {
      case IntRegClass:
        cpuStats.intRegfileReads++;
        break;
      case FloatRegClass:
        cpuStats.fpRegfileReads++;
        break;
      case CCRegClass:
        cpuStats.ccRegfileReads++;
        break;
      case VecRegClass:
      case VecElemClass:
        cpuStats.vecRegfileReads++;
        break;
      case VecPredRegClass:
        cpuStats.vecPredRegfileReads++;
        break;
      default:
        break;
    }
    return regFile.getReg(phys_reg);
}

void
CPU::getReg(PhysRegIdPtr phys_reg, void *val)
{
    switch (phys_reg->classValue()) {
      case IntRegClass:
        cpuStats.intRegfileReads++;
        break;
      case FloatRegClass:
        cpuStats.fpRegfileReads++;
        break;
      case CCRegClass:
        cpuStats.ccRegfileReads++;
        break;
      case VecRegClass:
      case VecElemClass:
        cpuStats.vecRegfileReads++;
        break;
      case VecPredRegClass:
        cpuStats.vecPredRegfileReads++;
        break;
      default:
        break;
    }
    regFile.getReg(phys_reg, val);
}

void *
CPU::getWritableReg(PhysRegIdPtr phys_reg)
{
    switch (phys_reg->classValue()) {
      case VecRegClass:
        cpuStats.vecRegfileReads++;
        break;
      case VecPredRegClass:
        cpuStats.vecPredRegfileReads++;
        break;
      default:
        break;
    }
    return regFile.getWritableReg(phys_reg);
}

void
CPU::setReg(PhysRegIdPtr phys_reg, RegVal val)
{
    switch (phys_reg->classValue()) {
      case IntRegClass:
        cpuStats.intRegfileWrites++;
        break;
      case FloatRegClass:
        cpuStats.fpRegfileWrites++;
        break;
      case CCRegClass:
        cpuStats.ccRegfileWrites++;
        break;
      case VecRegClass:
      case VecElemClass:
        cpuStats.vecRegfileWrites++;
        break;
      case VecPredRegClass:
        cpuStats.vecPredRegfileWrites++;
        break;
      default:
        break;
    }
    regFile.setReg(phys_reg, val);
}

void
CPU::setReg(PhysRegIdPtr phys_reg, const void *val)
{
    switch (phys_reg->classValue()) {
      case IntRegClass:
        cpuStats.intRegfileWrites++;
        break;
      case FloatRegClass:
        cpuStats.fpRegfileWrites++;
        break;
      case CCRegClass:
        cpuStats.ccRegfileWrites++;
        break;
      case VecRegClass:
      case VecElemClass:
        cpuStats.vecRegfileWrites++;
        break;
      case VecPredRegClass:
        cpuStats.vecPredRegfileWrites++;
        break;
      default:
        break;
    }
    regFile.setReg(phys_reg, val);
}

RegVal
CPU::getArchReg(const RegId &reg, ThreadID tid)
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(reg);
    return regFile.getReg(phys_reg);
}

void
CPU::getArchReg(const RegId &reg, void *val, ThreadID tid)
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(reg);
    regFile.getReg(phys_reg, val);
}

void *
CPU::getWritableArchReg(const RegId &reg, ThreadID tid)
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(reg);
    return regFile.getWritableReg(phys_reg);
}

void
CPU::setArchReg(const RegId &reg, RegVal val, ThreadID tid)
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(reg);
    regFile.setReg(phys_reg, val);
}

void
CPU::setArchReg(const RegId &reg, const void *val, ThreadID tid)
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(reg);
    regFile.setReg(phys_reg, val);
}

const PCStateBase &
CPU::pcState(ThreadID tid)
{
    return commit.pcState(tid);
}

void
CPU::pcState(const PCStateBase &val, ThreadID tid)
{
    commit.pcState(val, tid);
}

void
CPU::squashFromTC(ThreadID tid)
{
    thread[tid]->noSquashFromTC = true;
    commit.generateTCEvent(tid);
}

CPU::ListIt
CPU::addInst(const DynInstPtr &inst)
{
    instList.push_back(inst);

    return --(instList.end());
}

void
CPU::instDone(ThreadID tid, const DynInstPtr &inst)
{
    // Keep an instruction count.
    if (!inst->isMicroop() || inst->isLastMicroop()) {
        thread[tid]->numInst++;
        thread[tid]->threadStats.numInsts++;
        cpuStats.committedInsts[tid]++;
        if (!isChecker()) {
            auto insert_result = loadstorelogentry::addMainStaticInst(getContext(0)->contextId(), inst->staticInst->getName());
            if (insert_result.second) {
                cpuStats.committedStaticInsts[tid]++;
            }
            if (!isChecked()) {
                cpuStats.uncheckedCommittedInsts[tid]++;
            } else {
                if (loadstorelogentry::checkedMainStaticInst(insert_result.first)) {
                    cpuStats.checkedCommittedStaticInsts[tid]++; 
                }
            }
        }

        // Check for instruction-count-based events.
        thread[tid]->comInstEventQueue.serviceEvents(thread[tid]->numInst);
    }
    thread[tid]->numOp++;
    thread[tid]->threadStats.numOps++;
    cpuStats.committedOps[tid]++;
    if (!isChecker() && !isChecked()) {
        cpuStats.uncheckedCommittedOps[tid]++;
    }

    probeInstCommit(inst->staticInst, inst->pcState().instAddr());
}

void
CPU::removeFrontInst(const DynInstPtr &inst)
{
    DPRINTF(O3CPU, "Removing committed instruction [tid:%i] PC %s "
            "[sn:%lli]\n",
            inst->threadNumber, inst->pcState(), inst->seqNum);

    removeInstsThisCycle = true;

    // Remove the front instruction.
    removeList.push(inst->getInstListIt());
}

void
CPU::removeInstsNotInROB(ThreadID tid)
{
    DPRINTF(O3CPU, "Thread %i: Deleting instructions from instruction"
            " list.\n", tid);

    ListIt end_it;

    bool rob_empty = false;

    if (instList.empty()) {
        return;
    } else if (rob.isEmpty(tid)) {
        DPRINTF(O3CPU, "ROB is empty, squashing all insts.\n");
        end_it = instList.begin();
        rob_empty = true;
    } else {
        end_it = (rob.readTailInst(tid))->getInstListIt();
        DPRINTF(O3CPU, "ROB is not empty, squashing insts not in ROB.\n");
    }

    removeInstsThisCycle = true;

    ListIt inst_it = instList.end();

    inst_it--;

    // Walk through the instruction list, removing any instructions
    // that were inserted after the given instruction iterator, end_it.
    while (inst_it != end_it) {
        assert(!instList.empty());

        if ((*inst_it)->getLdstlogSeqNum() > 0) {
            if (insertSquashedLdStLogSeqNum((*inst_it)->getLdstlogSeqNum()))
            {
                incSquashedLdStLogSeqNumCount();
            }
            if ((*inst_it)->getLdstlogSeqNum() == getLdstlogSeq()) {
                incrementLdstlogSeq();
                DPRINTF(O3LdStLogSeqNum,
                        "incrementLdstlogSeq seqnum: %lld, isMicroop: %d, "
                        "isMemRef: %d, "
                        "isLastMicroop: %d, hasMemRef: %s\n",
                        getLdstlogSeq(), (*inst_it)->isMicroop(),
                        (*inst_it)->isMemRef(),
                        (*inst_it)->isLastMicroop(),
                        (*inst_it)->macroop ?
                            ((*inst_it)->macroop->hasMemRef() ? "1" : "0") :
                            "no macroop");
            }
            std::string inst_str;
            (*inst_it)->dump(inst_str);
            DPRINTF(O3LdStLogSeqNum,
                    "removeInstsNotInROB inst: %s, inst seqnum: %lld\n", inst_str,
                    (*inst_it)->getLdstlogSeqNum());
        }

        squashInstIt(inst_it, tid);

        inst_it--;
    }

    // If the ROB was empty, then we actually need to remove the first
    // instruction as well.
    if (rob_empty) {
        if ((*inst_it)->getLdstlogSeqNum() > 0) {
            if (insertSquashedLdStLogSeqNum((*inst_it)->getLdstlogSeqNum()))
            {
                incSquashedLdStLogSeqNumCount();
            }
            if ((*inst_it)->getLdstlogSeqNum() == getLdstlogSeq()) {
                incrementLdstlogSeq();
                DPRINTF(O3LdStLogSeqNum,
                        "incrementLdstlogSeq seqnum: %lld, isMicroop: %d, "
                        "isMemRef: %d, "
                        "isLastMicroop: %d, hasMemRef: %s\n",
                        getLdstlogSeq(), (*inst_it)->isMicroop(),
                        (*inst_it)->isMemRef(),
                        (*inst_it)->isLastMicroop(),
                        (*inst_it)->macroop ?
                            ((*inst_it)->macroop->hasMemRef() ? "1" : "0") :
                            "no macroop");
            }
            std::string inst_str;
            (*inst_it)->dump(inst_str);
            DPRINTF(O3LdStLogSeqNum,
                    "removeInstsNotInROB inst: %s, inst seqnum: %lld\n", inst_str,
                    (*inst_it)->getLdstlogSeqNum());
        }
        squashInstIt(inst_it, tid);
    }
}

void
CPU::removeInstsUntil(const InstSeqNum &seq_num, ThreadID tid)
{
    assert(!instList.empty());

    removeInstsThisCycle = true;

    ListIt inst_iter = instList.end();

    inst_iter--;

    DPRINTF(O3CPU, "Deleting instructions from instruction "
            "list that are from [tid:%i] and above [sn:%lli] (end=%lli).\n",
            tid, seq_num, (*inst_iter)->seqNum);

    while ((*inst_iter)->seqNum > seq_num) {

        bool break_loop = (inst_iter == instList.begin());

        if ((*inst_iter)->getLdstlogSeqNum() > 0) {
            if (insertSquashedLdStLogSeqNum((*inst_iter)->getLdstlogSeqNum()))
            {
                incSquashedLdStLogSeqNumCount();
            }
            if ((*inst_iter)->getLdstlogSeqNum() == getLdstlogSeq()) {
                incrementLdstlogSeq();
                DPRINTF(O3LdStLogSeqNum,
                        "incrementLdstlogSeq seqnum: %lld, isMicroop: %d, "
                        "isMemRef: %d, "
                        "isLastMicroop: %d, hasMemRef: %s\n",
                        getLdstlogSeq(), (*inst_iter)->isMicroop(),
                        (*inst_iter)->isMemRef(),
                        (*inst_iter)->isLastMicroop(),
                        (*inst_iter)->macroop ?
                            ((*inst_iter)->macroop->hasMemRef() ? "1" : "0") :
                            "no macroop");
            }
            std::string inst_str;
            (*inst_iter)->dump(inst_str);
            DPRINTF(O3LdStLogSeqNum,
                    "removeInstsUntil inst: %s, inst seqnum: %lld\n", inst_str,
                    (*inst_iter)->getLdstlogSeqNum());
        }

        squashInstIt(inst_iter, tid);

        inst_iter--;

        if (break_loop)
            break;
    }
}

void
CPU::squashInstIt(const ListIt &instIt, ThreadID tid)
{
    if ((*instIt)->threadNumber == tid) {
        DPRINTF(O3CPU, "Squashing instruction, "
                "[tid:%i] [sn:%lli] PC %s\n",
                (*instIt)->threadNumber,
                (*instIt)->seqNum,
                (*instIt)->pcState());

        // Mark it as squashed.
        (*instIt)->setSquashed();

        // @todo: Formulate a consistent method for deleting
        // instructions from the instruction list
        // Remove the instruction from the list.
        removeList.push(instIt);
    }
}

void
CPU::cleanUpRemovedInsts()
{
    while (!removeList.empty()) {
        DPRINTF(O3CPU, "Removing instruction, "
                "[tid:%i] [sn:%lli] PC %s\n",
                (*removeList.front())->threadNumber,
                (*removeList.front())->seqNum,
                (*removeList.front())->pcState());

        instList.erase(removeList.front());

        removeList.pop();
    }

    removeInstsThisCycle = false;
}
/*
void
CPU::removeAllInsts()
{
    instList.clear();
}
*/
void
CPU::dumpInsts()
{
    int num = 0;

    ListIt inst_list_it = instList.begin();

    cprintf("Dumping Instruction List\n");

    while (inst_list_it != instList.end()) {
        cprintf("Instruction:%i\nPC:%#x\n[tid:%i]\n[sn:%lli]\nIssued:%i\n"
                "Squashed:%i\n\n",
                num, (*inst_list_it)->pcState().instAddr(),
                (*inst_list_it)->threadNumber,
                (*inst_list_it)->seqNum, (*inst_list_it)->isIssued(),
                (*inst_list_it)->isSquashed());
        inst_list_it++;
        ++num;
    }
}

bool
CPU::insertSquashedLdStLogSeqNum(int64_t seqNum)
{
    auto result = squashedLdStLogSeqNumSet.insert(seqNum);
    return result.second;
}

void
CPU::setLoadstorelogSeqNum(int64_t seqNum)
{
    assert(isChecker());
    loadstorelogSeqNum = seqNum;
    squashedLdStLogSeqNumSet.clear();
    squashedLdStLogSeqNumCount = 0;
}

bool
CPU::isChecked() const
{
    return _isChecked;
}

bool
CPU::isStored() const
{
    return _isStored;
}

int
CPU::checkerID() const
{
    assert(isChecker());
    return threadContexts[0]->contextId() - NUMBEROFMAINCORES;
}

bool
CPU::canContinueUnchecked() const
{
    return _canContinueUnchecked;
}

bool 
CPU::sampledCheck() const 
{
    return _sampledCheck > 0;
}


bool 
CPU::shouldStartSample(uint64_t currCommittedInsts) const 
{
    bool shouldStart = sampledCheck() && ((currCommittedInsts / _sampledCheck) > lastSample);
    if (shouldStart) {
        std::cout << "shouldStart at "  << currCommittedInsts << " committedInsts" << std::endl;
    }
    return shouldStart;
}

void
CPU::updateLastSample(uint64_t currCommittedInsts)
{
    assert(sampledCheck());
    lastSample = currCommittedInsts / _sampledCheck;
}
/*
void
CPU::wakeDependents(const DynInstPtr &inst)
{
    iew.wakeDependents(inst);
}
*/
void
CPU::wakeCPU()
{
    if (activityRec.active() || tickEvent.scheduled()) {
        DPRINTF(Activity, "CPU already running.\n");
        return;
    }

    DPRINTF(Activity, "Waking up CPU\n");

    Cycles cycles(curCycle() - lastRunningCycle);
    // @todo: This is an oddity that is only here to match the stats
    if (cycles > 1) {
        --cycles;
        cpuStats.idleCycles += cycles;
        baseStats.numCycles += cycles;
    }

    schedule(tickEvent, clockEdge());
}

void
CPU::wakeup(ThreadID tid)
{
    if (thread[tid]->status() != gem5::ThreadContext::Suspended)
        return;

    wakeCPU();
    if (isChecker()) {
        drainResume();
    }

    DPRINTF(Quiesce, "Suspended Processor woken\n");
    threadContexts[tid]->activate();
}

ThreadID
CPU::getFreeTid()
{
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        if (!tids[tid]) {
            tids[tid] = true;
            return tid;
        }
    }

    return InvalidThreadID;
}

void
CPU::updateThreadPriority()
{
    if (activeThreads.size() > 1) {
        //DEFAULT TO ROUND ROBIN SCHEME
        //e.g. Move highest priority to end of thread list
        std::list<ThreadID>::iterator list_begin = activeThreads.begin();

        unsigned high_thread = *list_begin;

        activeThreads.erase(list_begin);

        activeThreads.push_back(high_thread);
    }
}

void
CPU::addThreadToExitingList(ThreadID tid)
{
    DPRINTF(O3CPU, "Thread %d is inserted to exitingThreads list\n", tid);

    // the thread trying to exit can't be already halted
    assert(tcBase(tid)->status() != gem5::ThreadContext::Halted);

    // make sure the thread has not been added to the list yet
    assert(exitingThreads.count(tid) == 0);

    // add the thread to exitingThreads list to mark that this thread is
    // trying to exit. The boolean value in the pair denotes if a thread is
    // ready to exit. The thread is not ready to exit until the corresponding
    // exit trap event is processed in the future. Until then, it'll be still
    // an active thread that is trying to exit.
    exitingThreads.emplace(std::make_pair(tid, false));
}

bool
CPU::isThreadExiting(ThreadID tid) const
{
    return exitingThreads.count(tid) == 1;
}

void
CPU::scheduleThreadExitEvent(ThreadID tid)
{
    assert(exitingThreads.count(tid) == 1);

    // exit trap event has been processed. Now, the thread is ready to exit
    // and be removed from the CPU.
    exitingThreads[tid] = true;

    // we schedule a threadExitEvent in the next cycle to properly clean
    // up the thread's states in the pipeline. threadExitEvent has lower
    // priority than tickEvent, so the cleanup will happen at the very end
    // of the next cycle after all pipeline stages complete their operations.
    // We want all stages to complete squashing instructions before doing
    // the cleanup.
    if (!threadExitEvent.scheduled()) {
        schedule(threadExitEvent, nextCycle());
    }
}

void
CPU::exitThreads()
{
    // there must be at least one thread trying to exit
    assert(exitingThreads.size() > 0);

    // terminate all threads that are ready to exit
    auto it = exitingThreads.begin();
    while (it != exitingThreads.end()) {
        ThreadID thread_id = it->first;
        bool readyToExit = it->second;

        if (readyToExit) {
            DPRINTF(O3CPU, "Exiting thread %d\n", thread_id);
            haltContext(thread_id);
            tcBase(thread_id)->setStatus(gem5::ThreadContext::Halted);
            it = exitingThreads.erase(it);
        } else {
            it++;
        }
    }
}

void
CPU::htmSendAbortSignal(ThreadID tid, uint64_t htm_uid,
        HtmFailureFaultCause cause)
{
    const Addr addr = 0x0ul;
    const int size = 8;
    const Request::Flags flags =
      Request::PHYSICAL|Request::STRICT_ORDER|Request::HTM_ABORT;

    // O3-specific actions
    iew.ldstQueue.resetHtmStartsStops(tid);
    commit.resetHtmStartsStops(tid);

    // notify l1 d-cache (ruby) that core has aborted transaction
    RequestPtr req =
        std::make_shared<Request>(addr, size, flags, _dataRequestorId);

    req->taskId(taskId());
    req->setContext(thread[tid]->contextId());
    req->setHtmAbortCause(cause);

    assert(req->isHTMAbort());

    PacketPtr abort_pkt = Packet::createRead(req);
    uint8_t *memData = new uint8_t[8];
    assert(memData);
    abort_pkt->dataStatic(memData);
    abort_pkt->setHtmTransactional(htm_uid);

    // TODO include correct error handling here
    if (!iew.ldstQueue.getDataPort().sendTimingReq(abort_pkt)) {
        panic("HTM abort signal was not sent to the memory subsystem.");
    }
}

} // namespace o3
} // namespace gem5
