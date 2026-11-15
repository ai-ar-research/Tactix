#pragma once
#include "tactix.h"
#include <string.h>

// A zeroed public step that runs once with an unlimited work allowance.
// Callers retain their own work unit, parameters, selection and post-action.
static inline tactix_online_step unlimited_online_step(
    uint64_t id, int rule_id, int granularity) {
  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = id;
  step.rule_id = rule_id;
  step.granularity = granularity;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.budget_unlimited = 1;
  return step;
}
