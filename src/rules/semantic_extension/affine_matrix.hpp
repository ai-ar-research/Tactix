// Exact, bounded Gaussian state for proof-before-CDCL affine reasoning.
#pragma once

#include "rules/semantic_extension/affine_image.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace tactix::affine_semantic_extension {

struct MatrixLimits {
  uint64_t maximum_work = 0;
  uint32_t maximum_rows = 0;
  uint32_t maximum_variables = 0;
  uint32_t maximum_row_width = 0;
  uint32_t maximum_sources = 0;
  uint32_t maximum_assignments = 0;
};

struct MatrixAssignment {
  External variable = 0;
  bool value = false;

  friend bool operator==(const MatrixAssignment &,
                         const MatrixAssignment &) = default;
};

struct MatrixRow {
  std::vector<External> variables;
  bool rhs = false;
  // Sorted indices into the immutable Equation vector supplied at build time.
  std::vector<size_t> source_equations;

  friend bool operator==(const MatrixRow &, const MatrixRow &) = default;
};

enum class MatrixEventKind : uint8_t { None, Implication, Conflict };

struct MatrixEvent {
  MatrixEventKind kind = MatrixEventKind::None;
  MatrixRow row;
  External implied_variable = 0;
  bool implied_value = false;
  // A globally valid clausal consequence of `row`. Assigned support literals
  // are false under the synchronized trail; an implication's final literal
  // is the implied value. A conflict contains assigned-support literals only.
  std::vector<External> reason;
};

enum class MatrixStatus : uint8_t {
  Complete,
  NoStructure,
  Exhausted,
  Interrupted,
  Invalid,
};

class AffineMatrix {
public:
  // Recondition the immutable authenticated source equations on an exact
  // trail snapshot. This deliberately supports both extension and backtrack.
  // The matrix and event remain unchanged on failure.
  MatrixStatus synchronize(
      const std::vector<MatrixAssignment> &,
      const std::function<bool(uint64_t)> &charge, MatrixEvent &event);

  uint64_t owner_identity() const { return owner_identity_; }
  uint64_t work() const { return work_; }
  const std::vector<MatrixAssignment> &trail() const { return trail_; }
  const std::vector<MatrixRow> &rows() const { return rows_; }

private:
  friend MatrixStatus build_affine_matrix(
      const std::vector<Equation> &, uint64_t, const MatrixLimits &,
      const std::function<bool(uint64_t)> &, AffineMatrix &);

  uint64_t owner_identity_ = 0;
  uint64_t work_ = 0;
  MatrixLimits limits_;
  std::vector<Equation> sources_;
  std::vector<MatrixAssignment> trail_;
  std::vector<MatrixRow> rows_;
};

// Build a source-bound matrix without changing the Boolean formula. Source
// indices are the exact algebraic provenance later returned with every event.
// `owner_identity` binds the copied equations to the caller's immutable arena.
// On every result except Complete, `output` is unchanged.
MatrixStatus build_affine_matrix(
    const std::vector<Equation> &, uint64_t owner_identity,
    const MatrixLimits &, const std::function<bool(uint64_t)> &charge,
    AffineMatrix &output);

} // namespace tactix::affine_semantic_extension
