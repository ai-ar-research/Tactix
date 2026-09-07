/* Actual native kernel uses one connected occurrence per literal in dense mode. */
#include "internal.h"
#include "inlinevector.h"
#include "dense.h"
#include "import.h"
#include "kissat.h"
#include <stdio.h>
int main(void) {
  kissat *solver = kissat_init();
  unsigned lits[3];
  for (int e = 1; e <= 3; ++e) lits[e - 1] = kissat_import_literal(solver, e);
  kissat_enter_dense_mode(solver, 0);
  for (unsigned i = 0; i != 3; ++i) PUSH_STACK(solver->clause, lits[i]);
  const reference ref = kissat_new_irredundant_clause(solver);
  CLEAR_STACK(solver->clause);
  unsigned entries = 0;
  bool exact = !solver->watching;
  for (unsigned i = 0; i != 3; ++i) {
    watches *w = solver->watches + lits[i];
    entries += SIZE_WATCHES(*w);
    exact &= SIZE_WATCHES(*w) == 1 && BEGIN_WATCHES(*w)->large.ref == ref;
  }
  printf("native connected=%u dense_entries=%u\n", solver->watching, entries);
  kissat_release(solver);
  return exact && entries == 3 ? 0 : 1;
}
