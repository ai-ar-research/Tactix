#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

namespace tactix {

RuleEligibility stconn_proof_guard(const Kernel &);
ApplyResult stconn_proof_apply(Kernel &);

} // namespace tactix
