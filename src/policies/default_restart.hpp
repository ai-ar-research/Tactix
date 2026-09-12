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
  } restart;
  struct {
    uint64_t u = 1, v = 1, wait = DefaultStrategyConfig::RELUCTANT_PERIOD_DEFAULT;
    bool trigger = false;
  } reluctant;

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
    if (k.stats.conflicts < restart.next_conflicts)
      return false;
    if (k.h->stable)
      return reluctant.trigger;
    const ModeAverages &avg = averages[0];
    return settings.glue_margin * avg.slow_glue.value <=
           avg.fast_glue.value;
  }

  void update_restart_limit(const Kernel &k,
                            const DefaultStrategyConfig::Restart &settings) {
    uint64_t delta = settings.initial_interval;
    if (k.stats.restarts)
      delta += static_cast<uint64_t>(
          std::log10(static_cast<double>(k.stats.restarts) +
                     settings.logarithm_offset) -
          settings.logarithm_bias);
    restart.next_conflicts = k.stats.conflicts + delta;
  }
};

} // namespace tactix
