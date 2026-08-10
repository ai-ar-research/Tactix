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

bool reduce_guard(const Kernel &);

// Ranked deletion candidates, worst-first (glue desc, size desc; redundant,
// unlocked, above the keep tier only — soundness filtering is the rule's). A
// pure query apart from transient reason locks. `measure` gates the scan
// counters: only a real reduction pass may move them, never `instances()`,
// since an enumeration that changes a counter breaks replay identity.
void reduce_candidates(Kernel &, std::vector<Ref> &out, bool measure = false);

// Recompute one mode's tier snapshot from configurable quantiles of its
// used-glue histogram (kissat 4.0 tiers.c). A pure configuration update — no
// proof delta, no formula change — and when/how to pose it is the policy's.
void reduce_retier(Kernel &, uint32_t mode, uint32_t tier1_default,
                   uint32_t tier2_default,
                   uint32_t tier1_relative_permille,
                   uint32_t tier2_relative_permille);

// delete exactly refs[0..n) (re-validating: reasons/garbage are skipped),
// emit proof deletes, then collect garbage. The CHOICE of n is the
// policy's (plan §5: ReductionPolicy sub-policy).
ApplyResult reduce_apply_refs(Kernel &, const Ref *refs, size_t n);
ApplyResult reduce_apply(Kernel &);

// Canonical deletion volume, kissat's default fraction of the ranked
// candidates; defined once so the canonical instance and DefaultPolicy's
// sub-policy cannot drift apart.
inline size_t reduce_canonical_victims(size_t candidates) {
  return (3 * candidates) / 4;
}

} // namespace tactix
