// Actual Kissat caller; successor to the preserved6732 compile-only failure.
#include "reduce.c"
#include "import.h"
#include "inlineframes.h"
static reference add(kissat *solver, unsigned tag, bool red) {
  const unsigned vars[] = {1,tag,20,21,22};
  if (!red) {
    for (unsigned i=0;i<5;++i) kissat_add(solver,vars[i]);
    kissat_add(solver,0);
    return solver->last_irredundant;
  }
  CLEAR_STACK(solver->clause);
  for (unsigned i=0;i<5;++i) PUSH_STACK(solver->clause,LIT(vars[i]-1));
  reference ref=kissat_new_redundant_clause(solver,9);
  CLEAR_STACK(solver->clause);
  return ref;
}
static clause *find(kissat *solver,unsigned tag) {
  for (all_clauses(c)) if (!c->garbage)
    for (unsigned i=0;i<c->size;++i) if(c->lits[i]==LIT(tag-1)) return c;
  return 0;
}
int main(void) {
  for (unsigned mode=0;mode<3;++mode) {
    kissat *solver=kissat_init(); kissat_set_option(solver,"quiet",1);
    kissat_set_option(solver,"compact",0);
    for (unsigned v=1;v<=26;++v) {
      kissat_import_literal(solver,v); kissat_activate_literal(solver,LIT(v-1));
    }
    add(solver,2,false);
    reference origin=add(solver,3,mode!=2);
    add(solver,4,false); add(solver,5,true); add(solver,6,true);
    if(mode!=2) {
      clause *c=kissat_dereference_clause(solver,origin);
      c->redundant=false;
      solver->statistics.clauses_redundant--;
      solver->statistics.clauses_irredundant++;
      kissat_update_last_irredundant(solver,c);
      if(mode==1) kissat_mark_clause_as_garbage(solver,c);
    }
    find(solver,2)->searched=4; find(solver,4)->searched=4;
    if(EMPTY_STACK(solver->frames)) kissat_push_frame(solver,UINT_MAX);
    if(kissat_reduce(solver)) return 2;
    unsigned target=find(solver,4)->searched, prefix=find(solver,2)->searched;
    if(target!=(mode==2?4:2) || prefix!=4) return 3;
    if(solver->statistics.clauses_redundant!=1) return 4;
    printf("native mode=%u target=%u prefix=%u surviving-redundant=1 PASS\n",mode,target,prefix);
    kissat_release(solver);
  }
  return 0;
}
