#include "policies/ops/semantic_variable_advice/semantic_variable_advice.hpp"

#include "strategy/semantic_variable_advice.hpp"
#include "strategy/theory_action_parameters.hpp"

namespace tactix {

RuleEligibility semantic_variable_advice_guard(const Kernel &k) {
  return RuleEligibility{}
      .fact("consistent", !k.inconsistent)
      .fact("root", !k.level && !k.conflict_valid && !k.pending.valid)
      .fact("heuristics-attached", k.h != nullptr)
      .fact("candidate-current",
            strategy::has_current_semantic_variable_candidate(k));
}

uint32_t semantic_variable_advice_rule_instances(Kernel &k, uint64_t *out,
                                                 uint32_t capacity) {
  uint32_t count = 0;
  for (const auto &candidate : k.semantic_variable_candidates) {
    if (count == capacity) break;
    if (strategy::semantic_variable_candidate(k, candidate.id))
      out[count++] = candidate.id;
  }
  return count;
}

ApplyResult semantic_variable_advice_rule_apply_instance(Kernel &k,
                                                         uint64_t instance) {
  if (!semantic_variable_advice_guard(k)) return ApplyResult::NoOp;
  if (instance == INSTANCE_CANONICAL) {
    uint64_t selected = 0;
    if (!semantic_variable_advice_rule_instances(k, &selected, 1))
      return ApplyResult::NoOp;
    instance = selected;
  }
  const auto &parameters = k.semantic_variable_advice_parameters;
  strategy::SemanticVariableAdvice advice;
  if (!strategy::make_semantic_variable_advice(
          k, instance, parameters.existing_theory_priority,
          parameters.introduced_theory_priority, advice))
    return ApplyResult::NoOp;
  const auto result = strategy::commit_semantic_variable_advice(
      k, advice,
      {parameters.maximum_variables, parameters.maximum_priority_sum});
  return result.status == strategy::SemanticVariableAdviceStatus::Applied
             ? ApplyResult::Ok
             : ApplyResult::NoOp;
}

ApplyResult semantic_variable_advice_rule_apply(Kernel &k) {
  return semantic_variable_advice_rule_apply_instance(k, INSTANCE_CANONICAL);
}

TACTIX_REGISTER_POLICY_OP(SemanticVariableAdvice, RuleTier::Macro,
                          semantic_variable_advice_guard,
                          semantic_variable_advice_rule_apply,
                          semantic_variable_advice_rule_instances,
                          semantic_variable_advice_rule_apply_instance);

} // namespace tactix
