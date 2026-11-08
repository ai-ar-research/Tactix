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
#include "tactix_search_demo.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void add_clause(tactix *s, const int *lits) {
  for (; *lits; lits++)
    tactix_add(s, *lits);
  tactix_add(s, 0);
}

static int dpll_solve(tactix *s) {
  tactix_search_demo program;
  assert(tactix_search_demo_begin(&program,TACTIX_DEMO_DPLL));
  program.step_limit=2000000; /* Original toy-only nontermination guard. */
  return tactix_search_demo_run(&program,s,NULL,NULL);
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
  /* Conflict reuse can make the post-analysis purge a no-op; either way the
   * external policy traversed a genuine conflict rather than preprocessing. */
  assert(tactix_stat(s, TACTIX_STAT_CONFLICTS) > 0);
  tactix_release(s);

  puts("demo1_dpll OK");
  return 0;
}
