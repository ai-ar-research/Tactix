#pragma once
#include <cstdint>
namespace tactix {
struct Kernel;
namespace strategy {
// A bounded discovery-and-proof request, not a hidden scheduling policy.
// Zero limits disable work; no production allowance is selected here.
struct ParityConsequenceParameters {
  uint64_t maximum_clauses = 0, maximum_groups = 0, selection_work = 0;
  uint32_t intermediate_width = 0, source_count = 0, output_width = 0;
  bool descending = false;
  uint64_t maximum_proof_additions = 0;
  uint32_t maximum_tree_variables = 0;
  uint64_t consequence = 0;
  bool valid() const { return output_width <= 4; }
  static ParityConsequenceParameters from_kernel(const Kernel &);
};
void install_rule_parameters(Kernel &, const ParityConsequenceParameters &);
} // namespace strategy
} // namespace tactix
