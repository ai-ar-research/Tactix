/* C-language API test: compiles tactix.h as C (a header/ABI sync check) and
 * exercises N1 — an external client choosing every branching literal. */
#undef NDEBUG
#include "tactix.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

static int rule_id(const char *name) {
  for (int i = 0; i < tactix_rule_count(); i++)
    if (!strcmp(tactix_rule_name(i), name))
      return i;
  return -1;
}

static void add_clause(tactix *s, const int *lits) {
  for (; *lits; lits++)
    tactix_add(s, *lits);
  tactix_add(s, 0);
}

static void test_solve_sat(void) {
  tactix *s = tactix_init();
  add_clause(s, (const int[]){1, 2, 0});
  add_clause(s, (const int[]){-1, 2, 0});
  add_clause(s, (const int[]){1, -2, 0});
  assert(tactix_solve(s) == 10);
  assert(tactix_value(s, 2) == 2);
  tactix_release(s);
}

/* N1: the client is the branching policy, driving the registry directly. */
static void test_external_branching(void) {
  tactix *s = tactix_init();
  /* exactly one literal true, and with (1 2),(1 3) the model must set 1 */
  add_clause(s, (const int[]){1, 2, 3, 0});
  add_clause(s, (const int[]){-1, -2, 0});
  add_clause(s, (const int[]){-1, -3, 0});
  add_clause(s, (const int[]){-2, -3, 0});
  add_clause(s, (const int[]){1, 2, 0});
  add_clause(s, (const int[]){1, 3, 0});

  const int propagate = rule_id("Propagate");
  const int conflictanalysis = rule_id("ConflictAnalysis");
  const int decide = rule_id("Decide");
  assert(propagate >= 0 && conflictanalysis >= 0 && decide >= 0);

  /* adversarial branching: always negative, on the highest candidate */
  int steps = 0, status = 0;
  while (status == 0 && steps++ < 1000) {
    int applicable[16];
    const int n = tactix_rules_applicable(s, applicable, 16);
    assert(n > 0);
    /* prefer propagate > conflict-analysis > external decide */
    int did = 0;
    const int order[2] = {propagate, conflictanalysis};
    for (int j = 0; j < 2 && !did; j++)
      for (int i = 0; i < n && !did; i++)
        if (applicable[i] == order[j]) {
          const int r = tactix_apply(s, order[j]);
          if (r == 20) status = 20;
          did = 1;
        }
    if (!did) {
      int cands[64];
      const int m = tactix_decide_candidates(s, cands, 64);
      if (m == 0) break; /* nothing to decide: all assigned */
      int pick = cands[0];
      for (int i = 1; i < m; i++)
        if (abs(cands[i]) > abs(pick)) pick = cands[i];
      pick = -abs(pick); /* branch false, adversarially */
      assert(tactix_decide(s, pick) == 0);
    }
    if (tactix_status(s) == 10) status = 10;
  }
  assert(status == 10);
  assert(tactix_value(s, 1) == 1); /* forced by (1 2),(1 3),(-2 -3) */
  assert(tactix_value(s, 2) * tactix_value(s, 3) < 0 ||
         (tactix_value(s, 2) < 0 && tactix_value(s, 3) < 0));
  tactix_release(s);
}

/* Instance surface (plan §5.1): enumeration plus chosen-instance apply. */
static void test_instance_surface(void) {
  tactix *s = tactix_init();
  add_clause(s, (const int[]){1, 2, 3, 0});
  add_clause(s, (const int[]){-1, -2, 0});

  const int propagate = rule_id("Propagate");
  const int decide = rule_id("Decide");
  const int restart = rule_id("Restart");
  const int switchmode = rule_id("SwitchMode");
  uint64_t insts[64];

  /* errors: unknown rule / guard not holding */
  assert(tactix_rule_instances(s, -1, insts, 64) == -1);
  assert(tactix_rule_instances(s, 9999, insts, 64) == -1);
  assert(tactix_rule_instances(s, restart, insts, 64) == 1); /* legal root no-op */
  assert(insts[0] == 0);
  assert(tactix_apply_instance(s, restart, 0) == 0);
  assert(tactix_rule_instances(s, propagate, insts, 64) == -1); /* queue
    empty: nothing to propagate on a fresh formula without units */

  /* no-choice rule: exactly the canonical sentinel */
  assert(tactix_rule_instances(s, switchmode, insts, 64) == 1);
  assert(insts[0] == TACTIX_INSTANCE_CANONICAL);

  /* Decide payloads are DIMACS literals; apply one explicitly */
  const int n = tactix_rule_instances(s, decide, insts, 64);
  assert(n > 0);
  const uint64_t before = tactix_stat(s, TACTIX_STAT_DECISIONS);
  assert(tactix_apply_instance(s, decide, TACTIX_LIT_INSTANCE(-3)) == 0);
  assert(tactix_stat(s, TACTIX_STAT_DECISIONS) == before + 1);
  assert(tactix_stat(s, TACTIX_STAT_LEVEL) == 1);
  assert(tactix_decide(s, -3) == -1); /* already assigned */

  /* N1: every literal is expressible as an instance, -1 included — the
     payload space is zero-extended, so nothing aliases the sentinel */
  assert(TACTIX_LIT_INSTANCE(-1) != TACTIX_INSTANCE_CANONICAL);
  for (int i = 0; i < n; i++)
    assert(insts[i] != TACTIX_INSTANCE_CANONICAL); /* no literal aliases it */
  {
    tactix *t = tactix_init();
    add_clause(t, (const int[]){1, 2, 0});
    add_clause(t, (const int[]){-1, 3, 0});
    assert(tactix_apply_instance(t, decide, TACTIX_LIT_INSTANCE(-1)) == 0);
    /* the literal asked for is assigned, not Decide's own saved phase */
    assert(tactix_value(t, 1) == -1);
    tactix_release(t);
  }

  assert(tactix_apply_instance(s, propagate, TACTIX_INSTANCE_CANONICAL) !=
         -1);

  /* Restart payloads are keep-levels: canonical first, 0 = full restart */
  assert(tactix_decide(s, 2) == 0);
  const int m = tactix_rule_instances(s, restart, insts, 64);
  assert(m >= 2);
  const uint64_t restarts_before = tactix_stat(s, TACTIX_STAT_RESTARTS);
  assert(tactix_apply_instance(s, restart, 0) == 0); /* full restart */
  assert(tactix_stat(s, TACTIX_STAT_LEVEL) == 0);
  assert(tactix_stat(s, TACTIX_STAT_RESTARTS) == restarts_before + 1);

  assert(tactix_stat(s, 9999) == UINT64_MAX);
  tactix_release(s);

  /* proof formats: all five accepted; junk isn't.
     Named after the writing process: ctest runs concurrently over several
     build directories, and a shared name lets those runs reopen and truncate
     each other's proof file. */
  char proof_path[64];
  snprintf(proof_path, sizeof proof_path, "/tmp/tactix-api-fmt-%ld.proof",
           (long)getpid());
  s = tactix_init();
  assert(tactix_proof(s, proof_path, 99) == -1);
  tactix_release(s);
  const int formats[] = {TACTIX_PROOF_DRAT_BINARY, TACTIX_PROOF_DRAT_ASCII,
                         TACTIX_PROOF_LRAT, TACTIX_PROOF_DSR_BINARY,
                         TACTIX_PROOF_DSR_ASCII};
  for (unsigned i = 0; i < sizeof formats / sizeof formats[0]; ++i) {
    s = tactix_init();
    assert(tactix_proof(s, proof_path, formats[i]) == 0);
    tactix_release(s);
  }
  remove(proof_path);
}

static void test_conflict_boundary_restart_and_reduce(void) {
  tactix *s = tactix_init();
  add_clause(s, (const int[]){1, 2, 0});
  add_clause(s, (const int[]){-1, 2, 0});
  add_clause(s, (const int[]){1, -2, 0});
  add_clause(s, (const int[]){-1, -2, 0});
  const int decide = rule_id("Decide");
  const int propagate = rule_id("Propagate");
  const int reduce = rule_id("Reduce");
  const int restart = rule_id("Restart");
  assert(tactix_apply_instance(s, decide, TACTIX_LIT_INSTANCE(-1)) == 0);
  assert(tactix_apply(s, propagate) == 2);

  /* N1 state boundary: database reduction is legal with the conflict pinned,
     and a subsequent full restart may abandon that conflicting search cube. */
  assert(tactix_apply(s, reduce) == 1);
  assert(tactix_apply_instance(s, restart, 0) == 0);
  assert(tactix_stat(s, TACTIX_STAT_LEVEL) == 0);
  assert(tactix_status(s) == 0);
  tactix_release(s);
}

/* A solver that never saw a variable must survive rule application. */
static void test_empty_solver_rule_application(void) {
  tactix *s = tactix_init();
  const int sw = rule_id("SwitchMode");
  assert(sw >= 0);
  assert(tactix_status(s) == 10);   /* empty formula: trivially SAT */
  assert(tactix_apply(s, sw) == 0); /* guard holds; ran invariants */
  assert(tactix_apply(s, sw) == 0);
  assert(tactix_status(s) == 10);
  assert(tactix_solve(s) == 10);
  tactix_release(s);
}

/* stepwise UNSAT: proof concluded through the API path too */
static void test_stepwise_unsat(void) {
  tactix *s = tactix_init();
  add_clause(s, (const int[]){1, 2, 0});
  add_clause(s, (const int[]){1, -2, 0});
  add_clause(s, (const int[]){-1, 2, 0});
  add_clause(s, (const int[]){-1, -2, 0});
  assert(tactix_solve(s) == 20);
  assert(tactix_status(s) == 20);
  tactix_release(s);
}

/* N1: BVE gate-type selection through the public API. Var 1 eliminates only
 * via the gate reduction, so the setter flips one apply to a no-op. */
static void test_bve_gate_selection(void) {
  const int bve = rule_id("BVE");
  assert(bve >= 0);
  const int base[] = {-1, 2, 3, 4, 5, 6, 7, 0};
  int i;

  tactix *s = tactix_init();
  add_clause(s, base);
  for (i = 2; i <= 7; i++)
    add_clause(s, (const int[]){1, -i, 0});
  for (i = 8; i <= 11; i++)
    add_clause(s, (const int[]){1, i, 0});
  for (i = 12; i <= 15; i++)
    add_clause(s, (const int[]){-1, i, 0});

  tactix_set_bve_gates(s, TACTIX_BVE_GATE_EQUIV); /* AND/OR off */
  assert(tactix_apply_instance(s, bve, TACTIX_LIT_INSTANCE(1)) == 1); /* no-op */
  tactix_set_bve_gates(s, TACTIX_BVE_GATE_AND_OR); /* AND/OR on */
  assert(tactix_apply_instance(s, bve, TACTIX_LIT_INSTANCE(1)) == 0); /* progress */
  tactix_release(s);
}

/* The §5.6 reward-counter and config-setter surface: every documented stat id
 * returns a real counter, and every setter is callable at any state (N1). */
static void test_stats_and_config_surface(void) {
  int id;
  tactix *s = tactix_init();
  /* a small UNSAT core so inprocessing counters exist meaningfully */
  add_clause(s, (const int[]){1, 2, 0});
  add_clause(s, (const int[]){1, -2, 0});
  add_clause(s, (const int[]){-1, 2, 0});
  add_clause(s, (const int[]){-1, -2, 0});
  /* setters callable pre-solve (N1: any state) */
  tactix_set_bve_bound(s, 4);
  tactix_set_factor_bound(s, 1);
  tactix_set_factor_composite(s, 1);
  tactix_set_work_budget(s, 1000000);
  tactix_set_vivify_scope(s, 1);
  /* ... and every setter's value must read back: a pose silently dropped
     gives an arm that measures nothing while reading as configured */
  assert(tactix_config(s, TACTIX_CFG_BVE_BOUND) == 4);
  assert(tactix_config(s, TACTIX_CFG_FACTOR_BOUND) == 1);
  assert(tactix_config(s, TACTIX_CFG_FACTOR_COMPOSITE) == 1);
  assert(tactix_config(s, TACTIX_CFG_WORK_BUDGET) == 1000000);
  assert(tactix_config(s, TACTIX_CFG_VIVIFY_SCOPE) == 1);
  tactix_set_bve_gates(s, TACTIX_BVE_GATE_ITE);
  assert(tactix_config(s, TACTIX_CFG_BVE_GATES) == TACTIX_BVE_GATE_ITE);
  assert(tactix_config(s, TACTIX_CFG_BACKBONE_INITIAL_ROUNDS) == 100);
  assert(tactix_config(s, TACTIX_CFG_BACKBONE_MAXIMUM_ROUNDS) == 1000);
  tactix_set_backbone_round_limits(s, 13, 17);
  assert(tactix_config(s, TACTIX_CFG_BACKBONE_INITIAL_ROUNDS) == 13);
  assert(tactix_config(s, TACTIX_CFG_BACKBONE_MAXIMUM_ROUNDS) == 17);
  assert(tactix_set_vivify_parameters(s, 4, 0, 1, 17) == 0);
  assert(tactix_config(s, TACTIX_CFG_VIVIFY_SCOPE) == 4);
  assert(tactix_config(s, TACTIX_CFG_VIVIFY_REUSE) == 0);
  assert(tactix_config(s, TACTIX_CFG_VIVIFY_FLUSH) == 1);
  assert(tactix_config(s, TACTIX_CFG_VIVIFY_IRREDUNDANT_SORT_RATIO) == 17);
  assert(tactix_set_vivify_parameters(s, 6, 1, 0, 10) == -1);
  assert(tactix_set_vivify_parameters(s, 0, 1, 0, 0) == -1);
  assert(tactix_config(s, TACTIX_CFG_VIVIFY_SCOPE) == 4);
  tactix_set_instantiate_material_threshold(s, 7);
  assert(tactix_config(s, TACTIX_CFG_INSTANTIATE_NEW_MATERIAL_THRESHOLD) == 7);
  assert(tactix_config(s, 26) == UINT64_MAX); /* first unknown cfg id */
  assert(tactix_config(s, -1) == UINT64_MAX);
  /* every band Vivify implements is settable: clamping 3/4/5 onto 0 would
     deny API callers a tier DefaultPolicy uses (N1) */
  for (id = 0; id <= 5; id++) {
    tactix_set_vivify_scope(s, (unsigned)id);
    assert(tactix_config(s, TACTIX_CFG_VIVIFY_SCOPE) == (uint64_t)id);
  }
  /* restore neutral config for the solve */
  tactix_set_bve_gates(s, ~0u);
  tactix_set_factor_composite(s, 0);
  tactix_set_work_budget(s, 0);
  tactix_set_vivify_scope(s, 0);
  assert(tactix_set_vivify_parameters(s, 0, 1, 0, 10) == 0);
  assert(tactix_solve(s) == 20);
  /* Completeness: every documented id 0..50 answers, in yield/effort pairs so
     the §5.6 learner can order outcomes by cost per unit output. Presence
     only — tools/docs/check-stat-ids.py pins the mapping. */
  for (id = 0; id <= 51; id++)
    assert(tactix_stat(s, id) != UINT64_MAX);
  assert(tactix_stat(s, 52) == UINT64_MAX); /* first unknown id */
  assert(tactix_stat(s, -1) == UINT64_MAX);
  tactix_release(s);
}

/* Pin the frozen registry id/name assignment and each entry's ActionKind.
 * A new rule renumbers ids by name sort, so update this list with it. */
static void test_action_kind(void) {
  static const char *const golden[] = {
      "BCE",        "BVE",        "Backbone",         "CCE",
      "ConflictAnalysis",
      "Congruence", "Decide",     "Factor",           "HyperBinaryRes",
      "HyperTernaryRes",
      "Instantiate", "Lucky",     "Probe",            "Propagate",
      "Reduce",     "Reorder",    "Rephase",          "Restart",
      "Strengthen",
      "Substitute", "SubsumeForward", "Sweep",        "SwitchMode",
      "TransitiveReduce",
      "Vivify",     "Walk"};
  const int n = (int)(sizeof(golden) / sizeof(golden[0]));
  assert(tactix_rule_count() == n);
  for (int i = 0; i < n; i++)
    assert(!strcmp(tactix_rule_name(i), golden[i]));

  assert(tactix_action_kind(rule_id("Rephase")) == TACTIX_ACTION_POLICY_OP);
  assert(tactix_action_kind(rule_id("SwitchMode")) == TACTIX_ACTION_POLICY_OP);
  assert(tactix_action_kind(rule_id("Walk")) == TACTIX_ACTION_POLICY_OP);
  assert(tactix_action_kind(rule_id("Reorder")) == TACTIX_ACTION_POLICY_OP);

  /* spot-check proof rules, and assert exactly those four are policy ops */
  assert(tactix_action_kind(rule_id("Propagate")) == TACTIX_ACTION_PROOF_RULE);
  assert(tactix_action_kind(rule_id("BVE")) == TACTIX_ACTION_PROOF_RULE);
  assert(tactix_action_kind(rule_id("Lucky")) == TACTIX_ACTION_PROOF_RULE);
  int policy_ops = 0;
  for (int i = 0; i < n; i++) {
    const int k = tactix_action_kind(i);
    assert(k == TACTIX_ACTION_PROOF_RULE || k == TACTIX_ACTION_POLICY_OP);
    if (k == TACTIX_ACTION_POLICY_OP)
      policy_ops++;
  }
  assert(policy_ops == 4);

  /* out-of-range → -1 (same convention as tactix_rule_id) */
  assert(tactix_action_kind(-1) == -1);
  assert(tactix_action_kind(n) == -1);
  assert(tactix_action_kind(9999) == -1);
}

int main(void) {
  test_solve_sat();
  test_external_branching();
  test_instance_surface();
  test_conflict_boundary_restart_and_reduce();
  test_empty_solver_rule_application();
  test_stepwise_unsat();
  test_bve_gate_selection();
  test_stats_and_config_surface();
  test_action_kind();
  puts("test_api_c OK");
  return 0;
}
