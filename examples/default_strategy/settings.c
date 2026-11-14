/* The settings the default strategy reads: the session's default-strategy
 * configuration by snapshot key (tactix_strategy_integer/_real/_text). */
#include "strategy.h"

#include <stdio.h>
#include <stdlib.h>

uint64_t ds_integer_setting(tactix *s, const char *key) {
  uint64_t value;
  if (tactix_strategy_integer(s, key, &value)) {
    fprintf(stderr, "tactix-default: no integer setting %s\n", key);
    abort();
  }
  return value;
}

double ds_real_setting(tactix *s, const char *key) {
  double value;
  if (tactix_strategy_real(s, key, &value)) {
    fprintf(stderr, "tactix-default: no real setting %s\n", key);
    abort();
  }
  return value;
}

/* The number of settings `<prefix><i>` that exist, numbered from 0. */
static int indexed_settings(tactix *s, const char *prefix) {
  char key[128];
  for (int i = 0;; ++i) {
    uint64_t integer;
    snprintf(key, sizeof key, "%s%d", prefix, i);
    if (tactix_strategy_integer(s, key, &integer) && tactix_strategy_text(s, key, NULL, 0) < 0)
      return i;
  }
}

static uint64_t indexed(tactix *s, const char *prefix, uint32_t i) {
  char key[64];
  snprintf(key, sizeof key, "%s%u", prefix, i);
  return ds_integer_setting(s, key);
}

void ds_read_settings(tactix *s, settings *c) {
#define I(group, key) c->group.key = ds_integer_setting(s, #group "." #key);
#define R(group, key) c->group.key = ds_real_setting(s, #group "." #key);
#define E(group, key) c->group.key = ds_integer_setting(s, #key ".enabled");
  DS_BRANCHING(I, R, branching)
  c->branching.focused_phases = (uint32_t)indexed_settings(s, "branching.focused_phase");
  for (uint32_t i = 0; i < c->branching.focused_phases; ++i)
    c->branching.focused_phase[i] = (int)(int64_t)indexed(s, "branching.focused_phase", i);
  DS_ANALYSIS(I, R, analysis)
  DS_RESTART(I, R, restart)
  DS_MODE(I, R, mode)
  DS_REDUCTION(I, R, reduction)
  DS_REPHASE(I, R, rephase)
  c->rephase.rotations = (uint32_t)indexed_settings(s, "rephase.rotation");
  for (uint32_t i = 0; i < c->rephase.rotations; ++i)
    c->rephase.rotation[i] = (uint8_t)indexed(s, "rephase.rotation", i);
  DS_REORDER(I, R, reorder)
  DS_ENABLED(E, R, enabled)
  DS_INPROCESS(I, R, inprocess)
#undef I
#undef R
#undef E
  for (uint32_t i = 0; i < 3; ++i) {
    c->inprocess.vivify_tier_share[i] = indexed(s, "inprocess.vivify_tier_share", i);
    c->inprocess.vivify_tier_scope[i] = indexed(s, "inprocess.vivify_tier_scope", i);
  }
  c->inprocess.bve_bounds = (uint32_t)indexed_settings(s, "inprocess.bve_bound");
  for (uint32_t i = 0; i < c->inprocess.bve_bounds && i < 16; ++i)
    c->inprocess.bve_bound[i] = indexed(s, "inprocess.bve_bound", i);
  c->inprocess.program_steps = indexed_settings(s, "inprocess.step");
  c->inprocess.structured_stages[0] = indexed_settings(s, "inprocess.structured_root_stage");
  c->inprocess.structured_stages[1] =
      indexed_settings(s, "inprocess.structured_root_bounded_backtracking_stage");
  c->inprocess.structured_stages[2] = indexed_settings(s, "inprocess.structured_root_large_stage");
}
