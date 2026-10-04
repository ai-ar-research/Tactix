#pragma once

#include "rules/semantic_extension/independent_set_live.hpp"

#include <cstdint>
#include <vector>

namespace tactix {

// Controller/executor-owned state.  It retains the exact pre-publication owner
// after the first productive cover and deliberately has no global or Kernel
// lifetime.  A session is bound to one Kernel object for its whole lifetime.
struct IndependentSetCoverActionSession {
  const Kernel *owner = nullptr;
  IndependentSetOwnerSnapshot snapshot;

  bool retained() const { return owner && !snapshot.clauses.empty(); }
  bool belongs_to(const Kernel &kernel) const { return owner == &kernel; }
};

struct IndependentSetCoverActionEffort {
  uint64_t recognition_work = 0;
  uint64_t source_scan_work = 0;
  uint64_t theory_recognition_work = 0;
  uint64_t proof_work = 0;
  uint64_t planned_proof_additions = 0;
  uint64_t proof_literal_occurrences = 0;
  uint64_t journal_work = 0;
};

struct IndependentSetCoverActionResult {
  IndependentSetLiveStatus status = IndependentSetLiveStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t action_id = 0;
  uint64_t semantic_id = 0;
  uint32_t vertices = 0;
  uint32_t target = 0;
  uint64_t published_clause_id = 0;
  bool owner_retained = false;
  bool candidate_staged = false;
  IndependentSetCoverActionEffort effort;
};

// Formula/proof-state eligibility only. The selected cover and typed limits
// are validated by apply, so policies can discover the action without hidden
// benchmark or payload-dependent scheduling predicates.
RuleEligibility independent_set_cover_action_guard(const Kernel &);

// The action has no canonical cover choice; registry canonical dispatch is a
// deliberate NoOp while the structured v4/direct surfaces supply a vector.
ApplyResult independent_set_cover_rule_apply(Kernel &);

// Apply one caller-selected positive cover.  The vector is a set encoded in
// strictly increasing stable external variable identities.  Limits come only
// from Kernel::independent_set_cover_parameters.  This feature-local adapter
// neither registers a rule nor advances strategy_epoch; registry dispatch owns
// that transition and publishes the staged semantic-variable candidate.
IndependentSetCoverActionResult independent_set_cover_action_apply(
    Kernel &, IndependentSetCoverActionSession &,
    const std::vector<uint32_t> &cover_vertices);

} // namespace tactix
