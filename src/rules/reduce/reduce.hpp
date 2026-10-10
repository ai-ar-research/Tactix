// S2 Reduce (plan §5.2): delete the least valuable of the redundant
// (learned) clauses — keeping recently-used low-glue tiers and clauses
// locked as reasons — then compact the arena. Proof delta: `delete` per
// removed clause (from the closed vocabulary).
#pragma once

#include "rules/registry.hpp"

namespace tactix {

// The tier option defaults (kissat options.h `tier1`/`tier2`): the bands
// before any used-glue data exists, and what a static-tier consumer uses.
constexpr uint32_t TIER1_STATIC = 2;
constexpr uint32_t TIER2_STATIC = 6;

RuleEligibility reduce_guard(const Kernel &);

// Mirror `kissat_flush_and_mark_reason_clauses`: when a reduction observes
// newly derived root units, unwind and propagate to a root fixpoint before
// selecting reason-locked victims, then consume the unflushed marker.
// Idempotent; propagation may refute the formula.
void reduce_prepare(Kernel &);

// Ranked deletion candidates, worst-first. Reason locks are soundness;
// optional tier/recency protection is policy advice, not public eligibility. A
// pure query apart from transient reason locks. `measure` gates the scan
// counters: only a real reduction pass may move them, never `instances()`,
// since an enumeration that changes a counter breaks replay identity.
void reduce_candidates(Kernel &, std::vector<Ref> &out, bool measure = false,
                       bool policy_protection = true);

// Additive v2 semantic payload: the high bit selects one arena-clause snapshot.
// The frozen v1 surface continues to enumerate only ranked victim counts.
constexpr uint64_t REDUCE_CLAUSE_INSTANCE = uint64_t{1} << 63;

// Recompute one mode's tier snapshot in `schedule` from its configurable
// quantiles of the used-glue histogram (kissat 4.0 tiers.c). A pure
// configuration update — no proof delta, no formula change — and when/how to
// pose it is the caller's. Counts `stats.retiered`.
void reduce_retier(Kernel &, RetierSchedule &schedule, uint32_t mode);
void reduce_retier_from_histogram(Kernel &, RetierSchedule &schedule,
                                  uint32_t mode, const uint64_t *used);

// The schedule's conflict deadline (kissat analyze.c, tested only on the
// conflict an OTFS re-analysis counts): past it, retier the current search
// mode (0 focused, 1 stable), pose that mode's tiers as `tier1/tier2`, and
// advance the deadline geometrically. True when it fired.
bool reduce_retier_at_conflict(Kernel &, RetierSchedule &schedule,
                               uint32_t mode);

// delete exactly refs[0..n) (re-validating: reasons/garbage are skipped),
// emit proof deletes, then collect garbage. The CHOICE of n and whether a due
// compacting collection starts at arena offset zero are the policy's (plan §5:
// ReductionPolicy sub-policy; Kissat reduce.c `compact ? 0 : first_reducible`).
ApplyResult reduce_apply_refs(Kernel &, const Ref *refs, size_t n,
                              bool compact_collect = false);
ApplyResult reduce_apply(Kernel &);

// Policy-selected deletion volume. The numerator and denominator live on the
// typed strategy surfaces; this arithmetic helper owns no heuristic default.
inline size_t reduce_fractional_victims(size_t candidates,
                                        uint32_t numerator,
                                        uint32_t denominator) {
  assert(denominator && numerator <= denominator);
  return static_cast<size_t>(
      (static_cast<unsigned __int128>(candidates) * numerator) / denominator);
}

} // namespace tactix
