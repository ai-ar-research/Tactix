#pragma once

#include "policies/default_strategy_config.hpp"
#include "policies/heuristics.hpp"

#include <cmath>
#include <cstdint>

namespace tactix {

// Mode switching owns only its two reference clocks. It reports entry into
// focused mode so the composing policy can reset the independent restart
// clock at the same boundary as before.
class DefaultModeControl {
public:
  struct State {
    uint64_t count = 0;
    uint64_t conflicts_limit =
        DefaultStrategyConfig::MODE_INITIAL_CONFLICTS_DEFAULT;
    uint64_t ticks_limit = 0;
    uint64_t ticks_at_focus_entry = 0;
  } mode;

  bool wants_switch(const Kernel &k, uint64_t search_ticks) const {
    return mode.count & 1 ? search_ticks >= mode.ticks_limit
                          : k.stats.conflicts >= mode.conflicts_limit;
  }

  bool update_limits(const Kernel &k,
                     const DefaultStrategyConfig::Mode &settings,
                     uint64_t search_ticks) {
    if (mode.count & 1) {
      const uint64_t focused_ticks =
          search_ticks - mode.ticks_at_focus_entry;
      mode.ticks_limit = search_ticks + focused_ticks;
      return false;
    }
    const uint64_t cycle = mode.count / 2 ? mode.count / 2 : 1;
    const double scale = std::pow(
        std::log10(static_cast<double>(cycle) + settings.logarithm_offset),
        settings.logarithm_power);
    mode.conflicts_limit =
        k.stats.conflicts +
        static_cast<uint64_t>(settings.interval *
                              static_cast<double>(cycle) * scale);
    mode.ticks_at_focus_entry = search_ticks;
    return true;
  }
};

class DefaultDatabaseControl {
public:
  struct State {
    uint64_t next_conflicts =
        DefaultStrategyConfig::REDUCE_INTERVAL_DEFAULT;
  } reduce;

  bool wants_reduce(const Kernel &k) const {
    return k.stats.conflicts >= reduce.next_conflicts;
  }

  void update_limit(const Kernel &k,
                    const DefaultStrategyConfig::Reduction &settings,
                    bool square_root_cadence) {
    if (square_root_cadence) {
      const double n = static_cast<double>(
          k.stats.reductions ? k.stats.reductions : 1);
      reduce.next_conflicts =
          k.stats.conflicts +
          static_cast<uint64_t>(settings.initial_interval * std::sqrt(n));
    } else {
      reduce.next_conflicts =
          k.stats.conflicts + settings.arithmetic_interval +
          settings.arithmetic_step * k.stats.reductions;
    }
  }
};

class DefaultPhaseControl {
public:
  struct RephaseState {
    uint64_t next_conflicts =
        DefaultStrategyConfig::REPHASE_INTERVAL_DEFAULT;
    uint64_t count = 0;
  } rephase;
  struct ReorderState {
    uint64_t next_conflicts =
        DefaultStrategyConfig::REORDER_INITIAL_INTERVAL_DEFAULT;
    uint64_t count = 0;
  } reorder;

  bool wants_rephase(const Kernel &k,
                     const DefaultStrategyConfig::Rephase &settings) const {
    return (!settings.stable_only || k.h->stable) &&
           k.stats.conflicts > rephase.next_conflicts;
  }

  bool wants_reorder(const Kernel &k,
                     const DefaultStrategyConfig::Reorder &settings) const {
    if (!settings.mode)
      return false;
    if (!k.h->stable && settings.mode < 2)
      return false;
    return k.level == 0 && k.stats.conflicts >= reorder.next_conflicts;
  }

  void update_reorder_limit(const Kernel &k,
                            const DefaultStrategyConfig::Reorder &settings) {
    reorder.next_conflicts =
        k.stats.conflicts + settings.interval * reorder.count;
  }

  void update_rephase_limit(const Kernel &k,
                            const DefaultStrategyConfig::Rephase &settings) {
    const double n = static_cast<double>(rephase.count);
    const double lg = std::log10(n + settings.logarithm_offset);
    rephase.next_conflicts =
        k.stats.conflicts +
        static_cast<uint64_t>(settings.interval * n *
                              std::pow(lg, settings.logarithm_power));
  }
};

} // namespace tactix
