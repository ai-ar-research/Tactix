#pragma once

#include "rules/semantic_extension/gf3_affine_live.hpp"
#include "rules/semantic_extension/gf3_gaussian_certificate.hpp"
#include "rules/structural_text.hpp"

#include <cstdint>
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
  Stale,
  Duplicate,
  Prepared,
  Applied,
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
  StructuralJournal records;
  // Empty accepts any batch; controller actions provide the canonical
  // complete-batch identity returned by inspect().
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
  // One work unit per canonical identity byte materialized.  The same
  // maximum_identity_bytes limit bounds both retained volume and this scan.
  uint64_t identity_work = 0;
  uint64_t identity_bytes = 0;
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
  const Gf3AffineOwnerSnapshot &owner() const { return owner_; }
  const gf3_gaussian_bridge::Plan &bridge() const { return bridge_; }
  const gf3_gaussian_certificate::Plan &certificate() const {
    return certificate_;
  }
  const std::string &batch_identity() const { return batch_identity_; }

private:
  friend struct PreparedAccess;
  friend Result prepare_internal(Kernel &, const Request &, Prepared &);
  friend Result publish_internal(Kernel &, const Request &, Prepared &);

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
  JournalText journal_;
  Result prepared_;
};

// Mutation-free preflight of apply(); it writes no proof or journal.
Result inspect(Kernel &, const Request &, Prepared &);
// Prepare and publish atomically; declines only terminal PBP.
Result apply(Kernel &, const Request &);

} // namespace tactix::gf3_gaussian_live
