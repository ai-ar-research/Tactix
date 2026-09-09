#define NDEBUG 1
#include "internal.h"
#include "collect.h"
#include "propsearch.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
int main(void) {
  for (unsigned reverse = 0; reverse != 2; ++reverse) {
    kissat *s = kissat_init(); kissat_set_option(s, "quiet", 1);
    for (unsigned i = 0; i != 6; ++i) kissat_add(s, reverse ? 6-i : i+1);
    kissat_add(s, 0); kissat_add(s, 2); kissat_add(s, 0); kissat_add(s, -4); kissat_add(s, 0);
    assert(!kissat_search_propagate(s));
    for (unsigned stage = 0; stage != 2; ++stage) {
      if (stage) kissat_sparse_collect(s, true, 0);
      const unsigned ext = reverse ? 4 : 2;
      const unsigned v = IDX(PEEK_STACK(s->import, ext).lit);
      assert(s->flags[v].fixed && !s->flags[v].active);
      printf("kissat\t%u\t%u\t%u\t%u\t%u\n", reverse, stage, ext, s->links[v].stamp, s->queue.stamp);
    }
    kissat_release(s);
  }
}
