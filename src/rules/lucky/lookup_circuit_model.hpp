#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

namespace tactix {

RuleEligibility lookup_circuit_model_guard(const Kernel &);
ApplyResult lookup_circuit_model_apply(Kernel &);

} // namespace tactix
