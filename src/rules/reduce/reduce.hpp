// S2 Reduce (plan §5.2): delete the least valuable redundant clauses, keeping
// used low-glue tiers and reasons, then compact. Proof delta: `delete` each.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

// The tier option defaults (kissat options.h `tier1`/`tier2`): the bands
// before any used-glue data exists, and what a static-tier consumer uses.
constexpr uint32_t TIER1_STATIC = 2;
constexpr uint32_t TIER2_STATIC = 6;

RuleEligibility reduce_guard(const Kernel &);

// `kissat_flush_and_mark_reason_clauses`: on new root units, propagate to a
// fixpoint before locking reasons. Idempotent; may refute.
void reduce_prepare(Kernel &);

// Ranked deletion candidates, worst-first. Reason locks are soundness, tier
// protection is advice. `measure` gates scan counters (never `instances()`).
void reduce_candidates(Kernel &, std::vector<Ref> &out, bool measure = false,
                       bool policy_protection = true);

// Additive v2 semantic payload: the high bit selects one arena-clause snapshot.
// The frozen v1 surface continues to enumerate only ranked victim counts.
constexpr uint64_t REDUCE_CLAUSE_INSTANCE = uint64_t{1} << 63;

// Recompute one mode's tiers from the used-glue quantiles (kissat 4.0 tiers.c).
// No proof delta. Counts `stats.retiered`.
void reduce_retier(Kernel &, RetierSchedule &schedule, uint32_t mode);
void reduce_retier_from_histogram(Kernel &, RetierSchedule &schedule,
                                  uint32_t mode, const uint64_t *used);

// The schedule's conflict deadline (kissat analyze.c, at OTFS re-analysis):
// retier the current mode, pose its tiers and advance geometrically. True when
// fired.
bool reduce_retier_at_conflict(Kernel &, RetierSchedule &schedule,
                               uint32_t mode);

// Delete refs[0..n) (skipping reasons/garbage), emit deletes, collect. n and a
// zero-offset compacting collection are the policy's (Kissat reduce.c
// `compact ? 0 : first_reducible`).
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
