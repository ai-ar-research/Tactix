#define NDEBUG 1
#include "internal.h"
#include "collect.h"
#include "propsearch.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
int main(void) {
  for (unsigned reverse = 0; reverse != 2; ++reverse) {
    kissat *solver = kissat_init(); kissat_set_option(solver, "quiet", 1);
    for (unsigned i = 0; i != 6; ++i) kissat_add(solver, reverse ? 6-i : i+1);
    kissat_add(solver, 0); kissat_add(solver, 2); kissat_add(solver, 0); kissat_add(solver, -4); kissat_add(solver, 0);
    assert(!kissat_search_propagate(solver));
    for (unsigned stage = 0; stage != 2; ++stage) {
      if (stage) kissat_sparse_collect(solver, true, 0);
      const unsigned ext = reverse ? 4 : 2;
      const unsigned v = IDX(PEEK_STACK(solver->import, ext).lit);
      assert(solver->flags[v].fixed && !solver->flags[v].active);
      printf("kissat\t%u\t%u\t%u\t%u\t%u\n", reverse, stage, ext, solver->links[v].stamp, solver->queue.stamp);
    }
    kissat_release(solver);
  }
}
