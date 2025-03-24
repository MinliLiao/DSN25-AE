/* Authors: Sam Ainsworth
*
*/

#include "mem/cache/loadstorelogentry.hh"

#include "cpu/error_injection.hh"
#include "debug/LoadStoreLogChecker.hh"
#include "debug/LoadStoreLogSeqNum.hh"
#include "debug/LoadStoreLogSleepGuard.hh"
//#include "sim/syscall_emul.hh"
#include <iostream>

#include "sim/syscalllog.hh"

namespace gem5 {


void loadstorelogentry::not_found_sleep(int id) {
              assert(id < NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE);
              checkerCPUMeta.at(id).entryIndices = 0;
              checkerCPUMeta.at(id).activeChecker = false;
              errordetection::detectError(id);
              assert(allCPUMeta[id+NUMBEROFMAINCORES].baseCPU->getContext(0)->status() !=  ThreadContext::Suspended);
              allCPUMeta[id+NUMBEROFMAINCORES].baseCPU->drain();
}


void loadstorelogentry::commit_minor_checkpoint(BaseCPU* cpu) {

    int checkerID = cpu->getContext(0)->contextId()-NUMBEROFMAINCORES;
    int mainCPUID = checkerID/NUMBEROFCHECKERCORESPERCORE;
    assert(checkerID < NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE);
    assert(checkerID >= 0);
    assert(mainCPUID < NUMBEROFMAINCORES);

    if ((checkerID % NUMBEROFCHECKERCORESPERCORE) == 0 && !mainCPUMeta.at(mainCPUID).ready) {
        printf("getting cpu %d ready\n", mainCPUID);
        mainCPUMeta.at(mainCPUID).ready = true;
    } else {

        if (checkerCPUMeta.at(checkerID).segmentFree) {
            printf("suspending not ready context %d\n",checkerID);
        }

        if (checkerCPUMeta.at(checkerID).expectedFinalContext.set || checkerCPUMeta.at(checkerID).segmentFree)checkerCPUMeta.at(checkerID).readyToCommit= true;
        else printf("not yet ready to commit %ld on %d\n", checkerCPUMeta.at(checkerID).timestamps, checkerID);
        if (checkerCPUMeta.at(checkerID).expectedFinalContext.set) {
            checkDelayCommittedInstructions += (checkerCPUMeta.at(checkerID).committedInstructions - checkerCPUMeta.at(checkerID).expectedSetCommittedInsts);
            assert(checkerCPUMeta.at(checkerID).checkerStartWakeupTick > 0 && checkerCPUMeta.at(checkerID).checkerStartWakeupTick < curTick());
            addToHisto(cpu->ticksToCycles(curTick() - checkerCPUMeta.at(checkerID).checkerStartWakeupTick), 
                checkerCptLenMaxHistoSize, checkerCptLenHistoEntries, checkerCptLenBigBucket);
        }
        //printf("ready to commit %ld on %d\n", checkerCPUMeta.at(checkerID).timestamps, checkerID);
        checkerCPUMeta.at(checkerID).aboutToValidate = false;
        checkerCPUMeta.at(checkerID).startingContext.initialized = false;
        checkerCPUMeta.at(checkerID).dataAddressOffset= 0;
        checkerCPUMeta.at(checkerID).midopEntryIndex= 0;
        syscalllogentry::reset_index(checkerID+NUMBEROFMAINCORES);
        if (errorinjection::hasInjectedError[checkerID]) {
            errorinjection::hasInjectedError[checkerID] = false;
            errorinjection::undetectedErrors++;
        }
        if (errorinjection::unchangedInjectedError[checkerID]) {
            errorinjection::unchangedInjectedError[checkerID] = false;
            errorinjection::cptOnlyUnchangedInjections++;
        }

    }
    cpu->getContext(0)->suspend();

    bool finishedThisRound = false;

    do {
        finishedThisRound = false;
        uint64_t minCurrentTimestamp = 0;
        for (int y=0; y<NUMBEROFCHECKERCORESPERCORE; y++) {
            int x = y + NUMBEROFCHECKERCORESPERCORE * mainCPUID;
            if (minCurrentTimestamp == 0 && !checkerCPUMeta[x].segmentFree) {
                minCurrentTimestamp = checkerCPUMeta[x].timestamps;
            }
            if (minCurrentTimestamp > 0 && !checkerCPUMeta[x].segmentFree && checkerCPUMeta[x].timestamps < minCurrentTimestamp) {
                minCurrentTimestamp = checkerCPUMeta[x].timestamps;
            }
        }
        if (mainCPUMeta.at(mainCPUID).committed_timestamp + 1 < minCurrentTimestamp) {
            std::cout << "All current checker running timestamp >= " << minCurrentTimestamp << ", committed timestamp " << mainCPUMeta.at(mainCPUID).committed_timestamp << std::endl;
            std::cout << "earlyCommittedTimestamps:" << std::endl;
            for (auto it = mainCPUMeta.at(mainCPUID).earlyCommittedTimestamps.begin(); it != mainCPUMeta.at(mainCPUID).earlyCommittedTimestamps.end(); ++it) {
                std::cout << it->first << " ";
            }
            std::cout << std::endl;
        }
        for (int y=0; y<NUMBEROFCHECKERCORESPERCORE; y++) {
            int x = y + NUMBEROFCHECKERCORESPERCORE * mainCPUID;
            assert(x < NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE);
            if(checkerCPUMeta.size() != num_checkSlot_per_checker*NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE) std::cout << checkerCPUMeta.size()<< " "  <<  NUMBEROFMAINCORES << " " << NUMBEROFCHECKERCORESPERCORE << "\n";
            assert(checkerCPUMeta.size() == num_checkSlot_per_checker*NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE);
            if (checkerCPUMeta[x].readyToCommit && 
            (checkerCPUMeta[x].timestamps<=mainCPUMeta.at(mainCPUID).committed_timestamp+1 
            || AUTOCOMMIT || minorCommitBypass)) {

                if (checkerCPUMeta[x].erroneous  && checkerCPUMeta[x].timestamps > mainCPUMeta.at(mainCPUID).committed_timestamp && !checkerCPUMeta[x].hasSyscall) {
                    if(mainCPUMeta.at(mainCPUID).mainCoreErroneous) {
                    	checkerCPUMeta[x].readyToCommit = false;
                    	checkerCPUMeta[x].segmentFree = true;
                    	return;
                    }
                    std::cout << curTick() << " Trigger rollback at " << checkerCPUMeta[x].timestamps  << " from " << mainCPUMeta.at(mainCPUID).timestamp << std::endl;
                    errordetection::detectErrorCommit(x);
                    mainCPUMeta.at(mainCPUID).mainCoreErroneous = true;
                    checkerCPUMeta[x].readyToCommit = false;
                    checkerCPUMeta[x].segmentFree = true;
                    return;
                }

                if ((checkerCPUMeta[x].timestamps&511)==1 && (checkerCPUMeta[x].timestamps>1||x==0)) {

                    printf("\ncommitting %ld for cpu %d on %d\n\n", checkerCPUMeta[x].timestamps, mainCPUID, x);
                }

                finishedThisRound=true;
                checkedCommittedInstructions += checkerCPUMeta[x].committedInstructions;
                if (mainCPUMeta.at(mainCPUID).committed_timestamp+1 < checkerCPUMeta[x].timestamps) {
                    assert(mainCPUMeta.at(mainCPUID).earlyCommittedTimestamps.find(checkerCPUMeta[x].timestamps) == mainCPUMeta.at(mainCPUID).earlyCommittedTimestamps.end());
                    mainCPUMeta.at(mainCPUID).earlyCommittedTimestamps[checkerCPUMeta[x].timestamps] = checkerCPUMeta.at(x).expectedFinalContext;
                } else if (mainCPUMeta.at(mainCPUID).committed_timestamp < checkerCPUMeta[x].timestamps) {
#if PARADVFS
#if LOGERRORS
                    if (mainCPUMeta.at(mainCPUID).voltage >= mainCPUMeta.at(mainCPUID).highestRecentVoltageError && (mainCPUMeta.at(mainCPUID).voltage- (AIMDDIFF) )<=    mainCPUMeta.at(mainCPUID).highestRecentVoltageError) {
                        errordetection::voltageSwitchPoints.push_back(mainCPUMeta.at(mainCPUID).highestRecentVoltageError);
                        errordetection::voltageSwitchTime.push_back(curTick());
                        std::cout << "\nVoltage Switch: " << curTick();

                    }
#endif
                    mainCPUMeta.at(mainCPUID).voltage -= (mainCPUMeta.at(mainCPUID).voltage < mainCPUMeta.at(mainCPUID).highestRecentVoltageError)? AIMDDIFF / 8.0 : AIMDDIFF;
                    errordetection::voltage_reset[mainCPUID] = true;
#endif
                    mainCPUMeta.at(mainCPUID).committed_timestamp = checkerCPUMeta.at(x).timestamps;
                    mainCPUMeta.at(mainCPUID).committed_context = checkerCPUMeta.at(x).expectedFinalContext;

                    while(mainCPUMeta.at(mainCPUID).earlyCommittedTimestamps.find(mainCPUMeta.at(mainCPUID).committed_timestamp+1) != mainCPUMeta.at(mainCPUID).earlyCommittedTimestamps.end()) {
                        mainCPUMeta.at(mainCPUID).committed_context = mainCPUMeta.at(mainCPUID).earlyCommittedTimestamps.at(mainCPUMeta.at(mainCPUID).committed_timestamp+1);
                        mainCPUMeta.at(mainCPUID).committed_timestamp = mainCPUMeta.at(mainCPUID).committed_timestamp+1;
                        mainCPUMeta.at(mainCPUID).earlyCommittedTimestamps.erase(mainCPUMeta.at(mainCPUID).committed_timestamp);
                    }
                }
                assert(x < NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE);
                checkerCPUMeta[x].segmentFree = true;
                checkerCPUMeta[x].readyToCommit = false;


                // Checkpoints cached in the extra checkerCPUMeta should be ordered leaving no gap
                for (int slot = 1; slot < num_checkSlot_per_checker; slot++) {
                    int prev_id = x + (slot-1)*NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE;
                    int this_id = x + slot*NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE;
                    assert(slot == 1 || checkerCPUMeta[this_id].segmentFree || (!checkerCPUMeta[this_id].segmentFree && !checkerCPUMeta[prev_id].segmentFree));
                }
                // Keep stats for later
                uint64_t tmp_drainDoneTick = checkerCPUMeta[x].checkerDrainDoneTick;
                // If there is a checkpoint cached in the extra checkerCPUMeta, pull it over to the core
                for (int slot = 1; slot < num_checkSlot_per_checker; slot++) {
                    int prev_id = x + (slot-1)*NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE;
                    int this_id = x + slot*NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE;
                    if (!checkerCPUMeta[this_id].segmentFree) {
                        assert(checkerCPUMeta[prev_id].segmentFree);
                        CheckerCPUMeta tmp = checkerCPUMeta[prev_id];
                        checkerCPUMeta[prev_id] = checkerCPUMeta[this_id];
                        checkerCPUMeta[this_id] = tmp;
                        syscalllogentry::move_segment(this_id, prev_id);
                        // Make sure that the main core still track the correct checkerCPUMeta
                        if (mainCPUMeta[mainCPUID].lastChecker == this_id) {
                            mainCPUMeta[mainCPUID].lastChecker = prev_id;
                        }
                        if (mainCPUMeta[mainCPUID].current_segment_to_fill == this_id) {
                            mainCPUMeta[mainCPUID].current_segment_to_fill = prev_id;
                            last_macro_addrs[prev_id] = last_macro_addrs[this_id];
                        }
                    }
                }
                // If a cached checkpoint have been pulled to the core, prepare for checking
                if (!checkerCPUMeta[x].segmentFree) {
                    // Update the drainDoneTick according to the previous checkpoint
                    checkerCPUMeta[x].checkerDrainDoneTick = tmp_drainDoneTick;
                    // Set the starting context
                    assert(checkerCPUMeta[x].startingContext.initialized);
                    assert(allCPUMeta[x+NUMBEROFMAINCORES].baseCPU);
                    assert(allCPUMeta[x+NUMBEROFMAINCORES].baseCPU->getContext(0)->status() ==  ThreadContext::Suspended);
                    assert(!checkerCPUMeta[x].activeChecker);
                    m_copyRegs(allCPUMeta[x+NUMBEROFMAINCORES].baseCPU->getContext(0), checkerCPUMeta[x].startingContext);
                    // Set checker core loadstorelog starting sequence number before checking
                    allCPUMeta[x+NUMBEROFMAINCORES].baseCPU->setLoadstorelogSeqNum(
                        checkerCPUMeta[x].startingSeqNum);
                    allCPUMeta[x+NUMBEROFMAINCORES].baseCPU->loadstorelogLastCommitSeqNum =
                        checkerCPUMeta[x].startingSeqNum;
                    DPRINTF(LoadStoreLogSeqNum,
                            "copy_main_registers_to_checker set startingSeqNum %d\n",
                            allCPUMeta[x+NUMBEROFMAINCORES].baseCPU->getLdstlogSeq());
                    allCPUMeta[x+NUMBEROFMAINCORES].baseCPU->committedInstrs = 0;
                    if (allCPUMeta[mainCPUID].baseCPU->isSleepGuarded) {
                        assert(checkerCPUMeta[x].timestamps!=1);
                        //printf("early wake\n");
                        //dirty hack: calling wakeup here can cause issues when it's from the minorCPU's suspend path, so we actually
                        //wake on the deSleepguard path in loadstorelogentry.
                        if (!checkerCPUMeta[x].expectedFinalContext.set) {
                            // Only set this if it hasn't been set yet
                            checkerCPUMeta[x].committedInstructions=-1;
                        }
                        if (!checkerCPUMeta[x].expectedFinalContext.set &&
                            checkerCPUMeta[x].checkpoint_entries == 0) { // Only sleep if needed
                            allCPUMeta[x+NUMBEROFMAINCORES].baseCPU->sleepGuardOn=true;
                            DPRINTF(LoadStoreLogSleepGuard,
                                    "commit_minor_checkpoint CPU %d sleepGuardOn set for "
                                    "CPU %d\n",
                                    mainCPUID, x+NUMBEROFMAINCORES);
                        }
                        checkerCPUMeta[x].startingCheckTick = curTick();
                    }
                    // Wake up the core if the final context was set or the LSL is not empty
                    if (checkerCPUMeta[x].expectedFinalContext.set ||
                        checkerCPUMeta[x].checkpoint_entries > 0) {
                        // Attempt to avoid suspending and waking up the core
                        // on the same tick by scheduling wakeup for later
                        checkerCPUMeta[x].copyingRegister = true;
                        allCPUMeta[x+NUMBEROFMAINCORES].baseCPU->schedule(
                            allCPUMeta[x+NUMBEROFMAINCORES].baseCPU
                                ->checkerWakeupEvent, 
                            allCPUMeta[x+NUMBEROFMAINCORES].baseCPU
                                ->clockEdge(Cycles(1)));
                    }
                }
            }
        }
    } while (finishedThisRound);

    for (int z = 0; z < NUMBEROFMAINCORES; z++) {
        if (!((allCPUMeta[z].baseCPU->canContinueUnchecked() || allCPUMeta[z].baseCPU->sampledCheck()) &&
              !allCPUMeta[z].baseCPU->isMain()) &&
            allCPUMeta[z].baseCPU->havingASleep &&
            !mainCPUMeta[z].mainCoreErroneous)
            allocate_little_for_big(z);
    }

}

void loadstorelogentry::checkerWakeup(int x) {
    checkerCPUMeta[x].copyingRegister = false;
    checkerCPUMeta[x].activeChecker = true;
    assert(allCPUMeta[x+NUMBEROFMAINCORES].baseCPU->getContext(0)->status() ==  ThreadContext::Suspended);
    allCPUMeta[x+NUMBEROFMAINCORES].baseCPU->wakeup(0);
    assert(checkerCPUMeta[x].checkerStartWakeupTick == 0); // First time wakeup
    checkerCPUMeta[x].checkerStartWakeupTick = curTick();
    cptStartDelayTicks += checkerCPUMeta[x].checkerStartWakeupTick
        - checkerCPUMeta[x].mainStartingTick;
    if (checkerCPUMeta[x].checkerDrainDoneTick != 0) {
        // Not first checkpoint on the checker, was drained before
        cptCheckerDrainDoneToStartDelayTicks += 
            checkerCPUMeta[x].checkerStartWakeupTick
            - checkerCPUMeta[x].checkerDrainDoneTick;
        // Reset stat after use
        checkerCPUMeta[x].checkerDrainDoneTick = 0;
    }
    checkerCPUMeta[x].startingCheckTick = curTick();
    assert(!allCPUMeta[x+NUMBEROFMAINCORES].baseCPU->sleepGuardOn);
}

bool loadstorelogentry::checkerCheckIfShouldSleep(BaseCPU* cpu) {
    int id = cpu->getContext(0)->contextId()-NUMBEROFMAINCORES;
    assert(id < NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE);
    if (checkerCPUMeta.at(id).aboutToValidate) {

        if (checkerCPUMeta.at(id).activeChecker) {

            //std::cout << "Checker " << id<<(id>=10?"":" ") << " validating after " << cpu->committedInstrs << " committed instructions at time " <<  loadstorelogentry::mainCPUMeta[id/NUMBEROFCHECKERCORESPERCORE].timestamp << "... ";
            DPRINTF(LoadStoreLogChecker,
                    "checkerCheckIfShouldSleep checker %d "
                    "committedInstrs(cpu/log): %ld/%ld\n",
                    id, cpu->committedInstrs,
                    checkerCPUMeta[id].committedInstructions);
            if (cpu->committedInstrs==1 && checkerCPUMeta.at(id).committedInstructions>1) {
                std::cout << "Initialization: skipping " << id << " at time " << mainCPUMeta[id/NUMBEROFCHECKERCORESPERCORE].timestamp << std::endl;
                checkerCPUMeta.at(id).expectedFinalContext.checked = true;
                if (useHash) {
                    checkerCPUMeta.at(id).initHash();
                }
            } else {
                if (useHash) {
                    loadstorelogentry l;
                    checkerCPUMeta.at(id).calcCheckedHash(l);
                    std::cerr << "checker " << id << "finishing timestamp " << checkerCPUMeta[id].timestamps << std::endl;
                    if (checkerCPUMeta.at(id).expectedFinalContext.set && m_identical(cpu->getContext(0), checkerCPUMeta.at(id).expectedFinalContext)) {
                        assert(checkerCPUMeta.at(id).expectedHash == checkerCPUMeta.at(id).hash);
                    } else {
                        assert(checkerCPUMeta.at(id).expectedHash != checkerCPUMeta.at(id).hash);
                    }
                }
                if (checkerCPUMeta.at(id).expectedFinalContext.set && m_identical(cpu->getContext(0), checkerCPUMeta.at(id).expectedFinalContext)) {
                    checkerCPUMeta.at(id).expectedFinalContext.checked = true;
                    //std::cout << "Same architectural state for checker " << id << std::endl;
                    errordetection::numberOfCorrectCheckpoints++;
                    DPRINTF(LoadStoreLogChecker,
                            "checkerCheckIfShouldSleep matching architectural "
                            "state for checker %d at time %d\n",
                            id,
                            mainCPUMeta[id / NUMBEROFCHECKERCORESPERCORE]
                                .timestamp);
                } else {
                    errordetection::numberOfDetectedErroneousArchStates++;
                    errordetection::detectError(id);
                    //std::cout << "Error for checker " << id << " at time " << loadstorelogentry::mainCPUMeta[id/NUMBEROFCHECKERCORESPERCORE].timestamp << " insts" << cpu->committedInstrs << std::endl;
                }
            }

            // std::cout << "------------------------------------------------ (" << id << ") " << loadstorelogentry::timestamp[id/NUMBEROFCHECKERCORESPERCORE] << std::endl;

            // assert(!loadstorelogentry::debugFlag);
            checkerCPUMeta.at(id).activeChecker = false;
        }
        //suspend CPU, maybe reawaken O3.
        checkerCPUMeta.at(id).entryIndices = 0;
        checkerCPUMeta.at(id).interrupted = false;
        allCPUMeta[id+NUMBEROFMAINCORES].baseCPU->drain();

        return true;
    }
    return false;
}

void
loadstorelogentry::recStartCommitStats(BaseCPU* cpu) {
    int checkerID = cpu->getContext(0)->contextId()-NUMBEROFMAINCORES;
    int mainCPUID = checkerID/NUMBEROFCHECKERCORESPERCORE;
    assert(checkerID < NUMBEROFMAINCORES*NUMBEROFCHECKERCORESPERCORE);
    assert(checkerID >= 0);
    assert(mainCPUID < NUMBEROFMAINCORES);
    if (!checkerCPUMeta.at(checkerID).segmentFree) { // Checker core checking
        // Should happen only once
        assert(checkerCPUMeta.at(checkerID).checkerStartCommitTick == 0);
        // Should have started
        assert(checkerCPUMeta.at(checkerID).checkerStartWakeupTick > 0);
        // Should have fetched
        assert(checkerCPUMeta.at(checkerID).checkerStartFetchTick > 0);
        assert(checkerCPUMeta.at(checkerID).checkerStartFetchTick
            >= checkerCPUMeta.at(checkerID).checkerStartWakeupTick);
        checkerCPUMeta.at(checkerID).checkerStartCommitTick = curTick();
        assert(checkerCPUMeta.at(checkerID).checkerStartCommitTick 
            >= checkerCPUMeta.at(checkerID).checkerStartFetchTick);
        cptCheckerStartToCommitDelayTicks += 
            checkerCPUMeta.at(checkerID).checkerStartCommitTick 
            - checkerCPUMeta.at(checkerID).checkerStartWakeupTick;
        cptCheckerFirstFetchToCommitDelayTicks +=
            checkerCPUMeta.at(checkerID).checkerStartCommitTick 
            - checkerCPUMeta.at(checkerID).checkerStartFetchTick;
        // Transform tick to cycles and collect histogram
        addToHisto(
            cpu->ticksToCycles(
                checkerCPUMeta.at(checkerID).checkerStartCommitTick 
                - checkerCPUMeta.at(checkerID).checkerStartFetchTick), 
            cptFirstFetchToCommitDelayMaxHistoSize, 
            cptFirstFetchToCommitDelayHistoEntries, 
            cptFirstFetchToCommitDelayBigBucket);    
        // If main core has already set the final context
        if (checkerCPUMeta.at(checkerID).expectedFinalContext.set) {
            checkStartDelayInstructions += checkerCPUMeta.at(checkerID)
                                               .committedInstructions;
        } else { // Main core is still working on this segment
            assert(mainCPUMeta[mainCPUID].current_segment_to_fill
                   == checkerID);
            checkStartDelayInstructions += allCPUMeta[mainCPUID]
                                               .baseCPU->committedInstrs;
        }
    }
}

bool
loadstorelogentry::isMainReady(int checkerCPUID)
{
    return mainCPUMeta.at(getMainID(checkerCPUID)).ready;
}

int
loadstorelogentry::getMainID(int checkerCPUID)
{
    assert(checkerCPUID < allCPUMeta.size());
    assert(checkerCPUID >= mainCPUMeta.size());
    return (checkerCPUID - NUMBEROFMAINCORES) / NUMBEROFCHECKERCORESPERCORE;
}
}
