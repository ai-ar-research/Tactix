#include "forward.c"
#include "dense.h"
#include "eliminate.h"
#include "import.h"
#include "propsearch.h"

static int check(bool incoming) {
  kissat *solver = kissat_init();
  kissat_set_option(solver, "quiet", 1);
  kissat_set_option(solver, "forwardeffort", 0);
  for (unsigned v = 1; v <= 16; ++v) {
    kissat_import_literal(solver, (int)v);
    kissat_activate_literal(solver, LIT(v - 1));
  }
  const int seed[] = {1,2,3,0, -2,3,0, -1,0};
  const int before[] = {3,8,9,0};
  const int after[] = {-3,4,5,0, -4,0, 5,8,9,0};
  for (unsigned i = 0; i < sizeof(seed)/sizeof(*seed); ++i) kissat_add(solver, seed[i]);
  const int *tail = incoming ? before : after;
  const unsigned n = incoming ? sizeof(before)/sizeof(*before) : sizeof(after)/sizeof(*after);
  for (unsigned i = 0; i < n; ++i) kissat_add(solver, tail[i]);
  if (kissat_search_propagate(solver) || VALUE(LIT(2))) return 2;
  kissat_enter_dense_mode(solver, 0);
  const size_t from = solver->propagate - BEGIN_ARRAY(solver->trail);
  if (kissat_forward_subsume_during_elimination(solver) || solver->inconsistent || VALUE(LIT(2)) != 1) return 3;
  kissat_flush_large_connected(solver);
  kissat_connect_irredundant_large_clauses(solver);
  solver->propagate = BEGIN_ARRAY(solver->trail) + from;
  kissat_flush_units_while_connected(solver);
  const size_t expected = incoming ? 0 : 1;
  const size_t eight = SIZE_WATCHES(WATCHES(LIT(7)));
  const size_t nine = SIZE_WATCHES(WATCHES(LIT(8)));
  if (solver->inconsistent || eight != expected || nine != expected) return 4;
  printf("native incoming=%u physical8=%zu physical9=%zu\n", incoming, eight, nine);
  kissat_release(solver);
  return 0;
}
int main(void) { int r = check(true); return r ? r : check(false); }
