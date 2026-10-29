// Opt-in persistent GF(2) restart service built above the authenticated live
// publisher. Initialization, retained maintenance and publication are root
// actions inside a full-restart window; implications remain ordinary BCP.
#pragma once

#include "rules/semantic_extension/affine_matrix_live.hpp"
#include "rules/semantic_extension/affine_root_image.hpp"
#include "strategy/theory_action_parameters.hpp"

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

// Diagnostic attribution for bounded initialization declines. It does not
// change admission or proof semantics.
enum class AffineMatrixInitResourceStage : uint8_t {
  None,
  Snapshot,
  RootImage,
  RootCensus,
  RootPreflight,
  Matrix,
  Retained,
};

// Public controller actions admit any authenticated affine subimage and may
// retain a bounded partial matrix.  DefaultPolicy can impose the stronger
// policy admission below before scheduling this relatively expensive service:
// every captured clause must belong to the image and every recovered equation
// must enter the matrix.  This changes scheduling only, never rule soundness.
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



// Zero retains the legacy live-source contract. A positive byte allowance
// requests checker-premise ownership across native deletion/simplification.
// This bounds retained representations, not the solver's total process heap.
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
  // The explicit deferred-search action installed a provisional clausal
  // reason whose checker certificate is owed; the explicit materialization
  // action discharged that debt.
  bool reason_deferred = false, reason_materialized = false;
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
  // Policy-owned scheduling may discard an unused session without touching
  // the formula or proof. Published Boolean clauses remain ordinary clauses;
  // this only releases cached algebraic/proof-planning state.
  void disable() { *this = AffineMatrixSearchSession{}; }
  // Persistent owners release leases only with an explicitly live Kernel.
  // The legacy clear is lifetime-safe even after the Kernel has gone away.
  void disable(Kernel &);
  // The typed result of the last public AffineMatrixInitialize application
  // (initialization/refresh or service step): its public observation.
  const std::optional<AffineMatrixInitResult> &last_initialization() const {
    return last_initialization_;
  }
  const std::optional<AffineMatrixServiceResult> &last_service() const { return last_service_; }
  bool belongs_to(const Kernel &kernel) const { return owner_ == &kernel; }
  // True while the installed search session is still bound to the same
  // Kernel, provenance sink and proof stream.  Formula/search changes do not
  // invalidate it: the service reauthenticates exact source clauses at every
  // publication boundary.
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
    return deferred > UINT64_MAX - owned ? UINT64_MAX : owned + deferred;
  }
  bool matches_profile(const strategy::AffineConsequenceParameters &basis,
      const AffineMatrixPersistenceLimits &limits) const {
    return parameters_ == basis && persistence_.maximum_retained_bytes == limits.maximum_retained_bytes &&
      persistence_.maximum_deferred_deletions == limits.maximum_deferred_deletions &&
      persistence_.maximum_deferred_literal_work == limits.maximum_deferred_literal_work;
  }
  size_t published_keys() const { return published_.size(); }
  uint64_t synchronization_allowance() const {
    return parameters_.basis_maximum_work;
  }
  // A terminal initialization failure is stable while both the root formula
  // and its explicit resource profile are unchanged.  The policy can then
  // stop offering the same expensive no-op until useful state or allowance
  // changes.
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
      Kernel &, AffineMatrixSearchSession &, bool, bool);
  friend AffineMatrixServiceResult affine_matrix_search_step_deferred(
      Kernel &, AffineMatrixSearchSession &);
  friend AffineMatrixServiceResult affine_matrix_search_step_detached_minimal(
      Kernel &, AffineMatrixSearchSession &);
  friend AffineMatrixServiceResult affine_matrix_materialize_reason(
      Kernel &, AffineMatrixSearchSession &);
  friend AffineMatrixAliasResult affine_matrix_search_preserve_aliases(
      Kernel &, AffineMatrixSearchSession &,
      const std::vector<std::pair<Lit, Lit>> &);
  friend AffineMatrixInitResult affine_matrix_search_refresh(
      Kernel &, AffineMatrixSearchSession &,
      const strategy::AffineConsequenceParameters &,
      const AffineMatrixPersistenceLimits &, bool);

  void remember_terminal_initialization_failure(
      const Kernel &, const strategy::AffineConsequenceParameters &);

  const Kernel *owner_ = nullptr;
  uint64_t owner_version_ = 0;
  // Owner whose complete exact source table was written to structural
  // provenance.  Later owner refreshes must emit sources inline until a new
  // retained table has been committed.
  uint64_t retained_source_provenance_owner_ = 0;
  Proof *proof_ = nullptr;
  uint64_t proof_stream_identity_ = 0;
  ProofFormat proof_format_ = ProofFormat::DratAscii;
  FILE *records_ = nullptr;
  strategy::AffineConsequenceParameters parameters_;
  AffineLiveSnapshot source_owner_;
  // Discovery has an independent bounded census; the admitted system never
  // needs original physical sources again. Pending IDs denote only unresolved
  // populations, not equation premises or proof authority.
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
  bool cached_event_valid_ = false;
  bool slice_retry_=false;
  std::vector<std::pair<Var, uint32_t>> capture_index_;
  std::vector<affine_semantic_extension::MatrixAssignment> capture_trail_;
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
    affine_semantic_extension::MatrixEvent event;
    affine_semantic_extension::MatrixLiveRequest request;
    affine_semantic_extension::AffineMatrixLivePrepared prepared;
    std::string event_identity;
    Ref provisional_ref = INVALID_REF;
    Var implied_variable = INVALID_VAR;
  };
  std::optional<DeferredReason> deferred_reason_;
};

RuleEligibility affine_matrix_initialize_guard(const Kernel &);
AffineMatrixInitResult affine_matrix_search_initialize(
    Kernel &, AffineMatrixSearchSession &,
    const strategy::AffineConsequenceParameters &,
    AffineMatrixAdmission = AffineMatrixAdmission::AuthenticatedSubimage,
    const AffineMatrixPersistenceLimits & = {});
ApplyResult affine_matrix_initialize_apply(Kernel &);

// One deterministic conflict-before-implication event at an exact Boolean
// propagation fixpoint.  No direct enqueue or conflict is permitted.
AffineMatrixServiceResult affine_matrix_search_step(
    Kernel &, AffineMatrixSearchSession &);
// Explicit public alternatives to the eager SearchStep.  At most one wide
// implication may be deferred; its exact certificate is preflighted before
// assignment, and MaterializeReason must publish it before any reason consumer.
AffineMatrixServiceResult affine_matrix_search_step_deferred(
    Kernel &, AffineMatrixSearchSession &);
// Eagerly certify the exact XOR-core reason, but keep a non-root large
// implication reason detached from the watched Boolean formula. This public
// operation isolates reason lifetime/attachment; it is not the later lazy
// `(matrix,row)` handle mechanism.
AffineMatrixServiceResult affine_matrix_search_step_detached_minimal(
    Kernel &, AffineMatrixSearchSession &);
AffineMatrixServiceResult affine_matrix_materialize_reason(
    Kernel &, AffineMatrixSearchSession &);
AffineMatrixAliasResult affine_matrix_search_preserve_aliases(
    Kernel &, AffineMatrixSearchSession &,
    const std::vector<std::pair<Lit, Lit>> &);
// Scheduling advice only (no rule semantics): whether a retained session's
// GF(2) inputs changed since `cursor` was last consumed. Inputs are root facts
// on the admitted support, retirement of a support variable (substitution,
// elimination, compaction), owner or premise-lease changes, and an unfinished
// slice. Each root-trail literal is scanned once; `work` counts the scan.
struct AffineMatrixInputCursor {
  uint64_t root_scanned = 0, namespace_epoch = 0, owner_version = 0;
  uint64_t premise_epoch = 0;
  bool valid = false;
};
bool affine_matrix_search_inputs_changed(const Kernel &,
    const AffineMatrixSearchSession &, AffineMatrixInputCursor &, uint64_t &work);
void affine_matrix_search_consume_inputs(const Kernel &,
    const AffineMatrixSearchSession &, AffineMatrixInputCursor &);
// Root-only bounded additional recognition; successful persistent admission
// extends the retained source system rather than replacing deleted sources.
AffineMatrixInitResult affine_matrix_search_refresh(
    Kernel &, AffineMatrixSearchSession &,
    const strategy::AffineConsequenceParameters &,
    const AffineMatrixPersistenceLimits &, bool allow_additional_recognition = true);

} // namespace tactix
