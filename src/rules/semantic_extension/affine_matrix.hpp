// Exact, bounded Gaussian state for proof-before-CDCL affine reasoning.
#pragma once

#include "rules/semantic_extension/affine_image.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <vector>

namespace tactix::affine_semantic_extension {

struct MatrixLimits {
  uint64_t maximum_work = 0;
  uint32_t maximum_rows = 0;
  uint32_t maximum_variables = 0;
  uint32_t maximum_row_width = 0;
  uint32_t maximum_sources = 0;
  uint32_t maximum_assignments = 0;
  // Keep every synchronized basis in reduced row-echelon form (Gauss-Jordan,
  // CryptoMiniSat EGaussian); false keeps the forward-echelon basis.
  bool reduced_row_echelon = false;
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
  // Connected component of the immutable source equation/variable incidence
  // graph. Components are numbered by first source-row appearance. This is
  // representation metadata only: it never enters a reason or proof identity.
  uint32_t component = 0;

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

struct MatrixSliceLimits {
  uint64_t maximum_work = 0, maximum_bytes = 0;
};
struct MatrixSliceEffort {
  uint64_t work = 0, cumulative_work = 0, completed_steps = 0, minimum_slice_work = 0;
  bool resumed = false, invalidated = false;
};

struct MatrixIncrementalLimits { uint64_t maximum_index_bytes = 0; };

enum class MatrixStatus : uint8_t {
  Complete,
  Partial,
  NoStructure,
  Exhausted,
  Interrupted,
  Invalid,
  Pending, // Private reduction incomplete; no event or basis publication.
};

enum class MatrixSynchronizationKind : uint8_t {
  Extension,
  Backtrack,
  Reassignment,
  PriorPartial,
};

class AffineMatrix {
public:
  // Recondition the immutable authenticated source equations on an exact
  // trail snapshot. This deliberately supports both extension and backtrack.
  // The matrix and event remain unchanged on failure. The opt-in sorted-set
  // mode accepts new coordinates anywhere in canonical order and atomically
  // declines a bounded partial reduction; legacy callers retain prefix mode.
  MatrixStatus synchronize(
      const std::vector<MatrixAssignment> &,
      const std::function<bool(uint64_t)> &charge, MatrixEvent &event,
      bool sorted_set_extension = false,
      const MatrixIncrementalLimits *incremental = nullptr);

  // Opt-in deterministic full-basis reduction in bounded atomic row/XOR steps.
  // Pending retains private progress; exact trail equality resumes it. An
  // atomic step larger than maximum_work declines Exhausted with its required
  // minimum rather than spinning. Interrupted clears private continuation.
  MatrixStatus synchronize_sliced(const std::vector<MatrixAssignment> &,
      const std::function<bool(uint64_t)> &, MatrixEvent &,
      const MatrixSliceLimits &);
  const MatrixSliceEffort &last_slice_effort() const { return slice_effort_; }
  uint64_t continuation_bytes() const { return continuation_.bytes; }
  bool continuation_pending() const { return continuation_.active; }
  bool last_synchronization_work_exhausted() const {
    return last_synchronization_work_exhausted_;
  }
  void cancel_continuation() { continuation_ = {}; }

  // Existentially project unavailable external variables. Derivation indices
  // still cite the original authenticated equations; no assignment is guessed
  // for an eliminated identity. Failure leaves the previous matrix unchanged.
  MatrixStatus project(const std::vector<External> &unavailable,
                       const std::function<bool(uint64_t)> &charge);

  uint64_t owner_identity() const { return owner_identity_; }
  uint64_t incremental_index_bytes() const { return index_reservation_; }
  size_t last_dirty_rows() const { return last_dirty_rows_; }
  bool last_dirty_update() const { return last_dirty_update_; }
  size_t basis_rank() const noexcept { return basis_rank_; }
  uint64_t work() const { return work_; }
  uint64_t row_width_bound() const { return limits_.maximum_row_width; }
  uint64_t row_source_bound() const { return limits_.maximum_sources; }
  bool complete_admission() const { return complete_admission_; }
  const std::vector<MatrixAssignment> &trail() const { return trail_; }
  const std::vector<MatrixRow> &rows() const { return rows_; }
  const std::vector<MatrixRow> &admitted_rows() const {
    return admitted_rows_;
  }
  const std::vector<size_t> &admitted_sources() const {
    return admitted_sources_;
  }
  // Immutable source image copied when the matrix is built. Same-stack proof
  // planning may consume this owner instead of comparing a session duplicate.
  const std::vector<Equation> &authenticated_sources() const {
    return sources_;
  }
  bool proof_sources_authenticated() const {
    return proof_sources_authenticated_;
  }
  // Sorted external variables that can affect any admitted row. Search-time
  // capture need not inspect assignments outside this exact support.
  const std::vector<External> &admitted_variables() const {
    return admitted_variables_;
  }
  bool last_synchronization_incremental() const {
    return last_synchronization_incremental_;
  }
  size_t last_synchronization_prefix() const {
    return last_synchronization_prefix_;
  }
  size_t last_synchronization_conditioned() const {
    return last_synchronization_conditioned_;
  }
  MatrixSynchronizationKind last_synchronization_kind() const {
    return last_synchronization_kind_;
  }
  bool last_synchronization_restored_predecessor() const {
    return last_synchronization_restored_predecessor_;
  }

  // Authenticate one event against the exact immutable owner and source
  // image used to build this matrix.  The event must also be the current
  // conflict-first/implication-second result for the synchronized trail.
  bool authenticates(uint64_t owner_identity,
                     const std::vector<Equation> &exact_sources,
                     const MatrixEvent &event,
                     const std::function<bool(uint64_t)> &charge) const;

  // Authenticate a previously selected implication or conflict directly from
  // its immutable source rows.  Unlike `authenticates`, this does not require
  // the event to remain the current tableau event after later assignments.
  // It is the proof-side prerequisite for any future lazy reason: the complete
  // falsifying clause must name the exact XOR support and violate its parity.
  bool authenticates_historical(
      uint64_t owner_identity, const std::vector<Equation> &exact_sources,
      const MatrixEvent &event,
      const std::function<bool(uint64_t)> &charge) const;

  // Same semantic check and exact work currency as `authenticates`, for a
  // caller already holding `authenticated_sources()`.  The immutable source
  // image and current row index were authenticated when they were committed,
  // so this returns their pre-accounted cost instead of walking them again.
  // A false result is observational only; callers fall back to the full audit.
  bool retained_authentication_work(const MatrixEvent &event,
                                    uint64_t &work) const;

private:
  struct Continuation {
    bool active = false;
    uint64_t bytes = 0, base_bytes = 0, payload_bytes = 0, cumulative_work = 0;
    External combine_pivot = 0;
    std::vector<MatrixAssignment> trail;
    std::map<External, bool> assignments;
    std::map<External, MatrixRow> basis;
    std::vector<MatrixRow> contradictions, rows;
    MatrixRow row;
    MatrixEvent event;
    size_t assignment = 0, seed = 0, scan = 0, reason_source = 0, reason_trail = 0;
    // Source support with odd-parity flags; `residue` counts odd entries.
    std::map<External, bool> support;
    size_t residue = 0;
    uint8_t phase = 0;
    bool row_loaded = false;
  };
  Continuation continuation_;
  MatrixSliceEffort slice_effort_;
  struct RowIndex {
    std::map<External, std::vector<size_t>> occurrences;
    std::map<External, size_t> pivots, singletons;
    std::set<size_t> conflicts;
    uint64_t bytes = 0, authentication_work = 0;
  };
  MatrixStatus build_row_index(const std::vector<MatrixRow> &,
      const std::function<bool(uint64_t)> &, uint64_t, RowIndex &);
  MatrixStatus synchronize_dirty(const std::vector<MatrixAssignment> &,
      const std::vector<MatrixAssignment> &, const std::function<bool(uint64_t)> &,
      uint64_t, uint64_t, uint64_t &, MatrixEvent &);
  MatrixEvent indexed_event() const;
  RowIndex index_;
  // Reused only by atomic dirty-row transactions.  Their capacities are
  // admitted as part of index_reservation_ and released with the index.
  std::vector<External> dirty_variable_xor_scratch_;
  std::vector<size_t> dirty_source_xor_scratch_;
  // Direct occurrence watches mark rows touched by newly assigned variables.
  // This pre-admitted workspace avoids allocating one tree node per touched
  // row on every search step.  Epochs discard marks from an interrupted
  // private transaction without changing the committed matrix state.
  std::vector<uint64_t> dirty_row_marks_;
  std::vector<size_t> dirty_row_ids_;
  uint64_t dirty_row_epoch_ = 0;
  uint64_t index_reservation_ = 0;
  bool index_ready_ = false, last_dirty_update_ = false;
  size_t last_dirty_rows_ = 0;
  friend MatrixStatus build_affine_matrix(
      const std::vector<Equation> &, uint64_t, const MatrixLimits &,
      const std::function<bool(uint64_t)> &, AffineMatrix &, bool);

  uint64_t source_width_bound_ = 0, source_support_bound_ = 0;
  uint64_t source_authentication_work_ = 0;
  size_t basis_rank_ = 0;
  uint64_t owner_identity_ = 0;
  uint64_t work_ = 0;
  bool complete_admission_ = false;
  bool proof_sources_authenticated_ = false;
  MatrixLimits limits_;
  std::vector<Equation> sources_;
  // Immutable sound row space admitted at initialization.  Search-time
  // conditioning may combine only these rows, so a skipped source can never
  // enter a reason later.
  std::vector<MatrixRow> admitted_rows_;
  std::vector<size_t> admitted_sources_;
  std::vector<External> admitted_variables_;
  std::vector<MatrixAssignment> trail_;
  std::vector<MatrixRow> rows_;
  // `rows_` is an exact conditioned basis for `trail_` only after a complete
  // reduction.  A bounded partial synchronization may still publish a sound
  // event, but the next call must rebuild from `admitted_rows_` so omitted
  // rows cannot disappear permanently.
  bool incremental_ready_ = false;
  // One complete predecessor only: moving the old basis here avoids copying
  // it on extension and permits an exact immediate-prefix backtrack restore.
  // This never retains partial reductions or an unbounded trail history.
  // The current and predecessor populations share maximum_rows and
  // maximum_assignments; immutable admitted rows remain separate as before.
  bool predecessor_ready_ = false;
  std::vector<MatrixAssignment> predecessor_trail_;
  std::vector<MatrixRow> predecessor_rows_;
  bool last_synchronization_incremental_ = false;
  bool last_synchronization_work_exhausted_ = false;
  bool last_synchronization_restored_predecessor_ = false;
  size_t last_synchronization_prefix_ = 0;
  size_t last_synchronization_conditioned_ = 0;
  MatrixSynchronizationKind last_synchronization_kind_ =
      MatrixSynchronizationKind::Extension;
};

// Build a source-bound matrix without changing the Boolean formula. Source
// indices are the exact algebraic provenance later returned with every event.
// `owner_identity` binds the copied equations to the caller's immutable arena.
// Complete means every source was processed within the bounds. Partial means a
// deterministic, nonempty sound submatrix was retained. On every other result,
// `output` is unchanged.
MatrixStatus build_affine_matrix(
    const std::vector<Equation> &, uint64_t owner_identity,
    const MatrixLimits &, const std::function<bool(uint64_t)> &charge,
    AffineMatrix &output, bool bounded_geometry = false);

} // namespace tactix::affine_semantic_extension
