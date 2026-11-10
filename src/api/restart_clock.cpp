#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "api/api_internal.hpp"
#include "policies/default_restart.hpp"

using namespace tactix;

struct tactix_restart_clock {
  DefaultStrategyConfig::Restart parameters;
  DefaultRestartControl control;
};

namespace {
bool parameters(const tactix_restart_clock_parameters *source,
                DefaultStrategyConfig::Restart &out) {
  out = {};
  if (source) {
    out.initial_interval = source->initial_interval;
    out.reluctant_period = source->reluctant_period;
    out.reluctant_limit = source->reluctant_limit;
    out.glue_margin = source->glue_margin;
    out.fast_ema_window = source->fast_ema_window;
    out.slow_ema_window = source->slow_ema_window;
    out.logarithm_offset = source->logarithm_offset;
    out.logarithm_bias = source->logarithm_bias;
  }
  // Native conversion truncates a floating delta to uint64_t. Its smallest
  // argument is restart count one; reject profiles with an undefined cast.
  return out.valid() &&
      std::log10(1.0 + out.logarithm_offset) - out.logarithm_bias > -1.0;
}
}

extern "C" {
tactix_restart_clock *tactix_restart_clock_create(
    tactix_c_handle *s, const tactix_restart_clock_parameters *source) {
  if (!s || !api_detail::kernel_for_fixed_testing(s).h) return nullptr;
  DefaultStrategyConfig::Restart profile;
  if (!parameters(source, profile)) return nullptr;
  auto *clock = new tactix_restart_clock;
  clock->parameters = profile;
  clock->control.reset(api_detail::kernel_for_fixed_testing(s), profile);
  return clock;
}

void tactix_restart_clock_release(tactix_restart_clock *clock) { delete clock; }

int tactix_restart_clock_configure(tactix_c_handle *s, tactix_restart_clock *clock,
                                  const tactix_restart_clock_parameters *source) {
  if (!s || !clock || !api_detail::kernel_for_fixed_testing(s).h) return -1;
  DefaultStrategyConfig::Restart profile;
  if (!parameters(source, profile)) return -1;
  clock->parameters = profile;
  clock->control.reset(api_detail::kernel_for_fixed_testing(s), profile);
  return 0;
}

int tactix_restart_clock_get_parameters(const tactix_restart_clock *clock,
                                       tactix_restart_clock_parameters *out) {
  if (!clock || !out) return -1;
  const auto &p = clock->parameters;
  *out = {p.initial_interval, p.reluctant_period, p.reluctant_limit,
          p.glue_margin, p.fast_ema_window, p.slow_ema_window,
          p.logarithm_offset, p.logarithm_bias};
  return 0;
}

int tactix_restart_clock_update(tactix_c_handle *s, tactix_restart_clock *clock,
                               int event, uint32_t glue) {
  if (!s || !clock || event < TACTIX_RESTART_CLOCK_RESET ||
      event > TACTIX_RESTART_CLOCK_MODE_ENTERED ||
      (glue && event != TACTIX_RESTART_CLOCK_GLUE)) return -1;
  const Kernel &k = api_detail::kernel_for_fixed_testing(s);
  if (!k.h) return -1;
  auto &c = clock->control;
  const auto &p = clock->parameters;
  switch (event) {
  case TACTIX_RESTART_CLOCK_RESET: c.reset(k, p); break;
  case TACTIX_RESTART_CLOCK_DECISION_RATE: c.observe_decision_rate(k, p); break;
  case TACTIX_RESTART_CLOCK_GLUE: c.observe_glue(k, p, glue); break;
  case TACTIX_RESTART_CLOCK_RELUCTANT: c.tick_reluctant(p); break;
  case TACTIX_RESTART_CLOCK_RESTARTED: c.restart_completed(k, p); break;
  case TACTIX_RESTART_CLOCK_MODE_ENTERED: c.mode_entered(k); break;
  }
  return 0;
}

int tactix_restart_clock_observe(tactix_c_handle *s, const tactix_restart_clock *clock,
                                tactix_restart_clock_state *out) {
  if (!s || !clock || !out || !api_detail::kernel_for_fixed_testing(s).h) return -1;
  const auto &c = clock->control;
  *out = {};
  out->due = c.want_restart(api_detail::kernel_for_fixed_testing(s), clock->parameters);
  out->next_conflicts = c.restart.next_conflicts;
  out->reluctant_trigger = c.reluctant.trigger;
  out->reluctant_u = c.reluctant.u; out->reluctant_v = c.reluctant.v;
  out->reluctant_wait = c.reluctant.wait;
  for (unsigned mode = 0; mode != 2; ++mode) {
    const auto &a = c.averages[mode];
    out->fast_glue[mode] = a.fast_glue.value;
    out->slow_glue[mode] = a.slow_glue.value;
    out->decision_rate[mode] = a.decision_rate.value;
    out->initialized[mode] = a.initialized;
    out->saved_decisions[mode] = a.saved_decisions;
  }
  return 0;
}
}
