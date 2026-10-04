#pragma once

#include <cstdint>
#include <vector>

namespace tactix::z3_implication_semantic {

struct Premise {
  uint64_t occurrence = 0;
  std::vector<int32_t> literals;
  uint64_t native_proof_id = 0;
};

struct Limits {
  uint64_t maximum_clauses = 0;
  uint64_t maximum_literals = 0;
  uint64_t maximum_work = 0;
  uint64_t maximum_derived_binaries = 0;
  uint32_t maximum_components = 0;
  uint32_t maximum_inputs_per_component = 0;
  uint32_t maximum_guards_per_component = 0;
};

using Charge = bool (*)(void *context, uint64_t amount);

enum class Status : uint8_t {
  Recognized,
  NotRecognized,
  LimitExceeded,
  Interrupted,
};

struct Guard {
  int32_t consequence = 0;
  uint64_t premise = 0;
};

struct FixedFalseInput {
  int32_t input = 0;
  uint64_t premise = 0;
};

// RUP consequence of {-input,root} and {-root,consequence}.
struct BinaryConsequence {
  int32_t trigger = 0;
  int32_t consequence = 0;
  uint64_t forward_premise = 0;
  uint64_t guard_premise = 0;
};

// Complete conservative definition root <-> Or(inputs), together with every
// supplied root -> literal guard.  Keeping this compact component is normally
// preferable to publishing its Cartesian product of binary consequences.
struct Component {
  int32_t root = 0;
  std::vector<int32_t> inputs;
  uint64_t backward_premise = 0;
  std::vector<uint64_t> forward_premises;
  std::vector<FixedFalseInput> fixed_false_inputs;
  std::vector<int32_t> active_inputs;
  std::vector<uint64_t> active_forward_premises;
  // The definition's backward clause after removing fixed-false inputs.
  std::vector<int32_t> reduced_backward;
  std::vector<uint64_t> reduced_backward_rup_premises;
  std::vector<Guard> guards;
  uint64_t derived_binary_count = 0;
};

struct Image {
  std::vector<Component> components;
  std::vector<Premise> classified_premises;
  uint64_t inspected_work = 0;
};

// `source` must be a closed collection of complete positive-input OR Tseitin
// definitions, optional negative units on unambiguously owned inputs, and
// root-guarded binary implications.  Every clause is consumed exactly once and
// `out` changes only on complete success.
Status recognize(const std::vector<Premise> &source, const Limits &limits,
                 Charge charge, void *charge_context, Image &out);

// Retrieves one binary consequence and its two-premise RUP witness without
// materializing the potentially large input-by-guard Cartesian product.
bool consequence_at(const Component &component, uint32_t input_index,
                    uint32_t guard_index, BinaryConsequence &out);

} // namespace tactix::z3_implication_semantic
