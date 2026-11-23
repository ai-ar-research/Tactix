#undef NDEBUG
#include "ipasir_driver.h"
#include "ipasir_test_support.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {unsigned calls,restored,steps;int preprocess;} driver_state;
static int scripted(void *data,void *session,tactix *view,unsigned cause) {
  driver_state *state=data;++state->calls;
  assert(cause==TACTIX_IPASIR_ENTER_QUERY || cause==TACTIX_IPASIR_ENTER_RESTORED);
  state->restored+=cause==TACTIX_IPASIR_ENTER_RESTORED;
  assert(tactix_ipasir_set_strategy(session,NULL,NULL)==-1);
  assert(tactix_ipasir_finish(session,0)==-1); // the blocking driver owns completion
  if(state->preprocess && cause==TACTIX_IPASIR_ENTER_QUERY) {
    assert(tactix_apply_instance(view,rule("BVE"),(uint64_t)(uint32_t)1)>=0);
    assert(next_query(session).action==TACTIX_QUERY_RESTORE);return 0;
  }
  return drive_query(session,view,&state->steps,NULL);
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
typedef struct {uint64_t subsume,strengthen;} backtrackable_counts;
static int backtrackable_delegate(void *data,void *session,tactix *view,unsigned cause) {
  (void)cause;
  const int counter=tactix_counter_id("rule_calls");
  const int subsume=rule("BacktrackableSubsume");
  const int strengthen=rule("BacktrackableStrengthen");
  const uint64_t before_subsume=tactix_counter(view,counter,subsume);
  const uint64_t before_strengthen=tactix_counter(view,counter,strengthen);
  const int result=tactix_solve(view);
  assert(tactix_ipasir_set_backtrackable_inprocessing(session,0)==-1);
  *(backtrackable_counts *)data=(backtrackable_counts){
    tactix_counter(view,counter,subsume)-before_subsume,
    tactix_counter(view,counter,strengthen)-before_strengthen,
  };
  return result;
}
static void artifact(void *s,const char *prefix,unsigned index,int answer,int reduced) {
  if(prefix)copy_query_bundle(tactix_ipasir_bundle(s),prefix,index,answer,
                             reduced?TEST_CORE_REQUIRED:TEST_CORE_NONE);
}
static void call(void *s,int expected,const char *prefix,unsigned index) {
  const int result=ipasir_solve(s);if(result!=expected)fprintf(stderr,"query %u result %d expected %d\n",index,result,expected);
  assert(result==expected);artifact(s,prefix,index,result,0);
}
static void backtrackable_queries(const char *prefix) {
  const int formula[]={-3,7,0,-2,0,-4,-5,0,-4,5,-7,0,-6,-7,0,
                       3,-6,0,-1,5,-4,0,8,4,0,6,-8,0};
  for (int mode=0;mode<3;++mode) {
    void *s=ipasir_init();assert(s);
    for(size_t i=0;i<sizeof formula/sizeof *formula;++i)ipasir_add(s,formula[i]);
    // Preserve clauses for the assumption-level test instead of solving at root.
    assert(tactix_ipasir_set_default_option(s,"TACTIX_SLOT_DISABLE",
                                           "BVE,Sweep,Backbone,Congruence")==0);
    if(!mode)assert(tactix_ipasir_set_backtrackable_inprocessing(s,2)==-1);
    if(mode)assert(tactix_ipasir_set_backtrackable_inprocessing(s,1)==0);
    if(mode==2)assert(tactix_ipasir_set_backtrackable_inprocessing(s,0)==0);
    const int enabled=mode==1;
    backtrackable_counts calls={0};
    assert(tactix_ipasir_set_strategy(s,&calls,backtrackable_delegate)==0);
    ipasir_assume(s,1);call(s,20,enabled?prefix:NULL,14);
    // A successful subsumption ends the attempt before the next rule.
    assert(ipasir_failed(s,1)&&calls.subsume==(uint64_t)enabled&&!calls.strengthen);
    ipasir_assume(s,-1);call(s,10,enabled?prefix:NULL,15);
    assert(ipasir_val(s,1)==-1);
    ipasir_release(s);
  }
}
static void backtrackable_query_restoration(const char *prefix) {
  const struct {const char *name;int victim[3], other[3];} cases[]={
    {"BacktrackableSubsume", {2,3,4}, {-5,2,3}},
    {"BacktrackableStrengthen", {1,2,3}, {-1,2,-5}},
    {"BacktrackableEliminate", {-1,2,3}, {1,2,-5}},
  };
  for(size_t i=0;i<sizeof cases/sizeof *cases;++i) {
    void *s=ipasir_init();assert(s);
    for(int j=0;j<3;++j)ipasir_add(s,cases[i].victim[j]);
    ipasir_add(s,0);
    for(int j=0;j<3;++j)ipasir_add(s,cases[i].other[j]);
    ipasir_add(s,0);
    ipasir_assume(s,5);tactix *view=NULL;
    assert(tactix_ipasir_begin(s,&view)==1&&next_query(s).action==TACTIX_QUERY_ASSUME);
    assert(tactix_ipasir_assume_next(s)==0);
    assert(tactix_apply(view,rule("Propagate"))>=0);
    assert(tactix_apply(view,rule(cases[i].name))==0);
    assert(drive_query(s,view,NULL,NULL)==10&&tactix_ipasir_finish(s,10)==10);
    // Flip the old assumption so a witness under the old trail cannot
    // supply a contradiction when the next query falsifies the victim.
    ipasir_assume(s,-5);
    for(int j=0;j<3;++j)ipasir_assume(s,-cases[i].victim[j]);
    call(s,20,prefix,(unsigned)(16+i));
    ipasir_release(s);
  }
}
static int stop_flag(void *state) {return *(int *)state;}
static void interrupt_between_boundary_and_action(void) {
  void *s=ipasir_init();assert(s);int stop=0;tactix *view=NULL;
  ipasir_set_terminate(s,&stop,stop_flag);ipasir_assume(s,5);
  assert(tactix_ipasir_begin(s,&view)==1&&next_query(s).action==TACTIX_QUERY_ASSUME);
  stop=1;
  assert(tactix_ipasir_assume_next(s)==TACTIX_IPASIR_ACTION_INTERRUPTED);
  assert(next_query(s).action==TACTIX_QUERY_INTERRUPTED);
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
  p.polls=0;p.limit=2; // Outer next_query() poll passes; its status() poll stops.
  assert(next_query(s).action==TACTIX_QUERY_INTERRUPTED&&p.polls==2);
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
    const uint64_t count=tactix_ipasir_rejected_models(s);assert(count==1);
    for(unsigned j=0;j<32;++j)assert(!tactix_value(view,1));
    assert(tactix_ipasir_rejected_models(s)==count);
    if(i==0)assert(tactix_apply_instance(view,rule("Restart"),0)>=0);
    else {
      assert(tactix_ipasir_assume_next(s)==-1); // Lucky's trail needs backtracking first.
      for(unsigned j=0;j<32;++j)assert(!tactix_value(view,1)&&!tactix_status(view));
      assert(tactix_apply_instance(view,rule("Restart"),0)>=0);
    }
    assert(tactix_ipasir_rejected_models(s)==count);
    driver_state state={0};assert(drive_query(s,view,&state.steps,NULL)==10);
    assert(tactix_ipasir_finish(s,10)==10&&ipasir_val(s,1)==-1&&ipasir_val(s,2)==2);
    assert(tactix_ipasir_rejected_models(s)==UINT64_MAX);
  }
  ipasir_release(s);
}
static void model_reads(void) {
  void *s=ipasir_init();assert(s);
  ipasir_add(s,-1);ipasir_add(s,2);ipasir_add(s,0);ipasir_assume(s,1);
  tactix *view=NULL;assert(tactix_ipasir_begin(s,&view)==1);
  for(unsigned i=0;i<32;++i)assert(!tactix_value(view,1)&&!tactix_value(view,2));
  assert(!tactix_ipasir_rejected_models(s)&&!tactix_status(view));
  driver_state state={0};assert(drive_query(s,view,&state.steps,NULL)==10);
  for(unsigned i=0;i<1024;++i)assert(tactix_value(view,1)==1&&tactix_value(view,-2)==2&&tactix_status(view)==10);
  assert(!tactix_ipasir_rejected_models(s));
  assert(tactix_apply_instance(view,rule("Restart"),0)>=0);
  for(unsigned i=0;i<32;++i)assert(!tactix_value(view,1)&&!tactix_value(view,2));
  assert(!tactix_ipasir_rejected_models(s));
  assert(drive_query(s,view,&state.steps,NULL)==10&&tactix_ipasir_finish(s,10)==10);
  assert(ipasir_val(s,1)==1&&ipasir_val(s,2)==2);ipasir_release(s);
}
static void model_reads_before_assumptions(void) {
  void *s=ipasir_init();assert(s);
  ipasir_add(s,1);ipasir_add(s,2);ipasir_add(s,0);ipasir_assume(s,-1);
  tactix *view=NULL;assert(tactix_ipasir_begin(s,&view)==1);
  assert(!tactix_decide(view,1)&&!tactix_decide(view,2));
  assert(tactix_apply(view,rule("Propagate"))>=0);
  for(unsigned i=0;i<32;++i)assert(!tactix_value(view,1)&&!tactix_status(view));
  assert(!tactix_ipasir_rejected_models(s));
  assert(tactix_apply_instance(view,rule("Restart"),0)>=0);
  driver_state state={0};assert(drive_query(s,view,&state.steps,NULL)==10);
  assert(tactix_ipasir_finish(s,10)==10&&ipasir_val(s,1)==-1);ipasir_release(s);
}
int main(int argc,char **argv) {
  assert(argc==1||argc==2);const char *prefix=argc==2?argv[1]:NULL;
  interrupt_between_boundary_and_action();
  interrupt_inside_boundary_observation();
  rejected_model_observation();
  model_reads();
  model_reads_before_assumptions();
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
  const int result=drive_query(s,view,&state.steps,NULL);assert(result==20);
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
  backtrackable_queries(prefix);
  backtrackable_query_restoration(prefix);
  puts("programmable IPASIR: runtime scripts, configured handoff, manual stepping, native repair and result validity pass");
}
