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
  ResourceExhausted,
  Interrupted,
  Stale,
  Applied,
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
  uint64_t rows = 0;
};

struct AffineMatrixServiceResult {
  AffineMatrixServiceStatus status = AffineMatrixServiceStatus::Disabled;
  ApplyResult result = ApplyResult::NoOp;
  std::string event_identity;
  uint64_t synchronization_work = 0;
  uint64_t session_work = 0;
  uint64_t reason_width = 0;
  affine_semantic_extension::MatrixLiveResult live;
  bool bcp_pending = false;

  bool terminal_failure() const {
    return status == AffineMatrixServiceStatus::Inapplicable ||
           status == AffineMatrixServiceStatus::ResourceExhausted ||
           status == AffineMatrixServiceStatus::Interrupted ||
           status == AffineMatrixServiceStatus::UnsupportedProof ||
           status == AffineMatrixServiceStatus::Stale ||
           status == AffineMatrixServiceStatus::Invalid;
  }
};

class AffineMatrixSearchSession {
public:
  bool enabled() const { return owner_ != nullptr; }
  bool belongs_to(const Kernel &kernel) const { return owner_ == &kernel; }
  uint64_t owner_version() const { return owner_version_; }
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
      const strategy::AffineConsequenceParameters &);
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
    const strategy::AffineConsequenceParameters &);
ApplyResult affine_matrix_initialize_apply(Kernel &);

// One deterministic conflict-before-implication event at an exact Boolean
// propagation fixpoint.  No direct enqueue or conflict is permitted.
AffineMatrixServiceResult affine_matrix_search_step(
    Kernel &, AffineMatrixSearchSession &);

} // namespace tactix
