#pragma once

#include "strategy/rule_invocation.hpp"

#include <cstdint>
#include <vector>

namespace tactix::strategy {

// A producer classifies identities by meaning. Only variables that represent
// a declared theory fact are eligible for branch advice; encoding and
// proof-only helpers remain implementation details.
enum class SemanticVariableRole : uint8_t {
  ExistingTheory,
  IntroducedTheory,
  EncodingHelper,
  ProofOnly,
};

struct SemanticVariable {
  uint32_t external = 0;
  SemanticVariableRole role = SemanticVariableRole::ExistingTheory;
  uint32_t priority = 1;
};

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

// Controller-owned application at a propagated root. Validation and resource
// accounting complete before either ranking changes, so every decline has no
// effect. The same advice is installed in VMTF and EVSIDS regardless of the
// currently active mode.
SemanticVariableAdviceResult apply_semantic_variable_advice(
    Kernel &, const SemanticVariableAdvice &,
    const SemanticVariableAdviceLimits &);

} // namespace tactix::strategy
