// Pure proof preflight for one authenticated affine-matrix event.
#pragma once

#include "rules/semantic_extension/affine_matrix.hpp"

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

struct MatrixTransactionLimits {
  uint64_t maximum_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_deletions = 0;
  uint32_t maximum_helper_extent = 0;
  uint64_t maximum_arena_words = 0;
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

class AffineMatrixTransaction {
public:
  bool ready() const { return ready_; }
  uint64_t owner_identity() const { return owner_identity_; }
  uint64_t additions() const { return additions_; }
  uint64_t deletions() const { return deletions_; }
  uint32_t helper_extent() const { return helper_extent_; }
  uint64_t arena_words() const { return arena_words_; }
  uint64_t work() const { return work_; }
  const std::vector<External> &reason() const { return reason_; }
  const std::vector<size_t> &source_equations() const {
    return source_equations_;
  }
  const std::vector<Equation> &source_identity() const {
    return source_identity_;
  }
  const std::vector<MatrixProofEvent> &proof_events() const { return events_; }

private:
  friend MatrixTransactionStatus prepare_affine_matrix_transaction(
      const AffineMatrix &, uint64_t, const std::vector<Equation> &,
      const MatrixEvent &, uint32_t, uint64_t,
      const MatrixTransactionLimits &, const std::function<bool(uint64_t)> &,
      AffineMatrixTransaction &);

  bool ready_ = false;
  uint64_t owner_identity_ = 0;
  uint64_t additions_ = 0;
  uint64_t deletions_ = 0;
  uint32_t helper_extent_ = 0;
  uint64_t arena_words_ = 0;
  uint64_t work_ = 0;
  std::vector<External> reason_;
  std::vector<size_t> source_equations_;
  std::vector<Equation> source_identity_;
  std::vector<MatrixProofEvent> events_;
};

// Validate an event against an exact matrix owner/source snapshot, then plan
// all certificate additions, deletions, helper IDs, and the one kernel clause.
// This function does not touch a kernel, proof stream, scheduler, or trail.
// Any result other than Prepared leaves `output` byte-for-byte unchanged at
// its public semantic boundary.
MatrixTransactionStatus prepare_affine_matrix_transaction(
    const AffineMatrix &, uint64_t expected_owner_identity,
    const std::vector<Equation> &exact_sources, const MatrixEvent &,
    uint32_t starting_external_extent, uint64_t starting_clause_id,
    const MatrixTransactionLimits &, const std::function<bool(uint64_t)> &,
    AffineMatrixTransaction &output);

} // namespace tactix::affine_semantic_extension
