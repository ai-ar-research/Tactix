#ifndef TACTIX_TESTS_G_ONLINE_TEST_SUPPORT_H
#define TACTIX_TESTS_G_ONLINE_TEST_SUPPORT_H
/* Per-action solver setup shared by the registry-wide public-action smokes
 * (test_api_instances, test_online_all_actions). Include after
 * `#undef NDEBUG`. */
#include "api_test_support.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Ample bounds for the structured producers the smokes run. */
static inline void exact_cover_extension_parameters(tactix_online_parameters *p) {
  p->kind = TACTIX_ONLINE_PARAMETERS_EXACT_COVER_EXTENSION;
  tactix_online_exact_cover_extension_parameters *e =
      &p->value.exact_cover_extension;
  e->arity = 3;
  e->maximum_items = 100;
  e->maximum_source_clauses = 100;
  e->maximum_source_literals = 1000;
  e->maximum_recognition_work = 100000;
  e->maximum_premise_references = 1000;
  e->maximum_definitions = 10;
  e->maximum_additions = 100;
  e->maximum_literal_occurrences = 1000;
  e->maximum_extent_growth = 10;
  e->maximum_external_extent = 1000;
  e->maximum_journal_bytes = 100000;
}

static inline void gf3_affine_consequence_parameters(tactix_online_parameters *p) {
  p->kind = TACTIX_ONLINE_PARAMETERS_GF3_AFFINE_CONSEQUENCE;
  p->value.gf3_affine_consequence =
      (tactix_online_gf3_affine_consequence_parameters){
          1000, 10000, 100, 100, 8, 10000, 8, 10000, 10000, 100000,
          100000, 100000, 100000, 100000, 100000, 10000, 100000,
          100000, 1000000, 10000, 10000, 100000};
}

static inline void direct_csp_support_parameters(tactix_online_parameters *p) {
  p->kind = TACTIX_ONLINE_PARAMETERS_DIRECT_CSP_SUPPORT;
  p->value.direct_csp_support =
      (tactix_online_direct_csp_support_parameters){
          1000, 10000, 100000, 32, 1000, 1000, 100000, 10000,
          100000, 1000000, 0, 10000, 10000, 100000, 0};
}

/* Three pairwise exactly-one constraints over 1, 2, 3: unsatisfiable. */
static inline void cardinality_fixture(tactix *s) {
  add_clause(s, (const int[]){1, 2, 0});
  add_clause(s, (const int[]){-1, -2, 0});
  add_clause(s, (const int[]){2, 3, 0});
  add_clause(s, (const int[]){-2, -3, 0});
  add_clause(s, (const int[]){1, 3, 0});
  add_clause(s, (const int[]){-1, -3, 0});
}

/* The structural provenance stream of the solver prepare() returned, if any. */
static FILE *structural_records;

/* A fresh solver on which `id`'s guard holds; search-state entries need more
 * than a quiescent root. Release it with release(). */
static inline tactix *prepare(int id) {
  static const char *const structural[] = {
      "ParityConsequence", "AffineConsequence", "AffineMatrixInitialize",
      "ExactCoverExtension", "IndependentSetCover", "Gf3AffineConsequence",
      "DirectCspSupport", "NaryCspEquality", "Gf3GaussianConsequence",
      "Gf3GeneralRowConsequence", "CardinalitySearch", "TseitinCycleBreaking",
      "GraphIntervalEquivalence", "SemanticVariableAdvice"};
  const char *name = tactix_rule_name(id);
  tactix *s = tactix_init();
  size_t i;
  if (!strcmp(name, "CardinalitySearch"))
    cardinality_fixture(s);
  else if (!strcmp(name, "SemanticVariableAdvice"))
    exact_cover_fixture(s);
  else
    fixture(s);
  for (i = 0; i < sizeof structural / sizeof structural[0]; ++i)
    if (!strcmp(name, structural[i])) {
      structural_records = tmpfile();
      assert(structural_records);
      assert(tactix_bind_structural_provenance(s, structural_records) == 0);
    }
  if (!strcmp(name, "SemanticVariableAdvice")) {
    /* Advice needs a theory: introduce the exact-cover extension first. */
    tactix_online_step producer;
    tactix_online_result result;
    memset(&producer, 0, sizeof producer);
    producer.step_id = 1000;
    producer.rule_id = rule_id("ExactCoverExtension");
    exact_cover_extension_parameters(&producer.parameters);
    producer.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
    step_once(&producer);
    const tactix_online_program program = {
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
    /* Both noncanonical keep-levels; the root no-op is tested elsewhere. */
    assert(tactix_decide(s, 5) == 0);
    assert(tactix_apply(s, rule_id("Propagate")) != -1);
  } else if (!strcmp(name, "ConflictAnalysis")) {
    /* Three decisions falsify (17 18 19), so propagation reports a conflict. */
    assert(tactix_decide(s, -17) == 0);
    assert(tactix_decide(s, -18) == 0);
    assert(tactix_decide(s, -19) == 0);
    assert(tactix_apply(s, rule_id("Propagate")) == 2);
  }
  return s;
}

static inline void release(tactix *s) {
  tactix_release(s);
  if (structural_records) {
    fclose(structural_records);
    structural_records = NULL;
  }
}

#endif
