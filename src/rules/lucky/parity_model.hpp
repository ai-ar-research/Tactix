// SAT-only Lucky sub-strategy: recover Boolean affine equations from ternary
// CNF groups, search the residual GF(2) space, and accept only a total model
// independently checked against every live clause.
#pragma once

#include "rules/registry.hpp"
#include "strategy/rule_parameters.hpp"

namespace tactix {

RuleEligibility parity_model_guard(const Kernel &);
ApplyResult parity_model_apply(Kernel &);

} // namespace tactix
