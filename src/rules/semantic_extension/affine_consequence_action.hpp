#pragma once

#include "rules/semantic_extension/affine_basis_action.hpp"
#include "rules/semantic_extension/affine_live.hpp"

#include <cstdint>

namespace tactix {

// The action reports the certified-basis status vocabulary unchanged.
using AffineConsequenceActionStatus = AffineBasisActionStatus;

struct AffineConsequenceActionResult {
  // The sequence advances only on a commit; zero committed_action_id means a
  // mutation-free decline.
  uint64_t proposed_action_id = 0;
  uint64_t committed_action_id = 0;
  uint64_t semantic_id = 0;
  strategy::AffineConsequenceSemantics semantics =
      strategy::AffineConsequenceSemantics::SelectedConsequenceV1;
  AffineConsequenceActionStatus status =
      AffineConsequenceActionStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  AffineLiveResult live;
  AffineBasisActionResult basis;
  bool candidate_staged = false;
};

// Formula/proof-state eligibility only.
RuleEligibility affine_consequence_action_guard(const Kernel &);

// Apply the selected consequence from Kernel parameters, staging
// ExistingTheory candidates; root propagation is left pending.
AffineConsequenceActionResult affine_consequence_action_apply(Kernel &);
AffineConsequenceActionResult affine_consequence_action_apply(
    Kernel &, AffineBasisActionSession &, const AffineBasisRowSelection &);

} // namespace tactix
