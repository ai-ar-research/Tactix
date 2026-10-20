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
  bool propagation_pending = false;
  bool proof_cache_hit = false;
  bool proof_cache_insert = false;
  bool proof_cache_resume = false;
  bool compact_pbp = false;
  uint64_t installed_reason_id = 0;
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
// identity and resource boundary. Publication emits the preflighted proof and
// installs only its marked reason. Ordinary propagation remains pending.
MatrixLiveResult prepare_affine_matrix_live(
    Kernel &, const MatrixLiveRequest &, AffineMatrixLivePrepared &);
MatrixLiveResult publish_affine_matrix_live(
    Kernel &, const MatrixLiveRequest &, AffineMatrixLivePrepared &);
MatrixLiveResult apply_affine_matrix_live(Kernel &, const MatrixLiveRequest &);

} // namespace tactix::affine_semantic_extension
