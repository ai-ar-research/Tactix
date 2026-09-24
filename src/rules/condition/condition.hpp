// X5: conditional-autarky deletion with an explicit finite cube witness.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

struct ConditionInstance {
  std::vector<Lit> clause;  // current internal literals, used to name live C
  std::vector<Lit> witness; // stable external literals, including compacted holes
};

RuleEligibility condition_guard(const Kernel &);

// This family has no default producer. The complete legal action is an
// explicit (clause, cube) tuple, exposed by tactix_apply_condition. False
// means inapplicable or interrupted and leaves proof/formula/rho unchanged.
struct ConditionPrepared {
  Ref target = INVALID_REF; // INVALID_REF denotes a live binary pair
  Lit first = INVALID_LIT, second = INVALID_LIT;
};
bool condition_prepare(Kernel &, const ConditionInstance &, ConditionPrepared &);
ApplyResult condition_commit(Kernel &, const ConditionInstance &,
                             const ConditionPrepared &);
bool condition_apply_explicit(Kernel &, const ConditionInstance &);
ApplyResult condition_apply(Kernel &);

} // namespace tactix
