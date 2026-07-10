/* Modularity stress test (plan §8.2 rule-order fuzzing, C API level):
 * random legal rule schedules — "legal" = the rule's SOUNDNESS guard holds,
 * scheduling advice deliberately ignored — must never crash, never wedge,
 * and must reach the same verdict as the default policy. Branching picks
 * uniformly random unassigned literals (the theoretical instance space).
 *
 * The debug build runs this under ASan/UBSan, so any stale ref, dangling
 * watch, or state-machine hole under weird-but-legal orders aborts here. */
#undef NDEBUG
#include "tactix.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long long rng_state;
static unsigned long long rnd(void) {
  rng_state ^= rng_state >> 12;
  rng_state ^= rng_state << 25;
  rng_state ^= rng_state >> 27;
  return rng_state * 0x2545F4914F6CDD1DULL;
}

struct formula {
  const char *name;
  const int *clauses; /* 0-terminated clauses; list ends with double 0 */
  int expected;       /* 10 or 20 */
};

static const int f_sat_chain[] = {1, 2, 0, -1, 2, 0, -2, 3, 0, -3, 4, 0, 0};
static const int f_unsat_enum[] = {1, 2, 0, 1, -2, 0, -1, 2, 0, -1, -2, 0, 0};
static const int f_unsat_php[] = {/* PHP(2): 3 pigeons, 2 holes */
    1, 2, 0, 3, 4, 0, 5, 6, 0, -1, -3, 0, -1, -5, 0, -3, -5, 0,
    -2, -4, 0, -2, -6, 0, -4, -6, 0, 0};
static const int f_sat_amo[] = {/* exactly-one of 4 + implications */
    1, 2, 3, 4, 0, -1, -2, 0, -1, -3, 0, -1, -4, 0, -2, -3, 0,
    -2, -4, 0, -3, -4, 0, -1, 5, 0, -5, 6, 0, 0};
static const int f_unsat_xor[] = {/* odd cyclic xor chain */
    1, 2, 0, -1, -2, 0, 2, 3, 0, -2, -3, 0, 1, 3, 0, -1, -3, 0, 0};
static const int f_sat_units[] = {1, 0, -2, 0, 3, 0, -3, 4, 0, 0};

static const struct formula battery[] = {
    {"sat-chain", f_sat_chain, 10},  {"unsat-enum", f_unsat_enum, 20},
    {"unsat-php2", f_unsat_php, 20}, {"sat-amo", f_sat_amo, 10},
    {"unsat-xor3", f_unsat_xor, 20}, {"sat-units", f_sat_units, 10},
};

static tactix *load(const struct formula *f) {
  tactix *s = tactix_init();
  const int *p = f->clauses;
  /* every clause carries its own 0 in the array; the extra trailing 0 is
   * only the list sentinel — adding it would add an EMPTY clause */
  while (!(p[0] == 0 && (p == f->clauses || p[-1] == 0))) {
    tactix_add(s, *p);
    p++;
  }
  return s;
}

static int max_var(const struct formula *f) {
  int m = 0;
  for (const int *p = f->clauses;
       !(p[0] == 0 && (p == f->clauses || p[-1] == 0)); p++) {
    const int v = *p < 0 ? -*p : *p;
    if (v > m)
      m = v;
  }
  return m;
}

static int rule_by_name(const char *name) {
  for (int i = 0; i < tactix_rule_count(); i++)
    if (!strcmp(tactix_rule_name(i), name))
      return i;
  return -1;
}

/* uniformly random unassigned literal, probing through tactix_decide's own
 * validation (a rejected pick returns nonzero and costs nothing) */
static int random_branch(tactix *s, int maxvar) {
  for (int tries = 0; tries < 64 * maxvar; tries++) {
    const int v = (int)(rnd() % maxvar) + 1;
    const int lit = (rnd() & 1) ? v : -v;
    if (tactix_decide(s, lit) == 0)
      return 1;
  }
  return 0;
}

static void run_random(const struct formula *f, unsigned long long seed) {
  rng_state = seed ? seed : 1;
  tactix *s = load(f);
  const int decide = rule_by_name("Decide");
  assert(decide >= 0);
  const int mv = max_var(f);

  int verdict = tactix_status(s);
  long steps = 0;
  while (verdict == 0) {
    assert(++steps < 200000); /* a random walk must not wedge */
    int rules[32];
    const int n = tactix_rules_applicable(s, rules, 32);
    assert(n > 0); /* pre-verdict, some rule is always applicable */
    const int id = rules[rnd() % n];
    if (id == decide) {
      random_branch(s, mv);
    } else {
      const int r = tactix_apply(s, id);
      assert(r != -1); /* guard held: apply must be accepted */
      if (r == 10 || r == 20) {
        verdict = r;
        break;
      }
    }
    verdict = tactix_status(s);
  }
  assert(verdict == f->expected); /* same truth under any legal schedule */
  tactix_release(s);
}

int main(void) {
  assert(tactix_rule_count() >= 6); /* M1-M4 + S1-S2 registered */
  for (size_t i = 0; i < sizeof(battery) / sizeof(battery[0]); i++) {
    tactix *s = load(&battery[i]);
    assert(tactix_solve(s) == battery[i].expected);
    tactix_release(s);
    for (unsigned long long seed = 1; seed <= 50; seed++)
      run_random(&battery[i], seed * 0x9E3779B97F4A7C15ULL);
    printf("random-policy %s: 50 seeds OK\n", battery[i].name);
  }
  puts("test_random_policy OK");
  return 0;
}
