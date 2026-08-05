// Analyze (plan §5.2): first-UIP conflict analysis with recursive
// minimization, all-UIP block shrinking, and on-the-fly subsumption.
// Learns the clause, emits its add-RUP proof delta, stores it in the
// arena (F/R/π changes), and hands the trail change to Backjump via
// kernel.pending (τ is Backjump's).
//
// API-freeze decision: Analyze is ATOMIC — its instance space is the
// recorded conflict alone; minimization/shrinking/OTFS intensity (kissat
// options minimize/shrink/otfs) are NOT instance parameters. Knobs may join
// additively if a policy preset needs them; splitting the passes into
// separate rules is REJECTED — it would expose half-analyzed intermediate
// states to N1 schedules (same reasoning as the OTFS fresh-allocation
// deviation).
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool analyze_guard(const Kernel &);
ApplyResult analyze_apply(Kernel &);

// SP-EAGERSUB-1 (kissat `eagersubsume`): the learn-time half of the fused
// rule's clause maintenance — garbage every recently-learned clause the
// pending clause strictly subsumes, then enrol the pending clause itself.
// Owned here (it is ConflictAnalysis's proof delta, alongside OTFS's `del`)
// but CALLED from `backjump_apply_to`, once the trail is back at the target:
// the subsumed candidates are reasons until then, measurably all of them.
// See the definition in analyze.cpp for the measurement and the argument.
// No-op unless `Kernel::eager_subsume` is nonzero and a clause is pending.
void analyze_eager_subsume(Kernel &);

} // namespace tactix
