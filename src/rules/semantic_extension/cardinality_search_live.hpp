#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"
#include "rules/semantic_extension/cardinality_search.hpp"

#include <cstdio>
#include <vector>

namespace tactix {

struct CardinalitySearchLiveRequest {
  std::vector<cardinality_search::Constraint> constraints;
  cardinality_search::Limits inspection;
  cardinality_search::ResolutionLimits resolution;
  uint64_t maximum_source_scan_work = 1000000;
  uint64_t maximum_arena_words = 1024;
  uint64_t maximum_journal_bytes = 100000;
  uint64_t action_id = 0;
  uint64_t semantic_id = 0;
  FILE *records = nullptr;
};

enum class CardinalitySearchLiveStatus : uint8_t {
  Invalid,
  Inapplicable,
  UnsupportedProof,
  NoSource,
  NoEvent,
  ResourceExhausted,
  Interrupted,
  Applied,
};

struct CardinalitySearchLiveResult {
  CardinalitySearchLiveStatus status = CardinalitySearchLiveStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  std::vector<int32_t> reason;
  std::vector<uint64_t> source_ids;
  std::vector<uint64_t> proof_ids;
  uint64_t source_scan_work = 0;
  uint64_t inspection_work = 0;
  uint64_t resolution_work = 0;
  uint64_t added_literals = 0;
  uint64_t installed_clause_id = 0;
  bool propagation_pending = false;
};

CardinalitySearchLiveResult cardinality_search_live_inspect(
    Kernel &, const CardinalitySearchLiveRequest &);
CardinalitySearchLiveResult cardinality_search_live_apply(
    Kernel &, const CardinalitySearchLiveRequest &);

} // namespace tactix
