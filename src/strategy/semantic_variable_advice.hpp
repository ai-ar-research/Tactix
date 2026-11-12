#pragma once

#include "strategy/semantic_variable.hpp"
#include "strategy/rule_invocation.hpp"

#include <cstdint>
#include <vector>

namespace tactix::strategy {

struct SemanticVariableAdvice {
  uint64_t action_id = 0;
  StateVersion source;
  std::vector<SemanticVariable> variables;
};

struct SemanticVariableAdviceLimits {
  uint32_t maximum_variables = 0;
  uint64_t maximum_priority_sum = 0;
};

enum class SemanticVariableAdviceStatus : uint8_t {
  Applied,
  NoChange,
  Invalid,
  Stale,
  ResourceExhausted,
  Inapplicable,
};

struct SemanticVariableAdviceResult {
  SemanticVariableAdviceStatus status = SemanticVariableAdviceStatus::Invalid;
  uint32_t variables = 0;
  uint64_t priority_sum = 0;
};

// Controller-owned application at a conflict-free root, possibly with
// propagation pending, so a producer can publish consequences and leave
// Propagate as the next action. Validation and accounting finish before either
// ranking changes, so declines have no effect; VMTF and EVSIDS get the same
// advice. Registry dispatch owns the epoch transition.
SemanticVariableAdviceResult commit_semantic_variable_advice(
    Kernel &, const SemanticVariableAdvice &,
    const SemanticVariableAdviceLimits &);

// Staged during apply; published after the epoch transition.
void begin_semantic_variable_candidate_action(Kernel &);
bool stage_semantic_variable_candidate(
    Kernel &, SemanticVariableProducer, uint64_t action_id,
    const std::vector<SemanticVariable> &);
// Carry a staged declaration across a composed settlement.
bool take_staged_semantic_variable_candidate(Kernel &,
                                             SemanticVariableCandidate &);
void finish_semantic_variable_candidate_action(Kernel &, ApplyResult);

const SemanticVariableCandidate *semantic_variable_candidate(
    const Kernel &, uint64_t id);
bool has_current_semantic_variable_candidate(const Kernel &);
std::vector<SemanticVariableCandidateInfo> semantic_variable_candidates(
    const Kernel &);

// Resolve one candidate into a state-bound advice request.
bool make_semantic_variable_advice(const Kernel &, uint64_t candidate_id,
                                   uint32_t existing_priority,
                                   uint32_t introduced_priority,
                                   SemanticVariableAdvice &);

} // namespace tactix::strategy
