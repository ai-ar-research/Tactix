#pragma once

#include "rules/registry.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace tactix {

struct ExactCoverLiveSessionAccess;

// Durable action-owner state. Keys are exact signed external disjunctions,
// never hashes, and survive later removal of their definition clauses. The
// caller owns one session for the lifetime of one solver and cannot mutate its
// committed contents directly.
class ExactCoverLiveSession {
public:
  size_t committed_keys() const { return committed_keys_.size(); }
  const std::vector<std::vector<int32_t>> &keys() const {
    return committed_keys_;
  }

private:
  friend struct ExactCoverLiveSessionAccess;
  std::vector<std::vector<int32_t>> committed_keys_;
};

// Private live-kernel boundary for the exact-cover batch foundation. The
// caller supplies only limits and an expected arity; source clauses, outputs,
// semantic identities, and proof obligations are recovered from current state.
struct ExactCoverLiveRequest {
  uint32_t arity = 0;
  uint64_t maximum_items = 0;
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint64_t maximum_recognition_work = 0;
  uint64_t maximum_premise_references = 0;
  uint32_t maximum_definitions = 0;
  uint64_t maximum_additions = 0;
  uint64_t maximum_literal_occurrences = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_extent_growth = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t action_id = 0;
  FILE *records = nullptr;
  ExactCoverLiveSession *session = nullptr;
};

enum class ExactCoverLiveStatus : uint8_t {
  Applied,
  Invalid,
  Inapplicable,
  NoStructure,
  AlreadyPublished,
  ResourceExhausted,
  Interrupted,
  UnsupportedProof,
  Stale,
};

struct ExactCoverLiveResult {
  ExactCoverLiveStatus status = ExactCoverLiveStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t source_clauses = 0;
  uint64_t source_literals = 0;
  uint64_t recognition_work = 0;
  uint64_t revalidation_work = 0;
  uint32_t definitions = 0;
  uint64_t additions = 0;
  uint64_t literal_occurrences = 0;
  uint64_t journal_bytes = 0;
  uint32_t extent_growth = 0;
  // Controller-owned ranking metadata. This adapter never changes heuristic
  // state; every output is a fresh block-occupancy theory variable.
  std::vector<uint32_t> introduced_theory_variables;
};

// At an assignment-free propagated root, capture the complete live formula,
// recognize one exact SGEN image, plan every occupancy definition in one
// retained batch, revalidate it within the recognition allowance, and publish
// it. Every recoverable decline is mutation-free. A successful publication
// commits its exact duplicate keys to the caller-owned session only after the
// proof, formula, and journal commit. Any later batch containing a committed
// key declines atomically, even if another rule removed the definition
// clauses. No public action or policy is registered here.
ExactCoverLiveResult exact_cover_live_apply(Kernel &,
                                            const ExactCoverLiveRequest &);

} // namespace tactix
