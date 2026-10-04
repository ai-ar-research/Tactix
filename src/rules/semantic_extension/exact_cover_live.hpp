#pragma once

#include "rules/registry.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace tactix {

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
};

enum class ExactCoverLiveStatus : uint8_t {
  Applied,
  Invalid,
  Inapplicable,
  NoStructure,
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
// retained batch, revalidate it, and publish it. Every recoverable decline is
// mutation-free. The resulting definition clauses suppress an immediate
// repeat in the current formula. Durable suppression after another rule
// removes those clauses belongs to the future action-owner session. No public
// action or policy is registered here.
ExactCoverLiveResult exact_cover_live_apply(Kernel &,
                                            const ExactCoverLiveRequest &);

} // namespace tactix
