#pragma once

#include "rules/semantic_extension/gf3_general_row_live.hpp"

#include <cstdint>
#include <vector>

namespace tactix {

struct Gf3GeneralRowActionLimits {
  Gf3GeneralRowLiveLimits live;
  uint64_t maximum_session_keys = 0;
  uint64_t maximum_session_work = 0;
  uint64_t maximum_session_identity_bytes = 0;

  bool valid() const;
};

class Gf3GeneralRowActionSession {
public:
  bool retained() const { return owner_ != nullptr; }
  bool belongs_to(const Kernel &kernel) const { return owner_ == &kernel; }
  const Gf3GeneralRowOwner *owner() const {
    return retained() ? &snapshot_ : nullptr;
  }
  const std::vector<Gf3GeneralRowActionSelection> &keys() const {
    return keys_;
  }

private:
  friend struct Gf3GeneralRowActionSessionAccess;
  const Kernel *owner_ = nullptr;
  Gf3GeneralRowOwner snapshot_;
  std::vector<Gf3GeneralRowActionSelection> keys_;
};

enum class Gf3GeneralRowActionStatus : uint8_t {
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

struct Gf3GeneralRowActionResult {
  Gf3GeneralRowActionStatus status = Gf3GeneralRowActionStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t proposed_action_id = 0;
  uint64_t committed_action_id = 0;
  uint64_t semantic_id = 0;
  uint64_t session_work = 0;
  uint64_t session_identity_bytes = 0;
  Gf3GeneralRowLiveResult live;
};

RuleEligibility gf3_general_row_action_guard(const Kernel &);
bool gf3_general_row_action_selection_valid(
    const Gf3GeneralRowActionSelection &);

Gf3GeneralRowActionResult gf3_general_row_action_apply(
    Kernel &, Gf3GeneralRowActionSession &,
    const Gf3GeneralRowActionLimits &, const Gf3GeneralRowActionSelection &);

struct Gf3GeneralRowActionOfferWindow {
  uint64_t maximum_candidates = 0;
  uint64_t maximum_offers = 0;
  uint64_t maximum_enumeration_work = 0;
  bool have_after = false;
  Gf3GeneralRowActionSelection after;

  bool valid() const;
};

struct Gf3GeneralRowActionOfferResult {
  Gf3GeneralRowActionStatus status = Gf3GeneralRowActionStatus::Invalid;
  bool complete = false;
  bool have_resume_after = false;
  Gf3GeneralRowActionSelection resume_after;
  std::vector<Gf3GeneralRowActionSelection> offers;
  uint64_t candidates = 0;
  uint64_t committed_keys_skipped = 0;
  uint64_t enumeration_work = 0;
  Gf3GeneralRowLiveEffort live_effort;
};

// Enumerates exact (outcome, tuple, authenticated identity) actions in
// lexicographic order.  A partial page resumes strictly after resume_after.
Gf3GeneralRowActionOfferResult gf3_general_row_action_offer(
    Kernel &, const Gf3GeneralRowActionSession &,
    const Gf3GeneralRowActionLimits &,
    const Gf3GeneralRowActionOfferWindow &);

void record_gf3_general_row_action_feedback(
    Kernel &, const Gf3GeneralRowActionResult &);

} // namespace tactix
