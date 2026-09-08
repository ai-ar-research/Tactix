#include "internal.h"
#include "assign.h"
#include "bump.h"
#include "decide.h"
#include "import.h"
#include "inline.h"
#include "inlineheap.h"
#include "propsearch.h"
#include "trail.h"

int main(void) {
  kissat *solver = kissat_init();
  kissat_set_option(solver, "quiet", 1);
  for (unsigned v = 1; v <= 8; ++v) {
    kissat_import_literal(solver, (int)v);
    kissat_activate_literal(solver, LIT(v - 1));
  }
  const int clauses[] = {-1,3,4,0, 1,2,0};
  for (unsigned i=0;i<sizeof(clauses)/sizeof(*clauses);++i) kissat_add(solver,clauses[i]);
  solver->stable = true;
  kissat_update_scores(solver);
  kissat_internal_assume(solver, NOT(LIT(0)));
  if (kissat_search_propagate(solver) || VALUE(LIT(1)) != 1 || LEVEL(LIT(1)) != 1) return 2;
  kissat_internal_assume(solver,LIT(4));
  kissat_internal_assume(solver,LIT(5));
  kissat_original_unit(solver,NOT(LIT(2)));
  kissat_original_unit(solver,NOT(LIT(3)));
  if (kissat_search_propagate(solver) || !solver->unflushed) return 3;
  if (!kissat_flush_and_mark_reason_clauses(solver,0) || solver->inconsistent || solver->level) return 4;
  if (VALUE(NOT(LIT(0))) != 1 || VALUE(LIT(1)) != 1) return 5;
  if (ACTIVE(0) || ACTIVE(1) || kissat_heap_contains(SCORES,0) || kissat_heap_contains(SCORES,1)) return 6;
  if (!ACTIVE(4) || !ACTIVE(5) || VALUE(LIT(4)) || VALUE(LIT(5))) return 7;
  puts("PASS: actual native Reduce preparation removes two new roots from guidance after retained-trail backtrack");
  kissat_release(solver);
  return 0;
}
