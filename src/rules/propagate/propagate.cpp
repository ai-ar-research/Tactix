// Propagate: wrapper over kernel_propagate (kissat proplit.h); owns the
// conflict count, the root-refutation proof delta and the ApplyResult.
#include "rules/propagate/propagate.hpp"

#include "rules/instance_helpers.hpp"

#include <cassert>

namespace tactix {

RuleEligibility propagate_guard(const Kernel &k) {
  // Pure precondition (calculus §3 M1): not refuted, no unresolved conflict,
  // work remains. `pending` never crosses a rule boundary (D-SPLIT-1).
  RuleEligibility result;
  return result.fact("no-conflict", !k.conflict_valid)
      .fact("consistent", !k.inconsistent)
      .fact("pending-propagation", !kernel_propagation_fixpoint(k));
}

ApplyResult propagate_service(Kernel &k) {
  if (k.inconsistent)
    return ApplyResult::Unsat;
  // `kernel_propagate` publishes the root literals it forces.
  const bool conflict = kernel_propagate(k);
  if (conflict) {
    k.stats.conflicts++;
    if (k.level == 0) { // root conflict: the formula is refuted
      k.inconsistent = true;
      k.conflict_valid = false;
      kernel_emit_empty(k);
      return ApplyResult::Unsat;
    }
    return ApplyResult::Conflict;
  }
  // `kissat_update_conflicts_and_trail` consumes the unflushed root prefix
  // after root-level propagation.
  if (k.level == 0)
    k.unflushed_root = 0;
  return ApplyResult::Ok;
}

// M1 Propagate's registered apply is exactly the propagation service.
ApplyResult propagate_apply(Kernel &k) { return propagate_service(k); }

TACTIX_REGISTER_RULE(Propagate, RuleTier::Micro, propagate_guard,
                     propagate_apply);

} // namespace tactix
