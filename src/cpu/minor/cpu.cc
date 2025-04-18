/*
 * Copyright (c) 2012-2014, 2017 ARM Limited
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

#include "cpu/minor/cpu.hh"

#include "cpu/minor/dyn_inst.hh"
#include "cpu/minor/fetch1.hh"
#include "cpu/minor/pipeline.hh"
#include "debug/Drain.hh"
#include "debug/MinorCPU.hh"
#include "debug/Quiesce.hh"
#include "mem/cache/loadstorelogentry.hh"

namespace gem5
{

MinorCPU::MinorCPU(const BaseMinorCPUParams &params) :
    BaseCPU(params),
    threadPolicy(params.threadPolicy), isDraining(false),
    stats(this),
    drainEvent([this]{ drain(); }, "MinorCPU drain")
{
    /* This is only written for one thread at the moment */
    minor::MinorThread *thread;

    for (ThreadID i = 0; i < numThreads; i++) {
        if (FullSystem) {
            thread = new minor::MinorThread(this, i, params.system,
                    params.mmu, params.isa[i], params.decoder[i]);
            thread->setStatus(ThreadContext::Halted);
        } else {
            thread = new minor::MinorThread(this, i, params.system,
                    params.workload[i], params.mmu,
                    params.isa[i], params.decoder[i]);
        }

        threads.push_back(thread);
        ThreadContext *tc = thread->getTC();
        threadContexts.push_back(tc);
    }


    if (params.checker) {
        fatal("The Minor model doesn't support checking (yet)\n");
    }

    pipeline = new minor::Pipeline(*this, params);
    activityRecorder = pipeline->getActivityRecorder();

    fetchEventWrapper = NULL;
    fakeReqIssueEventWrapper = NULL;
}

MinorCPU::~MinorCPU()
{
    delete pipeline;

    if (fetchEventWrapper != NULL)
        delete fetchEventWrapper;

    for (ThreadID thread_id = 0; thread_id < threads.size(); thread_id++) {
        delete threads[thread_id];
    }
}

void
MinorCPU::init()
{
    BaseCPU::init();

    if (!params().switched_out && system->getMemoryMode() != enums::timing) {
        fatal("The Minor CPU requires the memory system to be in "
            "'timing' mode.\n");
    }
}

/** Stats interface from SimObject (by way of BaseCPU) */
void
MinorCPU::regStats()
{
    BaseCPU::regStats();
    pipeline->regStats();
}

void
MinorCPU::serializeThread(CheckpointOut &cp, ThreadID thread_id) const
{
    threads[thread_id]->serialize(cp);
}

void
MinorCPU::unserializeThread(CheckpointIn &cp, ThreadID thread_id)
{
    threads[thread_id]->unserialize(cp);
}

void
MinorCPU::serialize(CheckpointOut &cp) const
{
    pipeline->serialize(cp);
    BaseCPU::serialize(cp);
}

void
MinorCPU::unserialize(CheckpointIn &cp)
{
    pipeline->unserialize(cp);
    BaseCPU::unserialize(cp);
}

void
MinorCPU::wakeup(ThreadID tid)
{
    DPRINTF(Drain, "[tid:%d] MinorCPU wakeup\n", tid);
    assert(tid < numThreads);

    if (threads[tid]->status() == ThreadContext::Suspended) {
        threads[tid]->activate();
        drainResume();
    }
}

void
MinorCPU::startup()
{
    DPRINTF(MinorCPU, "MinorCPU startup\n");

    BaseCPU::startup();

    if (this->isChecker()) {
        drain();
    } else {
        for (ThreadID tid = 0; tid < numThreads; tid++)
            pipeline->wakeupFetch(tid);
    }
}

DrainState
MinorCPU::drain()
{
    if (this->isChecker()) {
        isDraining = true;
    }
    // Deschedule any power gating event (if any)
    deschedulePowerGatingEvent();

    if (switchedOut()) {
        DPRINTF(Drain, "Minor CPU switched out, draining not needed.\n");
        return DrainState::Drained;
    }

    DPRINTF(Drain, "MinorCPU drain\n");

    /* Need to suspend all threads and wait for Execute to idle.
     * Tell Fetch1 not to fetch */
    if (pipeline->drain()) {
        DPRINTF(Drain, "MinorCPU drained\n");
        if (this->isChecker()) {
            signalDrainDone();
        }
        return DrainState::Drained;
    } else {
        DPRINTF(Drain, "MinorCPU not finished draining\n");
        return DrainState::Draining;
    }
}

void
MinorCPU::signalDrainDone()
{
    DPRINTF(Drain, "MinorCPU drain done\n");
    Drainable::signalDrainDone();
    if (this->isChecker()) {
        isDraining = false;
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

    // std::cout << "Drain done for checker " << checkerID << std::endl;
}

void
MinorCPU::drainResume()
{
    /* When taking over from another cpu make sure lastStopped
     * is reset since it might have not been defined previously
     * and might lead to a stats corruption */
    pipeline->resetLastStopped();

    if (switchedOut()) {
        DPRINTF(Drain, "drainResume while switched out.  Ignoring\n");
        return;
    }

    DPRINTF(Drain, "MinorCPU drainResume\n");

    if (!system->isTimingMode()) {
        fatal("The Minor CPU requires the memory system to be in "
            "'timing' mode.\n");
    }

    for (ThreadID tid = 0; tid < numThreads; tid++){
        wakeup(tid);
    }

    if (isChecker() &&
        !loadstorelogentry::isMainReady(getContext(0)->contextId())) {
        schedule(drainEvent, clockEdge(Cycles(0)));
        // Reschedule any power gating event (if any)
        schedulePowerGatingEvent();
        return;
    }
    pipeline->drainResume();

    // Reschedule any power gating event (if any)
    schedulePowerGatingEvent();
}

void
MinorCPU::memWriteback()
{
    DPRINTF(Drain, "MinorCPU memWriteback\n");
}

void
MinorCPU::switchOut()
{
    DPRINTF(MinorCPU, "MinorCPU switchOut\n");

    assert(!switchedOut());
    BaseCPU::switchOut();

    /* Check that the CPU is drained? */
    activityRecorder->reset();
}

void
MinorCPU::takeOverFrom(BaseCPU *old_cpu)
{
    DPRINTF(MinorCPU, "MinorCPU takeOverFrom\n");

    BaseCPU::takeOverFrom(old_cpu);
}

void
MinorCPU::activateContext(ThreadID thread_id)
{
    DPRINTF(MinorCPU, "ActivateContext thread: %d\n", thread_id);

    /* Do some cycle accounting.  lastStopped is reset to stop the
     *  wakeup call on the pipeline from adding the quiesce period
     *  to BaseCPU::numCycles */
    stats.quiesceCycles += pipeline->cyclesSinceLastStopped();
    pipeline->resetLastStopped();

    /* Wake up the thread, wakeup the pipeline tick */
    threads[thread_id]->activate();
    wakeupOnEvent(minor::Pipeline::CPUStageId);

    if (!threads[thread_id]->getUseForClone())//the thread is not cloned
    {
        pipeline->wakeupFetch(thread_id);
    } else { //the thread from clone
        if (fetchEventWrapper != NULL)
            delete fetchEventWrapper;
        fetchEventWrapper = new EventFunctionWrapper([this, thread_id]
                  { pipeline->wakeupFetch(thread_id); }, "wakeupFetch");
        schedule(*fetchEventWrapper, clockEdge(Cycles(0)));
    }

    BaseCPU::activateContext(thread_id);
}

bool
MinorCPU::isFakeReqIssueEventEnabled(ThreadID thread_id)
{
    if (fakeReqIssueEventWrapper != NULL)
        return true;
    else
	return false;
}

void
MinorCPU::activateFakeReqIssueEvent(ThreadID thread_id)
{
    assert(fakeReqIssueEventWrapper == NULL);
    fakeReqIssueEventWrapper = new EventFunctionWrapper([this, thread_id]
                  { pipeline->fakeReqIssue(thread_id); }, "fakeReqIssue");
    schedule(*fakeReqIssueEventWrapper, nextCycle());
}

void
MinorCPU::rescheduleFakeReqIssueEvent()
{
    assert(fakeReqIssueEventWrapper != NULL);
    assert(!fakeReqIssueEventWrapper->scheduled());
    schedule(*fakeReqIssueEventWrapper, nextCycle());
}

void
MinorCPU::suspendFakeReqIssueEvent(ThreadID thread_id)
{
    assert(fakeReqIssueEventWrapper != NULL);
    // deschedule event
    if (fakeReqIssueEventWrapper->scheduled())
        deschedule(fakeReqIssueEventWrapper);

    // delete event
    delete fakeReqIssueEventWrapper;
    fakeReqIssueEventWrapper = NULL;
}

void
MinorCPU::suspendContext(ThreadID thread_id)
{
    DPRINTF(MinorCPU, "SuspendContext %d\n", thread_id);

    threads[thread_id]->suspend();

    BaseCPU::suspendContext(thread_id);
}

void
MinorCPU::wakeupOnEvent(unsigned int stage_id)
{
    DPRINTF(Quiesce, "Event wakeup from stage %d\n", stage_id);

    /* Mark that some activity has taken place and start the pipeline */
    activityRecorder->activateStage(stage_id);
    pipeline->start();
}

Port &
MinorCPU::getInstPort()
{
    return pipeline->getInstPort();
}

Port &
MinorCPU::getDataPort()
{
    return pipeline->getDataPort();
}

Counter
MinorCPU::totalInsts() const
{
    Counter ret = 0;

    for (auto i = threads.begin(); i != threads.end(); i ++)
        ret += (*i)->numInst;

    return ret;
}

Counter
MinorCPU::totalOps() const
{
    Counter ret = 0;

    for (auto i = threads.begin(); i != threads.end(); i ++)
        ret += (*i)->numOp;

    return ret;
}

void 
MinorCPU::setLoadstorelogSeqNum(int64_t seqNum)
{
    assert(seqNum > 0); // Was initialized to 1 and should only increment
    loadstorelogSeqNum = seqNum;
    pipeline->resetDiscardLoadstore();
}

} // namespace gem5
