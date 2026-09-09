#define NDEBUG 1
#include "internal.h"
#include "collect.h"
#include "propsearch.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>

int main(void) {
  unsigned cases = 0;
  const unsigned stamps[] = {11u, 73u, UINT_MAX};
  for (unsigned reverse = 0; reverse != 2; ++reverse)
    for (unsigned stable = 0; stable != 2; ++stable)
      for (unsigned j = 0; j != 3; ++j) {
        kissat *s = kissat_init();
        kissat_set_option(s, "quiet", 1);
        for (unsigned i = 0; i != 6; ++i)
          kissat_add(s, reverse ? 6 - i : i + 1);
        kissat_add(s, 0);
        const unsigned first = reverse ? 4 : 2, other = reverse ? 2 : 4;
        const unsigned idx = IDX(PEEK_STACK(s->import, first).lit);
        const unsigned other_idx = IDX(PEEK_STACK(s->import, other).lit);
        const unsigned stamp = stamps[j];
        s->links[idx].stamp = stamp;
        s->links[other_idx].stamp = stamp ^ 0x55555555u;
        kissat_add(s, 2); kissat_add(s, 0);
        kissat_add(s, -4); kissat_add(s, 0);
        assert(s->links[idx].stamp == stamp);
        s->stable = stable;
        assert(!kissat_search_propagate(s));
        kissat_sparse_collect(s, true, 0);
        const unsigned mapped = IDX(PEEK_STACK(s->import, first).lit);
        assert(mapped == IDX(PEEK_STACK(s->import, other).lit));
        assert(s->flags[mapped].fixed && !s->flags[mapped].active);
        assert(s->links[mapped].stamp == stamp);
        assert(s->vars == 5 && s->active == 4);
        unsigned count = 0, previous = UINT_MAX;
        for (unsigned v = s->queue.first; v != UINT_MAX; v = s->links[v].next) {
          assert(v != mapped && count < 4);
          assert(s->links[v].prev == previous);
          assert(s->links[v].stamp == ++count);
          previous = v;
        }
        assert(count == 4 && previous == s->queue.last);
        printf("native reverse=%u stable=%u stamp=%u retained=%u active=%u\n",
               reverse, stable, stamp, s->links[mapped].stamp, count);
        kissat_release(s);
        ++cases;
      }
  printf("PASS actual native compaction cases=%u\n", cases);
}
