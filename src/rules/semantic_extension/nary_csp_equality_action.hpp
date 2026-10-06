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

RuleEligibility nary_csp_equality_action_guard(const Kernel &);

// The prefix session supplies an exact retained source owner.  The equality
// session owns only committed equality keys.  All declines preserve both
// sessions, Kernel, proof, provenance and the structural action sequence.
NaryCspEqualityActionResult nary_csp_equality_action_apply(
    Kernel &, const NaryCspPrefixActionSession &, NaryCspEqualitySession &,
    const NaryCspEqualityLimits &,
    const NaryCspEqualityActionSelection &);

struct NaryCspEqualityActionOfferWindow {
  uint64_t maximum_owner_candidates = 0;
  uint64_t maximum_coordinate_candidates = 0;
  uint64_t maximum_offers = 0;
  uint64_t maximum_enumeration_work = 0;
  bool have_after = false;
  NaryCspEqualityKey after;

  bool valid() const;
};

enum class NaryCspEqualityActionOfferStatus : uint8_t {
  Invalid,
  Inapplicable,
  ResourceExhausted,
  Interrupted,
  Stale,
  Ready,
};

struct NaryCspEqualityActionOfferEffort {
  uint64_t retained_owners = 0;
  uint64_t owner_candidates = 0;
  uint64_t coordinate_candidates = 0;
  uint64_t committed_keys_skipped = 0;
  uint64_t enumeration_work = 0;
};

struct NaryCspEqualityActionOfferResult {
  NaryCspEqualityActionOfferStatus status =
      NaryCspEqualityActionOfferStatus::Invalid;
  bool complete = false;
  bool have_resume_after = false;
  NaryCspEqualityKey resume_after;
  std::vector<NaryCspEqualityActionSelection> offers;
  NaryCspEqualityActionOfferEffort effort;
};

// Enumerate only canonical equality coordinate choices from exact owners that
// X17 has already retained.  The offerer is read-only and caller-bounded.
NaryCspEqualityActionOfferResult nary_csp_equality_action_offer(
    Kernel &, const NaryCspPrefixActionSession &,
    const NaryCspEqualitySession &, const NaryCspEqualityLimits &,
    const NaryCspEqualityActionOfferWindow &);

} // namespace tactix
