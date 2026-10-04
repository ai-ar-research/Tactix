#pragma once

#include "rules/registry.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace tactix {

struct IndependentSetOwnerClause {
  uint64_t occurrence = 0;
  uint64_t native_id = 0;
  std::vector<int32_t> literals;

  bool operator==(const IndependentSetOwnerClause &) const = default;
};

// Exact source retained before semantic consequences join the live formula.
// The clauses use stable external literals and native proof identities; no raw
// arena reference or internal variable identity survives the capture.
struct IndependentSetOwnerSnapshot {
  uint32_t external_variables = 0;
  uint32_t vertices = 0;
  uint32_t target = 0;
  uint64_t literals = 0;
  std::vector<IndependentSetOwnerClause> clauses;
};

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
  // Optional exact owner source captured before earlier consequences or
  // unrelated clauses joined the formula. Every retained clause is found and
  // revalidated in the current live formula before a commit.
  const IndependentSetOwnerSnapshot *owner_snapshot = nullptr;
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

// Capture and strictly recognize the complete current live formula as an
// owner source. On failure `output` is unchanged. This is a pure lifecycle
// operation: it emits neither proof nor journal records and changes no kernel
// state. The request contributes only recognition/source bounds.
bool independent_set_live_capture_owner(
    Kernel &, const IndependentSetLiveRequest &,
    IndependentSetOwnerSnapshot &output);

// At a fully propagated, assignment-free sparse root, recognize the exact live
// owner encoding, certify the selected N-K+1 cover, and publish that clause for
// continuing CDCL.  DSR/DRAT/no-proof are supported; LRAT/PBP decline before
// mutation.  Every recoverable failure leaves kernel, proof, records, and the
// returned result unchanged.  No new semantic or branchable variable is made.
IndependentSetLiveResult independent_set_live_apply(
    Kernel &, const IndependentSetLiveRequest &);

} // namespace tactix
