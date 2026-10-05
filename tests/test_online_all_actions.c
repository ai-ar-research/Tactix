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

static void exact_cover_fixture(tactix *solver) {
  int block, first, second, position, diagonal;
  for (block = 0; block < 3; ++block)
    for (first = 0; first < 3; ++first)
      for (second = first + 1; second < 3; ++second)
        add_clause(solver, (const int[]){-(3 * block + first + 1),
                                         -(3 * block + second + 1), 0});
  for (position = 0; position < 3; ++position) {
    int clause[] = {position + 1, position + 4, position + 7, 0};
    add_clause(solver, clause);
  }
  for (diagonal = 0; diagonal < 3; ++diagonal) {
    int clause[4];
    for (block = 0; block < 3; ++block)
      clause[block] = 3 * block + (block + diagonal) % 3 + 1;
    clause[3] = 0;
    add_clause(solver, clause);
  }
}

static void independent_set_fixture(tactix *solver) {
  static const int clauses[][4] = {
      {-1, -2, 0, 0}, {1, 5, 0, 0}, {-1, 6, 0, 0},
      {-5, 2, 7, 0}, {-5, -2, 8, 0}, {-6, 2, 8, 0}, {-6, -2, 9, 0},
      {-7, -8, 0, 0}, {-7, -9, 0, 0}, {-8, -9, 0, 0},
      {-7, 3, 10, 0}, {-7, -3, 11, 0}, {-8, 3, 11, 0},
      {-8, -3, 12, 0}, {-9, 3, 12, 0}, {-9, -3, 12, 0},
      {-10, -11, 0, 0}, {-10, -12, 0, 0}, {-11, -12, 0, 0},
      {-10, 4, 13, 0}, {-10, -4, 14, 0}, {-11, 4, 14, 0},
      {-11, -4, 15, 0}, {-12, 4, 15, 0}, {-12, -4, 15, 0},
      {-13, -14, 0, 0}, {-13, -15, 0, 0}, {-14, -15, 0, 0},
      {-13, 15, 0, 0}, {-14, 15, 0, 0}, {13, 14, 15, 0}};
  size_t i;
  for (i = 0; i < sizeof clauses / sizeof clauses[0]; ++i)
    add_clause(solver, clauses[i]);
}

static FILE *structural_records;
static void configure_parameters(const char *, tactix_online_parameters *);
static tactix *prepare(int id) {
  const char *name = tactix_rule_name(id);
  tactix *solver = tactix_init();
  if (!strcmp(name, "SemanticVariableAdvice"))
    exact_cover_fixture(solver);
  else
    fixture(solver);
  if (!strcmp(name, "ParityConsequence") ||
      !strcmp(name, "AffineConsequence") ||
      !strcmp(name, "ExactCoverExtension") ||
      !strcmp(name, "IndependentSetCover") ||
      !strcmp(name, "SemanticVariableAdvice")) {
    structural_records = tmpfile(); assert(structural_records);
    assert(tactix_bind_structural_provenance(solver, structural_records) == 0);
  }
  if (!strcmp(name, "SemanticVariableAdvice")) {
    tactix_online_step producer;
    tactix_online_program program;
    tactix_online_result result;
    memset(&producer, 0, sizeof producer);
    producer.step_id = 1000;
    producer.rule_id = rule_id("ExactCoverExtension");
    configure_parameters("ExactCoverExtension", &producer.parameters);
    producer.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
    producer.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
    producer.repeat_limit = 1;
    producer.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
    producer.budget_unlimited = 1;
    program = (tactix_online_program){
        &producer, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
    assert(tactix_online_configure(solver, &program, 1000,
                                   TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
    assert(tactix_online_advance(solver, &result) == 0);
    assert(result.status == TACTIX_ONLINE_COMPLETED &&
           result.made_rule_progress);
  }
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
  } else if (!strcmp(name, "Lucky")) {
    /* The generic smoke selects probe ID 0, whose exact payload is inherit. */
    parameters->kind = TACTIX_ONLINE_PARAMETERS_INHERIT;
  } else if (!strcmp(name, "AffineConsequence")) {
    parameters->kind = TACTIX_ONLINE_PARAMETERS_AFFINE_CONSEQUENCE;
    parameters->value.affine_consequence.maximum_clauses = 1000;
    parameters->value.affine_consequence.maximum_snapshot_literals = 10000;
    parameters->value.affine_consequence.maximum_snapshot_work = 10000;
    parameters->value.affine_consequence.maximum_groups = 1000;
    parameters->value.affine_consequence.maximum_patterns = 10000;
    parameters->value.affine_consequence.maximum_elimination_work = 10000;
    parameters->value.affine_consequence.maximum_arity = 4;
    parameters->value.affine_consequence.maximum_intermediate_width = 8;
    parameters->value.affine_consequence.maximum_sources = 32;
    parameters->value.affine_consequence.maximum_output_width = 4;
    parameters->value.affine_consequence.maximum_proof_additions = 10000;
    parameters->value.affine_consequence.maximum_proof_deletions = 10000;
    parameters->value.affine_consequence.maximum_journal_bytes = 100000;
    parameters->value.affine_consequence.semantics =
        TACTIX_ONLINE_AFFINE_CONSEQUENCE_CERTIFIED_BASIS_V2;
    parameters->value.affine_consequence.basis_maximum_work = 100000;
    parameters->value.affine_consequence.basis_maximum_intermediate_width = 16;
    parameters->value.affine_consequence.basis_maximum_sources = 64;
    parameters->value.affine_consequence.basis_maximum_premises = 1024;
    parameters->value.affine_consequence.basis_maximum_nodes = 1024;
    parameters->value.affine_consequence.basis_maximum_direct_width = 4;
    parameters->value.affine_consequence.maximum_external_extent = 10000;
    parameters->value.affine_consequence.maximum_session_keys = 10000;
    parameters->value.affine_consequence.maximum_session_work = 100000;
  } else if (!strcmp(name, "ExactCoverExtension")) {
    parameters->kind = TACTIX_ONLINE_PARAMETERS_EXACT_COVER_EXTENSION;
    parameters->value.exact_cover_extension.arity = 3;
    parameters->value.exact_cover_extension.maximum_items = 100;
    parameters->value.exact_cover_extension.maximum_source_clauses = 100;
    parameters->value.exact_cover_extension.maximum_source_literals = 1000;
    parameters->value.exact_cover_extension.maximum_recognition_work = 100000;
    parameters->value.exact_cover_extension.maximum_premise_references = 1000;
    parameters->value.exact_cover_extension.maximum_definitions = 10;
    parameters->value.exact_cover_extension.maximum_additions = 100;
    parameters->value.exact_cover_extension.maximum_literal_occurrences = 1000;
    parameters->value.exact_cover_extension.maximum_extent_growth = 10;
    parameters->value.exact_cover_extension.maximum_external_extent = 1000;
    parameters->value.exact_cover_extension.maximum_journal_bytes = 100000;
  } else if (!strcmp(name, "IndependentSetCover")) {
    parameters->kind = TACTIX_ONLINE_PARAMETERS_INDEPENDENT_SET_COVER;
    parameters->value.independent_set_cover.maximum_clauses = 10000;
    parameters->value.independent_set_cover.maximum_literals = 100000;
    parameters->value.independent_set_cover.maximum_recognition_work = 1000000;
    parameters->value.independent_set_cover.maximum_proof_work = 1000000;
    parameters->value.independent_set_cover.maximum_proof_additions = 100000;
    parameters->value.independent_set_cover.maximum_proof_deletions = 100000;
    parameters->value.independent_set_cover.maximum_proof_literal_occurrences = 1000000;
    parameters->value.independent_set_cover.maximum_journal_bytes = 1000000;
    parameters->value.independent_set_cover.maximum_vertices = 64;
    parameters->value.independent_set_cover.maximum_target = 64;
  } else if (!strcmp(name, "SemanticVariableAdvice")) {
    parameters->kind = TACTIX_ONLINE_PARAMETERS_SEMANTIC_VARIABLE_ADVICE;
    parameters->value.semantic_variable_advice.maximum_variables = 64;
    parameters->value.semantic_variable_advice.maximum_priority_sum = 1024;
    parameters->value.semantic_variable_advice.existing_theory_priority = 1;
    parameters->value.semantic_variable_advice.introduced_theory_priority = 4;
  } else if (!strcmp(name, "Gf3AffineConsequence")) {
    parameters->kind = TACTIX_ONLINE_PARAMETERS_GF3_AFFINE_CONSEQUENCE;
    parameters->value.gf3_affine_consequence =
        (tactix_online_gf3_affine_consequence_parameters){
            1000, 10000, 100, 100, 8, 10000, 8, 10000, 10000, 100000,
            100000, 100000, 100000, 100000, 100000, 10000, 100000,
            100000, 1000000, 10000, 10000, 100000};
  } else if (!strcmp(name, "DirectCspSupport")) {
    parameters->kind = TACTIX_ONLINE_PARAMETERS_DIRECT_CSP_SUPPORT;
    parameters->value.direct_csp_support =
        (tactix_online_direct_csp_support_parameters){
            1000, 10000, 100000, 32, 1000, 1000, 100000, 10000,
            100000, 1000000, 0, 10000, 10000, 100000, 0};
  } else if (!strcmp(name, "DirectPhpRefutation")) {
    parameters->kind = TACTIX_ONLINE_PARAMETERS_DIRECT_PHP_REFUTATION;
    parameters->value.direct_php_refutation =
        (tactix_online_direct_php_refutation_parameters){
            100000, 1000000, 10000000, 100000, 1000000, 100000000,
            10000, 1000000, 1000000, 20, 1000000, 1000000, 100000000,
            1000000, 2000000, 100000000};
  }
}

static void reaches_every_registered_action(void) {
  const int count = tactix_rule_count();
  int id;
  assert(count == 40);
  for (id = 0; id < count; ++id) {
    tactix *solver = prepare(id);
    const char *name = tactix_rule_name(id);
    if (!strcmp(name, "Gf3AffineConsequence") ||
        !strcmp(name, "DirectCspSupport") ||
        !strcmp(name, "NaryCspPrefix") ||
        !strcmp(name, "Gf3GaussianConsequence") ||
        !strcmp(name, "TseitinCycleBreaking")) {
      tactix_release(solver);
      continue;
    }
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
    if (!strcmp(name, "Condition")) {
      const int clause[] = {24, 25, 26};
      const int witness[] = {24};
      const tactix_online_condition_instance cube = {
          clause, 3, witness, 1};
      tactix_online_step_v3 exact;
      memset(&exact, 0, sizeof exact);
      exact.base.base = step;
      exact.base.base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
      exact.condition_instances = &cube;
      exact.condition_instance_count = 1;
      const tactix_online_program_v3 typed = {
          &exact, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
      tactix_online_result result;
      assert(tactix_online_configure_v3(solver, &typed, 19,
                                        TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
      assert(tactix_online_advance(solver, &result) == 0);
      assert(result.rule_id == (uint32_t)id && result.applications == 1);
      assert(result.made_rule_progress && result.instance == 0);
      assert(result.after_strategy_epoch > result.before_strategy_epoch);
      printf("  %-18s STRUCTURED\n", name);
      tactix_release(solver);
    if (structural_records) { fclose(structural_records); structural_records = NULL; }
      continue;
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
    if (structural_records) { fclose(structural_records); structural_records = NULL; }
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
  step.parameters.value.reduce.reduction_bookkeeping_mode = 99;
  assert(tactix_online_configure(solver, &program, 24,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  for (int mode = TACTIX_ONLINE_SETTING_INHERIT;
       mode <= TACTIX_ONLINE_SETTING_ENABLE; ++mode) {
    step.parameters.value.reduce.reduction_bookkeeping_mode = mode;
    assert(tactix_online_configure(solver, &program, 25 + mode,
                                   TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
    tactix_online_result result;
    assert(tactix_online_advance(solver, &result) == 0);
    assert(result.applications == 1);
  }
  tactix_release(solver);
}

static void invalid_parity_parameters_are_transactional(void) {
  tactix *solver = tactix_init();
  fixture(solver);
  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = 89;
  step.rule_id = rule_id("Lucky");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_PARITY_MODEL;
  step.parameters.value.parity_model.maximum_variables = 64;
  step.parameters.value.parity_model.maximum_groups = 0;
  step.parameters.value.parity_model.maximum_constraints = 256;
  step.parameters.value.parity_model.minimum_group_clauses = 3;
  step.parameters.value.parity_model.minimum_equations = 1;
  step.parameters.value.parity_model.maximum_free_variables = 4;
  step.parameters.value.parity_model.candidate_budget = 16;
  const uint64_t parity_instance = 6;
  step.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  step.instances = &parity_instance;
  step.instance_count = 1;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  tactix_online_program program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(solver, &program, 24,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.parameters.value.parity_model.maximum_groups = 64;
  assert(tactix_online_configure(solver, &program, 25,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  tactix_release(solver);
}

static void independent_set_v4_batch_retains_its_owner(void) {
  tactix *solver = tactix_init();
  FILE *records = tmpfile();
  const uint32_t first_vertices[] = {1, 2, 3};
  const uint32_t second_vertices[] = {1, 2, 4};
  const tactix_online_independent_set_cover_instance covers[] = {
      {first_vertices, 3}, {second_vertices, 3}};
  tactix_online_step_v4 step;
  tactix_online_program_v4 program;
  tactix_online_result result;
  assert(records);
  independent_set_fixture(solver);
  assert(tactix_bind_structural_provenance(solver, records) == 0);
  memset(&step, 0, sizeof step);
  step.base.base.base.step_id = 2000;
  step.base.base.base.rule_id = rule_id("IndependentSetCover");
  configure_parameters("IndependentSetCover",
                       &step.base.base.base.parameters);
  step.base.base.base.granularity = TACTIX_ONLINE_GRANULARITY_BATCH;
  step.base.base.base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.base.base.base.repeat_limit = 1;
  step.base.base.base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.independent_set_cover_instances = covers;
  step.independent_set_cover_instance_count = 2;
  program = (tactix_online_program_v4){
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};

  /* Composition budget zero declines before structural work. */
  assert(tactix_online_configure_v4(solver, &program, 1998,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_BUDGET_EXHAUSTED &&
         result.applications == 0 && !result.selection_complete);

  /* A rule-local recognition limit is reported distinctly and retains the
   * selected cover so a later explicit reconfiguration can retry it. */
  step.base.base.base.budget_unlimited = 1;
  step.base.base.base.parameters.value.independent_set_cover
      .maximum_recognition_work = 2;
  assert(tactix_online_configure_v4(solver, &program, 1999,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_RESOURCE_EXHAUSTED &&
         result.applications == 1 && result.work == 1 &&
         !result.selection_complete);

  step.base.base.base.parameters.value.independent_set_cover
      .maximum_recognition_work = 1000000;
  assert(tactix_online_configure_v4(solver, &program, 2000,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED &&
         result.made_rule_progress && result.applications == 1 &&
         result.instance == 0 && !result.selection_complete);
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED &&
         result.made_rule_progress && result.applications == 1 &&
         result.instance == 1 && result.selection_complete);

  /* The nested storage was copied. Malformed new storage is rejected without
   * replacing the already completed configuration. */
  {
    const uint32_t unsorted[] = {2, 1, 3};
    const tactix_online_independent_set_cover_instance malformed = {
        unsorted, 3};
    step.independent_set_cover_instances = &malformed;
    step.independent_set_cover_instance_count = 1;
    assert(tactix_online_configure_v4(solver, &program, 2001,
                                      TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  }
  tactix_release(solver);
  fclose(records);
}

static void theory_action_v5_payloads_are_deep_copied(void) {
  {
    tactix *solver = tactix_init();
    FILE *records = tmpfile();
    tactix_online_gf3_affine_instance pair = {0, 1};
    tactix_online_step_v5 step;
    tactix_online_program_v5 program;
    tactix_online_result result;
    assert(records && tactix_bind_structural_provenance(solver, records) == 0);
    memset(&step, 0, sizeof step);
    step.base.base.base.base.step_id = 3000;
    step.base.base.base.base.rule_id = rule_id("Gf3AffineConsequence");
    configure_parameters("Gf3AffineConsequence",
                         &step.base.base.base.base.parameters);
    step.base.base.base.base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
    step.base.base.base.base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
    step.base.base.base.base.repeat_limit = 1;
    step.base.base.base.base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
    step.base.base.base.base.budget_unlimited = 1;
    step.gf3_affine_instances = &pair;
    step.gf3_affine_instance_count = 1;
    program = (tactix_online_program_v5){
        &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
    assert(tactix_online_configure_v5(solver, &program, 3000,
                                      TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
    pair.first_equation = 2;
    pair.second_equation = 2;
    assert(tactix_online_advance(solver, &result) == 0);
    assert(result.status != TACTIX_ONLINE_INVALID_REQUEST);
    tactix_release(solver);
    fclose(records);
  }
  {
    tactix *solver = tactix_init();
    FILE *records = tmpfile();
    int left[] = {1, -2}, right[] = {3, -4};
    tactix_online_direct_csp_support_instance support = {
        left, 2, right, 2, 0};
    tactix_online_step_v5 step;
    tactix_online_program_v5 program;
    tactix_online_result result;
    assert(records && tactix_bind_structural_provenance(solver, records) == 0);
    memset(&step, 0, sizeof step);
    step.base.base.base.base.step_id = 3001;
    step.base.base.base.base.rule_id = rule_id("DirectCspSupport");
    configure_parameters("DirectCspSupport",
                         &step.base.base.base.base.parameters);
    step.base.base.base.base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
    step.base.base.base.base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
    step.base.base.base.base.repeat_limit = 1;
    step.base.base.base.base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
    step.base.base.base.base.budget_unlimited = 1;
    step.direct_csp_support_instances = &support;
    step.direct_csp_support_instance_count = 1;
    program = (tactix_online_program_v5){
        &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
    assert(tactix_online_configure_v5(solver, &program, 3001,
                                      TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
    left[0] = right[0] = 0;
    assert(tactix_online_advance(solver, &result) == 0);
    assert(result.status != TACTIX_ONLINE_INVALID_REQUEST);
    tactix_release(solver);
    fclose(records);
  }
}

static void invalid_theory_v5_inputs_are_transactional(void) {
  tactix *solver = tactix_init();
  FILE *records = tmpfile();
  tactix_online_gf3_affine_instance pair = {0, 1};
  tactix_online_step_v5 step;
  tactix_online_program_v5 program;
  tactix_online_result result;
  uint64_t scalar = 7;
  assert(records && tactix_bind_structural_provenance(solver, records) == 0);
  memset(&step, 0, sizeof step);
  step.base.base.base.base.step_id = 3100;
  step.base.base.base.base.rule_id = rule_id("Gf3AffineConsequence");
  configure_parameters("Gf3AffineConsequence",
                       &step.base.base.base.base.parameters);
  step.base.base.base.base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  step.base.base.base.base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.base.base.base.base.repeat_limit = 1;
  step.base.base.base.base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.base.base.base.base.budget_unlimited = 1;
  step.gf3_affine_instances = &pair;
  step.gf3_affine_instance_count = 1;
  program = (tactix_online_program_v5){
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure_v5(solver, &program, 3100,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == 0);

  program.step_count = -1;
  assert(tactix_online_configure_v5(solver, &program, 3101,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  program.step_count = 1;
  step.gf3_affine_instance_count = -1;
  assert(tactix_online_configure_v5(solver, &program, 3102,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.gf3_affine_instance_count = 1;
  step.gf3_affine_instances = NULL;
  assert(tactix_online_configure_v5(solver, &program, 3103,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.gf3_affine_instances = &pair;
  step.direct_csp_support_instance_count = 1;
  assert(tactix_online_configure_v5(solver, &program, 3104,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.direct_csp_support_instance_count = 0;
  pair.second_equation = 0;
  assert(tactix_online_configure_v5(solver, &program, 3105,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  pair.second_equation = 1;
  step.base.base.base.base.instances = &scalar;
  step.base.base.base.base.instance_count = 1;
  assert(tactix_online_configure_v5(solver, &program, 3106,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.base.base.base.base.instances = NULL;
  step.base.base.base.base.instance_count = 0;

  {
    int left[] = {1, -2}, right[] = {3, -4};
    tactix_online_direct_csp_support_instance support = {
        left, 2, right, 2, 0};
    step.base.base.base.base.rule_id = rule_id("DirectCspSupport");
    configure_parameters("DirectCspSupport",
                         &step.base.base.base.base.parameters);
    step.gf3_affine_instances = NULL;
    step.gf3_affine_instance_count = 0;
    step.direct_csp_support_instances = &support;
    step.direct_csp_support_instance_count = 1;
    step.base.base.base.base.parameters.value.direct_csp_support.allow_extension = 2;
    assert(tactix_online_configure_v5(solver, &program, 3107,
                                      TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
    step.base.base.base.base.parameters.value.direct_csp_support.allow_extension = 0;
    support.left_value_count = -1;
    assert(tactix_online_configure_v5(solver, &program, 3108,
                                      TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
    support.left_value_count = 2;
    support.left_values = NULL;
    assert(tactix_online_configure_v5(solver, &program, 3109,
                                      TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  }

  /* Every rejected replacement above leaves the original revision executable. */
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.configuration_revision == 3100 && result.step_id == 3100 &&
         result.status != TACTIX_ONLINE_INVALID_REQUEST);
  tactix_release(solver);
  fclose(records);
}

static void lucky_sat_constructors_are_public(void) {
  tactix *solver = tactix_init();
  /* Forbid the four even assignments on variables 1,2,3, then select one
   * affine witness with a binary clause. */
  add_clause(solver, (const int[]){1, 2, 3, 0});
  add_clause(solver, (const int[]){-1, -2, 3, 0});
  add_clause(solver, (const int[]){-1, 2, -3, 0});
  add_clause(solver, (const int[]){1, -2, -3, 0});
  add_clause(solver, (const int[]){-1, -2, 0});

  const int lucky = rule_id("Lucky");
  uint64_t instances[NINST];
  const int count =
      tactix_rule_semantic_instances(solver, lucky, instances, NINST);
  assert(count == 26);
  for (int i = 0; i < count; ++i)
    assert(instances[i] == (uint64_t)i);
  for (uint64_t restored = 11; restored <= 14; ++restored)
    assert(instances[restored] == restored);
  assert(instances[17] == 17 && instances[18] == 18 && instances[21] == 21 &&
         instances[25] == 25);

  uint64_t selected = 99;
  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = 90;
  step.rule_id = lucky;
  configure_parameters("Lucky", &step.parameters);
  step.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  step.instances = &selected;
  step.instance_count = 1;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  tactix_online_program program = {
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(solver, &program, 26,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
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
  invalid_parity_parameters_are_transactional();
  independent_set_v4_batch_retains_its_owner();
  theory_action_v5_payloads_are_deep_copied();
  invalid_theory_v5_inputs_are_transactional();
  lucky_sat_constructors_are_public();
  aba_transition_invalidates_a_pending_batch();
  puts("test_online_all_actions: all registered actions and ABA invalidation PASS");
  return 0;
}
