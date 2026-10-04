#undef NDEBUG
#include "tactix.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static int configure_version(tactix *solver, int version,
                             const tactix_online_step *step,
                             int bad_nested, uint64_t revision) {
  const tactix_online_program v1 = {
      step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  if (version == 1)
    return tactix_online_configure(
        solver, &v1, revision, TACTIX_ONLINE_RECONFIGURE_RESET);
  tactix_online_step_v2 step2;
  memset(&step2, 0, sizeof step2);
  step2.base = *step;
  if (version == 2) {
    step2.sr_instance_count = bad_nested ? -1 : 0;
    const tactix_online_program_v2 v2 = {
        &step2, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
    return tactix_online_configure_v2(
        solver, &v2, revision, TACTIX_ONLINE_RECONFIGURE_RESET);
  }
  tactix_online_step_v3 step3;
  memset(&step3, 0, sizeof step3);
  step3.base = step2;
  step3.condition_instance_count = bad_nested ? -1 : 0;
  const tactix_online_program_v3 v3 = {
      &step3, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  return tactix_online_configure_v3(
      solver, &v3, revision, TACTIX_ONLINE_RECONFIGURE_RESET);
}

static void rejected_configuration_keeps_input_open(void) {
  for (int version = 1; version <= 3; ++version)
    for (int proof_mode = 0; proof_mode <= 2; ++proof_mode) {
      tactix *solver = tactix_init();
      char path[128];
      const char *proof_dir = getenv("TACTIX_ONLINE_CONFIG_PROOF_DIR");
      if (proof_dir)
        snprintf(path, sizeof path, "%s/config-%d-%d.lrat",
                 proof_dir, version, proof_mode);
      else
        snprintf(path, sizeof path,
                 "/tmp/tactix-online-config-%ld-%d-%d.lrat",
                 (long)getpid(), version, proof_mode);
      if (proof_mode == 1)
        assert(tactix_proof(solver, path, TACTIX_PROOF_LRAT) == 0);
      clause(solver, (const int[]){1, 0});
      assert(tactix_add_checked(solver, 2) == 0); /* unfinished clause */
      tactix_online_step step;
      memset(&step, 0, sizeof step);
      step.rule_id = -1; /* converter rejection */
      step.step_id = 1;
      step.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
      step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
      step.repeat_limit = 1;
      step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
      step.budget_unlimited = 1;
      assert(configure_version(solver, version, &step, 0, 1) == -1);
      assert(tactix_online_complete(solver) == -1);
      assert(tactix_add_checked(solver, 0) == 0);
      if (proof_mode == 2)
        assert(tactix_proof(solver, path, TACTIX_PROOF_LRAT) == 0);

      step.rule_id = rule("Propagate");
      step.work_unit = TACTIX_ONLINE_WORK_PROPAGATIONS;
      step.step_id = 0; /* executor rejection after successful conversion */
      assert(configure_version(solver, version, &step, 0, 2) == -1);
      assert(tactix_online_complete(solver) == -1);
      clause(solver, (const int[]){3, 0});

      step.step_id = 1;
      if (version != 1) {
        assert(configure_version(solver, version, &step, 1, 3) == -1);
        assert(tactix_online_complete(solver) == -1);
        clause(solver, (const int[]){4, 0});
      }
      if (proof_mode)
        clause(solver, (const int[]){-1, 0});
      assert(configure_version(solver, version, &step, 0, 4) == 0);
      assert(tactix_add_checked(solver, 5) == -1);
      assert(tactix_solve(solver) == (proof_mode ? 20 : 10));
      if (!proof_mode)
        for (int variable = 1; variable <= (version == 1 ? 3 : 4);
             ++variable)
          assert(tactix_value(solver, variable) == variable);
      tactix_release(solver);
      if (proof_mode && !proof_dir)
        assert(unlink(path) == 0);
    }
}

static void condition_cube_v3_is_atomic_and_copied(void) {
  tactix *solver = tactix_init();
  clause(solver, (const int[]){1, 2, 0});
  clause(solver, (const int[]){1, 2, 0});
  clause(solver, (const int[]){-1, 3, 0});
  clause(solver, (const int[]){-3, 4, 0});
  int candidate[] = {1, 2};
  int witness[] = {1, 3, 4};
  tactix_online_condition_instance cubes[] = {
      {candidate, 2, witness, 3}, {candidate, 2, witness, 3}};
  tactix_online_step_v3 step;
  memset(&step, 0, sizeof step);
  step.base.base.step_id = 375;
  step.base.base.rule_id = rule("Condition");
  step.base.base.granularity = TACTIX_ONLINE_GRANULARITY_BATCH;
  step.base.base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.base.base.repeat_limit = 1;
  step.base.base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.base.base.budget_unlimited = 1;
  step.condition_instances = cubes;
  step.condition_instance_count = 2;
  tactix_online_program_v3 program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  step.base.base.rule_id = rule("BCE");
  assert(tactix_online_configure_v3(solver, &program, 1,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.base.base.rule_id = rule("Condition");
  assert(tactix_online_configure_v3(solver, &program, 2,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  /* The configured transaction owns both nested arrays after this call. */
  candidate[0] = candidate[1] = 0;
  witness[0] = witness[1] = witness[2] = 0;
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED);
  assert(result.made_rule_progress && result.applications == 1);
  assert(result.instance == 0 && result.configuration_revision == 2);
  /* The second saved tuple names a clause the first action retired. */
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_STALE_REQUEST);
  assert(result.applications == 0 && result.work == 0);
  assert(tactix_decide(solver, -1) == 0);
  assert(tactix_decide(solver, -2) == 0);
  assert(tactix_solve(solver) == 10);
  assert(tactix_value(solver, 1) == 1);
  assert(tactix_value(solver, 3) == 3);
  assert(tactix_value(solver, 4) == 4);
  tactix_release(solver);
}

/* Early and late LRAT attachment share eager import and can finish the input
 * refutation before the online executor runs. The executor must not report a
 * second UNSAT transition merely because proof attachment happened early. */
static void late_lrat_root_refutation_is_terminal(void) {
  /* Eager root propagation and native-LRAT replay can finish with
   * different unit trails after the same input has been refuted. */
  static const int input[] = {
      25, 0, -30, 13, -25, 0, 7, -22, 21, 0, 22, -14, 9, 0,
      -2, 21, 14, 0, -34, -24, -10, 0, -15, 2, -4, 0,
      -33, -34, 15, 0, 6, 0, 10, 33, -32, 0, 34, 22, -13, 0,
      7, -27, -32, 0, -26, 4, -32, 0, -8, -17, 7, 0,
      8, 2, 32, 0, -17, -6, -7, 0, -6, 17, 0,
      30, 22, 27, 0, -32, 26, 7, 0, -25, 24, 33, 0,
      -25, -21, 0, 8, -9, -17, 0};
  for (int late = 0; late <= 1; ++late) {
    char path[512];
    const char *proof_dir = getenv("TACTIX_F1_PROOF_DIR");
    if (proof_dir)
      snprintf(path, sizeof path, "%s/%s.lrat", proof_dir,
               late ? "late" : "early");
    else
      snprintf(path, sizeof path, "/tmp/tactix-online-late-root-%ld-%d.lrat",
               (long)getpid(), late);
    tactix *solver = tactix_init();
    if (!late)
      assert(tactix_proof(solver, path, TACTIX_PROOF_LRAT) == 0);
    int clauses = 0;
    for (unsigned i = 0; i < sizeof input / sizeof input[0]; ++i) {
      tactix_add(solver, input[i]);
      if (!input[i] && ++clauses == 1 && late)
        assert(tactix_proof(solver, path, TACTIX_PROOF_LRAT) == 0);
    }
    assert(clauses == 22);
    tactix_online_step step;
    memset(&step, 0, sizeof step);
    step.step_id = 1;
    step.rule_id = rule("Propagate");
    step.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
    step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
    step.repeat_limit = 1;
    step.work_unit = TACTIX_ONLINE_WORK_PROPAGATIONS;
    step.budget_unlimited = 1;
    tactix_online_program program = {&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
    assert(tactix_online_configure(solver, &program, 1,
                                   TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
    tactix_online_result result;
    assert(tactix_online_advance(solver, &result) == 0);
    assert(result.status == TACTIX_ONLINE_INAPPLICABLE);
    assert(tactix_status(solver) == 20);
    tactix_release(solver);
    FILE *proof = fopen(path, "rb");
    assert(proof);
    assert(fseek(proof, 0, SEEK_END) == 0);
    assert(ftell(proof) > 0);
    assert(fclose(proof) == 0);
    if (!proof_dir)
      assert(remove(path) == 0);
  }
}

static void canonical_bve_reaches_a_real_fixpoint(void) {
  for (int signs = 0; signs < 4; ++signs) {
    tactix *solver = tactix_init();
    const int a = (signs & 1) ? -1 : 1;
    const int b = (signs & 2) ? -2 : 2;
    const int input[] = {b, a, 0};
    clause(solver, input);
    int progress = 0, result;
    while ((result = tactix_apply(solver, rule("BVE"))) == 0) {
      assert(++progress <= 2);
      const int propagated = tactix_apply(solver, rule("Propagate"));
      // Pure elimination may already be at a propagation fixpoint, where the
      // propagation guard correctly declines the optional follow-up.
      assert(propagated == -1 || propagated == 0 || propagated == 1 ||
             propagated == 10);
    }
    assert(result == 1 && progress == 2);
    assert(tactix_stat(solver, TACTIX_STAT_BVE_ELIMINATED) == 2);
    assert(tactix_status(solver) == 10);
    assert(tactix_value(solver, a) == a || tactix_value(solver, b) == b);
    assert(tactix_apply(solver, rule("BVE")) == 1);
    assert(tactix_stat(solver, TACTIX_STAT_BVE_ELIMINATED) == 2);
    assert(tactix_value(solver, a) == a || tactix_value(solver, b) == b);
    tactix_release(solver);
  }
}

static void status_reconstructs_bve_models_and_refreshes_after_restart(void) {
  for (int signs = 0; signs < 4; ++signs) {
    tactix *solver = tactix_init();
    const int a = (signs & 1) ? -1 : 1;
    const int b = (signs & 2) ? -2 : 2;
    const int input[] = {b, a, 0};
    clause(solver, input);
    tactix_online_step step;
    memset(&step, 0, sizeof step);
    step.step_id = 101;
    step.rule_id = rule("BVE");
    step.parameters.kind = TACTIX_ONLINE_PARAMETERS_INHERIT;
    step.granularity = TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS;
    step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
    step.repeat_limit = 1;
    step.work_unit = TACTIX_ONLINE_WORK_BVE_TICKS;
    step.budget_unlimited = 1;
    tactix_online_program program = {&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
    assert(tactix_online_configure(solver, &program, 1,
                                  TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
    int advances = 0;
    while (!tactix_online_complete(solver)) {
      tactix_online_result result;
      assert(++advances < 64);
      assert(tactix_online_advance(solver, &result) == 0);
    }
    assert(tactix_stat(solver, TACTIX_STAT_BVE_ELIMINATED) > 0);
    // The original clause is the independent model oracle. Test value access
    // both before and after the explicit status query at the completed state.
    assert(tactix_value(solver, a) == a || tactix_value(solver, b) == b);
    assert(tactix_status(solver) == 10);
    assert(tactix_value(solver, a) == a || tactix_value(solver, b) == b);
    const uint64_t spent = tactix_online_step_consumed(solver, 101);
    assert(tactix_status(solver) == 10);
    assert(tactix_online_step_consumed(solver, 101) == spent);
    tactix_release(solver);
  }

  tactix *solver = tactix_init();
  const int input[] = {1, 2, 0};
  clause(solver, input);
  assert(tactix_decide(solver, 1) == 0);
  assert(tactix_apply(solver, rule("Propagate")) >= 0);
  assert(tactix_decide(solver, -2) == 0);
  assert(tactix_apply(solver, rule("Propagate")) >= 0);
  assert(tactix_status(solver) == 10);
  assert(tactix_value(solver, 1) == 1 && tactix_value(solver, 2) == -2);
  assert(tactix_apply(solver, rule("Restart")) == 0);
  assert(tactix_status(solver) == 0);
  assert(tactix_decide(solver, -1) == 0);
  assert(tactix_apply(solver, rule("Propagate")) >= 0);
  assert(tactix_status(solver) == 10);
  assert(tactix_value(solver, 1) == -1 && tactix_value(solver, 2) == 2);
  tactix_release(solver);
}

static void pure_bve_scope_is_public_and_validated(void) {
  tactix *solver = tactix_init();
  const int first[] = {1, 2, 0};
  const int second[] = {-2, 3, 0};
  clause(solver, first);
  clause(solver, second);

  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = 80;
  step.rule_id = rule("BVE");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_BVE;
  step.parameters.value.bve.growth_bound = 16;
  step.parameters.value.bve.occurrence_limit = UINT32_MAX;
  step.parameters.value.bve.clause_size_limit = UINT32_MAX;
  step.parameters.value.bve.xor_arity_limit = 2;
  step.parameters.value.bve.resolvent_size_limit = UINT32_MAX;
  step.parameters.value.bve.backward_occurrence_limit = UINT32_MAX;
  step.parameters.value.bve.definition_cores = 1;
  step.parameters.value.bve.candidate_scope =
      TACTIX_ONLINE_BVE_CANDIDATES_PURE_ONLY;
  step.granularity = TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_BVE_TICKS;
  step.budget_unlimited = 1;
  tactix_online_program program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(solver, &program, 20,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  uint64_t applications = 0;
  while (!tactix_online_complete(solver)) {
    tactix_online_result result;
    assert(tactix_online_advance(solver, &result) == 0);
    applications += result.applications;
  }
  assert(applications == 2);

  step.parameters.value.bve.candidate_scope =
      TACTIX_ONLINE_BVE_CANDIDATES_PURE_CLOSURE + 1;
  assert(tactix_online_configure(solver, &program, 21,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  assert(tactix_online_complete(solver));
  step.parameters.value.bve.candidate_scope =
      TACTIX_ONLINE_BVE_CANDIDATES_PURE_ONLY;
  step.parameters.value.bve.traversal = 2;
  assert(tactix_online_configure(solver, &program, 22,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  assert(tactix_online_complete(solver));
  tactix_release(solver);
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
  /* Four grids in each orientation: three 2x2 subgrids and the full grid. */
  assert(applications == 8);
  assert(progress == 1);
  assert(tactix_stat(solver, TACTIX_STAT_FACTORINGS) == 1);
  tactix_release(solver);
}

static void exact_sr_instance_v2_is_copied_and_applied(void) {
  tactix *solver = tactix_init();
  const int clauses[][3] = {{1, 2, 0}, {3, 4, 0},
                            {-1, -3, 0}, {-2, -4, 0}};
  int i;
  for (i = 0; i < 4; ++i)
    clause(solver, clauses[i]);

  int sr_clause[] = {-1};
  int witness[] = {-1, 3};
  tactix_online_sr_mapping mapping[] = {{2, 4}, {4, 2}};
  tactix_online_sr_instance instance = {
      sr_clause, 1, witness, 2, mapping, 2};
  tactix_online_step_v2 step;
  tactix_online_program_v2 program;
  tactix_online_result result;
  memset(&step, 0, sizeof step);
  step.base.step_id = 74;
  step.base.rule_id = rule("SubstitutionRedundancy");
  step.base.parameters.kind =
      TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY;
  step.base.parameters.value.substitution_redundancy.maximum_validation_ticks =
      UINT64_MAX;
  step.base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  step.base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.base.repeat_limit = 1;
  step.base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.base.budget_unlimited = 1;
  step.base.post_application = TACTIX_ONLINE_POST_PROPAGATE;
  step.sr_instances = &instance;
  step.sr_instance_count = 1;
  program.steps = &step;
  program.step_count = 1;
  program.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  program.repeat_limit = 1;
  assert(tactix_online_configure_v2(solver, &program, 17,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == 0);

  /* Every nested array is owned by the configured immutable snapshot. */
  sr_clause[0] = witness[0] = witness[1] = 0;
  mapping[0].source_literal = mapping[0].target_literal = 0;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED);
  assert(result.instance == 0 && result.made_rule_progress);
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED);
  assert(tactix_online_complete(solver));
  tactix_release(solver);
}

static void general_sr_producer_is_available_through_v1(void) {
  tactix *solver = tactix_init();
  /* A discarded tautology no longer keeps an unassigned identity active.
     Use a live clause so the public producer has a real variable domain. */
  clause(solver, (const int[]){1, 2, 0});
  tactix_online_step step;
  tactix_online_program program;
  tactix_online_result result;
  memset(&step, 0, sizeof step);
  step.step_id = 76;
  step.rule_id = rule("SubstitutionRedundancy");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY;
  step.parameters.value.substitution_redundancy.maximum_validation_ticks =
      UINT64_MAX;
  step.parameters.value.substitution_redundancy.maximum_detection_ticks =
      UINT64_MAX;
  step.parameters.value.substitution_redundancy.general_producer = 1;
  step.parameters.value.substitution_redundancy
      .maximum_general_candidates_per_advance = UINT64_MAX;
  step.granularity = TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget = 1;
  program = (tactix_online_program){&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(solver, &program, 18,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.applications == 1 && result.made_rule_progress);
  tactix_release(solver);
}

static void graph_sr_producer_is_typed_through_v1(void) {
  tactix *solver = tactix_init();
  const int clauses[][3] = {
      {1, 2, 0}, {3, 4, 0}, {-1, -3, 0}, {-2, -4, 0}};
  tactix_online_step step;
  tactix_online_program program;
  tactix_online_result result;
  size_t i;
  for (i = 0; i < sizeof clauses / sizeof clauses[0]; ++i)
    clause(solver, clauses[i]);
  memset(&step, 0, sizeof step);
  step.step_id = 77;
  step.rule_id = rule("SubstitutionRedundancy");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY;
  step.parameters.value.substitution_redundancy.maximum_validation_ticks =
      UINT64_MAX;
  step.parameters.value.substitution_redundancy.maximum_detection_ticks =
      UINT64_MAX;
  step.parameters.value.substitution_redundancy.producer =
      TACTIX_ONLINE_SR_GRAPH_SYMMETRY;
  step.parameters.value.substitution_redundancy.maximum_graph_vertices = 128;
  step.parameters.value.substitution_redundancy.maximum_graph_edges = 256;
  step.parameters.value.substitution_redundancy.maximum_symmetry_generators =
      64;
  step.parameters.value.substitution_redundancy
      .symmetry_backend_search_budget = 64;
  step.parameters.value.substitution_redundancy
      .symmetry_backend_support_budget = UINT64_MAX;
  step.parameters.value.substitution_redundancy
      .symmetry_backend_component_size_limit = UINT64_MAX;
  step.parameters.value.substitution_redundancy
      .maximum_symmetry_fixing_instances = 64;
  step.parameters.value.substitution_redundancy
      .maximum_symmetry_generator_instances_per_pass = 1;
  step.parameters.value.substitution_redundancy
      .maximum_symmetry_group_ticks = 100000;
  step.granularity = TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  program = (tactix_online_program){&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(solver, &program, 19,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED &&
         result.made_rule_progress);
  tactix_release(solver);
}

static void invalid_graph_sr_settings_are_transactional(void) {
  tactix *solver = tactix_init();
  const int clauses[][3] = {
      {1, 2, 0}, {3, 4, 0}, {-1, -3, 0}, {-2, -4, 0}};
  tactix_online_step step;
  tactix_online_result result;
  size_t i;
  for (i = 0; i < sizeof clauses / sizeof clauses[0]; ++i)
    clause(solver, clauses[i]);
  memset(&step, 0, sizeof step);
  step.step_id = 78;
  step.rule_id = rule("SubstitutionRedundancy");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY;
  step.parameters.value.substitution_redundancy.maximum_validation_ticks =
      UINT64_MAX;
  step.parameters.value.substitution_redundancy.maximum_detection_ticks =
      UINT64_MAX;
  step.parameters.value.substitution_redundancy.producer =
      TACTIX_ONLINE_SR_GRAPH_SYMMETRY;
  step.parameters.value.substitution_redundancy.maximum_graph_vertices = 128;
  step.parameters.value.substitution_redundancy.maximum_graph_edges = 256;
  step.parameters.value.substitution_redundancy.maximum_symmetry_generators =
      64;
  step.parameters.value.substitution_redundancy
      .symmetry_backend_search_budget = 64;
  step.parameters.value.substitution_redundancy
      .symmetry_backend_support_budget = UINT64_MAX;
  step.parameters.value.substitution_redundancy
      .symmetry_backend_component_size_limit = UINT64_MAX;
  step.parameters.value.substitution_redundancy
      .maximum_symmetry_fixing_instances = 64;
  step.parameters.value.substitution_redundancy
      .maximum_symmetry_generator_instances_per_pass = 1;
  step.parameters.value.substitution_redundancy
      .maximum_symmetry_group_ticks = 100000;
  step.granularity = TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  tactix_online_program program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(solver, &program, 20,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);

  step.parameters.value.substitution_redundancy.producer = 99;
  assert(tactix_online_configure(solver, &program, 21,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy.producer =
      TACTIX_ONLINE_SR_GRAPH_SYMMETRY;
  step.parameters.value.substitution_redundancy.orbitopal_producer = 1;
  assert(tactix_online_configure(solver, &program, 22,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);

  // Both rejected replacements leave revision 20's immutable snapshot live.
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.configuration_revision == 20 &&
         result.status == TACTIX_ONLINE_COMPLETED &&
         result.made_rule_progress);

  /* Matching legacy and typed selectors are accepted during migration. */
  step.parameters.value.substitution_redundancy.producer =
      TACTIX_ONLINE_SR_ORBITOPAL;
  step.parameters.value.substitution_redundancy.orbitopal_producer = 1;
  step.parameters.value.substitution_redundancy.minimum_orbitopal_rows = 2;
  step.parameters.value.substitution_redundancy.minimum_orbitopal_columns = 2;
  step.parameters.value.substitution_redundancy.orbitopal_column_order =
      TACTIX_ONLINE_ORBITOPAL_ORDER_COOCCURRENCE_MAXIMUM_CLIQUE;
  for (int source = TACTIX_ONLINE_ORBITOPAL_ROW_ORBITS_DETECTED_GENERATORS;
       source <= TACTIX_ONLINE_ORBITOPAL_ROW_ORBITS_REFINEMENT_CELLS; ++source) {
    step.parameters.value.substitution_redundancy.orbitopal_row_orbit_source = source;
    assert(tactix_online_configure(solver, &program, 231,
                                   TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  }
  const uint64_t planning_limits[] = {0, 1, UINT64_MAX};
  for (unsigned i = 0; i < 3; ++i) {
    step.parameters.value.substitution_redundancy
        .symmetry_chain_planning_tick_limit = planning_limits[i];
    assert(tactix_online_configure(solver, &program, 232,
                                   TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  }
  step.parameters.value.substitution_redundancy
      .symmetry_chain_planning_tick_limit = 0;
  step.parameters.value.substitution_redundancy.orbitopal_row_orbit_source = 2;
  assert(tactix_online_configure(solver, &program, 231,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy.orbitopal_row_orbit_source = -1;
  assert(tactix_online_configure(solver, &program, 231,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy.orbitopal_row_orbit_source =
      TACTIX_ONLINE_ORBITOPAL_ROW_ORBITS_DETECTED_GENERATORS;
  step.parameters.value.substitution_redundancy
      .maximum_orbitopal_clique_vertices = 128;
  step.parameters.value.substitution_redundancy
      .maximum_orbitopal_clique_ticks = 100000;
  step.parameters.value.substitution_redundancy
      .minimum_orbitopal_clique_columns = 5;
  assert(tactix_online_configure(solver, &program, 23,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);

  step.parameters.value.substitution_redundancy.orbitopal_column_order =
      TACTIX_ONLINE_ORBITOPAL_ORDER_COOCCURRENCE_GREEDY_CLIQUE;
  assert(tactix_online_configure(solver, &program, 230,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);

  step.parameters.value.substitution_redundancy.orbitopal_column_order = 99;
  assert(tactix_online_configure(solver, &program, 231,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);

  step.parameters.value.substitution_redundancy.orbitopal_column_order =
      TACTIX_ONLINE_ORBITOPAL_ORDER_COOCCURRENCE_BOUNDED_GREEDY_CLIQUE;
  assert(tactix_online_configure(solver, &program, 231,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  step.parameters.value.substitution_redundancy.orbitopal_column_order = 4;
  assert(tactix_online_configure(solver, &program, 231,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);

  step.parameters.value.substitution_redundancy.orbitopal_column_order =
      TACTIX_ONLINE_ORBITOPAL_ORDER_COOCCURRENCE_MAXIMUM_CLIQUE;
  step.parameters.value.substitution_redundancy
      .minimum_orbitopal_clique_columns = 1;
  assert(tactix_online_configure(solver, &program, 232,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);

  step.parameters.value.substitution_redundancy
      .minimum_orbitopal_clique_columns = 5;
  step.parameters.value.substitution_redundancy.orbitopal_completion =
      TACTIX_ONLINE_ORBITOPAL_COMPLETION_INDIVIDUALIZED_GRAPH;
  step.parameters.value.substitution_redundancy
      .maximum_orbitopal_product_states = 128;
  step.parameters.value.substitution_redundancy
      .orbitopal_unique_literal_amo_mode = TACTIX_ONLINE_SETTING_ENABLE;
  step.parameters.value.substitution_redundancy
      .orbitopal_generator_fallback_mode = TACTIX_ONLINE_SETTING_DISABLE;
  step.parameters.value.substitution_redundancy.orbitopal_refinement_mode =
      TACTIX_ONLINE_SETTING_ENABLE;
  step.parameters.value.substitution_redundancy
      .maximum_orbitopal_refinement_orbits = 6;
  step.parameters.value.substitution_redundancy
      .orbitopal_progress_feedback_mode = TACTIX_ONLINE_SETTING_ENABLE;
  step.parameters.value.substitution_redundancy
      .orbitopal_base_probe_filter_mode = TACTIX_ONLINE_SETTING_ENABLE;
  step.parameters.value.substitution_redundancy.orbitopal_base_probe_factor =
      8;
  step.parameters.value.substitution_redundancy.orbitopal_discovery_order =
      TACTIX_ONLINE_ORBITOPAL_DISCOVERY_UNIQUE_LITERAL_THEN_REFINEMENT;
  step.parameters.value.substitution_redundancy.maximum_graph_vertices = 1000;
  step.parameters.value.substitution_redundancy.maximum_graph_edges = 10000;
  step.parameters.value.substitution_redundancy.symmetry_binary_graph_mode =
      TACTIX_ONLINE_SYMMETRY_BINARY_GRAPH_DIRECT_EDGES;
  step.parameters.value.substitution_redundancy
      .symmetry_binary_graph_density_permille = 1250;
  step.parameters.value.substitution_redundancy.symmetry_clause_selection =
      TACTIX_ONLINE_SYMMETRY_CLAUSE_SELECTION_SHORTEST;
  step.parameters.value.substitution_redundancy.symmetry_chain_mode =
      TACTIX_ONLINE_SYMMETRY_CHAIN_FIX_AND_CUT;
  step.parameters.value.substitution_redundancy.symmetry_base_selection =
      TACTIX_ONLINE_SYMMETRY_BASE_COMMON_CLAUSE_AMO;
  assert(tactix_online_configure(solver, &program, 233,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  step.parameters.value.substitution_redundancy
      .orbitopal_progress_feedback_mode = 99;
  assert(tactix_online_configure(solver, &program, 233,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy
      .orbitopal_progress_feedback_mode = TACTIX_ONLINE_SETTING_ENABLE;
  for (int mode = TACTIX_ONLINE_SYMMETRY_BASE_FIRST_AMO_THEN_COMMON_CLAUSE;
       mode <= TACTIX_ONLINE_SYMMETRY_BASE_FIRST_SHORTEST_THEN_FORWARD_REMAINDER; ++mode) {
    step.parameters.value.substitution_redundancy.symmetry_base_selection = mode;
    assert(tactix_online_configure(solver, &program, 23312 + mode,
                                   TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  }
  step.parameters.value.substitution_redundancy.symmetry_base_selection = -1;
  assert(tactix_online_configure(solver, &program, 23317,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy.symmetry_base_selection =
      TACTIX_ONLINE_SYMMETRY_BASE_FIRST_SHORTEST_THEN_FORWARD_REMAINDER + 1;
  assert(tactix_online_configure(solver, &program, 23318,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy.symmetry_base_selection =
      TACTIX_ONLINE_SYMMETRY_BASE_FIRST_AMO_THEN_ORDERED_REMAINDER;
  for (int mode = TACTIX_ONLINE_SETTING_INHERIT;
       mode <= TACTIX_ONLINE_SETTING_ENABLE; ++mode) {
    step.parameters.value.substitution_redundancy
        .symmetry_pointwise_root_stabilizer_mode = mode;
    assert(tactix_online_configure(solver, &program, 23308 + mode,
                                   TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  }
  step.parameters.value.substitution_redundancy
      .symmetry_pointwise_root_stabilizer_mode = 99;
  assert(tactix_online_configure(solver, &program, 23311,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy
      .symmetry_pointwise_root_stabilizer_mode = TACTIX_ONLINE_SETTING_ENABLE;
  for (int point = TACTIX_ONLINE_SYMMETRY_CUT_POINT_INHERIT;
       point <= TACTIX_ONLINE_SYMMETRY_CUT_POINT_LARGEST_ORBIT_LAST; ++point)
    for (int mode = TACTIX_ONLINE_SETTING_INHERIT;
         mode <= TACTIX_ONLINE_SETTING_ENABLE; ++mode) {
      step.parameters.value.substitution_redundancy.symmetry_cut_point = point;
      step.parameters.value.substitution_redundancy
          .symmetry_negation_fixing_first_mode = mode;
      assert(tactix_online_configure(solver, &program, 23340 + 3 * point + mode,
                                     TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
    }
  step.parameters.value.substitution_redundancy.symmetry_cut_point = 99;
  assert(tactix_online_configure(solver, &program, 23350,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy.symmetry_cut_point =
      TACTIX_ONLINE_SYMMETRY_CUT_POINT_LARGEST_ORBIT_LAST;
  step.parameters.value.substitution_redundancy
      .symmetry_negation_fixing_first_mode = 99;
  assert(tactix_online_configure(solver, &program, 23351,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy
      .symmetry_negation_fixing_first_mode = TACTIX_ONLINE_SETTING_ENABLE;
  for (int mode = TACTIX_ONLINE_SETTING_INHERIT;
       mode <= TACTIX_ONLINE_SETTING_ENABLE; ++mode) {
    step.parameters.value.substitution_redundancy
        .symmetry_first_literal_fix_anchor_mode = mode;
    assert(tactix_online_configure(solver, &program, 23360 + mode,
                                   TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  }
  step.parameters.value.substitution_redundancy
      .symmetry_first_literal_fix_anchor_mode = 99;
  assert(tactix_online_configure(solver, &program, 23363,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy
      .symmetry_first_literal_fix_anchor_mode = TACTIX_ONLINE_SETTING_ENABLE;
  for (int mode = TACTIX_ONLINE_SETTING_INHERIT;
       mode <= TACTIX_ONLINE_SETTING_ENABLE; ++mode) {
    step.parameters.value.substitution_redundancy
        .symmetry_factor_support_components_mode = mode;
    assert(tactix_online_configure(solver, &program, 23320 + mode,
                                   TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  }
  step.parameters.value.substitution_redundancy
      .symmetry_factor_support_components_mode = 99;
  assert(tactix_online_configure(solver, &program, 23324,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy
      .symmetry_factor_support_components_mode = TACTIX_ONLINE_SETTING_ENABLE;
  for (int mode = TACTIX_ONLINE_SETTING_INHERIT;
       mode <= TACTIX_ONLINE_SETTING_ENABLE; ++mode) {
    step.parameters.value.substitution_redundancy
        .symmetry_preserve_detector_order_mode = mode;
    assert(tactix_online_configure(solver, &program, 23330 + mode,
                                   TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  }
  for (int invalid = -1; invalid <= 99; invalid += 100) {
    step.parameters.value.substitution_redundancy
        .symmetry_preserve_detector_order_mode = invalid;
    assert(tactix_online_configure(solver, &program, 23334,
                                   TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  }
  step.parameters.value.substitution_redundancy
      .symmetry_preserve_detector_order_mode = TACTIX_ONLINE_SETTING_ENABLE;
  for (int scope = TACTIX_ONLINE_SYMMETRY_STRUCTURED_SCOPE_INHERIT;
       scope <= TACTIX_ONLINE_SYMMETRY_STRUCTURED_SCOPE_AXIS_ANCHORS; ++scope) {
    step.parameters.value.substitution_redundancy.symmetry_structured_scope = scope;
    assert(tactix_online_configure(solver, &program, 23350 + scope,
                                   TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  }
  for (int scope = -1; scope <= 3; scope += 4) {
    step.parameters.value.substitution_redundancy.symmetry_structured_scope = scope;
    assert(tactix_online_configure(solver, &program, 23354,
                                   TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  }
  step.parameters.value.substitution_redundancy.symmetry_structured_scope =
      TACTIX_ONLINE_SYMMETRY_STRUCTURED_SCOPE_INHERIT;
  step.parameters.value.substitution_redundancy.symmetry_schreier_domain_limit =
      50000;
  assert(tactix_online_configure(solver, &program, 23355,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  step.parameters.value.substitution_redundancy.symmetry_schreier_domain_limit =
      0;
  int *handoff_fields[] = {
    &step.parameters.value.substitution_redundancy.symmetry_use_structured_generators_mode,
    &step.parameters.value.substitution_redundancy.symmetry_stabilize_structured_variables_mode
  };
  for (unsigned field = 0; field < 2; ++field) {
    for (int mode = TACTIX_ONLINE_SETTING_INHERIT; mode <= TACTIX_ONLINE_SETTING_ENABLE; ++mode) {
      *handoff_fields[field] = mode;
      assert(tactix_online_configure(solver, &program, 23340 + mode,
                                     TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
    }
    for (int invalid = -1; invalid <= 3; invalid += 4) {
      *handoff_fields[field] = invalid;
      assert(tactix_online_configure(solver, &program, 23344,
                                     TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
    }
    *handoff_fields[field] = TACTIX_ONLINE_SETTING_INHERIT;
  }

  step.parameters.value.substitution_redundancy.symmetry_chain_mode =
      TACTIX_ONLINE_SYMMETRY_CHAIN_FIX_AND_CUT_RESIFT;
  assert(tactix_online_configure(solver, &program, 23319,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  step.parameters.value.substitution_redundancy.symmetry_chain_mode = 99;
  assert(tactix_online_configure(solver, &program, 23306,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy.symmetry_chain_mode =
      TACTIX_ONLINE_SYMMETRY_CHAIN_FIX_AND_CUT;
  step.parameters.value.substitution_redundancy.symmetry_nonforcing_cut_admission =
      TACTIX_ONLINE_SYMMETRY_CUT_ADMISSION_REQUIRE_COMPOUND_PROGRESS;
  assert(tactix_online_configure(solver, &program, 23320,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  step.parameters.value.substitution_redundancy.symmetry_nonforcing_cut_admission = 99;
  assert(tactix_online_configure(solver, &program, 23321,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy.symmetry_nonforcing_cut_admission =
      TACTIX_ONLINE_SYMMETRY_CUT_ADMISSION_INHERIT;
  step.parameters.value.substitution_redundancy.symmetry_base_selection = 99;
  assert(tactix_online_configure(solver, &program, 23307,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy.symmetry_base_selection =
      TACTIX_ONLINE_SYMMETRY_BASE_COMMON_CLAUSE_AMO;
  step.parameters.value.substitution_redundancy.symmetry_clause_selection = 99;
  assert(tactix_online_configure(solver, &program, 23304,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy.symmetry_clause_selection =
      TACTIX_ONLINE_SYMMETRY_CLAUSE_SELECTION_SHORTEST;
  step.parameters.value.substitution_redundancy.symmetry_binary_graph_mode = 99;
  assert(tactix_online_configure(solver, &program, 23305,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy.symmetry_binary_graph_mode =
      TACTIX_ONLINE_SYMMETRY_BINARY_GRAPH_DIRECT_EDGES;
  {
    const int saved_producer = step.parameters.value.substitution_redundancy.producer;
    step.parameters.value.substitution_redundancy.producer = TACTIX_ONLINE_SR_ORBITOPAL;
    for (int scope = TACTIX_ONLINE_ORBITOPAL_DISCOVERY_REFINEMENT_ONLY;
         scope <= TACTIX_ONLINE_ORBITOPAL_DISCOVERY_UNIQUE_LITERAL_ONLY; ++scope) {
      step.parameters.value.substitution_redundancy.orbitopal_discovery_order = scope;
      assert(tactix_online_configure(solver, &program, 23306,
                                     TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
    }
    step.parameters.value.substitution_redundancy.orbitopal_discovery_order = 4;
    assert(tactix_online_configure(solver, &program, 23307,
                                   TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
    step.parameters.value.substitution_redundancy.orbitopal_discovery_order = -1;
    assert(tactix_online_configure(solver, &program, 23308,
                                   TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
    step.parameters.value.substitution_redundancy.producer = saved_producer;
  }
  step.parameters.value.substitution_redundancy.orbitopal_discovery_order = 99;
  assert(tactix_online_configure(solver, &program, 23300,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy.orbitopal_discovery_order =
      TACTIX_ONLINE_ORBITOPAL_DISCOVERY_UNIQUE_LITERAL_THEN_REFINEMENT;
  step.parameters.value.substitution_redundancy
      .orbitopal_unique_literal_amo_mode = 99;
  assert(tactix_online_configure(solver, &program, 2330,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy
      .orbitopal_unique_literal_amo_mode = TACTIX_ONLINE_SETTING_ENABLE;
  step.parameters.value.substitution_redundancy
      .orbitopal_generator_fallback_mode = 99;
  assert(tactix_online_configure(solver, &program, 23301,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy
      .orbitopal_generator_fallback_mode = TACTIX_ONLINE_SETTING_DISABLE;
  step.parameters.value.substitution_redundancy.orbitopal_refinement_mode = 99;
  assert(tactix_online_configure(solver, &program, 23302,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy.orbitopal_refinement_mode =
      TACTIX_ONLINE_SETTING_ENABLE;
  step.parameters.value.substitution_redundancy
      .orbitopal_base_probe_filter_mode = 99;
  assert(tactix_online_configure(solver, &program, 23303,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.substitution_redundancy
      .orbitopal_base_probe_filter_mode = TACTIX_ONLINE_SETTING_ENABLE;
  step.parameters.value.substitution_redundancy.orbitopal_base_probe_factor =
      0;
  assert(tactix_online_configure(solver, &program, 23304,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  step.parameters.value.substitution_redundancy.orbitopal_base_probe_factor =
      8;
  step.parameters.value.substitution_redundancy
      .maximum_orbitopal_product_states = 0;
  assert(tactix_online_configure(solver, &program, 2331,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  step.parameters.value.substitution_redundancy
      .maximum_orbitopal_product_states = 128;
  step.parameters.value.substitution_redundancy.orbitopal_completion = 99;
  step.parameters.value.substitution_redundancy
      .orbitopal_generator_fallback_mode = 99;
  step.parameters.value.substitution_redundancy.orbitopal_refinement_mode = 99;
  step.parameters.value.substitution_redundancy
      .orbitopal_base_probe_filter_mode = 99;
  assert(tactix_online_configure(solver, &program, 234,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);

  /* Dormant producer settings are normalized away and cannot narrow the
   * explicit X7 surface. */
  step.parameters.value.substitution_redundancy.producer =
      TACTIX_ONLINE_SR_EXPLICIT_ONLY;
  step.parameters.value.substitution_redundancy.orbitopal_producer = 0;
  step.parameters.value.substitution_redundancy.symmetry_factor_support_components_mode = 99;
  step.parameters.value.substitution_redundancy.symmetry_preserve_detector_order_mode = 99;
  step.parameters.value.substitution_redundancy.orbitopal_orientation = 99;
  step.parameters.value.substitution_redundancy.orbitopal_column_order = 99;
  step.parameters.value.substitution_redundancy.orbitopal_completion = 99;
  step.parameters.value.substitution_redundancy.symmetry_fixing_mode = 99;
  step.parameters.value.substitution_redundancy.symmetry_generator_order = 99;
  step.parameters.value.substitution_redundancy.symmetry_clause_scope = 99;
  step.parameters.value.substitution_redundancy.symmetry_pair_orientation = 99;
  step.parameters.value.substitution_redundancy.symmetry_binary_graph_mode = 99;
  step.parameters.value.substitution_redundancy.symmetry_clause_selection = 99;
  step.parameters.value.substitution_redundancy
      .symmetry_pointwise_root_stabilizer_mode = 99;
  step.parameters.value.substitution_redundancy.symmetry_chain_mode =
      TACTIX_ONLINE_SYMMETRY_CHAIN_FIX_AND_CUT_RESIFT;
  assert(tactix_online_configure(solver, &program, 23319,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  step.parameters.value.substitution_redundancy.symmetry_chain_mode = 99;
  step.parameters.value.substitution_redundancy.symmetry_nonforcing_cut_admission = 99;
  step.parameters.value.substitution_redundancy.symmetry_base_selection = 99;
  step.parameters.value.substitution_redundancy
      .symmetry_direct_generator_involutions_mode = 99;
  assert(tactix_online_configure(solver, &program, 24,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  tactix_release(solver);
}

static void walk_scoring_is_validated_and_copied(void) {
  tactix *solver = tactix_init();
  const int binary[] = {1, 2, 0};
  clause(solver, binary);
  tactix_online_walk_score_point curve[3] = {
      {0, 1500}, {3000, 2700}, {8000, 6900}};
  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = 75;
  step.rule_id = rule("Walk");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_WALK;
  step.parameters.value.walk.step_budget = 10;
  step.parameters.value.walk.random_seed = 11;
  step.parameters.value.walk.even_break_base_milli = 2200;
  step.parameters.value.walk.score_curve = curve;
  step.parameters.value.walk.score_curve_count = 3;
  step.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget = 1;
  tactix_online_program program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};

  curve[0].break_base_milli = 0;
  assert(tactix_online_configure(solver, &program, 152,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  curve[0].break_base_milli = 1500;
  curve[2].average_clause_size_milli = 3000;
  assert(tactix_online_configure(solver, &program, 15,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  curve[2].average_clause_size_milli = 8000;
  assert(tactix_online_configure(solver, &program, 16,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  /* The configured program owns the curve after this call. */
  curve[1].break_base_milli = 0;
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED ||
         result.status == TACTIX_ONLINE_NO_CHANGE);
  curve[1].break_base_milli = 2700;
  step.parameters.value.walk.seed_candidate = 1; /* kind-only is not an identity */
  assert(tactix_online_configure(solver, &program, 17,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.walk.seed_candidate = 511; /* unknown producer */
  assert(tactix_online_configure(solver, &program, 18,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.walk.seed_candidate = 256; /* well-formed, unpublished */
  assert(tactix_online_configure(solver, &program, 19,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  step.parameters.value.walk.seed_candidate = 0; /* configured request owns copy */
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_STALE_REQUEST);
  tactix_release(solver);
}

static void affine_model_parameters_require_instance_eight(void) {
  tactix *solver=tactix_init();
  for(unsigned mask=0;mask<16;++mask) {
    unsigned parity=0;for(unsigned i=0;i<4;++i)parity^=(mask>>i)&1;
    if(parity)continue;
    int lits[5];for(unsigned i=0;i<4;++i)lits[i]=(mask&(1u<<i))?-(int)(i+1):(int)(i+1);
    lits[4]=0;clause(solver,lits);
  }
  for(int i=1;i<4;++i){const int pair[]={-i,-(i+1),0};clause(solver,pair);}
  tactix_online_step step;memset(&step,0,sizeof step);
  step.step_id=908;step.rule_id=rule("Lucky");
  step.parameters.kind=TACTIX_ONLINE_PARAMETERS_AFFINE_MODEL;
  tactix_online_affine_model_parameters *p=&step.parameters.value.affine_model;
  p->maximum_variables=100;p->maximum_constraints=1000;p->maximum_arity=8;
  p->maximum_projection=10;p->minimum_equations=1;p->maximum_entries=1000;
  p->import_bytes=p->linear_bytes=p->table_bytes=1000000;
  p->maximum_work=1000000;p->candidate_budget=1000;p->permutations=1;
  p->projection_mode=TACTIX_ONLINE_AFFINE_EXPLICIT;
  int projection[]={4,-1,3,-2};p->projection=projection;p->projection_count=4;
  tactix_online_affine_scope scope={2,2,2,4};p->scopes=&scope;p->scope_count=1;
  uint64_t instance=8;step.instances=&instance;step.instance_count=1;
  step.granularity=TACTIX_ONLINE_GRANULARITY_ONE;
  step.repeat_mode=TACTIX_ONLINE_REPEAT_ONCE;step.repeat_limit=1;
  step.work_unit=TACTIX_ONLINE_WORK_APPLICATIONS;step.budget_unlimited=1;
  tactix_online_program program={&step,1,TACTIX_ONLINE_REPEAT_ONCE,1};
  assert(tactix_online_configure(solver,&program,1,
             TACTIX_ONLINE_RECONFIGURE_RESET)==0);
  instance=7;
  assert(tactix_online_configure(solver,&program,2,
             TACTIX_ONLINE_RECONFIGURE_RESET)==-1);
  tactix_online_result result;
  assert(tactix_online_advance(solver,&result)==0);
  assert(result.instance==8 && result.configuration_revision==1);
  tactix_release(solver);
}

static void balanced_signature_parameters_are_public_and_validated(void) {
  tactix *solver = tactix_init();
  clause(solver, (const int[]){1, 2, 0});
  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = 909;
  step.rule_id = rule("Lucky");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_BALANCED_SIGNATURE_MODEL;
  tactix_online_balanced_signature_model_parameters *p =
      &step.parameters.value.balanced_signature_model;
  p->maximum_balls = 256;
  p->maximum_width = 8;
  p->restarts = 160;
  p->steps_per_restart = 140000;
  p->samples_per_step = 36;
  p->seed = UINT64_C(0xabcdef1234567890);
  p->completion_rounds = 64;
  uint64_t instance = 10;
  step.instances = &instance;
  step.instance_count = 1;
  step.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  tactix_online_program program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(
             solver, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  p->maximum_balls = 2;
  assert(tactix_online_configure(
             solver, &program, 2, TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED ||
         result.status == TACTIX_ONLINE_NO_CHANGE);
  tactix_release(solver);
}

static void fermat_parameters_are_public_and_validated(void) {
  tactix *solver = tactix_init();
  clause(solver, (const int[]){1, 2, 0});
  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = 910;
  step.rule_id = rule("Lucky");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_FERMAT_MODEL;
  tactix_online_fermat_model_parameters *p =
      &step.parameters.value.fermat_model;
  p->maximum_bits = 63;
  p->comparator_tail_clauses = 80;
  p->rho_constants = 199;
  p->rho_iterations = 200000;
  uint64_t instance = 11;
  step.instances = &instance;
  step.instance_count = 1;
  step.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  tactix_online_program program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(
             solver, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  instance = 12;
  assert(tactix_online_configure(
             solver, &program, 2, TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  instance = 11;
  p->maximum_bits = 0;
  assert(tactix_online_configure(
             solver, &program, 3, TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  tactix_release(solver);
}

static void lookup_circuit_parameters_are_public_and_validated(void) {
  tactix *solver = tactix_init();
  clause(solver, (const int[]){1, 2, 0});
  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = 911;
  step.rule_id = rule("Lucky");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_LOOKUP_CIRCUIT_MODEL;
  tactix_online_lookup_circuit_model_parameters *p =
      &step.parameters.value.lookup_circuit_model;
  p->maximum_fanin = 8;
  p->maximum_fanout = 8;
  p->maximum_primary_inputs = 64;
  p->maximum_support = 32;
  p->maximum_vector_gate_evaluations = 6000000000ULL;
  uint64_t instance = 13;
  step.instances = &instance;
  step.instance_count = 1;
  step.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  tactix_online_program program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(
             solver, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  instance = 12;
  assert(tactix_online_configure(
             solver, &program, 2, TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  instance = 13;
  p->maximum_fanin = 0;
  assert(tactix_online_configure(
             solver, &program, 3, TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  tactix_release(solver);
}

static void simon_parameters_are_public_and_validated(void) {
  tactix *solver = tactix_init();
  clause(solver, (const int[]){1, 2, 0});
  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = 912;
  step.rule_id = rule("Lucky");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_SIMON_MODEL;
  tactix_online_simon_model_parameters *p = &step.parameters.value.simon_model;
  p->maximum_rounds = 64;
  p->maximum_candidates = 1ULL << 32;
  uint64_t instance = 14;
  step.instances = &instance;
  step.instance_count = 1;
  step.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  tactix_online_program program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(
             solver, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  instance = 12;
  assert(tactix_online_configure(
             solver, &program, 2, TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  instance = 14;
  p->maximum_rounds = 0;
  assert(tactix_online_configure(
             solver, &program, 3, TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  tactix_release(solver);
}

static void stconn_parameters_are_public_and_validated(void) {
  tactix *solver = tactix_init();
  clause(solver, (const int[]){1, 2, 0});
  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = 913;
  step.rule_id = rule("Lucky");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_STCONN_PROOF;
  tactix_online_stconn_proof_parameters *p =
      &step.parameters.value.stconn_proof;
  p->maximum_width = 9;
  p->maximum_columns = UINT32_MAX;
  p->maximum_rup_clauses = UINT64_MAX;
  uint64_t instance = 15;
  step.instances = &instance;
  step.instance_count = 1;
  step.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  tactix_online_program program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(
             solver, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  p->maximum_width = 10;
  assert(tactix_online_configure(
             solver, &program, 2, TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED ||
         result.status == TACTIX_ONLINE_NO_CHANGE);
  tactix_release(solver);
}

static void signed_cardinality_parameters_are_public_and_validated(void) {
  tactix *solver = tactix_init();
  clause(solver, (const int[]){1, 2, 0});
  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = 914;
  step.rule_id = rule("Lucky");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_SIGNED_CARDINALITY_PROOF;
  tactix_online_signed_cardinality_proof_parameters *p =
      &step.parameters.value.signed_cardinality_proof;
  p->maximum_variables = 4096;
  p->maximum_groups = 4096;
  p->maximum_constraints = 262144;
  p->maximum_group_arity = 8;
  p->maximum_bdd_nodes = UINT64_MAX;
  p->maximum_proof_additions = UINT64_MAX;
  uint64_t instance = 16;
  step.instances = &instance;
  step.instance_count = 1;
  step.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  tactix_online_program program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(
             solver, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  p->maximum_group_arity = 17;
  assert(tactix_online_configure(
             solver, &program, 2, TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED ||
         result.status == TACTIX_ONLINE_NO_CHANGE);
  tactix_release(solver);
}

static void exact_cover_parameters_are_public_and_validated(void) {
  tactix *solver = tactix_init();
  clause(solver, (const int[]){1, 2, 0});
  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = 915;
  step.rule_id = rule("Lucky");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_EXACT_COVER_MODEL;
  tactix_online_exact_cover_model_parameters *p =
      &step.parameters.value.exact_cover_model;
  p->block_arity = 5;
  p->maximum_nodes = 2000000;
  p->restarts = 0;
  p->restart_nodes = 2000000;
  p->seed = 0x123456789ULL;
  uint64_t instance = 17;
  step.instances = &instance;
  step.instance_count = 1;
  step.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  tactix_online_program program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  uint64_t wrong = 18;
  step.instances = &wrong;
  assert(tactix_online_configure(
             solver, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.instances = &instance;
  assert(tactix_online_configure(
             solver, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.instance == 17 && result.status == TACTIX_ONLINE_NO_CHANGE);
  tactix_release(solver);
}

static void independent_set_parameters_are_public_and_validated(void) {
  tactix *solver = tactix_init();
  clause(solver, (const int[]){1, 2, 0});
  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = 916;
  step.rule_id = rule("Lucky");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_INDEPENDENT_SET_MODEL;
  tactix_online_independent_set_model_parameters *p =
      &step.parameters.value.independent_set_model;
  p->maximum_vertices = 4096;
  p->maximum_steps = 0;
  p->completion_rounds = 0;
  p->seed = 1;
  uint64_t instance = 18;
  step.instances = &instance;
  step.instance_count = 1;
  step.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  tactix_online_program program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  uint64_t wrong = 17;
  step.instances = &wrong;
  assert(tactix_online_configure(
             solver, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.instances = &instance;
  assert(tactix_online_configure(
             solver, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.instance == 18 && result.status == TACTIX_ONLINE_NO_CHANGE);
  tactix_release(solver);
}

static void graph_labeling_parameters_are_public_and_validated(void) {
  tactix *solver = tactix_init();
  clause(solver, (const int[]){-4, 0}); /* parser-shortened final column */
  for (int column = 0; column < 4; ++column) {
    int cover[5];
    for (int row = 0; row < 4; ++row) cover[row] = 4 * row + column + 1;
    cover[4] = 0;
    clause(solver, cover);
  }
  for (int row = 0; row < 4; ++row)
    for (int a = 0; a < 4; ++a)
      for (int b = a + 1; b < 4; ++b)
        clause(solver, (const int[]){-(4 * row + a + 1), -(4 * row + b + 1), 0});
  for (int a = 0; a < 4; ++a)
    for (int b = a + 1; b < 4; ++b)
      clause(solver, (const int[]){-(4 * a + 1), -(4 * b + 1), 0});
  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = 21;
  step.rule_id = rule("Lucky");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_GRAPH_LABELING;
  tactix_online_graph_labeling_parameters *p = &step.parameters.value.graph_labeling;
  p->maximum_vertices = 4;
  p->workspace_bytes = 1000000;
  for (int i = 0; i < TACTIX_GRAPH_STAGES; ++i) p->work[i] = 10000;
  p->maximum_dfs_vertices = 4;
  p->maximum_dfs_nodes = 100;
  p->maximum_dfs_starts = 4;
  p->early_dfs_choices = 4;
  p->repair_rounds = 3;
  p->anneal_iterations = 20;
  p->stall_anneal_iterations = 3;
  uint64_t instance = 21;
  step.instances = &instance;
  step.instance_count = 1;
  step.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  tactix_online_program program = {&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  uint64_t wrong = 18;
  step.instances = &wrong;
  assert(tactix_online_configure(solver, &program, 1,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.instances = &instance;
  assert(tactix_online_configure(solver, &program, 1,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.instance == 21);
  assert(result.status == TACTIX_ONLINE_SAT ||
         result.status == TACTIX_ONLINE_COMPLETED ||
         result.status == TACTIX_ONLINE_NO_CHANGE);
  tactix_release(solver);
}

int main(void) {
  graph_labeling_parameters_are_public_and_validated();
  rejected_configuration_keeps_input_open();
  condition_cube_v3_is_atomic_and_copied();
  late_lrat_root_refutation_is_terminal();
  canonical_bve_reaches_a_real_fixpoint();
  status_reconstructs_bve_models_and_refreshes_after_restart();
  affine_model_parameters_require_instance_eight();
  balanced_signature_parameters_are_public_and_validated();
  fermat_parameters_are_public_and_validated();
  lookup_circuit_parameters_are_public_and_validated();
  simon_parameters_are_public_and_validated();
  stconn_parameters_are_public_and_validated();
  signed_cardinality_parameters_are_public_and_validated();
  exact_cover_parameters_are_public_and_validated();
  independent_set_parameters_are_public_and_validated();
  pure_bve_scope_is_public_and_validated();
  exact_factor_grid_is_copied_and_applied();
  complete_factor_grid_is_discovered_and_applied();
  exact_sr_instance_v2_is_copied_and_applied();
  general_sr_producer_is_available_through_v1();
  graph_sr_producer_is_typed_through_v1();
  invalid_graph_sr_settings_are_transactional();
  walk_scoring_is_validated_and_copied();
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
  step.post_application = 99;
  assert(tactix_online_configure(solver, &program, 84,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  assert(tactix_online_complete(solver));
  step.post_application = TACTIX_ONLINE_POST_NONE;

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

  /* The size-scaled fixpoint is the same observable mode with a derived ceiling. */
  step.repeat_mode = TACTIX_ONLINE_REPEAT_SIZE_SCALED_FIXPOINT;
  step.repeat_limit = 128;
  assert(tactix_online_configure(solver, &program, 11,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_SCOPED_FIXPOINT);
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED);
  assert(tactix_online_complete(solver));
  step.repeat_mode = 99;
  assert(tactix_online_configure(solver, &program, 12,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.repeat_mode = TACTIX_ONLINE_REPEAT_FIXPOINT;
  step.repeat_limit = 0;

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
