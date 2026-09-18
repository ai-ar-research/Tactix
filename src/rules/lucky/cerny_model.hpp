#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

namespace tactix {

RuleEligibility cerny_model_guard(const Kernel &);
ApplyResult cerny_model_apply(Kernel &);

} // namespace tactix
