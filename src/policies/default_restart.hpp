#pragma once

#include "policies/default_strategy_config.hpp"
#include "policies/heuristics.hpp"

#include <cmath>

namespace tactix {

// Cohesive restart scheduling state, independent of the DefaultPolicy search
// loop. The caller supplies strategy settings; this component never applies a
// proof rule or resets solver state. Mode-local averages and reluctant clocks
// retain their histories until the caller explicitly resets them.
class DefaultRestartControl {
public:
  struct Smooth {
    double value = 0, biased = 0, alpha = 0, beta = 0, exp = 1.0;
    void init(double window) {
      alpha = 1.0 / window;
      beta = 1.0 - alpha;
      value = biased = 0;
      exp = 1.0;
    }
    void update(double y) {
      biased += alpha * (y - biased);
      if (!exp) {
        value = biased;
        return;
      }
      const double old_exp = exp;
      exp *= beta;
      if (exp == old_exp) {
        exp = 0;
        value = biased;
      } else {
        value = biased / (1.0 - exp);
      }
    }
  };
  struct ModeAverages {
    bool initialized = false;
    Smooth fast_glue, slow_glue, decision_rate;
    uint64_t saved_decisions = 0;
  };

  ModeAverages averages[2];
  struct {
    uint64_t next_conflicts = DefaultStrategyConfig::RESTART_INTERVAL_DEFAULT;
    uint64_t counted = 0, interval = 0; // Luby index or geometric interval
  } restart;

  enum Policy : uint32_t { NATIVE, GLUE, LUBY, GEOMETRIC, FIXED, NONE };
  struct {
    uint64_t u = 1, v = 1, wait = DefaultStrategyConfig::RELUCTANT_PERIOD_DEFAULT;
    bool trigger = false;
  } reluctant;

  void reset(const Kernel &k, const DefaultStrategyConfig::Restart &settings) {
    averages[0] = averages[1] = {};
    restart.counted = restart.interval = 0;
    if (counted(settings)) schedule_counted(k, settings);
    else update_restart_limit(k, settings);
    reluctant = {};
    reluctant.wait = settings.reluctant_period;
  }

  void mode_entered(const Kernel &k, const DefaultStrategyConfig::Restart &settings) {
    averages[k.h->stable].saved_decisions = k.stats.decisions;
    if (k.h->stable) {
      reluctant = {};
      reluctant.wait = settings.reluctant_period;
    }
  }

  void restart_completed(const Kernel &k,
                         const DefaultStrategyConfig::Restart &settings) {
    reluctant.trigger = false;
    if (counted(settings)) schedule_counted(k, settings);
    else if (settings.policy == GLUE || !k.h->stable) update_restart_limit(k, settings);
  }

  void observe_glue(const Kernel &k, const DefaultStrategyConfig::Restart &settings,
                    uint32_t glue) {
    ModeAverages &avg = averages[k.h->stable];
    if (!avg.initialized) { // lazy, once per mode (kissat init_averages)
      avg.fast_glue.init(settings.fast_ema_window);
      avg.slow_glue.init(settings.slow_ema_window);
      avg.decision_rate.init(settings.slow_ema_window);
      avg.initialized = true;
    }
    avg.fast_glue.update(glue);
    avg.slow_glue.update(glue);
  }

  void observe_decision_rate(const Kernel &k,
                             const DefaultStrategyConfig::Restart &settings) {
    ModeAverages &avg = averages[k.h->stable];
    if (!avg.initialized) {
      avg.fast_glue.init(settings.fast_ema_window);
      avg.slow_glue.init(settings.slow_ema_window);
      avg.decision_rate.init(settings.slow_ema_window);
      avg.initialized = true;
    }
    const uint64_t cur = k.stats.decisions;
    const uint64_t prev = avg.saved_decisions;
    avg.saved_decisions = cur;
    avg.decision_rate.update(static_cast<double>(cur - prev));
  }

  void tick_reluctant(const DefaultStrategyConfig::Restart &settings) {
    if (reluctant.trigger)
      return;
    if (--reluctant.wait)
      return;
    uint64_t u = reluctant.u, v = reluctant.v;
    if ((u & -u) == v) {
      u++;
      v = 1;
    } else {
      v *= 2;
    }
    uint64_t wait = v * settings.reluctant_period;
    if (wait > settings.reluctant_limit) {
      u = v = 1;
      wait = settings.reluctant_period;
    }
    reluctant.trigger = true;
    reluctant.wait = wait;
    reluctant.u = u;
    reluctant.v = v;
  }

  bool want_restart(const Kernel &k,
                    const DefaultStrategyConfig::Restart &settings) const {
    if (settings.policy == NONE || k.stats.conflicts < restart.next_conflicts)
      return false;
    if (counted(settings))
      return true;
    if (settings.policy == NATIVE && k.h->stable)
      return reluctant.trigger;
    const ModeAverages &avg = averages[settings.policy == GLUE && k.h->stable];
    return settings.glue_margin * avg.slow_glue.value <=
           avg.fast_glue.value;
  }

  void update_restart_limit(const Kernel &k,
                            const DefaultStrategyConfig::Restart &settings) {
    if (counted(settings)) return; // only a restart advances a counted schedule
    uint64_t delta = settings.initial_interval;
    if (k.stats.restarts)
      delta += static_cast<uint64_t>(
          std::log10(static_cast<double>(k.stats.restarts) +
                     settings.logarithm_offset) -
          settings.logarithm_bias);
    restart.next_conflicts = k.stats.conflicts + delta;
  }

private:
  static bool counted(const DefaultStrategyConfig::Restart &settings) {
    return settings.policy == LUBY || settings.policy == GEOMETRIC ||
           settings.policy == FIXED;
  }

  // The i-th element (1-based) of the Luby sequence 1, 1, 2, 1, 1, 2, 4, ...
  static uint64_t luby(uint64_t i) {
    for (;;) {
      uint64_t k = 1;
      while ((uint64_t{1} << k) - 1 < i) ++k;
      if ((uint64_t{1} << k) - 1 == i) return uint64_t{1} << (k - 1);
      i -= (uint64_t{1} << (k - 1)) - 1;
    }
  }

  static uint64_t scaled(uint64_t value, uint64_t numerator, uint64_t denominator) {
    const unsigned __int128 product = static_cast<unsigned __int128>(value) * numerator / denominator;
    return product > UINT64_MAX ? UINT64_MAX : static_cast<uint64_t>(product);
  }

  // The next counted deadline after a restart (or the first, after reset).
  void schedule_counted(const Kernel &k, const DefaultStrategyConfig::Restart &settings) {
    const uint64_t base = settings.base_interval;
    if (settings.policy == LUBY)
      restart.interval = scaled(base, luby(++restart.counted), 1);
    else if (settings.policy == GEOMETRIC && restart.interval)
      restart.interval = scaled(restart.interval, settings.growth_permille, 1000);
    else
      restart.interval = base; // fixed, or the first geometric interval
    restart.next_conflicts = k.stats.conflicts > UINT64_MAX - restart.interval
                                 ? UINT64_MAX : k.stats.conflicts + restart.interval;
  }
};

} // namespace tactix
