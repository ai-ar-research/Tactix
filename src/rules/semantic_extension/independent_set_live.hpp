#pragma once

#include "rules/registry.hpp"

#include <cstdint>
#include <vector>

namespace tactix {

struct IndependentSetOwnerClause {
  uint64_t occurrence = 0;
  uint64_t native_id = 0;
  std::vector<int32_t> literals;

  bool operator==(const IndependentSetOwnerClause &) const = default;
};

// Source retained before consequences join the formula, in external literals
// and native proof identities.
struct IndependentSetOwnerSnapshot {
  uint32_t external_variables = 0;
  uint32_t vertices = 0;
  uint32_t target = 0;
  uint64_t literals = 0;
  std::vector<IndependentSetOwnerClause> clauses;
  // Effort of this capture, charged once by the first application.
  uint64_t capture_source_scan_work = 0;
  uint64_t capture_theory_recognition_work = 0;
};

enum class IndependentSetLiveStatus : uint8_t {
  Applied,
  Invalid,
  Inapplicable,
  NoStructure,
  Duplicate,
  ResourceExhausted,
  Interrupted,
  UnsupportedProof,
  Stale,
};

// One caller-selected positive cover over external vertex identities.
struct IndependentSetLiveRequest {
  std::vector<uint32_t> cover_vertices;
  uint64_t maximum_clauses = 0;
  uint64_t maximum_literals = 0;
  uint64_t maximum_recognition_work = 0;
  uint64_t maximum_proof_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_deletions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_vertices = 0;
  uint32_t maximum_target = 0;
  uint64_t action_id = 0;
  uint64_t semantic_id = 0;
  StructuralJournal records;
  // Optional retained owner; every clause is revalidated before a commit.
  const IndependentSetOwnerSnapshot *owner_snapshot = nullptr;
  // Charge the owner capture to this application's recognition allowance.
  bool include_owner_capture_effort = false;
};

struct IndependentSetLiveResult {
  IndependentSetLiveStatus status = IndependentSetLiveStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint32_t vertices = 0;
  uint32_t target = 0;
  uint64_t source_clauses = 0;
  uint64_t source_literals = 0;
  // Total recognition work: source scanning plus theory recognition.
  uint64_t recognition_work = 0;
  uint64_t source_scan_work = 0;
  uint64_t theory_recognition_work = 0;
  uint64_t proof_work = 0;
  uint64_t planned_proof_additions = 0;
  uint64_t emitted_proof_additions = 0;
  uint64_t emitted_proof_deletions = 0;
  uint64_t proof_literal_occurrences = 0;
  uint64_t journal_work = 0;
  uint64_t published_clause_id = 0;
  std::vector<int32_t> advice_vertex_literals;
  std::vector<int32_t> published_cover;
};

struct IndependentSetOwnerCaptureResult {
  IndependentSetLiveStatus status = IndependentSetLiveStatus::Invalid;
  uint64_t source_clauses = 0;
  uint64_t source_literals = 0;
  uint64_t recognition_work = 0;
  uint64_t source_scan_work = 0;
  uint64_t theory_recognition_work = 0;
  bool captured() const { return status == IndependentSetLiveStatus::Applied; }
};

// Capture and recognize the live formula as an owner source, without proof,
// journal or kernel changes. On failure `output` is unchanged.
bool independent_set_live_capture_owner(
    Kernel &, const IndependentSetLiveRequest &,
    IndependentSetOwnerSnapshot &output);

// As above, reporting the decline reason and consumed work.
IndependentSetOwnerCaptureResult independent_set_live_capture_owner_detailed(
    Kernel &, const IndependentSetLiveRequest &,
    IndependentSetOwnerSnapshot &output);

// At a propagated, assignment-free root, recognize the owner encoding, certify
// the selected N-K+1 cover and publish it. Terminal PBP and unit covers
// decline; every decline leaves kernel, proof and records unchanged.
IndependentSetLiveResult independent_set_live_apply(
    Kernel &, const IndependentSetLiveRequest &);

} // namespace tactix
