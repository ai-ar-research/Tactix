// Persistent GF(2) service over the live publisher. Initialization,
// maintenance and publication are root actions in a full-restart window.
#pragma once

#include "rules/semantic_extension/affine_matrix_live.hpp"
#include "rules/semantic_extension/affine_root_image.hpp"
#include "strategy/theory_action_parameters.hpp"

#include <cstdlib>
#include <string>
#include <optional>
#include <utility>

namespace tactix {

enum class AffineMatrixInitStatus : uint8_t {
  Invalid,
  Inapplicable,
  NoStructure,
  IncompleteImage,
  IncompleteMatrix,
  ResourceExhausted,
  Interrupted,
  Stale,
  Applied,
  AppliedPartial,
  Pending, // Retained Gaussian slice incomplete; resume at a later restart.
};

// Diagnostic attribution for initialization declines.
enum class AffineMatrixInitResourceStage : uint8_t {
  None,
  Snapshot,
  RootImage,
  RootCensus,
  RootPreflight,
  Matrix,
  Retained,
};

// Actions admit any authenticated affine subimage and may keep a partial
// matrix. A policy may require that every captured clause belongs to the image
// and every equation enters the matrix; this affects scheduling only.
enum class AffineMatrixAdmission : uint8_t {
  AuthenticatedSubimage,
  CompleteSnapshot,
};

enum class AffineMatrixServiceStatus : uint8_t {
  Disabled,
  NoEvent,
  Duplicate,
  Applied,
  Inapplicable,
  ResourceExhausted,
  Interrupted,
  UnsupportedProof,
  Stale,
  Invalid,
  Pending,
};



// A positive byte allowance owns checker premises across native deletion and
// simplification; zero relies on live sources.
struct AffineMatrixPersistenceLimits {
  uint64_t maximum_retained_bytes = 0;
  uint64_t maximum_deferred_deletions = 0;
  uint64_t maximum_deferred_literal_work = 0;
};

struct AffineMatrixServiceResult {
  AffineMatrixServiceStatus status = AffineMatrixServiceStatus::Disabled;
  ApplyResult result = ApplyResult::NoOp;
  std::string event_identity;
  uint64_t synchronization_work = 0;
  uint64_t synchronization_nanos = 0, publication_nanos = 0;
  uint64_t trail_capture_work = 0;
  bool synchronized = false;
  bool synchronization_incremental = false;
  bool synchronization_restored_predecessor = false;
  uint64_t synchronization_prefix = 0;
  uint64_t synchronization_conditioned = 0;
  affine_semantic_extension::MatrixSynchronizationKind synchronization_kind =
      affine_semantic_extension::MatrixSynchronizationKind::Extension;
  affine_semantic_extension::MatrixSynchronizationFailureStage
      synchronization_failure_stage =
          affine_semantic_extension::MatrixSynchronizationFailureStage::None;
  uint64_t session_work = 0;
  uint64_t reason_width = 0;
  uint64_t lifecycle_work = 0;
  uint64_t retired_source_groups = 0;
  uint64_t projected_variables = 0;
  std::vector<int32_t> installed_reason;
  affine_semantic_extension::MatrixLiveResult live;
  bool bcp_pending = false;
  bool incremental_capture = false, unchanged_support = false, cached_event = false;
  uint64_t capture_delta_literals = 0;
  bool dirty_row_update = false;
  uint64_t dirty_rows = 0;
  // A deferred search step owes a certificate; materialization discharges it.
  bool reason_deferred = false, reason_materialized = false;
  // Exact support became a signed reason vector in this call.
  bool reason_vector_constructed = false;
  bool slice_pending=false, slice_resumed=false, slice_invalidated=false;
  uint64_t slice_work=0, slice_cumulative_work=0, slice_completed_steps=0, slice_minimum_work=0;

  bool terminal_failure() const {
    return status == AffineMatrixServiceStatus::ResourceExhausted ||
           status == AffineMatrixServiceStatus::Interrupted ||
           status == AffineMatrixServiceStatus::UnsupportedProof ||
           status == AffineMatrixServiceStatus::Invalid;
  }
};

struct AffineMatrixAliasResult {
  AffineMatrixServiceStatus status = AffineMatrixServiceStatus::Disabled;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t work = 0, proof_additions = 0, retained_aliases = 0, journal_bytes = 0;
};

struct AffineMatrixInitResult {
  AffineMatrixInitStatus status = AffineMatrixInitStatus::Invalid;
  AffineMatrixInitResourceStage resource_stage = AffineMatrixInitResourceStage::None;
  uint64_t resource_required_bytes = 0, resource_limit_bytes = 0;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t owner_version = 0;
  uint64_t snapshot_clauses = 0;
  uint64_t snapshot_literals = 0;
  uint64_t snapshot_work = 0;
  uint64_t recognition_work = 0;
  uint64_t normalization_rup_checks = 0, shortening_additions = 0;
  uint64_t discovery_recognized_groups=0, discovery_skipped_groups=0, discovery_recognizer_work=0;
  uint64_t matrix_work = 0;
  uint64_t equations = 0;
  uint64_t covered_clauses = 0;
  uint64_t rows = 0;
  uint64_t admitted_sources = 0;
  uint64_t journal_bytes = 0;
  uint64_t retained_bytes = 0;
  uint64_t premise_retention_work = 0;
  bool unchanged = false;
  // RootMaintenance executes retained GE, independently of discovery.
  uint64_t root_reason_additions = 0, maintenance_service_work = 0;
  std::optional<AffineMatrixServiceResult> root_service;
  uint64_t root_unit_additions = 0, root_unit_work = 0;
  AffineMatrixInitStatus additional_recognition_status = AffineMatrixInitStatus::Applied;
};

class AffineMatrixSearchSession {
public:
  bool enabled() const { return owner_ != nullptr; }
  // Release cached algebraic state; formula and proof are unchanged.
  void disable() {
    // A live lazy handle still needs this owner; use disable(Kernel &).
    if (!lazy_reasons_.empty()) return;
    *this = AffineMatrixSearchSession{};
  }
  // Releases persistent leases on a live Kernel.
  void disable(Kernel &);
  // The result of the last AffineMatrixInitialize application.
  const std::optional<AffineMatrixInitResult> &last_initialization() const {
    return last_initialization_;
  }
  const std::optional<AffineMatrixServiceResult> &last_service() const { return last_service_; }
  bool belongs_to(const Kernel &kernel) const { return owner_ == &kernel; }
  // Bound to the same Kernel, provenance sink and proof stream; sources are
  // reauthenticated at every publication.
  bool attached_to(const Kernel &) const;
  uint64_t owner_version() const { return owner_version_; }
  bool complete_admission() const { return matrix_.complete_admission(); }
  bool persistent() const { return persistence_.maximum_retained_bytes != 0; }
  const strategy::AffineConsequenceParameters &parameters() const { return parameters_; }
  const AffineMatrixPersistenceLimits &persistence_limits() const { return persistence_; }
  size_t active_sources() const { return image_.equations.size(); }
  size_t basis_rank() const noexcept { return matrix_.basis_rank(); }
  size_t support_variables() const noexcept { return matrix_.admitted_variables().size(); }
  const std::vector<affine_semantic_extension::External> &support() const noexcept {
    return matrix_.admitted_variables();
  }
  // A retained Gaussian slice is unfinished and resumes at the next service.
  bool slice_pending() const noexcept {
    return slice_retry_ || matrix_.continuation_pending();
  }
  bool has_deferred_reason() const noexcept {
    return deferred_reason_.has_value();
  }
  size_t lazy_reason_count() const noexcept { return lazy_reasons_.size(); }
  uint32_t lowest_lazy_reason_level(const Kernel &kernel) const noexcept;
  // Called by kernel_unassign_literal to retire the variable's lazy handle.
  void discard_lazy_reason(Var implied) noexcept;
  bool has_lazy_reason(Var implied) const noexcept {
    for (const auto &reason : lazy_reasons_)
      if (reason.implied_variable == implied)
        return true;
    return false;
  }
  // Read-only ancestry variables for ConflictAnalysis; keeps the handle.
  bool inspect_lazy_reason(const Kernel &, Var,
                           std::vector<Var> &);
  uint64_t retained_bytes() const {
    const uint64_t continuation = matrix_.continuation_bytes();
    const uint64_t proof = pbp_proof_cache_.retained_bytes();
    if (continuation > UINT64_MAX - retained_bytes_)
      return UINT64_MAX;
    const uint64_t base = retained_bytes_ + continuation;
    const uint64_t owned = proof > UINT64_MAX - base ? UINT64_MAX : base + proof;
    const uint64_t deferred = deferred_reason_
        ? deferred_reason_->prepared.retained_bytes()
        : 0;
    uint64_t total = deferred > UINT64_MAX - owned ? UINT64_MAX
                                                    : owned + deferred;
#ifndef NDEBUG
    uint64_t lazy = 0;
    for (const auto &reason : lazy_reasons_)
      lazy = reason.retained_bytes > UINT64_MAX - lazy
          ? UINT64_MAX : lazy + reason.retained_bytes;
    assert(lazy == lazy_reason_bytes_);
#endif
    total = lazy_reason_bytes_ > UINT64_MAX - total
        ? UINT64_MAX : total + lazy_reason_bytes_;
    return total;
  }
  bool matches_profile(const strategy::AffineConsequenceParameters &basis,
      const AffineMatrixPersistenceLimits &limits) const {
    return parameters_ == basis && persistence_.maximum_retained_bytes == limits.maximum_retained_bytes &&
      persistence_.maximum_deferred_deletions == limits.maximum_deferred_deletions &&
      persistence_.maximum_deferred_literal_work == limits.maximum_deferred_literal_work;
  }
  uint64_t synchronization_allowance() const {
    return parameters_.basis_maximum_work;
  }
  // A terminal initialization failure repeats while the root formula and
  // resource profile are unchanged.
  bool initialization_retry_suppressed(
      const Kernel &,
      const strategy::AffineConsequenceParameters &) const;

private:
  friend ApplyResult affine_matrix_initialize_apply(Kernel &);
  friend AffineMatrixInitResult affine_matrix_search_initialize(
      Kernel &, AffineMatrixSearchSession &,
      const strategy::AffineConsequenceParameters &, AffineMatrixAdmission,
      const AffineMatrixPersistenceLimits &);
  friend AffineMatrixServiceResult affine_matrix_search_step(
      Kernel &, AffineMatrixSearchSession &);
  friend AffineMatrixServiceResult affine_matrix_search_step_impl(
      Kernel &, AffineMatrixSearchSession &, bool, bool, bool, bool);
  friend AffineMatrixServiceResult affine_matrix_search_step_deferred(
      Kernel &, AffineMatrixSearchSession &);
  friend AffineMatrixServiceResult affine_matrix_search_step_detached_minimal(
      Kernel &, AffineMatrixSearchSession &);
  friend AffineMatrixServiceResult affine_matrix_search_step_detached_source(
      Kernel &, AffineMatrixSearchSession &);
  friend AffineMatrixServiceResult affine_matrix_search_step_lazy_source(
      Kernel &, AffineMatrixSearchSession &);
  friend AffineMatrixServiceResult affine_matrix_search_step_lazy_minimal(
      Kernel &, AffineMatrixSearchSession &);
  friend AffineMatrixServiceResult affine_matrix_materialize_reason(
      Kernel &, AffineMatrixSearchSession &);
  friend AffineMatrixServiceResult affine_matrix_materialize_lazy_reason(
      Kernel &, AffineMatrixSearchSession &, Var, bool);
  friend AffineMatrixAliasResult affine_matrix_search_preserve_aliases(
      Kernel &, AffineMatrixSearchSession &,
      const std::vector<std::pair<Lit, Lit>> &);
  friend AffineMatrixInitResult affine_matrix_search_refresh(
      Kernel &, AffineMatrixSearchSession &,
      const strategy::AffineConsequenceParameters &,
      const AffineMatrixPersistenceLimits &, bool);

  void remember_terminal_initialization_failure(
      const Kernel &, const strategy::AffineConsequenceParameters &);
  // A live publication request against this session's owner and caches.
  affine_semantic_extension::MatrixLiveRequest matrix_live_request(
      const Kernel &, const affine_semantic_extension::MatrixEvent &,
      bool core_reason_authentication);
  // Retain one newly published event identity at its sorted position.
  void publish_identity(std::vector<std::string>::iterator,
                        const std::string &);

  const Kernel *owner_ = nullptr;
  uint64_t owner_version_ = 0;
  // Owner whose source table is in structural provenance; other owners emit
  // sources inline.
  uint64_t retained_source_provenance_owner_ = 0;
  Proof *proof_ = nullptr;
  uint64_t proof_stream_identity_ = 0;
  StructuralJournal records_;
  strategy::AffineConsequenceParameters parameters_;
  AffineLiveSnapshot source_owner_;
  // Discovery census; pending IDs are unresolved populations only.
  const Kernel *discovery_owner_=nullptr;
  const Proof *discovery_proof_=nullptr;
  uint64_t discovery_stream_=0, discovery_clause_id_=0, discovery_bytes_=0;
  std::vector<uint64_t> discovery_known_binaries_, discovery_pending_large_, discovery_pending_binaries_;
  affine_semantic_extension::RootScopeCensus discovery_census_;
  affine_semantic_extension::Image image_;
  affine_semantic_extension::AffineMatrix matrix_;
  affine_semantic_extension::AffineMatrixProofCache proof_cache_;
  PbpParityBoundsCache pbp_proof_cache_;
  std::vector<std::string> published_;
  uint64_t published_work_ = 0, published_capacity_bytes_ = 0;
  affine_semantic_extension::MatrixEvent cached_event_;
  uint64_t cached_event_owner_ = 0, cached_event_bytes_ = 0;
  bool cached_event_valid_ = false, cached_event_exact_core_ = false,
       cached_event_deferred_exact_ = false;
  bool slice_retry_=false;
  std::vector<std::pair<Var, uint32_t>> capture_index_;
  std::vector<affine_semantic_extension::MatrixAssignment> capture_trail_;
  // Incremental-capture scratch within the preflight's change-vector reserve.
  std::vector<std::pair<affine_semantic_extension::External, bool>>
      capture_changes_;
  uint64_t capture_owner_ = 0, capture_namespace_epoch_ = 0, capture_retraction_epoch_ = 0;
  uint64_t capture_cursor_ = 0, capture_bytes_ = 0;
  bool capture_valid_ = false;
  AffineMatrixPersistenceLimits persistence_;
  uint64_t retained_bytes_ = 0, matrix_index_bytes_ = 0, matrix_row_bytes_ = 0;
  uint64_t premise_epoch_ = 0;
  std::vector<uint64_t> premise_leases_;
  std::vector<affine_semantic_extension::External> projected_variables_;
  uint64_t refresh_arena_generation_ = 0, refresh_touch_epoch_ = 0;
  uint64_t refresh_fixed_head_ = 0, refresh_clause_id_ = 0, refresh_namespace_epoch_ = 0;
  uint64_t refresh_root_assignments_ = 0, refresh_retraction_epoch_ = 0;
  const Kernel *failed_owner_ = nullptr;
  uint64_t failed_arena_generation_ = 0;
  uint64_t failed_touch_epoch_ = 0;
  uint64_t failed_touch_fixed_head_ = 0;
  uint64_t failed_trail_size_ = 0;
  uint64_t failed_next_clause_id_ = 0;
  strategy::AffineConsequenceParameters failed_parameters_;
  bool have_terminal_initialization_failure_ = false;
  std::optional<AffineMatrixInitResult> last_initialization_;
  std::optional<AffineMatrixServiceResult> last_service_;
  struct DeferredReason {
    DeferredReason() = default;

    affine_semantic_extension::MatrixEvent event;
    affine_semantic_extension::MatrixLiveRequest request;
    affine_semantic_extension::AffineMatrixLivePrepared prepared;
    std::string event_identity;
    Ref provisional_ref = INVALID_REF;
    Var implied_variable = INVALID_VAR;
  };
  std::optional<DeferredReason> deferred_reason_;
  struct LazyReason {
    affine_semantic_extension::MatrixEvent event;
    std::string event_identity;
    Var implied_variable = INVALID_VAR;
    uint64_t retained_bytes = 0;
    bool core_reason_authentication = false;
  };
  std::vector<LazyReason> lazy_reasons_;
  uint64_t lazy_reason_bytes_ = 0;
  void erase_lazy_reason_at(size_t) noexcept;
};

RuleEligibility affine_matrix_initialize_guard(const Kernel &);
AffineMatrixInitResult affine_matrix_search_initialize(
    Kernel &, AffineMatrixSearchSession &,
    const strategy::AffineConsequenceParameters &,
    AffineMatrixAdmission = AffineMatrixAdmission::AuthenticatedSubimage,
    const AffineMatrixPersistenceLimits & = {});
ApplyResult affine_matrix_initialize_apply(Kernel &);

// One conflict-before-implication event at a propagation fixpoint.
AffineMatrixServiceResult affine_matrix_search_step(
    Kernel &, AffineMatrixSearchSession &);
// Defers at most one wide implication, preflighted before assignment;
// MaterializeReason publishes it before any reason consumer.
AffineMatrixServiceResult affine_matrix_search_step_deferred(
    Kernel &, AffineMatrixSearchSession &);
// Eagerly certified XOR-core reason, detached from the watched formula when
// it is a non-root large implication.
AffineMatrixServiceResult affine_matrix_search_step_detached_minimal(
    Kernel &, AffineMatrixSearchSession &);
// As above with the source-support weakening as the explanation.
AffineMatrixServiceResult affine_matrix_search_step_detached_source(
    Kernel &, AffineMatrixSearchSession &);
// Install a compact trail handle; proof and publication wait for
// MaterializeReasons or ConflictAnalysis. A consumed source-support reason
// stays a watched clause (deviation from CryptoMiniSat).
AffineMatrixServiceResult affine_matrix_search_step_lazy_source(
    Kernel &, AffineMatrixSearchSession &);
// As above with the exact XOR-core explanation.
AffineMatrixServiceResult affine_matrix_search_step_lazy_minimal(
    Kernel &, AffineMatrixSearchSession &);
AffineMatrixServiceResult affine_matrix_materialize_reason(
    Kernel &, AffineMatrixSearchSession &);
// The seven entries above as one public service operation (SearchStep through
// LazyMinimalSearchStep, and MaterializeReasons); any other operation aborts.
inline AffineMatrixServiceResult affine_matrix_search_service(
    Kernel &k, AffineMatrixSearchSession &session,
    strategy::AffineMatrixInitializeParameters::Operation operation) {
  using Operation = strategy::AffineMatrixInitializeParameters::Operation;
  switch (operation) {
  case Operation::SearchStep: return affine_matrix_search_step(k, session);
  case Operation::DeferredSearchStep: return affine_matrix_search_step_deferred(k, session);
  case Operation::DetachedMinimalSearchStep:
    return affine_matrix_search_step_detached_minimal(k, session);
  case Operation::DetachedSourceSearchStep:
    return affine_matrix_search_step_detached_source(k, session);
  case Operation::LazySourceSearchStep: return affine_matrix_search_step_lazy_source(k, session);
  case Operation::LazyMinimalSearchStep: return affine_matrix_search_step_lazy_minimal(k, session);
  case Operation::MaterializeReasons: return affine_matrix_materialize_reason(k, session);
  default: break;
  }
  std::abort();
}
// Materialize one handle: the oldest for MaterializeReasons, each ancestor
// handle for ConflictAnalysis.
AffineMatrixServiceResult affine_matrix_materialize_lazy_reason(
    Kernel &, AffineMatrixSearchSession &, Var, bool honor_termination = true);
AffineMatrixAliasResult affine_matrix_search_preserve_aliases(
    Kernel &, AffineMatrixSearchSession &,
    const std::vector<std::pair<Lit, Lit>> &);
// Scheduling advice: whether the session's GF(2) inputs (root facts on the
// support, retired support variables, owner or lease changes, an unfinished
// slice) changed since `cursor`. `work` counts the root-trail scan.
struct AffineMatrixInputCursor {
  uint64_t root_scanned = 0, namespace_epoch = 0, owner_version = 0;
  uint64_t premise_epoch = 0;
  bool valid = false;
};
bool affine_matrix_search_inputs_changed(const Kernel &,
    const AffineMatrixSearchSession &, AffineMatrixInputCursor &, uint64_t &work);
void affine_matrix_search_consume_inputs(const Kernel &,
    const AffineMatrixSearchSession &, AffineMatrixInputCursor &);
// Root-only additional recognition; persistent admission extends the
// retained source system.
AffineMatrixInitResult affine_matrix_search_refresh(
    Kernel &, AffineMatrixSearchSession &,
    const strategy::AffineConsequenceParameters &,
    const AffineMatrixPersistenceLimits &, bool allow_additional_recognition = true);

} // namespace tactix
