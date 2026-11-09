#undef NDEBUG
#include "api_test_support.h"
#include "tactix_ipasir.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {unsigned calls,restored,steps;int preprocess;} driver_state;
static tactix_ipasir_query_state next(void *s) {
  tactix_ipasir_query_state q={0};q.version=TACTIX_IPASIR_SESSION_VERSION;q.size=sizeof q;
  assert(tactix_ipasir_next(s,&q)==0);return q;
}
static int run_script(void *session,tactix *view,driver_state *state) {
  for(unsigned i=0;i<10000;++i) {
    ++state->steps;
    const tactix_ipasir_query_state q=next(session);
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
      int literal=0;assert(tactix_decide_candidates(view,&literal,1)==1);
      assert(tactix_decide(view,literal)==0);break;
    }
    case TACTIX_QUERY_SAT:case TACTIX_QUERY_UNSAT:return q.action;
    case TACTIX_QUERY_RESTORE:case TACTIX_QUERY_INTERRUPTED:return 0;
    default:assert(!"unexpected programmable-query boundary");
    }
  }
  assert(!"script did not terminate");return 0;
}
static int scripted(void *data,void *session,tactix *view,unsigned cause) {
  driver_state *state=data;++state->calls;
  assert(cause==TACTIX_IPASIR_ENTER_QUERY || cause==TACTIX_IPASIR_ENTER_RESTORED);
  state->restored+=cause==TACTIX_IPASIR_ENTER_RESTORED;
  assert(tactix_ipasir_set_strategy(session,NULL,NULL)==-1);
  assert(tactix_ipasir_finish(session,0)==-1); // the blocking driver owns completion
  if(state->preprocess && cause==TACTIX_IPASIR_ENTER_QUERY) {
    assert(tactix_apply_instance(view,rule("BVE"),(uint64_t)(uint32_t)1)>=0);
    assert(next(session).action==TACTIX_QUERY_RESTORE);return 0;
  }
  return run_script(session,view,state);
}
static int delegate(void *data,void *session,tactix *view,unsigned cause) {
  driver_state *state=data;++state->calls;(void)session;(void)cause;
  assert(tactix_set_default_option(view,"TACTIX_LUCKY_LATE","0")==0);
  return tactix_solve(view);
}
typedef struct {uint64_t conflicts,sweep_ticks;} preflight_state;
static int preflight_delegate(void *data,void *session,tactix *view,unsigned cause) {
  preflight_state *state=data;(void)session;(void)cause;
  const int result=tactix_solve(view);
  state->conflicts=tactix_stat(view,TACTIX_STAT_CONFLICTS);
  state->sweep_ticks=tactix_stat(view,TACTIX_STAT_SWEEP_TICKS);
  return result;
}
static void artifact(void *s,const char *prefix,unsigned index,int answer,int reduced) {
  if(!prefix)return;
  const char *source=tactix_ipasir_bundle(s);assert(source);
  const char *suffix[]={".cnf",".proof",".json",answer==10?".model":".core","-core.cnf","-core.proof"};
  for(unsigned i=0;i<(reduced?6u:4u);++i) {
    char a[4096],b[4096];snprintf(a,sizeof a,"%s%s",source,suffix[i]);
    snprintf(b,sizeof b,"%s-%u%s",prefix,index,suffix[i]);
    FILE *in=fopen(a,"rb"),*out=fopen(b,"wb");assert(in&&out);char buffer[4096];size_t n;
    while((n=fread(buffer,1,sizeof buffer,in)))assert(fwrite(buffer,1,n,out)==n);
    assert(!ferror(in)&&!fclose(in)&&!fclose(out));
  }
}
static void call(void *s,int expected,const char *prefix,unsigned index) {
  const int result=ipasir_solve(s);if(result!=expected)fprintf(stderr,"query %u result %d expected %d\n",index,result,expected);
  assert(result==expected);artifact(s,prefix,index,result,0);
}
static int stop_flag(void *state) {return *(int *)state;}
static void interrupt_between_boundary_and_action(void) {
  void *s=ipasir_init();assert(s);int stop=0;tactix *view=NULL;
  ipasir_set_terminate(s,&stop,stop_flag);ipasir_assume(s,5);
  assert(tactix_ipasir_begin(s,&view)==1&&next(s).action==TACTIX_QUERY_ASSUME);
  stop=1;
  assert(tactix_ipasir_assume_next(s)==TACTIX_IPASIR_ACTION_INTERRUPTED);
  assert(next(s).action==TACTIX_QUERY_INTERRUPTED);
  assert(tactix_ipasir_finish(s,0)==0&&!tactix_ipasir_result_acquire(s));
  ipasir_set_terminate(s,NULL,NULL);assert(ipasir_solve(s)==10&&!ipasir_val(s,5));
  ipasir_release(s);
}
typedef struct {unsigned polls,limit;} poll_state;
static int stop_on_poll(void *state) {
  poll_state *p=state;return ++p->polls>=p->limit;
}
static void interrupt_inside_boundary_observation(void) {
  void *s=ipasir_init();assert(s);tactix *view=NULL;
  poll_state p={0,UINT32_MAX};ipasir_set_terminate(s,&p,stop_on_poll);
  assert(tactix_ipasir_begin(s,&view)==1);
  p.polls=0;p.limit=2; // Outer next() poll passes; its status() poll stops.
  assert(next(s).action==TACTIX_QUERY_INTERRUPTED&&p.polls==2);
  assert(!tactix_ipasir_finish(s,0)&&!tactix_ipasir_bundle(s));
  ipasir_set_terminate(s,NULL,NULL);assert(ipasir_solve(s)==10);ipasir_release(s);
}
static void rejected_model_observation(void) {
  assert(tactix_ipasir_rejected_models(NULL)==UINT64_MAX);
  void *s=ipasir_init();assert(s);
  ipasir_add(s,1);ipasir_add(s,2);ipasir_add(s,0);
  assert(tactix_ipasir_rejected_models(s)==UINT64_MAX);
  for(unsigned i=0;i<2;++i) {
    ipasir_assume(s,-1);tactix *view=NULL;
    assert(tactix_ipasir_begin(s,&view)==1&&!tactix_ipasir_rejected_models(s));
    assert(tactix_apply_instance(view,rule("Lucky"),0)==0); // all true misses A
    const uint64_t count=tactix_ipasir_rejected_models(s);assert(count&&count!=UINT64_MAX);
    assert(tactix_ipasir_rejected_models(s)==count);
    assert(tactix_apply_instance(view,rule("Restart"),0)>=0);
    assert(tactix_ipasir_rejected_models(s)==count);
    driver_state state={0};assert(run_script(s,view,&state)==10);
    assert(tactix_ipasir_finish(s,10)==10&&ipasir_val(s,1)==-1&&ipasir_val(s,2)==2);
    assert(tactix_ipasir_rejected_models(s)==UINT64_MAX);
  }
  ipasir_release(s);
}
int main(int argc,char **argv) {
  assert(argc==1||argc==2);const char *prefix=argc==2?argv[1]:NULL;
  interrupt_between_boundary_and_action();
  interrupt_inside_boundary_observation();
  rejected_model_observation();
  void *s=ipasir_init();assert(s);driver_state state={0};
  ipasir_add(s,1);ipasir_add(s,2);ipasir_add(s,0);
  assert(tactix_ipasir_set_default_option(s,"bad",NULL)==-1);
  assert(tactix_ipasir_set_default_option(s,"TACTIX_LUCKY_LATE","0")==0);
  assert(tactix_ipasir_set_strategy(s,&state,scripted)==0);
  ipasir_assume(s,-1);call(s,10,prefix,1);assert(ipasir_val(s,1)==-1&&ipasir_val(s,2)==2);
  ipasir_assume(s,-1);ipasir_assume(s,-2);call(s,20,prefix,2);
  assert(ipasir_failed(s,-1)&&ipasir_failed(s,-2));
  assert(tactix_ipasir_set_strategy(s,&state,delegate)==0);
  call(s,10,prefix,3);
  ipasir_add(s,-2);ipasir_add(s,0);ipasir_assume(s,-1);call(s,20,prefix,4);
  assert(state.calls>=4&&state.steps>0);
  assert(tactix_ipasir_set_strategy(s,NULL,NULL)==0);call(s,10,prefix,5);
  // Manual stepping uses the same owner, and invalid terminal claims do not
  // consume the query or manufacture an UNSAT result.
  ipasir_assume(s,-1);tactix *view=NULL,*other=NULL;
  assert(tactix_ipasir_begin(s,&view)==1&&view);
  assert(tactix_ipasir_begin(s,&other)==-1&&!other);
  ipasir_add(s,7);ipasir_assume(s,7); // input mutation is rejected while active
  assert(tactix_ipasir_finish(s,10)==-1);
  const int result=run_script(s,view,&state);assert(result==20);
  assert(tactix_ipasir_finish(s,result)==20);artifact(s,prefix,6,result,0);
  assert(tactix_ipasir_finish(s,0)==-1);
  call(s,10,prefix,7);assert(!ipasir_val(s,7));
  // A public BVE action can request explicit restoration. The same installed
  // script is re-entered with a new view instead of switching to DefaultPolicy.
  void *r=ipasir_init();assert(r);driver_state repair={0};repair.preprocess=1;
  ipasir_add(r,1);ipasir_add(r,2);ipasir_add(r,0);ipasir_assume(r,-1);
  assert(tactix_ipasir_set_strategy(r,&repair,scripted)==0);call(r,10,prefix,8);
  assert(repair.calls==2&&repair.restored==1&&ipasir_val(r,2)==2);
  ipasir_release(r);ipasir_release(s);
  // A MaxSAT client's dedicated script uses the same extraction/certificate
  // services, including an irrelevant selector that must disappear from C.
  s=ipasir_init();assert(s);driver_state core={0};
  assert(tactix_ipasir_set_strategy(s,&core,scripted)==0);
  assert(tactix_ipasir_set_core_budget(s,1000000)==0);
  ipasir_add(s,1);ipasir_add(s,0);ipasir_assume(s,7);ipasir_assume(s,-1);
  assert(ipasir_solve(s)==20&&core.calls==1&&core.steps>0);
  assert(ipasir_failed(s,-1)&&!ipasir_failed(s,7));artifact(s,prefix,9,20,1);
  call(s,10,prefix,10);assert(core.calls==2&&ipasir_val(s,1)==1);
  ipasir_release(s);
  // F implies -1 without root unit propagation. The query's assumption 1
  // exposes a conflict through ordinary analysis before startup Sweep work.
  // The preceding irrelevant assumption must not enter the reduced core.
  s=ipasir_init();assert(s);preflight_state preflight={0};
  assert(!tactix_ipasir_set_strategy(s,&preflight,preflight_delegate));
  assert(!tactix_ipasir_set_core_budget(s,1000000));
  ipasir_add(s,-1);ipasir_add(s,2);ipasir_add(s,0);
  ipasir_add(s,-1);ipasir_add(s,-2);ipasir_add(s,0);
  ipasir_assume(s,7);ipasir_assume(s,1);
  assert(ipasir_solve(s)==20);
  assert(preflight.conflicts>0&&preflight.sweep_ticks==0);
  assert(ipasir_failed(s,1)&&!ipasir_failed(s,7));artifact(s,prefix,11,20,1);
  call(s,10,prefix,12);assert(ipasir_val(s,1)==-1);
  ipasir_add(s,1);ipasir_add(s,0);call(s,20,prefix,13);
  assert(!ipasir_failed(s,1)&&!ipasir_failed(s,7));
  ipasir_release(s);
  puts("programmable IPASIR: runtime scripts, configured handoff, manual stepping, native repair and result validity pass");
}
