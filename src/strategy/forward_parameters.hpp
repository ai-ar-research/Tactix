#pragma once

#include "strategy/scoped_rule_parameters.hpp"

#include <cstdint>

namespace tactix {

struct Kernel;

namespace strategy {

// Canonical-round discovery costs. These are strategy choices, never
// preconditions on an explicit legal C1 action. UINT32_MAX removes a cap.
struct SubsumeForwardParameters {
  static constexpr uint32_t CLAUSE_SIZE_DEFAULT = 1000;
  static constexpr uint32_t LARGE_OCCURRENCE_DEFAULT = 100;
  static constexpr uint32_t BINARY_OCCURRENCE_DEFAULT = 10000;

  uint32_t clause_size_limit = CLAUSE_SIZE_DEFAULT;
  uint32_t large_occurrence_limit = LARGE_OCCURRENCE_DEFAULT;
  uint32_t binary_occurrence_limit = BINARY_OCCURRENCE_DEFAULT;

  static SubsumeForwardParameters from_kernel(const Kernel &);
  constexpr bool valid() const { return true; }
};

// Kissat's canonical fused-forward discovery costs. Explicit C2 instances
// scan their finite input and therefore bypass both strategy limits.
struct StrengthenParameters {
  static constexpr uint32_t CLAUSE_SIZE_DEFAULT = 1000;
  static constexpr uint32_t OCCURRENCE_DEFAULT = 1000;

  uint32_t clause_size_limit = CLAUSE_SIZE_DEFAULT;
  uint32_t occurrence_limit = OCCURRENCE_DEFAULT;

  static StrengthenParameters from_kernel(const Kernel &);
  constexpr bool valid() const { return true; }
};

void install_rule_parameters(Kernel &, const SubsumeForwardParameters &);
void install_rule_parameters(Kernel &, const StrengthenParameters &);

using ScopedSubsumeForwardParameters = ScopedRuleParameters<SubsumeForwardParameters>;

using ScopedStrengthenParameters = ScopedRuleParameters<StrengthenParameters>;

} // namespace strategy
} // namespace tactix
