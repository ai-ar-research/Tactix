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

bool current(const Kernel &k, const SemanticVariableCandidateInfo &candidate) {
  const StateVersion state = state_version(k);
  return candidate.arena_generation == state.arena_generation &&
         candidate.touch_epoch == state.touch_epoch &&
         candidate.strategy_epoch == state.strategy_epoch;
}

} // namespace

SemanticVariableAdviceResult commit_semantic_variable_advice(
    Kernel &k, const SemanticVariableAdvice &advice,
    const SemanticVariableAdviceLimits &limits) {
  SemanticVariableAdviceResult out;
  ++k.stats.semantic_variable_advice_attempts;
  struct RecordOutcome {
    Kernel &kernel;
    SemanticVariableAdviceResult &result;
    ~RecordOutcome() {
      switch (result.status) {
      case SemanticVariableAdviceStatus::Applied:
        ++kernel.stats.semantic_variable_advice_applied;
        kernel.stats.semantic_variable_advice_variables += result.variables;
        kernel.stats.semantic_variable_advice_priority += result.priority_sum;
        break;
      case SemanticVariableAdviceStatus::Invalid:
      case SemanticVariableAdviceStatus::NoChange:
        ++kernel.stats.semantic_variable_advice_invalid;
        break;
      case SemanticVariableAdviceStatus::Stale:
        ++kernel.stats.semantic_variable_advice_stale;
        break;
      case SemanticVariableAdviceStatus::ResourceExhausted:
        ++kernel.stats.semantic_variable_advice_resource_exhausted;
        break;
      case SemanticVariableAdviceStatus::Inapplicable:
        ++kernel.stats.semantic_variable_advice_inapplicable;
        break;
      }
    }
  } record{k, out};
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
  out.status = SemanticVariableAdviceStatus::Applied;
  out.variables = static_cast<uint32_t>(resolved.size());
  out.priority_sum = sum;
  return out;
}

SemanticVariableAdviceResult apply_semantic_variable_advice(
    Kernel &k, const SemanticVariableAdvice &advice,
    const SemanticVariableAdviceLimits &limits) {
  SemanticVariableAdviceResult result =
      commit_semantic_variable_advice(k, advice, limits);
  if (result.status == SemanticVariableAdviceStatus::Applied)
    ++k.strategy_epoch;
  return result;
}

void begin_semantic_variable_candidate_action(Kernel &k) {
  if (!k.semantic_variable_candidate_pending) return;
  k.semantic_variable_candidate_pending = false;
  static_cast<SemanticVariableCandidateInfo &>(
      k.staged_semantic_variable_candidate) = {};
  k.staged_semantic_variable_candidate.variables.clear();
}

bool stage_semantic_variable_candidate(
    Kernel &k, SemanticVariableProducer producer, uint64_t action_id,
    const std::vector<SemanticVariable> &variables) {
  const auto reject = [&]() {
    begin_semantic_variable_candidate_action(k);
    return false;
  };
  if (!valid_semantic_variable_producer(producer) || !action_id ||
      variables.empty() || k.semantic_variable_candidate_pending ||
      k.semantic_variable_publication == (UINT64_MAX >> 8))
    return reject();
  std::unordered_set<uint32_t> seen;
  SemanticVariableCandidate candidate;
  candidate.action_id = action_id;
  candidate.producer = producer;
  candidate.variables.reserve(variables.size());
  for (SemanticVariable variable : variables) {
    if (!variable.external || !branchable_role(variable.role) ||
        !seen.insert(variable.external).second)
      return reject();
    variable.priority = 1;
    candidate.existing_variables +=
        variable.role == SemanticVariableRole::ExistingTheory;
    candidate.introduced_variables +=
        variable.role == SemanticVariableRole::IntroducedTheory;
    candidate.variables.push_back(variable);
  }
  k.staged_semantic_variable_candidate = std::move(candidate);
  k.semantic_variable_candidate_pending = true;
  return true;
}

void finish_semantic_variable_candidate_action(Kernel &k, ApplyResult result) {
  if (!k.semantic_variable_candidate_pending || result != ApplyResult::Ok) {
    begin_semantic_variable_candidate_action(k);
    return;
  }
  SemanticVariableCandidate candidate =
      std::move(k.staged_semantic_variable_candidate);
  k.semantic_variable_candidate_pending = false;
  const uint64_t generation = ++k.semantic_variable_publication;
  candidate.id = (generation << 8) | static_cast<uint8_t>(candidate.producer);
  const StateVersion state = state_version(k);
  candidate.arena_generation = state.arena_generation;
  candidate.touch_epoch = state.touch_epoch;
  candidate.strategy_epoch = state.strategy_epoch;
  const uint32_t existing = candidate.existing_variables;
  const uint32_t introduced = candidate.introduced_variables;
  k.semantic_variable_candidates[static_cast<size_t>(candidate.producer)] =
      std::move(candidate);
  ++k.stats.semantic_variable_candidates_published;
  k.stats.semantic_variable_candidate_existing += existing;
  k.stats.semantic_variable_candidate_introduced += introduced;
}

const SemanticVariableCandidate *semantic_variable_candidate(
    const Kernel &k, uint64_t id) {
  if (!(id >> 8)) return nullptr;
  const auto producer = static_cast<SemanticVariableProducer>(id & 255);
  if (!valid_semantic_variable_producer(producer)) return nullptr;
  const auto &candidate =
      k.semantic_variable_candidates[static_cast<size_t>(producer)];
  return candidate.available() && candidate.id == id && current(k, candidate)
             ? &candidate
             : nullptr;
}

bool has_current_semantic_variable_candidate(const Kernel &k) {
  for (const auto &candidate : k.semantic_variable_candidates)
    if (candidate.available() && current(k, candidate)) return true;
  return false;
}

std::vector<SemanticVariableCandidateInfo> semantic_variable_candidates(
    const Kernel &k) {
  std::vector<SemanticVariableCandidateInfo> result;
  for (const auto &candidate : k.semantic_variable_candidates)
    if (candidate.available() && current(k, candidate))
      result.push_back(candidate);
  return result;
}

bool make_semantic_variable_advice(const Kernel &k, uint64_t candidate_id,
                                   uint32_t existing_priority,
                                   uint32_t introduced_priority,
                                   SemanticVariableAdvice &out) {
  const auto *candidate = semantic_variable_candidate(k, candidate_id);
  if (!candidate || (!existing_priority && candidate->existing_variables) ||
      (!introduced_priority && candidate->introduced_variables))
    return false;
  SemanticVariableAdvice result;
  result.action_id = candidate->action_id;
  result.source = state_version(k);
  result.variables = candidate->variables;
  for (auto &variable : result.variables)
    variable.priority =
        variable.role == SemanticVariableRole::ExistingTheory
            ? existing_priority
            : introduced_priority;
  out = std::move(result);
  return true;
}

} // namespace tactix::strategy
