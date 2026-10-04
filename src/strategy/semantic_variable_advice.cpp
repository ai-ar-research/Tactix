#include "strategy/semantic_variable_advice.hpp"

#include "policies/heuristics.hpp"

#include <algorithm>
#include <limits>
#include <unordered_set>

namespace tactix::strategy {

namespace {

bool same_version(const StateVersion &a, const StateVersion &b) {
  return a.arena_generation == b.arena_generation &&
         a.touch_epoch == b.touch_epoch &&
         a.strategy_epoch == b.strategy_epoch;
}

bool branchable_role(SemanticVariableRole role) {
  return role == SemanticVariableRole::ExistingTheory ||
         role == SemanticVariableRole::IntroducedTheory;
}

} // namespace

SemanticVariableAdviceResult apply_semantic_variable_advice(
    Kernel &k, const SemanticVariableAdvice &advice,
    const SemanticVariableAdviceLimits &limits) {
  SemanticVariableAdviceResult out;
  if (!advice.action_id || !limits.maximum_variables ||
      !limits.maximum_priority_sum || advice.variables.empty())
    return out;
  if (!same_version(advice.source, state_version(k))) {
    out.status = SemanticVariableAdviceStatus::Stale;
    return out;
  }
  if (k.inconsistent || k.conflict_valid || k.level ||
      !kernel_propagation_fixpoint(k) || !k.h) {
    out.status = SemanticVariableAdviceStatus::Inapplicable;
    return out;
  }
  if (advice.variables.size() > limits.maximum_variables) {
    out.status = SemanticVariableAdviceStatus::ResourceExhausted;
    return out;
  }

  struct Resolved {
    Var variable;
    uint32_t priority;
    uint32_t external;
  };
  std::vector<Resolved> resolved;
  resolved.reserve(advice.variables.size());
  std::unordered_set<uint32_t> seen;
  uint64_t sum = 0;
  for (const SemanticVariable &item : advice.variables) {
    if (!item.external || item.external >
                              static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
        !item.priority || !branchable_role(item.role) ||
        !seen.insert(item.external).second ||
        var_of(internalize(static_cast<int>(item.external))) >=
            k.external_to_internal.size())
      return out;
    const Var v = k.external_to_internal[
        var_of(internalize(static_cast<int>(item.external)))];
    if (v == INVALID_VAR || v >= k.nvars || kernel_var_retired(k, v) ||
        kernel_root_value(k, make_lit(v, false)) ||
        !heuristics_var_active(k, v))
      return out;
    if (item.priority > limits.maximum_priority_sum - sum) {
      out.status = SemanticVariableAdviceStatus::ResourceExhausted;
      return out;
    }
    sum += item.priority;
    resolved.push_back({v, item.priority, item.external});
  }

  // Low priorities move first; the highest-priority identity is the final
  // VMTF bump and therefore the next preferred queue member. External identity
  // breaks ties deterministically across compaction and replay.
  std::sort(resolved.begin(), resolved.end(), [](const Resolved &a,
                                                  const Resolved &b) {
    if (a.priority != b.priority)
      return a.priority < b.priority;
    return a.external > b.external;
  });
  for (const Resolved &item : resolved) {
    heuristics_queue_bump(k, item.variable);
    for (uint32_t dose = 0; dose < item.priority; ++dose)
      heuristics_score_bump(k, item.variable);
  }
  // Advice changes the controller-owned search state. Advance the shared
  // strategy version so replaying the same state-bound request is stale and
  // model/decision caches cannot survive the changed ranking.
  ++k.strategy_epoch;
  out.status = SemanticVariableAdviceStatus::Applied;
  out.variables = static_cast<uint32_t>(resolved.size());
  out.priority_sum = sum;
  return out;
}

} // namespace tactix::strategy
