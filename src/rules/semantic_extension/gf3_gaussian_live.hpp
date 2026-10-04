#pragma once

#include "rules/semantic_extension/gf3_affine_live.hpp"
#include "rules/semantic_extension/gf3_gaussian_certificate.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace tactix::gf3_gaussian_live {

enum class Status : uint8_t {
  Invalid,
  Inapplicable,
  NoStructure,
  NoSupportedOutcome,
  ResourceExhausted,
  Interrupted,
  UnsupportedProof,
  UnsupportedProofSplit,
  Stale,
  Duplicate,
  Prepared,
  Applied,
  AlreadyApplied,
};

struct Request {
  gf3_affine::RecognitionBounds recognition;
  gf3_gaussian_certificate::Limits certificate;
  uint64_t maximum_capture_work = 0;
  uint64_t maximum_revalidation_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_journal_bytes = 0;
  uint64_t maximum_identity_bytes = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t action_id = 0;
  uint64_t semantic_id = 0;
  FILE *records = nullptr;
  // Empty retains the private batch API.  Public controller actions always
  // provide the exact canonical complete-batch identity returned by inspect().
  std::string selected_identity;
};

struct Accounting {
  uint64_t source_clauses = 0;
  uint64_t source_literals = 0;
  uint64_t capture_work = 0;
  uint64_t bridge_work = 0;
  uint64_t certificate_work = 0;
  uint64_t validation_work = 0;
  uint64_t revalidation_work = 0;
  uint64_t proof_additions = 0;
  uint64_t proof_literal_occurrences = 0;
  uint64_t installed_clauses = 0;
  uint64_t duplicate_clauses = 0;
  uint64_t journal_work = 0;
  uint64_t journal_bytes = 0;
  uint64_t identity_bytes = 0;

  friend bool operator==(const Accounting &, const Accounting &) = default;
};

struct Result {
  Status status = Status::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  Accounting effort;
  bool contradiction = false;
  bool propagation_pending = false;
  std::vector<int32_t> published_units;
};

class Prepared {
public:
  Prepared() = default;
  Prepared(const Prepared &) = delete;
  Prepared &operator=(const Prepared &) = delete;
  Prepared(Prepared &&) = default;
  Prepared &operator=(Prepared &&) = default;
  bool ready() const { return ready_; }
  bool spent() const { return spent_; }
  const Gf3AffineOwnerSnapshot &owner() const { return owner_; }
  const gf3_gaussian_bridge::Plan &bridge() const { return bridge_; }
  const gf3_gaussian_certificate::Plan &certificate() const {
    return certificate_;
  }
  const std::string &batch_identity() const { return batch_identity_; }

private:
  friend struct PreparedAccess;
  friend Result prepare(Kernel &, const Request &, Prepared &);
  friend Result apply(Kernel &, const Request &, Prepared &);
  friend Result apply(Kernel &, const Request &);
  friend Result prepare_internal(Kernel &, const Request &, Prepared &, bool);
  friend Result publish_internal(Kernel &, const Request &, Prepared &, bool);

  bool ready_ = false;
  bool spent_ = false;
  // A prepared transaction is bound to every semantic/resource parameter and
  // to the exact FILE sink whose bytes were precomputed.  Publication rejects
  // even a looser limit or a different sink rather than silently changing the
  // action contract across the caller-visible gap.
  Request request_;
  uint64_t starting_clause_id_ = 0;
  uint64_t arena_generation_ = 0;
  uint32_t starting_nvars_ = 0;
  uint32_t starting_extent_ = 0;
  Gf3AffineOwnerSnapshot owner_;
  gf3_gaussian_bridge::Plan bridge_;
  gf3_gaussian_certificate::Plan certificate_;
  std::vector<uint64_t> event_ids_;
  std::vector<uint8_t> install_;
  std::string batch_identity_;
  std::string journal_;
  Result prepared_;
};

// Split preparation is proofless by construction: Proof's private stream
// cannot be authenticated across a caller-visible gap.  Atomic apply supports
// DSR, DRAT, and no-proof modes.  LRAT/PBP decline before mutation.
Result prepare(Kernel &, const Request &, Prepared &);
// Mutation-free atomic preflight.  Unlike split prepare(), inspection is also
// available with DSR/DRAT because no proof stream is retained or written.
Result inspect(Kernel &, const Request &, Prepared &);
Result apply(Kernel &, const Request &, Prepared &);
Result apply(Kernel &, const Request &);

} // namespace tactix::gf3_gaussian_live
