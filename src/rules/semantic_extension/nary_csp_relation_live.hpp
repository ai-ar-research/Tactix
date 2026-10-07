#pragma once

#include "rules/registry.hpp"
#include "rules/semantic_extension/nary_csp_relation.hpp"

#include <cstdint>
#include <vector>

namespace tactix {

// An observational snapshot of exact live clauses. It owns no solver state and
// grants no permission to publish a consequence.
struct NaryCspRelationOwnerSnapshot {
  uint32_t external_extent = 0;
  uint64_t source_literals = 0;
  uint64_t capture_work = 0;
  std::vector<nary_csp_relation::Premise> source;
  nary_csp_relation::Relation relation;

  bool operator==(const NaryCspRelationOwnerSnapshot &) const = default;
};

enum class NaryCspRelationCaptureStatus : uint8_t {
  Invalid,
  Inapplicable,
  NoStructure,
  Duplicate,
  ResourceExhausted,
  Interrupted,
  Stale,
  Applied,
};

struct NaryCspRelationCaptureRequest {
  // Domains must be disjoint, contain at least two values, and be strictly
  // ordered by (absolute variable, signed literal).
  std::vector<std::vector<int32_t>> domains;
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint64_t maximum_recognition_work = 0;
  uint32_t maximum_domains = 0;
  uint32_t maximum_domain_values = 0;
  uint64_t maximum_forbidden_tuples = 0;
  uint32_t maximum_external_extent = 0;

  bool valid() const;
};

struct NaryCspRelationCaptureResult {
  NaryCspRelationCaptureStatus status =
      NaryCspRelationCaptureStatus::Invalid;
  uint64_t source_clauses = 0;
  uint64_t source_literals = 0;
  uint64_t recognition_work = 0;
  uint64_t session_work = 0;
  uint64_t retained_owners = 0;
  bool recognition_limit_exhausted = false;
  bool owner_retained = false;
};

// Authenticate exact ALO, pairwise AMO, and forbidden-tuple clauses at a
// propagated root. The function changes only output on Applied. It never
// writes a proof, journal, clause, assignment, phase, or semantic candidate.
NaryCspRelationCaptureResult nary_csp_relation_capture_owner(
    Kernel &, const NaryCspRelationCaptureRequest &,
    NaryCspRelationOwnerSnapshot &output);

RuleEligibility nary_csp_relation_owner_guard(const Kernel &);

} // namespace tactix
