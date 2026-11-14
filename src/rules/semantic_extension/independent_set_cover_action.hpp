#pragma once

#include "rules/semantic_extension/independent_set_live.hpp"

#include <cstdint>
#include <vector>

namespace tactix {

// Bound to one Kernel; retains the owner after the first productive cover.
struct IndependentSetCoverActionSession {
  const Kernel *owner = nullptr;
  IndependentSetOwnerSnapshot snapshot;

  bool retained() const { return owner && !snapshot.clauses.empty(); }
  bool belongs_to(const Kernel &kernel) const { return owner == &kernel; }
};

struct IndependentSetCoverActionEffort {
  uint64_t source_clauses = 0;
  uint64_t source_literals = 0;
  uint64_t recognition_work = 0;
  uint64_t source_scan_work = 0;
  uint64_t theory_recognition_work = 0;
  uint64_t proof_work = 0;
  uint64_t planned_proof_additions = 0;
  uint64_t emitted_proof_additions = 0;
  uint64_t emitted_proof_deletions = 0;
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

// Formula/proof-state eligibility only.
RuleEligibility independent_set_cover_action_guard(const Kernel &);

// Apply one positive cover (strictly increasing external variables) with
// Kernel::independent_set_cover_parameters; dispatch advances strategy_epoch.
IndependentSetCoverActionResult independent_set_cover_action_apply(
    Kernel &, IndependentSetCoverActionSession &,
    const std::vector<uint32_t> &cover_vertices);

} // namespace tactix
