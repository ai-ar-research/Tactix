#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "api/api_internal.hpp"
#include "policies/default_policy.hpp"
#include "rules/lucky/graph_labeling_model.hpp"
#include "rules/lucky/lucky.hpp"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
using namespace tactix;

static void pure_reads() {
  assert(tactix_lucky_eligible(nullptr,0)==-1);
  assert(tactix_propagate(nullptr)==-1);
  auto *s=tactix_init_configured(TACTIX_INIT_DEFAULT_FRONTEND);assert(s);
  auto &k=api_detail::kernel_for_fixed_testing(s);
  const auto epoch=k.strategy_epoch;
  tactix_execution_state execution{};execution.external_variables=97;
  assert(tactix_read_execution_state(nullptr,&execution)==-1&&execution.external_variables==97);
  assert(!tactix_read_execution_state(s,&execution));
  tactix_inprocess_state phase{};phase.inconsistent=97;
  assert(tactix_read_inprocess_state(nullptr,&phase)==-1&&phase.inconsistent==97);
  assert(!tactix_read_inprocess_state(s,&phase)&&!phase.inconsistent&&!phase.phase_open);
  k.inconsistent=true;
  assert(!tactix_read_inprocess_state(s,&phase)&&phase.inconsistent);
  k.inconsistent=false;
  assert(!execution.context_depth&&execution.context_top==-1&&execution.provenance_bound&&execution.affine_matrix_bound);
  tactix_online_affine_matrix_initialize_parameters profile{};int active=17;
  assert(tactix_read_affine_matrix_profile(nullptr,&profile,&active)==-1&&active==17);
  assert(!tactix_read_affine_matrix_profile(s,&profile,&active)&&!active);
  assert(profile.basis.maximum_clauses==k.affine_consequence_parameters.maximum_clauses);
  k.affine_matrix_initialize_parameters_active=true;
  k.affine_matrix_initialize_parameters.basis.maximum_clauses=37;
  k.affine_matrix_initialize_parameters.operation=strategy::AffineMatrixInitializeParameters::Operation::SearchStep;
  assert(!tactix_read_affine_matrix_profile(s,&profile,&active)&&active);
  assert(profile.basis.maximum_clauses==37&&profile.operation==TACTIX_AFFINE_MATRIX_SEARCH_STEP);
  k.affine_matrix_initialize_parameters_active=false;

  assert(tactix_lucky_eligible(s,0)==-1);
  tactix_graph_labeling_state state{};state.fingerprint=97;
  assert(tactix_read_graph_labeling_state(nullptr,&state)==-1&&state.fingerprint==97);
  assert(tactix_read_graph_labeling_state(s,nullptr)==-1);
  assert(!tactix_read_graph_labeling_state(s,&state));
  assert(!state.authenticated&&!state.complete&&!state.authentication_status&&!state.slice_status);
  assert(!tactix_stat(s,TACTIX_STAT_INPUT_CLOSED)&&k.strategy_epoch==epoch&&!k.h);
  GraphLabelingAuthenticationResult a{GraphLabelingAuthenticationStatus::Exhausted,UINT64_MAX};
  GraphLabelingSliceResult b{ApplyResult::Sat,GraphLabelingSliceStatus::Sat,17,31,29,true,true};
  k.graph_labeling_session.record_public_result(a,b);
  assert(!tactix_read_graph_labeling_state(s,&state));
  assert(state.authentication_status==TACTIX_GRAPH_AUTH_EXHAUSTED&&state.authentication_work==UINT64_MAX);
  assert(state.slice_status==TACTIX_GRAPH_SLICE_SAT&&state.slice_result==10&&state.slice_work==17);
  assert(state.objective_before==31&&state.objective_after==29&&state.hint_published&&state.useful_hint);
  assert(!tactix_stat(s,TACTIX_STAT_INPUT_CLOSED)&&k.strategy_epoch==epoch&&!k.model_extended&&!k.inconsistent);
  for(int lit:{1,2,0,-1,2,0})assert(!tactix_add_checked(s,lit));
  (void)tactix_status(s);
  const auto before_service=k.strategy_epoch;
  const auto ticks=k.stats.search_ticks;
  assert(tactix_propagate(s)==0&&tactix_propagate(s)==0);
  assert(k.strategy_epoch>before_service&&k.stats.search_ticks==ticks&&!k.startup_certificate_open);
  const auto closed=k.strategy_epoch;
  for(uint64_t id=0;id<28;++id) {
    const int eligible=tactix_lucky_eligible(s,id);
    assert(id==25?eligible==-1:eligible>=0);
  }
  assert(tactix_lucky_eligible(s,UINT64_MAX)>=0&&tactix_lucky_eligible(s,28)==-1);
  assert(k.strategy_epoch==closed&&!k.h&&k.cold_state_deferred);
  assert(tactix_apply_instance(s,tactix_rule_id("Lucky"),0)==10);
  assert(tactix_stat(s,TACTIX_STAT_LEVEL)>0&&k.model_extended);
  assert(!tactix_support(s,TACTIX_SUPPORT_PREPARATION_RESTART,0));
  assert(!tactix_stat(s,TACTIX_STAT_LEVEL)&&k.cold_state_deferred&&!k.h);
  assert(!k.model_extended&&k.model_values.empty());
  assert(!tactix_read_execution_state(s,&execution)&&!execution.model_cached);
  assert(!tactix_support(s,TACTIX_SUPPORT_RESET_GRAPH_LABELING_SESSION,0));
  assert(!tactix_read_graph_labeling_state(s,&state)&&!state.slice_status&&!state.authentication_status);
  tactix_release(s);
}
static void propagation_boundaries() {
  auto *s=tactix_init();assert(s);
  for(int lit:{-1,2,0,-1,-2,0})assert(!tactix_add_checked(s,lit));
  assert(!tactix_decide(s,1));
  assert(tactix_propagate(s)==2);
  auto &k=api_detail::kernel_for_fixed_testing(s);const auto epoch=k.strategy_epoch;
  assert(k.conflict_valid&&!k.inconsistent&&tactix_lucky_eligible(s,0)==0);
  assert(tactix_propagate(s)==-1&&k.strategy_epoch==epoch&&k.conflict_valid);
  assert(!tactix_support(s,TACTIX_SUPPORT_PREPARATION_RESTART,0));
  assert(!k.level&&!k.conflict_valid&&tactix_propagate(s)==0);
  tactix_release(s);
  s=tactix_init();assert(s);
  for(int lit:{1,0,-1,0})assert(!tactix_add_checked(s,lit));
  assert(tactix_propagate(s)==20&&tactix_propagate(s)==20);
  tactix_release(s);
}
static int code(ApplyResult r) {
  switch(r) {case ApplyResult::Ok:return 0;case ApplyResult::NoOp:return 1;
    case ApplyResult::Conflict:return 2;case ApplyResult::Sat:return 10;
    case ApplyResult::Unsat:return 20;}
  std::abort();
}
struct Capture final:strategy::controller::FeedbackReceiver {
  const Kernel &kernel;std::string proof,prefix;bool boundary=false;
  Capture(const Kernel &k,const char *p):kernel(k),proof(p),prefix(proof+".prefix") {}
  void save() {
    if(kernel.proof)kernel.proof->flush();
    std::ifstream in(proof,std::ios::binary);std::ofstream out(prefix,std::ios::binary);
    out<<in.rdbuf();
  }
  void after_action(const strategy::controller::ActionFeedback &r) override {
    if(boundary)return;
    if(r.identity=="MaterializeColdState") {save();boundary=true;return;}
    if(r.identity=="Lucky"&&r.instance==LUCKY_GRAPH_AUTHENTICATE)
      std::cout<<"graph "<<kernel.graph_labeling_session.authenticated()<<' '
               <<kernel.graph_labeling_session.complete()<<' '
               <<kernel.graph_labeling_session.candidate_fingerprint()<<' '
               <<kernel.graph_labeling_session.recommended_continuation_work()<<'\n';
    if(r.identity=="Lucky"||r.identity=="Propagate"||r.identity=="AffineMatrixInitialize"||r.identity=="AffineMatrixSearch")
      std::cout<<"action "<<(r.identity=="AffineMatrixSearch"?"AffineMatrixInitialize":r.identity)<<' '<<r.instance<<' '<<code(r.outcome.invocation.rule_result)<<'\n';
  }
  void search_interval(const strategy::controller::SearchIntervalFeedback &) override {}
};
int main(int argc,char **argv) {
  pure_reads();propagation_boundaries();if(argc==1)return 0;
  assert(argc==4);
  auto *s=tactix_init_configured(TACTIX_INIT_DEFAULT_FRONTEND);assert(s);
  const int format=std::atoi(argv[3]);if(format)assert(!tactix_proof(s,argv[2],format));
  FILE *in=std::fopen(argv[1],"r");assert(in);assert(!tactix_read_dimacs(s,in));std::fclose(in);
  auto &k=api_detail::kernel_for_fixed_testing(s);
  DefaultPolicy policy;Capture capture(k,argv[2]);policy.set_feedback(&capture);
  const int answer=policy.solve(k);assert(answer==10||answer==20);
  if(!capture.boundary)capture.save();
  std::cout<<"prefix "<<(capture.boundary?0:answer)<<'\n';
  if(!capture.boundary&&answer==10) {
    kernel_extend_model(k);std::cout<<"model";
    for(uint32_t v=0;v<kernel_num_output_vars(k);++v)
      std::cout<<' '<<(k.model_values[make_lit(v,false)]>0?int(v+1):-int(v+1));
    std::cout<<'\n';
  }
  tactix_release(s);
}
