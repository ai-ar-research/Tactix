// Propagate (plan §5.2): unit propagation with blocking literals.
// Canonical instance: propagate the trail to fixpoint or first conflict.
// Proof delta: none.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

RuleEligibility propagate_guard(const Kernel &);
ApplyResult propagate_apply(Kernel &);

// Propagation service (a primitive): propagate to fixpoint or conflict, emit
// root units, refute a root conflict. Shared so inprocessing rules can reach
// `U`.
ApplyResult propagate_service(Kernel &);

// D-LEVEL: complete the root closure `U` at entry; a no-op at a fixpoint. True
// iff it refuted.
inline bool inprocess_reach_root(Kernel &k) {
  return !kernel_propagation_fixpoint(k) &&
         propagate_service(k) == ApplyResult::Unsat;
}

} // namespace tactix
