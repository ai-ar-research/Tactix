/* Fixed native mechanism fixture, not a solver benchmark. */
#include "factor.c"
#include "kissat.h"
#include "import.h"
#include <stdio.h>

int main(void) {
  kissat *solver = kissat_init();
  for (int i = 1; i <= 2; ++i) {
    kissat_add(solver, i); kissat_add(solver, i + 1); kissat_add(solver, 0);
  }
  const unsigned first = IDX(kissat_import_literal(solver, 1));
  kissat_move_to_front(solver, first);
  solver->queue.search.idx = solver->queue.first;
  solver->queue.search.stamp = LINK(solver->queue.first).stamp;
  factoring f = {0};
  f.solver = solver;
  INIT_STACK(f.fresh);
  const unsigned before = solver->queue.stamp;
  adjust_scores_and_phases_of_fresh_variables(&f);
  unsigned count = 0;
  bool ok = true;
  for (unsigned idx = solver->queue.first; !DISCONNECTED(idx); idx = LINK(idx).next)
    ok &= LINK(idx).stamp == ++count;
  ok &= solver->queue.stamp == count;
  ok &= solver->queue.search.idx == solver->queue.last;
  ok &= solver->queue.search.stamp == solver->queue.stamp;
  printf("empty native Factor: stamp %u -> %u, dense=%d, cursor_tail=%d\n",
         before, solver->queue.stamp, ok, solver->queue.search.idx == solver->queue.last);
  RELEASE_STACK(f.fresh);
  kissat_release(solver);
  return ok ? 0 : 1;
}
