// Match Release internal-structure ABI before enabling fixture assertions.
#define NDEBUG 1
#include "internal.h"
#include "substitute.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
int main(void) {
  for (unsigned copies = 1; copies <= 3; ++copies) {
    kissat *solver = kissat_init();
    kissat_set_option(solver, "quiet", 1);
    kissat_set_option(solver, "substituterounds", 1);
    for (unsigned i = 0; i < copies; ++i) {
      kissat_add(solver, -1); kissat_add(solver, 2); kissat_add(solver, 0);
      kissat_add(solver, 1); kissat_add(solver, -2); kissat_add(solver, 0);
    }
    kissat_add(solver, 2); kissat_add(solver, 3); kissat_add(solver, 0);
    kissat_add(solver, 3); kissat_add(solver, 4); kissat_add(solver, 0);
    assert(solver->vars == 4);
    for (unsigned i = 0; i < solver->vars; ++i) solver->flags[i].eliminate = false;
    const uint64_t before = solver->statistics.variables_eliminate;
    solver->probing = true;
    kissat_substitute(solver, false);
    assert(!solver->inconsistent);
    assert(solver->flags[0].eliminate && solver->flags[1].eliminate);
    assert(!solver->flags[2].eliminate && !solver->flags[3].eliminate);
    assert(solver->statistics.variables_eliminate - before == 2);
    assert(!solver->values[0] && !solver->values[2]);
    solver->probing = false;
    kissat_release(solver);
  }
  puts("PASS: actual Kissat Substitute re-arms disappeared binary endpoints only (3 duplicate cases)");
}
