/* §8.4 demo policy 1 — pure DPLL (no learning): decomposition sanity.
 *
 * Expressed with phase-≤2 rules only, through the frozen v1 API: conflict
 * handling is the single atomic ConflictAnalysis rule (Analyze+Backjump
 * fused), and "no learning" is Reduce with an everything-sized
 * victim count right after every conflict. The
 * learned clause exists transiently — N2: it is still in the proof, and
 * clauses locked as reasons survive until the next purge — so this is
 * DPLL in search shape, not a bit-exact replica of the textbook loop.
 * That is the point of the demo: the rule vocabulary expresses the
 * ancestor algorithm without touching the kernel. */
#undef NDEBUG
#include "tactix.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int rule_id(const char *name) {
  for (int i = 0; i < tactix_rule_count(); i++)
    if (!strcmp(tactix_rule_name(i), name))
      return i;
  return -1;
}

static int has(const int *ids, int n, int id) {
  for (int i = 0; i < n; i++)
    if (ids[i] == id)
      return 1;
  return 0;
}

static void add_clause(tactix *s, const int *lits) {
  for (; *lits; lits++)
    tactix_add(s, *lits);
  tactix_add(s, 0);
}

static int dpll_solve(tactix *s) {
  const int propagate = rule_id("Propagate");
  const int conflictanalysis = rule_id("ConflictAnalysis");
  const int decide = rule_id("Decide");
  const int reduce = rule_id("Reduce");
  assert(propagate >= 0 && conflictanalysis >= 0 && decide >= 0 &&
         reduce >= 0);
  long steps = 0;
  for (;;) {
    assert(++steps < 2000000); /* DPLL on toy formulas must not wedge */
    const int st = tactix_status(s);
    if (st)
      return st;
    int ids[32];
    const int n = tactix_rules_applicable(s, ids, 32);
    assert(n > 0);
    if (has(ids, n, propagate)) {
      const int r = tactix_apply(s, propagate);
      if (r == 20)
        return 20;
      continue;
    }
    if (has(ids, n, conflictanalysis)) {
      /* atomic learn+backjump (Analyze and Backjump are one rule; calculus
       * §8). Backjumps to the assertion level. */
      int r = tactix_apply(s, conflictanalysis);
      if (r == 20)
        return 20;
      /* "no learning": purge every learned clause the kernel will
       * release (payload = victim count; oversized clamps to all) */
      tactix_apply_instance(s, reduce, UINT64_MAX - 1);
      continue;
    }
    assert(has(ids, n, decide));
    const int r = tactix_apply(s, decide); /* canonical pick */
    assert(r != -1);
  }
}

int main(void) {
  /* SAT: implication chain */
  tactix *s = tactix_init();
  add_clause(s, (const int[]){1, 2, 0});
  add_clause(s, (const int[]){-1, 2, 0});
  add_clause(s, (const int[]){-2, 3, 0});
  assert(dpll_solve(s) == 10);
  tactix_release(s);

  /* UNSAT: full enumeration over two vars */
  s = tactix_init();
  add_clause(s, (const int[]){1, 2, 0});
  add_clause(s, (const int[]){1, -2, 0});
  add_clause(s, (const int[]){-1, 2, 0});
  add_clause(s, (const int[]){-1, -2, 0});
  assert(dpll_solve(s) == 20);
  tactix_release(s);

  /* UNSAT: PHP(3->2), needs real search */
  s = tactix_init();
  add_clause(s, (const int[]){1, 2, 0});
  add_clause(s, (const int[]){3, 4, 0});
  add_clause(s, (const int[]){5, 6, 0});
  add_clause(s, (const int[]){-1, -3, 0});
  add_clause(s, (const int[]){-1, -5, 0});
  add_clause(s, (const int[]){-3, -5, 0});
  add_clause(s, (const int[]){-2, -4, 0});
  add_clause(s, (const int[]){-2, -6, 0});
  add_clause(s, (const int[]){-4, -6, 0});
  assert(dpll_solve(s) == 20);
  /* the purge ran and the DB stayed lean: deletions happened */
  assert(tactix_stat(s, TACTIX_STAT_REDUCTIONS) > 0);
  tactix_release(s);

  puts("demo1_dpll OK");
  return 0;
}
