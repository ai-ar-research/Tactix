#pragma once

#include "rules/registry.hpp"

namespace tactix {

RuleEligibility backtrackable_subsume_guard(const Kernel &);
ApplyResult backtrackable_subsume_apply(Kernel &);
ApplyResult backtrackable_strengthen_apply(Kernel &);
RuleEligibility backtrackable_eliminate_guard(const Kernel &);
ApplyResult backtrackable_eliminate_apply(Kernel &);

} // namespace tactix
