// Controller-facing action boundary for certified DirectCSP support clauses.
#pragma once

#include "rules/semantic_extension/direct_csp_support_live.hpp"
#include "strategy/semantic_variable.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tactix {

// A support action is selected by an exact ordered pair of domains and one
// row of the left domain.  The domains are stable external literals; retaining
// them in replay data prevents a row ordinal from acquiring a new meaning.
struct DirectCspSupportActionSelection {
  std::vector<int32_t> left_values;
  std::vector<int32_t> right_values;
  uint32_t selected_left_value = 0;
};

struct DirectCspSupportActionKey {
  uint32_t selected_left_value = 0;

  bool operator==(const DirectCspSupportActionKey &) const = default;
  bool operator<(const DirectCspSupportActionKey &other) const {
    return selected_left_value < other.selected_left_value;
  }
};

// The controller owns one session for one Kernel lifetime.  Its first
// productive action retains the exact pre-publication source snapshot.  Row
// identities remain committed even if a later inprocessing rule deletes the
// published support clause.
class DirectCspSupportActionSession {
public:
  bool retained() const { return owner_ != nullptr; }
  bool belongs_to(const Kernel &kernel) const { return owner_ == &kernel; }
  size_t committed_keys() const { return keys_.size(); }
  const DirectCspSupportOwnerSnapshot *snapshot() const {
    return retained() ? &snapshot_ : nullptr;
  }
  const std::vector<DirectCspSupportActionKey> &keys() const { return keys_; }

private:
  friend struct DirectCspSupportActionSessionAccess;
  const Kernel *owner_ = nullptr;
  DirectCspSupportOwnerSnapshot snapshot_;
  std::vector<DirectCspSupportActionKey> keys_;
};

// Every recognition, proof, namespace, journal, and retained-session cost is
// selected by the caller.  No hidden global or policy budget is consulted.
struct DirectCspSupportActionLimits {
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
  uint64_t maximum_session_keys = 0;
  uint64_t maximum_session_work = 0;
  bool allow_extension = false;

  bool valid() const;
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

// Exact-OR extensions are omitted from the external model.  Their value is
// defined by the listed original literals, so no reconstruction witness is
// required.  An empty obligation denotes an original-variable-only support.
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
  bool owner_retained = false;
  // The shared registry bridge can stage this neutral metadata once DirectCSP
  // receives its own SemanticVariableProducer value.
  bool semantic_candidate_ready = false;
  std::vector<strategy::SemanticVariable> semantic_variables;
  DirectCspSupportActionProjection projection;
  DirectCspSupportActionEffort effort;
  DirectCspSupportLiveResult live;
};

// Formula/proof-state eligibility only. Selection, effort, and retained state
// remain explicit controller inputs; the guard is pure and performs no scan.
RuleEligibility direct_csp_support_action_guard(const Kernel &);

// Deterministically apply one selected support row at an assignment-free
// propagated root. Publication and propagation are separate actions. DSR,
// DRAT, and no-proof modes are supported by the live publisher; LRAT and PBP
// decline before mutation. Every recoverable decline preserves the kernel,
// session, provenance stream, and global action sequence.
DirectCspSupportActionResult direct_csp_support_action_apply(
    Kernel &, DirectCspSupportActionSession &,
    const DirectCspSupportActionLimits &,
    const DirectCspSupportActionSelection &);

} // namespace tactix
