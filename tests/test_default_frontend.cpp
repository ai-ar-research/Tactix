#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "api/api_internal.hpp"
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/bve/bve.hpp"
#include "strategy/representation_execution.hpp"
#include "strategy/execution_scope.hpp"
#include "temporary_file.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
using namespace tactix;

static void snapshot(tactix_c_handle *s) {
  for(int stat:{TACTIX_STAT_VARIABLES,TACTIX_STAT_OUTPUT_VARIABLES,
      TACTIX_STAT_COLD_STATE_DEFERRED,TACTIX_STAT_HEURISTICS_ATTACHED})
    std::cout<<tactix_stat(s,stat)<<',';
  std::cout<<tactix_config(s,TACTIX_CFG_BVE_GATES);
}
static void guards(const char *path) {
  auto *s=tactix_init_configured(TACTIX_INIT_DEFAULT_FRONTEND);assert(s);
  FILE *in=std::fopen(path,"r");assert(in);
  assert(!tactix_read_dimacs(s,in));std::fclose(in);
  auto &k=api_detail::kernel_for_fixed_testing(s);
  const auto epoch=k.strategy_epoch;
  uint64_t instances[8];
  for(int id=0;id<tactix_rule_count();++id) {
    const char *name=tactix_rule_name(id);
    if(!std::strcmp(name,"Lucky")||!std::strcmp(name,"Propagate")||
       !std::strcmp(name,"AffineMatrixInitialize"))continue;
    assert(tactix_apply(s,id)==-1&&tactix_apply_instance(s,id,0)==-1);
    assert(tactix_apply_semantic_instance(s,id,0)==-1);
    assert(tactix_rule_instances(s,id,instances,8)==-1);
    assert(tactix_rule_semantic_instances(s,id,instances,8)==-1);
  }
  for(int support=1;support<=TACTIX_SUPPORT_DETACH_HEURISTICS;++support) {
    if(support==TACTIX_SUPPORT_MATERIALIZE_COLD_STATE ||
       support==TACTIX_SUPPORT_PREPARATION_RESTART ||
       support==TACTIX_SUPPORT_CLOSE_STARTUP_CERTIFICATE ||
       support==TACTIX_SUPPORT_RESET_GRAPH_LABELING_SESSION ||
       support==TACTIX_SUPPORT_RELEASE_INPUT_ORDER ||
       support==TACTIX_SUPPORT_CONSUME_ROOT_PREFIX ||
       support==TACTIX_SUPPORT_ENTER_CONTEXT || support==TACTIX_SUPPORT_LEAVE_CONTEXT ||
       support==TACTIX_SUPPORT_RELEASE_AFFINE_MATRIX)continue;
    assert(tactix_support(s,support,0)==-1);
  }
  assert(tactix_support(s,TACTIX_SUPPORT_ROOT_PREPARATION,TACTIX_ROOT_PREPARATION_PROBING)==-1);
  assert(tactix_support(s,TACTIX_SUPPORT_ROOT_PREPARATION,TACTIX_ROOT_PREPARATION_CAPTURE_TARGET)==-1);
  assert(tactix_decide(s,1)==-1&&tactix_regions_begin(s)==-1);
  assert(!tactix_bve_order_create(s,nullptr,0));
  assert(!tactix_restart_clock_create(s,nullptr));
  assert(k.strategy_epoch==epoch&&k.cold_state_deferred&&!k.h);
  assert(!tactix_support(s,TACTIX_SUPPORT_MATERIALIZE_COLD_STATE,0));
  assert(tactix_support(s,TACTIX_SUPPORT_ROOT_PREPARATION,TACTIX_ROOT_PREPARATION_CAPTURE_TARGET)==-1);
  assert(tactix_support(s,TACTIX_SUPPORT_DETACH_HEURISTICS,0)==1);
  assert(!tactix_support(s,TACTIX_SUPPORT_ATTACH_HEURISTICS,0));
  assert(!tactix_support(s,TACTIX_SUPPORT_ENTER_CONTEXT,TACTIX_CONTEXT_STARTUP));
  assert(tactix_support(s,TACTIX_SUPPORT_MATERIALIZE_COLD_STATE,0)==1);
  assert(tactix_support(s,TACTIX_SUPPORT_ATTACH_HEURISTICS,0)==1);
  assert(tactix_support(s,TACTIX_SUPPORT_DETACH_HEURISTICS,0)==-1);
  assert(!tactix_support(s,TACTIX_SUPPORT_LEAVE_CONTEXT,0));
  assert(!tactix_support(s,TACTIX_SUPPORT_ENTER_INPROCESS_PHASE,0));
  for(int op:{TACTIX_SUPPORT_MATERIALIZE_COLD_STATE,TACTIX_SUPPORT_ATTACH_HEURISTICS,TACTIX_SUPPORT_DETACH_HEURISTICS})
    assert(tactix_support(s,op,0)==-1);
  assert(tactix_support(s,TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE,0)>=0);
  auto *clock=tactix_restart_clock_create(s,nullptr);assert(clock);
  assert(!tactix_support(s,TACTIX_SUPPORT_DETACH_HEURISTICS,0));
  assert(!tactix_restart_clock_create(s,nullptr));
  assert(tactix_restart_clock_configure(s,clock,nullptr)==-1);
  for(int event=TACTIX_RESTART_CLOCK_RESET;event<=TACTIX_RESTART_CLOCK_MODE_ENTERED;++event)
    assert(tactix_restart_clock_update(s,clock,event,0)==-1);
  tactix_restart_clock_state observed{};observed.next_conflicts=UINT64_MAX;
  assert(tactix_restart_clock_observe(s,clock,&observed)==-1&&observed.next_conflicts==UINT64_MAX);
  assert(!tactix_support(s,TACTIX_SUPPORT_ATTACH_HEURISTICS,0));
  assert(!tactix_restart_clock_update(s,clock,TACTIX_RESTART_CLOCK_RESET,0));
  assert(!tactix_restart_clock_observe(s,clock,&observed));
  tactix_restart_clock_release(clock);
  assert(k.h&&k.h->nvars==k.nvars&&!k.cold_state_deferred);
  tactix_release(s);
  for(int frontend:{0,1}) {
    auto *u=tactix_init_configured(frontend?TACTIX_INIT_DEFAULT_FRONTEND:0);assert(u);
    for(int lit:{-1,2,0,1,0})assert(!tactix_add_checked(u,lit));
    // Input units close eagerly; rearm the existing root prefix to exercise
    // the attachment fixpoint restriction without inventing a new inference.
    assert(tactix_status(u)==10);
    auto &uk=api_detail::kernel_for_fixed_testing(u);
    uk.propagate_head=0;
    if(frontend) {
      assert(!tactix_support(u,TACTIX_SUPPORT_ENTER_CONTEXT,TACTIX_CONTEXT_STARTUP));
      const uint64_t search=uk.stats.search_ticks,work=uk.stats.ticks;
      assert(tactix_apply(u,tactix_rule_id("Propagate"))>=0);
      assert(uk.cold_state_deferred&&!uk.h&&uk.stats.search_ticks==search&&uk.stats.ticks>work);
      uk.propagate_head=0;
    }
    if(frontend)assert(!tactix_support(u,TACTIX_SUPPORT_MATERIALIZE_COLD_STATE,0));
    else assert(!tactix_support(u,TACTIX_SUPPORT_DETACH_HEURISTICS,0));
    assert(tactix_support(u,TACTIX_SUPPORT_ATTACH_HEURISTICS,0)==-1);
    assert(tactix_support(u,TACTIX_SUPPORT_ROOT_PREPARATION,TACTIX_ROOT_PREPARATION_CAPTURE_TARGET)==-1);
    assert(tactix_apply(u,tactix_rule_id("Propagate"))>=0);
    assert(!tactix_support(u,TACTIX_SUPPORT_ATTACH_HEURISTICS,0));
    if(frontend)assert(!tactix_support(u,TACTIX_SUPPORT_LEAVE_CONTEXT,0));
    assert(tactix_solve(u)==10);
    assert(!tactix_support(u,TACTIX_SUPPORT_DETACH_HEURISTICS,0));
    tactix_release(u);
  }
}
static void cold_lucky(const char *path) {
  uint64_t instances[128];
  auto *probe=tactix_init_configured(TACTIX_INIT_DEFAULT_FRONTEND);assert(probe);
  FILE *in=std::fopen(path,"r");assert(in);assert(!tactix_read_dimacs(probe,in));std::fclose(in);
  const int lucky=tactix_rule_id("Lucky");
  const int count=tactix_rule_instances(probe,lucky,instances,128);assert(count>0&&count<128);
  tactix_release(probe);
  for(int i=-1;i<count;++i) {
    auto *s=tactix_init_configured(TACTIX_INIT_DEFAULT_FRONTEND);assert(s);
    FILE *f=std::fopen(path,"r");assert(f);assert(!tactix_read_dimacs(s,f));std::fclose(f);
    assert((i<0?tactix_apply(s,lucky):tactix_apply_instance(s,lucky,instances[i]))>=0);
    assert(tactix_stat(s,TACTIX_STAT_COLD_STATE_DEFERRED)==1);
    tactix_release(s);
  }
}
static void run(const char *path) {
  auto *old=tactix_init(),*s=tactix_init_configured(TACTIX_INIT_DEFAULT_FRONTEND);
  assert(old&&s&&!tactix_init_configured(UINT32_MAX)&&!tactix_init_configured(2));
  auto &a=api_detail::kernel_for_fixed_testing(old),&b=api_detail::kernel_for_fixed_testing(s);
  // Exact previous tactix_public_main.cpp frontend, retained as an independent twin.
  a.h=nullptr;a.saved_phases=nullptr;kernel_defer_cold_state(a);
  assert(!tactix_set_guidance_publication(old,TACTIX_GUIDANCE_NATIVE));
  tactix_set_bve_gates(old,BVE_GATE_KISSAT_DEFAULT);
  assert(!tactix_discard_structural_provenance(old));
  for(auto *handle:{old,s}) {
    FILE *in=std::fopen(path,"r");assert(in);
    assert(!tactix_read_dimacs(handle,in));assert(!std::fclose(in));
  }
  assert(a.cold_state_deferred&&b.cold_state_deferred&&!a.h&&!b.h);
  assert(a.input_activation_order==b.input_activation_order&&a.internal_to_external==b.internal_to_external);
  std::cout<<"[ [";snapshot(old);std::cout<<"], [";snapshot(s);std::cout<<"] ]\n";
  assert(tactix_support(s,TACTIX_SUPPORT_ATTACH_HEURISTICS,0)==-1);
  assert(tactix_support(s,TACTIX_SUPPORT_MATERIALIZE_COLD_STATE,1)==-1);
  assert(b.cold_state_deferred&&!b.h);
  const auto startup=execution::enter_execution_context(a,execution::ExecutionContext::Startup);
  assert(!tactix_support(s,TACTIX_SUPPORT_ENTER_CONTEXT,TACTIX_CONTEXT_STARTUP));
  execution::execute_action(a,execution::MaterializeColdStateRequest{});
  assert(!tactix_support(s,TACTIX_SUPPORT_MATERIALIZE_COLD_STATE,0));
  assert(tactix_support(s,TACTIX_SUPPORT_MATERIALIZE_COLD_STATE,0)==1);
  assert(a.bve_retry==b.bve_retry&&a.touched==b.touched&&a.seen==b.seen);
  Heuristics expected;
  execution::execute_action(a,execution::HeuristicAttachmentRequest{expected});
  assert(!tactix_support(s,TACTIX_SUPPORT_ATTACH_HEURISTICS,0));
  assert(tactix_support(s,TACTIX_SUPPORT_ATTACH_HEURISTICS,0)==1);
  assert(a.h->nvars==b.h->nvars&&a.h->activation_count==b.h->activation_count);
  assert(a.h->saved==b.h->saved&&a.h->scores.score==b.h->scores.score);
  std::vector<int> x(a.nvars),y(b.nvars);
  const int nx=tactix_decide_candidates(old,x.data(),x.size()),ny=tactix_decide_candidates(s,y.data(),y.size());
  assert(nx==ny&&std::equal(x.begin(),x.begin()+nx,y.begin()));
  std::cout<<"[ [";snapshot(old);std::cout<<"], [";snapshot(s);std::cout<<"] ]\n";
  std::cout<<'[';for(int i=0;i<nx;++i)std::cout<<(i?",":"")<<x[i];std::cout<<"]\n";
  execution::leave_execution_context(a,execution::ExecutionContext::Startup,startup);
  assert(!tactix_support(s,TACTIX_SUPPORT_LEAVE_CONTEXT,0));
  execution::execute_action(a,execution::HeuristicDetachmentRequest{expected});
  assert(!tactix_support(s,TACTIX_SUPPORT_DETACH_HEURISTICS,0));
  assert(tactix_support(s,TACTIX_SUPPORT_DETACH_HEURISTICS,0)==1);
  assert(!b.h&&!b.saved_phases);
  kernel_check_invariants(a);kernel_check_invariants(b);
  tactix_release(old);tactix_release(s);
}
int main(int argc,char **argv) {
  if(argc==2) {guards(argv[1]);cold_lucky(argv[1]);run(argv[1]);return 0;}
  assert(argc==1);TemporaryFile input;
  {std::ofstream f(input.path);f<<"p cnf 7 3\n1 2 0\n-1 3 0\n-3 4 0\n";}
  guards(input.path.c_str());cold_lucky(input.path.c_str());run(input.path.c_str());
}
