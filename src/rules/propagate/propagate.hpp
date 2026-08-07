// Propagate (plan §5.2): unit propagation with blocking literals.
// Canonical instance: propagate the trail to fixpoint or first conflict.
// Proof delta: none (reasons recorded for Analyze's LRAT hints).
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool propagate_guard(const Kernel &);
ApplyResult propagate_apply(Kernel &);

// The propagation service (a primitive, not a rule): propagate to a fixpoint
// or first conflict; emit each derived root unit's LRAT id and, on a root
// conflict, refute. The Propagate rule's `apply` is exactly this. Exposed so
// inprocessing rules can reach the root closure `U` at entry — a shared
// primitive like `kernel_propagate`, not a rule calling a rule. The root-unit
// emission is load-bearing for them: a drained unit needs `unit_ids[v]` set
// before any LRAT chain can cite it.
ApplyResult propagate_service(Kernel &);

// Guard-purification helper (D-LEVEL): an inprocessing rule whose guard does
// not require a fixpoint calls this at each entry point to complete the root
// closure `U` before reading the assignment. A no-op at a fixpoint, so
// DefaultPolicy stays bit-exact. True iff the drain refuted the formula.
inline bool inprocess_reach_root(Kernel &k) {
  return k.propagate_head < k.trail.size() &&
         propagate_service(k) == ApplyResult::Unsat;
}

} // namespace tactix
