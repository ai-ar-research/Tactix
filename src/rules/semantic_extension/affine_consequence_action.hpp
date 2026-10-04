#pragma once

#include "rules/semantic_extension/affine_basis_action.hpp"
#include "rules/semantic_extension/affine_live.hpp"

#include <cstdint>

namespace tactix {

enum class AffineConsequenceActionStatus : uint8_t {
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

struct AffineConsequenceActionResult {
  // The prospective identity is observable for deterministic replay, but the
  // Kernel sequence advances only when the live adapter commits a mutation or
  // terminal verdict.  Zero committed_action_id means a mutation-free decline.
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

// Formula/proof-state eligibility only.  Typed limits and the selected
// consequence remain controller-owned execution parameters rather than hidden
// scheduling predicates.
RuleEligibility affine_consequence_action_guard(const Kernel &);

// Apply the selected affine consequence using Kernel-owned typed parameters.
// Productive continuing applications stage neutral ExistingTheory candidates;
// registry dispatch owns their publication and the strategy-epoch transition.
// Root propagation is deliberately left to a following Propagate action.
AffineConsequenceActionResult affine_consequence_action_apply(Kernel &);
AffineConsequenceActionResult affine_consequence_action_apply(
    Kernel &, AffineBasisActionSession &);
ApplyResult affine_consequence_rule_apply(Kernel &);

} // namespace tactix
