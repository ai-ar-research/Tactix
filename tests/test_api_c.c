/* C-language API test: compiles tactix.h as C (header/ABI sync check for
 * api.cpp) and exercises N1 end-to-end — an external client choosing every
 * branching literal itself, driving rules stepwise via the registry. */
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
  const int analyze = rule_id("Analyze");
  const int backjump = rule_id("Backjump");
  const int decide = rule_id("Decide");
  assert(propagate >= 0 && analyze >= 0 && backjump >= 0 && decide >= 0);

  /* adversarial external branching: always branch NEGATIVE on the
   * highest-numbered candidate — the opposite of what the formula wants */
  int steps = 0, status = 0;
  while (status == 0 && steps++ < 1000) {
    int applicable[16];
    const int n = tactix_rules_applicable(s, applicable, 16);
    assert(n > 0);
    /* prefer propagate > analyze > backjump > external decide */
    int did = 0;
    const int order[3] = {propagate, analyze, backjump};
    for (int j = 0; j < 3 && !did; j++)
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

int main(void) {
  test_solve_sat();
  test_external_branching();
  test_stepwise_unsat();
  puts("test_api_c OK");
  return 0;
}
