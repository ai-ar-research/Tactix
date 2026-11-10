#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "api/api_internal.hpp"
#include "policies/default_policy.hpp"
#include "strategy/policy_completion.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>
using namespace tactix;

struct Capture final:strategy::controller::FeedbackReceiver {
  void after_action(const strategy::controller::ActionFeedback &event) override {
    if(!event.native_request||!event.native_request->invocation)return;
    const auto &d=*event.native_request;
    const auto &r=event.outcome.invocation;
    std::cout<<"action "<<d.description<<' '<<d.effort_allowance.is_unlimited()<<' '
        <<(d.effort_allowance.is_unlimited()?0:d.effort_allowance.amount())<<' '
        <<unsigned(r.rule_result)<<' '<<unsigned(r.work_unit)<<' '<<r.work<<' '
        <<r.applications<<' '<<r.made_rule_progress<<' '<<r.discovery_progress_units<<' '
        <<r.discovery_entry_required<<' '<<r.discovery_exhausted<<'\n';
  }
  void search_interval(const strategy::controller::SearchIntervalFeedback &) override {}
};

static void telemetry(const char *label,
    const DefaultInprocessScheduler::StructuredRootStageTelem &t) {
  std::cout<<label<<' '<<t.runs<<' '<<t.advances<<' '<<t.applications<<' '
      <<t.effective<<' '<<t.sequence_passes<<' '<<unsigned(t.terminal)<<'\n';
}
int main(int argc,char **argv) {
  if(argc==1) {
    auto *s=tactix_init();assert(s);
    tactix_inprocess_state state{};state.inconsistent=97;
    assert(tactix_read_inprocess_state(nullptr,&state)==-1&&state.inconsistent==97);
    auto &k=api_detail::kernel_for_fixed_testing(s);const auto epoch=k.strategy_epoch;
    assert(!tactix_read_inprocess_state(s,&state)&&!state.inconsistent&&!state.phase_open);
    k.inconsistent=true;
    assert(!tactix_read_inprocess_state(s,&state)&&state.inconsistent);
    k.inconsistent=false;
    assert(!tactix_stat(s,TACTIX_STAT_INPUT_CLOSED)&&k.strategy_epoch==epoch);
    tactix_release(s);return 0;
  }
  assert(argc==5||argc==6);
  auto *s=tactix_init_configured(TACTIX_INIT_DEFAULT_FRONTEND);assert(s);
  const int format=std::atoi(argv[3]),arm=std::atoi(argv[4]);
  if(format)assert(!tactix_proof(s,argv[2],format));
  FILE *input=std::fopen(argv[1],"r");assert(input);
  assert(!tactix_read_dimacs(s,input));std::fclose(input);
  assert(tactix_status(s)!=20);
  assert(tactix_support(s,TACTIX_SUPPORT_MATERIALIZE_COLD_STATE,0)>=0);
  assert(tactix_propagate(s)==0);
  assert(tactix_support(s,TACTIX_SUPPORT_ENTER_CONTEXT,TACTIX_CONTEXT_STARTUP)>=0);
  if(arm==6)assert(!tactix_support(s,TACTIX_SUPPORT_ENTER_INPROCESS_PHASE,0));
  auto &k=api_detail::kernel_for_fixed_testing(s);
  auto config=DefaultPolicy::configured_strategy_config(k);assert(config);
  auto &c=config->inprocess;
  if(arm==1)c.structured_root_backtracking_domain_limit=0;
  if(arm==2)c.structured_root_large_domain_limit=0;
  if(arm==3||arm==4) {
    c.structured_root_progress_minimum_ticks=1;
    c.structured_root_progress_ticks_per_variable=0;
    c.structured_root_sr_discovery_budget=strategy::WorkLimit::bounded(arm==3?256:0);
  }
  if(arm==5)c.structured_root_progress_feedback=false;
  if(arm==7||arm==8) {
    c.structured_root_sr_discovery_budget=strategy::WorkLimit::unlimited();
    c.structured_root_progress_feedback=arm==8;
    c.structured_root_progress_minimum_ticks=1;
    c.structured_root_progress_ticks_per_variable=0;
  }
  if(arm==9) {
    c.structured_root_large_domain_limit=0;
    c.structured_root_large_domain_progress_feedback=true;
    c.structured_root_progress_minimum_ticks=1;
    c.structured_root_progress_ticks_per_variable=0;
    c.structured_root_sr_discovery_budget=strategy::WorkLimit::bounded(256);
  }
  if(arm==10)c.structured_root_progress_ticks_per_variable=UINT64_MAX;
  DefaultInprocessScheduler scheduler(&c);
  Capture receiver;
  execution::PolicyCompletion completion(k,&receiver,
      [](const Kernel &kernel,const void *) {return strategy::controller::observe_kernel(kernel);},nullptr);
  completion.begin();
  tactix_strategy_state initial{};assert(!tactix_read_strategy_state(s,&initial));
  std::cout<<"initial "<<initial.arena_generation<<' '<<initial.touch_epoch<<' '
      <<initial.strategy_epoch<<'\n';
  int result=scheduler.run_structured_root(k,nullptr,&completion);
  const auto &t=scheduler.structured_root;
  telemetry("total",t);
  std::cout<<"discovery "<<t.sr_discovery_ticks<<' '<<t.sr_discovery_progress_window
      <<' '<<t.sr_discovery_renewals<<' '<<t.sr_discovery_overflow<<'\n';
  for(const auto &stage:t.stages)telemetry("stage",stage);
  const auto &o=scheduler.structured_fastel_ownership();
  std::cout<<"ownership "<<o.grew_root_trail<<' '<<o.external_begin<<' '
      <<o.external_end<<' '<<o.symmetry_applied<<' '<<o.coupled_factor_ranges.size()<<'\n';
  tactix_strategy_state state{};assert(!tactix_read_strategy_state(s,&state));
  std::cout<<"state "<<state.arena_generation<<' '<<state.touch_epoch<<' '
      <<state.strategy_epoch<<' '<<state.trail_size<<' '<<state.sr_applied<<' '
      <<state.unique_literal_amo_binaries<<' '<<state.nonforcing_cuts_declined<<' '
      <<state.post_amo_nonhelper_applied<<' '<<state.post_amo_graph_root_units<<'\n';
  assert(!scheduler.run_structured_root(k,nullptr));
  if(arm==6)assert(tactix_support(s,TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE,0)>=0);
  assert(!tactix_support(s,TACTIX_SUPPORT_LEAVE_CONTEXT,0));
  std::cout<<"result "<<result<<'\n';
  if(argc==6&&std::atoi(argv[5])&&!result)result=tactix_solve(s);
  std::cout<<"verdict "<<result<<'\n';
  if(result==10) {
    std::cout<<"model";
    for(unsigned v=1;v<=tactix_stat(s,TACTIX_STAT_OUTPUT_VARIABLES);++v)
      std::cout<<' '<<tactix_value(s,int(v));
    std::cout<<'\n';
  }
  tactix_release(s);
}
