#undef NDEBUG
#include "policies/random_controller_policy.hpp"
#include "frontend/parse.hpp"
#include <cassert>
#include <cstdio>
#include <sstream>
using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;
namespace gc = tactix::direct_csp_guarded_cover;

static DirectCspGuardedCoverOfferLimits limits() {
  return {{100,1000,10,10,10,10,10,10},
          {1000000,100,100,100,100,100,8},
          10000000,1000,10000,1000000,1000,100000,100,8,1000,10000};
}
static OnlineStrategyProgram profile() {
  OnlineInvocation step;
  step.id = 1;
  step.rule="DirectCspSupport";
  step.parameters=DirectCspGuardedCoverParameters{
      100,1000,1000000,10,10,10,10,10,10,10000,10,100,100000,100,10,1000000};
  step.granularity=InvocationGranularity::NativeProducerOffer;
  step.work_unit=WorkUnit::Applications;
  step.budget=WorkLimit::bounded(1);
  step.rule_work_budget={RuleWorkBudgetMode::Bounded,1000000};
  return {{step},{RepeatMode::Once,1}};
}
static void populate(Kernel &k, bool signed_domain) {
  gc::Selection s{{{1,2,3},{4,5},{11,12}},{0,0,0},{8,9,10},
    {{6,{4,7}}},{{8,0,0,0},{9,0,1,0},{10,0,2,0}}};
  if(signed_domain){s.domains[1]={-4,-5};s.exact_or[0].inputs[0]=-4;}
  auto expected=gc::expected_clauses(s,limits().theory,[](uint64_t){return true;});
  assert(expected.status==gc::Status::Planned);
  std::ostringstream text;text<<"p cnf 12 "<<expected.clauses.size()<<'\n';
  for(const auto &c:expected.clauses){for(auto x:c)text<<x<<' ';text<<"0\n";}
  const auto bytes=text.str();FILE *input=std::tmpfile();assert(input);
  assert(std::fwrite(bytes.data(),1,bytes.size(),input)==bytes.size());std::rewind(input);
  assert(!parse_dimacs(input,k));std::fclose(input);
  k.structural_provenance=std::tmpfile();assert(k.structural_provenance);
}
static ActionRequest select_rule(Kernel &k, RestartController &controller,
    RandomControllerSelector &selector,const RandomControllerConfig &config) {
  std::string error;
  for(unsigned i=0;i<128;++i){
    ActionRequest request;
    assert(selector.choose(k,controller.observe(),config,request,error));
    if(request.type==ActionType::Rule)return request;
  }
  assert(false);return {};
}
int main(){
 {
  Kernel empty;FILE *f=std::tmpfile();assert(f);
  const char input[]="p cnf 12 0\n";
  assert(std::fwrite(input,1,sizeof(input)-1,f)==sizeof(input)-1);std::rewind(f);
  assert(!parse_dimacs(f,empty));std::fclose(f);
  empty.structural_provenance=std::tmpfile();assert(empty.structural_provenance);
  DirectCspGuardedCoverActionSession actions;empty.direct_csp_guarded_cover_action_session=&actions;
  RandomControllerConfig config;config.parameters.rules={"DirectCspSupport"};
  std::string error;
  const bool configured = config.configure_guarded_cover_producer_profile(profile(),limits(),&error);
  if (!configured) std::fprintf(stderr, "GuardedCover producer configuration: %s\n", error.c_str());
  assert(configured);
  RestartController controller(empty);RandomControllerSelector selector(7);
  for(unsigned i=0;i<3;++i){ActionRequest request;
   assert(selector.choose(empty,controller.observe(),config,request,error));
   assert(request.type==ActionType::ResumeSearch && !request.producer_effort.empty());
   assert(!request.producer_effort.front().complete);
  }
  std::fclose(empty.structural_provenance);empty.structural_provenance=nullptr;
 }
 for(bool signed_domain:{false,true}){
  Kernel k;populate(k,signed_domain);
  DirectCspGuardedCoverActionSession session;k.direct_csp_guarded_cover_action_session=&session;
  RestartController controller(k);RandomControllerSelector selector(7);
  RandomControllerConfig config;config.parameters.rules={"DirectCspSupport"};
  assert(!config.guarded_cover_producer.enabled);
  config.guarded_cover_producer.window.preferred_literals={1,signed_domain?-4:4,11};
  std::string error;auto program=profile();auto bounded=limits();
  // Admit the authored preference snapshot, then exhaust during discovery.
  bounded.maximum_work=config.guarded_cover_producer.window.preferred_literals.size();
  assert(config.configure_guarded_cover_producer_profile(program,bounded,&error));
  ActionRequest request;
  assert(selector.choose(k,controller.observe(),config,request,error));
  assert(request.type==ActionType::ResumeSearch && !request.producer_effort.empty());
  assert(request.producer_effort.front().status ==
         static_cast<uint8_t>(DirectCspGuardedCoverOfferStatus::ResourceExhausted));
  // Exhaustion remains retry-gated even if an unrelated formula stamp changes.
  ++k.strategy_epoch;
  assert(selector.choose(k,controller.observe(),config,request,error));
  assert(request.type==ActionType::ResumeSearch && request.producer_effort.empty());
  assert(config.configure_guarded_cover_producer_profile(program,limits(),&error));
  const auto identity=controller_configuration(config);
  auto changed=config;
  ++changed.guarded_cover_producer.limits.maximum_binary_edges;
  assert(controller_configuration(changed)!=identity);
  changed=config;changed.guarded_cover_producer.window.preferred_literals[0]=2;
  assert(controller_configuration(changed)!=identity);
  changed=config;++changed.guarded_cover_producer.window.role_alternative;
  assert(controller_configuration(changed)!=identity);
  request=select_rule(k,controller,selector,config);
  assert(request.invocation.granularity==InvocationGranularity::OneInstance);
  assert(request.invocation.direct_csp_guarded_cover_instances.size()==1);
  assert(std::holds_alternative<DirectCspGuardedCoverParameters>(request.invocation.parameters));
  const auto exact=request.invocation.direct_csp_guarded_cover_instances.front();
  const auto encoded=serialize_online_strategy_program({{request.invocation},{RepeatMode::Once,1}});
  OnlineStrategyProgram decoded;
  assert(deserialize_online_strategy_program(encoded,decoded,&error));
  assert(serialize_online_strategy_program(decoded)==encoded);
  const auto result=controller.apply_rule(request,config.allowance("DirectCspSupport"));
  assert(result.invocation.status==InvocationStatus::Completed && session.committed_keys()==1);
  assert(session.lookup_committed(exact,limits().theory,[](uint64_t){return true;})==DirectCspGuardedCoverKeyLookup::Present);
  // Subsequent offers cannot replay the consumed durable tuple, and absence
  // never ends choose() or claims exhaustive owner discovery.
  assert(selector.choose(k,controller.observe(),config,request,error));
  if(request.type==ActionType::Rule)
    assert(request.invocation.direct_csp_guarded_cover_instances.front()!=exact);
  auto malformed=program;malformed.sequence[0].budget=WorkLimit::bounded(0);
  const auto revision=config.guarded_cover_producer.revision;
  assert(!config.configure_guarded_cover_producer_profile(malformed,limits(),&error));
  assert(config.guarded_cover_producer.revision==revision);
  std::fclose(k.structural_provenance);k.structural_provenance=nullptr;
 }
}
