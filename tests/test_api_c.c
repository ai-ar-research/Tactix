/* C-language API test: compiles tactix.h as C (header/ABI sync check for
 * api.cpp) and exercises N1 end-to-end — an external client choosing every
 * branching literal itself, driving rules stepwise via the registry. */
#undef NDEBUG
#include "tactix.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* default-policy solve via the plain API */
static void test_solve_sat(void) {
  tactix *s = tactix_init();
  add_clause(s, (const int[]){1, 2, 0});
  add_clause(s, (const int[]){-1, 2, 0});
  add_clause(s, (const int[]){1, -2, 0});
  assert(tactix_solve(s) == 10);
  assert(tactix_value(s, 2) == 2);
  tactix_release(s);
}

/* N1: the client is the branching policy; the solver never picks a
 * decision itself. Micro-loop driven entirely through the registry. */
static void test_external_branching(void) {
  tactix *s = tactix_init();
  /* (1 2 3)(−1 −2)(−1 −3)(−2 −3)(1 2)(1 3): forces exactly one true,
   * combined with (1 2) and (1 3) the model must set 1 true. */
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

  /* adversarial external branching: always branch NEGATIVE on the
   * highest-numbered candidate — the opposite of what the formula wants */
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

/* v1 instance surface (plan §5.1): generic enumeration + chosen-
 * instance application across the payload encodings */
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
  assert(tactix_rule_instances(s, restart, insts, 64) == -1); /* level 0 */
  assert(tactix_rule_instances(s, propagate, insts, 64) == -1); /* queue
    empty: nothing to propagate on a fresh formula without units */

  /* no-choice rule: exactly the canonical sentinel */
  assert(tactix_rule_instances(s, switchmode, insts, 64) == 1);
  assert(insts[0] == TACTIX_INSTANCE_CANONICAL);

  /* Decide payloads are DIMACS literals (TACTIX_LIT_INSTANCE); apply one
     explicitly */
  const int n = tactix_rule_instances(s, decide, insts, 64);
  assert(n > 0);
  const uint64_t before = tactix_stat(s, TACTIX_STAT_DECISIONS);
  assert(tactix_apply_instance(s, decide, TACTIX_LIT_INSTANCE(-3)) == 0);
  assert(tactix_stat(s, TACTIX_STAT_DECISIONS) == before + 1);
  assert(tactix_stat(s, TACTIX_STAT_LEVEL) == 1);
  assert(tactix_decide(s, -3) == -1); /* already assigned */

  /* N1: EVERY literal must be expressible as an instance, -1 included. A
     SIGN-extending encoding maps -1 onto TACTIX_INSTANCE_CANONICAL, which
     silently branches on the solver's own pick instead of the one asked for;
     the payload space is zero-extended and disjoint. */
  assert(TACTIX_LIT_INSTANCE(-1) != TACTIX_INSTANCE_CANONICAL);
  for (int i = 0; i < n; i++)
    assert(insts[i] != TACTIX_INSTANCE_CANONICAL); /* no literal aliases it */
  {
    tactix *t = tactix_init();
    add_clause(t, (const int[]){1, 2, 0});
    add_clause(t, (const int[]){-1, 3, 0});
    assert(tactix_apply_instance(t, decide, TACTIX_LIT_INSTANCE(-1)) == 0);
    /* the literal we ASKED for is the one assigned: value(1) is FALSE. Under
       the old encoding this ran Decide's canonical instance, which picks the
       saved phase (+1), leaving value(1) TRUE. */
    assert(tactix_value(t, 1) == -1);
    tactix_release(t);
  }

  /* canonical sentinel routes to the canonical instance (propagate is
   * applicable now: the decision is unpropagated) */
  assert(tactix_apply_instance(s, propagate, TACTIX_INSTANCE_CANONICAL) !=
         -1);

  /* Restart payloads are keep-levels: canonical first, 0 = full restart */
  assert(tactix_decide(s, 2) == 0);
  const int m = tactix_rule_instances(s, restart, insts, 64);
  assert(m >= 2);
  assert(tactix_apply_instance(s, restart, 0) == 0); /* full restart */
  assert(tactix_stat(s, TACTIX_STAT_LEVEL) == 0);
  assert(tactix_stat(s, TACTIX_STAT_RESTARTS) == 1);

  /* stats: unknown id */
  assert(tactix_stat(s, 9999) == UINT64_MAX);
  tactix_release(s);

  /* proof formats: all three accepted (native LRAT landed); junk isn't */
  s = tactix_init();
  assert(tactix_proof(s, "/tmp/tactix_api_fmt.drat", 99) == -1);
  assert(tactix_proof(s, "/tmp/tactix_api_fmt.drat", TACTIX_PROOF_LRAT) ==
         0);
  tactix_release(s);
  s = tactix_init();
  assert(tactix_proof(s, "/tmp/tactix_api_fmt.drat",
                      TACTIX_PROOF_DRAT_BINARY) == 0);
  tactix_release(s);
  remove("/tmp/tactix_api_fmt.drat");
}

/* regression 001-empty-formula-frames: a solver that never saw a variable
 * must survive rule application — the fresh-kernel frame stack used to
 * violate the frames/level invariant, aborting kernel_check_invariants
 * inside tactix_apply on ANY H-rule (debug builds) */
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

/* E1d N1 surface: BVE gate-type selection through the public C API. Var 1 of
 * this AND/OR-gate instance eliminates ONLY via the gate reduction (its full
 * cross product exceeds the growth bound), so disabling AND/OR makes the same
 * apply a no-op and enabling it makes progress — the public setter is honored. */
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

/* rev 69: the §5.6 reward-counter + config-setter surface. Every documented
 * stat id must return a real counter (not the UINT64_MAX unknown-id
 * sentinel) after a solve, and every config setter must be callable at any
 * state with the solve still correct — config is sound by class, so the
 * only contract to pin is reachability + a sane verdict. */
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
  /* restore neutral config for the solve */
  tactix_set_factor_composite(s, 0);
  tactix_set_work_budget(s, 0);
  tactix_set_vivify_scope(s, 0);
  assert(tactix_solve(s) == 20);
  /* completeness: every documented id 1..31 answers. Ids 26-31 come in
     yield/effort PAIRS because the plan §5.6 learner reads them as rewards and
     cost-per-unit-output — not yield alone — is the axis that orders rule
     outcomes. */
  for (id = 1; id <= 31; id++)
    assert(tactix_stat(s, id) != UINT64_MAX);
  assert(tactix_stat(s, 32) == UINT64_MAX); /* first unknown id */
  tactix_release(s);
}

/* Pin the frozen registry id/name assignment (a permanent
 * id-drift guard — reclassifying Rephase/SwitchMode in place must NOT move any
 * id) and each entry's ActionKind. Rephase and SwitchMode are POLICY OPERATIONS
 * (H-only, plan rev 59/60), as are Walk and Reorder; every other entry is a PROOF RULE. Adding a rule
 * renumbers ids by the name sort — update this golden list deliberately then. */
static void test_action_kind(void) {
  static const char *const golden[] = {
      "BCE",        "BVE",        "Backbone",         "CCE",
      "ConflictAnalysis",
      "Congruence", "Decide",     "Factor",           "HyperTernaryRes",
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

  /* the policy operations (rev 59/60; Walk joined in — it sorts last,
   * so no existing id moved) */
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
  test_empty_solver_rule_application();
  test_stepwise_unsat();
  test_bve_gate_selection();
  test_stats_and_config_surface();
  test_action_kind();
  puts("test_api_c OK");
  return 0;
}
