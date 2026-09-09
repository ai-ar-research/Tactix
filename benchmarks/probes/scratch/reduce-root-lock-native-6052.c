// Fixed actual Kissat caller: root flush versus non-root reason protection.
#include "reduce.c"
#include "decide.h"
#include "import.h"
#include "inlineframes.h"
#include "propsearch.h"
int main(void) {
  for (unsigned mode=0;mode<3;++mode) {
    kissat *solver=kissat_init(); kissat_set_option(solver,"quiet",1);
    for (unsigned v=1;v<=7;++v) {
      kissat_import_literal(solver,v); kissat_activate_literal(solver,LIT(v-1));
    }
    const int clauses[]={1,2,3,4,0,1,2,3,-4,0};
    for(unsigned i=0;i<sizeof clauses/sizeof *clauses;++i) kissat_add(solver,clauses[i]);
    CLEAR_STACK(solver->clause);
    for(unsigned v=0;v<3;++v) PUSH_STACK(solver->clause,LIT(v));
    const reference ref=kissat_new_redundant_clause(solver,3);
    CLEAR_STACK(solver->clause);
    clause *c=kissat_dereference_clause(solver,ref); c->used=3;
    solver->tier1[0]=2; solver->tier2[1]=6;
    if(EMPTY_STACK(solver->frames)) kissat_push_frame(solver,UINT_MAX);
    if(mode<2) {
      kissat_add(solver,-1);kissat_add(solver,0);
      kissat_add(solver,-2);kissat_add(solver,0);
    } else {
      kissat_internal_assume(solver,NOT(LIT(0)));
      if(kissat_search_propagate(solver)) return 2;
      kissat_internal_assume(solver,NOT(LIT(1)));
    }
    if(kissat_search_propagate(solver)) return 3;
    if(solver->values[LIT(2)]!=1) return 4;
    if(mode==1) kissat_internal_assume(solver,LIT(6));
    if(kissat_search_propagate(solver)) return 5;
    if(!kissat_flush_and_mark_reason_clauses(solver,0)) return 6;
    reducibles reds; INIT_STACK(reds);
    const bool found=collect_reducibles(solver,&reds,0);
    const size_t expected=mode<2?1:0;
    if(SIZE_STACK(reds)!=expected || found!=(bool)expected) return 7;
    if(expected && PEEK_STACK(reds,0).ref!=ref) return 8;
    printf("native mode=%u candidates=%zu root=%u reason=%u PASS\n",mode,SIZE_STACK(reds),
           solver->assigned[2].level==0,kissat_dereference_clause(solver,ref)->reason);
    RELEASE_STACK(reds);kissat_unmark_reason_clauses(solver,0);kissat_release(solver);
  }
  return 0;
}
