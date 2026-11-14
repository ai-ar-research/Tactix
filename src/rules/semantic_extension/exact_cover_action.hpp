#pragma once

#include "rules/semantic_extension/exact_cover_live.hpp"

#include <cstdint>
#include <cstdio>

namespace tactix {

// Per-solver runtime state; nothing here is an action parameter.
struct ExactCoverExtensionActionSession {
  ExactCoverLiveSession live_session;
};

struct ExactCoverExtensionActionResult {
  // Zero means that the prospective structural identity was not committed.
  uint64_t action_id = 0;
  ExactCoverLiveResult live;
  bool semantic_candidate_staged = false;
};

// Formula/proof-state eligibility only. Configuration, provenance ownership,
// action identity, and retained lifecycle state belong to apply/session.
RuleEligibility exact_cover_extension_action_guard(const Kernel &);

// Apply the all-block action from Kernel parameters; the action identity
// commits only with a mutation or verdict, and fresh outputs are staged as
// IntroducedTheory candidates.
ExactCoverExtensionActionResult exact_cover_extension_action_apply(
    Kernel &, ExactCoverExtensionActionSession &);

} // namespace tactix
