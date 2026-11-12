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
  // Reduced row-echelon basis (CryptoMiniSat EGaussian); else forward echelon.
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
  // Source incidence component, numbered by first source-row appearance;
  // never part of a reason or proof identity.
  uint32_t component = 0;

  friend bool operator==(const MatrixRow &, const MatrixRow &) = default;
};

enum class MatrixEventKind : uint8_t { None, Implication, Conflict };

struct MatrixEvent {
  MatrixEventKind kind = MatrixEventKind::None;
  MatrixRow row;
  External implied_variable = 0;
  bool implied_value = false;
  // Clausal consequence of `row`: support literals false under the trail; an
  // implication ends with the implied literal.
  std::vector<External> reason;
  // Deferred exact-core support as a bitset over `admitted_variables()`;
  // source-support reasons never use it.
  std::vector<uint64_t> deferred_exact_support;
  uint32_t reason_width = 0;

  friend bool operator==(const MatrixEvent &, const MatrixEvent &) = default;
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
  Extension, Backtrack, Reassignment, PriorPartial
};

// Telemetry for an Invalid synchronization; never affects admission or work.
enum class MatrixSynchronizationFailureStage : uint8_t {
  None = 0,
  Request = 1,
  TrailOrder = 2,
  TrailSupport = 3,
  SimplexPreparation = 4,
  CachedAssignment = 5,
  DeferredReason = 6,
  SourceIdentity = 7,
};

class AffineMatrix {
public:
  // Recondition the source equations on an exact trail snapshot (extension or
  // backtrack); unchanged on failure. Sorted-set mode accepts new coordinates
  // anywhere in canonical order and declines a partial reduction.
  MatrixStatus synchronize(
      const std::vector<MatrixAssignment> &,
      const std::function<bool(uint64_t)> &charge, MatrixEvent &event,
      bool sorted_set_extension = false,
      const MatrixIncrementalLimits *incremental = nullptr,
      bool exact_core_reason = false,
      bool defer_exact_reason = false);

  // Full-basis reduction in bounded atomic row/XOR steps. Pending keeps
  // progress for the same trail; a step above maximum_work is Exhausted with
  // its minimum; Interrupted clears the continuation.
  MatrixStatus synchronize_sliced(const std::vector<MatrixAssignment> &,
      const std::function<bool(uint64_t)> &, MatrixEvent &,
      const MatrixSliceLimits &, bool exact_core_reason = false,
      bool defer_exact_reason = false);
  const MatrixSliceEffort &last_slice_effort() const { return slice_effort_; }
  uint64_t continuation_bytes() const { return continuation_.bytes; }
  bool continuation_pending() const { return continuation_.active; }
  bool last_synchronization_work_exhausted() const {
    return last_synchronization_work_exhausted_;
  }
  MatrixSynchronizationFailureStage last_synchronization_failure_stage() const {
    return last_synchronization_failure_stage_;
  }
  void cancel_continuation() { continuation_ = {}; }

  // Existentially project unavailable variables; derivations still cite the
  // source equations. Unchanged on failure.
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
  const std::vector<MatrixRow> &rows() const;
  size_t row_count() const {
    return simplex_.active ? simplex_.rhs.size() : rows_.size();
  }
  const std::vector<MatrixRow> &admitted_rows() const {
    return admitted_rows_;
  }
  const std::vector<size_t> &admitted_sources() const {
    return admitted_sources_;
  }
  // Source image copied at build time.
  const std::vector<Equation> &authenticated_sources() const {
    return sources_;
  }
  bool proof_sources_authenticated() const {
    return proof_sources_authenticated_;
  }
  // Sorted variables that occur in any admitted row.
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

  // Authenticate an event against this matrix's owner and source image; it
  // must be the current conflict-first event for the synchronized trail.
  bool authenticates(uint64_t owner_identity,
                     const std::vector<Equation> &exact_sources,
                     const MatrixEvent &event,
                     const std::function<bool(uint64_t)> &charge) const;

  // Authenticate an earlier event from its source rows; it need not be
  // current. The clause must contain a complete falsifying XOR-support core
  // and may weaken it with extra literals.
  bool authenticates_historical(
      uint64_t owner_identity, const std::vector<Equation> &exact_sources,
      const MatrixEvent &event,
      const std::function<bool(uint64_t)> &charge) const;

  // The work `authenticates` would charge, from the pre-accounted cost of the
  // committed source image and row index; false means use the full audit.
  bool retained_authentication_work(const MatrixEvent &event,
                                    uint64_t &work) const;

  // Visit a deferred support; materialization signs it from the live
  // assignment when a reason consumer reaches it.
  bool inspect_deferred_exact_support(
      const MatrixEvent &,
      const std::function<bool(External)> &) const;
  bool materialize_deferred_exact_reason(
      MatrixEvent &,
      const std::function<int8_t(External)> &) const;

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
    bool row_loaded = false, exact_core_reason = false,
         defer_exact_reason = false;
  };
  Continuation continuation_;
  MatrixSliceEffort slice_effort_;
  struct RowIndex {
    std::map<External, std::vector<size_t>> occurrences;
    std::map<External, size_t> pivots, singletons;
    std::set<size_t> conflicts;
    uint64_t bytes = 0, authentication_work = 0;
  };
  // Compact [I|D] simplex state (CryptoMiniSat Gaussian search): responsible
  // columns are implicit; D columns and source provenance are packed.
  struct SimplexState {
    bool ready = false, active = false, watches_ready = false;
    size_t coefficient_words = 0, source_words = 0;
    size_t row_words = 0, variable_words = 0;
    uint64_t bytes = 0, authentication_work = 0;
    std::vector<uint64_t> coefficients, provenance;
    std::vector<size_t> responsible, variable_to_row, variable_to_slot,
        slot_to_variable;
    std::vector<int8_t> values;
    std::vector<uint8_t> rhs;
    std::vector<uint32_t> components;
    std::vector<uint64_t> slot_unset, slot_values;
    mutable std::vector<External> reason_scratch;
    mutable std::vector<size_t> implication_rows;
    mutable std::vector<uint8_t> implication_values;
    // CryptoMiniSat-style two-coordinate row watches in flat bit matrices;
    // event bitsets keep singleton/conflict rows not yet selected.
    std::vector<uint64_t> watch_rows, singleton_rows, conflict_rows,
        active_singleton_variables, opposite_rows, pending_responsible_rows,
        touched_rows;
    std::vector<size_t> watch_first, watch_second, event_coordinates;
    std::vector<uint8_t> event_kinds, event_values;
  };
  MatrixStatus synchronize_simplex(
      const std::vector<MatrixAssignment> &,
      const std::function<bool(uint64_t)> &, MatrixEvent &,
      const MatrixIncrementalLimits &, bool);
  bool simplex_layout(uint64_t, size_t &, size_t &, uint64_t &) const;
  bool prepare_simplex(uint64_t, SimplexState &) const;
  void simplex_evaluate_row(size_t, size_t &, size_t &, bool &,
                            size_t * = nullptr, size_t * = nullptr) const;
  bool simplex_row_contains(size_t row, size_t coordinate) const;
  size_t simplex_first_singleton(size_t coordinate, bool value) const;
  size_t simplex_opposite_singleton(size_t coordinate) const;
  void simplex_make_core_reason(size_t, size_t, MatrixEvent &) const;
  void simplex_make_deferred_core_support(size_t, size_t,
                                          MatrixEvent &) const;
  bool make_deferred_exact_support(MatrixEvent &) const;
  MatrixEvent simplex_build_event(size_t, size_t, MatrixEventKind, External,
                                  bool, bool, bool) const;
  void simplex_remove_row_index(size_t);
  void simplex_refresh_row_index(size_t);
  void simplex_rebuild_index();
  MatrixEvent simplex_indexed_event(bool, bool = false) const;
  MatrixEvent simplex_event(bool exact_core_reason,
                            bool defer_exact_reason = false) const;
  void materialize_simplex_rows() const;
  void reset_sparse_after_simplex();
  void clear_row_index();
  void record_synchronization(MatrixSynchronizationKind, bool incremental,
                              bool restored_predecessor, size_t prefix,
                              size_t conditioned, bool dirty_update,
                              size_t dirty_rows);
  SimplexState simplex_;
  mutable std::vector<MatrixRow> simplex_rows_cache_;
  mutable bool simplex_rows_cache_valid_ = false;
  MatrixStatus build_row_index(const std::vector<MatrixRow> &,
      const std::function<bool(uint64_t)> &, uint64_t, RowIndex &);
  MatrixStatus synchronize_dirty(const std::vector<MatrixAssignment> &,
      const std::vector<MatrixAssignment> &, const std::function<bool(uint64_t)> &,
      uint64_t, uint64_t, uint64_t &, MatrixEvent &, bool exact_core_reason,
      bool defer_exact_reason);
  MatrixEvent indexed_event() const;
  RowIndex index_;
  // Dirty-row transaction scratch, admitted within index_reservation_.
  std::vector<External> dirty_variable_xor_scratch_;
  std::vector<size_t> dirty_source_xor_scratch_;
  // Rows touched by newly assigned variables; epochs discard the marks of an
  // interrupted transaction.
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
  // Row space admitted at build; conditioning combines only these rows.
  std::vector<MatrixRow> admitted_rows_;
  std::vector<size_t> admitted_sources_;
  std::vector<External> admitted_variables_;
  std::vector<MatrixAssignment> trail_;
  std::vector<MatrixRow> rows_;
  // `rows_` is an exact basis for `trail_` only after a complete reduction;
  // after a partial one the next call rebuilds from `admitted_rows_`.
  bool incremental_ready_ = false;
  // One complete predecessor basis for an immediate-prefix backtrack restore;
  // it shares maximum_rows and maximum_assignments with the current basis.
  bool predecessor_ready_ = false;
  std::vector<MatrixAssignment> predecessor_trail_;
  std::vector<MatrixRow> predecessor_rows_;
  bool last_synchronization_incremental_ = false;
  bool last_synchronization_work_exhausted_ = false;
  MatrixSynchronizationFailureStage last_synchronization_failure_stage_ =
      MatrixSynchronizationFailureStage::None;
  bool last_synchronization_restored_predecessor_ = false;
  size_t last_synchronization_prefix_ = 0;
  size_t last_synchronization_conditioned_ = 0;
  MatrixSynchronizationKind last_synchronization_kind_ =
      MatrixSynchronizationKind::Extension;
};

// Build a matrix over `owner_identity`'s equations; source indices are the
// provenance of every event. Complete processed every source; Partial kept a
// nonempty sound submatrix; any other result leaves `output` unchanged.
MatrixStatus build_affine_matrix(
    const std::vector<Equation> &, uint64_t owner_identity,
    const MatrixLimits &, const std::function<bool(uint64_t)> &charge,
    AffineMatrix &output, bool bounded_geometry = false);

} // namespace tactix::affine_semantic_extension
