#pragma once
#include "rules/registry.hpp"
namespace tactix {
RuleEligibility affine_model_guard(const Kernel &);
ApplyResult affine_model_apply(Kernel &);
}
