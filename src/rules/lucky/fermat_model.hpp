#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

namespace tactix {

RuleEligibility fermat_model_guard(const Kernel &);
ApplyResult fermat_model_apply(Kernel &);

} // namespace tactix
