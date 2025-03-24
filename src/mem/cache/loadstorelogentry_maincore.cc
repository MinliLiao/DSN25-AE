/* Authors: Sam Ainsworth
*
*/

#include "mem/cache/loadstorelogentry.hh"

#include "cpu/error_injection.hh"
#include "cpu/o3/cpu.hh"
#include "debug/LoadStoreLogMainContUnchecked.hh"
#include "debug/LoadStoreLogSeqNum.hh"
#include "debug/LoadStoreLogSleepGuard.hh"

//#include "sim/syscall_emul.hh"
#include <iostream>

#include "sim/syscalllog.hh"

namespace gem5 {


bool loadstorelogentry::allocate_little_for_big(int mainCPUID) {
    assert(mainCPUID < mainCPUMeta.size());


    for (int y=0; y<NUMBEROFCHECKERCORESPERCORE; y++) {
        int x = y + NUMBEROFCHECKERCORESPERCORE * mainCPUID;
        assert(x < NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE);

        for (int slot = 0; slot < num_checkSlot_per_checker; ++slot) {
            if (slot > 0) {
                x = x + NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE;
            }
            assert(x < checkerCPUMeta.size());

        if (checkerCPUMeta[x].segmentFree) {
            // printf("starting main cpu at %ld\n", curTick());
            mainCPUMeta[mainCPUID].current_segment_to_fill = x;
            if ((allCPUMeta[mainCPUID].baseCPU->canContinueUnchecked() ||
                 allCPUMeta[mainCPUID].baseCPU->sampledCheck()) &&
                !allCPUMeta[mainCPUID].baseCPU->isMain())
            {
                // Re-starting to take checkpoints, need to copy the initial
                // register files
                auto o3cpu =
                    dynamic_cast<o3::CPU *>(allCPUMeta[mainCPUID].baseCPU);
                assert(o3cpu);
                o3cpu->_isChecked = o3cpu->_wasChecked;
                o3cpu->_isStored = true;
                updateMainContexts(allCPUMeta[mainCPUID].baseCPU);
                if (allCPUMeta[mainCPUID].baseCPU->sampledCheck()) {
                    o3cpu->updateLastSample(o3cpu->cpuStats.committedInsts[0].value()); // in our experiments, there's only 1 thread per core
                }
                DPRINTF(LoadStoreLogMainContUnchecked,
                        "CPU %d continueing checked\n", mainCPUID);
            }
            //std::cout << "Reinitialising current_entry of " << mainCPUID << " to " << x << std::endl;
            checkerCPUMeta[x].segmentFree = false;
            checkerCPUMeta[x].erroneous = false;
            checkerCPUMeta[x].timestamps = ++mainCPUMeta[mainCPUID].timestamp;
            checkerCPUMeta[x].hasSyscall= false;
            //printf("allocating %d at timestamp %ld\n",x, checkerCPUMeta[x].timestamps);

            checkerCPUMeta[x].committedInstructions=0;
            checkerCPUMeta[x].currentCommittedInstructions=0;
            if (slot == 0) // Only set the core's committedInstrs if this is not a spare slot
            allCPUMeta[x+NUMBEROFMAINCORES].baseCPU->committedInstrs = 0;
            mainCPUMeta[mainCPUID].current_entry = 0;
            mainCPUMeta[mainCPUID].current_size = 0;
            checkerCPUMeta[x].checkpoint_entries = 0;
            checkerCPUMeta[x].checkpoint_cachelines = 0;
            checkerCPUMeta[x].entryIndices = 0;
            checkerCPUMeta[x].startingTick = mainCPUMeta[mainCPUID].startingTickTmp;
            checkerCPUMeta[x].mainStartingTick = curTick();
            checkerCPUMeta[x].checkerStartWakeupTick = 0;
            checkerCPUMeta[x].checkerStartFetchTick = 0;
            checkerCPUMeta[x].checkerStartFetchAccCompleteTick = 0;
            checkerCPUMeta[x].checkerStartCommitTick = 0;
            checkerCPUMeta[x].checkerLastCommitTick = 0;
            if (useHash) {
                checkerCPUMeta[x].initHash();
            }
            // Copy starting loadstorelog sequence number to allocated checker
            checkerCPUMeta[x].startingSeqNum = 
                mainCPUMeta[mainCPUID].startingSeqNum;
            DPRINTF(LoadStoreLogSeqNum,
                    "allocate_little_for_big checker startingSeqNum %d\n",
                    checkerCPUMeta[x].startingSeqNum);
            assert(mainCPUMeta[mainCPUID].startingSeqNum ==
                allCPUMeta[mainCPUID].baseCPU->loadstorelogLastCommitSeqNum +
                    1);

            allCPUMeta[mainCPUID].baseCPU->committedInstrs = 0;
            syscalllogentry::reset_index(mainCPUID);


            allCPUMeta[mainCPUID].baseCPU->havingASleep = false;
            mainCPUMeta[mainCPUID].startingTickTmp = curTick();
            if (slot == 0) {
            copy_main_registers_to_checker(allCPUMeta[mainCPUID].baseCPU,x+NUMBEROFMAINCORES);
            } else {
                mainCPUMeta[mainCPUID].lastChecker = x;
                checkerCPUMeta[x].startingContext = miniContext(mainCPUMeta[mainCPUID].previousThreadContext);
                assert(checkerCPUMeta[x].startingContext.initialized);
                checkerCPUMeta[x].expectedFinalContext.set = false;
                checkerCPUMeta[x].expectedFinalContext.checked = false;
            }
            return true;
        }
        }

    }
    return false;
}



void loadstorelogentry::copy_main_registers_to_checker(BaseCPU* cpu, int checkerCoreId) {
    assert(checkerCoreId < allCPUMeta.size());
    assert(checkerCoreId >= mainCPUMeta.size());
    //input: actual cpuID of checker core
    int cpuID = cpu->getContext(0)->contextId();
    assert(cpuID < mainCPUMeta.size());
    mainCPUMeta.at(cpuID).lastChecker = checkerCoreId-NUMBEROFMAINCORES;
    assert(checkerCoreId-NUMBEROFMAINCORES < NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE);
    assert(mainCPUMeta.at(cpuID).current_segment_to_fill < NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE);
    assert(!checkerCPUMeta[mainCPUMeta.at(cpuID).current_segment_to_fill].segmentFree);
    assert(checkerCPUMeta[mainCPUMeta.at(cpuID).current_segment_to_fill].entryIndices == 0);
    assert(!checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).startingContext.initialized);

    // Set the starting register state for the checker core
#if 0 //TODO: wasSyscall should be filtered through to here if needed
#ifdef ARCHSTATE_ERRORRATE
    if (!(wasSyscall || checkerCPUMeta[mainCPUMeta.at(cpuID).current_segment_to_fill].hasSyscall) compromise_architectural_state(cpuID, &(mainCPUMeta.at(cpuID).previousThreadContext));
#endif
#endif

    m_copyRegs(allCPUMeta[checkerCoreId].baseCPU->getContext(0), mainCPUMeta.at(cpuID).previousThreadContext);
    // Set checker core loadstorelog starting sequence number before checking
    allCPUMeta[checkerCoreId].baseCPU->setLoadstorelogSeqNum(
        checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).startingSeqNum);
    allCPUMeta[checkerCoreId].baseCPU->loadstorelogLastCommitSeqNum =
        checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).startingSeqNum;
    DPRINTF(LoadStoreLogSeqNum,
            "copy_main_registers_to_checker set startingSeqNum %d\n",
            allCPUMeta[checkerCoreId].baseCPU->getLdstlogSeq());
    checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).startingContext = miniContext(mainCPUMeta.at(cpuID).previousThreadContext);
    assert(checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).startingContext.initialized);
    checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).expectedFinalContext.set = false;
    checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).expectedFinalContext.checked = false;
    //std::cout << "Checker " << mainCPUMeta.at(cpuID).lastChecker << " starting with pc " << allCPUMeta[checkerCoreId].baseCPU->getContext(0)->pcState().pc() << std::endl;

    //printf("starting cpu %d at address %ld and timestamp %ld, %ld instructions\n", mainCPUMeta.at(cpuID).current_segment_to_fill,mainCPUMeta.at(cpuID).previousThreadContext.pcState.pc(), checkerCPUMeta[mainCPUMeta.at(cpuID).current_segment_to_fill].timestamps, checkerCPUMeta[mainCPUMeta.at(cpuID).[mainCPUMeta.at(cpuID).current_segment_to_fill].committedInstructions);
    assert(allCPUMeta[checkerCoreId].baseCPU);
    assert(allCPUMeta[checkerCoreId].baseCPU->getContext(0)->status() ==  ThreadContext::Suspended);
    if(checkerCPUMeta[mainCPUMeta.at(cpuID).lastChecker].activeChecker) {
	    std::cout << "Warning: Checker " << mainCPUMeta.at(cpuID).lastChecker << " is active when it shouldn't be." << std::endl;
	    assert(!checkerCPUMeta[mainCPUMeta.at(cpuID).lastChecker].activeChecker);
    }
    
    /*section moved from end-of-taken-checkpoint to start*/

    //Wake, but Sleepguard:

    if (cpu->isSleepGuarded && mainCPUMeta.at(cpuID).timestamp!=1) {
        //printf("early wake\n");
        //dirty hack: calling wakeup here can cause issues when it's from the minorCPU's suspend path, so we actually
        //wake on the deSleepguard path in loadstorelogentry.
        checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).committedInstructions=-1;
        allCPUMeta[checkerCoreId].baseCPU->sleepGuardOn=true;
        DPRINTF(LoadStoreLogSleepGuard,
                "copy_main_registers_to_checker CPU %d sleepGuardOn set for "
                "CPU %d\n",
                cpuID, checkerCoreId);
        checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).startingCheckTick = curTick();

    }
}


int loadstorelogentry::mainCPURollback(BaseCPU* cpu) {
    int toRemove = 0;
#ifdef ROLLBACK_DEBUG
    std::cout << "have drained - store roll\n";
#endif
    int cpuID = cpu->getContext(0)->contextId();

    uint64_t committed = mainCPUMeta.at(cpuID).committed_timestamp;
    uint64_t current = mainCPUMeta.at(cpuID).timestamp;

    if (mainCPUMeta.at(cpuID).mainCoreErroneous) {

        uint64_t num_writebacks = 0;

        for (int segment = mainCPUMeta.at(cpuID).current_segment_to_fill; current != committed; segment = (segment==cpuID*NUMBEROFCHECKERCORESPERCORE)?
            (cpuID+1)*NUMBEROFCHECKERCORESPERCORE - 1 : segment -1) {
            assert(segment < NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE);
            if (current != checkerCPUMeta.at(segment).timestamps) continue;

#ifdef ROLLBACK_DEBUG
            std::cout << current << "\n";
#endif
            if (checkerCPUMeta.at(segment).readyToCommit) {
                checkerCPUMeta.at(segment).readyToCommit = false;
                checkerCPUMeta.at(segment).segmentFree = true;
            }
#if PARAGLIDER

            uint64_t toadd =  checkerCPUMeta.at(segment).checkpoint_cachelines;
#else
            uint64_t toadd =  checkerCPUMeta.at(segment).checkpoint_entries;

#endif
            num_writebacks += toadd;

            toRemove  = allCPUMeta[segment+NUMBEROFMAINCORES].baseCPU->committedInstrs;

            int size_of_segment = logsize;
            for (int x = size_of_segment -1; x>=0; x--) {
                if (checkerCPUMeta.at(segment).entries[x].valid && !checkerCPUMeta.at(segment).entries[x].load) {
                    cpu->writeMem(checkerCPUMeta.at(segment).entries[x].addr, &(checkerCPUMeta.at(segment).entries[x].oldData[0]), checkerCPUMeta.at(segment).entries[x].oldData.size(), checkerCPUMeta.at(segment).entries[x].pc,checkerCPUMeta.at(segment).entries[x].flags /*TODO: proper flags?*/);
#ifdef ROLLBACK_DEBUG
                    std::cout<< "Undoing" << checkerCPUMeta.at(segment).entries[x].addr << " data ";
                    for (auto i: checkerCPUMeta.at(segment).entries[x].oldData)
                        std::cout << i << ' ';

                    std::cout << "\n";
#endif
                }
            }

            current--;
        }

#if LOGERRORS

        errordetection::memoryRecoveryTime.push_back(num_writebacks);
#endif
        errordetection::total_memory_recoveries += num_writebacks;

        errordetection::min_memory_recoveries = std::min( num_writebacks,errordetection::min_memory_recoveries);
        errordetection::min_memory_recoveries = errordetection::min_memory_recoveries==0?  num_writebacks : errordetection::min_memory_recoveries;
        errordetection::max_memory_recoveries = std::max( num_writebacks ,errordetection::max_memory_recoveries);


        mainCPUMeta.at(cpuID).committed_timestamp = mainCPUMeta.at(cpuID).timestamp;

        std::cout << "Resume from end of " << mainCPUMeta.at(cpuID).committed_timestamp << "\n";

        m_copyRegs(cpu->getContext(0), mainCPUMeta.at(cpuID).committed_context);


        mainCPUMeta.at(cpuID).previousThreadContext = mainCPUMeta.at(cpuID).committed_context;

        mainCPUMeta.at(cpuID).mainCoreErroneous = false;




        for (int z=0; z<NUMBEROFMAINCORES; z++) {
            if (allCPUMeta[z].baseCPU->havingASleep && ! mainCPUMeta[z].mainCoreErroneous) loadstorelogentry::allocate_little_for_big(z);
        }
        cpu->drainResume();
        assert(!allCPUMeta[cpuID].baseCPU->havingASleep);

    }
    return toRemove;
}


void loadstorelogentry::updateMainComparisonContexts(BaseCPU* cpu)
{
    int cpuID = cpu->getContext(0)->contextId();
    assert(cpuID < mainCPUMeta.size());
    int checkerID = mainCPUMeta.at(cpuID).lastChecker;
    assert(checkerID < num_checkSlot_per_checker*NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE);
    // Set the expected final register state for the checker core
    mainCPUMeta.at(cpuID).previousThreadContext = m_serialize(cpu->getContext(0));
    // Record starting loadstorelog sequence number for the current segment
    mainCPUMeta.at(cpuID).startingSeqNum = 
        cpu->loadstorelogLastCommitSeqNum + 1;
    DPRINTF(LoadStoreLogSeqNum,
            "updateMainComparisonContexts main startingSeqNum %d\n",
            mainCPUMeta.at(cpuID).startingSeqNum);

    checkerCPUMeta[checkerID].expectedFinalContext = mainCPUMeta.at(cpuID).previousThreadContext;
    checkerCPUMeta[checkerID].expectedFinalContext.set = true;
    if (checkerID < NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE) {
        checkerCPUMeta[checkerID].expectedSetCommittedInsts = allCPUMeta[checkerID + NUMBEROFMAINCORES].baseCPU->committedInstrs;
    } else {
        checkerCPUMeta[checkerID].expectedSetCommittedInsts = 0;
    }

    mainCPUMeta.at(cpuID).waiting_to_checkpoint = false;

    if (!cpu->commitBlocked) {
        cpu->commitBlocked = true;
        cpu->schedule(cpu->finUnblock, cpu->clockEdge(Cycles(8))); //Changed. Was 16 cycles on 4-port reg file. Now 8 cycles on 8-port regfile.
    }
    /*
    int blocked = cpu->getDataPort().blockedEntries(cpuID);
    if(blocked > loadstorelogentry::blocked_lines.size()) loadstorelogentry::blocked_lines.resize(blocked);
    loadstorelogentry::blocked_lines.at(blocked)++;
    */
}

void loadstorelogentry::updateMainContexts(BaseCPU* cpu)
{
    int cpuID = cpu->getContext(0)->contextId();
    assert(cpuID < mainCPUMeta.size());
    // Set the expected final register state for the checker core
    mainCPUMeta.at(cpuID).previousThreadContext = m_serialize(cpu->getContext(0));
    // Record starting loadstorelog sequence number for the current segment
    mainCPUMeta.at(cpuID).startingSeqNum = 
        cpu->loadstorelogLastCommitSeqNum + 1;
    DPRINTF(LoadStoreLogSeqNum,
            "updateMainContexts main startingSeqNum %d\n",
            mainCPUMeta.at(cpuID).startingSeqNum);

    mainCPUMeta.at(cpuID).waiting_to_checkpoint = false;

    if (!cpu->commitBlocked) {
        cpu->commitBlocked = true;
        cpu->schedule(cpu->finUnblock, cpu->clockEdge(Cycles(8)));
    }
    /*
    int blocked = cpu->getDataPort().blockedEntries(cpuID);
    if(blocked > loadstorelogentry::blocked_lines.size()) loadstorelogentry::blocked_lines.resize(blocked);
    loadstorelogentry::blocked_lines.at(blocked)++;
    */
}


void loadstorelogentry::mainDoCheckpoint(BaseCPU* cpu, bool wasSyscall) {

    int cpuID = cpu->getContext(0)->contextId();
    assert(cpuID < mainCPUMeta.size());
    int checkerCoreId = mainCPUMeta.at(cpuID).current_segment_to_fill + NUMBEROFMAINCORES;
    assert(checkerCoreId-NUMBEROFMAINCORES < num_checkSlot_per_checker*NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE);
    assert(cpuID < NUMBEROFMAINCORES);
    mainCPUMeta.at(cpuID).waiting_to_checkpoint = true;
    checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).committedInstructions = cpu->committedInstrs;
    totalCommittedInstructions += cpu->committedInstrs;

    int size_of_segment = logsize;


    for (int x= mainCPUMeta.at(cpuID).current_entry; x< size_of_segment; x++) {
        checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).entries[x].valid = false;
    }

    //std::cout << "segment: " << loadstorelogentry::current_segment_to_fill[cpuID] << " free " << loadstorelogentry::segmentFree[loadstorelogentry::current_segment_to_fill[cpuID]] << std::endl;

    if (mainCPUMeta.at(cpuID).previousThreadContext.initialized && !checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).segmentFree) {

        addToCptLengthHisto(cpu->committedInstrs);
        addToHisto(curTick() - checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).mainStartingTick, 
            cptLenMaxHistoSize, cptLenHistoEntries, cptLenBigBucket);
        cptLenTicks += curTick() - checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).mainStartingTick;
        if (loadstorelogentry::debugFlag) {
            std::cout << "*  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *"
                      << "  Launching checker core "
                      << mainCPUMeta.at(cpuID).lastChecker << " after "
                      << checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).committedInstructions
                      << " committed instructions at time " << mainCPUMeta.at(cpuID).timestamp << std::endl;
        }

	bool was_already_checked = checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).expectedFinalContext.checked;
        updateMainComparisonContexts(cpu);
        addToLengthFromLastLSLHisto(checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).committedInstructions - checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).currentCommittedInstructions);
        loadstorelogentry l; // uninitialised blank field in to signal the end.
        do_write(
            l, cpuID, false, false,
            std::numeric_limits<uint64_t>::
                max()); // Set currentCommittedInstructions to max to make sure
                        // that checker continues to commit new instructions

        //Wakes up here if not done by early-waking mechanism.
        if (checkerCoreId-NUMBEROFMAINCORES < NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE && 
            !checkerCPUMeta[checkerCoreId-NUMBEROFMAINCORES].copyingRegister &&
            allCPUMeta[checkerCoreId].baseCPU->getContext(0)->status() != ThreadContext::Status::Active) {

            if (was_already_checked) {
                printf("core %d ready to commit before checkpoint created.\n", checkerCoreId-NUMBEROFMAINCORES);
                commit_minor_checkpoint(allCPUMeta[checkerCoreId].baseCPU);
            } else {
                //printf("late wake\n");
                assert(checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).startingContext.initialized);
                checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).activeChecker = true;
                allCPUMeta[checkerCoreId].baseCPU->wakeup(0);
                if (checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).checkerStartWakeupTick == 0) {
                    // First time wakeup
                    checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).checkerStartWakeupTick = curTick();
                    cptStartDelayTicks += checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).checkerStartWakeupTick
                        - checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).mainStartingTick;
                    if (checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).checkerDrainDoneTick != 0) {
                        // Not first checkpoint on the checker, was drained before
                        cptCheckerDrainDoneToStartDelayTicks += 
                            checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).checkerStartWakeupTick
                            - checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).checkerDrainDoneTick;
                        // Reset stat after use
                        checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).checkerDrainDoneTick = 0;
                    }
                }
                checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).startingCheckTick = curTick();
                allCPUMeta[checkerCoreId].baseCPU->sleepGuardOn=false;
                DPRINTF(
                    LoadStoreLogSleepGuard,
                    "mainDoCheckpoint CPU %d sleepGuardOn unset for CPU %d\n",
                    cpuID, checkerCoreId);
            }
        }

        // Set the next loadstorelog segment to fill, ie choose the next checker core.

        if (mainCPUMeta.at(cpuID).mainCoreErroneous) {
            //Initiate rollback.
            assert(!allCPUMeta[cpuID].baseCPU->havingASleep);
            if(cpu->drain() == DrainState::Draining) {
            	cpu->havingASleep = true;
            	return;
            }
        }

        if (cpu->sampledCheck()) {
            // Stop checking when one sample has finished
            // Wait till the next sample, do not allocate checker core
            cpu->havingASleep = true; // Only to denote that there's no checker checking
            auto o3cpu = dynamic_cast<o3::CPU *>(cpu);
            if (o3cpu) {
                o3cpu->_isChecked = false;
                o3cpu->_isStored = false;
            }
            // DPRINTF(LoadStoreLogMainContUnchecked,
            //         "CPU %d continueing unchecked\n", cpuID);
            syscalllogentry::reset_index(cpuID);
            return;
        }

        bool avail = loadstorelogentry::allocate_little_for_big(cpuID);
        if (!avail) {
            //suspend if the next queue still needs to be emptied.
            // printf("sleeping main cpu %ld\n", curTick());
            cpu->havingASleep = true;
            if (cpu->canContinueUnchecked()) {
                auto o3cpu = dynamic_cast<o3::CPU *>(cpu);
                if (o3cpu) {
                    o3cpu->_isChecked = false;
                    o3cpu->_isStored = false;
                }
                DPRINTF(LoadStoreLogMainContUnchecked,
                        "CPU %d continueing unchecked\n", cpuID);
                syscalllogentry::reset_index(cpuID);
            }
        } else {
            mainCPUMeta.at(cpuID).startingTickTmp = curTick();
        }

    } else {
        // This should not happen, except for the very first checkpoint.
        assert(mainCPUMeta.at(cpuID).timestamp==1);
        cpu->committedInstrs = 0;
        checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).currentCommittedInstructions = 0;
        checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).checkerLSLStallCycles = 0;
        checkerCPUMeta[cpuID*NUMBEROFCHECKERCORESPERCORE].segmentFree = false;
        checkerCPUMeta[checkerCoreId-NUMBEROFMAINCORES].mainStartingTick = curTick();
        checkerCPUMeta[checkerCoreId-NUMBEROFMAINCORES].initHash();
        mainCPUMeta.at(cpuID).current_entry = 0;
        mainCPUMeta.at(cpuID).current_size = 0;
        std::cout << "\n\nSeed: " << errorinjection::seed << "\n\n" << std::endl;
        // We're outputing the seed here since it only occurs once and the seed is useful for debugging.
        assert(checkerCPUMeta.at(checkerCoreId-NUMBEROFMAINCORES).expectedFinalContext.checked);
        loadstorelogentry::updateMainComparisonContexts(cpu);
        assert(mainCPUMeta.at(cpuID).previousThreadContext.initialized);
        loadstorelogentry::copy_main_registers_to_checker(cpu,checkerCoreId);
        assert(!checkerCPUMeta[mainCPUMeta.at(cpuID).current_segment_to_fill].segmentFree);
    }

}

bool loadstorelogentry::mainShouldBlock(BaseCPU* cpu, bool should_wait, bool should_commit) {
    assert(cpu->getContext(0)->contextId() < mainCPUMeta.size());
    if (!mainCPUMeta[cpu->getContext(0)->contextId()].mainCoreErroneous) {
        if (cpu->commitBlocked) {
            checkpointingCycles++;
            int cpuID = cpu->getContext(0)->contextId();
            if (// Checker was allocated
                !checkerCPUMeta[mainCPUMeta.at(cpuID).current_segment_to_fill]
                    .segmentFree && 
                // The current segment is empty, checkpointing before starting
                cpu->committedInstrs == 0 &&
                // Stat was set before checkpointing finished
                checkerCPUMeta[mainCPUMeta.at(cpuID).current_segment_to_fill]
                    .mainStartingTick < curTick()
            ) {
                // Update stat
                checkerCPUMeta[mainCPUMeta.at(cpu->getContext(0)->contextId())
                    .current_segment_to_fill].mainStartingTick = curTick();
            }
        } else if (cpu->havingASleep &&
            (!cpu->canContinueUnchecked() || cpu->isMain())) {
            noCheckerCycles++;
        } else if (should_wait && !should_commit) {
            blockingWaitCycles++;
        }
    }
    if ((cpu->commitBlocked) ||
        (cpu->havingASleep &&
         (!cpu->canContinueUnchecked() || cpu->isMain())) ||
        (should_wait && !should_commit))
    {
        if (!mainCPUMeta[cpu->getContext(0)->contextId()].mainCoreErroneous) return true;
    }
    return false;
}

}
