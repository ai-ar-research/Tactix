// SAT-only Lucky sub-strategy for exact one-hot GF(3) affine encodings.
#pragma once

#include "rules/registry.hpp"
#include "strategy/rule_parameters.hpp"

namespace tactix {

RuleEligibility onehot_model_guard(const Kernel &);
ApplyResult onehot_model_apply(Kernel &);

} // namespace tactix
