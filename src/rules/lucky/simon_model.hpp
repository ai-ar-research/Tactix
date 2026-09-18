#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

namespace tactix {

RuleEligibility simon_model_guard(const Kernel &);
ApplyResult simon_model_apply(Kernel &);

} // namespace tactix
