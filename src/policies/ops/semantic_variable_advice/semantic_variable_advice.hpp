#pragma once

#include "rules/registry.hpp"

namespace tactix {

RuleEligibility semantic_variable_advice_guard(const Kernel &);
ApplyResult semantic_variable_advice_rule_apply(Kernel &);
uint32_t semantic_variable_advice_rule_instances(Kernel &, uint64_t *,
                                                 uint32_t capacity);
ApplyResult semantic_variable_advice_rule_apply_instance(Kernel &, uint64_t);

} // namespace tactix
