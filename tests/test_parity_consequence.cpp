#undef NDEBUG
#include "parity_test_support.hpp"
#include "test_io.hpp"
#include "rules/parity_consequence/parity_consequence.hpp"
#include "rules/lucky/parity_certificate.hpp"
#include "policies/restart_controller.hpp"
#include "strategy/feedback_trace.hpp"
#include "proof/proof.hpp"
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "strategy/online_inprocess.hpp"
#include <cassert>
#include <cstdio>
#include <string>
using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;
using namespace tactix::parity_certificate;
static const std::vector<Equation> equations = parity_cycle();
static ParityConsequenceParameters profile() {
  return {100,100,10000,32,32,2,true,10000,100,0};
}
static OnlineInvocation invocation() {
  OnlineInvocation step; step.id=1; step.rule="ParityConsequence";
  step.parameters=profile();
  step.granularity=InvocationGranularity::CanonicalApplication;
  return step;
}
static void input(Kernel &k) {
  kernel_reserve_vars(k,7);add_xor_clauses(k,equations);
}
struct Capture final:FeedbackReceiver {
  std::vector<ActionFeedback> actions;
  void after_action(const ActionFeedback &a) override {actions.push_back(a);}
  void search_interval(const SearchIntervalFeedback &) override {}
};
static std::vector<Var> exercise(const char *proof_path,const char *trace_path) {
  Kernel k;input(k);FILE *records=std::fopen((std::string(trace_path)+".provenance").c_str(),"w+");assert(records);
  assert(!parity_consequence_guard(k));
  k.structural_provenance=records;assert(parity_consequence_guard(k));
  const auto initial=k.external_to_internal;const uint64_t initial_id=k.next_clause_id;
  k.level=1;assert(!parity_consequence_guard(k));k.level=0;
  k.pending.valid=true;assert(!parity_consequence_guard(k));k.pending.valid=false;
  k.large_watches_connected=false;assert(!parity_consequence_guard(k));k.large_watches_connected=true;
  Proof proof;
  if(proof_path) {
    assert(proof.open(proof_path,ProofFormat::DsrAscii));
    proof.bind_variable_map(&k.internal_to_external);
    proof.bind_external_variable_map(&k.external_to_internal);k.proof=&proof;
  }
  auto step=invocation();OnlineStrategyProgram program;program.sequence={step};
  const std::string wire=serialize_online_strategy_program(program);
  const OnlineStrategyProgram &replay=program;
  assert(wire.find("parity-consequence(100,100,10000,32,32,2,1,10000,100,0)")!=std::string::npos);
  auto bad=replay;std::get<ParityConsequenceParameters>(bad.sequence[0].parameters).output_width=5;
  assert(!validate_online_strategy_program(bad));
  bad=replay;bad.sequence[0].rule="Lucky";assert(!validate_online_strategy_program(bad));
  Capture capture;size_t extent=0;
  {
    RestartController controller(k,{},&capture);
    ActionAllowance allowance;ActionRequest request;
    request.invocation=replay.sequence[0];
    auto apply=[&] {
      request.sequence=controller.observe().sequence+1;
      request.expected=state_version(k);
      return controller.apply_rule(request,allowance);
    };
    std::get<ParityConsequenceParameters>(request.invocation.parameters).maximum_proof_additions=1;
    assert(apply().invocation.rule_result==ApplyResult::NoOp);
    assert(k.external_to_internal==initial&&k.next_clause_id==initial_id);
    assert(k.stats.parity_consequence_preflight_additions==1);
    request.invocation=replay.sequence[0];
    assert(apply().invocation.rule_result==ApplyResult::Ok);
    assert(kernel_binary_live(k,internalize(-1),internalize(2)));
    assert(kernel_binary_live(k,internalize(1),internalize(-2)));
    extent=k.external_to_internal.size();
    assert(apply().invocation.rule_result==ApplyResult::NoOp);
    assert(k.external_to_internal.size()==extent);
    assert(k.stats.parity_consequence_attempts==3);
    assert(k.stats.parity_consequence_published==2);
    assert(k.stats.parity_consequence_additions>0);
    assert(k.stats.parity_consequence_helpers==extent-initial.size());
    assert(k.parity_consequence_parameters.maximum_clauses==0); // scope restored
    assert(capture.actions.size()==3);
    const auto &productive=capture.actions[1];
    assert(productive.after.cumulative.parity_consequence_published-
           productive.before.cumulative.parity_consequence_published==2);
    assert(productive.request&&std::holds_alternative<ParityConsequenceParameters>(productive.request->invocation.parameters));
    // All counters and requests survive strict deterministic feedback replay.
    FeedbackTrace writer;assert(writer.write(trace_path,"parity-action-fixture"));
    writer.session_begin(capture.actions.front().before);
    for(const auto &a:capture.actions)writer.after_action(a);
    writer.session_end(controller.observe(),0,StopReason::Detached);
    assert(writer.terminal()&&writer.error().empty());
    FeedbackTrace reader;assert(reader.replay(trace_path,"parity-action-fixture"));
    reader.session_begin(capture.actions.front().before);
    for(const auto &a:capture.actions)reader.after_action(a);
    reader.session_end(controller.observe(),0,StopReason::Detached);
    assert(reader.terminal()&&reader.error().empty());
  }
  const auto journal=stream_contents(records);
  assert(journal.find("\"kind\":\"action-complete\"")!=std::string::npos);
  assert(journal.find("\"kind\":\"publish-xor-consequence\"")!=std::string::npos);
  assert(k.nvars==7&&kernel_num_output_vars(k)==7);
  // Proof-only helper IDs are never reused by later extension variables.
  const Var fresh=kernel_new_extension_vars(k,1);assert(k.internal_to_external[fresh]==extent);
  if(proof_path)proof.flush();
  k.proof=nullptr;
  k.structural_provenance=nullptr;std::fclose(records);return k.external_to_internal;
}
static void public_c() {
  tactix_c_handle *s=tactix_init();FILE *records=std::tmpfile();assert(records);
  assert(tactix_bind_structural_provenance(s,records)==0);
  for(const auto &e:equations)for(const auto &c:xor_equation(e.variables.data(),e.size,e.rhs)) {
    for(unsigned i=0;i<c.size;++i)tactix_add(s,c.lits[i]);
    tactix_add(s,0);
  }
  int id=-1;for(int i=0;i<tactix_rule_count();++i)
    if(std::string(tactix_rule_name(i))=="ParityConsequence")id=i;
  assert(id==29&&tactix_action_kind(id)==TACTIX_ACTION_PROOF_RULE);
  tactix_online_step step{};step.step_id=1;step.rule_id=id;
  step.granularity=TACTIX_ONLINE_GRANULARITY_CANONICAL;
  step.parameters.kind=TACTIX_ONLINE_PARAMETERS_PARITY_CONSEQUENCE;
  step.parameters.value.parity_consequence={100,100,10000,32,32,2,1,10000,100,0};
  step.repeat_mode=TACTIX_ONLINE_REPEAT_ONCE;step.repeat_limit=1;
  step.work_unit=TACTIX_ONLINE_WORK_APPLICATIONS;step.budget_unlimited=1;
  tactix_online_program program{&step,1,TACTIX_ONLINE_REPEAT_ONCE,1};
  assert(tactix_online_configure(s,&program,1,TACTIX_ONLINE_RECONFIGURE_RESET)==0);
  tactix_online_result result{};assert(tactix_online_advance(s,&result)==0);
  assert(result.applications==1&&tactix_stat(s,TACTIX_STAT_PARITY_CONSEQUENCE_PUBLISHED)==2);
  assert(tactix_decide(s,1)==0);
  // A decision-level application is rejected without another structural action.
  assert(tactix_apply(s,id)==-1);
  assert(tactix_stat(s,TACTIX_STAT_PARITY_CONSEQUENCE_ATTEMPTS)==1);
  assert(tactix_bind_structural_provenance(s,nullptr)==0);
  tactix_release(s);std::fclose(records);
}
int main(int argc,char **argv) {
  const std::string base=argc>1?argv[1]:"/tmp/tactix-parity-consequence";
  assert(exercise(nullptr,(base+"-no-proof-feedback.jsonl").c_str())==
         exercise((base+".dsr").c_str(),(base+"-feedback.jsonl").c_str()));
  public_c();std::puts("test_parity_consequence: all passed");
}
