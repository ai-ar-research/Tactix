// Opt-in persistent GF(2) search service built above the authenticated live
// publisher.  Initialization is a typed root policy action; search publication
// remains a deterministic service and leaves all implications to ordinary BCP.
#pragma once

#include "rules/semantic_extension/affine_matrix_live.hpp"
#include "strategy/theory_action_parameters.hpp"

#include <string>

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
};

struct AffineMatrixInitResult {
  AffineMatrixInitStatus status = AffineMatrixInitStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t owner_version = 0;
  uint64_t snapshot_clauses = 0;
  uint64_t snapshot_literals = 0;
  uint64_t snapshot_work = 0;
  uint64_t recognition_work = 0;
  uint64_t matrix_work = 0;
  uint64_t equations = 0;
  uint64_t covered_clauses = 0;
  uint64_t rows = 0;
  uint64_t admitted_sources = 0;
  uint64_t journal_bytes = 0;
};

struct AffineMatrixServiceResult {
  AffineMatrixServiceStatus status = AffineMatrixServiceStatus::Disabled;
  ApplyResult result = ApplyResult::NoOp;
  std::string event_identity;
  uint64_t synchronization_work = 0;
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
  affine_semantic_extension::MatrixLiveResult live;
  bool bcp_pending = false;

  bool terminal_failure() const {
    return status == AffineMatrixServiceStatus::ResourceExhausted ||
           status == AffineMatrixServiceStatus::Interrupted ||
           status == AffineMatrixServiceStatus::UnsupportedProof ||
           status == AffineMatrixServiceStatus::Invalid;
  }
};

class AffineMatrixSearchSession {
public:
  bool enabled() const { return owner_ != nullptr; }
  // Policy-owned scheduling may discard an unused session without touching
  // the formula or proof. Published Boolean clauses remain ordinary clauses;
  // this only releases cached algebraic/proof-planning state.
  void disable() { *this = AffineMatrixSearchSession{}; }
  bool belongs_to(const Kernel &kernel) const { return owner_ == &kernel; }
  // True while the installed search session is still bound to the same
  // Kernel, provenance sink and proof stream.  Formula/search changes do not
  // invalidate it: the service reauthenticates exact source clauses at every
  // publication boundary.
  bool attached_to(const Kernel &) const;
  uint64_t owner_version() const { return owner_version_; }
  bool complete_admission() const { return matrix_.complete_admission(); }
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
  friend AffineMatrixInitResult affine_matrix_search_initialize(
      Kernel &, AffineMatrixSearchSession &,
      const strategy::AffineConsequenceParameters &, AffineMatrixAdmission);
  friend AffineMatrixServiceResult affine_matrix_search_step(
      Kernel &, AffineMatrixSearchSession &);

  void remember_terminal_initialization_failure(
      const Kernel &, const strategy::AffineConsequenceParameters &);

  const Kernel *owner_ = nullptr;
  uint64_t owner_version_ = 0;
  Proof *proof_ = nullptr;
  ProofFormat proof_format_ = ProofFormat::DratAscii;
  FILE *records_ = nullptr;
  strategy::AffineConsequenceParameters parameters_;
  AffineLiveSnapshot source_owner_;
  affine_semantic_extension::Image image_;
  affine_semantic_extension::AffineMatrix matrix_;
  affine_semantic_extension::AffineMatrixProofCache proof_cache_;
  std::vector<std::string> published_;
  const Kernel *failed_owner_ = nullptr;
  uint64_t failed_arena_generation_ = 0;
  uint64_t failed_touch_epoch_ = 0;
  uint64_t failed_touch_fixed_head_ = 0;
  uint64_t failed_trail_size_ = 0;
  uint64_t failed_next_clause_id_ = 0;
  strategy::AffineConsequenceParameters failed_parameters_;
  bool have_terminal_initialization_failure_ = false;
};

RuleEligibility affine_matrix_initialize_guard(const Kernel &);
AffineMatrixInitResult affine_matrix_search_initialize(
    Kernel &, AffineMatrixSearchSession &,
    const strategy::AffineConsequenceParameters &,
    AffineMatrixAdmission = AffineMatrixAdmission::AuthenticatedSubimage);
ApplyResult affine_matrix_initialize_apply(Kernel &);

// One deterministic conflict-before-implication event at an exact Boolean
// propagation fixpoint.  No direct enqueue or conflict is permitted.
AffineMatrixServiceResult affine_matrix_search_step(
    Kernel &, AffineMatrixSearchSession &);

} // namespace tactix
