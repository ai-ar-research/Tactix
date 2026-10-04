#undef NDEBUG
#include "rules/lucky/ramsey_startup.hpp"
#include "rules/lucky/lucky.hpp"
#include "policies/random_controller_policy.hpp"
#include "policies/default_strategy_config.hpp"
#include "strategy/native_feedback.hpp"
#include "frontend/parse.hpp"
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;
static RamseyStartupParameters profile() {
  return {10000,200,40000,10000000,10000000,100000,1000000,200000};
}
static void input(Kernel &k) {
  FILE *in=std::tmpfile();assert(in);
  std::fputs("p cnf 6 2\n1 2 3 4 5 6 0\n-1 -2 -3 -4 -5 -6 0\n",in);
  std::rewind(in);assert(!parse_dimacs(in,k));std::fclose(in);
}
static OnlineInvocation invocation() {
  OnlineInvocation step;step.id=1;step.rule="Lucky";step.parameters=profile();
  step.instances={LUCKY_STARTUP_RAMSEY};
  step.granularity=InvocationGranularity::OneInstance;
  return step;
}
struct Capture final:FeedbackReceiver {
  std::vector<ActionFeedback> actions;
  void after_action(const ActionFeedback &a) override {actions.push_back(a);}
  void search_interval(const SearchIntervalFeedback &) override {}
};
int main() {
  const auto p=profile();assert(p.valid()&&p.enabled());
  assert(RamseyStartupParameters{}.valid()&&!RamseyStartupParameters{}.enabled());
  auto invalid=p;invalid.maximum_external_extent=UINT32_MAX;assert(!invalid.valid());
  invalid=p;invalid.certificate_work=UINT64_MAX;assert(!invalid.valid());
  DefaultStrategyConfig defaults;assert(!defaults.ramsey_startup.enabled);
  defaults.ramsey_startup.parameters=p;assert(defaults.valid());
  assert(defaults.serialize().find("ramsey-startup1(")!=std::string::npos);
  Kernel k;input(k);assert(ramsey_startup_guard(k));
  const auto original=k.external_to_internal;const auto id=k.next_clause_id;
  OnlineStrategyProgram program;program.sequence={invocation()};
  const auto wire=serialize_online_strategy_program(program);
  OnlineStrategyProgram decoded;std::string error;
  assert(deserialize_online_strategy_program(wire,decoded,&error));
  assert(serialize_online_strategy_program(decoded)==wire);
  decoded.sequence[0].rule="Walk";assert(!validate_online_strategy_program(decoded));
  FILE *records=std::tmpfile();assert(records);k.structural_provenance=records;
  Capture capture;RestartController controller(k,{},&capture);
  ActionRequest request;request.invocation=invocation();
  auto apply=[&]() {
    request.sequence=controller.observe().sequence+1;request.expected=state_version(k);
    return controller.apply_rule(request,ActionAllowance{});
  };
  std::get<RamseyStartupParameters>(request.invocation.parameters).recognition_work=1;
  auto result=apply();assert(result.invocation.rule_result==ApplyResult::NoOp);
  assert(result.invocation.status==InvocationStatus::BudgetExhausted);
  assert(k.stats.ramsey_startup_attempts==1&&k.stats.ramsey_startup_recognition_work==1);
  assert(k.stats.ramsey_startup_exhausted==1);
  assert(k.ramsey_startup_parameters.recognition_work==0); // scoped profile
  assert(k.external_to_internal==original&&k.next_clause_id==id&&k.startup_certificate_open);
  const auto observed=controller.observe();const auto offset=std::ftell(records);
  for(int i=0;i<3;++i) {
    const auto again=controller.observe();
    assert(again.cumulative.ramsey_startup_attempts==observed.cumulative.ramsey_startup_attempts);
    assert(k.structural_action_sequence==1&&std::ftell(records)==offset);
  }
  request.invocation=invocation();assert(apply().invocation.rule_result==ApplyResult::NoOp);
  assert(k.stats.ramsey_startup_attempts==2&&!k.inconsistent);
  assert(capture.actions.size()==2);
  assert(capture.actions.front().after.cumulative.ramsey_startup_exhausted==1);
  RandomControllerConfig config;config.parameters.rules={"Lucky"};
  config.parameters.ramsey_startup_reference=p;
  assert(controller_configuration(config).find("ramsey-startup1(")!=std::string::npos);
  RandomControllerSelector selector(42);bool selected=false;
  for(int i=0;i<1000&&!selected;++i) {
    ActionRequest choice;assert(selector.choose(k,controller.observe(),config,choice,error));
    if(choice.type==ActionType::Rule&&choice.invocation.instances==std::vector<uint64_t>{22}) {
      const auto &chosen=std::get<RamseyStartupParameters>(choice.invocation.parameters);
      assert(chosen.recognition_work==p.recognition_work);selected=true;
    }
  }
  assert(selected);
  k.close_startup_certificate();assert(!ramsey_startup_guard(k));
  assert(apply().invocation.rule_result==ApplyResult::NoOp);
  assert(k.structural_action_sequence==2);
  k.structural_provenance=nullptr;std::fclose(records);

  tactix_c_handle *solver=tactix_init();assert(solver);
  for(int x:{1,2,3,4,5,6,0,-1,-2,-3,-4,-5,-6,0})tactix_add(solver,x);
  int lucky=-1;
  for(int i=0;i<tactix_rule_count();++i)if(!std::strcmp(tactix_rule_name(i),"Lucky"))lucky=i;
  assert(lucky>=0);
  uint64_t instances[32];const int count=tactix_rule_instances(solver,lucky,instances,32);
  assert(count==int(LUCKY_ACTIVE_INSTANCE_COUNT));
  assert(std::find(instances,instances+count,LUCKY_STARTUP_RAMSEY)!=instances+count);
  tactix_online_step step{};step.step_id=1;step.rule_id=lucky;
  step.parameters.kind=TACTIX_ONLINE_PARAMETERS_RAMSEY_STARTUP;
  step.parameters.value.ramsey_startup={10000,200,40000,1,10000000,100000,1000000,200000};
  uint64_t instance=22;step.instances=&instance;step.instance_count=1;
  step.granularity=TACTIX_ONLINE_GRANULARITY_ONE;
  step.repeat_mode=TACTIX_ONLINE_REPEAT_ONCE;step.repeat_limit=1;
  step.work_unit=TACTIX_ONLINE_WORK_APPLICATIONS;step.budget_unlimited=1;
  tactix_online_program cprogram{&step,1,TACTIX_ONLINE_REPEAT_ONCE,1};
  assert(tactix_online_configure(solver,&cprogram,1,TACTIX_ONLINE_RECONFIGURE_RESET)==0);
  step.parameters.value.ramsey_startup.maximum_external_extent=UINT32_MAX;
  assert(tactix_online_configure(solver,&cprogram,2,TACTIX_ONLINE_RECONFIGURE_RESET)==-1);
  tactix_online_result cresult{};assert(tactix_online_advance(solver,&cresult)==0);
  assert(cresult.instance==22); // missing provenance declines, never closes startup
  tactix_release(solver);
}
