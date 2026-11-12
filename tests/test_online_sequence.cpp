#include "unit_test.hpp"
#include "tactix_c_api.hpp"
#include "api/api_internal.hpp"
#include "policies/default_policy.hpp"
#include "strategy/online_sequence.hpp"
#include "policies/strategy_settings.hpp"
#include "query_test_support.hpp"
#include <iostream>
using namespace tactix::strategy;

static void readers() {
  tactix_strategy_state out{};out.trail_size=73;
  assert(tactix_read_strategy_state(nullptr,&out)==-1&&out.trail_size==73);
  uint64_t bound=73;
  assert(tactix_online_repeat_round_limit(nullptr,0,1,&bound)==-1&&bound==73);
  auto *s=tactix_init();assert(s);
  auto *other=tactix_init();assert(other);
  auto &k=api_detail::kernel_for_fixed_testing(s);
  const auto epoch=k.strategy_epoch;
  assert(!tactix_read_strategy_state(s,&out));
  assert(out.strategy_epoch==epoch&&!out.trail_size&&!out.stopped&&out.query_status==TACTIX_EXECUTION_COMPLETE);
  volatile sig_atomic_t stop=1;k.stop=&stop;
  assert(!tactix_read_strategy_state(s,&out)&&out.stopped);k.stop=nullptr;
  for(int mode=0;mode<4;++mode) {
    assert(!tactix_online_repeat_round_limit(s,mode,7,&bound));
    assert(bound==repeat_round_limit(k,{static_cast<RepeatMode>(mode),7}));
  }
  for(int mode:{-1,4,256})assert(tactix_online_repeat_round_limit(s,mode,7,&bound)==-1);
  assert(tactix_online_repeat_round_limit(s,TACTIX_ONLINE_REPEAT_COUNT,0,&bound)==-1);
  auto *e=tactix_online_executor_create(s);assert(e);
  assert(tactix_online_executor_rearm(s,e,2,0)==-1);
  assert(!tactix_stat(s,TACTIX_STAT_INPUT_CLOSED)&&k.strategy_epoch==epoch&&k.h);
  tactix_online_program empty{nullptr,0,TACTIX_ONLINE_REPEAT_ONCE,1};
  assert(!tactix_online_executor_configure(s,e,1,&empty,1,0));
  assert(tactix_online_executor_rearm(other,e,2,0)==-1);
  assert(tactix_online_executor_rearm(s,e,2,9)==-1);
  assert(!tactix_online_executor_rearm(s,e,2,TACTIX_ONLINE_RECONFIGURE_RETAIN_BUDGETS));
  tactix_release(s);
  assert(tactix_online_executor_rearm(other,e,3,0)==-1);
  assert(tactix_online_executor_complete(e)==-1);
  assert(tactix_online_executor_consumed(e,1)==UINT64_MAX);
  assert(!tactix_online_executor_clone(e));
  tactix_online_executor_release(e);tactix_release(other);
  // A callback can latch a stop without writing any external stop pointer.
  session::QueryHistory history;query_test::clause(history,{1,2});
  session::QueryKernel query(session::NativeQueryLimits{{256,4096,1024},
      {64,4096,1024,8192,64,1024,8192},1000000});
  assert(history.begin_query()==session::InputStatus::Ok&&query.replay(history)==session::ReplayStatus::Ok);
  DefaultPolicy policy;StrategySettings options;
  auto *view=static_cast<tactix_c_handle *>(api_detail::create_query_execution(query,policy,options,nullptr));assert(view);
  int polls=0;
  query.set_terminate(&polls,[](void *state) {++*static_cast<int *>(state);return 1;});
  assert(query.poll_terminate()&&polls==1);
  assert(!tactix_read_strategy_state(view,&out)&&out.stopped&&polls==1);
  query.reset_termination();
  assert(!tactix_read_strategy_state(view,&out)&&!out.stopped&&polls==1);
  query.set_terminate(nullptr,nullptr);
  api_detail::destroy_query_execution(view);
}

// A rejected F-model is not a query model. The public recovery restart must
// discard its reconstructed cache before handing control to another strategy.
static void restart_candidate_cache() {
  session::QueryHistory history;query_test::clause(history,{1,2});
  assert(history.assume(-1)==session::InputStatus::Ok);
  session::QueryKernel query(session::NativeQueryLimits{{256,4096,1024},
      {64,4096,1024,8192,64,1024,8192},1000000});
  assert(history.begin_query()==session::InputStatus::Ok&&query.replay(history)==session::ReplayStatus::Ok);
  DefaultPolicy policy;StrategySettings options;
  auto *view=static_cast<tactix_c_handle *>(api_detail::create_query_execution(query,policy,options,nullptr));assert(view);
  const auto ticket=query.ticket();auto &k=*query.kernel(ticket);
  assert(tactix_apply_instance(view,tactix_rule_id("Lucky"),TACTIX_INSTANCE_CANONICAL)==0);
  assert(query.check_model(ticket).status==session::ModelStatus::FalsifiedAssumption);
  assert(k.model_extended);
  assert(tactix_support(view,TACTIX_SUPPORT_PREPARATION_RESTART,0)>=0);
  assert(!k.model_extended&&k.model_values.empty());
  assert(tactix_solve(view)==10&&query.check_model(ticket).status==session::ModelStatus::Valid);
  api_detail::destroy_query_execution(view);
}

static OnlineStrategyStage request(Kernel &k,const std::string &kind) {
  OnlineInvocation producer;
  producer.id=1;producer.rule="SubstitutionRedundancy";
  producer.granularity=InvocationGranularity::OneInstance;
  producer.substitution_redundancy_instances={{{internalize(1)},{},{}}};
  producer.post_application=PostApplication::Propagate;
  OnlineInvocation cleanup;
  cleanup.id=2;cleanup.rule="Reduce";
  ReduceParameters p;p.tier1=p.tier2=0;p.victim_numerator=0;p.victim_denominator=2;
  p.compact_collection=true;p.reduction_bookkeeping=false;
  cleanup.parameters=p;cleanup.granularity=InvocationGranularity::CanonicalApplication;
  if(kind=="pool"||kind=="pool-zero"||kind=="pool-free") {
    producer.substitution_redundancy_instances.clear();
    producer.granularity=InvocationGranularity::CompleteCurrentPass;
    SubstitutionRedundancyParameters parameters;
#define SR_FIELD(native, public_field, value) parameters.native=static_cast<decltype(parameters.native)>(value);
#include "sequence_graph_profile.inc"
#undef SR_FIELD
    producer.parameters=parameters;
  }
  OnlineStrategyStage stage;
  stage.program.sequence={producer,cleanup};stage.progress_followups={2};
  stage.on_budget_exhaustion=BudgetExhaustionPolicy::Continue;
  if(kind=="before") {stage.before_progress={2};stage.progress_followups.clear();}
  if(kind=="batch") {
    auto &step=stage.program.sequence[0];
    step.granularity=InvocationGranularity::SelectedBatch;
    step.substitution_redundancy_instances.push_back(step.substitution_redundancy_instances[0]);
    stage.before_progress={1};stage.program.repeat={RepeatMode::UntilScopedFixpoint,0};
  }
  if(kind=="continue"||kind=="external"||kind=="stop"||kind=="finish") {
    stage.program.sequence[0].budget=WorkLimit::bounded(0);
    stage.program.repeat={RepeatMode::Count,3};
    if(kind=="stop")stage.on_budget_exhaustion=BudgetExhaustionPolicy::Stop;
    if(kind=="finish")stage.on_budget_exhaustion=BudgetExhaustionPolicy::FinishRoundThenStop;
  }
  if(kind=="scaled")stage.program.repeat={RepeatMode::UntilSizeScaledFixpoint,3};
  if(kind=="retain") {
    stage.program.sequence[0].budget=WorkLimit::bounded(1);
    stage.program.repeat={RepeatMode::Count,2};
  }
  return stage;
}

static void row(const InvocationResult &r,const OnlineStrategySequence &sequence,size_t count,
                const Allowance *pool) {
  std::cout<<"row "<<static_cast<int>(r.status)<<' '<<static_cast<int>(r.rule_result)<<' '
      <<static_cast<int>(r.budget_status)<<' '<<static_cast<int>(r.work_unit)<<' '
      <<r.work<<' '<<r.applications<<' '<<r.step_id<<' '<<r.rule_id<<' '<<r.instance<<' '
      <<r.made_rule_progress<<' '<<r.selection_complete<<' '<<r.configuration_revision<<' '
      <<r.before.arena_generation<<' '<<r.before.touch_epoch<<' '<<r.before.strategy_epoch<<' '
      <<r.after.arena_generation<<' '<<r.after.touch_epoch<<' '<<r.after.strategy_epoch<<' '
      <<r.discovery_progress_units<<' '<<r.discovery_entry_required<<' '<<r.discovery_exhausted<<' '
      <<sequence.rounds()<<' '<<sequence.complete();
  for(size_t i=0;i<count;++i)std::cout<<' '<<sequence.dispatches(i)<<' '<<sequence.consumed(i);
  std::cout<<' '<<(pool?pool->limit().amount():0)<<' '<<(pool?pool->consumed():0)
      <<' '<<(pool?pool->limit().is_unlimited():true)<<' '<<(pool?pool->accounting_overflow():false);
  std::cout<<'\n';
}

int main(int argc,char **argv) {
  readers();restart_candidate_cache();if(argc==1)return 0;assert(argc==6);
  const std::string kind=argv[1];const int format=std::atoi(argv[4]);
  auto *s=tactix_init();assert(s);
  auto &k=api_detail::kernel_for_fixed_testing(s);
  if(format)assert(!tactix_proof(s,argv[3],format));
  FILE *input=std::fopen(argv[2],"r");assert(input);
  assert(!tactix_read_dimacs(s,input));std::fclose(input);
  int verdict=tactix_status(s)==20?20:0;
  if(!verdict) {
    verdict=tactix_propagate(s);assert(verdict==0||verdict==20);
  }
  if(!verdict) {
    assert(tactix_support(s,TACTIX_SUPPORT_ENTER_CONTEXT,TACTIX_CONTEXT_STARTUP)>=0);
    assert(tactix_support(s,TACTIX_SUPPORT_ENTER_CONTEXT,TACTIX_CONTEXT_INPROCESSING)>=0);
    auto stage=request(k,kind);OnlineStrategySequence sequence;
    const bool pooled=kind=="pool"||kind=="pool-zero";
    Allowance pool(WorkUnit::SrDiscoveryTicks,WorkLimit::bounded(kind=="pool"?1:0));
    assert(sequence.configure(stage,7));
    bool requested_entry=false,resumed=false,feedback_raised=false;
    uint64_t blocked_step=0;
    const auto initial_sr=k.stats.sr_applied;
    for(unsigned call=0;!sequence.complete();++call) {
      assert(call<2000);
      const auto limit_before=pool.limit().amount();
      const auto result=sequence.advance(k,nullptr,pooled?&pool:nullptr,
          pooled?DiscoveryFeedbackPolicy{1024,20000000}:DiscoveryFeedbackPolicy{});
      row(result,sequence,stage.program.sequence.size(),pooled?&pool:nullptr);
      if(pooled) {
        feedback_raised|=pool.limit().amount()>limit_before;
        if(blocked_step&&result.step_id==blocked_step)resumed=true;
        if(!result.discovery_entry_required)assert(pool.consumed()<=pool.limit().amount());
        if(result.discovery_entry_required) {
          if(kind=="pool")assert(limit_before>0);
          assert(result.status==InvocationStatus::BudgetExhausted&&pool.consumed()>=limit_before);
          assert(result.step_id==0||result.step_id==stage.program.sequence.front().id);
          requested_entry=true;blocked_step=stage.program.sequence.front().id;
        }
      } else if(kind=="pool-free")assert(!result.discovery_entry_required);
      if(pooled&&(result.discovery_progress_units||result.discovery_entry_required))
        pool.set_limit(WorkLimit::bounded(std::max(pool.limit().amount(),pool.consumed())+8192));
      if(result.status==InvocationStatus::Sat||result.status==InvocationStatus::Unsat) {
        verdict=result.status==InvocationStatus::Sat?10:20;break;
      }
      if(kind=="external"&&!call)assert(tactix_propagate(s)>=0);
      if(kind=="retain"&&!call) {
        std::reverse(stage.program.sequence.begin(),stage.program.sequence.end());
        assert(sequence.configure(stage,8,ReconfigureMode::RetainMatchingStepBudgets));
      }
      if(kind=="stop"&&call==2)break;
      assert(result.status==InvocationStatus::Completed||result.status==InvocationStatus::NoChange||
             result.status==InvocationStatus::ScopedFixpoint||result.status==InvocationStatus::BudgetExhausted);
    }
    if(pooled) {
      std::cout<<"pool-control "<<requested_entry<<' '<<resumed<<' '<<feedback_raised<<' '<<pool.consumed()<<' '<<(k.stats.sr_applied-initial_sr)<<'\n';
      assert(requested_entry&&resumed&&feedback_raised&&pool.consumed()>0&&k.stats.sr_applied>initial_sr);
    }
    assert(tactix_support(s,TACTIX_SUPPORT_LEAVE_CONTEXT,0)>=0);
    assert(tactix_support(s,TACTIX_SUPPORT_LEAVE_CONTEXT,0)>=0);
  }
  std::cout<<"prefix "<<verdict<<'\n';
  if(std::atoi(argv[5])&&!verdict)verdict=tactix_solve(s);
  std::cout<<"verdict "<<verdict<<'\n';
  if(verdict==20)assert(tactix_status(s)==20);
  if(verdict==10) {
    kernel_extend_model(k);
    std::cout<<"model";
    for(uint64_t v=1;v<=tactix_stat(s,TACTIX_STAT_OUTPUT_VARIABLES);++v)
      std::cout<<' '<<tactix_value(s,static_cast<int>(v));
    std::cout<<'\n';
  }
  kernel_check_invariants(k);tactix_release(s);return 0;
}
