// Invoke the actual pinned native forward pass, not a mirror implementation.
#include "forward.c"
#include "dense.h"
#include "import.h"
#include "assign.h"
#include "propsearch.h"
int main(void) {
  for (unsigned tail_size=3;tail_size<=4;tail_size++) {
    kissat *solver=kissat_init();
    kissat_set_option(solver,"quiet",1);
    for (unsigned v=1;v<=7;v++) {
      kissat_import_literal(solver,(int)v);
      kissat_activate_literal(solver,LIT(v-1));
    }
    const int first[]={1,2,3,0}, tail[]={4,5,6,7};
    for(unsigned i=0;i<4;i++) kissat_add(solver,first[i]);
    for(unsigned i=0;i<tail_size;i++) kissat_add(solver,tail[i]);
    kissat_add(solver,0);
    const reference tail_ref=kissat_reference_clause(solver,kissat_last_irredundant_clause(solver));
    kissat_add(solver,-2);kissat_add(solver,3);kissat_add(solver,0);
    kissat_add(solver,4);kissat_add(solver,5);kissat_add(solver,0);
    kissat_add(solver,-1);kissat_add(solver,0);
    if (kissat_search_propagate(solver)) return 2;
    if (VALUE(LIT(2))) return 3;
    // Eliminate retains binary watches; passing a save-list would remove them.
    kissat_enter_dense_mode(solver,0);
    printf("ENTRY tail=%u first_size=%u first_garbage=%u flags=%u,%u,%u steps=%llu\n",tail_size,
        kissat_dereference_clause(solver,0)->size,kissat_dereference_clause(solver,0)->garbage,
        solver->flags[0].subsume,solver->flags[1].subsume,solver->flags[2].subsume,
        (unsigned long long)solver->statistics.forward_steps);
    kissat_forward_subsume_during_elimination(solver);
    const unsigned removed=kissat_dereference_clause(solver,tail_ref)->garbage;
    if (VALUE(LIT(2)) != 1 || !removed) {
      printf("MISMATCH value3=%d tail_removed=%u steps=%llu\n",VALUE(LIT(2)),removed,
          (unsigned long long)solver->statistics.forward_steps);
      return 4;
    }
    printf("NATIVE tail=%u forced3=%d subsumed=%llu\n",tail_size,VALUE(LIT(2)),
        (unsigned long long)removed);
    kissat_release(solver);
  }
  return 0;
}
