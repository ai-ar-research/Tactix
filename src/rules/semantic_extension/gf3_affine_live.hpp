#pragma once

#include "rules/registry.hpp"
#include "rules/semantic_extension/gf3_affine.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace tactix {

enum class Gf3AffineSourceKind : uint8_t { Large, Binary };

struct Gf3AffineLiveSource {
  uint64_t occurrence = 0;
  Gf3AffineSourceKind kind = Gf3AffineSourceKind::Large;
  Ref ref = INVALID_REF;
  Lit first = INVALID_LIT;
  Lit second = INVALID_LIT;
  uint64_t native_proof_id = 0;
  std::vector<int32_t> clause;
};

// Caller-owned immutable owner.  It retains both exact physical source
// identities and the recognized semantic image.  It may survive unrelated
// clause additions, but any retained source deletion, rewrite, id change,
// retirement, or arena compaction makes it stale.
struct Gf3AffineOwnerSnapshot {
  uint32_t external_extent = 0;
  uint64_t arena_generation = 0;
  uint64_t source_literals = 0;
  uint64_t capture_work = 0;
  std::vector<Gf3AffineLiveSource> sources;
  gf3_affine::Theory theory;
};

enum class Gf3AffineLiveStatus : uint8_t {
  Applied,
  Invalid,
  Inapplicable,
  NoStructure,
  NoConsequence,
  Duplicate,
  ResourceExhausted,
  Interrupted,
  UnsupportedProof,
  Stale,
};

struct Gf3AffineLiveRequest {
  gf3_affine::RecognitionBounds recognition;
  gf3_affine::PairEliminationBounds elimination;
  const Gf3AffineOwnerSnapshot *retained_owner = nullptr;
  uint32_t first_equation = 0;
  uint32_t second_equation = 0;

  uint64_t maximum_recognition_work = 0;
  uint64_t maximum_planning_work = 0;
  uint64_t maximum_duplicate_scan_work = 0;
  uint64_t maximum_revalidation_work = 0;
  uint64_t maximum_proof_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_arena_words = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t action_id = 0;
  uint64_t semantic_id = 0;
  FILE *records = nullptr;
};

struct Gf3AffineOwnerCaptureResult {
  Gf3AffineLiveStatus status = Gf3AffineLiveStatus::Invalid;
  uint64_t source_clauses = 0;
  uint64_t source_literals = 0;
  uint64_t recognition_work = 0;
  uint64_t equations = 0;
};

struct Gf3AffineLiveResult {
  Gf3AffineLiveStatus status = Gf3AffineLiveStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t source_clauses = 0;
  uint64_t source_literals = 0;
  uint64_t equations = 0;
  uint64_t source_scan_work = 0;
  uint64_t recognition_work = 0;
  uint64_t planning_work = 0;
  uint64_t duplicate_scan_work = 0;
  uint64_t revalidation_work = 0;
  uint64_t proof_work = 0;
  uint64_t planned_additions = 0;
  uint64_t duplicate_additions = 0;
  uint64_t emitted_additions = 0;
  uint64_t proof_literal_occurrences = 0;
  uint64_t arena_words = 0;
  uint64_t journal_work = 0;
  uint64_t journal_bytes = 0;
  uint64_t first_published_clause_id = 0;
  uint64_t last_published_clause_id = 0;
  bool propagation_pending = false;
  bool reconstruction_witness_required = false;
  gf3_affine::EquationMeaning consequence;
  std::vector<uint64_t> published_clause_ids;
  // Existing positive selection literals participating in the consequence.
  // They are metadata for controller-owned advice; this adapter does not
  // mutate phase, VMTF, EVSIDS, or any other heuristic state.
  std::vector<int32_t> semantic_variables;
};

// Capture and recognize one exact assignment-free live GF(3) source image.
// Output remains unchanged on every decline.
Gf3AffineOwnerCaptureResult gf3_affine_live_capture_owner(
    Kernel &, const Gf3AffineLiveRequest &, Gf3AffineOwnerSnapshot &output);

// Plan and atomically publish one caller-selected pair elimination.  DSR,
// DRAT, and no-proof modes are supported.  Propagation is deliberately left to
// the controller's next action.  Every recoverable decline is mutation-free.
Gf3AffineLiveResult gf3_affine_live_apply(Kernel &,
                                          const Gf3AffineLiveRequest &);

} // namespace tactix
