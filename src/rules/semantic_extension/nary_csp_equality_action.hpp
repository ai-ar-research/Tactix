// Controller-neutral wrapper for one compact aligned-domain equality action.
#pragma once

#include "rules/semantic_extension/nary_csp_equality_live.hpp"
#include "rules/semantic_extension/nary_csp_prefix_action.hpp"

#include <cstdint>
#include <vector>

namespace tactix {

struct NaryCspEqualityActionSelection {
  std::vector<std::vector<int32_t>> domains;
  nary_csp_equality::Selection coordinates;

  bool operator==(const NaryCspEqualityActionSelection &) const = default;
};

bool nary_csp_equality_action_selection_valid(
    const NaryCspEqualityActionSelection &);
bool nary_csp_equality_action_selection_within_limits(
    const NaryCspEqualityActionSelection &, const NaryCspEqualityLimits &);

enum class NaryCspEqualityActionStatus : uint8_t {
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

struct NaryCspEqualityActionResult {
  NaryCspEqualityActionStatus status = NaryCspEqualityActionStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t proposed_action_id = 0;
  uint64_t committed_action_id = 0;
  uint64_t semantic_id = 0;
  bool semantic_candidate_ready = false;
  std::vector<strategy::SemanticVariable> semantic_variables;
  NaryCspEqualityLiveResult live;
};

// The prefix session supplies an exact retained source owner.  The equality
// session owns only committed equality keys.  All declines preserve both
// sessions, Kernel, proof, provenance and the structural action sequence.
NaryCspEqualityActionResult nary_csp_equality_action_apply(
    Kernel &, const NaryCspPrefixActionSession &, NaryCspEqualitySession &,
    const NaryCspEqualityLimits &,
    const NaryCspEqualityActionSelection &);

} // namespace tactix
