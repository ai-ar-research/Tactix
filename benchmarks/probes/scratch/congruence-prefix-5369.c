/* Fixed native mechanism fixture: interleaved redundant prefix, not a benchmark. */
#include "congruence.c"
#include "kissat.h"
#include "import.h"
#include <stdio.h>

static void add3(kissat *s, int a, int b, int c) {
  kissat_add(s,a); kissat_add(s,b); kissat_add(s,c); kissat_add(s,0);
}
static void fixture_and(kissat *s, int out) {
  kissat_add(s,-out); kissat_add(s,1); kissat_add(s,0);
  kissat_add(s,-out); kissat_add(s,2); kissat_add(s,0);
  add3(s,out,-1,-2);
}
static reference redundant(kissat *solver, int a, int b, int c) {
  const int external[3]={a,b,c};
  for (unsigned i=0;i<3;i++) {
    const unsigned lit=kissat_import_literal(solver,external[i]);
    PUSH_STACK(solver->clause,lit);
  }
  reference r=kissat_new_redundant_clause(solver,1);
  CLEAR_STACK(solver->clause);
  return r;
}
int main(void) {
  kissat *solver=kissat_init();
  fixture_and(solver,3); fixture_and(solver,4);
  reference before=redundant(solver,3,-4,5);
  add3(solver,5,6,7);
  reference after=redundant(solver,3,-4,6);
  solver->probing=true;
  kissat_congruence(solver);
  solver->probing=false;
  bool prefix=kissat_dereference_clause(solver,before)->garbage;
  bool suffix=kissat_dereference_clause(solver,after)->garbage;
  printf("native prefix_deleted=%d suffix_deleted=%d\n",prefix,suffix);
  kissat_release(solver);
  return prefix && !suffix ? 0 : 1;
}
