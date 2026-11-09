#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "api/api_internal.hpp"
#include "policies/default_policy.hpp"
#include <cassert>
#include <cstring>
#include <limits>

using namespace tactix;

static void same(tactix_c_handle *s, tactix_restart_clock *clock,
                 const DefaultPolicy &native, const Kernel &k) {
  tactix_restart_clock_state a{}, b{};
  assert(!tactix_restart_clock_observe(s, clock, &a));
  assert(!tactix_restart_clock_observe(s, clock, &b));
  assert(!std::memcmp(&a, &b, sizeof a));
  assert(bool(a.due) == native.want_restart(k));
  assert(a.next_conflicts == native.restart.next_conflicts);
  assert(a.reluctant_trigger == native.reluctant.trigger);
  assert(a.reluctant_u == native.reluctant.u && a.reluctant_v == native.reluctant.v);
  assert(a.reluctant_wait == native.reluctant.wait);
  for (unsigned mode = 0; mode != 2; ++mode) {
    const auto &n = native.averages[mode];
    assert(a.fast_glue[mode] == n.fast_glue.value);
    assert(a.slow_glue[mode] == n.slow_glue.value);
    assert(a.decision_rate[mode] == n.decision_rate.value);
    assert(bool(a.initialized[mode]) == n.initialized);
    assert(a.saved_decisions[mode] == n.saved_decisions);
  }
}

int main() {
  assert(!tactix_restart_clock_create(nullptr, nullptr));
  tactix_restart_clock_release(nullptr);
  auto *s = tactix_init();
  Kernel &k = api_detail::kernel_for_fixed_testing(s);
  assert(!tactix_add_checked(s, 1)); // clock operations must not close this clause
  auto *clock = tactix_restart_clock_create(s, nullptr);
  assert(clock);
  tactix_restart_clock_parameters defaults{};
  assert(!tactix_restart_clock_get_parameters(clock, &defaults));
  assert(!tactix_restart_clock_update(s, clock, TACTIX_RESTART_CLOCK_GLUE, 4));
  assert(!tactix_add_checked(s, 0));
  assert(!tactix_add_checked(s, 2));
  assert(!tactix_restart_clock_configure(s, clock, &defaults));
  assert(!tactix_add_checked(s, 0));

  for (unsigned profile = 0; profile != 2; ++profile) {
    DefaultStrategyConfig config;
    if (profile) {
      config.restart.fast_ema_window = 3;
      config.restart.slow_ema_window = 7;
      config.restart.reluctant_period = 2;
      config.restart.reluctant_limit = 16;
    }
    const auto &p = config.restart;
    tactix_restart_clock_parameters parameters{p.initial_interval, p.reluctant_period,
        p.reluctant_limit, p.glue_margin, p.fast_ema_window, p.slow_ema_window,
        p.logarithm_offset, p.logarithm_bias};
    DefaultPolicy native(config);
    k.stats.conflicts = k.stats.restarts = k.stats.decisions = 0;
    native.reset_schedule(k);
    assert(!tactix_restart_clock_configure(s, clock, &parameters));
    for (unsigned i = 0; i != 4096; ++i) {
      const bool stable = i % 8 >= 4, previous = k.h->stable;
      k.h->stable = stable;
      k.stats.conflicts = i; k.stats.restarts = i / 5;
      k.stats.decisions += i % 7;
      const auto epoch = k.strategy_epoch, touch = k.touch_epoch, ids = k.next_clause_id;
      const auto trail = k.trail;
      if (stable != previous) {
        native.averages[stable].saved_decisions = k.stats.decisions;
        if (stable) native.reluctant = {};
        assert(!tactix_restart_clock_update(s, clock, TACTIX_RESTART_CLOCK_MODE_ENTERED, 0));
      }
      native.DefaultRestartControl::observe_decision_rate(k, p);
      assert(!tactix_restart_clock_update(s, clock, TACTIX_RESTART_CLOCK_DECISION_RATE, 0));
      const uint32_t glue = 7 * i % 19;
      native.DefaultRestartControl::observe_glue(k, p, glue);
      assert(!tactix_restart_clock_update(s, clock, TACTIX_RESTART_CLOCK_GLUE, glue));
      if (stable) {
        native.tick_reluctant();
        assert(!tactix_restart_clock_update(s, clock, TACTIX_RESTART_CLOCK_RELUCTANT, 0));
      }
      if (i % 5 == 0) {
        native.reluctant.trigger = false;
        if (!stable) native.update_restart_limit(k);
        assert(!tactix_restart_clock_update(s, clock, TACTIX_RESTART_CLOCK_RESTARTED, 0));
      }
      if (i % 17 == 0) {
        native.reset_schedule(k);
        assert(!tactix_restart_clock_update(s, clock, TACTIX_RESTART_CLOCK_RESET, 0));
      }
      same(s, clock, native, k);
      assert(k.strategy_epoch == epoch && k.touch_epoch == touch &&
             k.next_clause_id == ids && k.trail == trail);
    }
    tactix_restart_clock_state before{}, after{};
    assert(!tactix_restart_clock_observe(s, clock, &before));
    auto invalid = parameters;
    for (double value : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::quiet_NaN()}) {
      invalid.glue_margin = value;
      assert(tactix_restart_clock_configure(s, clock, &invalid) == -1);
    }
    invalid = parameters; invalid.reluctant_period = 0;
    assert(tactix_restart_clock_configure(s, clock, &invalid) == -1);
    invalid = parameters; invalid.logarithm_bias = 1000;
    assert(!tactix_restart_clock_create(s, &invalid));
    assert(tactix_restart_clock_configure(s, clock, &invalid) == -1);
    assert(tactix_restart_clock_update(s, clock, 99, 0) == -1);
    assert(tactix_restart_clock_update(s, clock, TACTIX_RESTART_CLOCK_RESET, 1) == -1);
    assert(!tactix_restart_clock_observe(s, clock, &after));
    assert(!std::memcmp(&before, &after, sizeof before));
    same(s, clock, native, k);
  }
  tactix_release(s);
  // An owned clock retains no kernel or view pointer.
  assert(!tactix_restart_clock_get_parameters(clock, &defaults));
  tactix_restart_clock_release(clock);
}
