#undef NDEBUG
#include "policies/random_controller_policy.hpp"
#include "frontend/parse.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>
using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;
namespace gc = tactix::direct_csp_guarded_cover;

static DirectCspGuardedCoverOfferLimits limits();

struct LogicalImage {
  uint64_t next_clause_id;
  uint64_t structural_action_sequence;
  std::vector<uint32_t> arena;
  std::vector<Lit> trail;
  std::vector<signed char> values;
  long provenance_bytes;
  bool conflict_valid;

  bool operator==(const LogicalImage &) const = default;
};

static LogicalImage logical_image(const Kernel &k) {
  return {k.next_clause_id,
          k.structural_action_sequence,
          {k.arena.data(), k.arena.data() + k.arena.size()},
          k.trail,
          k.values,
          k.structural_provenance ? std::ftell(k.structural_provenance) : 0,
          k.conflict_valid};
}

static std::vector<int32_t> consequence(const gc::Selection &selection) {
  std::vector<int32_t> clause;
  for (size_t d = 0; d < selection.domains.size(); ++d)
    clause.push_back(-selection.domains[d][selection.selected_values[d]]);
  std::sort(clause.begin(), clause.end());
  return clause;
}

static bool contains_clause(const Kernel &k, const std::vector<int32_t> &wanted) {
  if (wanted.size() == 2) {
    const Lit first=kernel_internalize(k,wanted[0]);
    const Lit second=kernel_internalize(k,wanted[1]);
    return first!=INVALID_LIT && second!=INVALID_LIT &&
           kernel_binary_live(k,first,second);
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    ref += clause_actual_words(clause);
    if (clause->garbage() || clause->size != wanted.size()) continue;
    std::vector<int32_t> actual;
    for (uint32_t i = 0; i < clause->size; ++i)
      actual.push_back(kernel_externalize(k, clause->lits[i]));
    std::sort(actual.begin(), actual.end());
    if (actual == wanted) return true;
  }
  return false;
}

static std::string provenance(FILE *file) {
  assert(!std::fflush(file));
  const long size = std::ftell(file);
  assert(size >= 0 && !std::fseek(file, 0, SEEK_SET));
  std::string result(static_cast<size_t>(size), '\0');
  assert(std::fread(result.data(), 1, result.size(), file) == result.size());
  assert(!std::fseek(file, size, SEEK_SET));
  return result;
}

static gc::Selection fixture_selection(bool signed_domain) {
  gc::Selection selection{{{1,2,3},{4,5},{11,12}},{0,0,0},{8,9,10},
    {{6,{4,7}}},{{8,0,0,0},{9,0,1,0},{10,0,2,0}}};
  if(signed_domain){
    selection.domains[1]={-4,-5};
    selection.exact_or[0].inputs[0]=-4;
  }
  return selection;
}

static void check_original_model(const Kernel &k, bool signed_domain) {
  const auto original = gc::expected_clauses(
      fixture_selection(signed_domain), limits().theory,
      [](uint64_t) { return true; });
  assert(original.status == gc::Status::Planned);
  for (const auto &clause : original.clauses) {
    bool satisfied = false;
    for (int32_t literal : clause)
      satisfied |= kernel_external_value(k, internalize(literal)) > 0;
    assert(satisfied);
  }
}

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
  const auto s=fixture_selection(signed_domain);
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
   const auto before=logical_image(empty);
   assert(selector.choose(empty,controller.observe(),config,request,error));
   assert(request.type==ActionType::ResumeSearch && !request.producer_effort.empty());
   assert(!request.producer_effort.front().complete);
   assert(logical_image(empty)==before);
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
  const auto initial=logical_image(k);
  assert(selector.choose(k,controller.observe(),config,request,error));
  assert(request.type==ActionType::ResumeSearch && !request.producer_effort.empty());
  assert(request.producer_effort.front().status ==
         static_cast<uint8_t>(DirectCspGuardedCoverOfferStatus::ResourceExhausted));
  assert(logical_image(k)==initial && session.committed_keys()==0);
  // Exhaustion remains retry-gated even if an unrelated formula stamp changes.
  ++k.strategy_epoch;
  assert(selector.choose(k,controller.observe(),config,request,error));
  assert(request.type==ActionType::ResumeSearch && request.producer_effort.empty());
  assert(logical_image(k)==initial && session.committed_keys()==0);
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
  assert(logical_image(k)==initial && session.committed_keys()==0);
  assert(request.invocation.granularity==InvocationGranularity::OneInstance);
  assert(request.invocation.direct_csp_guarded_cover_instances.size()==1);
  assert(std::holds_alternative<DirectCspGuardedCoverParameters>(request.invocation.parameters));
  const auto exact=request.invocation.direct_csp_guarded_cover_instances.front();
  const auto encoded=serialize_online_strategy_program({{request.invocation},{RepeatMode::Once,1}});
  OnlineStrategyProgram decoded;
  assert(deserialize_online_strategy_program(encoded,decoded,&error));
  assert(serialize_online_strategy_program(decoded)==encoded);
  assert(decoded.sequence.size()==1 &&
         decoded.sequence.front().direct_csp_guarded_cover_instances.size()==1 &&
         decoded.sequence.front().direct_csp_guarded_cover_instances.front()==exact);
  const auto published=consequence(exact);
  assert(published.size()>=2 && !contains_clause(k,published));
  const auto before_application=logical_image(k);
  const auto result=controller.apply_rule(request,config.allowance("DirectCspSupport"));
  assert(result.invocation.status==InvocationStatus::Completed && session.committed_keys()==1);
  assert(k.next_clause_id==before_application.next_clause_id+1 &&
         contains_clause(k,published));
  assert(session.lookup_committed(exact,limits().theory,[](uint64_t){return true;})==DirectCspGuardedCoverKeyLookup::Present);
  std::ostringstream journal_clause;
  journal_clause<<"\"clause\":[";
  for(size_t i=0;i<published.size();++i){if(i)journal_clause<<',';journal_clause<<published[i];}
  journal_clause<<']';
  const auto journal=provenance(k.structural_provenance);
  assert(journal.find("\"kind\":\"direct-csp-guarded-cover-v2\"")!=std::string::npos &&
         journal.find(journal_clause.str())!=std::string::npos &&
         journal.find("\"antecedents\":[")!=std::string::npos);
  // Subsequent offers cannot replay the consumed durable tuple, and absence
  // never ends choose() or claims exhaustive owner discovery.
  const auto before_duplicate_rejection=logical_image(k);
  assert(selector.choose(k,controller.observe(),config,request,error));
  if(request.type==ActionType::Rule)
    assert(request.invocation.direct_csp_guarded_cover_instances.front()!=exact);
  assert(logical_image(k)==before_duplicate_rejection && session.committed_keys()==1);

  // Continue ordinary CDCL under explicit search assignments.  The selected
  // native clause, rather than an existing source clause, must force the
  // remaining selected-domain literal and be retained as its exact reason.
  const auto &decide=tactix_rules()[tactix_rule_id("Decide")];
  const auto &propagate=tactix_rules()[tactix_rule_id("Propagate")];
  for(int32_t decision:{11,1}){
   assert(kernel_propagation_fixpoint(k) && !k.conflict_valid);
   assert(apply_rule_instance(decide,k,static_cast<uint32_t>(decision))==ApplyResult::Ok);
   const auto propagated=apply_rule(propagate,k);
   assert(propagated==ApplyResult::Ok || propagated==ApplyResult::Sat ||
          propagated==ApplyResult::NoOp);
  }
  const int32_t forced=-exact.domains[1][exact.selected_values[1]];
  assert(kernel_external_value(k,internalize(forced))>0 && !k.conflict_valid &&
         !k.unassigned);
  const auto &reason=k.assigned[var_of(kernel_internalize(k,forced))];
  std::vector<int32_t> cause;
  if(reason.binary){
   cause={forced,kernel_externalize(k,reason.reason)};
   if(k.binary_ids.tracks_values())
    assert(kernel_binary_id(k,kernel_internalize(k,cause[0]),
                              kernel_internalize(k,cause[1]))==k.next_clause_id);
  }else{
   assert(reason.reason!=UNIT_REASON && reason.reason!=DECISION_REASON);
   const Clause *used=clause_at(k.arena,reason.reason);
   for(uint32_t i=0;i<used->size;++i)
    cause.push_back(kernel_externalize(k,used->lits[i]));
  }
  std::sort(cause.begin(),cause.end());
  assert(cause==published);
  check_original_model(k,signed_domain);

  auto malformed=program;malformed.sequence[0].budget=WorkLimit::bounded(0);
  const auto revision=config.guarded_cover_producer.revision;
  const auto before_rejected_configuration=logical_image(k);
  assert(!config.configure_guarded_cover_producer_profile(malformed,limits(),&error));
  assert(config.guarded_cover_producer.revision==revision);
  assert(logical_image(k)==before_rejected_configuration);
  std::fclose(k.structural_provenance);k.structural_provenance=nullptr;
 }
}
