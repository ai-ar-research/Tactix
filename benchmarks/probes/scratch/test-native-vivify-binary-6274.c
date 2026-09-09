#define NDEBUG 1
#include "vivify.c"
#undef NDEBUG
#include <assert.h>
int main(void) {
  for (unsigned instantiate = 0; instantiate != 2; ++instantiate) {
    // Native stack macros require this exact local variable name.
    kissat *solver = kissat_init(); kissat_set_option(solver, "quiet", 1);
    kissat_add(solver, 1); kissat_add(solver, 2); kissat_add(solver, 3); kissat_add(solver, 0);
    if (instantiate) {
      kissat_add(solver, -3); kissat_add(solver, 4); kissat_add(solver, 0);
      kissat_add(solver, -3); kissat_add(solver, -4); kissat_add(solver, 0);
    } else { kissat_add(solver, 1); kissat_add(solver, -2); kissat_add(solver, 0); }
    clause *c = kissat_dereference_clause(solver, 0); assert(c->size == 3);
    for (unsigned v = 0; v != 3; ++v) solver->flags[v].eliminate = false;
    PUSH_STACK(solver->clause, 0); PUSH_STACK(solver->clause, instantiate ? 2 : 4);
    vivify_learn_binary(solver, c);
    assert(c->garbage && !solver->inconsistent);
    for (unsigned v = 0; v != 3; ++v) assert(solver->flags[v].eliminate);
    CLEAR_STACK(solver->clause); kissat_release(solver);
  }
  puts("PASS: actual native Vivify binary terminal re-arms all original endpoints (2 cases)");
}
