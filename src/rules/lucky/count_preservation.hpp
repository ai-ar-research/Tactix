#pragma once
#include "rules/registry.hpp"

namespace tactix {
// Terminal original-input count conservation. Complete selector rows and
// guarded equivalences define a balanced network; fixed boundary counts clash.
// Work is caller-bounded by Kernel::work_budget and the shared stop contract.
RuleEligibility count_preservation_guard(const Kernel &);
ApplyResult count_preservation_apply(Kernel &);
}
