// Controller-facing action boundary for certified DirectCSP support clauses.
#pragma once

#include "rules/semantic_extension/direct_csp_action_limits.hpp"
#include "rules/semantic_extension/direct_csp_support_live.hpp"
#include "strategy/action_status_mapping.hpp"
#include "strategy/semantic_variable.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace tactix {

// An ordered domain pair in external literals and one row of the left domain.
struct DirectCspSupportActionSelection {
  std::vector<int32_t> left_values;
  std::vector<int32_t> right_values;
  uint32_t selected_left_value = 0;

  bool operator==(const DirectCspSupportActionSelection &) const = default;
};

struct DirectCspSupportActionKey {
  std::vector<int32_t> left_values;
  std::vector<int32_t> right_values;
  uint32_t selected_left_value = 0;
  int32_t selected_left_literal = 0;

  auto operator<=>(const DirectCspSupportActionKey &) const = default;
};

// One session per Kernel. The first action of each ordered pair retains its
// source snapshot; committed rows outlive their clauses.
class DirectCspSupportActionSession {
public:
  bool retained() const { return owner_ != nullptr; }
  bool belongs_to(const Kernel &kernel) const { return owner_ == &kernel; }
  size_t committed_keys() const { return keys_.size(); }
  size_t retained_owners() const { return snapshots_.size(); }
  const DirectCspSupportOwnerSnapshot *snapshot(
      const std::vector<int32_t> &left_values,
      const std::vector<int32_t> &right_values) const;
  const std::vector<DirectCspSupportOwnerSnapshot> &snapshots() const {
    return snapshots_;
  }
  const std::vector<DirectCspSupportActionKey> &keys() const { return keys_; }

private:
  friend struct DirectCspSupportActionSessionAccess;
  const Kernel *owner_ = nullptr;
  std::vector<DirectCspSupportOwnerSnapshot> snapshots_;
  std::vector<DirectCspSupportActionKey> keys_;
};

enum class DirectCspSupportActionStatus : uint8_t {
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

struct DirectCspSupportActionEffort {
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

// Exact-OR extensions are defined by original literals and omitted from the
// model; empty means no extension.
struct DirectCspSupportActionProjection {
  uint32_t extension_variable = 0;
  std::vector<int32_t> definition_inputs;
  bool exact_or_definition = false;
  bool project_from_external_model = false;
  bool reconstruction_witness_required = false;
};

struct DirectCspSupportActionResult {
  DirectCspSupportActionStatus status =
      DirectCspSupportActionStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t proposed_action_id = 0;
  uint64_t committed_action_id = 0;
  uint64_t semantic_id = 0;
  DirectCspSupportActionKey key;
  uint64_t committed_keys = 0;
  uint64_t retained_owners = 0;
  bool owner_retained = false;
  // Staged under the DirectCsp producer after the action settles.
  bool semantic_candidate_ready = false;
  std::vector<strategy::SemanticVariable> semantic_variables;
  DirectCspSupportActionProjection projection;
  DirectCspSupportActionEffort effort;
  DirectCspSupportLiveResult live;
};

// Adapters for actions over the DirectCSP live publisher: live recognition
// accumulates, other counters replace the action's fields.
inline DirectCspSupportActionStatus direct_csp_support_action_status(
    DirectCspSupportLiveStatus status) {
  if (const auto failure = strategy::map_standard_action_failure<
          DirectCspSupportActionStatus>(status))
    return *failure;
  switch (status) {
  case DirectCspSupportLiveStatus::Applied:
    return DirectCspSupportActionStatus::Applied;
  case DirectCspSupportLiveStatus::NoStructure:
    return DirectCspSupportActionStatus::NoStructure;
  case DirectCspSupportLiveStatus::NoConsequence:
    return DirectCspSupportActionStatus::NoConsequence;
  case DirectCspSupportLiveStatus::Duplicate:
    return DirectCspSupportActionStatus::AlreadyPublished;
  default: std::abort();
  }
}

inline void direct_csp_support_action_absorb_live_effort(
    DirectCspSupportActionResult &output,
    const DirectCspSupportLiveResult &live) {
  output.effort.source_clauses = live.source_clauses;
  output.effort.source_literals = live.source_literals;
  output.effort.recognition_work += live.recognition_work;
  output.effort.source_scan_work = live.source_scan_work;
  output.effort.planning_work = live.planning_work;
  output.effort.revalidation_work = live.revalidation_work;
  output.effort.proof_work = live.proof_work;
  output.effort.planned_additions = live.planned_additions;
  output.effort.emitted_additions = live.emitted_additions;
  output.effort.proof_literal_occurrences = live.proof_literal_occurrences;
  output.effort.journal_work = live.journal_work;
  output.effort.journal_bytes = live.journal_bytes;
}

// Formula/proof-state eligibility only; no scan.
RuleEligibility direct_csp_support_action_guard(const Kernel &);

// Apply one support row at an assignment-free propagated root (PBP
// declines); every decline leaves all state unchanged.
DirectCspSupportActionResult direct_csp_support_action_apply(
    Kernel &, DirectCspSupportActionSession &,
    const DirectCspSupportActionLimits &,
    const DirectCspSupportActionSelection &);

} // namespace tactix
