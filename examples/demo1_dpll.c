/* Demo 1, pure DPLL without learning, through the public API: conflict
 * handling is the atomic ConflictAnalysis rule, and "no learning" is Reduce
 * with an everything-sized victim count after every conflict. The learned
 * clause still exists transiently (it is in the proof, and reason-locked
 * clauses survive until the next purge), so this is DPLL in search shape
 * rather than a bit-exact replica of the textbook loop. */
#undef NDEBUG
#include "example_support.h"

int main(void) {
  /* SAT: implication chain */
  tactix *s = tactix_init();
  EXAMPLE_ADD(s, 1, 2, 0, -1, 2, 0, -2, 3, 0);
  assert(example_search_demo(s, TACTIX_DEMO_DPLL) == 10);
  tactix_release(s);

  /* UNSAT: full enumeration over two vars */
  s = tactix_init();
  EXAMPLE_ADD(s, 1, 2, 0, 1, -2, 0, -1, 2, 0, -1, -2, 0);
  assert(example_search_demo(s, TACTIX_DEMO_DPLL) == 20);
  tactix_release(s);

  /* UNSAT: PHP(3->2), needs real search */
  s = tactix_init();
  EXAMPLE_ADD(s, 1, 2, 0, 3, 4, 0, 5, 6, 0, -1, -3, 0, -1, -5, 0, -3, -5, 0, -2, -4, 0, -2, -6, 0,
              -4, -6, 0);
  assert(example_search_demo(s, TACTIX_DEMO_DPLL) == 20);
  /* Conflict reuse can make the post-analysis purge a no-op; either way the
   * external policy traversed a genuine conflict rather than preprocessing. */
  assert(tactix_stat(s, TACTIX_STAT_CONFLICTS) > 0);
  tactix_release(s);

  puts("demo1_dpll OK");
  return 0;
}
