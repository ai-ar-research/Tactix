// S2 Reduce (plan §5.2): delete the least valuable half of the redundant
// (learned) clauses — keeping low-glue tier-1 clauses and clauses locked as
// reasons — then compact the arena. Proof delta: `delete` per removed
// clause (from the closed vocabulary).
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool reduce_guard(const Kernel &);

// ranked deletion candidates (canonical worst-first order: glue desc,
// size desc; redundant, unlocked, above the keep tier only — soundness
// filtering is the rule's). Pure query apart from transient reason locks.
void reduce_candidates(Kernel &, std::vector<Ref> &out);

// delete exactly refs[0..n) (re-validating: reasons/garbage are skipped),
// emit proof deletes, then collect garbage. The CHOICE of n is the
// policy's (plan §5 rev 17: ReductionPolicy sub-policy).
ApplyResult reduce_apply_refs(Kernel &, const Ref *refs, size_t n);
ApplyResult reduce_apply(Kernel &);

} // namespace tactix
