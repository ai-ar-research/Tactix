#include "forward.c"
#include "dense.h"
#include "eliminate.h"
#include "import.h"
#include "propsearch.h"

int main(void) {
  kissat *solver = kissat_init();
  kissat_set_option(solver, "quiet", 1);
  kissat_set_option(solver, "forwardeffort", 0);
  for (unsigned v = 1; v <= 16; ++v) {
    kissat_import_literal(solver, (int)v);
    kissat_activate_literal(solver, LIT(v - 1));
  }
  const int formula[] = {1,2,3,0, -2,3,0, -1,0, -3,4,5,0, -4,0};
  for (unsigned i = 0; i < sizeof(formula) / sizeof(*formula); ++i)
    kissat_add(solver, formula[i]);
  if (kissat_search_propagate(solver) || VALUE(LIT(2)) || VALUE(LIT(4)))
    return 2;
  kissat_enter_dense_mode(solver, 0);
  const size_t from = solver->propagate - BEGIN_STACK(solver->trail);
  const bool complete = kissat_forward_subsume_during_elimination(solver);
  if (complete || solver->inconsistent || VALUE(LIT(2)) != 1 || VALUE(LIT(4)))
    return 3;
  printf("native partial-forward: unit5=%d complete=%u\n", VALUE(LIT(4)), complete);
  kissat_flush_large_connected(solver);
  kissat_connect_irredundant_large_clauses(solver);
  solver->propagate = BEGIN_STACK(solver->trail) + from;
  kissat_flush_units_while_connected(solver);
  if (solver->inconsistent || VALUE(LIT(4)) != 1)
    return 4;
  puts("PASS: native caller closes unit5 before scheduling BVE");
  kissat_release(solver);
}
