#pragma once

#include "rules/registry.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace tactix {

// Private composition boundary for the layered independent-set theory.  The
// caller selects one positive cover over stable external vertex identities;
// this module makes no scheduling, ranking, phase, or score decision.
struct IndependentSetLiveRequest {
  std::vector<uint32_t> cover_vertices;
  uint64_t maximum_clauses = 0;
  uint64_t maximum_literals = 0;
  uint64_t maximum_recognition_work = 0;
  uint64_t maximum_proof_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_deletions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint32_t maximum_vertices = 0;
  uint32_t maximum_target = 0;
  uint64_t action_id = 0;
  uint64_t semantic_id = 0;
  FILE *records = nullptr;
};

struct IndependentSetLiveResult {
  ApplyResult result = ApplyResult::NoOp;
  uint32_t vertices = 0;
  uint32_t target = 0;
  uint64_t source_clauses = 0;
  uint64_t source_literals = 0;
  uint64_t recognition_work = 0;
  uint64_t proof_work = 0;
  uint64_t planned_proof_additions = 0;
  uint64_t emitted_proof_additions = 0;
  uint64_t emitted_proof_deletions = 0;
  uint64_t proof_literal_occurrences = 0;
  uint64_t published_clause_id = 0;
  std::vector<int32_t> advice_vertex_literals;
  std::vector<int32_t> published_cover;
};

// At a fully propagated, assignment-free sparse root, recognize the exact live
// owner encoding, certify the selected N-K+1 cover, and publish that clause for
// continuing CDCL.  DSR/DRAT/no-proof are supported; LRAT/PBP decline before
// mutation.  Every recoverable failure leaves kernel, proof, records, and the
// returned result unchanged.  No new semantic or branchable variable is made.
IndependentSetLiveResult independent_set_live_apply(
    Kernel &, const IndependentSetLiveRequest &);

} // namespace tactix
