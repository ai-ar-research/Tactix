#undef NDEBUG
#include "rules/lucky/ramsey_startup.hpp"
#include "frontend/parse.hpp"
#include "policies/random_controller_policy.hpp"
#include "rules/lucky/lucky.hpp"
#include "proof/proof.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>
using namespace tactix;
static std::string fixture() {
  int edge[18][18]={};int v=0;
  for(int a=0;a<18;++a)for(int b=a+1;b<18;++b)edge[a][b]=edge[b][a]=++v;
  std::ostringstream s;s<<"p cnf 153 6120\n";
  for(int a=0;a<18;++a)for(int b=a+1;b<18;++b)for(int c=b+1;c<18;++c)for(int d=c+1;d<18;++d)
    for(int sign:{1,-1})s<<sign*edge[a][b]<<' '<<sign*edge[a][c]<<' '<<sign*edge[a][d]<<' '<<sign*edge[b][c]<<' '<<sign*edge[b][d]<<' '<<sign*edge[c][d]<<" 0\n";
  return s.str();
}
static void parse(Kernel &k,const std::string &cnf) {
  FILE *in=std::tmpfile();assert(in);assert(std::fwrite(cnf.data(),1,cnf.size(),in)==cnf.size());std::rewind(in);
  assert(!parse_dimacs(in,k));std::fclose(in);
}
static RamseyStartupRequest profile(FILE *out) {return {{10000,200,40000,10000000},10000000,100000,1000000,200000,41,out};}
static void controls(const std::string &cnf) {
  Kernel k;parse(k,cnf);FILE *out=std::tmpfile();assert(out);auto r=profile(out);
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
  for(auto fmt:{ProofFormat::DratAscii,ProofFormat::Lrat}) {
    char temporary[]="/tmp/tactix-ramsey-format-XXXXXX";const int fd=mkstemp(temporary);assert(fd>=0);close(fd);
    const std::string path=temporary;
    Proof proof;assert(proof.open(path.c_str(),fmt));k.proof=&proof;
    assert(!ramsey_startup_guard(k));assert(ramsey_startup_apply(k,r).result==ApplyResult::NoOp);k.proof=nullptr;
    std::filesystem::remove(path);no_change();
  }
  k.close_startup_certificate();assert(!ramsey_startup_guard(k));assert(ramsey_startup_apply(k,r).result==ApplyResult::NoOp);
  std::fclose(out);
}
struct Outcome {RamseyStartupResult result;std::vector<Var> map;uint64_t id;};
static Outcome run(const std::string &cnf,const std::string &prefix,bool with_proof,bool public_action=false) {
  Proof proof;Kernel k;
  if(with_proof){assert(proof.open((prefix+".dsr").c_str(),ProofFormat::DsrAscii));proof.bind_variable_map(&k.internal_to_external);proof.bind_external_variable_map(&k.external_to_internal);k.proof=&proof;}
  parse(k,cnf);const auto nvars=k.nvars;const auto extent=k.external_to_internal.size();const auto old_map=k.external_to_internal;
  FILE *out=prefix.empty()?std::tmpfile():std::fopen((prefix+".records.jsonl").c_str(),"w+");assert(out);
  RamseyStartupResult r;
  if(public_action) {
    using namespace tactix::strategy;
    using namespace tactix::strategy::controller;
    k.structural_provenance=out;k.structural_action_sequence=40;
    RestartController controller(k);ActionRequest request;
    request.sequence=controller.observe().sequence+1;request.expected=state_version(k);
    request.invocation.id=1;request.invocation.rule="Lucky";
    request.invocation.instances={LUCKY_STARTUP_RAMSEY};
    request.invocation.granularity=InvocationGranularity::OneInstance;
    request.invocation.parameters=RamseyStartupParameters{10000,200,40000,10000000,10000000,100000,1000000,200000};
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
  }
  const auto &r=none.result;
  std::cout<<"{\"native_unsat\":true,\"vertices\":"<<r.vertices<<",\"recognition_work\":"<<r.recognition_work<<",\"certificate_work\":"<<r.certificate_work<<",\"nodes\":"<<r.nodes<<",\"helpers\":"<<r.helpers<<",\"additions\":"<<r.additions<<",\"deletions\":"<<r.deletions<<",\"logical_id\":"<<none.id<<"}\n";
}
