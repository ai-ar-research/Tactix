#undef NDEBUG
#include "policies/default_policy.hpp"
#include "policies/random_controller_policy.hpp"
#include "rules/lucky/lucky.hpp"
#include "strategy/feedback_trace.hpp"
#include "frontend/parse.hpp"
#include <cassert>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;
static RamseyStartupParameters profile() {
  return {10000,200,40000,10000000,10000000,100000,1000000,200000};
}
static void input(Kernel &k,const std::string &cnf) {
  FILE *file=std::tmpfile();assert(file);
  assert(std::fwrite(cnf.data(),1,cnf.size(),file)==cnf.size());std::rewind(file);
  assert(!parse_dimacs(file,k));std::fclose(file);
}
static std::string fixture() {
  int edge[18][18]={};int v=0;std::ostringstream out;out<<"p cnf 153 6120\n";
  for(int a=0;a<18;++a)for(int b=a+1;b<18;++b)edge[a][b]=++v;
  for(int a=0;a<18;++a)for(int b=a+1;b<18;++b)for(int c=b+1;c<18;++c)for(int d=c+1;d<18;++d)
    for(int sign:{1,-1})out<<sign*edge[a][b]<<' '<<sign*edge[a][c]<<' '<<sign*edge[a][d]<<' '<<sign*edge[b][c]<<' '<<sign*edge[b][d]<<' '<<sign*edge[c][d]<<" 0\n";
  return out.str();
}
static RandomControllerConfig random_config() {
  OnlineInvocation yes;yes.id=1;yes.rule="Lucky";yes.parameters=profile();
  yes.granularity=InvocationGranularity::OneInstance;yes.instances={LUCKY_STARTUP_RAMSEY};
  yes.budget=WorkLimit::bounded(1);
  yes.rule_work_budget={RuleWorkBudgetMode::Bounded,20000000};
  auto no=yes;no.id=2;std::get<RamseyStartupParameters>(no.parameters).certificate_work=0;
  OnlineStrategyProgram choices;choices.sequence={yes,no};
  RandomControllerConfig config;assert(config.configure_actions(choices));return config;
}
// Find a deterministic selection witness for a NoOp followed by productive
// Ramsey. This is targeted coverage, not seed selection for performance.
static uint64_t seed_for_two_actions(const std::string &cnf,const RandomControllerConfig &config) {
  Kernel k;input(k,cnf);RestartController controller(k);
  for(uint64_t seed=1;seed<=1000;++seed) {
    RandomControllerSelector selector(seed);ActionRequest first,second;std::string error;
    assert(selector.choose(k,controller.observe(),config,first,error));
    assert(selector.choose(k,controller.observe(),config,second,error));
    if(first.type==ActionType::Rule && second.type==ActionType::Rule &&
       !std::get<RamseyStartupParameters>(first.invocation.parameters).certificate_work &&
       std::get<RamseyStartupParameters>(second.invocation.parameters).certificate_work) return seed;
  }
  assert(false);return 0;
}
struct Outcome {std::vector<Var> map;uint64_t logical,recognition,certificate,additions;};
static Outcome execute(const std::string &cnf,const std::string &prefix,bool random,
                       uint64_t seed,bool proof_enabled,bool replay) {
  Kernel k;Proof proof;
  const std::string suffix=random?"-random":"-default";
  const std::string proof_path=prefix+suffix+(replay?"-replay":"")+".dsr";
  if(proof_enabled) {
    assert(proof.open(proof_path.c_str(),ProofFormat::DsrAscii));
    proof.bind_variable_map(&k.internal_to_external);proof.bind_external_variable_map(&k.external_to_internal);k.proof=&proof;
  }
  input(k,cnf);const auto variables=k.nvars;const auto original=k.external_to_internal;
  const std::string records_path=prefix+suffix+(replay?"-replay":"")+(proof_enabled?"":"-no-proof")+".records.jsonl";
  FILE *records=std::fopen(records_path.c_str(),"w+");assert(records);k.structural_provenance=records;
  FeedbackTrace feedback;
  const std::string feedback_path=prefix+suffix+".feedback.jsonl";
  if(proof_enabled)assert(replay?feedback.replay(feedback_path.c_str(),"ramsey-policy-paths"):feedback.write(feedback_path.c_str(),"ramsey-policy-paths"));
  if(random) {
    const auto config=random_config();ControllerTrace trace;
    const std::string path=prefix+"-random.controller";
    if(proof_enabled)assert(replay?trace.replay(path.c_str(),controller_configuration(config)):trace.write(path.c_str(),seed,controller_configuration(config)));
    RandomControllerPolicy policy(seed,config,proof_enabled?&trace:nullptr,nullptr,proof_enabled?&feedback:nullptr);
    assert(policy.solve(k)==20);
    assert(policy.coverage().rules.at("Lucky").applications==2);
    assert(k.stats.ramsey_startup_attempts==2&&k.structural_action_sequence==2);
    assert(trace.error().empty());
  } else {
    DefaultStrategyConfig config;config.ramsey_startup.enabled=true;config.ramsey_startup.parameters=profile();
    DefaultPolicy policy(config);if(proof_enabled)policy.set_feedback(&feedback);
    assert(policy.solve(k)==20);
    assert(k.stats.ramsey_startup_attempts==1&&k.structural_action_sequence==1);
  }
  assert(k.stats.ramsey_startup_unsat==1&&k.stats.ramsey_startup_recognized==1);
  assert(k.inconsistent&&!k.startup_certificate_open&&k.nvars==variables);
  assert(k.stats.decisions==0&&k.stats.conflicts==0);
  assert(std::equal(original.begin(),original.end(),k.external_to_internal.begin()));
  for(size_t i=original.size();i<k.external_to_internal.size();++i)assert(k.external_to_internal[i]==INVALID_VAR);
  if(proof_enabled) {
    proof.flush();assert(!proof.unbacked_deletions());assert(feedback.terminal()&&feedback.error().empty());
    assert(std::string(proof.verification_route())=="dsr-trestle");
  }
  const Outcome result{k.external_to_internal,k.next_clause_id,k.stats.ramsey_startup_recognition_work,k.stats.ramsey_startup_certificate_work,k.stats.ramsey_startup_additions};
  k.structural_provenance=nullptr;k.proof=nullptr;std::fclose(records);return result;
}
int main(int argc,char **argv) {
  std::string cnf=fixture(),prefix="/tmp/tactix-ramsey-policy-paths";
  if(argc==3){std::ifstream in(argv[1]);assert(in);std::ostringstream buffer;buffer<<in.rdbuf();cnf=buffer.str();prefix=argv[2];}
  else assert(argc==1);
  const uint64_t seed=seed_for_two_actions(cnf,random_config());
  Outcome reference;
  for(bool random:{false,true}) {
    const auto plain=execute(cnf,prefix,random,seed,false,false);
    const auto recorded=execute(cnf,prefix,random,seed,true,false);
    const auto replayed=execute(cnf,prefix,random,seed,true,true);
    for(const auto *other:{&recorded,&replayed}) {
      assert(plain.map==other->map&&plain.logical==other->logical);
      assert(plain.recognition==other->recognition&&plain.certificate==other->certificate&&plain.additions==other->additions);
    }
    if(!random)reference=plain;
    else assert(reference.map==plain.map&&reference.logical==plain.logical&&reference.additions==plain.additions);
  }
  std::cout<<"{\"policy_paths_verified\":true,\"random_seed\":"<<seed<<",\"logical_id\":"<<reference.logical<<",\"additions\":"<<reference.additions<<"}\n";
}
