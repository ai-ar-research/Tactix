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

// Additive v2 semantic ConflictAnalysis payload (high bit, as Reduce's
// REDUCE_CLAUSE_INSTANCE): the backjump target is chosen after learning, the
// chronological target on every path (docs/calculus.md §3 M3). The frozen v1
// surface keeps its level payloads.
constexpr uint64_t ANALYZE_TARGET_CHRONO = uint64_t{1} << 63;

// How a sub-step below chooses the backjump target. Explicit: a level the
// caller fixed before analysis, clamped to [jump_level, level-1] on every
// path. Chrono: conflict_reuse_target on every path; the learned clause's
// target is reported to the caller. Advised: chronological on the reuse
// paths, the learned clause's target left to the caller (a declared
// diagnostic arm, docs/implementation-notes.md).
enum class AnalysisTarget : uint8_t { Explicit, Chrono, Advised };

RuleEligibility analyze_guard(const Kernel &);

// The sub-steps below are internal to ConflictAnalysis requests. The shared
// executor consumes any pending clause before returning an action; none is an
// independently selectable public rule. Each applies a retier schedule at
// every OTFS re-analysis conflict: the caller's for analyze_apply_native,
// otherwise the installed `Kernel::retier_schedule` when enabled.
ApplyResult analyze_apply(Kernel &); // Advised
ApplyResult analyze_apply_with_target(Kernel &, uint32_t target_level);
ApplyResult analyze_apply_chrono(Kernel &, uint32_t *chosen_target);
// `retier` may be null; `chosen_target` is written in Chrono mode only, when
// the analysis leaves a pending learned clause.
ApplyResult analyze_apply_native(Kernel &, AnalysisTarget mode,
                                 RetierSchedule *retier,
                                 uint32_t *chosen_target);

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
