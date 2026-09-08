#include "forward.c"
#include "dense.h"
#include "import.h"
#include "assign.h"
#include "propsearch.h"
#include "forward-unit-cases-5802.h"

int main(void) {
  for (unsigned test = 0; test < 6; ++test) {
    kissat *solver = kissat_init();
    kissat_set_option(solver, "quiet", 1);
    for (unsigned v = 1; v <= 16; ++v) {
      kissat_import_literal(solver, (int)v);
      kissat_activate_literal(solver, LIT(v-1));
    }
    for (unsigned i = 0; i < lengths[test]; ++i)
      kissat_add(solver, cases[test][i]);
    if (kissat_search_propagate(solver) || VALUE(LIT(2))) return 2;
    const size_t entry = SIZE_STACK(solver->trail);
    kissat_enter_dense_mode(solver, 0);
    const bool complete = kissat_forward_subsume_during_elimination(solver);
    printf("CASE %u complete=%u steps=%llu subsumed=%llu strengthened=%llu trail=",
           test, complete, (unsigned long long)solver->statistics.forward_steps,
           (unsigned long long)solver->statistics.subsumed,
           (unsigned long long)solver->statistics.strengthened);
    for (size_t i = entry; i < SIZE_STACK(solver->trail); ++i) {
      const unsigned lit = PEEK_STACK(solver->trail, i);
      printf("%d,", (lit & 1) ? -(int)(IDX(lit)+1) : (int)(IDX(lit)+1));
    }
    printf(" values=");
    for (unsigned v = 0; v < 16; ++v) printf("%d,", VALUE(LIT(v)));
    puts("");
    if (solver->inconsistent || VALUE(LIT(2)) != 1) return 3;
    kissat_release(solver);
  }
}
