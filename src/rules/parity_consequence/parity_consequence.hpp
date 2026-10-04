#pragma once
#include "rules/registry.hpp"
namespace tactix {
RuleEligibility parity_consequence_guard(const Kernel &);
ApplyResult parity_consequence_apply(Kernel &);
} // namespace tactix
