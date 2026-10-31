// Controller-facing action boundary for certified DirectCSP support clauses.
#pragma once

#include "rules/semantic_extension/direct_csp_action_limits.hpp"
#include "rules/semantic_extension/direct_csp_support_live.hpp"
#include "strategy/semantic_variable.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace tactix {

// A support action is selected by an exact ordered pair of domains and one
// row of the left domain.  The domains are stable external literals; retaining
// them in replay data prevents a row ordinal from acquiring a new meaning.
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

  bool operator==(const DirectCspSupportActionKey &) const = default;
  bool operator<(const DirectCspSupportActionKey &other) const {
    if (left_values != other.left_values)
      return left_values < other.left_values;
    if (right_values != other.right_values)
      return right_values < other.right_values;
    if (selected_left_value != other.selected_left_value)
      return selected_left_value < other.selected_left_value;
    return selected_left_literal < other.selected_left_literal;
  }
};

// The controller owns one session for one Kernel lifetime.  Its first
// productive action for each ordered domain pair retains that pair's exact
// pre-publication source snapshot. Row identities remain committed even if a
// later inprocessing rule deletes the published support clause. Left/right
// order is semantic: reversing a pair selects the reverse support direction.
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
  uint64_t retained_owners = 0;
  bool owner_retained = false;
  // The shared registry bridge publishes this neutral metadata under the
  // DirectCsp producer identity after the composed action settles.
  bool semantic_candidate_ready = false;
  std::vector<strategy::SemanticVariable> semantic_variables;
  DirectCspSupportActionProjection projection;
  DirectCspSupportActionEffort effort;
  DirectCspSupportLiveResult live;
};

// Shared adapters for actions backed by the DirectCSP live publisher. Some
// composed actions account owner recognition before publication, so live
// recognition is accumulated while the other publisher counters replace
// their corresponding action fields.
inline DirectCspSupportActionStatus direct_csp_support_action_status(
    DirectCspSupportLiveStatus status) {
  switch (status) {
  case DirectCspSupportLiveStatus::Applied:
    return DirectCspSupportActionStatus::Applied;
  case DirectCspSupportLiveStatus::Invalid:
    return DirectCspSupportActionStatus::Invalid;
  case DirectCspSupportLiveStatus::Inapplicable:
    return DirectCspSupportActionStatus::Inapplicable;
  case DirectCspSupportLiveStatus::NoStructure:
    return DirectCspSupportActionStatus::NoStructure;
  case DirectCspSupportLiveStatus::NoConsequence:
    return DirectCspSupportActionStatus::NoConsequence;
  case DirectCspSupportLiveStatus::Duplicate:
    return DirectCspSupportActionStatus::AlreadyPublished;
  case DirectCspSupportLiveStatus::ResourceExhausted:
    return DirectCspSupportActionStatus::ResourceExhausted;
  case DirectCspSupportLiveStatus::Interrupted:
    return DirectCspSupportActionStatus::Interrupted;
  case DirectCspSupportLiveStatus::UnsupportedProof:
    return DirectCspSupportActionStatus::UnsupportedProof;
  case DirectCspSupportLiveStatus::Stale:
    return DirectCspSupportActionStatus::Stale;
  }
  std::abort();
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

// One caller-bounded deterministic page over exact ordered domain pairs and
// their selected left rows.  The continuation key is the last syntactically
// valid disjoint pair/row position processed, including a rejected candidate;
// this lets the next page advance across non-structures and terminal rows.
// It stays meaningful while that Kernel's root formula and session are
// unchanged; callers restart from the beginning after an action mutates either
// one. Zero never means unlimited.
struct DirectCspSupportActionOfferWindow {
  uint64_t maximum_domain_candidates = 0;
  uint64_t maximum_pair_candidates = 0;
  uint64_t maximum_row_candidates = 0;
  uint64_t maximum_offers = 0;
  uint64_t maximum_enumeration_work = 0;
  bool have_after = false;
  DirectCspSupportActionKey after;

  bool valid() const;
};

enum class DirectCspSupportActionOfferStatus : uint8_t {
  Invalid,
  Inapplicable,
  NoStructure,
  ResourceExhausted,
  Interrupted,
  Stale,
  Ready,
};

struct DirectCspSupportActionOfferEffort {
  uint64_t formula_clauses = 0;
  uint64_t formula_literals = 0;
  uint64_t candidate_scan_work = 0;
  uint64_t owner_capture_work = 0;
  uint64_t planning_work = 0;
  uint64_t duplicate_scan_work = 0;
  uint64_t session_work = 0;
  uint64_t session_preflight_work = 0;
  uint64_t enumeration_work = 0;
  uint64_t traversal_work = 0;
  uint64_t domain_candidates = 0;
  uint64_t pair_candidates = 0;
  uint64_t row_candidates = 0;
  uint64_t retained_owners_revalidated = 0;
  uint64_t committed_rows_skipped = 0;
  uint64_t consequence_free_rows = 0;
  uint64_t live_consequences_skipped = 0;
  uint64_t resource_rejected_rows = 0;
  uint64_t page_limited_captures = 0;
};

struct DirectCspSupportActionOfferResult {
  DirectCspSupportActionOfferStatus status =
      DirectCspSupportActionOfferStatus::Invalid;
  bool complete = false;
  bool have_resume_after = false;
  DirectCspSupportActionKey resume_after;
  std::vector<DirectCspSupportActionSelection> offers;
  DirectCspSupportActionOfferEffort effort;
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

// Discover exact, productive support-row payloads without modifying the
// kernel, proof stream, provenance stream, or retained session.  Candidate
// domains come from live ALO clauses, while the existing complete owner census
// certifies both AMO domains and every directed cross entry.  Retained owners
// are recaptured and compared before any offer is returned.  Durable committed
// keys are never re-offered.
DirectCspSupportActionOfferResult direct_csp_support_action_offer(
    Kernel &, const DirectCspSupportActionSession &,
    const DirectCspSupportActionLimits &,
    const DirectCspSupportActionOfferWindow &);

} // namespace tactix
