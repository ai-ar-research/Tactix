#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tactix::strategy {

enum class SemanticVariableRole : uint8_t {
  ExistingTheory,
  IntroducedTheory,
  EncodingHelper,
  ProofOnly,
};

enum class SemanticVariableProducer : uint8_t {
  Affine = 0,
  ExactCover = 1,
  IndependentSet = 2,
  Gf3Affine = 3,
  DirectCsp = 4,
  NaryCsp = 5,
  NaryCspEquality = 6,
  Count = 7,
};

inline constexpr size_t SEMANTIC_VARIABLE_PRODUCER_COUNT =
    static_cast<size_t>(SemanticVariableProducer::Count);

inline constexpr bool valid_semantic_variable_producer(
    SemanticVariableProducer producer) {
  return static_cast<size_t>(producer) < SEMANTIC_VARIABLE_PRODUCER_COUNT;
}

struct SemanticVariable {
  uint32_t external = 0;
  SemanticVariableRole role = SemanticVariableRole::ExistingTheory;
  uint32_t priority = 1;
  bool operator==(const SemanticVariable &) const = default;
};

struct SemanticVariableCandidateInfo {
  uint64_t id = 0;
  uint64_t action_id = 0;
  uint64_t arena_generation = 0;
  uint64_t touch_epoch = 0;
  uint64_t strategy_epoch = 0;
  SemanticVariableProducer producer = SemanticVariableProducer::Affine;
  uint32_t existing_variables = 0;
  uint32_t introduced_variables = 0;
  bool operator==(const SemanticVariableCandidateInfo &) const = default;
};

struct SemanticVariableCandidate : SemanticVariableCandidateInfo {
  std::vector<SemanticVariable> variables;
  bool available() const { return id && !variables.empty(); }
};

} // namespace tactix::strategy
