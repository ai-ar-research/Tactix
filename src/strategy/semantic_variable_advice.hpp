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

// Controller-owned application at a conflict-free root. It may run while root
// propagation is pending, which lets a producer publish Boolean consequences,
// expose its semantic variables, and leave Propagate as the learner's next
// separately accounted action. Validation and resource accounting complete
// before either ranking changes, so every decline has no effect. The same
// advice is installed in VMTF and EVSIDS regardless of the active mode.
//
// `commit_semantic_variable_advice` leaves epoch ownership to registry dispatch.
// The direct wrapper below advances it once on success for non-registry callers.
SemanticVariableAdviceResult commit_semantic_variable_advice(
    Kernel &, const SemanticVariableAdvice &,
    const SemanticVariableAdviceLimits &);
SemanticVariableAdviceResult apply_semantic_variable_advice(
    Kernel &, const SemanticVariableAdvice &,
    const SemanticVariableAdviceLimits &);

// Proof-rule adapters stage neutral producer declarations during their atomic
// apply. Registry dispatch publishes them only after its single strategy-epoch
// transition, so a learner can observe and select advice in that exact state.
void begin_semantic_variable_candidate_action(Kernel &);
bool stage_semantic_variable_candidate(
    Kernel &, SemanticVariableProducer, uint64_t action_id,
    const std::vector<SemanticVariable> &);
void finish_semantic_variable_candidate_action(Kernel &, ApplyResult);

const SemanticVariableCandidate *semantic_variable_candidate(
    const Kernel &, uint64_t id);
bool has_current_semantic_variable_candidate(const Kernel &);
std::vector<SemanticVariableCandidateInfo> semantic_variable_candidates(
    const Kernel &);

// Resolve one current candidate into a state-bound advice request. Role doses
// are controller choices; producers publish neutral priority one.
bool make_semantic_variable_advice(const Kernel &, uint64_t candidate_id,
                                   uint32_t existing_priority,
                                   uint32_t introduced_priority,
                                   SemanticVariableAdvice &);

} // namespace tactix::strategy
