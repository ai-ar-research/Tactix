#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

namespace tactix {
RuleEligibility graph_labeling_model_guard(const Kernel &);
ApplyResult graph_labeling_model_apply(Kernel &);
} // namespace tactix
