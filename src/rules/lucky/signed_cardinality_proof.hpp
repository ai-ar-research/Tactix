#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

namespace tactix {

RuleEligibility signed_cardinality_proof_guard(const Kernel &);
ApplyResult signed_cardinality_proof_apply(Kernel &);

} // namespace tactix
