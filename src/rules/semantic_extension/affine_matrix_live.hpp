// Live publication of one preflighted persistent affine-matrix reason.
#pragma once

#include "rules/semantic_extension/affine_live.hpp"
#include "rules/semantic_extension/affine_matrix_transaction.hpp"
#include "proof/proof.hpp"

#include <cstdio>
#include <string>

namespace tactix::affine_semantic_extension {

enum class MatrixLiveStatus : uint8_t {
  Prepared,
  Applied,
  AlreadyApplied,
  Invalid,
  Inapplicable,
  UnsupportedOutcome,
  UnsupportedProof,
  ResourceExhausted,
  Interrupted,
  Stale,
};

struct MatrixLiveRequest {
  const AffineLiveSnapshot *owner = nullptr;
  const AffineMatrix *matrix = nullptr;
  const std::vector<Equation> *exact_sources = nullptr;
  const MatrixEvent *event = nullptr;
  uint64_t owner_identity = 0;
  MatrixTransactionLimits transaction;
  uint64_t maximum_revalidation_work = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t action_id = 0;
  uint64_t semantic_id = 0;
  FILE *records = nullptr;
  bool compact_provenance = false;
  bool retained_source_provenance = false;
  // Pure certificate preflight for an implication/conflict retained from an
  // earlier matrix synchronization point.  The complete source-derived
  // clause is authenticated independently of the matrix's current tableau;
  // no publication semantics are relaxed by this flag.
  bool historical_event = false;
  // Prepare the current event normally, but retain enough immutable identity
  // for a later publication after its implication has been consumed.  This is
  // used only by the explicit deferred/materialize public action pair.
  bool retained_publication = false;
  AffineMatrixProofCache *proof_cache = nullptr;
  PbpParityBoundsCache *pbp_parity_cache = nullptr;
  // Optional exact per-equation checker-premise ownership. Absence preserves
  // the legacy live native-source authentication boundary.
  const std::vector<uint64_t> *premise_leases = nullptr;
  uint64_t maximum_persistent_bytes = 0;
};

struct MatrixLiveResult {
  MatrixLiveStatus status = MatrixLiveStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t planner_work = 0;
  uint64_t revalidation_work = 0;
  uint64_t selected_sources = 0;
  uint64_t selected_premises = 0;
  uint64_t preflight_additions = 0;
  uint64_t preflight_deletions = 0;
  uint64_t additions = 0;
  uint64_t deletions = 0;
  uint32_t helper_extent = 0;
  uint64_t arena_words = 0;
  uint64_t journal_work = 0;
  uint64_t preflight_journal_bytes = 0;
  uint64_t journal_bytes = 0;
  bool implication = false;
  bool conflict = false;
  // A non-root implication is assigned immediately from its newly installed
  // reason clause. Propagation remains pending for that literal's watch lists.
  bool implication_consumed = false;
  bool propagation_pending = false;
  bool proof_cache_hit = false;
  bool proof_cache_insert = false;
  bool proof_cache_resume = false;
  bool compact_pbp = false;
  // The separately callable publication reused the matrix's authenticated
  // retained state after proving that its exact support trail was unchanged.
  bool retained_authentication = false;
  uint64_t installed_reason_id = 0;
  Ref installed_reason_ref = INVALID_REF;
  // Publication audit boundary for a declined two-phase transaction.  This is
  // diagnostic evidence only; status remains the public semantic outcome.
  // Values are stable for campaign attribution and append-only telemetry.
  enum class AuditStage : uint8_t {
    None = 0,
    PreparedContract = 1,
    ProofSupport = 2,
    Applicability = 3,
    KernelSnapshot = 4,
    TrailOrEvent = 5,
    AuthenticationOrSources = 6,
    SourceIdentity = 7,
    ReasonOrder = 8,
    PbpPlan = 9,
    MatrixAuthentication = 10,
    SelectedSources = 11,
    ResumeSources = 12,
  } audit_stage = AuditStage::None;
};

class AffineMatrixLivePrepared {
public:
  AffineMatrixLivePrepared() = default;
  AffineMatrixLivePrepared(const AffineMatrixLivePrepared &) = delete;
  AffineMatrixLivePrepared &operator=(const AffineMatrixLivePrepared &) = delete;
  AffineMatrixLivePrepared(AffineMatrixLivePrepared &&) = default;
  AffineMatrixLivePrepared &operator=(AffineMatrixLivePrepared &&) = default;

  bool ready() const { return ready_; }
  bool spent() const { return spent_; }
  const AffineMatrixTransaction &transaction() const { return transaction_; }
  const std::string &journal() const { return journal_; }
  const std::vector<Lit> &physical_reason() const { return physical_reason_; }
  uint64_t retained_bytes() const;

private:
  friend MatrixLiveResult prepare_affine_matrix_live(
      Kernel &, const MatrixLiveRequest &, AffineMatrixLivePrepared &);
  friend MatrixLiveResult publish_affine_matrix_live(
      Kernel &, const MatrixLiveRequest &, AffineMatrixLivePrepared &);
  friend MatrixLiveResult apply_affine_matrix_live(
      Kernel &, const MatrixLiveRequest &);

  bool ready_ = false;
  bool spent_ = false;
  bool immediate_ = false;
  MatrixLiveRequest request_;
  uint64_t starting_clause_id_ = 0;
  uint64_t arena_generation_ = 0;
  uint32_t starting_nvars_ = 0;
  uint32_t starting_extent_ = 0;
  uint32_t starting_level_ = 0;
  bool had_proof_ = false;
  ProofFormat proof_format_ = ProofFormat::DratAscii;
  Proof *proof_ = nullptr;
  long records_position_ = -1;
  std::vector<MatrixAssignment> trail_;
  // Exact matrix state authenticated during prepare.  A retained deferred
  // publication may let the Boolean trail grow, but the public debt grammar
  // forbids another matrix step.  Equality here therefore permits the same
  // aggregate retained authentication used by the one-shot path; a changed
  // matrix trail falls back to the full historical source audit.
  std::vector<MatrixAssignment> matrix_trail_;
  std::vector<uint32_t> trail_levels_;
  MatrixEvent event_;
  AffineLiveSnapshot owner_;
  std::vector<uint64_t> source_occurrences_;
  std::vector<uint64_t> source_clause_ids_;
  std::vector<uint64_t> resume_source_occurrences_;
  std::vector<uint64_t> resume_source_clause_ids_;
  std::vector<Lit> physical_reason_;
  std::vector<PbpParityEquation> pbp_equations_;
  PbpParityReasonPlan pbp_plan_;
  AffineMatrixTransaction transaction_;
  std::string journal_;
  MatrixLiveResult prepared_;
};

// Both phases are mutation-free until publish has revalidated every retained
// identity and resource boundary. Publication emits the preflighted proof,
// installs its marked reason, and directly consumes a non-root implication;
// ordinary propagation remains pending for the new literal's watch lists.
MatrixLiveResult prepare_affine_matrix_live(
    Kernel &, const MatrixLiveRequest &, AffineMatrixLivePrepared &);
MatrixLiveResult publish_affine_matrix_live(
    Kernel &, const MatrixLiveRequest &, AffineMatrixLivePrepared &);
MatrixLiveResult apply_affine_matrix_live(Kernel &, const MatrixLiveRequest &);

} // namespace tactix::affine_semantic_extension
