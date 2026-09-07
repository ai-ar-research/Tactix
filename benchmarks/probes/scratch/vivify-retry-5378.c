/* Fixed fixture of the actual native full Vivify rule, not a benchmark. */
#include "vivify.c"
#include "kissat.h"
#include "import.h"
#include <stdio.h>

int main(void) {
  kissat *solver = kissat_init();
  for (int i = 1; i <= 4; ++i) kissat_add(solver, i);
  kissat_add(solver, 0);
  kissat_add(solver, 4); kissat_add(solver, -3); kissat_add(solver, 0);
  for (unsigned i = 0; i < solver->vars; ++i) solver->flags[i].eliminate = false;
  solver->probing = true;
  kissat_vivify(solver);
  solver->probing = false;
  unsigned armed = 0;
  for (int e = 1; e <= 4; ++e) {
    const unsigned lit = kissat_import_literal(solver, e);
    armed += solver->flags[IDX(lit)].eliminate;
  }
  clause *c = kissat_dereference_clause(solver, 0);
  const bool ok = !c->garbage && c->size == 3 && armed == 4;
  printf("native size=%u armed=%u\n", c->size, armed);
  kissat_release(solver);
  return ok ? 0 : 1;
}
