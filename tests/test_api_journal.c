#undef NDEBUG
#include "tactix.h"

#include <assert.h>

static void one_session(void) {
  tactix *s = tactix_init();
  const int clauses[][4] = {{1, 2, 0}, {1, 2, 0},
                            {-1, 3, 0}, {-3, 4, 0}};
  for (unsigned i = 0; i < 4; ++i) {
    for (unsigned j = 0; clauses[i][j]; ++j)
      tactix_add(s, clauses[i][j]);
    tactix_add(s, 0);
  }
  const int clause[] = {1, 2};
  const int witness[] = {1, 3, 4};
  assert(tactix_apply_condition(s, clause, 2, witness, 3) == 0);
  assert(tactix_solve(s) == 10);
  tactix_release(s);
}

int main(void) {
  one_session();
  one_session();
  /* Intentionally leave the journal open: the exit callback must check replay
   * exhaustion without touching a transaction whose thread-local lifetime
   * may have ended first. ASan runs this fixture with journaling enabled. */
  return 0;
}
