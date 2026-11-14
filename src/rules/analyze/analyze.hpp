// Analyze (docs/dev/calculus.md §3 M3): first-UIP analysis with minimization,
// shrinking and OTFS. Learns the clause, emits add-RUP, stores it, and hands
// the trail change to Backjump via kernel.pending. Atomic: the instance is the
// recorded conflict; splitting passes would expose half-analyzed states.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

// Semantic payload (high bit): the chronological target chosen after learning
// (calculus §3 M3). v1 keeps level payloads.
constexpr uint64_t ANALYZE_TARGET_CHRONO = uint64_t{1} << 63;

// Target choice. Explicit: caller level clamped to [jump_level, level-1].
// Chrono: conflict_reuse_target everywhere, reported to the caller. Advised:
// chronological on reuse paths, the learned target left to the caller.
enum class AnalysisTarget : uint8_t { Explicit, Chrono, Advised };

RuleEligibility analyze_guard(const Kernel &);

// Internal sub-steps of ConflictAnalysis. Each applies a retier schedule at
// every OTFS re-analysis conflict: the caller's for analyze_apply_native,
// otherwise `Kernel::retier_schedule` when enabled.
ApplyResult analyze_apply(Kernel &); // Advised
ApplyResult analyze_apply_with_target(Kernel &, uint32_t target_level);
ApplyResult analyze_apply_chrono(Kernel &, uint32_t *chosen_target);
// `retier` may be null; `chosen_target` is written in Chrono mode only, when
// the analysis leaves a pending learned clause.
ApplyResult analyze_apply_native(Kernel &, AnalysisTarget mode,
                                 RetierSchedule *retier,
                                 uint32_t *chosen_target);

// Eager subsumption (kissat `eagersubsume`): garbage recent learned clauses the
// pending clause strictly subsumes, then enrol it. Called from
// `backjump_apply_to` after the trail is cut. No-op unless
// `Kernel::eager_subsume` is nonzero (implementation-notes §15).
void analyze_eager_subsume(Kernel &);

// Finish OTFS's delayed replacement after Backjump unassigns the analysis
// level.
void analyze_retire_otfs_reasons(Kernel &);

// Deferred bump after the fused Backjump (Kissat learns/backtracks first).
void analyze_bump_after_backjump(Kernel &);

} // namespace tactix
