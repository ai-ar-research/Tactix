#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

namespace tactix {

// Internal composition, not yet a registered action. A semantic producer owns
// the meaning/premise record named by semantic_id. This helper certifies only
// the conservative Boolean definition, never an asserted theory consequence.
struct DisjunctionDefinitionRequest {
  std::vector<int32_t> inputs; // signed, stable external DIMACS identities
  uint32_t maximum_arity = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t maximum_additions = 0;
  uint64_t action_id = 0;
  uint64_t semantic_id = 0;
  FILE *records = nullptr;
};

struct DisjunctionDefinitionResult {
  ApplyResult result = ApplyResult::NoOp;
  int32_t output = 0;
  uint64_t first_clause_id = 0;
  uint64_t additions = 0;
  uint64_t literal_occurrences = 0;
  uint64_t inspected_inputs = 0;
};

// Introduce fresh p iff OR(inputs), leaving ordinary propagation pending.
// A successful call is atomic with respect to termination; all limits are
// checked before mutation. No phase, score, scheduling, or duplicate policy.
DisjunctionDefinitionResult define_disjunction(
    Kernel &, const DisjunctionDefinitionRequest &);

} // namespace tactix
