#pragma once

#include "rules/registry.hpp"

namespace tactix::execution {

struct RegisteredRuleRequest {
  const RuleVTable &rule;
  uint64_t instance = INSTANCE_CANONICAL;
};

// Discovery and instance selection remain in policies. Registered execution
// retains the existing semantic-instance preference and canonical fallback.
inline ApplyResult execute_action(Kernel &k, const RegisteredRuleRequest &request) {
  const auto &rule = request.rule;
  if (request.instance == INSTANCE_CANONICAL ||
      (!rule.apply_semantic_instance && !rule.apply_instance))
    return apply_rule(rule, k);
  return rule.apply_semantic_instance
             ? apply_rule_semantic_instance(rule, k, request.instance)
             : apply_rule_instance(rule, k, request.instance);
}

} // namespace tactix::execution
