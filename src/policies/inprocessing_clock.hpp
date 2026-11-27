#pragma once

// Plan 5.6 central conflict clock for search-time Probe and Eliminate.
// Ticks fall on one grid from the search origin: every `base` conflicts
// (fixed) or after base * Luby(i) conflicts for i = 1, 2, ... (Luby). A tick
// only opens a check; pass deadlines, eligibility and effort stay with the
// inprocessing scheduler. The event cadence uses no grid: the search loop
// opens a check when DefaultInprocessScheduler::due_state changes.

#include "policies/default_strategy_config.hpp"

#include <cstdint>

namespace tactix {

class InprocessingClock {
public:
  void start(uint32_t cadence, uint64_t base, uint64_t origin) {
    cadence_ = cadence;
    base_ = base;
    luby_u_ = luby_v_ = 1;
    pending_ = false;
    next_ = enabled() ? advance(origin) : UINT64_MAX;
  }
  bool enabled() const {
    return cadence_ != DefaultStrategyConfig::INPROCESSING_CLOCK_PER_DECISION;
  }
  // Conflict handling may cross several ticks; they open one pending check.
  void observe(uint64_t conflicts) {
    while (conflicts >= next_) {
      pending_ = true;
      next_ = advance(next_);
    }
  }
  // A natural restart is a checkpoint between ticks; it leaves the grid as is.
  void mark_pending() { pending_ = enabled(); }
  bool pending() const { return pending_; }
  void checked() { pending_ = false; }
  uint64_t next() const { return next_; }

private:
  uint64_t advance(uint64_t from) {
    const uint64_t multiplier = interval_multiplier();
    if (multiplier && base_ > UINT64_MAX / multiplier) return UINT64_MAX;
    const uint64_t step = base_ * multiplier;
    return from > UINT64_MAX - step ? UINT64_MAX : from + step;
  }
  // Knuth's reluctant-doubling recurrence yields Luby 1,1,2,1,1,2,4,...
  uint64_t interval_multiplier() {
    if (cadence_ != DefaultStrategyConfig::INPROCESSING_CLOCK_LUBY)
      return 1;
    const uint64_t current = luby_v_;
    if ((luby_u_ & (~luby_u_ + 1)) == luby_v_) {
      luby_u_++;
      luby_v_ = 1;
    } else {
      luby_v_ *= 2;
    }
    return current;
  }

  uint32_t cadence_ = DefaultStrategyConfig::INPROCESSING_CLOCK_PER_DECISION;
  uint64_t base_ = 1, next_ = UINT64_MAX;
  uint64_t luby_u_ = 1, luby_v_ = 1;
  bool pending_ = false;
};

} // namespace tactix
