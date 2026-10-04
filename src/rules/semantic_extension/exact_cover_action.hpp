#pragma once

#include "rules/semantic_extension/exact_cover_live.hpp"

#include <cstdint>
#include <cstdio>

namespace tactix {

// Feature-local runtime state. The eventual controller owns this object for
// one solver lifetime; none of its identities or retained duplicate keys are
// serialized as action parameters.
struct ExactCoverExtensionActionSession {
  ExactCoverLiveSession live_session;
  FILE *records = nullptr;
  uint64_t last_committed_structural_action_id = 0;
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

// Apply the one canonical all-block action using the exact typed parameters
// currently installed in the kernel. A prospective structural action identity
// becomes committed session state only when the live adapter reports an
// applied mutation or terminal verdict. Productive continuing actions stage
// their fresh outputs as neutral IntroducedTheory candidates for registry
// dispatch to publish after its state transition.
ExactCoverExtensionActionResult exact_cover_extension_action_apply(
    Kernel &, ExactCoverExtensionActionSession &);

} // namespace tactix
