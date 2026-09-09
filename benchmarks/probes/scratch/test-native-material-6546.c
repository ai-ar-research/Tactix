/* Fixed reference contract, not a solver run or a simulated forward pass. */
#define NDEBUG 1
#include "internal.h"
#include "inline.h"
#include "eliminate.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static unsigned checks;
static void check(kissat *solver, unsigned material, unsigned factor,
                  bool admitted) {
  assert(solver->statistics.variables_subsume == material);
  assert(solver->statistics.literals_factor == factor);
  assert(solver->statistics.variables_eliminate == 0);
  assert(kissat_eliminating(solver) == admitted);
  ++checks;
}

int main(void) {
  kissat storage = {0}, *solver = &storage;
  flags variables[2] = {{0}, {0}};
  solver->flags = variables;
  solver->enabled.eliminate = true;
  solver->statistics.clauses_irredundant = 1;
  solver->statistics.conflicts = 9;
  solver->last.conflicts.reduce = 8;
  solver->limits.eliminate.conflicts = 9;
  check(solver, 0, 0, false);
  kissat_mark_added_literal(solver, 0);
  check(solver, 1, 1, true);
  solver->limits.eliminate.variables.subsume = 1;
  kissat_mark_added_literal(solver, 0);
  check(solver, 1, 1, false);
  kissat_mark_added_literal(solver, 1);
  check(solver, 1, 2, false); /* opposite sign is not a new variable */

  /* Model the source-reviewed consumer's reset; do not claim to call it. */
  variables[0].subsume = false;
  check(solver, 1, 2, false); /* reset does not reset the cumulative counter */
  kissat_mark_added_literal(solver, 1);
  check(solver, 2, 2, true);
  solver->limits.eliminate.variables.subsume = 2;
  variables[0].subsume = false;
  variables[0].subsume = true; /* forward tail rearm, intentionally no increment */
  kissat_mark_added_literal(solver, 0);
  check(solver, 2, 2, false);
  kissat_mark_added_literal(solver, 2);
  check(solver, 3, 3, true);

  solver->limits.eliminate.variables.subsume = 3;
  variables[1].subsume = false;
  variables[1].fixed = true;
  kissat_mark_added_literal(solver, 3);
  check(solver, 4, 4, true); /* unlike removal marking, no fixed-variable filter */
  solver->enabled.eliminate = false;
  check(solver, 4, 4, false);
  solver->enabled.eliminate = true;
  solver->statistics.clauses_irredundant = 0;
  check(solver, 4, 4, false);
  solver->statistics.clauses_irredundant = 1;
  solver->last.conflicts.reduce = 9;
  check(solver, 4, 4, false);
  solver->last.conflicts.reduce = 8;
  solver->limits.eliminate.conflicts = 10;
  check(solver, 4, 4, false);
  solver->limits.eliminate.conflicts = 9;
  check(solver, 4, 4, true);
  printf("PASS native material producer and admission: %u checks; consumer reset/rearm are explicit fixture setup\n", checks);
  return 0;
}
