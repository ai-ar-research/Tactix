#pragma once

#include "rules/registry.hpp"
#include "rules/semantic_extension/direct_csp_support.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace tactix {

struct DirectCspSupportOwnerSnapshot {
  uint32_t external_extent = 0;
  uint64_t source_literals = 0;
  uint64_t capture_work = 0;
  std::vector<direct_csp_support::Premise> source;
  direct_csp_support::Relation relation;
};

inline bool direct_csp_support_same_owner_image(
    const DirectCspSupportOwnerSnapshot &first,
    const DirectCspSupportOwnerSnapshot &second) {
  return first.source_literals == second.source_literals &&
         first.source == second.source && first.relation == second.relation;
}

inline bool direct_csp_support_same_live_owner(
    const DirectCspSupportOwnerSnapshot &first,
    const DirectCspSupportOwnerSnapshot &second) {
  return first.external_extent == second.external_extent &&
         first.capture_work == second.capture_work &&
         direct_csp_support_same_owner_image(first, second);
}

enum class DirectCspSupportLiveStatus : uint8_t {
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

enum class DirectCspSemanticRole : uint8_t {
  SelectedDomainValue,
  CompatibleDomainValue,
  SupportDisjunction,
};

// Controller-visible meaning only.  The adapter does not alter VMTF, EVSIDS,
// phase state, or any other heuristic state.
struct DirectCspSemanticVariable {
  int32_t literal = 0;
  DirectCspSemanticRole role = DirectCspSemanticRole::SelectedDomainValue;
  bool fresh_extension = false;
  std::vector<int32_t> definition_inputs;
};

struct DirectCspSupportLiveRequest {
  // Used for a fresh capture.  With retained_owner, empty vectors reuse the
  // exact owner vectors; nonempty vectors must match them exactly.
  std::vector<int32_t> left_values;
  std::vector<int32_t> right_values;
  const DirectCspSupportOwnerSnapshot *retained_owner = nullptr;
  uint32_t selected_left_value = 0;

  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint64_t maximum_recognition_work = 0;
  uint32_t maximum_domain_values = 0;
  uint32_t maximum_direct_clause_literals = 0;
  uint32_t maximum_definition_clause_literals = 0;
  uint64_t maximum_proof_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_extent_growth = 0;
  uint32_t maximum_external_extent = 0;
  bool allow_extension = false;
  uint64_t action_id = 0;
  uint64_t semantic_id = 0;
  FILE *records = nullptr;
};

struct DirectCspSupportOwnerCaptureResult {
  DirectCspSupportLiveStatus status = DirectCspSupportLiveStatus::Invalid;
  uint64_t source_clauses = 0;
  uint64_t source_literals = 0;
  uint64_t recognition_work = 0;
  bool recognition_limit_exhausted = false;
};

struct DirectCspSupportLiveResult {
  DirectCspSupportLiveStatus status = DirectCspSupportLiveStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t source_clauses = 0;
  uint64_t source_literals = 0;
  uint64_t recognition_work = 0;
  uint64_t source_scan_work = 0;
  uint64_t planning_work = 0;
  uint64_t revalidation_work = 0;
  uint64_t proof_work = 0;
  uint64_t planned_additions = 0;
  uint64_t emitted_additions = 0;
  uint64_t proof_literal_occurrences = 0;
  uint64_t journal_work = 0;
  uint64_t journal_bytes = 0;
  uint32_t extent_growth = 0;
  // Distinguishes an exhausted recognition allowance from proof, namespace,
  // journal, clause-id, arena, or other physical resource admission.
  bool recognition_limit_exhausted = false;
  uint64_t published_clause_id = 0;
  bool propagation_pending = false;
  std::vector<int32_t> published_support;
  std::vector<DirectCspSemanticVariable> semantic_variables;
};

// Pure lifecycle operation.  On failure output is unchanged and no proof,
// journal, formula, namespace, or heuristic state is modified.
DirectCspSupportOwnerCaptureResult direct_csp_support_live_capture_owner(
    Kernel &, const DirectCspSupportLiveRequest &,
    DirectCspSupportOwnerSnapshot &output);

// Run the exact validation, planning, proof/journal sizing, physical-capacity,
// source revalidation, and duplicate checks used by apply, but stop before the
// irreversible commit boundary.  Applied means the same request can cross
// that boundary immediately if the caller does not mutate the Kernel.
DirectCspSupportLiveResult direct_csp_support_live_preflight(
    Kernel &, const DirectCspSupportLiveRequest &);

// Plan and atomically publish one selected support consequence at an
// assignment-free propagated root. DSR, DRAT, no-proof and streaming PBP are
// accepted; LRAT and terminal-PBP decline before mutation. Every recoverable
// failure is mutation-free.
DirectCspSupportLiveResult direct_csp_support_live_apply(
    Kernel &, const DirectCspSupportLiveRequest &);

} // namespace tactix
