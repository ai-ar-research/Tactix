// M1 Propagate (plan §5.2): unit propagation with blocking literals.
// Canonical instance: propagate the trail to fixpoint or first conflict.
// Proof delta: none (reasons recorded for Analyze's future LRAT hints).
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool propagate_guard(const Kernel &);
ApplyResult propagate_apply(Kernel &);

// The propagation SERVICE (a primitive, not a rule): propagate to a fixpoint or
// first conflict; at level 0 emit each derived root unit's LRAT id and, on a
// root conflict, refute (empty clause + `inconsistent`). Returns Ok (fixpoint),
// Conflict (below-root conflict recorded), or Unsat (root refuted). M1
// Propagate's `apply` is exactly this. It is exposed so the inprocessing rules
// can reach the root closure `U` at entry when applied off a fixpoint
// (guard purification, D-LEVEL) — a shared propagation primitive like
// `kernel_propagate`, so this is NOT a rule calling a rule. The root-unit
// emission is load-bearing for those callers: a drained unit needs its
// `unit_ids[v]` set before any later LRAT chain can cite it (probe.hpp dev. 8).
ApplyResult propagate_service(Kernel &);

// Guard-purification helper (D-LEVEL): an inprocessing rule whose guard no
// longer requires a propagation fixpoint calls this at each policy entry point
// (apply / apply_instance) to complete the root closure `U` before it reads the
// assignment. It is a **no-op at a fixpoint**, so DefaultPolicy — which always
// enters propagated — never calls `propagate_service` here and stays bit-exact;
// only a free N1 schedule that interleaves inprocessing without an intervening
// M1 triggers it. Returns true iff the drain refuted the formula (a root
// conflict), in which case the caller returns `Unsat`.
inline bool inprocess_reach_root(Kernel &k) {
  return k.propagate_head < k.trail.size() &&
         propagate_service(k) == ApplyResult::Unsat;
}

} // namespace tactix
