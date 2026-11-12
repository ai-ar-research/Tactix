// Feature-local controller bridge for the certified affine-basis transaction.
#pragma once

#include "rules/semantic_extension/affine_basis_live.hpp"

#include <cstdint>
#include <vector>

namespace tactix {

// One session per Kernel; row and premise identities outlive their clauses.
class AffineBasisActionSession {
public:
  bool bound() const { return kernel_ != nullptr; }
  bool belongs_to(const Kernel &kernel) const { return kernel_ == &kernel; }
  uint64_t owner_version() const { return owner_version_; }
  size_t committed_keys() const { return keys_.size(); }
  const std::vector<AffineBasisConsumedKey> &keys() const { return keys_; }
  const AffineLiveSnapshot *owner() const {
    return bound() ? &owner_snapshot_ : nullptr;
  }

private:
  friend struct AffineBasisActionSessionAccess;
  const Kernel *kernel_ = nullptr;
  uint64_t owner_version_ = 0;
  AffineLiveSnapshot owner_snapshot_;
  std::vector<AffineBasisConsumedKey> keys_;
};

// Feature-local limits of the basis action.
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

// Allocation-free structural check of a row payload (not authentication).
bool affine_basis_row_selection_within_limits(
    const AffineBasisRowSelection &, const AffineBasisActionLimits &);

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
  AffineBasisRowSelection selection;
  AffineBasisLiveResult live;
};

enum class AffineBasisOfferStatus : uint8_t {
  Complete,
  NoStructure,
  Inapplicable,
  ResourceExhausted,
  Interrupted,
  Stale,
  Invalid,
};

struct AffineBasisOfferCursor {
  bool present = false;
  AffineBasisRowSelection last_examined;
};

struct AffineBasisOfferPage {
  AffineBasisOfferStatus status = AffineBasisOfferStatus::Invalid;
  uint64_t owner_version = 0;
  uint64_t candidates_examined = 0;
  uint64_t enumeration_work = 0;
  uint64_t preflight_recognition_work = 0;
  uint64_t preflight_basis_work = 0;
  uint64_t preflight_additions = 0;
  uint64_t preflight_deletions = 0;
  uint64_t duplicates = 0;
  uint64_t covered = 0;
  uint64_t stale = 0;
  bool more = false;
  AffineBasisOfferCursor continuation;
  std::vector<AffineBasisRowSelection> offers;
};

// A mutation-free page of row actions ordered contradiction, unit, binary,
// increasing width, then canonical payload.
AffineBasisOfferPage affine_basis_action_offers(
    Kernel &, const AffineBasisActionSession &,
    const AffineBasisActionLimits &, const AffineBasisOfferCursor &,
    uint32_t maximum_offers);

// Formula/proof-state guard only.  Limits and retained lifecycle state remain
// explicit controller inputs, and no heuristic or propagation work occurs.
RuleEligibility affine_basis_action_guard(const Kernel &);

AffineBasisActionResult affine_basis_action_apply(
    Kernel &, AffineBasisActionSession &, const AffineBasisActionLimits &);
AffineBasisActionResult affine_basis_action_apply(
    Kernel &, AffineBasisActionSession &, const AffineBasisActionLimits &,
    const AffineBasisRowSelection &);

} // namespace tactix
