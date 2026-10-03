// Internal atomic adapter; public action/scheduling integration remains pending.
#pragma once
#include "rules/lucky/parity_selection.hpp"
#include "rules/registry.hpp"
#include <cstdio>
namespace tactix {
struct ParityLiveRequest {
  parity_premises::Limits premises;
  parity_premises::SelectionLimits selection;
  bool descending;
  uint64_t maximum_proof_additions;
  uint32_t maximum_tree_variables;
  size_t consequence;
  uint64_t action_id;
  FILE *records; // Caller-owned streamed semantic/proof provenance, mandatory.
};
struct ParityLiveResult {
  ApplyResult result = ApplyResult::NoOp;
  uint64_t clauses = 0, equations = 0, work = 0;
  uint64_t additions = 0, deletions = 0, published = 0;
  uint32_t helpers = 0;
};
ParityLiveResult parity_live_apply(Kernel &, const ParityLiveRequest &);
} // namespace tactix
