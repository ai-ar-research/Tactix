#undef NDEBUG
#include "tactix_ipasir.h"
#include "ipasir_test_support.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void artifact(void *s,const char *prefix,unsigned q,int answer) {
  copy_query_bundle(tactix_ipasir_bundle(s),prefix,q,answer,TEST_CORE_NONE);
}
static void solve(void *s,const char *prefix,unsigned q,int answer) {
  const int actual=ipasir_solve(s);
  if(actual!=answer)fprintf(stderr,"query %u: got %d expected %d\n",q,actual,answer);
  assert(actual==answer);artifact(s,prefix,q,answer);
}
/* BCE cleanup used to contradict A, then repeat after
 * every repair. Bound cancellation polls so the old solver fails this fixture
 * without relying on a wall-clock timeout. All models are checked against F/A. */
static int query_polls(void *state) {return ++*(unsigned *)state>1500;}
static void query_stabilizer_regression(void) {
  const int clauses[][4]={{1,2,3,0},{4,5,6,0},{-1,-4,0},{-2,-5,0},
                          {-3,-6,0},{-3,-9,0}};
  const int assumptions[]={1,-1,2,-2,3,-3,4,-4,5,-5,6,-6,9,-9,0};
  const char *strategies[]={"default","cruxmiter","chvatal","scripted-default"};
  for(unsigned strategy=0;strategy<sizeof strategies/sizeof *strategies;++strategy) {
    void *session=ipasir_init();assert(session);
    assert(!tactix_ipasir_set_strategy_name(session,strategies[strategy]));
    for(unsigned i=0;i<sizeof clauses/sizeof *clauses;++i) {
      for(unsigned j=0;clauses[i][j];++j)ipasir_add(session,clauses[i][j]);
      ipasir_add(session,0);
    }
    unsigned polls=0;ipasir_set_terminate(session,&polls,query_polls);
    for(unsigned q=0;q<sizeof assumptions/sizeof *assumptions;++q) {
      polls=0;
      if(assumptions[q])ipasir_assume(session,assumptions[q]);
      const int answer=ipasir_solve(session);
      if(answer!=10)fprintf(stderr,"query stabilizer: strategy=%s assumption=%d result=%d polls=%u\n",strategies[strategy],assumptions[q],answer,polls);
      assert(answer==10&&"generated query domain must preserve assumptions");
      if(assumptions[q])assert(ipasir_val(session,assumptions[q])==assumptions[q]);
      for(unsigned i=0;i<sizeof clauses/sizeof *clauses;++i) {
        int satisfied=0;
        for(unsigned j=0;clauses[i][j];++j)
          satisfied|=ipasir_val(session,clauses[i][j])==clauses[i][j];
        assert(satisfied);
      }
    }
    ipasir_release(session);
  }
  puts("Query stabilizer: 60 checked SAT models across four strategies");
}
int main(int argc,char **argv) {
  assert(argc==1||argc==2);const char *prefix=argc==2?argv[1]:NULL;
  assert(ipasir_signature()&&strstr(ipasir_signature(),"tactix"));
  void *s=ipasir_init(),*other=ipasir_init();assert(s&&other&&s!=other);
  assert(!ipasir_val(s,1)&&!ipasir_failed(s,1)&&!tactix_ipasir_bundle(s));
  ipasir_add(s,1);ipasir_add(s,2);ipasir_add(s,0);
  ipasir_assume(s,-1);solve(s,prefix,1,10);
  assert(ipasir_val(s,1)==-1&&ipasir_val(s,-1)==-1&&ipasir_val(s,2)==2);
  assert(!ipasir_failed(s,-1));
  ipasir_assume(s,-2);assert(!ipasir_val(s,1)&&!tactix_ipasir_bundle(s));
  solve(s,prefix,2,10);assert(ipasir_val(s,1)==1&&ipasir_val(s,-2)==-2);
  ipasir_assume(s,7);ipasir_assume(s,-1);ipasir_assume(s,-2);
  solve(s,prefix,3,20);
  assert(ipasir_failed(s,-1)&&ipasir_failed(s,-2)&&!ipasir_failed(s,1));
  assert(!ipasir_val(s,2));
  solve(s,prefix,4,10); // the last query's assumption cube has been consumed
  ipasir_add(s,3);assert(!tactix_ipasir_bundle(s));ipasir_add(s,0);ipasir_assume(s,-3);
  solve(s,prefix,5,20);assert(ipasir_failed(s,-3)&&!ipasir_failed(s,-1));
  solve(s,prefix,6,10);assert(ipasir_val(s,3)==3);
  ipasir_add(s,0);solve(s,prefix,7,20);assert(!ipasir_failed(s,-3));
  // Session isolation and contradictory assumptions outside permanent F.
  ipasir_assume(other,9);ipasir_assume(other,-9);
  solve(other,prefix,8,20);assert(ipasir_failed(other,9)&&ipasir_failed(other,-9));
  solve(other,prefix,9,10);assert(!ipasir_val(other,9)&&!ipasir_failed(other,9));
  char owned[4096];assert(snprintf(owned,sizeof owned,"%s.json",tactix_ipasir_bundle(s))>0);
  ipasir_release(s);assert(!fopen(owned,"rb"));ipasir_release(other);
  query_stabilizer_regression();
  puts("IPASIR query C client: nine queries, isolated sessions and result lifetimes pass");
}
