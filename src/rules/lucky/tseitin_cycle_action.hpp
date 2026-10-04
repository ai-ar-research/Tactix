#pragma once

#include "rules/lucky/tseitin_cycle_discovery.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace tactix {

struct TseitinCycleBreakingLimits {
  tseitin_cycle_live::SnapshotLimits snapshot;
  tseitin_cycle_discovery::Limits discovery;
  tseitin_cycle::AdmissionLimits admission;
  tseitin_cycle::Limits planning;
  tseitin_cycle::TransactionLimits transaction;
  uint64_t maximum_validation_ticks = 0;
  uint64_t maximum_components = 0;
  uint64_t maximum_identity_bytes = 0;
  uint64_t maximum_total_identity_bytes = 0;
  // Deterministic encoded/canonical identity-byte work.  Encoding is pure and
  // the cap is checked before any live mutation.
  uint64_t maximum_identity_work = 0;
  uint64_t maximum_journal_bytes = 0;
  uint64_t maximum_session_keys = 0;
  uint64_t maximum_session_work = 0;
  uint64_t maximum_session_identity_bytes = 0;
  bool valid() const;
};

class TseitinCycleBreakingSession {
public:
  bool retained() const { return owner_ != nullptr; }
  bool belongs_to(const Kernel &kernel) const { return owner_ == &kernel; }
  const std::vector<std::string> &keys() const { return keys_; }

private:
  friend struct TseitinCycleBreakingSessionAccess;
  const Kernel *owner_ = nullptr;
  std::vector<std::string> keys_;
};

enum class TseitinCycleBreakingStatus : uint8_t {
  Invalid,
  Inapplicable,
  NoStructure,
  ResourceExhausted,
  Interrupted,
  UnsupportedProof,
  Stale,
  AlreadyApplied,
  Applied,
};

struct TseitinCycleBreakingEffort {
  uint64_t snapshot_clauses = 0;
  uint64_t snapshot_literals = 0;
  uint64_t snapshot_work = 0;
  uint64_t recapture_work = 0;
  uint64_t discovery_work = 0;
  uint64_t retained_bytes = 0;
  uint64_t admission_work = 0;
  uint64_t planning_work = 0;
  uint64_t transaction_work = 0;
  uint64_t validation_ticks = 0;
  uint64_t candidates = 0;
  uint64_t offers = 0;
  uint64_t identity_bytes = 0;
  uint64_t identity_work = 0;
  uint64_t journal_work = 0;
  uint64_t journal_bytes = 0;
  uint64_t session_work = 0;
  uint64_t session_identity_bytes = 0;
  uint64_t proof_additions = 0;
  uint64_t proof_deletions = 0;
  uint64_t proof_literal_occurrences = 0;
  uint64_t proof_helpers = 0;
  uint64_t proof_only_variables = 0;
  uint64_t proof_witness_literals = 0;
  uint64_t proof_routes = 0;
};

struct TseitinCycleBreakingOfferResult {
  TseitinCycleBreakingStatus status = TseitinCycleBreakingStatus::Invalid;
  std::vector<std::string> offers;
  TseitinCycleBreakingEffort effort;
};

struct TseitinCycleBreakingResult {
  TseitinCycleBreakingStatus status = TseitinCycleBreakingStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  std::string identity;
  uint64_t proposed_action_id = 0;
  uint64_t committed_action_id = 0;
  TseitinCycleBreakingEffort effort;
  tseitin_cycle_live::Result live;
};

RuleEligibility tseitin_cycle_breaking_guard(const Kernel &);

// Enumerates every exact eligible connected component at this root boundary.
// The returned strings are complete typed payloads, not hashes or ranks.
TseitinCycleBreakingOfferResult tseitin_cycle_breaking_offer(
    Kernel &, const TseitinCycleBreakingSession &,
    const TseitinCycleBreakingLimits &);

// Freshly rebuilds all component evidence and exact-matches the selected
// payload before the existing live X7 adapter can publish one negative unit.
TseitinCycleBreakingResult tseitin_cycle_breaking_apply(
    Kernel &, TseitinCycleBreakingSession &,
    const TseitinCycleBreakingLimits &, const std::string &identity);

void record_tseitin_cycle_breaking_feedback(
    Kernel &, const TseitinCycleBreakingResult &);

} // namespace tactix
