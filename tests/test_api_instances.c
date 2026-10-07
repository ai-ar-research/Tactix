/* Arm-space coverage: every registry entry is exercised through its declared
 * public instance surface. Most use uint64 payloads; X5's variable-length
 * (clause,cube) tuple necessarily uses tactix_apply_condition. A rule without
 * a uint64 payload rejects an arbitrary payload (-1). */
#undef NDEBUG
#include "tactix.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_RULES 64
#define NINST 64

/* Entries with no uint64 payload surface: a numeric payload is invalid.
 * Condition instead has a structured public tuple surface below. */
static const char *const NO_SURFACE[] = {"Propagate", "SubstitutionRedundancy",
                                         "SwitchMode", "Condition", "ParityConsequence",
                                         "AffineConsequence", "ExactCoverExtension",
                                         "IndependentSetCover",
                                         "Gf3AffineConsequence",
                                         "DirectCspSupport",
                                         "NaryCspPrefix",
                                         "NaryCspEquality",
                                         "NaryCspEqualityAmo", "NaryCspEqualityTransitivity",
                                         "DirectPhpRefutation",
                                         "Gf3GaussianConsequence",
                                         "Gf3GeneralRowConsequence",
                                         "TseitinCycleBreaking",
                                         "AffineMatrixInitialize"};

/* Entries whose enumeration is exactly the canonical sentinel: an
 * `apply_instance` with nothing to choose between. Joining or leaving this set
 * is a deliberate act. Congruence is here by specification, not by omission:
 * V6's effect is the congruence closure, so a single merge is a prefix of one
 * application rather than an application (`N1-CONG-INSTANCES`, disposed in
 * docs/calculus.md §6 V6 and pinned by test_congruence.cpp). Factor left this
 * set when its seed-literal surface landed (`N1-FACTOR-INSTANCES`). */
static const char *const CANONICAL_ONLY[] = {"Congruence", "Propagate",
                                             "SubstitutionRedundancy",
                                             "SwitchMode", "Walk", "ParityConsequence",
                                             "AffineConsequence", "ExactCoverExtension",
                                             "IndependentSetCover",
                                             "Gf3AffineConsequence",
                                             "DirectCspSupport",
                                             "NaryCspPrefix",
                                             "NaryCspEquality",
                                             "NaryCspEqualityAmo", "NaryCspEqualityTransitivity",
                                             "DirectPhpRefutation",
                                             "Gf3GaussianConsequence",
                                             "Gf3GeneralRowConsequence",
                                             "TseitinCycleBreaking",
                                             "GraphIntervalEquivalence",
                                             "AffineMatrixInitialize"};

static int in_list(const char *const *list, size_t n, const char *name) {
  size_t i;
  for (i = 0; i < n; i++)
    if (!strcmp(list[i], name))
      return 1;
  return 0;
}

#define NO_SURFACE_P(nm) \
  in_list(NO_SURFACE, sizeof NO_SURFACE / sizeof NO_SURFACE[0], (nm))
#define CANONICAL_ONLY_P(nm) \
  in_list(CANONICAL_ONLY, sizeof CANONICAL_ONLY / sizeof CANONICAL_ONLY[0], (nm))

static int rule_id(const char *name) {
  int i;
  for (i = 0; i < tactix_rule_count(); i++)
    if (!strcmp(tactix_rule_name(i), name))
      return i;
  return -1;
}

static void add_clause(tactix *s, const int *lits) {
  for (; *lits; lits++)
    tactix_add(s, *lits);
  tactix_add(s, 0);
}

static int compare_ints(const void *left, const void *right) {
  const int a = *(const int *)left, b = *(const int *)right;
  return (a > b) - (a < b);
}

static void affine_v9_exact_row_boundary(void) {
  tactix *s = tactix_init();
  FILE *records = tmpfile();
  int clauses[32][5], premise = 0, mask, bit;
  tactix_online_affine_source_identity first_sources[16], second_sources[16];
  tactix_online_affine_equation_identity equations[2];
  const int first_variables[] = {1, 2, 3, 4, 5};
  const int second_variables[] = {1, 2, 3, 4, 6};
  const int row_variables[] = {5, 6};
  const uint64_t ordinals[] = {0, 1};
  tactix_online_affine_basis_instance instance;
  tactix_online_step_v9 step;
  tactix_online_program_v9 program;
  tactix_online_result result;
  tactix_online_step *base;
  assert(records && tactix_bind_structural_provenance(s, records) == 0);
  for (int equation = 0; equation < 2; ++equation) {
    const int *variables = equation ? second_variables : first_variables;
    const int rhs = equation != 0;
    for (mask = 0; mask < 32; ++mask) {
      unsigned parity = 0;
      for (bit = 0; bit < 5; ++bit) parity ^= (unsigned)(mask >> bit) & 1u;
      if (parity == (unsigned)rhs) continue;
      for (bit = 0; bit < 5; ++bit)
        clauses[premise][bit] =
            (mask & (1 << bit)) ? -variables[bit] : variables[bit];
      add_clause(s, (const int[]){clauses[premise][0], clauses[premise][1],
                                  clauses[premise][2], clauses[premise][3],
                                  clauses[premise][4], 0});
      qsort(clauses[premise], 5, sizeof(int), compare_ints);
      tactix_online_affine_source_identity *source =
          equation ? &second_sources[premise - 16] : &first_sources[premise];
      source->native_clause_id = (uint64_t)premise + 1;
      source->clause = clauses[premise];
      source->clause_count = 5;
      ++premise;
    }
  }
  assert(premise == 32);
  equations[0] = (tactix_online_affine_equation_identity){
      first_variables, 5, 0, first_sources, 16};
  equations[1] = (tactix_online_affine_equation_identity){
      second_variables, 5, 1, second_sources, 16};
  instance = (tactix_online_affine_basis_instance){
      1, 1, TACTIX_ONLINE_AFFINE_ROW_BINARY, row_variables, 2, 1,
      ordinals, 2, equations, 2};
  memset(&step, 0, sizeof step);
  base = &step.base.base.base.base.base.base.base.base;
  base->step_id = 9001;
  base->rule_id = rule_id("AffineConsequence");
  base->parameters.kind = TACTIX_ONLINE_PARAMETERS_AFFINE_CONSEQUENCE;
  base->parameters.value.affine_consequence =
      (tactix_online_affine_consequence_parameters){
          1000, 10000, 100000, 100, 1000, 1000000, 8, 32, 32, 4,
          100000, 100000, 1000000, 0,
          TACTIX_ONLINE_AFFINE_CONSEQUENCE_CERTIFIED_BASIS_V2,
          10000000, 32, 32, 10000, 10000, 3, 100000, 10000, 1000000};
  base->granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  base->repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  base->repeat_limit = 1;
  base->work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  base->budget_unlimited = 1;
  step.affine_basis_instances = &instance;
  step.affine_basis_instance_count = 1;
  program = (tactix_online_program_v9){
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure_v9(
             s, &program, 9001, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  /* Configuration owns every nested array after this point. */
  clauses[0][0] = 999;
  assert(tactix_online_advance(s, &result) == 0 &&
         result.status == TACTIX_ONLINE_COMPLETED &&
         result.made_rule_progress);
  instance.owner_version = 0;
  assert(tactix_online_configure_v9(
             s, &program, 9002, TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  tactix_release(s);
  fclose(records);
}

/* A level-0 formula with material for every inprocessing family, satisfiable
 * so no rule can refute it out from under the walk. */
static void fixture(tactix *s) {
  /* the same AND gate twice, so a congruence has something to merge */
  add_clause(s, (const int[]){-10, 1, 0});
  add_clause(s, (const int[]){-10, 2, 0});
  add_clause(s, (const int[]){10, -1, -2, 0});
  add_clause(s, (const int[]){-11, 1, 0});
  add_clause(s, (const int[]){-11, 2, 0});
  add_clause(s, (const int[]){11, -1, -2, 0});
  /* implication chain 5 -> 6 -> 7 with the transitive shortcut 5 -> 7 */
  add_clause(s, (const int[]){-5, 6, 0});
  add_clause(s, (const int[]){-6, 7, 0});
  add_clause(s, (const int[]){-5, 7, 0});
  /* 2x3 grid over factors {12,13} and remainders {14,15,16} */
  add_clause(s, (const int[]){12, 14, 0});
  add_clause(s, (const int[]){12, 15, 0});
  add_clause(s, (const int[]){12, 16, 0});
  add_clause(s, (const int[]){13, 14, 0});
  add_clause(s, (const int[]){13, 15, 0});
  add_clause(s, (const int[]){13, 16, 0});
  /* (17 18 19) subsumes; (21 22 23) and (21 22 -23) strengthen to (21 22) */
  add_clause(s, (const int[]){17, 18, 19, 20, 0});
  add_clause(s, (const int[]){17, 18, 19, 0});
  add_clause(s, (const int[]){21, 22, 23, 0});
  add_clause(s, (const int[]){21, 22, -23, 0});
  /* 24 occurs positively only: every clause holding it is blocked */
  add_clause(s, (const int[]){24, 25, 26, 0});
  add_clause(s, (const int[]){24, -25, 0});
}

static void exact_cover_fixture(tactix *s) {
  int block, first, second, position, diagonal;
  for (block = 0; block < 3; ++block)
    for (first = 0; first < 3; ++first)
      for (second = first + 1; second < 3; ++second)
        add_clause(s, (const int[]){-(3 * block + first + 1),
                                    -(3 * block + second + 1), 0});
  for (position = 0; position < 3; ++position)
    add_clause(s, (const int[]){position + 1, position + 4,
                                position + 7, 0});
  for (diagonal = 0; diagonal < 3; ++diagonal) {
    int clause[4];
    for (block = 0; block < 3; ++block)
      clause[block] = 3 * block + (block + diagonal) % 3 + 1;
    clause[3] = 0;
    add_clause(s, clause);
  }
}

/* A fresh solver where `id`'s guard holds; search-state entries need more
 * than a quiescent root. */
static FILE *structural_records;
static tactix *prepare(int id) {
  const char *name = tactix_rule_name(id);
  tactix *s = tactix_init();
  if (!strcmp(name, "SemanticVariableAdvice"))
    exact_cover_fixture(s);
  else
    fixture(s);
  if (!strcmp(name, "ParityConsequence") ||
      (!strcmp(name, "AffineConsequence") ||
       !strcmp(name, "AffineMatrixInitialize")) ||
      !strcmp(name, "ExactCoverExtension") ||
      !strcmp(name, "IndependentSetCover") ||
      !strcmp(name, "Gf3AffineConsequence") ||
      !strcmp(name, "DirectCspSupport") ||
      !strcmp(name, "NaryCspPrefix") ||
      !strcmp(name, "NaryCspEquality") ||
      !strcmp(name, "Gf3GaussianConsequence") ||
      !strcmp(name, "Gf3GeneralRowConsequence") ||
      !strcmp(name, "TseitinCycleBreaking") ||
      !strcmp(name, "GraphIntervalEquivalence") ||
      !strcmp(name, "SemanticVariableAdvice")) {
    structural_records = tmpfile(); assert(structural_records);
    assert(tactix_bind_structural_provenance(s, structural_records) == 0);
  }
  if (!strcmp(name, "SemanticVariableAdvice")) {
    tactix_online_step producer;
    tactix_online_program program;
    tactix_online_result result;
    memset(&producer, 0, sizeof producer);
    producer.step_id = 1000;
    producer.rule_id = rule_id("ExactCoverExtension");
    producer.parameters.kind = TACTIX_ONLINE_PARAMETERS_EXACT_COVER_EXTENSION;
    producer.parameters.value.exact_cover_extension.arity = 3;
    producer.parameters.value.exact_cover_extension.maximum_items = 100;
    producer.parameters.value.exact_cover_extension.maximum_source_clauses = 100;
    producer.parameters.value.exact_cover_extension.maximum_source_literals = 1000;
    producer.parameters.value.exact_cover_extension.maximum_recognition_work = 100000;
    producer.parameters.value.exact_cover_extension.maximum_premise_references = 1000;
    producer.parameters.value.exact_cover_extension.maximum_definitions = 10;
    producer.parameters.value.exact_cover_extension.maximum_additions = 100;
    producer.parameters.value.exact_cover_extension.maximum_literal_occurrences = 1000;
    producer.parameters.value.exact_cover_extension.maximum_extent_growth = 10;
    producer.parameters.value.exact_cover_extension.maximum_external_extent = 1000;
    producer.parameters.value.exact_cover_extension.maximum_journal_bytes = 100000;
    producer.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
    producer.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
    producer.repeat_limit = 1;
    producer.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
    producer.budget_unlimited = 1;
    program = (tactix_online_program){
        &producer, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
    assert(tactix_online_configure(s, &program, 1000,
                                   TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
    assert(tactix_online_advance(s, &result) == 0);
    assert(result.status == TACTIX_ONLINE_COMPLETED &&
           result.made_rule_progress);
  }
  if (!strcmp(name, "Propagate")) {
    assert(tactix_decide(s, 5) == 0); /* a decision leaves the queue nonempty */
  } else if (!strcmp(name, "Restart")) {
    /* Exercise both noncanonical keep-levels; the root no-op is covered by
       the dedicated C API state-boundary test. */
    assert(tactix_decide(s, 5) == 0);
    assert(tactix_apply(s, rule_id("Propagate")) != -1);
  } else if (!strcmp(name, "ConflictAnalysis")) {
    /* three decisions falsify (17 18 19) outright, so propagation reports the
       conflict analyze_guard needs */
    assert(tactix_decide(s, -17) == 0);
    assert(tactix_decide(s, -18) == 0);
    assert(tactix_decide(s, -19) == 0);
    assert(tactix_apply(s, rule_id("Propagate")) == 2 &&
           "the fixture must reach a conflict for Analyze's guard");
  }
  return s;
}

/* Per-rule coverage, accumulated across the fresh solvers below. */
static int hit_canonical[MAX_RULES];
static int hit_payload[MAX_RULES];
static int hit_structured[MAX_RULES];
static int enum_empty[MAX_RULES];
static int progressed;

static void drive_theory_v5(tactix *s, int id, const char *name) {
  tactix_online_step_v5 step;
  tactix_online_program_v5 program;
  tactix_online_result result;
  tactix_online_gf3_affine_instance pair = {0, 1};
  int left[] = {1, -2}, right[] = {3, -4};
  tactix_online_direct_csp_support_instance support = {
      left, 2, right, 2, 0};
  uint64_t scalar;

  assert(tactix_rule_instances(s, id, &scalar, 1) == 1 &&
         scalar == TACTIX_INSTANCE_CANONICAL);
  assert(tactix_apply_instance(s, id, scalar) == 1 &&
         "the legacy canonical placeholder must not invent a typed request");

  memset(&step, 0, sizeof step);
  step.base.base.base.base.step_id = 2000 + (uint64_t)id;
  step.base.base.base.base.rule_id = id;
  step.base.base.base.base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  step.base.base.base.base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.base.base.base.base.repeat_limit = 1;
  step.base.base.base.base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.base.base.base.base.budget_unlimited = 1;
  if (!strcmp(name, "Gf3AffineConsequence")) {
    step.base.base.base.base.parameters.kind =
        TACTIX_ONLINE_PARAMETERS_GF3_AFFINE_CONSEQUENCE;
    step.base.base.base.base.parameters.value.gf3_affine_consequence =
        (tactix_online_gf3_affine_consequence_parameters){
            1000, 10000, 100, 100, 8, 10000, 8, 10000, 10000, 100000,
            100000, 100000, 100000, 100000, 100000, 10000, 100000,
            100000, 1000000, 10000, 10000, 100000};
    step.gf3_affine_instances = &pair;
    step.gf3_affine_instance_count = 1;
  } else {
    step.base.base.base.base.parameters.kind =
        TACTIX_ONLINE_PARAMETERS_DIRECT_CSP_SUPPORT;
    step.base.base.base.base.parameters.value.direct_csp_support =
        (tactix_online_direct_csp_support_parameters){
            1000, 10000, 100000, 32, 1000, 1000, 100000, 10000,
            100000, 1000000, 0, 10000, 10000, 100000, 0};
    step.direct_csp_support_instances = &support;
    step.direct_csp_support_instance_count = 1;
  }
  program = (tactix_online_program_v5){
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure_v5(s, &program, step.base.base.base.base.step_id,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  assert(tactix_online_advance(s, &result) == 0);
  assert(result.status != TACTIX_ONLINE_INVALID_REQUEST);
  hit_structured[id] = 1;
  progressed += result.made_rule_progress;
}

/* Apply the entry, preferring a payload its own enumeration offered. */
static void drive(tactix *s, int id) {
  uint64_t insts[NINST];
  const char *name = tactix_rule_name(id);
  if (!strcmp(name, "Gf3AffineConsequence") ||
      !strcmp(name, "DirectCspSupport")) {
    drive_theory_v5(s, id, name);
    return;
  }
  if (!strcmp(name, "Gf3GaussianConsequence")) {
    const int count = tactix_rule_instances(s, id, insts, NINST);
    assert(count == 1 && insts[0] == TACTIX_INSTANCE_CANONICAL);
    hit_structured[id] = 1;
    return;
  }
  if (!strcmp(name, "NaryCspPrefix")) {
    const int count = tactix_rule_instances(s, id, insts, NINST);
    assert(count == 1 && insts[0] == TACTIX_INSTANCE_CANONICAL);
    hit_structured[id] = 1;
    return;
  }
  if (!strcmp(name, "NaryCspEquality")) {
    const int count = tactix_rule_instances(s, id, insts, NINST);
    assert(count == 1 && insts[0] == TACTIX_INSTANCE_CANONICAL);
    hit_structured[id] = 1;
    return;
  }
  if (!strcmp(name, "NaryCspEqualityTransitivity")) {
    const int count = tactix_rule_instances(s, id, insts, NINST);
    assert(count == -1 &&
           "Transitivity guard requires the typed prefix/equality session pipeline");
    /* Exact C15 selections are exercised by the dedicated frontend test. */
    hit_structured[id] = 1;
    return;
  }
  if (!strcmp(name, "NaryCspEqualityAmo")) {
    const int count = tactix_rule_instances(s, id, insts, NINST);
    assert(count == -1 &&
           "AMO guard requires the typed prefix/equality session pipeline");
    hit_structured[id] = 1;
    return;
  }
  if (!strcmp(name, "TseitinCycleBreaking")) {
    const int count = tactix_rule_instances(s, id, insts, NINST);
    assert(count == 1 && insts[0] == TACTIX_INSTANCE_CANONICAL);
    hit_structured[id] = 1;
    return;
  }
  const int n = tactix_rule_instances(s, id, insts, NINST);
  assert(n >= 0 && "prepare() must reach a state where the guard holds");

  const int bare = (n == 1 && insts[0] == TACTIX_INSTANCE_CANONICAL);
  if (!strcmp(name, "Condition")) {
    const int clause[] = {24, 25, 26};
    const int witness[] = {24};
    assert(bare && "structured X5 has no uint64 tuple encoding");
    assert(tactix_apply_instance(s, id, TACTIX_INSTANCE_CANONICAL) == 1 &&
           "the X5 canonical placeholder must not guess a cube");
    assert(tactix_apply_condition(s, clause, 3, witness, 1) == 0 &&
           "the explicit X5 tuple must reach an effect");
    hit_structured[id] = 1;
    progressed++;
    return;
  }
  assert(bare == CANONICAL_ONLY_P(name) &&
         "the canonical-only set moved; see CANONICAL_ONLY");
  if (n == 0)
    enum_empty[id] = 1; /* an instance surface with nothing to offer here */

  /* the first payload that is not the sentinel: enumeration is ranked with the
     canonical instance first, so element 0 may legitimately be it */
  int pick = -1, i;
  for (i = 0; i < n && pick < 0; i++)
    if (insts[i] != TACTIX_INSTANCE_CANONICAL)
      pick = i;

  if (pick >= 0) {
    const int r = tactix_apply_instance(s, id, insts[pick]);
    assert(r != -1 && "the guard held and the payload came from instances()");
    progressed += (r == 0);
    hit_payload[id] = 1;
  } else {
    assert(bare || n == 0);
    const int r = tactix_apply_instance(s, id, TACTIX_INSTANCE_CANONICAL);
    assert(r != -1);
    progressed += (r == 0);
    hit_canonical[id] = 1;
  }
}

/* The payload contract, on its own solver: 7 is a legal keep-level for
 * Restart, a victim count for Reduce, a variable for BVE, and nonsense to a
 * closure. */
static void alias_contract(tactix *s, int id) {
  const char *name = tactix_rule_name(id);
  const uint64_t before = tactix_stat(s, TACTIX_STAT_CONFLICTS) +
                          tactix_stat(s, TACTIX_STAT_PROPAGATIONS) +
                          tactix_stat(s, TACTIX_STAT_DECISIONS);
  const int r = tactix_apply_instance(s, id, 7);
  if (NO_SURFACE_P(name)) {
    assert(r == -1 && "PAYLOAD ALIASING: a payload on an entry with no "
                      "instance surface ran the canonical round instead of "
                      "failing");
    assert(before == tactix_stat(s, TACTIX_STAT_CONFLICTS) +
                         tactix_stat(s, TACTIX_STAT_PROPAGATIONS) +
                         tactix_stat(s, TACTIX_STAT_DECISIONS) &&
           "the rejected call still did work");
  } else {
    assert(r != -1 &&
           "an entry WITH an instance surface must interpret, clamp or no-op "
           "any payload; -1 is reserved for a failed guard / unknown rule");
  }
}

int main(void) {
  const int n = tactix_rule_count();
  int id, reached = 0, payload = 0, structured = 0;
  assert(n <= MAX_RULES);
  affine_v9_exact_row_boundary();

  for (id = 0; id < n; id++) {
    /* a fresh solver per entry: a shared one lets an earlier rule's effect
       make a later guard false, so the run reads green over less */
    tactix *s = prepare(id);
    drive(s, id);
    tactix_release(s);
    if (structural_records) { fclose(structural_records); structural_records = NULL; }
    tactix *t = prepare(id);
    alias_contract(t, id);
    tactix_release(t);
    if (structural_records) { fclose(structural_records); structural_records = NULL; }
    reached += (hit_canonical[id] || hit_payload[id] || hit_structured[id]);
    payload += hit_payload[id];
    structured += hit_structured[id];
  }

  /* Per entry: the chosen payload, structured tuple, or sentinel path ran. */
  for (id = 0; id < n; id++)
    printf("  %-18s %s%s\n", tactix_rule_name(id),
           hit_structured[id] ? "STRUCT "
                              : hit_payload[id] ? "PAYLOAD" : "CANON  ",
           enum_empty[id] ? "  (enumeration empty here)" : "");

  assert(reached == n &&
         "every registry entry must be reachable through its public action surface");
  assert(progressed > 0 &&
         "not one application made progress, so this run pinned the return "
         "convention and nothing else");
  printf("test_api_instances: %d/%d entries reached via public actions "
         "(%d through a chosen uint64 payload, %d structured), %d made progress\n",
         reached, n, payload, structured, progressed);
  return 0;
}
