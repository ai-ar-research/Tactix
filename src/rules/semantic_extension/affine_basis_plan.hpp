// Pure planning for a provenance-carrying affine basis and its compact
// conservative Boolean extension.  Live variable allocation and proof
// publication deliberately belong to a later kernel adapter.
#pragma once

#include "rules/semantic_extension/affine_image.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace tactix::affine_semantic_extension {

// Existing atoms name positive external variables.  Extension atoms name
// one-based, plan-local nodes; the live adapter chooses their external names.
enum class BasisAtomKind : uint8_t { Existing, Extension };

struct BasisAtom {
  BasisAtomKind kind = BasisAtomKind::Existing;
  uint32_t value = 0;

  friend bool operator==(const BasisAtom &, const BasisAtom &) = default;
  friend bool operator<(const BasisAtom &a, const BasisAtom &b) {
    if (a.kind != b.kind)
      return static_cast<uint8_t>(a.kind) < static_cast<uint8_t>(b.kind);
    return a.value < b.value;
  }
};

struct BasisLiteral {
  BasisAtom atom;
  bool positive = true;
};

using BasisClause = std::vector<BasisLiteral>;

struct XorDefinition {
  // `output = first xor second`.  Definitions are topologically ordered and
  // each output is fresh with respect to all existing variables and earlier
  // outputs.
  BasisAtom output;
  BasisAtom first;
  BasisAtom second;
  std::vector<BasisClause> clauses;
  // Indices into BasisPlan::rows whose compact image uses this node.
  std::vector<size_t> used_by_rows;
};

enum class ProjectionOperation : uint8_t {
  // Omit the fresh variable when returning a model of the original formula.
  OmitExtension,
  // To extend an original assignment, evaluate the two operands by XOR.
  EvaluateOperandXor,
};

struct ProjectionObligation {
  BasisAtom extension;
  BasisAtom first;
  BasisAtom second;
  ProjectionOperation original_projection =
      ProjectionOperation::OmitExtension;
  ProjectionOperation extension_reconstruction =
      ProjectionOperation::EvaluateOperandXor;
};

enum class BasisRowKind : uint8_t {
  DirectContradiction,
  DirectUnit,
  DirectBinary,
  DirectSmall,
  Extended,
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
  // Direct rows hold their complete parity CNF.  Extended rows hold one unit
  // asserting the root of an exact conservative XOR-definition network.
  std::vector<BasisClause> clauses;
  BasisAtom root; // Meaningful for DirectUnit and Extended rows.
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
  std::vector<XorDefinition> definitions;
  std::vector<ProjectionObligation> projection;
  uint64_t elimination_work = 0;
  uint64_t planning_work = 0;
  uint64_t input_rows = 0;
  bool original_variables_unchanged = true;
};

enum class BasisPlanStatus : uint8_t {
  Complete,
  NoStructure,
  Exhausted,
  Interrupted,
  Invalid,
};

// Reduce exact affine equations into a deterministic ascending-pivot basis and
// plan a compact Boolean image.  On every result except Complete, `output` is
// unchanged.  `charge` accounts caller-owned work and may interrupt planning.
BasisPlanStatus build_basis_plan(const std::vector<Equation> &,
                                 const BasisPlanLimits &,
                                 const std::function<bool(uint64_t)> &charge,
                                 BasisPlan &output);

// The repeated row-at-a-time slice enumerates only directly clausal rows.  It
// performs the same reduction and source accounting but skips wide rows before
// constructing extension networks, so unrelated wide rows cannot consume a
// selected direct row's node/proof allowance.
BasisPlanStatus build_direct_basis_plan(
    const std::vector<Equation> &, const BasisPlanLimits &,
    const std::function<bool(uint64_t)> &charge, BasisPlan &output);

// Enumerate every reduced row while leaving wide rows as exact
// source-carrying metadata.  A selected wide row is certified later by the
// scalable publication transaction, so unrelated rows allocate no XOR nodes.
BasisPlanStatus build_selectable_basis_plan(
    const std::vector<Equation> &, const BasisPlanLimits &,
    const std::function<bool(uint64_t)> &charge, BasisPlan &output);

} // namespace tactix::affine_semantic_extension
