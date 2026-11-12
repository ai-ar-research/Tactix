#undef NDEBUG
#include "api_test_support.h"
#include "tactix_ipasir.h"
#include "ipasir_test_support.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {void *session;int max,stop,calls,limit;unsigned polls;int body[64][16],sizes[64];} state;
static void learned(void *data,int *clause) {
  state *s=data;assert(s->calls<64);int n=0;
  while(clause[n]) {assert(n<s->max&&n<15);s->body[s->calls][n]=clause[n];++n;}
  s->sizes[s->calls++]=n;
  assert(tactix_ipasir_set_learn_budget(s->session,0)==-1);
  assert(tactix_ipasir_set_strategy(s->session,NULL,NULL)==-1);
  ipasir_set_learn(s->session,NULL,0,NULL); // busy setter must not disconnect it
  clause[0]=INT_MAX;
}
static int terminate(void *data) {state *s=data;++s->polls;return (s->limit>0&&s->polls>=(unsigned)s->limit)||(s->stop&&s->calls);}
static int scripted(void *data,void *session,tactix *view,unsigned cause) {
  (void)data;(void)cause;
  for(unsigned steps=0;steps<10000;++steps) {
    tactix_ipasir_query_state q={0};q.version=1;q.size=sizeof q;assert(!tactix_ipasir_next(session,&q));
    switch(q.action) {
    case TACTIX_QUERY_ASSUME: {
      const int code=tactix_ipasir_assume_next(session);
      if(code==TACTIX_IPASIR_ACTION_INTERRUPTED)return 0;
      assert(!code);break;
    }
    case TACTIX_QUERY_PROPAGATE:assert(tactix_apply(view,rule("Propagate"))>=0);break;
    case TACTIX_QUERY_ANALYZE:assert(tactix_apply_instance(view,rule("ConflictAnalysis"),0)>=0);break;
    case TACTIX_QUERY_BACKTRACK:assert(tactix_apply_instance(view,rule("Restart"),0)>=0);break;
    case TACTIX_QUERY_READY: {
      // Force the F-impossible branch instead of depending on heuristic phase
      // or variable order to happen to produce a conflict.
      if(!tactix_decide(view,-1))break;
      int literal=0;assert(tactix_decide_candidates(view,&literal,1)==1);
      assert(!tactix_decide(view,-literal));break;
    }
    case TACTIX_QUERY_SAT:case TACTIX_QUERY_UNSAT:return q.action;
    case TACTIX_QUERY_RESTORE:case TACTIX_QUERY_INTERRUPTED:return 0;
    default:assert(0);
    }
  }
  assert(0);return 0;
}
static void formula(void *s) {for(unsigned i=0;i<2;++i) {ipasir_add(s,1);ipasir_add(s,i?-2:2);ipasir_add(s,0);}}
static void artifact(void *solver,const char *prefix,unsigned index,int answer,state *s) {
  if(!prefix)return;
  copy_query_bundle(tactix_ipasir_bundle(solver),prefix,index,answer,TEST_CORE_NONE);
  char path[4096];snprintf(path,sizeof path,"%s-%u.learned",prefix,index);FILE *f=fopen(path,"w");assert(f);
  for(int i=0;i<s->calls;++i) {for(int j=0;j<s->sizes[i];++j)fprintf(f,"%d ",s->body[i][j]);fputs("0\n",f);}assert(!fclose(f));
}
static unsigned null_calls;
static void null_state(void *data,int *clause) {assert(!data&&!clause[0]);++null_calls;}
int main(int argc,char **argv) {
  assert(argc==1||argc==2);const char *prefix=argc==2?argv[1]:NULL;
  for(int unsat=0;unsat<2;++unsat) {
    void *s=ipasir_init();assert(s);state calls={0};calls.session=s;calls.max=1;
    formula(s);ipasir_set_learn(s,&calls,1,learned);assert(!tactix_ipasir_set_strategy(s,NULL,scripted));
    if(unsat)ipasir_assume(s,-1);
    assert(ipasir_solve(s)==(unsat?20:10));assert(calls.calls>0);
    for(int i=0;i<calls.calls;++i)assert(calls.sizes[i]==1&&calls.body[i][0]==1);
    artifact(s,prefix,1+unsat,unsat?20:10,&calls);
    const char *published=tactix_ipasir_bundle(s);assert(published);
    ipasir_set_learn(s,NULL,0,NULL);assert(published==tactix_ipasir_bundle(s));
    const int before=calls.calls;assert(ipasir_solve(s)==10&&calls.calls==before);ipasir_release(s);
  }
  void *s=ipasir_init();assert(s);state calls={0};calls.session=s;calls.max=0;
  ipasir_set_learn(s,&calls,0,learned);ipasir_assume(s,9);ipasir_assume(s,-9);
  assert(ipasir_solve(s)==20&&!calls.calls);artifact(s,prefix,3,20,&calls);ipasir_release(s);
  s=ipasir_init();assert(s);memset(&calls,0,sizeof calls);calls.session=s;calls.max=0;
  ipasir_add(s,1);ipasir_add(s,0);ipasir_add(s,-1);ipasir_add(s,0);ipasir_set_learn(s,&calls,0,learned);
  assert(ipasir_solve(s)==20&&calls.calls==1&&!calls.sizes[0]);artifact(s,prefix,4,20,&calls);ipasir_release(s);
  s=ipasir_init();assert(s);formula(s);memset(&calls,0,sizeof calls);calls.session=s;calls.max=1;
  ipasir_set_learn(s,&calls,-1,learned);assert(!tactix_ipasir_set_strategy(s,NULL,scripted));assert(ipasir_solve(s)==10&&!calls.calls);ipasir_release(s);
  s=ipasir_init();assert(s);formula(s);calls.session=s;ipasir_set_learn(s,&calls,1,learned);
  assert(!tactix_ipasir_set_learn_budget(s,0)&&!tactix_ipasir_set_strategy(s,NULL,scripted));
  assert(ipasir_solve(s)==10&&!calls.calls);ipasir_release(s);
  s=ipasir_init();assert(s);ipasir_add(s,1);ipasir_add(s,0);ipasir_add(s,-1);ipasir_add(s,0);
  ipasir_set_learn(s,NULL,0,null_state);assert(ipasir_solve(s)==20&&null_calls==1);ipasir_release(s);
  s=ipasir_init();assert(s);formula(s);memset(&calls,0,sizeof calls);calls.session=s;calls.max=1;calls.stop=1;
  ipasir_set_learn(s,&calls,1,learned);ipasir_set_terminate(s,&calls,terminate);
  assert(!tactix_ipasir_set_strategy(s,NULL,scripted));ipasir_assume(s,-1);
  assert(ipasir_solve(s)==0&&calls.calls&&!tactix_ipasir_bundle(s));
  ipasir_set_terminate(s,NULL,NULL);ipasir_set_learn(s,NULL,0,NULL);assert(ipasir_solve(s)==10);ipasir_release(s);
  // Probe every termination boundary around a manual driver's startup UNSAT,
  // including export work inside final capture after its initial stop poll.
  for(int limit=1;limit<=32;++limit) {
    s=ipasir_init();assert(s);memset(&calls,0,sizeof calls);calls.session=s;calls.limit=limit;
    ipasir_add(s,1);ipasir_add(s,0);ipasir_add(s,-1);ipasir_add(s,0);
    ipasir_set_learn(s,&calls,0,learned);ipasir_set_terminate(s,&calls,terminate);
    assert(!tactix_ipasir_set_strategy(s,NULL,scripted));const int answer=ipasir_solve(s);
    assert(answer==0||answer==20);assert(calls.polls<(unsigned)limit||answer==0);
    if(!answer)assert(!tactix_ipasir_bundle(s));
    ipasir_release(s);
  }
  puts("IPASIR learning: global units/empty clauses, assumptions, size/budget/disconnect/null-state, scratch writes and interruption pass");
}
