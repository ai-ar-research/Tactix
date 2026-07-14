// M1 Propagate: a thin wrapper over the kernel propagation engine
// (kernel_propagate, kissat proplit.h transliteration — moved into the
// kernel 2026-07-14 as a reviewed §5.3 primitive so probing rules can
// share it). This rule owns the search semantics: the conflict COUNT,
// the root-refutation proof delta, and the ApplyResult mapping.
#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

namespace tactix {

bool propagate_guard(const Kernel &k) {
  return !k.conflict_valid && !k.inconsistent &&
         k.propagate_head < k.trail.size();
}

ApplyResult propagate_apply(Kernel &k) {
  if (k.inconsistent)
    return ApplyResult::Unsat;
  if (kernel_propagate(k)) {
    k.stats.conflicts++;
    if (k.level == 0) { // root conflict: the formula is refuted
      k.inconsistent = true;
      k.conflict_valid = false;
      if (k.proof)
        k.proof->add_empty();
      return ApplyResult::Unsat;
    }
    return ApplyResult::Conflict;
  }
  return ApplyResult::Ok;
}

TACTIX_REGISTER_RULE(Propagate, RuleTier::Micro, propagate_guard,
                     propagate_apply);

} // namespace tactix
