// Private live-kernel adapter for certified affine CNF consequences.
#pragma once

#include "kernel/kernel.hpp"
#include "rules/semantic_extension/affine_image.hpp"
#include "rules/registry.hpp"

#include <cstdio>

namespace tactix {

enum class AffineSourceKind : uint8_t { Large, Binary, CheckerPremise, RootUnit };

struct AffineLiveSource {
  uint64_t occurrence = 0;
  AffineSourceKind kind = AffineSourceKind::Large;
  Ref ref = INVALID_REF;       // Large source only.
  Lit first = INVALID_LIT;     // Binary source only.
  Lit second = INVALID_LIT;    // Binary source only.
  uint64_t clause_id = 0;
  bool proof_id_exact = true; // False for authenticated presence sentinels.
  std::vector<int32_t> clause;
  bool recognition_candidate=true; // False for authenticated root-RUP oracle support only.
};

struct AffineLiveSnapshot {
  std::vector<affine_semantic_extension::Premise> premises;
  std::vector<AffineLiveSource> sources;
  // Every large-clause Ref is valid only in this arena generation. Cached
  // snapshots from an earlier collection are rejected before any Ref is read.
  uint64_t arena_generation = 0;
  uint64_t literals = 0;
  uint64_t work = 0;
};

struct AffineLiveRequest {
  affine_semantic_extension::Limits image;
  uint64_t maximum_snapshot_literals = 0;
  uint64_t maximum_snapshot_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_deletions = 0;
  uint64_t maximum_journal_bytes = 0;
  size_t consequence = 0;
  uint64_t action_id = 0;
  uint64_t semantic_id = 0;
  FILE *records = nullptr;
  // Optional caller-cached exact source view. Payload counters are recomputed,
  // and selected sources are revalidated immediately before commit; stale
  // occurrences make the action a clean NoOp.
  const AffineLiveSnapshot *snapshot = nullptr;
  uint64_t maximum_snapshot_bytes = 0; // Zero retains legacy allocation contract.
  bool full_root_view = false; // Positive persistence only; legacy arity filter remains.
  // Incremental discovery: known large IDs are excluded unless unresolved;
  // binaries use exact signed external-body identity even without native IDs.
  uint64_t discovered_through_clause_id = 0;
  const std::vector<uint64_t> *known_binary_bodies = nullptr;
  const std::vector<uint64_t> *pending_large_ids = nullptr;
  const std::vector<uint64_t> *pending_binary_bodies = nullptr;
};

inline uint64_t affine_binary_body_identity(int32_t a,int32_t b) {
  if(b<a)std::swap(a,b);
  return (uint64_t(uint32_t(a))<<32)|uint32_t(b);
}

enum class AffineLiveStatus : uint8_t {
  Invalid,
  Inapplicable,
  NoStructure,
  ResourceExhausted,
  Interrupted,
  UnsupportedProof,
  Stale,
  Applied,
};

struct AffineLiveResult {
  // Every recoverable decline is classified without mutating the formula,
  // proof, provenance stream, or clause-id namespace. Applied covers both a
  // continuing Ok result and a terminal Unsat result.
  AffineLiveStatus status = AffineLiveStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  affine_semantic_extension::Status recognition =
      affine_semantic_extension::Status::NoStructure;
  uint64_t snapshot_clauses = 0, snapshot_literals = 0, snapshot_work = 0;
  uint64_t equations = 0, consequences = 0, recognition_work = 0;
  uint64_t selected_sources = 0, selected_premises = 0;
  // RUP additions materialized by proof planning. On an addition-budget
  // decline this is the complete consumed quota even though no plan commits.
  uint64_t proof_addition_work = 0;
  uint64_t preflight_additions = 0, preflight_deletions = 0;
  uint64_t additions = 0, deletions = 0, published = 0;
  // Bytes considered while constructing the bounded journal, and bytes
  // actually committed to records. A journal-budget decline can have nonzero
  // work while journal_bytes remains zero.
  uint64_t journal_work = 0, journal_bytes = 0;
  bool contradiction = false;
  bool rhs = false;
  // Publication can re-arm root propagation. The adapter deliberately does
  // not drain it: the controller owns the following Propagate action and its
  // separately budgeted proof/journal delta.
  bool propagation_pending = false;
  // Controller-owned advice may rank these existing variables later. This
  // adapter never changes phase, EVSIDS, VMTF, or any other heuristic state.
  std::vector<int32_t> semantic_variables;
};

// Capture only clauses eligible for the requested affine arity. On failure the
// output is unchanged. Occurrences are stable within this exact source view.
bool capture_affine_live_snapshot(Kernel &, const AffineLiveRequest &,
                                  AffineLiveSnapshot &);

// Validate the named synthetic occurrences against current live clause
// identity, literals, variable status, and binary/arena liveness.
bool affine_live_sources_valid(const Kernel &, const AffineLiveSnapshot &,
                               const std::vector<uint64_t> &occurrences);

// Recognize, preflight, revalidate, emit the preflighted RUP plan, and publish
// only missing target clauses. Root propagation is left pending for the
// controller so this action's proof IDs and effort remain exactly preflighted.
// No public action or policy is registered here.
AffineLiveResult derive_affine_live(Kernel &, const AffineLiveRequest &);

} // namespace tactix
