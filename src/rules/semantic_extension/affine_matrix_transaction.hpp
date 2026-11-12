// Pure proof preflight for one authenticated affine-matrix event.
#pragma once

#include "rules/semantic_extension/affine_matrix.hpp"
#include "rules/lucky/parity_certificate.hpp"
#include "proof/proof.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace tactix::affine_semantic_extension {

enum class MatrixTransactionStatus : uint8_t {
  Prepared,
  Invalid,
  Stale,
  UnsupportedProof,
  ResourceExhausted,
  Interrupted,
};

enum class MatrixTransactionFailureStage : uint8_t {
  None = 0,
  Request = 1,
  CoordinateFrame = 2,
  DenseMapping = 3,
  CachePopulation = 4,
  CertificatePlanning = 5,
  CertificateSemantics = 6,
  ProofPopulation = 7,
};

struct MatrixTransactionLimits {
  uint64_t maximum_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_deletions = 0;
  uint32_t maximum_helper_extent = 0;
  uint64_t maximum_arena_words = 0;

  friend bool operator==(const MatrixTransactionLimits &,
                         const MatrixTransactionLimits &) = default;
};

struct MatrixProofEvent {
  enum class Kind : uint8_t { Rat, Rup, Delete } kind = Kind::Rup;
  std::vector<External> clause;
  uint64_t id = 0;
  uint64_t deleted_id = 0;
  // Exactly one retained RUP reason is marked. Helpers are proof-only.
  bool install = false;

  friend bool operator==(const MatrixProofEvent &,
                         const MatrixProofEvent &) = default;
};

struct MatrixProofCacheEntry {
  // Monotone per session; provenance records the full entry once.
  uint64_t entry_id = 0;
  uint64_t owner_identity = 0;
  uint64_t proof_epoch = 0;
  uint32_t external_extent = 0;
  std::vector<size_t> source_equations;
  std::vector<Equation> source_identity;
  std::vector<uint64_t> support_ids;
  std::vector<External> coordinates;
  parity_certificate::ActiveTree tree;
  uint64_t parent_entry_id = 0;
};

// Proof-only RAT definitions and their root unit kept in the DSR checker
// population across service calls; DSR retires them at the next witnessed
// add_sr. Entry IDs are never reused; the variables are never branchable.
class AffineMatrixProofCache {
public:
  uint64_t generation() const { return generation_; }
  size_t size() const { return entries_.size(); }
  uint64_t retained_bytes() const;
  const std::vector<MatrixProofCacheEntry> &entries() const { return entries_; }
  const MatrixProofCacheEntry *find(
      uint64_t, uint64_t proof_epoch, const std::vector<size_t> &,
      const std::vector<Equation> &) const;
  bool commit(MatrixProofCacheEntry &&, uint64_t expected_generation);

private:
  uint64_t generation_ = 0;
  std::vector<MatrixProofCacheEntry> entries_;
};

class AffineMatrixTransaction {
public:
  bool ready() const { return ready_; }
  uint64_t owner_identity() const { return owner_identity_; }
  uint64_t additions() const { return additions_; }
  uint64_t deletions() const { return deletions_; }
  uint32_t helper_extent() const { return helper_extent_; }
  uint64_t arena_words() const { return arena_words_; }
  uint64_t work() const { return work_; }
  uint64_t authentication_work() const { return authentication_work_; }
  uint64_t retained_bytes() const;
  const std::vector<External> &reason() const { return reason_; }
  const std::vector<size_t> &source_equations() const {
    return source_equations_;
  }
  const std::vector<Equation> &source_identity() const {
    return source_identity_;
  }
  const std::vector<MatrixProofEvent> &proof_events() const { return events_; }
  bool proof_cache_hit() const { return cache_hit_; }
  bool proof_cache_insert() const { return cache_insert_; }
  bool proof_cache_resume() const { return cache_resume_; }
  bool compact_pbp() const { return compact_pbp_; }
  const PbpParityReasonPlan &pbp_plan() const { return pbp_plan_; }
  uint64_t proof_cache_generation() const { return cache_generation_; }
  const MatrixProofCacheEntry &proof_cache_entry() const {
    return cache_entry_;
  }
  const std::vector<size_t> &resume_source_equations() const {
    return resume_source_equations_;
  }
  const std::vector<Equation> &resume_source_identity() const {
    return resume_source_identity_;
  }

private:
  friend MatrixTransactionStatus prepare_affine_matrix_transaction(
      const AffineMatrix &, uint64_t, const std::vector<Equation> &,
      const MatrixEvent &, uint32_t, uint64_t,
      const MatrixTransactionLimits &, const std::function<bool(uint64_t)> &,
      AffineMatrixTransaction &, const AffineMatrixProofCache *, uint64_t,
      const PbpParityReasonPlan *, bool, bool, bool,
      MatrixTransactionFailureStage *);

  bool ready_ = false;
  uint64_t owner_identity_ = 0;
  uint64_t additions_ = 0;
  uint64_t deletions_ = 0;
  uint32_t helper_extent_ = 0;
  uint64_t arena_words_ = 0;
  uint64_t work_ = 0;
  uint64_t authentication_work_ = 0;
  std::vector<External> reason_;
  std::vector<size_t> source_equations_;
  std::vector<Equation> source_identity_;
  std::vector<size_t> resume_source_equations_;
  std::vector<Equation> resume_source_identity_;
  std::vector<MatrixProofEvent> events_;
  bool cache_hit_ = false;
  bool cache_insert_ = false;
  bool cache_resume_ = false;
  uint64_t cache_generation_ = 0;
  MatrixProofCacheEntry cache_entry_;
  bool compact_pbp_ = false;
  PbpParityReasonPlan pbp_plan_;
};

// Validate an event against the owner/source snapshot and plan every
// certificate event and the kernel clause, without mutation; a non-Prepared
// result leaves `output` unchanged.
MatrixTransactionStatus prepare_affine_matrix_transaction(
    const AffineMatrix &, uint64_t expected_owner_identity,
    const std::vector<Equation> &exact_sources, const MatrixEvent &,
    uint32_t starting_external_extent, uint64_t starting_clause_id,
    const MatrixTransactionLimits &, const std::function<bool(uint64_t)> &,
    AffineMatrixTransaction &output,
    const AffineMatrixProofCache *proof_cache = nullptr,
    uint64_t proof_epoch = 0,
    const PbpParityReasonPlan *pbp_plan = nullptr,
    // Only a same-stack compact-PBP consumer may skip the identity snapshot.
    bool retain_source_identity = true,
    // Only the same-stack persistent service may aggregate authentication.
    bool aggregate_retained_authentication = false,
    // Plan a retained reason by historical authentication.
    bool historical_event = false,
    MatrixTransactionFailureStage *failure_stage = nullptr);

} // namespace tactix::affine_semantic_extension
