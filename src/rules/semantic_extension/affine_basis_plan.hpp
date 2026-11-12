// Planning of a provenance-carrying affine basis and its direct clauses.
#pragma once

#include "rules/semantic_extension/affine_image.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace tactix::affine_semantic_extension {

// A literal over a positive external variable.
struct BasisLiteral {
  uint32_t variable = 0;
  bool positive = true;
};

using BasisClause = std::vector<BasisLiteral>;

enum class BasisRowKind : uint8_t {
  DirectContradiction, DirectUnit, DirectBinary, DirectSmall, Extended
};

struct BasisRow {
  std::vector<External> variables;
  bool rhs = false;
  // Sorted indices into the input Equation vector.  XORing precisely these
  // equations yields this row.
  std::vector<size_t> source_equations;
  // Sorted unique live-occurrence identities needed by those sources.
  std::vector<uint64_t> premises;
  BasisRowKind kind = BasisRowKind::DirectSmall;
  // Direct rows hold their complete parity CNF; Extended rows hold none.
  std::vector<BasisClause> clauses;
};

struct BasisPlanLimits {
  uint64_t maximum_work = 0;
  uint32_t maximum_intermediate_width = 0;
  uint32_t maximum_sources = 0;
  uint32_t maximum_premises = 0;
  uint32_t maximum_nodes = 0;
  // Widths at most this bound are emitted directly.  The bound is restricted
  // to [2,8], matching the bounded clausal parity representation.
  uint32_t maximum_direct_width = 0;
};

struct BasisPlan {
  std::vector<BasisRow> rows;
  uint64_t elimination_work = 0;
  uint64_t planning_work = 0;
  uint64_t input_rows = 0;
};

enum class BasisPlanStatus : uint8_t {
  Complete, NoStructure, Exhausted, Interrupted, Invalid
};

// Reduce equations to an ascending-pivot basis and plan its direct rows' CNF;
// wide rows stay source metadata. `output` changes only on Complete.
BasisPlanStatus build_selectable_basis_plan(
    const std::vector<Equation> &, const BasisPlanLimits &,
    const std::function<bool(uint64_t)> &charge, BasisPlan &output);

} // namespace tactix::affine_semantic_extension
