// Controller-facing action boundary for certified n-ary CSP prefix clauses.
#pragma once

#include "rules/semantic_extension/nary_csp_prefix_live.hpp"
#include "strategy/semantic_variable.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tactix {

struct NaryCspPrefixActionSelection {
  std::vector<std::vector<int32_t>> domains;
  nary_csp_prefix::Selection prefix;

  bool operator==(const NaryCspPrefixActionSelection &) const = default;
};

struct NaryCspPrefixActionKey {
  std::vector<std::vector<int32_t>> domains;
  nary_csp_prefix::Selection prefix;

  bool operator==(const NaryCspPrefixActionKey &) const = default;
  bool operator<(const NaryCspPrefixActionKey &other) const {
    if (domains != other.domains) return domains < other.domains;
    if (prefix.residual_domain != other.prefix.residual_domain)
      return prefix.residual_domain < other.prefix.residual_domain;
    return prefix.prefix < other.prefix.prefix;
  }
};

// One session belongs to one Kernel lifetime.  Productive actions retain the
// exact pre-publication source owner per ordered domain collection and durable
// prefix keys even if later inprocessing deletes a published clause.
class NaryCspPrefixActionSession {
public:
  bool retained() const { return owner_ != nullptr; }
  bool belongs_to(const Kernel &kernel) const { return owner_ == &kernel; }
  size_t committed_keys() const { return keys_.size(); }
  size_t retained_owners() const { return snapshots_.size(); }
  const NaryCspPrefixOwnerSnapshot *snapshot(
      const std::vector<std::vector<int32_t>> &domains) const;
  const std::vector<NaryCspPrefixOwnerSnapshot> &snapshots() const {
    return snapshots_;
  }
  const std::vector<NaryCspPrefixActionKey> &keys() const { return keys_; }

private:
  friend struct NaryCspPrefixActionSessionAccess;
  const Kernel *owner_ = nullptr;
  std::vector<NaryCspPrefixOwnerSnapshot> snapshots_;
  std::vector<NaryCspPrefixActionKey> keys_;
};

struct NaryCspPrefixActionLimits {
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
  uint64_t maximum_session_keys = 0;
  uint64_t maximum_session_work = 0;

  bool valid() const;
};

enum class NaryCspPrefixActionStatus : uint8_t {
  Invalid,
  Inapplicable,
  NoStructure,
  NoConsequence,
  ResourceExhausted,
  Interrupted,
  UnsupportedProof,
  Stale,
  AlreadyPublished,
  Applied,
};

struct NaryCspPrefixActionEffort {
  uint64_t source_clauses = 0;
  uint64_t source_literals = 0;
  uint64_t owner_capture_work = 0;
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
  uint64_t session_work = 0;
};

struct NaryCspPrefixActionProjection {
  uint32_t extension_variable = 0;
  std::vector<int32_t> definition_inputs;
  bool exact_and_definition = false;
  bool project_from_external_model = false;
  bool reconstruction_witness_required = false;
};

struct NaryCspPrefixActionResult {
  NaryCspPrefixActionStatus status = NaryCspPrefixActionStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t proposed_action_id = 0;
  uint64_t committed_action_id = 0;
  uint64_t semantic_id = 0;
  NaryCspPrefixActionKey key;
  uint64_t committed_keys = 0;
  uint64_t retained_owners = 0;
  bool owner_retained = false;
  bool semantic_candidate_ready = false;
  std::vector<strategy::SemanticVariable> semantic_variables;
  NaryCspPrefixActionProjection projection;
  NaryCspPrefixActionEffort effort;
  NaryCspPrefixLiveResult live;
};

RuleEligibility nary_csp_prefix_action_guard(const Kernel &);

// Deterministically apply one exact prefix at an assignment-free propagated
// root. Publication and propagation are separate actions. All recoverable
// declines preserve Kernel, proof, provenance, session and action sequence.
NaryCspPrefixActionResult nary_csp_prefix_action_apply(
    Kernel &, NaryCspPrefixActionSession &,
    const NaryCspPrefixActionLimits &,
    const NaryCspPrefixActionSelection &);

} // namespace tactix
