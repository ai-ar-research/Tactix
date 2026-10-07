#pragma once

#include "rules/semantic_extension/direct_csp_guarded_cover.hpp"
#include <functional>

namespace tactix::direct_csp_guarded_cover::proposals {

struct Limits {
  uint64_t maximum_work = 0;
  uint64_t maximum_proposals = 0;
  uint64_t maximum_single_moves = 0;
  uint64_t maximum_pair_moves = 0;
  uint64_t maximum_repair_steps = 0;
  uint64_t maximum_visited_keys = 0;
  uint64_t maximum_nominations = 0;
  bool valid() const;
};

enum class Status : uint8_t {
  Invalid, IncompleteTheory, NoProgress, ResourceExhausted, Interrupted,
  Nominated,
};

struct Effort {
  uint64_t work = 0;
  uint64_t proposals = 0;
  uint64_t single_moves = 0;
  uint64_t pair_moves = 0;
  uint64_t repair_steps = 0;
  uint64_t visited_keys = 0;
  uint64_t planner_work = 0;
};

struct Nomination {
  Selection selection;
  Addition addition;
};

struct Result {
  Status status = Status::Invalid;
  Effort effort;
  std::vector<Nomination> nominations;
  // Only the last local repair neighborhood, never all possible tuples.
  bool neighborhood_complete = false;
  bool nomination_limit_reached = false;
};

// Preferences replace owner.selected_values: one explicit index per domain.
// This is a
// deterministic, incomplete proposal search, not a SAT search or novelty test.
// Interrupted/resource/malformed results contain no nominations; recoverable
// local no-progress may return already authenticated nominations as Nominated.
Result propose(const std::vector<Premise> &, const Selection &owner,
               const std::vector<uint32_t> &preferred_values, const Bounds &,
               const Limits &, std::function<bool()> interrupted = {});

} // namespace tactix::direct_csp_guarded_cover::proposals
