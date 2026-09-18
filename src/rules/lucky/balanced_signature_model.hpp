#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

namespace tactix {

RuleEligibility balanced_signature_model_guard(const Kernel &);
ApplyResult balanced_signature_model_apply(Kernel &);

} // namespace tactix
