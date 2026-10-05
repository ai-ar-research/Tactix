#pragma once

#include "rules/registry.hpp"
#include "rules/semantic_extension/nary_csp_prefix.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace tactix {

struct NaryCspPrefixOwnerSnapshot {
  uint32_t external_extent = 0;
  uint64_t source_literals = 0;
  uint64_t capture_work = 0;
  std::vector<nary_csp_prefix::Premise> source;
  nary_csp_prefix::Relation relation;

  bool operator==(const NaryCspPrefixOwnerSnapshot &) const = default;
};

enum class NaryCspPrefixLiveStatus : uint8_t {
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

enum class NaryCspSemanticRole : uint8_t {
  PrefixDomainValue,
  ExcludedResidualValue,
  PrefixConjunction,
};

// Controller-visible meaning only.  The adapter changes no score, phase, or
// scheduling state; a later shared SemanticVariableAdvice action owns that.
struct NaryCspSemanticVariable {
  int32_t literal = 0;
  NaryCspSemanticRole role = NaryCspSemanticRole::PrefixDomainValue;
  bool fresh_extension = false;
  std::vector<int32_t> definition_inputs;
};

struct NaryCspPrefixLiveRequest {
  // Fresh capture uses these complete ordered signed domains.  With a retained
  // owner, an empty vector reuses its domains; a nonempty vector must match.
  std::vector<std::vector<int32_t>> domains;
  const NaryCspPrefixOwnerSnapshot *retained_owner = nullptr;
  nary_csp_prefix::Selection selection;

  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint64_t maximum_recognition_work = 0;
  uint32_t maximum_domains = 0;
  uint32_t maximum_domain_values = 0;
  uint64_t maximum_forbidden_tuples = 0;
  uint32_t maximum_prefix_arity = 0;
  uint32_t maximum_definition_clause_literals = 0;
  uint64_t maximum_proof_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_extent_growth = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t action_id = 0;
  uint64_t semantic_id = 0;
  FILE *records = nullptr;
};

struct NaryCspPrefixOwnerCaptureResult {
  NaryCspPrefixLiveStatus status = NaryCspPrefixLiveStatus::Invalid;
  uint64_t source_clauses = 0;
  uint64_t source_literals = 0;
  uint64_t recognition_work = 0;
  bool recognition_limit_exhausted = false;
};

struct NaryCspPrefixLiveResult {
  NaryCspPrefixLiveStatus status = NaryCspPrefixLiveStatus::Invalid;
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
  bool recognition_limit_exhausted = false;
  uint64_t first_published_clause_id = 0;
  uint64_t last_published_clause_id = 0;
  bool propagation_pending = false;
  std::vector<std::vector<int32_t>> published_consequences;
  std::vector<NaryCspSemanticVariable> semantic_variables;
};

// Pure lifecycle operation.  Failure leaves output and all Kernel/proof/journal
// state unchanged.
NaryCspPrefixOwnerCaptureResult nary_csp_prefix_live_capture_owner(
    Kernel &, const NaryCspPrefixLiveRequest &,
    NaryCspPrefixOwnerSnapshot &output);

// Run capture/owner validation, planning, sizing, immediate recapture and
// staleness checks, stopping before the irreversible commit boundary.
NaryCspPrefixLiveResult nary_csp_prefix_live_preflight(
    Kernel &, const NaryCspPrefixLiveRequest &);

// Atomically introduce the exact-AND helper and publish the selected residual
// exclusions at a propagated root containing only level-zero assignments.
// Selected domain variables remain unassigned. DSR, DRAT and no-proof modes
// are supported; LRAT/PBP decline before mutation.
NaryCspPrefixLiveResult nary_csp_prefix_live_apply(
    Kernel &, const NaryCspPrefixLiveRequest &);

} // namespace tactix
