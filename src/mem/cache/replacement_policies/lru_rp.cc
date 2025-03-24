/**
 * Copyright (c) 2018-2020 Inria
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

#include "mem/cache/replacement_policies/lru_rp.hh"

#include <cassert>
#include <memory>

#include "params/LRURP.hh"
#include "sim/cur_tick.hh"
#include "mem/cache/loadstorelogentry.hh"

namespace gem5
{

GEM5_DEPRECATED_NAMESPACE(ReplacementPolicy, replacement_policy);
namespace replacement_policy
{

LRU::LRU(const Params &p)
  : Base(p)
{
}

void
LRU::invalidate(const std::shared_ptr<ReplacementData>& replacement_data)
{
    // Reset last touch timestamp
    std::static_pointer_cast<LRUReplData>(
        replacement_data)->lastTouchTick = Tick(0);
}

void
LRU::touch(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    // Update last touch timestamp
    std::static_pointer_cast<LRUReplData>(
        replacement_data)->lastTouchTick = curTick();
}

void
LRU::reset(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    // Set last touch timestamp
    std::static_pointer_cast<LRUReplData>(
        replacement_data)->lastTouchTick = curTick();
}

ReplaceableEntry*
LRU::getVictim(const ReplacementCandidates& candidates) const
{
    // There must be at least one replacement candidate
    assert(candidates.size() > 0);
    assert(cpuID != -2);

    // Visit all candidates to find victim
    ReplaceableEntry* victim = candidates[0];
    for (const auto& candidate : candidates) {
        // Update victim entry if necessary
        if (std::static_pointer_cast<LRUReplData>(candidate->replacementData)->lastTouchTick <
                std::static_pointer_cast<LRUReplData>(victim->replacementData)->lastTouchTick // Candidate last touch older than victim
            || (!useVanillaReplacement && cpuID < NUMBEROFMAINCORES && cpuID>=0 && dynamic_cast<CacheBlk*>(victim) // is main core and victim not null
             && (static_cast<CacheBlk*>(victim))->timestamp > (static_cast<CacheBlk*>(candidate))->timestamp // candidate last checkpoint older than victim
             && (static_cast<CacheBlk*>(victim))->timestamp > loadstorelogentry::mainCPUMeta.at(cpuID).committed_timestamp // victim checkpoint younger than committed checkpoint
            )
           ) {
            if(cpuID >= NUMBEROFMAINCORES || cpuID<0 || dynamic_cast<CacheBlk *>(candidate) == nullptr || useVanillaReplacement || // Checker core or candidate invalid
               (!useVanillaReplacement && dynamic_cast<CacheBlk*>(candidate) && // candidate is valid
                ((static_cast<CacheBlk*>(candidate))->timestamp <= loadstorelogentry::mainCPUMeta.at(cpuID).committed_timestamp || // candidate was checked
                 (static_cast<CacheBlk*>(candidate))->timestamp <= (static_cast<CacheBlk*>(victim))->timestamp || // candidate is older than victim
                 (static_cast<CacheBlk*>(victim))->timestamp == 0 // victim is not dirty
                )
               )
              ) {
                victim = candidate;
            }
        }
    }

    return victim;
}

std::shared_ptr<ReplacementData>
LRU::instantiateEntry()
{
    return std::shared_ptr<ReplacementData>(new LRUReplData());
}

} // namespace replacement_policy
} // namespace gem5
