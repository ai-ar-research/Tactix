// Live publication of one preflighted persistent affine-matrix reason.
#pragma once

#include "rules/semantic_extension/affine_live.hpp"
#include "rules/semantic_extension/affine_matrix_transaction.hpp"
#include "proof/proof.hpp"

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
  StructuralJournal records;
  bool compact_provenance = false;
  bool retained_source_provenance = false;
  // Certificate preflight for an event from an earlier synchronization,
  // authenticated from its sources.
  bool historical_event = false;
  // Authenticate an exact-core spelling through the source-derived audit.
  bool core_reason_authentication = false;
  // Keep identity for a publication after the implication is consumed
  // (deferred/materialize pair).
  bool retained_publication = false;
  // False inside ConflictAnalysis, which must finish a selected lazy reason.
  bool honor_termination = true;
  // Publish a non-root implication of three or more literals as a detached
  // arena record that only conflict analysis sees.
  bool detached_reason = false;
  AffineMatrixProofCache *proof_cache = nullptr;
  PbpParityBoundsCache *pbp_parity_cache = nullptr;
  // Per-equation checker-premise leases; null authenticates live sources.
  const std::vector<uint64_t> *premise_leases = nullptr;
  uint64_t maximum_persistent_bytes = 0;

  bool operator==(const MatrixLiveRequest &) const = default;
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
  // A non-root implication was assigned from its new reason; propagation of
  // its watches is pending.
  bool implication_consumed = false;
  bool propagation_pending = false;
  bool proof_cache_hit = false;
  bool proof_cache_insert = false;
  bool proof_cache_resume = false;
  bool compact_pbp = false;
  // Publication reused retained authentication for an unchanged trail.
  bool retained_authentication = false;
  bool detached_reason = false;
  uint64_t installed_reason_id = 0;
  Ref installed_reason_ref = INVALID_REF;
  MatrixTransactionFailureStage transaction_failure_stage =
      MatrixTransactionFailureStage::None;
  // Diagnostic audit stage of a declined two-phase publication (append-only).
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
    TransactionPlan = 13,
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
  Proof *proof_ = nullptr;
  long records_position_ = -1;
  std::vector<MatrixAssignment> trail_;
  // Matrix trail at prepare; equality permits retained authentication, else
  // publish runs the full source audit.
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
  std::string journal_; // spelled only for a stream; prepared_ holds its size
  MatrixLiveResult prepared_;
};

// Nothing mutates until publish revalidates every identity and bound; it then
// emits the proof, installs the reason and assigns a non-root implication.
MatrixLiveResult prepare_affine_matrix_live(
    Kernel &, const MatrixLiveRequest &, AffineMatrixLivePrepared &);
MatrixLiveResult publish_affine_matrix_live(
    Kernel &, const MatrixLiveRequest &, AffineMatrixLivePrepared &);
MatrixLiveResult apply_affine_matrix_live(Kernel &, const MatrixLiveRequest &);

// Order a matrix event's reason as the kernel's clause (no proof, no audit).
bool affine_matrix_event_reason_order(const Kernel &, const MatrixEvent &,
                                      std::vector<Lit> &);

} // namespace tactix::affine_semantic_extension
