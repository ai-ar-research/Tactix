#define NDEBUG 1
#include "internal.h"
#include "collect.h"
#include "propsearch.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
int main(void) {
  unsigned observations = 0;
  for (unsigned reverse = 0; reverse != 2; ++reverse) {
    kissat *solver = kissat_init(); kissat_set_option(solver, "quiet", 1);
    kissat_add(solver, -1); kissat_add(solver, 0);
    kissat_add(solver, -4); kissat_add(solver, 0);
    for (unsigned i = 0; i != 6; ++i) kissat_add(solver, reverse ? 6-i : i+1);
    kissat_add(solver, 0); assert(!kissat_search_propagate(solver));
    for (unsigned stage = 0; stage != 2; ++stage) {
      if (stage) kissat_sparse_collect(solver, true, 0);
      const unsigned first = IDX(PEEK_STACK(solver->import, 1).lit);
      assert(solver->flags[first].fixed && !solver->flags[first].active);
      assert(solver->links[first].stamp == 1); ++observations;
    }
    kissat_release(solver);
  }
  printf("native unit-first observations=%u stamp=1\n", observations);
  assert(observations == 4);
}
