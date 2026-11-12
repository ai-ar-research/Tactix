#pragma once

#include "rules/registry.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tactix {

struct ExactCoverLiveSessionAccess;

// One session per solver. Keys are signed external disjunctions and outlive
// their definition clauses.
class ExactCoverLiveSession {
public:
  size_t committed_keys() const { return committed_keys_.size(); }

private:
  friend struct ExactCoverLiveSessionAccess;
  std::vector<std::vector<int32_t>> committed_keys_;
};

// Limits and an expected arity; everything else comes from the live state.
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
  StructuralJournal records;
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
  // Fresh block-occupancy variables, offered as ranking metadata.
  std::vector<uint32_t> introduced_theory_variables;
};

// At an assignment-free propagated root, recognize one SGEN image, plan and
// revalidate every occupancy definition, and publish the batch; declines are
// mutation-free. Committed keys make any later batch containing one decline.
ExactCoverLiveResult exact_cover_live_apply(Kernel &,
                                            const ExactCoverLiveRequest &);

} // namespace tactix
