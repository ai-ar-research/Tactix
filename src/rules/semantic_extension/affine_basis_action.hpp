// Feature-local controller bridge for the certified affine-basis transaction.
#pragma once

#include "rules/semantic_extension/affine_basis_live.hpp"

#include <cstdint>
#include <vector>

namespace tactix {

// The controller owns one session for the lifetime of one Kernel.  Exact row
// and premise identities survive deletion of the clauses published by an
// earlier action, preventing a replay from silently consuming a new action id.
class AffineBasisActionSession {
public:
  bool bound() const { return owner_ != nullptr; }
  bool belongs_to(const Kernel &kernel) const { return owner_ == &kernel; }
  size_t committed_keys() const { return keys_.size(); }
  const std::vector<AffineBasisConsumedKey> &keys() const { return keys_; }

private:
  friend struct AffineBasisActionSessionAccess;
  const Kernel *owner_ = nullptr;
  std::vector<AffineBasisConsumedKey> keys_;
};

// These are deliberately feature-local until the public AffineConsequence
// parameter and replay surfaces are migrated to the basis action.
struct AffineBasisActionLimits {
  affine_semantic_extension::Limits image;
  affine_semantic_extension::BasisPlanLimits basis;
  uint64_t maximum_snapshot_literals = 0;
  uint64_t maximum_snapshot_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_deletions = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t maximum_session_keys = 0;
  uint64_t maximum_session_work = 0;

  bool valid() const;
};

enum class AffineBasisActionStatus : uint8_t {
  Invalid,
  Inapplicable,
  NoStructure,
  ResourceExhausted,
  Interrupted,
  UnsupportedProof,
  Stale,
  AlreadyPublished,
  Applied,
};

struct AffineBasisActionResult {
  AffineBasisActionStatus status = AffineBasisActionStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t proposed_action_id = 0;
  uint64_t committed_action_id = 0;
  uint64_t semantic_id = 0;
  uint64_t session_work = 0;
  uint64_t transaction_keys = 0;
  uint64_t committed_keys = 0;
  bool candidate_staged = false;
  AffineBasisLiveResult live;
};

// Formula/proof-state guard only.  Limits and retained lifecycle state remain
// explicit controller inputs, and no heuristic or propagation work occurs.
RuleEligibility affine_basis_action_guard(const Kernel &);

AffineBasisActionResult affine_basis_action_apply(
    Kernel &, AffineBasisActionSession &, const AffineBasisActionLimits &);

} // namespace tactix
