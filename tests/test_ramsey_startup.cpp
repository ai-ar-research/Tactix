#include "unit_test.hpp"
#include "dimacs_text.hpp"
#include "rules/lucky/ramsey_startup.hpp"
#include "policies/default_policy.hpp"
#include "strategy/online_inprocess.hpp"
#include "policies/restart_controller.hpp"
#include "rules/semantic_extension/direct_csp_support_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_action.hpp"
#include "rules/semantic_extension/gf3_affine_action.hpp"
#include "rules/semantic_extension/gf3_gaussian_action.hpp"
#include "rules/semantic_extension/gf3_general_row_action.hpp"
#include "rules/lucky/tseitin_cycle_action.hpp"
#include "rules/lucky/lucky.hpp"
#include "strategy/feedback_trace.hpp"
#include "strategy/native_feedback.hpp"
#include "proof/proof.hpp"
#include "tactix_c_api.hpp"
#include <fstream>
#include <sstream>
using namespace tactix::strategy;
using namespace tactix::strategy::controller;

// R(4,4) on 18 vertices: every 4-clique has a red and a blue edge.
static std::string fixture() {
  int edge[18][18]={};int v=0;
  for(int a=0;a<18;++a)for(int b=a+1;b<18;++b)edge[a][b]=edge[b][a]=++v;
  std::ostringstream s;s<<"p cnf 153 6120\n";
  for(int a=0;a<18;++a)for(int b=a+1;b<18;++b)for(int c=b+1;c<18;++c)for(int d=c+1;d<18;++d)
    for(int sign:{1,-1})s<<sign*edge[a][b]<<' '<<sign*edge[a][c]<<' '<<sign*edge[a][d]<<' '<<sign*edge[b][c]<<' '<<sign*edge[b][d]<<' '<<sign*edge[c][d]<<" 0\n";
  return s.str();
}
static RamseyStartupParameters parameters() {
  return {10000,200,40000,10000000,10000000,100000,1000000,200000};
}
static RamseyStartupRequest profile(FILE *out) {return {{10000,200,40000,10000000},10000000,100000,1000000,200000,41,out};}
static OnlineInvocation invocation() {
  OnlineInvocation step;step.id=1;step.rule="Lucky";step.parameters=parameters();
  step.instances={LUCKY_STARTUP_RAMSEY};
  step.granularity=InvocationGranularity::OneInstance;
  return step;
}

// Every resource, guard and snapshot failure declines without mutation.
static void controls(const std::string &cnf) {
  Kernel k;parse_dimacs_text(cnf,k);FILE *out=std::tmpfile();assert(out);auto r=profile(out);
  const auto map=k.external_to_internal;const auto raw=k.startup_input;const auto ids=k.next_clause_id;const auto n=k.nvars;const auto arena=k.arena.size();
  const auto no_change=[&](){assert(k.external_to_internal==map&&k.startup_input==raw&&k.next_clause_id==ids&&k.nvars==n&&k.arena.size()==arena&&!k.inconsistent&&k.startup_certificate_open&&std::ftell(out)==0);};
  k.work_budget_bounded=true;k.work_budget=0;
  assert(ramsey_startup_apply(k,r).exhausted);no_change();
  k.work_budget=1;assert(ramsey_startup_apply(k,r).exhausted);no_change();
  k.work_budget=0;k.work_budget_bounded=false;
  auto bad=r;bad.records=nullptr;assert(ramsey_startup_apply(k,bad).result==ApplyResult::NoOp);no_change();
  bad=r;bad.certificate_work=0;assert(ramsey_startup_apply(k,bad).result==ApplyResult::NoOp);no_change();
  bad=r;bad.recognition.work=1;assert(ramsey_startup_apply(k,bad).result==ApplyResult::NoOp);no_change();
  bad=r;bad.maximum_external_extent=map.size();assert(ramsey_startup_apply(k,bad).result==ApplyResult::NoOp);no_change();
  bad=r;bad.maximum_additions=1;assert(ramsey_startup_apply(k,bad).result==ApplyResult::NoOp);no_change();
  bad=r;bad.maximum_nodes=1;assert(ramsey_startup_apply(k,bad).result==ApplyResult::NoOp);no_change();
  k.level=1;assert(!ramsey_startup_guard(k));k.level=0;
  k.pending.valid=true;assert(!ramsey_startup_guard(k));k.pending.valid=false;
  k.large_watches_connected=false;assert(!ramsey_startup_guard(k));k.large_watches_connected=true;
  volatile sig_atomic_t stop=1;k.stop=&stop;assert(ramsey_startup_apply(k,r).interrupted);k.stop=nullptr;no_change();
  // Actual database and retained original snapshot must agree.
  k.startup_input[0]=-k.startup_input[0];assert(ramsey_startup_apply(k,r).result==ApplyResult::NoOp);k.startup_input=raw;no_change();
  const uint64_t before=k.next_clause_id;k.next_clause_id=UINT64_MAX;
  assert(ramsey_startup_apply(k,r).result==ApplyResult::NoOp);k.next_clause_id=before;no_change();
  k.close_startup_certificate();assert(!ramsey_startup_guard(k));assert(ramsey_startup_apply(k,r).result==ApplyResult::NoOp);
  std::fclose(out);
}

// The native apply (or with public_action, the controller's Lucky 22) refutes
// the fixture, extending the variable map only by unmapped helpers.
struct Outcome {RamseyStartupResult result;std::vector<Var> map;uint64_t id;};
static Outcome run(const std::string &cnf,const std::string &prefix,bool with_proof,bool public_action=false) {
  Proof proof;Kernel k;
  if(with_proof){assert(proof.open((prefix+".dsr").c_str(),ProofFormat::DsrAscii));proof.bind_variable_map(&k.internal_to_external);proof.bind_external_variable_map(&k.external_to_internal);k.proof=&proof;}
  parse_dimacs_text(cnf,k);const auto nvars=k.nvars;const auto extent=k.external_to_internal.size();const auto old_map=k.external_to_internal;
  FILE *out=prefix.empty()?std::tmpfile():std::fopen((prefix+".records.jsonl").c_str(),"w+");assert(out);
  RamseyStartupResult r;
  if(public_action) {
    k.structural_provenance=out;k.structural_action_sequence=40;
    RestartController controller(k);ActionRequest request;
    request.sequence=controller.observe().sequence+1;request.expected=state_version(k);
    request.invocation=invocation();
    r.result=controller.apply_rule(request,ActionAllowance{}).invocation.rule_result;
    assert(k.stats.ramsey_startup_attempts==1&&k.stats.ramsey_startup_unsat==1);
    r.recognized=k.stats.ramsey_startup_recognized;
    r.interrupted=k.stats.ramsey_startup_interrupted;r.exhausted=k.stats.ramsey_startup_exhausted;
    r.recognition_work=k.stats.ramsey_startup_recognition_work;
    r.certificate_work=k.stats.ramsey_startup_certificate_work;r.nodes=k.stats.ramsey_startup_nodes;
    r.additions=k.stats.ramsey_startup_additions;r.deletions=k.stats.ramsey_startup_deletions;
    r.helpers=k.stats.ramsey_startup_helpers;
    assert(k.structural_action_sequence==41&&k.ramsey_startup_parameters.recognition_work==0);
    k.structural_provenance=nullptr;
  } else r=ramsey_startup_apply(k,profile(out));
  assert(r.result==ApplyResult::Unsat&&r.recognized&&!r.interrupted&&!r.exhausted);
  assert(k.inconsistent&&!k.startup_certificate_open&&k.startup_input.empty());
  assert(k.nvars==nvars&&k.original_external_vars==extent&&k.external_to_internal.size()==extent+r.helpers);
  assert(std::equal(old_map.begin(),old_map.end(),k.external_to_internal.begin()));
  for(size_t i=extent;i<k.external_to_internal.size();++i)assert(k.external_to_internal[i]==INVALID_VAR);
  assert(ramsey_startup_apply(k,profile(out)).result==ApplyResult::NoOp);
  if(with_proof){proof.flush();assert(std::string(proof.verification_route())=="dsr-trestle");assert(!proof.unbacked_deletions());}
  Outcome answer{r,k.external_to_internal,k.next_clause_id};
  std::fclose(out);k.proof=nullptr;return answer;
}

// DefaultPolicy refutes at startup with the same map, ids and effort with no
// proof, with a proof and recorded feedback, and when replaying that feedback.
struct PolicyOutcome {std::vector<Var> map;uint64_t logical,recognition,certificate,additions;};
static PolicyOutcome execute(const std::string &cnf,const std::string &prefix,
                             bool proof_enabled,bool replay) {
  Kernel k;Proof proof;
  const std::string stem=prefix+"-default";
  if(proof_enabled) {
    assert(proof.open((stem+(replay?"-replay":"")+".dsr").c_str(),ProofFormat::DsrAscii));
    proof.bind_variable_map(&k.internal_to_external);proof.bind_external_variable_map(&k.external_to_internal);k.proof=&proof;
  }
  parse_dimacs_text(cnf,k);const auto variables=k.nvars;const auto original=k.external_to_internal;
  const std::string records_path=stem+(replay?"-replay":"")+(proof_enabled?"":"-no-proof")+".records.jsonl";
  FILE *records=std::fopen(records_path.c_str(),"w+");assert(records);k.structural_provenance=records;
  FeedbackTrace feedback;
  const std::string feedback_path=stem+".feedback.jsonl";
  if(proof_enabled)assert(replay?feedback.replay(feedback_path.c_str(),"ramsey-policy-paths"):feedback.write(feedback_path.c_str(),"ramsey-policy-paths"));
  DefaultStrategyConfig config;assert(config.ramsey_startup.enabled);
  DefaultPolicy policy(config);if(proof_enabled)policy.set_feedback(&feedback);
  assert(policy.solve(k)==20);
  assert(k.stats.ramsey_startup_attempts==1&&k.structural_action_sequence==1);
  assert(k.stats.ramsey_startup_unsat==1&&k.stats.ramsey_startup_recognized==1);
  assert(k.inconsistent&&!k.startup_certificate_open&&k.nvars==variables);
  assert(k.stats.decisions==0&&k.stats.conflicts==0);
  assert(std::equal(original.begin(),original.end(),k.external_to_internal.begin()));
  for(size_t i=original.size();i<k.external_to_internal.size();++i)assert(k.external_to_internal[i]==INVALID_VAR);
  if(proof_enabled) {
    proof.flush();assert(!proof.unbacked_deletions());assert(feedback.terminal()&&feedback.error().empty());
    assert(std::string(proof.verification_route())=="dsr-trestle");
  }
  const PolicyOutcome result{k.external_to_internal,k.next_clause_id,k.stats.ramsey_startup_recognition_work,k.stats.ramsey_startup_certificate_work,k.stats.ramsey_startup_additions};
  k.structural_provenance=nullptr;k.proof=nullptr;std::fclose(records);return result;
}

// On a non-Ramsey input the declined startup leaves the trajectory unchanged.
struct NonRamseyOutcome {
  int verdict;
  std::vector<int8_t> model;
  std::vector<Var> map;
  uint64_t decisions, conflicts, propagations;
};
static NonRamseyOutcome non_ramsey(bool ramsey_enabled) {
  Kernel k;parse_dimacs_text("p cnf 3 2\n1 2 3 0\n-1 -2 -3 0\n",k);
  FILE *records=std::tmpfile();assert(records);k.structural_provenance=records;
  DefaultStrategyConfig config;config.ramsey_startup.enabled=ramsey_enabled;
  const int verdict=DefaultPolicy(config).solve(k);
  assert(verdict==10);
  assert(k.stats.ramsey_startup_attempts==(ramsey_enabled?1u:0u));
  assert(!k.stats.ramsey_startup_recognized&&!k.stats.ramsey_startup_unsat);
  const NonRamseyOutcome result{verdict,k.model_values,k.external_to_internal,
      k.stats.decisions,k.stats.conflicts,k.stats.propagations};
  k.structural_provenance=nullptr;std::fclose(records);return result;
}
static void policy_paths(const std::string &cnf) {
  const std::string prefix="/tmp/tactix-ramsey-policy-paths";
  const auto plain=execute(cnf,prefix,false,false);
  const auto recorded=execute(cnf,prefix,true,false);
  const auto replayed=execute(cnf,prefix,true,true);
  for(const auto *other:{&recorded,&replayed}) {
    assert(plain.map==other->map&&plain.logical==other->logical);
    assert(plain.recognition==other->recognition&&plain.certificate==other->certificate&&plain.additions==other->additions);
  }
  const auto enabled=non_ramsey(true),disabled=non_ramsey(false);
  assert(enabled.verdict==disabled.verdict&&enabled.model==disabled.model);
  assert(enabled.map==disabled.map&&enabled.decisions==disabled.decisions);
  assert(enabled.conflicts==disabled.conflicts&&enabled.propagations==disabled.propagations);
}

struct Capture final:FeedbackReceiver {
  std::vector<ActionFeedback> actions;
  void after_action(const ActionFeedback &a) override {actions.push_back(a);}
  void search_interval(const SearchIntervalFeedback &) override {}
};
// Lucky 22 through the controller and the C API on a non-Ramsey input: a
// scoped budget, idempotent observation and a closed startup all decline.
static void public_action() {
  Kernel k;parse_dimacs_text("p cnf 6 2\n1 2 3 4 5 6 0\n-1 -2 -3 -4 -5 -6 0\n",k);assert(ramsey_startup_guard(k));
  const auto original=k.external_to_internal;const auto id=k.next_clause_id;
  OnlineStrategyProgram decoded;decoded.sequence={invocation()};
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
  k.close_startup_certificate();assert(!ramsey_startup_guard(k));
  assert(apply().invocation.rule_result==ApplyResult::NoOp);
  assert(k.structural_action_sequence==2);
  k.structural_provenance=nullptr;std::fclose(records);

  tactix_c_handle *solver=tactix_init();assert(solver);
  for(int x:{1,2,3,4,5,6,0,-1,-2,-3,-4,-5,-6,0})tactix_add(solver,x);
  const int lucky=tactix_rule_id("Lucky");assert(lucky>=0);
  tactix_online_step step{};step.step_id=1;step.rule_id=lucky;
  step.parameters.kind=TACTIX_ONLINE_PARAMETERS_RAMSEY_STARTUP;
  step.parameters.value.ramsey_startup={10000,200,40000,1,10000000,100000,1000000,200000};
  uint64_t instance=LUCKY_STARTUP_RAMSEY;step.instances=&instance;step.instance_count=1;
  step.granularity=TACTIX_ONLINE_GRANULARITY_ONE;
  step.repeat_mode=TACTIX_ONLINE_REPEAT_ONCE;step.repeat_limit=1;
  step.work_unit=TACTIX_ONLINE_WORK_APPLICATIONS;step.budget_unlimited=1;
  tactix_online_program cprogram{&step,1,TACTIX_ONLINE_REPEAT_ONCE,1};
  assert(tactix_online_configure(solver,&cprogram,1,TACTIX_ONLINE_RECONFIGURE_RESET)==0);
  step.parameters.value.ramsey_startup.maximum_external_extent=UINT32_MAX;
  assert(tactix_online_configure(solver,&cprogram,2,TACTIX_ONLINE_RECONFIGURE_RESET)==-1);
  tactix_online_result cresult{};assert(tactix_online_advance(solver,&cresult)==0);
  assert(cresult.instance==LUCKY_STARTUP_RAMSEY); // missing provenance declines, never closes startup
  tactix_release(solver);
}

// Optional probe: test_ramsey_startup CNF PREFIX [public] writes PREFIX.dsr and
// PREFIX.records.jsonl for the native (or public) refutation.
int main(int argc,char **argv) {
  std::string cnf=fixture(),prefix;
  if(argc==3||argc==4){std::ifstream in(argv[1]);assert(in);std::ostringstream s;s<<in.rdbuf();cnf=s.str();prefix=argv[2];}
  else assert(argc==1);
  controls(cnf);
  const auto none=run(cnf,"",false);
  if(!prefix.empty()) {
    const auto emitted=run(cnf,prefix,true,argc==4);
    assert(none.map==emitted.map&&none.id==emitted.id);
    assert(none.result.additions==emitted.result.additions&&none.result.deletions==emitted.result.deletions&&none.result.certificate_work==emitted.result.certificate_work);
    return 0;
  }
  // The controller route produces the same refutation as the native apply.
  const auto routed=run(cnf,"",false,true);
  assert(none.map==routed.map&&none.id==routed.id);
  assert(none.result.additions==routed.result.additions&&none.result.certificate_work==routed.result.certificate_work);
  policy_paths(cnf);
  public_action();
}
