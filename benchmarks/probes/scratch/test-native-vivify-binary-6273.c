// Include this translation unit to exercise its actual private terminal.
#define NDEBUG 1
#include "vivify.c"
#undef NDEBUG
#include <assert.h>
int main(void) {
  for (unsigned instantiate = 0; instantiate != 2; ++instantiate) {
    kissat *s = kissat_init(); kissat_set_option(s, "quiet", 1);
    kissat_add(s, 1); kissat_add(s, 2); kissat_add(s, 3); kissat_add(s, 0);
    if (instantiate) {
      kissat_add(s, -3); kissat_add(s, 4); kissat_add(s, 0);
      kissat_add(s, -3); kissat_add(s, -4); kissat_add(s, 0);
    } else { kissat_add(s, 1); kissat_add(s, -2); kissat_add(s, 0); }
    clause *c = kissat_dereference_clause(s, 0); assert(c->size == 3);
    for (unsigned v = 0; v != 3; ++v) s->flags[v].eliminate = false;
    PUSH_STACK(s->clause, 0); PUSH_STACK(s->clause, instantiate ? 2 : 4);
    vivify_learn_binary(s, c);
    assert(c->garbage && !s->inconsistent);
    for (unsigned v = 0; v != 3; ++v) assert(s->flags[v].eliminate);
    CLEAR_STACK(s->clause); kissat_release(s);
  }
  puts("PASS: actual native Vivify binary terminal re-arms all original endpoints (2 cases)");
}
