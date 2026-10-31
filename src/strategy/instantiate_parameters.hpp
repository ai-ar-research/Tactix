#pragma once

#include "strategy/scoped_rule_parameters.hpp"

#include <cstdint>

namespace tactix {

struct Kernel;

namespace strategy {

// Canonical X4 discovery policy.  Explicit semantic (clause,literal)
// instances bypass these limits; they are not soundness preconditions.
struct InstantiateParameters {
  static constexpr uint32_t CLAUSE_SIZE_DEFAULT = 3;
  static constexpr uint32_t OCCURRENCE_DEFAULT = 1;
  static constexpr uint32_t UNASSIGNED_DEFAULT = 3;
  static constexpr uint32_t NEW_MATERIAL_DEFAULT = 1;

  // Zero removes the size/unassigned lower bound. UINT32_MAX removes the
  // occurrence upper bound. Learned clauses stay outside the reference
  // canonical round unless the caller opts in.
  uint32_t minimum_clause_size = CLAUSE_SIZE_DEFAULT;
  uint32_t occurrence_limit = OCCURRENCE_DEFAULT;
  uint32_t minimum_unassigned = UNASSIGNED_DEFAULT;
  bool include_redundant = false;
  uint32_t new_material_threshold = NEW_MATERIAL_DEFAULT;

  static InstantiateParameters from_kernel(const Kernel &);
  constexpr bool valid() const { return true; }
};

void install_rule_parameters(Kernel &, const InstantiateParameters &);

using ScopedInstantiateParameters = ScopedRuleParameters<InstantiateParameters>;

} // namespace strategy
} // namespace tactix
