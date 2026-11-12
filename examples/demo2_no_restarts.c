/* §8.4 demo policy 2 — "CDCL, no restarts, no inprocessing" ablation.
 *
 * The ablation is a schedule, not a build: the client simply never
 * applies Restart / Reduce / SwitchMode. Everything else is the standard
 * micro-loop through the frozen v1 API, with canonical instances. */
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

static int cdcl_no_restarts(tactix *s) {
  tactix_search_demo program;
  assert(tactix_search_demo_begin(&program,TACTIX_DEMO_NO_RESTARTS));
  program.step_limit=2000000; /* Original toy-only nontermination guard. */
  return tactix_search_demo_run(&program,s,NULL,NULL);
}

/* PHP(p -> p-1): every pigeon in a hole, no hole shared */
static void add_php(tactix *s, int pigeons) {
  const int holes = pigeons - 1;
  for (int p = 0; p < pigeons; p++) {
    for (int h = 0; h < holes; h++)
      tactix_add(s, p * holes + h + 1);
    tactix_add(s, 0);
  }
  for (int h = 0; h < holes; h++)
    for (int p1 = 0; p1 < pigeons; p1++)
      for (int p2 = p1 + 1; p2 < pigeons; p2++) {
        tactix_add(s, -(p1 * holes + h + 1));
        tactix_add(s, -(p2 * holes + h + 1));
        tactix_add(s, 0);
      }
}

int main(void) {
  /* SAT with forced propagation */
  tactix *s = tactix_init();
  add_clause(s, (const int[]){1, 2, 3, 0});
  add_clause(s, (const int[]){-1, -2, 0});
  add_clause(s, (const int[]){-1, -3, 0});
  add_clause(s, (const int[]){1, 2, 0});
  assert(cdcl_no_restarts(s) == 10);
  tactix_release(s);

  /* UNSAT needing conflict-driven search: PHP(5 -> 4) */
  s = tactix_init();
  add_php(s, 5);
  assert(cdcl_no_restarts(s) == 20);
  /* the ablation held: learning happened, nothing else did */
  assert(tactix_stat(s, TACTIX_STAT_LEARNED) > 0);
  assert(tactix_stat(s, TACTIX_STAT_RESTARTS) == 0);
  assert(tactix_stat(s, TACTIX_STAT_REDUCTIONS) == 0);
  assert(tactix_stat(s, TACTIX_STAT_SWITCHES) == 0);
  tactix_release(s);

  puts("demo2_no_restarts OK");
  return 0;
}
