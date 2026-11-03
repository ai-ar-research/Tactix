#pragma once

#include "rules/semantic_extension/cardinality_search_live.hpp"

namespace tactix {

struct CardinalitySearchBound {
  std::vector<int32_t> literals;
  uint32_t lower = 0;
  uint32_t upper = 0;
  bool operator==(const CardinalitySearchBound &) const = default;
};

struct CardinalitySearchActionLimits {
  cardinality_search::Limits inspection;
  cardinality_search::ResolutionLimits resolution;
  uint64_t maximum_source_scan_work = 1000000;
  uint64_t maximum_arena_words = 1024;
  uint64_t maximum_journal_bytes = 100000;
  bool valid() const;
};

// The action reports its live adapter's status unchanged.
using CardinalitySearchActionStatus = CardinalitySearchLiveStatus;

struct CardinalitySearchActionResult {
  CardinalitySearchActionStatus status =
      CardinalitySearchActionStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t proposed_action_id = 0;
  uint64_t committed_action_id = 0;
  CardinalitySearchLiveResult live;
};

RuleEligibility cardinality_search_action_guard(const Kernel &);
CardinalitySearchActionResult cardinality_search_action_apply(
    Kernel &, const std::vector<CardinalitySearchBound> &,
    const CardinalitySearchActionLimits &);

} // namespace tactix
