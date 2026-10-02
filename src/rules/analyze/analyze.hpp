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

RuleEligibility analyze_guard(const Kernel &);
ApplyResult analyze_apply(Kernel &);

// Internal analysis sub-step for an explicit ConflictAnalysis request. The
// shared executor consumes any pending clause before returning an action;
// this is not an independently selectable public rule.
ApplyResult analyze_apply_with_target(Kernel &, uint32_t target_level);

// Eager subsumption (kissat `eagersubsume`): the learn-time half of the fused
// rule's clause maintenance — garbage every recently-learned clause the
// pending clause strictly subsumes, then enrol the pending clause itself.
// Owned here, since it is ConflictAnalysis's proof delta alongside OTFS's
// `del`, but called from `backjump_apply_to` once the trail is back at the
// target, because until then the subsumed candidates are still reasons.
// A no-op unless `Kernel::eager_subsume` is nonzero and a clause is pending.
// Why it is a sub-step and not a rule: docs/implementation-notes.md §15.
void analyze_eager_subsume(Kernel &);

// Complete OTFS's delayed replacement after Backjump has unassigned every
// analysis-level propagated literal.  Kept separate only because Backjump
// owns the trail mutation; ConflictAnalysis still exposes one atomic step.
void analyze_retire_otfs_reasons(Kernel &);

// Complete Analyze's deferred score/queue transaction after the fused
// Backjump has restored the trail. Kissat learns/backtracks before it bumps;
// keeping this ordering is observable through equal-score heap pose.
void analyze_bump_after_backjump(Kernel &);

} // namespace tactix
