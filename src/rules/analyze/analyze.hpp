// Analyze (docs/calculus.md §3 M3): first-UIP conflict analysis with recursive
// minimization, all-UIP block shrinking and on-the-fly subsumption. Learns the
// clause, emits its add-RUP proof delta, stores it in the arena (F/R/π), and
// hands the trail change to Backjump via kernel.pending (τ is Backjump's).
//
// Analyze is atomic: its instance space is the recorded conflict alone, and
// minimization/shrinking/OTFS intensity are not instance parameters. Knobs may
// join additively if a policy preset needs them, but splitting the passes into
// separate rules is rejected — it would expose half-analyzed states to N1
// schedules, the same reasoning as the OTFS fresh-allocation deviation.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool analyze_guard(const Kernel &);
ApplyResult analyze_apply(Kernel &);

// Eager subsumption (kissat `eagersubsume`): the learn-time half of the fused
// rule's clause maintenance — garbage every recently-learned clause the
// pending clause strictly subsumes, then enrol the pending clause itself.
// Owned here, since it is ConflictAnalysis's proof delta alongside OTFS's
// `del`, but called from `backjump_apply_to` once the trail is back at the
// target, because until then the subsumed candidates are still reasons.
// A no-op unless `Kernel::eager_subsume` is nonzero and a clause is pending.
// Why it is a sub-step and not a rule: docs/implementation-notes.md §15.
void analyze_eager_subsume(Kernel &);

} // namespace tactix
