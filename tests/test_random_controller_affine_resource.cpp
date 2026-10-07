#undef NDEBUG
#include "policies/random_controller_policy.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"
#include "rules/semantic_extension/affine_image.hpp"
#include "proof/proof.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include <unistd.h>
using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;
namespace {
strategy::AffineConsequenceParameters parameters() {
  strategy::AffineConsequenceParameters p;
  p.maximum_clauses=1000;p.maximum_snapshot_literals=10000;p.maximum_snapshot_work=1000000;
  p.maximum_groups=100;p.maximum_patterns=1000;p.maximum_elimination_work=1000000;
  p.maximum_arity=4;p.maximum_intermediate_width=16;p.maximum_sources=32;p.maximum_output_width=4;
  p.maximum_proof_additions=100000;p.maximum_proof_deletions=100000;p.maximum_journal_bytes=1000000;
  p.semantics=strategy::AffineConsequenceSemantics::CertifiedBasisV2;
  p.basis_maximum_work=10000000;p.basis_maximum_intermediate_width=16;p.basis_maximum_sources=32;
  p.basis_maximum_premises=10000;p.basis_maximum_nodes=10000;p.basis_maximum_direct_width=3;
  p.maximum_external_extent=128;p.maximum_session_keys=1000;p.maximum_session_work=1000000;
  assert(p.valid());return p;
}
struct Fixture {
 Kernel k;Proof proof;AffineMatrixSearchSession session;
 FILE *journal=nullptr;std::string path;
 std::vector<std::vector<int32_t>> original;
 AffineConsequenceParameters p=parameters();
 Fixture(){
  kernel_reserve_vars(k,7);p.basis_maximum_work=100;
  for(auto variables:{std::vector<int32_t>{1,2,3},std::vector<int32_t>{2,3,4}})
   for(auto c:affine_semantic_extension::clausal_form(variables,variables.front()==2))original.push_back(c);
  original.push_back({5,6,7});
  for(const auto &c:original){std::vector<Lit> ls;for(int x:c)ls.push_back(kernel_internalize(k,x));
   kernel_add_large_clause(k,ls.data(),ls.size(),false,0,++k.next_clause_id);}
  journal=std::tmpfile();assert(journal);k.structural_provenance=journal;
  path="/tmp/tactix-controller-resource-proof-"+std::to_string(getpid());
  assert(proof.open(path.c_str(),ProofFormat::DsrAscii));proof.bind_variable_map(&k.internal_to_external);proof.bind_external_variable_map(&k.external_to_internal);
  uint64_t id=0;for(const auto &c:original){std::vector<Lit> ls;for(int x:c)ls.push_back(kernel_internalize(k,x));proof.seed_population(++id,ls.data(),ls.size());}proof.arm_population_shadow();
  k.proof=&proof;k.affine_matrix_search_session=&session;
  assert(affine_matrix_search_initialize(k,session,p,AffineMatrixAdmission::AuthenticatedSubimage,{64000000,1000,100000}).status==AffineMatrixInitStatus::Applied);
 }
 ~Fixture(){session.disable(k);k.proof=nullptr;k.affine_matrix_search_session=nullptr;k.structural_provenance=nullptr;std::fclose(journal);std::remove(path.c_str());}
 void check_model(){kernel_extend_model(k);for(const auto &c:original){bool sat=false;for(int x:c)sat|=k.model_values[internalize(x)]>0;assert(sat);}}
 void export_artifacts(const std::string &directory,const char *arm){
  if(directory.empty())return;
  const std::string base=directory+"/"+arm;
  FILE *cnf=std::fopen((base+".cnf").c_str(),"w");assert(cnf);
  std::fprintf(cnf,"p cnf 7 %zu\n",original.size());
  for(const auto &c:original){for(int x:c)std::fprintf(cnf,"%d ",x);std::fputs("0\n",cnf);}
  assert(!std::fclose(cnf));
  FILE *model=std::fopen((base+".model").c_str(),"w");assert(model);
  std::fputs("s SATISFIABLE\nv ",model);
  for(int v=1;v<=7;++v){const int value=k.model_values[internalize(v)];assert(value);std::fprintf(model,"%d ",value>0?v:-v);}
  std::fputs("0\n",model);assert(!std::fclose(model));
  proof.flush();
  FILE *input=std::fopen(path.c_str(),"rb"),*output=std::fopen((base+".dsr").c_str(),"wb");assert(input&&output);
  char buffer[4096];size_t size;
  while((size=std::fread(buffer,1,sizeof buffer,input)))assert(std::fwrite(buffer,1,size,output)==size);
  assert(!std::ferror(input));assert(!std::fclose(input));assert(!std::fclose(output));
 }
};
struct Resume final:ControllerSelection {
 bool choose(Kernel &,const Observation &o,const RandomControllerConfig &,ActionRequest &r,std::string &,volatile sig_atomic_t *) override{
  r.type=ActionType::ResumeSearch;r.sequence=o.sequence+1;r.expected=o.state;r.search={};return true;
 }
};
struct Feedback final:FeedbackReceiver {
 uint64_t resources=0;int verdict=-1;
 void after_action(const ActionFeedback &a) override{if(a.identity=="AffineMatrixSearch" && a.outcome.invocation.status==InvocationStatus::ResourceExhausted)++resources;}
 void search_interval(const SearchIntervalFeedback &) override{}
 void session_end(const Observation &,int v,StopReason) override{verdict=v;}
};
void explicit_decline(){
 Fixture f;RestartController controller(f.k);ActionRequest request;
 request.type=ActionType::Rule;request.sequence=1;request.expected=state_version(f.k);
 request.invocation.id=73;request.invocation.rule="AffineMatrixInitialize";
 request.invocation.granularity=InvocationGranularity::CanonicalApplication;
 AffineMatrixInitializeParameters p;p.basis=f.p;p.maximum_retained_bytes=64000000;p.maximum_deferred_deletions=1000;p.maximum_deferred_literal_work=100000;
 p.operation=AffineMatrixInitializeParameters::Operation::SearchStep;request.invocation.parameters=p;
 request.invocation.repeat={RepeatMode::Once,1};request.invocation.work_unit=WorkUnit::Applications;request.invocation.budget=WorkLimit::bounded(1);
 const auto id=f.k.next_clause_id,owner=f.session.owner_version();
 const auto result=controller.apply_rule(request,{WorkUnit::Applications,1,{RuleWorkBudgetMode::Bounded,1000000}});
 assert(result.invocation.status==InvocationStatus::ResourceExhausted);
 assert(f.session.enabled() && f.k.next_clause_id==id && f.session.owner_version()==owner && f.k.trail.empty());
}
}
int main(int argc,char **argv){
 assert(argc<=2);const std::string directory=argc==2?argv[1]:"";
 if(!directory.empty())std::filesystem::create_directories(directory);
 explicit_decline();
 const std::string path="/tmp/tactix-controller-resource-trace-"+std::to_string(getpid());RandomControllerConfig config;Resume resume;
 {
  Fixture f;ControllerTrace trace;assert(trace.write(path.c_str(),73,controller_configuration(config)));Feedback feedback;
  RandomControllerPolicy policy(73,config,&trace,&resume,&feedback);
  assert(policy.solve(f.k)==10 && trace.error().empty());
  assert(feedback.resources==1 && feedback.verdict==10 && !f.session.enabled());f.check_model();f.export_artifacts(directory,"record");
 }
 {
  Fixture f;ControllerTrace trace;assert(trace.replay(path.c_str(),controller_configuration(config)));Feedback feedback;
  RandomControllerPolicy policy(999,config,&trace,&resume,&feedback);
  assert(policy.solve(f.k)==10 && trace.error().empty());
  assert(feedback.resources==1 && feedback.verdict==10 && !f.session.enabled());f.check_model();f.export_artifacts(directory,"replay");
 }
 std::remove(path.c_str());
}
