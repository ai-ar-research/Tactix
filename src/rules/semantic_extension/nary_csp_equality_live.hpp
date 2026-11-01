#pragma once

#include "rules/semantic_extension/nary_csp_action_limits.hpp"
#include "rules/registry.hpp"
#include "rules/semantic_extension/nary_csp_equality.hpp"
#include "rules/semantic_extension/nary_csp_relation_live.hpp"
#include "strategy/semantic_variable.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace tactix {

struct NaryCspEqualityKey {
  std::vector<std::vector<int32_t>> domains;
  uint32_t first_equal_domain = 0;
  uint32_t second_equal_domain = 0;
  uint32_t residual_domain = 0;

  bool operator==(const NaryCspEqualityKey &) const = default;
  bool operator<(const NaryCspEqualityKey &other) const {
    if (domains != other.domains) return domains < other.domains;
    if (first_equal_domain != other.first_equal_domain)
      return first_equal_domain < other.first_equal_domain;
    if (second_equal_domain != other.second_equal_domain)
      return second_equal_domain < other.second_equal_domain;
    return residual_domain < other.residual_domain;
  }
};

struct NaryCspEqualityRecord {
  NaryCspEqualityKey key;
  uint32_t extension_variable = 0;
  std::vector<std::vector<int32_t>> definition_clauses;
  bool operator==(const NaryCspEqualityRecord &) const = default;
};

class NaryCspEqualitySession {
public:
  bool retained() const { return owner_ != nullptr; }
  bool belongs_to(const Kernel &kernel) const { return owner_ == &kernel; }
  const std::vector<NaryCspEqualityKey> &keys() const { return keys_; }
  const std::vector<NaryCspEqualityRecord> &records() const { return records_; }

private:
  friend struct NaryCspEqualitySessionAccess;
  const Kernel *owner_ = nullptr;
  std::vector<NaryCspEqualityKey> keys_;
  std::vector<NaryCspEqualityRecord> records_;
};

struct NaryCspEqualityRequest {
  const NaryCspRelationOwnerSnapshot *owner = nullptr;
  // The action wrapper may pass a just-captured staged owner. No Kernel state
  // can have changed since that capture, so avoid charging a second census.
  bool owner_freshly_captured = false;
  nary_csp_equality::Selection selection;
  NaryCspEqualityLimits limits;
  uint64_t action_id = 0;
  uint64_t semantic_id = 0;
  FILE *records = nullptr;
};

enum class NaryCspEqualityLiveStatus : uint8_t {
  Invalid,
  Inapplicable,
  NoConsequence,
  ResourceExhausted,
  Interrupted,
  UnsupportedProof,
  Stale,
  Duplicate,
  Applied,
};

struct NaryCspEqualityProjection {
  uint32_t extension_variable = 0;
  std::vector<int32_t> left_values;
  std::vector<int32_t> right_values;
  bool exact_aligned_equality = false;
  bool project_from_external_model = false;
  bool reconstruction_witness_required = false;
};

struct NaryCspEqualityLiveResult {
  NaryCspEqualityLiveStatus status = NaryCspEqualityLiveStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  NaryCspEqualityKey key;
  uint64_t source_clauses = 0;
  uint64_t source_literals = 0;
  uint64_t recognition_work = 0;
  uint64_t planning_work = 0;
  uint64_t proof_work = 0;
  uint64_t planned_additions = 0;
  uint64_t emitted_additions = 0;
  uint64_t proof_literal_occurrences = 0;
  uint64_t journal_bytes = 0;
  uint32_t extent_growth = 0;
  uint64_t first_published_clause_id = 0;
  uint64_t last_published_clause_id = 0;
  bool propagation_pending = false;
  NaryCspEqualityProjection projection;
  std::vector<std::vector<int32_t>> published_consequences;
  std::vector<strategy::SemanticVariable> semantic_variables;
  bool semantic_candidate_ready = false;
};

// One atomic internal action.  The caller supplies an authenticated neutral
// relation owner;
// apply freshly recaptures it, plans and sizes the complete transaction, then
// publishes the equality definition and transferred diagonal nogoods.  No
// controller scheduling or public action registry is implied by this surface.
NaryCspEqualityLiveResult nary_csp_equality_live_apply(
    Kernel &, NaryCspEqualitySession &, const NaryCspEqualityRequest &);

} // namespace tactix
