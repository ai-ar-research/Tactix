// S2 Reduce (plan §5.2): delete the least valuable of the redundant
// (learned) clauses — keeping recently-used low-glue tiers and clauses
// locked as reasons — then compact the arena. Proof delta: `delete` per
// removed clause (from the closed vocabulary).
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool reduce_guard(const Kernel &);

// ranked deletion candidates (canonical worst-first order: glue desc,
// size desc; redundant, unlocked, above the keep tier only — soundness
// filtering is the rule's). Pure query apart from transient reason locks.
void reduce_candidates(Kernel &, std::vector<Ref> &out);

// recompute Kernel::tier1/tier2 as the 50 %/90 % quantiles of the used-glue
// histogram (kissat 4.0 tiers.c). Pure configuration update: no proof delta,
// no formula change — it only moves which clauses reduce_candidates offers.
// WHEN to call it is the policy's decision (kissat: at every reduction and on
// a doubling conflict interval).
void reduce_retier(Kernel &);

// delete exactly refs[0..n) (re-validating: reasons/garbage are skipped),
// emit proof deletes, then collect garbage. The CHOICE of n is the
// policy's (plan §5: ReductionPolicy sub-policy).
ApplyResult reduce_apply_refs(Kernel &, const Ref *refs, size_t n);
ApplyResult reduce_apply(Kernel &);

// canonical deletion volume — kissat's default fraction of the ranked
// candidates; defined once here so the canonical instance and the
// DefaultPolicy sub-policy cannot drift apart
inline size_t reduce_canonical_victims(size_t candidates) {
  return (3 * candidates) / 4;
}

} // namespace tactix
