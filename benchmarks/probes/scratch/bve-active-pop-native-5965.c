// Fixed actual native elimination caller; not a real-instance solve.
#include "eliminate.c"
#include "import.h"
#include "propsearch.h"
int main(void) {
  for(unsigned sides=1;sides<=3;++sides)for(unsigned negative=0;negative<2;++negative) {
    kissat *solver=kissat_init();kissat_set_option(solver,"quiet",1);
    kissat_set_option(solver,"forward",0);kissat_set_option(solver,"extract",0);
    kissat_set_option(solver,"definitions",0);kissat_set_option(solver,"eliminaterounds",1);
    for(unsigned v=1;v<=4;++v) {kissat_import_literal(solver,v);kissat_activate_literal(solver,LIT(v-1));}
    const int u=negative?-2:2;
    const int binaries[]={1,u,0,-1,u,0};
    for(unsigned i=0;i<sizeof(binaries)/sizeof(*binaries);++i)kissat_add(solver,binaries[i]);
    if(sides&1) {const int c[]={1,u,3,0};for(unsigned i=0;i<4;++i)kissat_add(solver,c[i]);}
    if(sides&2) {const int c[]={-1,u,4,0};for(unsigned i=0;i<4;++i)kissat_add(solver,c[i]);}
    if(kissat_search_propagate(solver))return 2;
    for(unsigned v=0;v<solver->vars;++v)solver->flags[v].eliminate=false;
    solver->flags[0].eliminate=true;solver->statistics.conflicts=1;
    kissat_enter_dense_mode(solver,0);eliminate_variables(solver);
    unsigned retired=0;for(unsigned v=0;v<solver->vars;++v)retired+=solver->flags[v].eliminated;
    if(solver->inconsistent || retired!=1u+!!(sides&1)+!!(sides&2) || solver->statistics.eliminated!=retired ||
       solver->values[negative?NOT(LIT(1)):LIT(1)]!=1)return 3;
    printf("native sides=%u negative=%u eliminated=%" PRIu64 " retired=%u units=%" PRIu64 " PASS\n",
           sides,negative,solver->statistics.eliminated,retired,solver->statistics.units);
    kissat_resume_sparse_mode(solver,true,0);kissat_release(solver);
  }
  return 0;
}
