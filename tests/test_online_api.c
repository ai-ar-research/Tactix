#undef NDEBUG
#include "tactix.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int rule(const char *name) {
  int i;
  for (i = 0; i < tactix_rule_count(); ++i)
    if (!strcmp(tactix_rule_name(i), name))
      return i;
  assert(0);
  return -1;
}

static void clause(tactix *solver, const int *literals) {
  while (*literals)
    tactix_add(solver, *literals++);
  tactix_add(solver, 0);
}

static void exact_factor_grid_is_copied_and_applied(void) {
  tactix *solver = tactix_init();
  int factor_literals[2] = {1, 2};
  int column_literals[3][1] = {{3}, {4}, {5}};
  tactix_online_factor_remainder columns[3];
  tactix_online_factor_grid grid;
  tactix_online_step step;
  tactix_online_program program;
  tactix_online_result result;
  int row, column;
  for (row = 0; row < 2; ++row)
    for (column = 0; column < 3; ++column) {
      const int binary[] = {factor_literals[row],
                            column_literals[column][0], 0};
      clause(solver, binary);
    }
  for (column = 0; column < 3; ++column) {
    columns[column].literals = column_literals[column];
    columns[column].literal_count = 1;
  }
  grid.factors = factor_literals;
  grid.factor_count = 2;
  grid.remainders = columns;
  grid.remainder_count = 3;
  memset(&step, 0, sizeof step);
  step.step_id = 73;
  step.rule_id = rule("Factor");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_INHERIT;
  step.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  step.factor_grids = &grid;
  step.factor_grid_count = 1;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  program.steps = &step;
  program.step_count = 1;
  program.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  program.repeat_limit = 1;
  assert(tactix_online_configure(solver, &program, 13,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);

  /* Configuration owns a deep copy, including every nested literal array. */
  factor_literals[0] = factor_literals[1] = 0;
  for (column = 0; column < 3; ++column)
    column_literals[column][0] = 0;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED);
  assert(result.instance == 0 && result.made_rule_progress);
  assert(result.after_strategy_epoch > result.before_strategy_epoch);
  tactix_release(solver);
}

static void complete_factor_grid_is_discovered_and_applied(void) {
  tactix *solver = tactix_init();
  int factor, remainder;
  for (factor = 1; factor <= 2; ++factor)
    for (remainder = 3; remainder <= 5; ++remainder) {
      const int binary[] = {factor, remainder, 0};
      clause(solver, binary);
    }

  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = 74;
  step.rule_id = rule("Factor");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_INHERIT;
  step.granularity = TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  tactix_online_program program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(solver, &program, 14,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);

  int applications = 0, progress = 0;
  tactix_online_result result;
  while (!tactix_online_complete(solver)) {
    assert(tactix_online_advance(solver, &result) == 0);
    applications += (int)result.applications;
    progress += result.made_rule_progress;
  }
  assert(applications == 2); /* the 2x3 grid and its 3x2 orientation */
  assert(progress == 1);
  assert(tactix_stat(solver, TACTIX_STAT_FACTORINGS) == 1);
  tactix_release(solver);
}

int main(void) {
  exact_factor_grid_is_copied_and_applied();
  complete_factor_grid_is_discovered_and_applied();
  tactix *solver = tactix_init();
  const int binary[] = {1, 2, 0};
  const int victim[] = {1, 2, 3, 0};
  clause(solver, binary);
  clause(solver, victim);

  uint64_t instance[4];
  const int strengthen = rule("Strengthen");
  assert(tactix_rule_semantic_instances(solver, strengthen, instance, 4) == 1);

  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = 41;
  step.rule_id = strengthen;
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_STRENGTHEN;
  step.parameters.value.strengthen.clause_size_limit = 0;
  step.parameters.value.strengthen.occurrence_limit = 0;
  step.granularity = TACTIX_ONLINE_GRANULARITY_BATCH;
  step.instances = instance;
  step.instance_count = 1;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget = 1;

  tactix_online_program program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(solver, &program, 7,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  assert(!tactix_online_complete(solver));
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED);
  assert(result.configuration_revision == 7 && result.step_id == 41);
  assert(result.rule_id == (uint32_t)strengthen && result.instance == instance[0]);
  assert(result.applications == 1 && result.work == 1);
  assert(result.made_rule_progress && result.selection_complete);
  assert(tactix_online_step_consumed(solver, 41) == 1);
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED && result.selection_complete);
  assert(tactix_online_complete(solver));

  /* Invalid replacement is transactional: the completed revision survives. */
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_BVE;
  assert(tactix_online_configure(solver, &program, 8,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  assert(tactix_online_complete(solver));
  assert(tactix_online_step_consumed(solver, 41) == 1);

  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_STRENGTHEN;
  step.rule_budget_mode = 99;
  assert(tactix_online_configure(solver, &program, 81,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  assert(tactix_online_complete(solver));
  assert(tactix_online_step_consumed(solver, 41) == 1);
  step.rule_budget_mode = TACTIX_ONLINE_RULE_BUDGET_UNLIMITED;
  step.rule_budget = 1;
  assert(tactix_online_configure(solver, &program, 82,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  assert(tactix_online_complete(solver));
  step.rule_budget = 0;
  step.touch_filter_mode = 99;
  assert(tactix_online_configure(solver, &program, 83,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  assert(tactix_online_complete(solver));
  step.touch_filter_mode = TACTIX_ONLINE_SETTING_INHERIT;

  /* A complete pass may be configured after mutation and reaches fixpoint. */
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_STRENGTHEN;
  step.granularity = TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS;
  step.instances = NULL;
  step.instance_count = 0;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_FIXPOINT;
  step.repeat_limit = 0;
  step.budget = 4;
  assert(tactix_online_configure(solver, &program, 9,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_SCOPED_FIXPOINT);
  assert(result.configuration_revision == 9 && result.applications == 0);
  /* Step-level fixpoint is observable before the enclosing sequence closes. */
  assert(!tactix_online_complete(solver));
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED);
  assert(tactix_online_complete(solver));

  /* The public converter validates typed per-call parameters transactionally. */
  step.rule_id = rule("Vivify");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_VIVIFY;
  step.parameters.value.vivify.scope = 2;
  step.parameters.value.vivify.reuse = 1;
  step.parameters.value.vivify.flush = 0;
  step.parameters.value.vivify.irredundant_sort_ratio = 0;
  assert(tactix_online_configure(solver, &program, 10,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  assert(tactix_online_complete(solver));
  step.parameters.value.vivify.irredundant_sort_ratio = 3;
  assert(tactix_online_configure(solver, &program, 10,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);

  tactix_release(solver);
  puts("C online configuration, advance, revision and transaction PASS");
  return 0;
}
