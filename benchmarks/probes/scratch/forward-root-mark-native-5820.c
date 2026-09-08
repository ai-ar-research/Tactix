#include "forward.c"
#include "dense.h"
#include "import.h"
#include "assign.h"
#include "propsearch.h"
int main(void) {
  kissat *solver = kissat_init();
  kissat_set_option(solver,"quiet",1);
  for (unsigned v = 1; v <= 3; ++v) {
    kissat_import_literal(solver,(int)v);
    kissat_activate_literal(solver,LIT(v-1));
  }
  const int formula[] = {1,2,3,0,2,3,0};
  for (unsigned i = 0; i < sizeof(formula)/sizeof(*formula); ++i)
    kissat_add(solver,formula[i]);
  for (unsigned i = 0; i < 3; ++i) solver->flags[i].subsume = false;
  solver->flags[2].subsume = true;
  kissat_add(solver,-1); kissat_add(solver,0);
  if (kissat_search_propagate(solver) || solver->flags[0].subsume) return 2;
  kissat_enter_dense_mode(solver,0);
  kissat_forward_subsume_during_elimination(solver);
  if (solver->inconsistent || solver->statistics.subsumed ||
      kissat_dereference_clause(solver,0)->garbage || VALUE(LIT(0)) != -1) return 3;
  puts("NATIVE root assignment adds no admission mark;subsumed=0");
  kissat_release(solver);
}
