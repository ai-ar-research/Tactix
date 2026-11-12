#undef NDEBUG
#include "api_test_support.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Configure the one-step, run-once v1 program around `step`. */
static int configure(tactix *solver, const tactix_online_step *step,
                     uint64_t revision) {
  const tactix_online_program program = {step, 1, TACTIX_ONLINE_REPEAT_ONCE,
                                         1};
  return tactix_online_configure(solver, &program, revision,
                                 TACTIX_ONLINE_RECONFIGURE_RESET);
}

static int configure_version(tactix *solver, int version,
                             const tactix_online_step *step,
                             int bad_nested, uint64_t revision) {
  if (version == 1)
    return configure(solver, step, revision);
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

/* A zeroed step for `rule_name` at `granularity`, applied once. */
static void step_for(tactix_online_step *step, uint64_t id,
                     const char *rule_name, int granularity) {
  memset(step, 0, sizeof *step);
  step->step_id = id;
  step->rule_id = rule(rule_name);
  step->granularity = granularity;
  step_once(step);
}

/* Unbounded BVE parameters with the given growth bound. */
static void bve_step(tactix_online_step *step, uint64_t id, int granularity,
                     uint32_t growth_bound) {
  step_for(step, id, "BVE", granularity);
  step->parameters.kind = TACTIX_ONLINE_PARAMETERS_BVE;
  tactix_online_bve_parameters *bve = &step->parameters.value.bve;
  bve->growth_bound = growth_bound;
  bve->occurrence_limit = bve->clause_size_limit = UINT32_MAX;
  bve->resolvent_size_limit = bve->backward_occurrence_limit = UINT32_MAX;
  bve->xor_arity_limit = 2;
  bve->definition_cores = 1;
  step->work_unit = TACTIX_ONLINE_WORK_BVE_TICKS;
}

static void rejected_configuration_keeps_input_open(void) {
  for (int version = 1; version <= 3; ++version)
    for (int proof_mode = 0; proof_mode <= 2; ++proof_mode) {
      tactix *solver = tactix_init();
      char path[128];
      const char *proof_dir = getenv("TACTIX_ONLINE_CONFIG_PROOF_DIR");
      if (proof_dir)
        snprintf(path, sizeof path, "%s/config-%d-%d.dsr",
                 proof_dir, version, proof_mode);
      else
        snprintf(path, sizeof path,
                 "/tmp/tactix-online-config-%ld-%d-%d.dsr",
                 (long)getpid(), version, proof_mode);
      if (proof_mode == 1)
        assert(tactix_proof(solver, path, TACTIX_PROOF_DSR_BINARY) == 0);
      add_clause(solver, (const int[]){1, 0});
      assert(tactix_add_checked(solver, 2) == 0); /* unfinished clause */
      tactix_online_step step;
      memset(&step, 0, sizeof step);
      step.rule_id = -1; /* converter rejection */
      step.step_id = 1;
      step.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
      step_once(&step);
      assert(configure_version(solver, version, &step, 0, 1) == -1);
      assert(tactix_online_complete(solver) == -1);
      assert(tactix_add_checked(solver, 0) == 0);
      if (proof_mode == 2)
        assert(tactix_proof(solver, path, TACTIX_PROOF_DSR_BINARY) == 0);

      step.rule_id = rule("Propagate");
      step.work_unit = TACTIX_ONLINE_WORK_PROPAGATIONS;
      step.step_id = 0; /* executor rejection after successful conversion */
      assert(configure_version(solver, version, &step, 0, 2) == -1);
      assert(tactix_online_complete(solver) == -1);
      add_clause(solver, (const int[]){3, 0});

      step.step_id = 1;
      if (version != 1) {
        assert(configure_version(solver, version, &step, 1, 3) == -1);
        assert(tactix_online_complete(solver) == -1);
        add_clause(solver, (const int[]){4, 0});
      }
      if (proof_mode)
        add_clause(solver, (const int[]){-1, 0});
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
  add_clause(solver, (const int[]){1, 2, 0});
  add_clause(solver, (const int[]){1, 2, 0});
  add_clause(solver, (const int[]){-1, 3, 0});
  add_clause(solver, (const int[]){-3, 4, 0});
  int candidate[] = {1, 2};
  int witness[] = {1, 3, 4};
  tactix_online_condition_instance cubes[] = {
      {candidate, 2, witness, 3}, {candidate, 2, witness, 3}};
  tactix_online_step_v3 step;
  memset(&step, 0, sizeof step);
  step_for(&step.base.base, 375, "BCE", TACTIX_ONLINE_GRANULARITY_BATCH);
  step.condition_instances = cubes;
  step.condition_instance_count = 2;
  tactix_online_program_v3 program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
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

/* Early and late proof attachment can both refute the input (with different
 * unit trails) before the executor runs; the executor then reports no second
 * UNSAT transition. */
static void late_proof_root_refutation_is_terminal(void) {
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
      snprintf(path, sizeof path, "%s/%s.dsr", proof_dir,
               late ? "late" : "early");
    else
      snprintf(path, sizeof path, "/tmp/tactix-online-late-root-%ld-%d.dsr",
               (long)getpid(), late);
    tactix *solver = tactix_init();
    if (!late)
      assert(tactix_proof(solver, path, TACTIX_PROOF_DSR_BINARY) == 0);
    int clauses = 0;
    for (unsigned i = 0; i < sizeof input / sizeof input[0]; ++i) {
      tactix_add(solver, input[i]);
      if (!input[i] && ++clauses == 1 && late)
        assert(tactix_proof(solver, path, TACTIX_PROOF_DSR_BINARY) == 0);
    }
    assert(clauses == 22);
    tactix_online_step step;
    step_for(&step, 1, "Propagate", TACTIX_ONLINE_GRANULARITY_CANONICAL);
    step.work_unit = TACTIX_ONLINE_WORK_PROPAGATIONS;
    assert(configure(solver, &step, 1) == 0);
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
    add_clause(solver, (const int[]){b, a, 0});
    int progress = 0, result;
    while ((result = tactix_apply(solver, rule("BVE"))) == 0) {
      assert(++progress <= 2);
      /* Pure elimination may already sit at a propagation fixpoint. */
      const int propagated = tactix_apply(solver, rule("Propagate"));
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

static void bve_otf_strengthening_is_public_and_observable(void) {
  tactix *solver = tactix_init();
  add_clause(solver, (const int[]){1, 2, 3, 0});
  add_clause(solver, (const int[]){-1, 2, 0});
  tactix_online_step step;
  bve_step(&step, 102, TACTIX_ONLINE_GRANULARITY_ONE, 0);
  step.parameters.value.bve.resolvent_count_limit = 1;
  step.parameters.value.bve.on_the_fly_strengthening = 1;
  const uint64_t pivot = TACTIX_LIT_INSTANCE(1);
  step.instances = &pivot;
  step.instance_count = 1;
  assert(configure(solver, &step, 23) == 0);
  while (!tactix_online_complete(solver)) {
    tactix_online_result result;
    assert(tactix_online_advance(solver, &result) == 0);
  }
  assert(tactix_stat(solver, TACTIX_STAT_BVE_ELIMINATED) == 1);
  assert(tactix_stat(solver, TACTIX_STAT_BVE_OTF_STRENGTHENED) == 1);
  assert(tactix_stat(solver, TACTIX_STAT_BVE_OTF_SUBSUMED) == 0);
  assert(tactix_solve(solver) == 10);
  assert(tactix_value(solver, 1) == 1 || tactix_value(solver, 2) == 2 ||
         tactix_value(solver, 3) == 3);
  assert(tactix_value(solver, -1) == -1 || tactix_value(solver, 2) == 2);
  tactix_release(solver);
}

static void status_reconstructs_bve_models_and_refreshes_after_restart(void) {
  for (int signs = 0; signs < 4; ++signs) {
    tactix *solver = tactix_init();
    const int a = (signs & 1) ? -1 : 1;
    const int b = (signs & 2) ? -2 : 2;
    add_clause(solver, (const int[]){b, a, 0});
    tactix_online_step step;
    step_for(&step, 101, "BVE", TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS);
    step.parameters.kind = TACTIX_ONLINE_PARAMETERS_INHERIT;
    step.work_unit = TACTIX_ONLINE_WORK_BVE_TICKS;
    assert(configure(solver, &step, 1) == 0);
    int advances = 0;
    while (!tactix_online_complete(solver)) {
      tactix_online_result result;
      assert(++advances < 64);
      assert(tactix_online_advance(solver, &result) == 0);
    }
    assert(tactix_stat(solver, TACTIX_STAT_BVE_ELIMINATED) > 0);
    /* The input clause is the model oracle, before and after status(). */
    assert(tactix_value(solver, a) == a || tactix_value(solver, b) == b);
    assert(tactix_status(solver) == 10);
    assert(tactix_value(solver, a) == a || tactix_value(solver, b) == b);
    const uint64_t spent = tactix_online_step_consumed(solver, 101);
    assert(tactix_status(solver) == 10);
    assert(tactix_online_step_consumed(solver, 101) == spent);
    tactix_release(solver);
  }

  tactix *solver = tactix_init();
  add_clause(solver, (const int[]){1, 2, 0});
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
  add_clause(solver, (const int[]){1, 2, 0});
  add_clause(solver, (const int[]){-2, 3, 0});
  tactix_online_step step;
  bve_step(&step, 80, TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS, 16);
  tactix_online_bve_parameters *bve = &step.parameters.value.bve;
  bve->candidate_scope = TACTIX_ONLINE_BVE_CANDIDATES_PURE_ONLY;
  assert(configure(solver, &step, 20) == 0);
  uint64_t applications = 0;
  while (!tactix_online_complete(solver)) {
    tactix_online_result result;
    assert(tactix_online_advance(solver, &result) == 0);
    applications += result.applications;
  }
  assert(applications == 2);

  bve->candidate_scope = TACTIX_ONLINE_BVE_CANDIDATES_QUERY_PURE_CLOSURE + 1;
  assert(configure(solver, &step, 21) == -1);
  assert(tactix_online_complete(solver));
  bve->candidate_scope = 256;
  assert(configure(solver, &step, 21) == -1);
  bve->candidate_scope = TACTIX_ONLINE_BVE_CANDIDATES_PURE_ONLY;
  bve->traversal = 2;
  assert(configure(solver, &step, 22) == -1);
  assert(tactix_online_complete(solver));
  bve->traversal = 0;
  for (int scope = TACTIX_ONLINE_BVE_CANDIDATES_QUERY_ALL;
       scope <= TACTIX_ONLINE_BVE_CANDIDATES_QUERY_PURE_CLOSURE; ++scope) {
    bve->candidate_scope = scope;
    assert(configure(solver, &step, 23) == 0);
  }
  tactix_release(solver);
}

static void exact_factor_grid_is_copied_and_applied(void) {
  tactix *solver = tactix_init();
  int factor_literals[2] = {1, 2};
  int column_literals[3][1] = {{3}, {4}, {5}};
  tactix_online_factor_remainder columns[3];
  for (int row = 0; row < 2; ++row)
    for (int column = 0; column < 3; ++column)
      add_clause(solver, (const int[]){factor_literals[row],
                                       column_literals[column][0], 0});
  for (int column = 0; column < 3; ++column) {
    columns[column].literals = column_literals[column];
    columns[column].literal_count = 1;
  }
  tactix_online_factor_grid grid = {factor_literals, 2, columns, 3};
  tactix_online_step step;
  step_for(&step, 73, "Factor", TACTIX_ONLINE_GRANULARITY_ONE);
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_INHERIT;
  step.factor_grids = &grid;
  step.factor_grid_count = 1;
  assert(configure(solver, &step, 13) == 0);

  /* Configuration owns a deep copy, including every nested literal array. */
  factor_literals[0] = factor_literals[1] = 0;
  for (int column = 0; column < 3; ++column)
    column_literals[column][0] = 0;
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED);
  assert(result.instance == 0 && result.made_rule_progress);
  assert(result.after_strategy_epoch > result.before_strategy_epoch);
  tactix_release(solver);
}

static void complete_factor_grid_is_discovered_and_applied(void) {
  tactix *solver = tactix_init();
  for (int factor = 1; factor <= 2; ++factor)
    for (int remainder = 3; remainder <= 5; ++remainder)
      add_clause(solver, (const int[]){factor, remainder, 0});
  tactix_online_step step;
  step_for(&step, 74, "Factor", TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS);
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_INHERIT;
  assert(configure(solver, &step, 14) == 0);
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

/* The 2-colouring square (1 2)(3 4)(-1 -3)(-2 -4). */
static void sr_square(tactix *solver) {
  add_clause(solver, (const int[]){1, 2, 0});
  add_clause(solver, (const int[]){3, 4, 0});
  add_clause(solver, (const int[]){-1, -3, 0});
  add_clause(solver, (const int[]){-2, -4, 0});
}

static void exact_sr_instance_v2_is_copied_and_applied(void) {
  tactix *solver = tactix_init();
  sr_square(solver);
  int sr_clause[] = {-1};
  int witness[] = {-1, 3};
  tactix_online_sr_mapping mapping[] = {{2, 4}, {4, 2}};
  tactix_online_sr_instance instance = {
      sr_clause, 1, witness, 2, mapping, 2};
  tactix_online_step_v2 step;
  memset(&step, 0, sizeof step);
  step_for(&step.base, 74, "SubstitutionRedundancy",
           TACTIX_ONLINE_GRANULARITY_ONE);
  step.base.parameters.kind =
      TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY;
  step.base.parameters.value.substitution_redundancy.maximum_validation_ticks =
      UINT64_MAX;
  step.base.post_application = TACTIX_ONLINE_POST_PROPAGATE;
  step.sr_instances = &instance;
  step.sr_instance_count = 1;
  const tactix_online_program_v2 program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure_v2(solver, &program, 17,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == 0);

  /* Every nested array is owned by the configured immutable snapshot. */
  sr_clause[0] = witness[0] = witness[1] = 0;
  mapping[0].source_literal = mapping[0].target_literal = 0;
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED);
  assert(result.instance == 0 && result.made_rule_progress);
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED);
  assert(tactix_online_complete(solver));
  tactix_release(solver);
}

/* A complete-pass SR step with unbounded validation and detection. */
static tactix_online_sr_parameters *sr_step(tactix_online_step *step,
                                            uint64_t id) {
  step_for(step, id, "SubstitutionRedundancy",
           TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS);
  step->parameters.kind = TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY;
  tactix_online_sr_parameters *sr =
      &step->parameters.value.substitution_redundancy;
  sr->maximum_validation_ticks = sr->maximum_detection_ticks = UINT64_MAX;
  return sr;
}

static void general_sr_producer_is_available_through_v1(void) {
  tactix *solver = tactix_init();
  /* A live clause gives the producer a real variable domain. */
  add_clause(solver, (const int[]){1, 2, 0});
  tactix_online_step step;
  tactix_online_sr_parameters *sr = sr_step(&step, 76);
  sr->general_producer = 1;
  sr->maximum_general_candidates_per_advance = UINT64_MAX;
  step.budget_unlimited = 0;
  step.budget = 1;
  assert(configure(solver, &step, 18) == 0);
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.applications == 1 && result.made_rule_progress);
  tactix_release(solver);
}

static void accept_values(tactix *solver, const tactix_online_step *step,
                          int *field, int low, int high) {
  for (int value = low; value <= high; ++value) {
    *field = value;
    assert(configure(solver, step, 23) == 0);
  }
}

static void reject_value(tactix *solver, const tactix_online_step *step,
                         int *field, int bad, int restore) {
  *field = bad;
  assert(configure(solver, step, 23) == -1);
  *field = restore;
}

/* Graph-symmetry SR is typed through v1; each invalid setting is rejected
 * without replacing the live snapshot, and dormant settings are ignored. */
static void invalid_graph_sr_settings_are_transactional(void) {
  tactix *solver = tactix_init();
  sr_square(solver);
  tactix_online_step step;
  tactix_online_sr_parameters *sr = sr_step(&step, 78);
  sr->producer = TACTIX_ONLINE_SR_GRAPH_SYMMETRY;
  sr->maximum_graph_vertices = 128;
  sr->maximum_graph_edges = 256;
  sr->maximum_symmetry_generators = 64;
  sr->symmetry_backend_search_budget = 64;
  sr->symmetry_backend_support_budget = UINT64_MAX;
  sr->symmetry_backend_component_size_limit = UINT64_MAX;
  sr->maximum_symmetry_fixing_instances = 64;
  sr->maximum_symmetry_generator_instances_per_pass = 1;
  sr->maximum_symmetry_group_ticks = 100000;
  assert(configure(solver, &step, 20) == 0);
  reject_value(solver, &step, &sr->producer, 99,
               TACTIX_ONLINE_SR_GRAPH_SYMMETRY);
  reject_value(solver, &step, &sr->orbitopal_producer, 1, 1);
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.configuration_revision == 20 &&
         result.status == TACTIX_ONLINE_COMPLETED &&
         result.made_rule_progress);

  /* Matching legacy and typed selectors are accepted together. */
  sr->producer = TACTIX_ONLINE_SR_ORBITOPAL;
  sr->minimum_orbitopal_rows = sr->minimum_orbitopal_columns = 2;
  sr->orbitopal_column_order =
      TACTIX_ONLINE_ORBITOPAL_ORDER_COOCCURRENCE_MAXIMUM_CLIQUE;
  accept_values(solver, &step, &sr->orbitopal_row_orbit_source,
                TACTIX_ONLINE_ORBITOPAL_ROW_ORBITS_DETECTED_GENERATORS,
                TACTIX_ONLINE_ORBITOPAL_ROW_ORBITS_REFINEMENT_CELLS);
  const uint64_t planning_limits[] = {0, 1, UINT64_MAX};
  for (unsigned i = 0; i < 3; ++i) {
    sr->symmetry_chain_planning_tick_limit = planning_limits[i];
    assert(configure(solver, &step, 232) == 0);
  }
  sr->symmetry_chain_planning_tick_limit = 0;
  const int detected = TACTIX_ONLINE_ORBITOPAL_ROW_ORBITS_DETECTED_GENERATORS;
  reject_value(solver, &step, &sr->orbitopal_row_orbit_source, 2, detected);
  reject_value(solver, &step, &sr->orbitopal_row_orbit_source, -1, detected);
  sr->maximum_orbitopal_clique_vertices = 128;
  sr->maximum_orbitopal_clique_ticks = 100000;
  sr->minimum_orbitopal_clique_columns = 5;
  assert(configure(solver, &step, 23) == 0);
  accept_values(solver, &step, &sr->orbitopal_column_order,
                TACTIX_ONLINE_ORBITOPAL_ORDER_COOCCURRENCE_GREEDY_CLIQUE,
                TACTIX_ONLINE_ORBITOPAL_ORDER_COOCCURRENCE_GREEDY_CLIQUE);
  reject_value(solver, &step, &sr->orbitopal_column_order, 99, 0);
  accept_values(
      solver, &step, &sr->orbitopal_column_order,
      TACTIX_ONLINE_ORBITOPAL_ORDER_COOCCURRENCE_BOUNDED_GREEDY_CLIQUE,
      TACTIX_ONLINE_ORBITOPAL_ORDER_COOCCURRENCE_BOUNDED_GREEDY_CLIQUE);
  reject_value(solver, &step, &sr->orbitopal_column_order, 4,
               TACTIX_ONLINE_ORBITOPAL_ORDER_COOCCURRENCE_MAXIMUM_CLIQUE);
  sr->minimum_orbitopal_clique_columns = 1;
  assert(configure(solver, &step, 232) == -1);
  sr->minimum_orbitopal_clique_columns = 5;

  sr->orbitopal_completion =
      TACTIX_ONLINE_ORBITOPAL_COMPLETION_INDIVIDUALIZED_GRAPH;
  sr->maximum_orbitopal_product_states = 128;
  sr->orbitopal_unique_literal_amo_mode = TACTIX_ONLINE_SETTING_ENABLE;
  sr->orbitopal_generator_fallback_mode = TACTIX_ONLINE_SETTING_DISABLE;
  sr->orbitopal_refinement_mode = TACTIX_ONLINE_SETTING_ENABLE;
  sr->maximum_orbitopal_refinement_orbits = 6;
  sr->orbitopal_progress_feedback_mode = TACTIX_ONLINE_SETTING_ENABLE;
  sr->orbitopal_base_probe_filter_mode = TACTIX_ONLINE_SETTING_ENABLE;
  sr->orbitopal_base_probe_factor = 8;
  sr->orbitopal_discovery_order =
      TACTIX_ONLINE_ORBITOPAL_DISCOVERY_UNIQUE_LITERAL_THEN_REFINEMENT;
  sr->maximum_graph_vertices = 1000;
  sr->maximum_graph_edges = 10000;
  sr->symmetry_binary_graph_mode =
      TACTIX_ONLINE_SYMMETRY_BINARY_GRAPH_DIRECT_EDGES;
  sr->symmetry_binary_graph_density_permille = 1250;
  sr->symmetry_clause_selection =
      TACTIX_ONLINE_SYMMETRY_CLAUSE_SELECTION_SHORTEST;
  sr->symmetry_chain_mode = TACTIX_ONLINE_SYMMETRY_CHAIN_FIX_AND_CUT;
  sr->symmetry_base_selection = TACTIX_ONLINE_SYMMETRY_BASE_COMMON_CLAUSE_AMO;
  assert(configure(solver, &step, 233) == 0);
  reject_value(solver, &step, &sr->orbitopal_progress_feedback_mode, 99,
               TACTIX_ONLINE_SETTING_ENABLE);

  const int first_base =
      TACTIX_ONLINE_SYMMETRY_BASE_FIRST_AMO_THEN_COMMON_CLAUSE;
  const int last_base =
      TACTIX_ONLINE_SYMMETRY_BASE_FIRST_SHORTEST_THEN_FORWARD_REMAINDER;
  accept_values(solver, &step, &sr->symmetry_base_selection, first_base,
                last_base);
  reject_value(solver, &step, &sr->symmetry_base_selection, -1, 0);
  reject_value(solver, &step, &sr->symmetry_base_selection, last_base + 1,
               TACTIX_ONLINE_SYMMETRY_BASE_FIRST_AMO_THEN_ORDERED_REMAINDER);
  accept_values(solver, &step, &sr->symmetry_pointwise_root_stabilizer_mode,
                TACTIX_ONLINE_SETTING_INHERIT, TACTIX_ONLINE_SETTING_ENABLE);
  reject_value(solver, &step, &sr->symmetry_pointwise_root_stabilizer_mode,
               99, TACTIX_ONLINE_SETTING_ENABLE);
  for (int point = TACTIX_ONLINE_SYMMETRY_CUT_POINT_INHERIT;
       point <= TACTIX_ONLINE_SYMMETRY_CUT_POINT_LARGEST_ORBIT_LAST; ++point) {
    sr->symmetry_cut_point = point;
    accept_values(solver, &step, &sr->symmetry_negation_fixing_first_mode,
                  TACTIX_ONLINE_SETTING_INHERIT, TACTIX_ONLINE_SETTING_ENABLE);
  }
  reject_value(solver, &step, &sr->symmetry_cut_point, 99,
               TACTIX_ONLINE_SYMMETRY_CUT_POINT_LARGEST_ORBIT_LAST);
  reject_value(solver, &step, &sr->symmetry_negation_fixing_first_mode, 99,
               TACTIX_ONLINE_SETTING_ENABLE);
  int *const tristate[] = {&sr->symmetry_first_literal_fix_anchor_mode,
                           &sr->symmetry_factor_support_components_mode,
                           &sr->symmetry_preserve_detector_order_mode};
  for (unsigned i = 0; i < 3; ++i) {
    accept_values(solver, &step, tristate[i], TACTIX_ONLINE_SETTING_INHERIT,
                  TACTIX_ONLINE_SETTING_ENABLE);
    reject_value(solver, &step, tristate[i], 99, TACTIX_ONLINE_SETTING_ENABLE);
  }
  reject_value(solver, &step, &sr->symmetry_preserve_detector_order_mode, -1,
               TACTIX_ONLINE_SETTING_ENABLE);
  accept_values(solver, &step, &sr->symmetry_structured_scope,
                TACTIX_ONLINE_SYMMETRY_STRUCTURED_SCOPE_INHERIT,
                TACTIX_ONLINE_SYMMETRY_STRUCTURED_SCOPE_AXIS_ANCHORS);
  reject_value(solver, &step, &sr->symmetry_structured_scope, -1, 0);
  reject_value(solver, &step, &sr->symmetry_structured_scope, 3,
               TACTIX_ONLINE_SYMMETRY_STRUCTURED_SCOPE_INHERIT);
  sr->symmetry_schreier_domain_limit = 50000;
  assert(configure(solver, &step, 23355) == 0);
  sr->symmetry_schreier_domain_limit = 0;
  int *const handoff[] = {
      &sr->symmetry_use_structured_generators_mode,
      &sr->symmetry_stabilize_structured_variables_mode};
  for (unsigned i = 0; i < 2; ++i) {
    accept_values(solver, &step, handoff[i], TACTIX_ONLINE_SETTING_INHERIT,
                  TACTIX_ONLINE_SETTING_ENABLE);
    reject_value(solver, &step, handoff[i], -1, 0);
    reject_value(solver, &step, handoff[i], 3, TACTIX_ONLINE_SETTING_INHERIT);
  }

  accept_values(solver, &step, &sr->symmetry_chain_mode,
                TACTIX_ONLINE_SYMMETRY_CHAIN_FIX_AND_CUT_RESIFT,
                TACTIX_ONLINE_SYMMETRY_CHAIN_FIX_AND_CUT_RESIFT);
  reject_value(solver, &step, &sr->symmetry_chain_mode, 99,
               TACTIX_ONLINE_SYMMETRY_CHAIN_FIX_AND_CUT);
  accept_values(
      solver, &step, &sr->symmetry_nonforcing_cut_admission,
      TACTIX_ONLINE_SYMMETRY_CUT_ADMISSION_REQUIRE_COMPOUND_PROGRESS,
      TACTIX_ONLINE_SYMMETRY_CUT_ADMISSION_REQUIRE_COMPOUND_PROGRESS);
  reject_value(solver, &step, &sr->symmetry_nonforcing_cut_admission, 99,
               TACTIX_ONLINE_SYMMETRY_CUT_ADMISSION_INHERIT);
  reject_value(solver, &step, &sr->symmetry_base_selection, 99,
               TACTIX_ONLINE_SYMMETRY_BASE_COMMON_CLAUSE_AMO);
  reject_value(solver, &step, &sr->symmetry_clause_selection, 99,
               TACTIX_ONLINE_SYMMETRY_CLAUSE_SELECTION_SHORTEST);
  reject_value(solver, &step, &sr->symmetry_binary_graph_mode, 99,
               TACTIX_ONLINE_SYMMETRY_BINARY_GRAPH_DIRECT_EDGES);
  accept_values(solver, &step, &sr->orbitopal_discovery_order,
                TACTIX_ONLINE_ORBITOPAL_DISCOVERY_REFINEMENT_ONLY,
                TACTIX_ONLINE_ORBITOPAL_DISCOVERY_UNIQUE_LITERAL_ONLY);
  const int unique_then_refinement =
      TACTIX_ONLINE_ORBITOPAL_DISCOVERY_UNIQUE_LITERAL_THEN_REFINEMENT;
  const int bad_discovery[] = {4, -1, 99};
  for (unsigned i = 0; i < 3; ++i)
    reject_value(solver, &step, &sr->orbitopal_discovery_order,
                 bad_discovery[i], unique_then_refinement);
  reject_value(solver, &step, &sr->orbitopal_unique_literal_amo_mode, 99,
               TACTIX_ONLINE_SETTING_ENABLE);
  reject_value(solver, &step, &sr->orbitopal_generator_fallback_mode, 99,
               TACTIX_ONLINE_SETTING_DISABLE);
  reject_value(solver, &step, &sr->orbitopal_refinement_mode, 99,
               TACTIX_ONLINE_SETTING_ENABLE);
  reject_value(solver, &step, &sr->orbitopal_base_probe_filter_mode, 99,
               TACTIX_ONLINE_SETTING_ENABLE);
  sr->orbitopal_base_probe_factor = 0;
  assert(configure(solver, &step, 23304) == 0);
  sr->orbitopal_base_probe_factor = 8;
  sr->maximum_orbitopal_product_states = 0;
  assert(configure(solver, &step, 2331) == 0);
  sr->maximum_orbitopal_product_states = 128;
  sr->orbitopal_completion = 99;
  sr->orbitopal_generator_fallback_mode = 99;
  sr->orbitopal_refinement_mode = 99;
  sr->orbitopal_base_probe_filter_mode = 99;
  assert(configure(solver, &step, 234) == -1);

  /* Dormant producer settings are normalized away and cannot narrow the
   * explicit surface. */
  sr->producer = TACTIX_ONLINE_SR_EXPLICIT_ONLY;
  sr->orbitopal_producer = 0;
  int *const dormant[] = {
      &sr->symmetry_factor_support_components_mode,
      &sr->symmetry_preserve_detector_order_mode,
      &sr->orbitopal_orientation, &sr->orbitopal_column_order,
      &sr->orbitopal_completion, &sr->symmetry_fixing_mode,
      &sr->symmetry_generator_order, &sr->symmetry_clause_scope,
      &sr->symmetry_pair_orientation, &sr->symmetry_binary_graph_mode,
      &sr->symmetry_clause_selection,
      &sr->symmetry_pointwise_root_stabilizer_mode};
  for (unsigned i = 0; i < sizeof dormant / sizeof dormant[0]; ++i)
    *dormant[i] = 99;
  sr->symmetry_chain_mode = TACTIX_ONLINE_SYMMETRY_CHAIN_FIX_AND_CUT_RESIFT;
  assert(configure(solver, &step, 23319) == 0);
  sr->symmetry_chain_mode = 99;
  sr->symmetry_nonforcing_cut_admission = 99;
  sr->symmetry_base_selection = 99;
  sr->symmetry_direct_generator_involutions_mode = 99;
  assert(configure(solver, &step, 24) == 0);
  tactix_release(solver);
}

static void walk_scoring_is_validated_and_copied(void) {
  tactix *solver = tactix_init();
  add_clause(solver, (const int[]){1, 2, 0});
  tactix_online_walk_score_point curve[3] = {
      {0, 1500}, {3000, 2700}, {8000, 6900}};
  tactix_online_step step;
  step_for(&step, 75, "Walk", TACTIX_ONLINE_GRANULARITY_CANONICAL);
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_WALK;
  tactix_online_walk_parameters *walk = &step.parameters.value.walk;
  walk->step_budget = 10;
  walk->random_seed = 11;
  walk->even_break_base_milli = 2200;
  walk->score_curve = curve;
  walk->score_curve_count = 3;
  step.budget_unlimited = 0;
  step.budget = 1;

  curve[0].break_base_milli = 0;
  assert(configure(solver, &step, 152) == -1);
  curve[0].break_base_milli = 1500;
  curve[2].average_clause_size_milli = 3000;
  assert(configure(solver, &step, 15) == -1);
  curve[2].average_clause_size_milli = 8000;
  assert(configure(solver, &step, 16) == 0);
  /* The configured program owns the curve after this call. */
  curve[1].break_base_milli = 0;
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED ||
         result.status == TACTIX_ONLINE_NO_CHANGE);
  curve[1].break_base_milli = 2700;
  walk->seed_candidate = 1; /* kind-only is not an identity */
  assert(configure(solver, &step, 17) == -1);
  walk->seed_candidate = 511; /* unknown producer */
  assert(configure(solver, &step, 18) == -1);
  walk->seed_candidate = 256; /* well-formed, unpublished */
  assert(configure(solver, &step, 19) == 0);
  walk->seed_candidate = 0; /* configured request owns copy */
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_STALE_REQUEST);
  walk->algorithm = 99;
  assert(configure(solver, &step, 20) == -1);
  walk->algorithm = TACTIX_WALK_ALGORITHM_PROBSAT;
  walk->initialization = TACTIX_WALK_INITIALIZATION_NEGATIVE;
  assert(configure(solver, &step, 21) == -1);
  walk->algorithm = TACTIX_WALK_ALGORITHM_CONFIGURATION_CHECKING;
  walk->initialization = TACTIX_WALK_INITIALIZATION_RANDOM;
  walk->maximum_tries = 2;
  walk->flips_per_try = 100;
  walk->weight_smoothing_threshold = 5;
  walk->smoothing_old_numerator = 1;
  walk->smoothing_denominator = 2;
  walk->aspiration_mode = TACTIX_ONLINE_SETTING_ENABLE;
  assert(configure(solver, &step, 22) == 0);
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status != TACTIX_ONLINE_INVALID_REQUEST);
  tactix_release(solver);
}

/* A one-application Lucky step for `*instance` with typed parameters. */
static void lucky_step(tactix_online_step *step, uint64_t id, int kind,
                       uint64_t *instance) {
  step_for(step, id, "Lucky", TACTIX_ONLINE_GRANULARITY_ONE);
  step->parameters.kind = kind;
  step->instances = instance;
  step->instance_count = 1;
}

/* Advance once; a model producer either completes or changes nothing. */
static void advance_model_producer(tactix *solver) {
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED ||
         result.status == TACTIX_ONLINE_NO_CHANGE);
}

static void affine_model_parameters_require_instance_eight(void) {
  tactix *solver = tactix_init();
  for (unsigned mask = 0; mask < 16; ++mask) {
    unsigned parity = 0;
    for (unsigned i = 0; i < 4; ++i)
      parity ^= (mask >> i) & 1;
    if (parity)
      continue;
    int lits[5];
    for (unsigned i = 0; i < 4; ++i)
      lits[i] = (mask & (1u << i)) ? -(int)(i + 1) : (int)(i + 1);
    lits[4] = 0;
    add_clause(solver, lits);
  }
  for (int i = 1; i < 4; ++i)
    add_clause(solver, (const int[]){-i, -(i + 1), 0});
  tactix_online_step step;
  uint64_t instance = 8;
  lucky_step(&step, 908, TACTIX_ONLINE_PARAMETERS_AFFINE_MODEL, &instance);
  tactix_online_affine_model_parameters *p =
      &step.parameters.value.affine_model;
  p->maximum_variables = 100;
  p->maximum_constraints = 1000;
  p->maximum_arity = 8;
  p->maximum_projection = 10;
  p->minimum_equations = 1;
  p->maximum_entries = 1000;
  p->import_bytes = p->linear_bytes = p->table_bytes = 1000000;
  p->maximum_work = 1000000;
  p->candidate_budget = 1000;
  p->permutations = 1;
  p->projection_mode = TACTIX_ONLINE_AFFINE_EXPLICIT;
  int projection[] = {4, -1, 3, -2};
  p->projection = projection;
  p->projection_count = 4;
  tactix_online_affine_scope scope = {2, 2, 2, 4};
  p->scopes = &scope;
  p->scope_count = 1;
  assert(configure(solver, &step, 1) == 0);
  instance = 7;
  assert(configure(solver, &step, 2) == -1);
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.instance == 8 && result.configuration_revision == 1);
  tactix_release(solver);
}

/* Each model/proof producer's typed parameters are accepted for its own
 * instance and rejected on a wrong instance or an out-of-range limit. */
static void lucky_parameters_are_public_and_validated(void) {
  tactix *solver = tactix_init();
  add_clause(solver, (const int[]){1, 2, 0});
  tactix_online_step step;
  uint64_t instance = 10;
  lucky_step(&step, 909, TACTIX_ONLINE_PARAMETERS_BALANCED_SIGNATURE_MODEL,
             &instance);
  tactix_online_balanced_signature_model_parameters *balanced =
      &step.parameters.value.balanced_signature_model;
  balanced->maximum_balls = 256;
  balanced->maximum_width = 8;
  balanced->restarts = 160;
  balanced->steps_per_restart = 140000;
  balanced->samples_per_step = 36;
  balanced->seed = UINT64_C(0xabcdef1234567890);
  balanced->completion_rounds = 64;
  assert(configure(solver, &step, 1) == 0);
  balanced->maximum_balls = 2;
  assert(configure(solver, &step, 2) == -1);
  advance_model_producer(solver);
  tactix_release(solver);

  solver = tactix_init();
  add_clause(solver, (const int[]){1, 2, 0});
  instance = 11;
  lucky_step(&step, 910, TACTIX_ONLINE_PARAMETERS_FERMAT_MODEL, &instance);
  tactix_online_fermat_model_parameters *fermat =
      &step.parameters.value.fermat_model;
  fermat->maximum_bits = 63;
  fermat->comparator_tail_clauses = 80;
  fermat->rho_constants = 199;
  fermat->rho_iterations = 200000;
  assert(configure(solver, &step, 1) == 0);
  instance = 12;
  assert(configure(solver, &step, 2) == -1);
  instance = 11;
  fermat->maximum_bits = 0;
  assert(configure(solver, &step, 3) == -1);
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  tactix_release(solver);

  solver = tactix_init();
  add_clause(solver, (const int[]){1, 2, 0});
  instance = 13;
  lucky_step(&step, 911, TACTIX_ONLINE_PARAMETERS_LOOKUP_CIRCUIT_MODEL,
             &instance);
  tactix_online_lookup_circuit_model_parameters *lookup =
      &step.parameters.value.lookup_circuit_model;
  lookup->maximum_fanin = 8;
  lookup->maximum_fanout = 8;
  lookup->maximum_primary_inputs = 64;
  lookup->maximum_support = 32;
  lookup->maximum_vector_gate_evaluations = 6000000000ULL;
  assert(configure(solver, &step, 1) == 0);
  instance = 12;
  assert(configure(solver, &step, 2) == -1);
  instance = 13;
  lookup->maximum_fanin = 0;
  assert(configure(solver, &step, 3) == -1);
  assert(tactix_online_advance(solver, &result) == 0);
  tactix_release(solver);

  solver = tactix_init();
  add_clause(solver, (const int[]){1, 2, 0});
  instance = 14;
  lucky_step(&step, 912, TACTIX_ONLINE_PARAMETERS_SIMON_MODEL, &instance);
  tactix_online_simon_model_parameters *simon =
      &step.parameters.value.simon_model;
  simon->maximum_rounds = 64;
  simon->maximum_candidates = 1ULL << 32;
  assert(configure(solver, &step, 1) == 0);
  instance = 12;
  assert(configure(solver, &step, 2) == -1);
  instance = 14;
  simon->maximum_rounds = 0;
  assert(configure(solver, &step, 3) == -1);
  assert(tactix_online_advance(solver, &result) == 0);
  tactix_release(solver);

  solver = tactix_init();
  add_clause(solver, (const int[]){1, 2, 0});
  instance = 15;
  lucky_step(&step, 913, TACTIX_ONLINE_PARAMETERS_STCONN_PROOF, &instance);
  tactix_online_stconn_proof_parameters *stconn =
      &step.parameters.value.stconn_proof;
  stconn->maximum_width = 9;
  stconn->maximum_columns = UINT32_MAX;
  stconn->maximum_rup_clauses = UINT64_MAX;
  assert(configure(solver, &step, 1) == 0);
  stconn->maximum_width = 10;
  assert(configure(solver, &step, 2) == -1);
  advance_model_producer(solver);
  tactix_release(solver);

  solver = tactix_init();
  add_clause(solver, (const int[]){1, 2, 0});
  instance = 16;
  lucky_step(&step, 914, TACTIX_ONLINE_PARAMETERS_SIGNED_CARDINALITY_PROOF,
             &instance);
  tactix_online_signed_cardinality_proof_parameters *signed_cardinality =
      &step.parameters.value.signed_cardinality_proof;
  signed_cardinality->maximum_variables = 4096;
  signed_cardinality->maximum_groups = 4096;
  signed_cardinality->maximum_constraints = 262144;
  signed_cardinality->maximum_group_arity = 8;
  signed_cardinality->maximum_bdd_nodes = UINT64_MAX;
  signed_cardinality->maximum_proof_additions = UINT64_MAX;
  assert(configure(solver, &step, 1) == 0);
  signed_cardinality->maximum_group_arity = 17;
  assert(configure(solver, &step, 2) == -1);
  advance_model_producer(solver);
  tactix_release(solver);
}

/* Model producers whose instance is fixed: a wrong instance is rejected and
 * the right one leaves this small input unchanged (graph labeling may also
 * solve it). */
static void model_instances_are_public_and_validated(void) {
  uint64_t instance, wrong;
  tactix_online_step step;
  tactix_online_result result;
  for (int producer = 0; producer < 3; ++producer) {
    tactix *solver = tactix_init();
    if (producer < 2) {
      add_clause(solver, (const int[]){1, 2, 0});
    } else {
      add_clause(solver, (const int[]){-4, 0}); /* parser-shortened column */
      for (int column = 0; column < 4; ++column)
        add_clause(solver, (const int[]){column + 1, column + 5, column + 9,
                                         column + 13, 0});
      for (int row = 0; row < 4; ++row)
        for (int a = 0; a < 4; ++a)
          for (int b = a + 1; b < 4; ++b)
            add_clause(solver, (const int[]){-(4 * row + a + 1),
                                             -(4 * row + b + 1), 0});
      for (int a = 0; a < 4; ++a)
        for (int b = a + 1; b < 4; ++b)
          add_clause(solver, (const int[]){-(4 * a + 1), -(4 * b + 1), 0});
    }
    if (producer == 0) {
      instance = 17;
      wrong = 18;
      lucky_step(&step, 915, TACTIX_ONLINE_PARAMETERS_EXACT_COVER_MODEL,
                 &instance);
      tactix_online_exact_cover_model_parameters *p =
          &step.parameters.value.exact_cover_model;
      p->block_arity = 5;
      p->maximum_nodes = 2000000;
      p->restarts = 0;
      p->restart_nodes = 2000000;
      p->seed = 0x123456789ULL;
    } else if (producer == 1) {
      instance = 18;
      wrong = 17;
      lucky_step(&step, 916, TACTIX_ONLINE_PARAMETERS_INDEPENDENT_SET_MODEL,
                 &instance);
      tactix_online_independent_set_model_parameters *p =
          &step.parameters.value.independent_set_model;
      p->maximum_vertices = 4096;
      p->maximum_steps = 0;
      p->completion_rounds = 0;
      p->seed = 1;
    } else {
      instance = 21;
      wrong = 18;
      lucky_step(&step, 21, TACTIX_ONLINE_PARAMETERS_GRAPH_LABELING,
                 &instance);
      tactix_online_graph_labeling_parameters *p =
          &step.parameters.value.graph_labeling;
      p->maximum_vertices = 4;
      p->workspace_bytes = 1000000;
      for (int i = 0; i < TACTIX_GRAPH_STAGES; ++i)
        p->work[i] = 10000;
      p->maximum_dfs_vertices = 4;
      p->maximum_dfs_nodes = 100;
      p->maximum_dfs_starts = 4;
      p->early_dfs_choices = 4;
      p->repair_rounds = 3;
      p->anneal_iterations = 20;
      p->stall_anneal_iterations = 3;
    }
    step.instances = &wrong;
    assert(configure(solver, &step, 1) == -1);
    step.instances = &instance;
    assert(configure(solver, &step, 1) == 0);
    assert(tactix_online_advance(solver, &result) == 0);
    assert(result.instance == instance);
    assert(result.status == TACTIX_ONLINE_NO_CHANGE ||
           (producer == 2 && (result.status == TACTIX_ONLINE_SAT ||
                              result.status == TACTIX_ONLINE_COMPLETED)));
    tactix_release(solver);
  }
}

static void independent_online_continuations(void) {
  tactix *s = tactix_init(), *foreign = tactix_init();
  tactix_online_executor *e = tactix_online_executor_create(s);
  assert(e && !tactix_online_executor_create(NULL));
  assert(tactix_online_executor_complete(NULL) == -1);
  assert(tactix_online_executor_consumed(NULL, 1) == UINT64_MAX);
  tactix_online_execution_result out, untouched;
  memset(&out, 0x5a, sizeof out);
  untouched = out;
  assert(tactix_online_executor_advance(s, e, NULL, &out) == -1);
  assert(!memcmp(&out, &untouched, sizeof out));
  int clause[] = {1, 2}, witness[] = {1, 3, 4};
  tactix_online_condition_instance cubes[] = {{clause, 2, witness, 3},
                                              {clause, 2, witness, 3}};
  tactix_online_step_v3 step = {0};
  step_for(&step.base.base, 375, "Condition",
           TACTIX_ONLINE_GRANULARITY_BATCH);
  step.condition_instances = cubes;
  step.condition_instance_count = 2;
  tactix_online_program_v3 program = {&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_executor_configure(foreign, e, 3, &program, 1, 0) == -1);
  assert(tactix_add_checked(foreign, 1) == 0 &&
         tactix_add_checked(foreign, 0) == 0);
  assert(tactix_online_executor_configure(s, e, 18, &program, 1, 0) == -1);
  add_clause(s, (const int[]){1, 2, 0});
  add_clause(s, (const int[]){1, 2, 0});
  add_clause(s, (const int[]){-1, 3, 0});
  add_clause(s, (const int[]){-3, 4, 0});
  assert(!tactix_online_executor_configure(s, e, 3, &program, 2, 0));
  memset(clause, 0, sizeof clause);
  memset(witness, 0, sizeof witness);
  tactix_online_discovery_budget pool = {17, 11, 0, 17, 2, 0};
  assert(tactix_online_executor_advance(s, e, &pool, &out) == -1);
  assert(pool.consumed == 11 && !memcmp(&out, &untouched, sizeof out));
  pool.unlimited = 0;
  assert(!tactix_online_executor_advance(s, e, &pool, &out));
  assert(out.base.status == TACTIX_ONLINE_COMPLETED &&
         out.base.applications == 1);
  assert(!out.query_rejected_model && !out.discovery_progress_units);
  assert(pool.limit == 17 && pool.consumed == 11 && !pool.accounting_overflow);
  assert(tactix_online_executor_consumed(e, 375) == 1);
  tactix_online_executor *copy = tactix_online_executor_clone(e);
  assert(copy && tactix_online_executor_consumed(copy, 375) == 1);
  program.step_count = -1;
  assert(tactix_online_executor_configure(s, e, 3, &program, 9, 0) == -1);
  assert(!tactix_online_executor_advance(s, e, NULL, &out));
  assert(out.base.status == TACTIX_ONLINE_STALE_REQUEST &&
         !out.base.applications);
  assert(!tactix_online_executor_advance(s, copy, NULL, &out));
  assert(out.base.status == TACTIX_ONLINE_STALE_REQUEST &&
         out.base.configuration_revision == 2);
  tactix_release(s);
  assert(tactix_online_executor_complete(e) == -1);
  assert(tactix_online_executor_consumed(copy, 375) == UINT64_MAX);
  assert(!tactix_online_executor_clone(e));
  assert(tactix_online_executor_advance(foreign, copy, NULL, &out) == -1);
  tactix_online_executor_release(e);
  tactix_online_executor_release(copy);
  tactix_online_executor_release(NULL);
  tactix_release(foreign);
}

/* Revisioned Strengthen programs: per-instance batch, transactional
 * rejection, scoped fixpoints and typed Vivify validation. */
static void strengthen_program_revisions(void) {
  tactix *solver = tactix_init();
  add_clause(solver, (const int[]){1, 2, 0});
  add_clause(solver, (const int[]){1, 2, 3, 0});
  uint64_t instance[4];
  const int strengthen = rule("Strengthen");
  assert(tactix_rule_semantic_instances(solver, strengthen, instance, 4) == 1);
  tactix_online_step step;
  step_for(&step, 41, "Strengthen", TACTIX_ONLINE_GRANULARITY_BATCH);
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_STRENGTHEN;
  step.instances = instance;
  step.instance_count = 1;
  step.budget_unlimited = 0;
  step.budget = 1;
  assert(configure(solver, &step, 7) == 0);
  assert(!tactix_online_complete(solver));
  tactix_online_result result;
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED);
  assert(result.configuration_revision == 7 && result.step_id == 41);
  assert(result.rule_id == (uint32_t)strengthen &&
         result.instance == instance[0]);
  assert(result.applications == 1 && result.work == 1);
  assert(result.made_rule_progress && result.selection_complete);
  assert(tactix_online_step_consumed(solver, 41) == 1);
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED && result.selection_complete);
  assert(tactix_online_complete(solver));

  /* Invalid replacement is transactional: the completed revision survives. */
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_BVE;
  assert(configure(solver, &step, 8) == -1);
  assert(tactix_online_complete(solver));
  assert(tactix_online_step_consumed(solver, 41) == 1);
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_STRENGTHEN;
  step.rule_budget_mode = 99;
  assert(configure(solver, &step, 81) == -1);
  assert(tactix_online_complete(solver));
  assert(tactix_online_step_consumed(solver, 41) == 1);
  step.rule_budget_mode = TACTIX_ONLINE_RULE_BUDGET_UNLIMITED;
  step.rule_budget = 1;
  assert(configure(solver, &step, 82) == -1);
  assert(tactix_online_complete(solver));
  step.rule_budget = 0;
  step.touch_filter_mode = 99;
  assert(configure(solver, &step, 83) == -1);
  assert(tactix_online_complete(solver));
  step.touch_filter_mode = TACTIX_ONLINE_SETTING_INHERIT;
  step.post_application = 99;
  assert(configure(solver, &step, 84) == -1);
  assert(tactix_online_complete(solver));
  step.post_application = TACTIX_ONLINE_POST_NONE;

  /* A complete pass may be configured after mutation and reaches fixpoint;
   * the step-level fixpoint is observable before the sequence closes. The
   * size-scaled fixpoint behaves the same with a derived ceiling. */
  step.granularity = TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS;
  step.instances = NULL;
  step.instance_count = 0;
  step.budget = 4;
  const int modes[] = {TACTIX_ONLINE_REPEAT_FIXPOINT,
                       TACTIX_ONLINE_REPEAT_SIZE_SCALED_FIXPOINT};
  const uint64_t limits[] = {0, 128}, revisions[] = {9, 11};
  for (int i = 0; i < 2; ++i) {
    step.repeat_mode = modes[i];
    step.repeat_limit = limits[i];
    assert(configure(solver, &step, revisions[i]) == 0);
    assert(tactix_online_advance(solver, &result) == 0);
    assert(result.status == TACTIX_ONLINE_SCOPED_FIXPOINT);
    assert(result.configuration_revision == revisions[i] &&
           result.applications == 0);
    assert(!tactix_online_complete(solver));
    assert(tactix_online_advance(solver, &result) == 0);
    assert(result.status == TACTIX_ONLINE_COMPLETED);
    assert(tactix_online_complete(solver));
  }
  step.repeat_mode = 99;
  assert(configure(solver, &step, 12) == -1);
  step.repeat_mode = TACTIX_ONLINE_REPEAT_FIXPOINT;
  step.repeat_limit = 0;

  /* The public converter validates typed per-call parameters. */
  step.rule_id = rule("Vivify");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_VIVIFY;
  step.parameters.value.vivify.scope = 2;
  step.parameters.value.vivify.reuse = 1;
  step.parameters.value.vivify.flush = 0;
  step.parameters.value.vivify.irredundant_sort_ratio = 0;
  assert(configure(solver, &step, 10) == -1);
  assert(tactix_online_complete(solver));
  step.parameters.value.vivify.irredundant_sort_ratio = 3;
  assert(configure(solver, &step, 10) == 0);
  tactix_release(solver);
}

int main(void) {
  independent_online_continuations();
  model_instances_are_public_and_validated();
  rejected_configuration_keeps_input_open();
  condition_cube_v3_is_atomic_and_copied();
  late_proof_root_refutation_is_terminal();
  canonical_bve_reaches_a_real_fixpoint();
  bve_otf_strengthening_is_public_and_observable();
  status_reconstructs_bve_models_and_refreshes_after_restart();
  affine_model_parameters_require_instance_eight();
  lucky_parameters_are_public_and_validated();
  pure_bve_scope_is_public_and_validated();
  exact_factor_grid_is_copied_and_applied();
  complete_factor_grid_is_discovered_and_applied();
  exact_sr_instance_v2_is_copied_and_applied();
  general_sr_producer_is_available_through_v1();
  invalid_graph_sr_settings_are_transactional();
  walk_scoring_is_validated_and_copied();
  strengthen_program_revisions();
  puts("C online configuration, advance, revision and transaction PASS");
  return 0;
}
