/* The online composer is the shared strategy action surface: every stable
 * registry id must configure and execute through it, including search rules
 * and H-only policy operations. */
#undef NDEBUG
#include "tactix.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define NINST 128

static int rule_id(const char *name) {
  int i;
  for (i = 0; i < tactix_rule_count(); ++i)
    if (!strcmp(tactix_rule_name(i), name))
      return i;
  return -1;
}

static void add_clause(tactix *solver, const int *literals) {
  while (*literals)
    tactix_add(solver, *literals++);
  tactix_add(solver, 0);
}

static void fixture(tactix *solver) {
  add_clause(solver, (const int[]){-10, 1, 0});
  add_clause(solver, (const int[]){-10, 2, 0});
  add_clause(solver, (const int[]){10, -1, -2, 0});
  add_clause(solver, (const int[]){-11, 1, 0});
  add_clause(solver, (const int[]){-11, 2, 0});
  add_clause(solver, (const int[]){11, -1, -2, 0});
  add_clause(solver, (const int[]){-5, 6, 0});
  add_clause(solver, (const int[]){-6, 7, 0});
  add_clause(solver, (const int[]){-5, 7, 0});
  add_clause(solver, (const int[]){12, 14, 0});
  add_clause(solver, (const int[]){12, 15, 0});
  add_clause(solver, (const int[]){12, 16, 0});
  add_clause(solver, (const int[]){13, 14, 0});
  add_clause(solver, (const int[]){13, 15, 0});
  add_clause(solver, (const int[]){13, 16, 0});
  add_clause(solver, (const int[]){17, 18, 19, 20, 0});
  add_clause(solver, (const int[]){17, 18, 19, 0});
  add_clause(solver, (const int[]){21, 22, 23, 0});
  add_clause(solver, (const int[]){21, 22, -23, 0});
  add_clause(solver, (const int[]){24, 25, 26, 0});
  add_clause(solver, (const int[]){24, -25, 0});
}

static tactix *prepare(int id) {
  const char *name = tactix_rule_name(id);
  tactix *solver = tactix_init();
  fixture(solver);
  if (!strcmp(name, "Propagate")) {
    assert(tactix_decide(solver, 5) == 0);
  } else if (!strcmp(name, "Restart")) {
    assert(tactix_decide(solver, 5) == 0);
    assert(tactix_apply(solver, rule_id("Propagate")) != -1);
  } else if (!strcmp(name, "ConflictAnalysis")) {
    assert(tactix_decide(solver, -17) == 0);
    assert(tactix_decide(solver, -18) == 0);
    assert(tactix_decide(solver, -19) == 0);
    assert(tactix_apply(solver, rule_id("Propagate")) == 2);
  }
  return solver;
}

static int work_unit(const char *name) {
  if (!strcmp(name, "BVE"))
    return TACTIX_ONLINE_WORK_BVE_TICKS;
  if (!strcmp(name, "Propagate"))
    return TACTIX_ONLINE_WORK_PROPAGATIONS;
  return TACTIX_ONLINE_WORK_APPLICATIONS;
}

static void configure_parameters(const char *name,
                                 tactix_online_parameters *parameters) {
  parameters->kind = TACTIX_ONLINE_PARAMETERS_INHERIT;
  if (!strcmp(name, "ConflictAnalysis")) {
    parameters->kind = TACTIX_ONLINE_PARAMETERS_ANALYZE;
    parameters->value.analyze.minimize_depth_limit = 0;
    parameters->value.analyze.conflict_reuse = 1;
    parameters->value.analyze.chronological_levels = 100;
    parameters->value.analyze.bump_reason_limit = 3;
    parameters->value.analyze.bump_reason_depth = 2;
    parameters->value.analyze.eager_subsume = 2;
  } else if (!strcmp(name, "BCE")) {
    parameters->kind = TACTIX_ONLINE_PARAMETERS_BCE;
    parameters->value.bce.occurrence_limit = 7;
  } else if (!strcmp(name, "HyperTernaryRes")) {
    parameters->kind = TACTIX_ONLINE_PARAMETERS_TERNARY;
    parameters->value.ternary.occurrence_limit = 9;
    parameters->value.ternary.maximum_add_percent = 1000;
  } else if (!strcmp(name, "Factor")) {
    parameters->kind = TACTIX_ONLINE_PARAMETERS_FACTOR;
    parameters->value.factor.maximum_clause_size = 5;
    parameters->value.factor.growth_bound = 0;
    parameters->value.factor.candidate_rounds = 2;
    parameters->value.factor.composite = 0;
    parameters->value.factor.demote_fresh = 1;
  } else if (!strcmp(name, "TransitiveReduce")) {
    parameters->kind = TACTIX_ONLINE_PARAMETERS_TRANSITIVE;
    parameters->value.transitive.early_exit = 1;
    parameters->value.transitive.live_meter = 1;
    parameters->value.transitive.checked_budget_meter = 0;
  } else if (!strcmp(name, "Substitute")) {
    parameters->kind = TACTIX_ONLINE_PARAMETERS_SUBSTITUTE;
    parameters->value.substitute.demote_substituted = 0;
  } else if (!strcmp(name, "Reduce")) {
    parameters->kind = TACTIX_ONLINE_PARAMETERS_REDUCE;
    parameters->value.reduce.tier1 = 2;
    parameters->value.reduce.tier2 = 6;
  } else if (!strcmp(name, "Walk")) {
    parameters->kind = TACTIX_ONLINE_PARAMETERS_WALK;
    parameters->value.walk.step_budget = 4;
    parameters->value.walk.random_seed = 123;
  } else if (!strcmp(name, "Reorder")) {
    parameters->kind = TACTIX_ONLINE_PARAMETERS_REORDER;
    parameters->value.reorder.maximum_clause_size = 7;
  }
}

static void reaches_every_registered_action(void) {
  const int count = tactix_rule_count();
  int id;
  assert(count == 26);
  for (id = 0; id < count; ++id) {
    tactix *solver = prepare(id);
    const char *name = tactix_rule_name(id);
    uint64_t instances[NINST], selected = TACTIX_INSTANCE_CANONICAL;
    const int n = tactix_rule_semantic_instances(solver, id, instances, NINST);
    int i;
    assert(n >= 0);
    for (i = 0; i < n; ++i)
      if (instances[i] != TACTIX_INSTANCE_CANONICAL) {
        selected = instances[i];
        break;
      }

    tactix_online_step step;
    memset(&step, 0, sizeof step);
    step.step_id = (uint64_t)id + 1;
    step.rule_id = id;
    configure_parameters(name, &step.parameters);
    step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
    step.repeat_limit = 1;
    step.work_unit = work_unit(name);
    step.budget_unlimited = 1;
    if (selected == TACTIX_INSTANCE_CANONICAL) {
      step.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
    } else {
      step.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
      step.instances = &selected;
      step.instance_count = 1;
    }
    tactix_online_program program = {
        &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
    assert(tactix_online_configure(solver, &program, 19,
                                   TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
    tactix_online_result result;
    assert(tactix_online_advance(solver, &result) == 0);
    assert(result.rule_id == (uint32_t)id);
    assert(result.step_id == step.step_id && result.applications == 1);
    assert(result.work_unit == step.work_unit);
    assert(result.after_strategy_epoch > result.before_strategy_epoch);
    if (!strcmp(name, "ConflictAnalysis"))
      assert(tactix_config(solver, TACTIX_CFG_ANALYZE_MINIMIZE_DEPTH) == 1000);
    printf("  %-18s %s\n", name,
           selected == TACTIX_INSTANCE_CANONICAL ? "CANON" : "INSTANCE");
    tactix_release(solver);
  }
}

static void invalid_reduce_parameters_are_transactional(void) {
  tactix *solver = tactix_init();
  fixture(solver);
  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = 88;
  step.rule_id = rule_id("Reduce");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_REDUCE;
  step.parameters.value.reduce.tier1 = 7;
  step.parameters.value.reduce.tier2 = 2;
  step.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  tactix_online_program program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(solver, &program, 20,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);

  step.parameters.value.reduce.tier1 = 2;
  step.parameters.value.reduce.tier2 = 6;
  step.parameters.value.reduce.victim_numerator = 5;
  step.parameters.value.reduce.victim_denominator = 4;
  assert(tactix_online_configure(solver, &program, 21,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);

  step.parameters.value.reduce.victim_numerator = 3;
  step.parameters.value.reduce.victim_denominator = 4;
  step.parameters.value.reduce.tier_protection_mode = 99;
  assert(tactix_online_configure(solver, &program, 22,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);

  step.parameters.value.reduce.tier_protection_mode =
      TACTIX_ONLINE_SETTING_DISABLE;
  step.parameters.value.reduce.compact_collection_mode =
      TACTIX_ONLINE_SETTING_ENABLE;
  step.parameters.value.reduce.victim_numerator = 0;
  assert(tactix_online_configure(solver, &program, 23,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  tactix_release(solver);
}

static void aba_transition_invalidates_a_pending_batch(void) {
  tactix *solver = tactix_init();
  fixture(solver);
  const uint64_t decisions[] = {5, 6};
  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = 91;
  step.rule_id = rule_id("Decide");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_INHERIT;
  step.granularity = TACTIX_ONLINE_GRANULARITY_BATCH;
  step.instances = decisions;
  step.instance_count = 2;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  tactix_online_program program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(solver, &program, 20,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.applications == 1 && result.instance == 5);

  /* Return to the same visible level/trail shape through other public actions.
   * The monotonic strategy epoch still makes the old second payload stale. */
  assert(tactix_apply(solver, rule_id("Restart")) == 0);
  assert(tactix_decide(solver, 5) == 0);
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_STALE_REQUEST);
  assert(result.applications == 0 && !result.selection_complete);
  tactix_release(solver);
}

int main(void) {
  reaches_every_registered_action();
  invalid_reduce_parameters_are_transactional();
  aba_transition_invalidates_a_pending_batch();
  puts("test_online_all_actions: all 26 actions and ABA invalidation PASS");
  return 0;
}
