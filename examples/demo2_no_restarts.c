/* Demo 2, the "CDCL, no restarts, no inprocessing" ablation: a schedule
 * rather than a build, since the client never applies Restart, Reduce or
 * SwitchMode and runs the standard micro-loop with canonical instances. */
#undef NDEBUG
#include "example_support.h"

/* PHP(p -> p-1): every pigeon in a hole, no hole shared */
static void add_php(tactix *s, int pigeons) {
  const int holes = pigeons - 1;
  for (int p = 0; p < pigeons; p++) {
    for (int h = 0; h < holes; h++) tactix_add(s, p * holes + h + 1);
    tactix_add(s, 0);
  }
  for (int h = 0; h < holes; h++)
    for (int p1 = 0; p1 < pigeons; p1++)
      for (int p2 = p1 + 1; p2 < pigeons; p2++) EXAMPLE_ADD(s, -(p1 * holes + h + 1), -(p2 * holes + h + 1), 0);
}

int main(void) {
  /* SAT with forced propagation */
  tactix *s = tactix_init();
  EXAMPLE_ADD(s, 1, 2, 3, 0, -1, -2, 0, -1, -3, 0, 1, 2, 0);
  assert(example_search_demo(s, TACTIX_DEMO_NO_RESTARTS) == 10);
  tactix_release(s);

  /* UNSAT needing conflict-driven search: PHP(5 -> 4) */
  s = tactix_init();
  add_php(s, 5);
  assert(example_search_demo(s, TACTIX_DEMO_NO_RESTARTS) == 20);
  /* the ablation held: learning happened, nothing else did */
  assert(tactix_stat(s, TACTIX_STAT_LEARNED) > 0);
  assert(tactix_stat(s, TACTIX_STAT_RESTARTS) == 0);
  assert(tactix_stat(s, TACTIX_STAT_REDUCTIONS) == 0);
  assert(tactix_stat(s, TACTIX_STAT_SWITCHES) == 0);
  tactix_release(s);

  puts("demo2_no_restarts OK");
  return 0;
}
