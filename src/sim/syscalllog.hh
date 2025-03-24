
#ifndef __SIM_SYSCALL_LOG_HH__
#define __SIM_SYSCALL_LOG_HH__

///
/// @file syscalllog.hh
///
/// This file defines a small log to forward syscall results from the main cores
/// to the checker cores.



#include <vector>

#include "mem/cache/loadstorelogentry.hh"
#include "sim/syscall_return.hh"

#define SIZEOFSYSCALLSEGMENT ((TIMEOUT/10)+20)
#define SYSCALLLOGSIZE (loadstorelogentry::num_checkSlot_per_checker*NUMBEROFCHECKERCORESPERCORE*NUMBEROFMAINCORES*SIZEOFSYSCALLSEGMENT)

/* The log roughly follows the convention set in loadstorelogentry: do_write
 * stores the entry and do_read retrieves it.
 * update_context is used to store the architectural state after the actual
 * syscall. It is called in arch/arm/faults.cc */
namespace gem5 {
class syscalllogentry
{

    private:
                SyscallReturn result;
                Addr instAddr;
                miniContext stateBefore;
                miniContext stateAfter;
                bool initialized;
                bool updated;
                bool unread;

                syscalllogentry(SyscallReturn r, uint64_t inst, miniContext before)
                : result(r), instAddr(inst), stateBefore(before),
                          stateAfter(miniContext()), initialized(true), updated(false), unread(true)
            {}

                static std::vector<syscalllogentry> entries;//[SYSCALLLOGSIZE];
                static std::vector<int> current_entry;//[NUMBEROFMAINCORES];
                static std::vector<int> entryIndices;//[NUMBEROFCHECKERCORESPERCORE*NUMBEROFMAINCORES];
                static std::vector<int> maxIndices;//[NUMBEROFCHECKERCORESPERCORE*NUMBEROFMAINCORES];

        public:

                syscalllogentry()
                : result(0), instAddr(0), stateBefore(miniContext()),
                          stateAfter(miniContext()), initialized(false), updated(false), unread(false)
            {}
            
            static void resizeLogs(int mains, int checkers) {
               entries.resize(SYSCALLLOGSIZE,syscalllogentry());
               current_entry.resize(mains,0);
               entryIndices.resize(loadstorelogentry::num_checkSlot_per_checker*checkers*mains,0);
               maxIndices.resize(loadstorelogentry::num_checkSlot_per_checker*checkers*mains,0);
            }

                static bool do_write(SyscallReturn r, int cpuID, Addr instAddr)
                {
                        assert(cpuID < loadstorelogentry::mainCPUMeta.size());
                        // Bypass write when there is no checker or the main CPU is not checked nor stored
                        if (NUMBEROFCHECKERCORESPERCORE == 0 || !loadstorelogentry::allCPUMeta[cpuID].baseCPU->isMain()) {
                                return false;
                        }
                        int pos = loadstorelogentry::mainCPUMeta[cpuID].current_segment_to_fill*SIZEOFSYSCALLSEGMENT + current_entry[cpuID];
                        // std::cout << "Syscalllogentry write by main core " << cpuID << " at position " << pos << std::endl;
                        assert(pos < SYSCALLLOGSIZE);
                        entries.at(pos) = syscalllogentry(r, instAddr, m_serialize(loadstorelogentry::allCPUMeta[cpuID].baseCPU->getContext(0)));
                        return (++current_entry[cpuID] >= SIZEOFSYSCALLSEGMENT);
                }

                static void update_context(int cpuID) {
                        assert(cpuID < loadstorelogentry::mainCPUMeta.size());
                        // Bypass update when there is no checker or the main CPU is not checked nor stored
                        if (NUMBEROFCHECKERCORESPERCORE == 0 || !loadstorelogentry::allCPUMeta[cpuID].baseCPU->isMain()) {
                                return;
                        }
                        // std::cout << "\n- - - Updating syslog" << std::endl;
                        int pos = loadstorelogentry::mainCPUMeta[cpuID].current_segment_to_fill*SIZEOFSYSCALLSEGMENT + current_entry[cpuID] - 1;
                        assert(pos < SYSCALLLOGSIZE);
                        assert(pos >= 0);
                        assert(entries.at(pos).initialized);
                        assert(!entries.at(pos).updated);
                        entries.at(pos).stateAfter = m_serialize(loadstorelogentry::allCPUMeta[cpuID].baseCPU->getContext(0));
                        assert(entries.at(pos).stateBefore.initialized);
                        entries.at(pos).updated = true;
                }

                static SyscallReturn do_read(int checkerID, Addr instAddr)
                {
                        assert(NUMBEROFCHECKERCORESPERCORE > 0); // Should not come here when there is no checker
                        int id = checkerID - NUMBEROFMAINCORES;
                        assert(id < NUMBEROFCHECKERCORESPERCORE*NUMBEROFMAINCORES);
                        int pos = SIZEOFSYSCALLSEGMENT*id + entryIndices[id];
                        // std::cout << "Syscalllogentry read by checker " << id << " at position " << pos << std::endl;
                        ++entryIndices[id];

                        assert(pos < SYSCALLLOGSIZE);
                        if (!entries.at(pos).initialized || entries.at(pos).instAddr != instAddr) {
                                // This will trigger a graceful error detection at next commit.
                                if (loadstorelogentry::debugFlag) std::cout << "Checker " << id << " attempted to read an uninitialised syscalllogentry at position " << pos << std::endl;
                                loadstorelogentry::checkerCPUMeta[id].interrupted = true;
                                return 0;
                        }
                        assert(entries.at(pos).unread);
                        assert(entries.at(pos).updated);

                        if (m_identical(loadstorelogentry::allCPUMeta[checkerID].baseCPU->getContext(0), entries.at(pos).stateBefore)) {
                           if (loadstorelogentry::debugFlag) std::cout << " --> Syscall modified state before, not an actual error" << std::endl;
                           m_copyRegs(loadstorelogentry::allCPUMeta[checkerID].baseCPU->getContext(0), entries.at(pos).stateAfter);
                        } // Otherwise, an error occurred before the syscall, so it is propagated.

                        entries.at(pos).unread = false;
                        return entries.at(pos).result;
                }

                static void reset_index(int id) {
                        if (id < NUMBEROFMAINCORES) {
                                assert(current_entry.at(id) >= 0);
                                maxIndices.at(loadstorelogentry::mainCPUMeta[id].current_segment_to_fill) = current_entry[id]-
                                        ((current_entry.at(id) > 0)? 1 : 0);
                                current_entry.at(id) = 0;
                        } else {
                                assert(NUMBEROFCHECKERCORESPERCORE > 0); // Should not come here when there is no checker
                                int checkerID = id - NUMBEROFMAINCORES;
                                assert(checkerID < NUMBEROFCHECKERCORESPERCORE*NUMBEROFMAINCORES);
                                // if (entryIndices[checkerID] != 0) std::cout << "Resetindex of checker " << checkerID << std::endl;
                                assert(SIZEOFSYSCALLSEGMENT*checkerID+maxIndices[checkerID] < SYSCALLLOGSIZE);
                                for (int i = maxIndices[checkerID]; i>=0; --i) {
                                        entries.at(SIZEOFSYSCALLSEGMENT*checkerID+i).initialized = false;
                                }
                                entryIndices.at(checkerID) = 0;
                        }
                }
                static void move_segment(int from_id, int to_id) {
                        assert(from_id >= NUMBEROFCHECKERCORESPERCORE*NUMBEROFMAINCORES);
                        assert(from_id < loadstorelogentry::checkerCPUMeta.size());
                        assert(to_id == from_id - NUMBEROFCHECKERCORESPERCORE*NUMBEROFMAINCORES);
                        for (int i = 0; i < SIZEOFSYSCALLSEGMENT; ++i) {
                                // if (entries.at(SIZEOFSYSCALLSEGMENT*to_id+i).initialized) {
                                //         std::cout << "move_segment to_id " << to_id << ", from_id " << from_id << ", i " << i;
                                //         std::cout << ", entryIndices[to_id] " << entryIndices[to_id] << ", maxIndices[to_id] " << maxIndices[to_id] << std::endl;
                                // }
                                // assert(!entries.at(SIZEOFSYSCALLSEGMENT*to_id+i).initialized);
                                syscalllogentry tmp = entries.at(SIZEOFSYSCALLSEGMENT*to_id+i);
                                entries.at(SIZEOFSYSCALLSEGMENT*to_id+i) = entries.at(SIZEOFSYSCALLSEGMENT*from_id+i);
                                entries.at(SIZEOFSYSCALLSEGMENT*from_id+i) = tmp;
                        }
                        int tmp = entryIndices[to_id];
                        entryIndices[to_id] = entryIndices[from_id];
                        entryIndices[from_id] = tmp;
                        tmp = maxIndices[to_id];
                        maxIndices[to_id] = maxIndices[from_id];
                        maxIndices[from_id] = tmp;
                }

};
}
#endif
