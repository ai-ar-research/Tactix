// Bind this include to the immutable source archive, not the working checkout.
// This scratch TU supplies the authentic C API implementation; link excludes
// archived api.cpp.o. Kernel access below is read-only diagnostics only.
#include "api/api.cpp"
#include "selection.hpp"
#include <algorithm>
#include <openssl/sha.h>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

static tactix_solver *active_solver=nullptr;
static FILE *active_provenance=nullptr;
static const char *failure_stage="configuration";
static const char *active_arm="unknown";
[[noreturn]] static void fail(const char *message) {
 std::cerr<<"pilot hard stop: "<<message<<'\n';
 std::cout<<"{\"record\":\"terminal\",\"arm\":\""<<active_arm<<"\",\"context\":\""<<message<<"\",\"failure_stage\":\""<<failure_stage<<"\",\"driver_exit\":3,\"mechanism_complete\":false}\n";
 if(active_solver)tactix_release(active_solver);
 if(active_provenance)fclose(active_provenance);
 std::exit(3);
}
struct InputCounts { uint64_t variables=0,clauses=0,literals=0,sort_pairs=0,max_width=0; };
static uint64_t bounded(__uint128_t x) {
 if(x>UINT64_MAX) fail("derived allowance overflow");
 return uint64_t(x);
}
static InputCounts ingest(tactix_solver *s,const char *path) {
 std::ifstream input(path); if(!input) fail("input open");
 InputCounts c; std::string token; uint64_t width=0,seen=0; bool header=false;
 while(input>>token) {
  if(token=="c") { std::string line;std::getline(input,line);continue; }
  if(token=="p") {std::string format;if(header||!(input>>format>>c.variables>>c.clauses)||format!="cnf")fail("header");header=true;continue;}
  int64_t l=0;const auto parsed=std::from_chars(token.data(),token.data()+token.size(),l);
  if(!header||parsed.ec!=std::errc{}||parsed.ptr!=token.data()+token.size()||l<=INT32_MIN||l>INT32_MAX||uint64_t(l<0?-l:l)>c.variables)fail("literal");
  if(tactix_add_checked(s,int(l)))fail("public ingest rejected");
  if(l){++width;++c.literals;} else {++seen;c.max_width=std::max(c.max_width,width);c.sort_pairs=bounded(__uint128_t(c.sort_pairs)+__uint128_t(width)*(width?width-1:0)/2);width=0;}
 }
 if(!input.eof()||!header||width||seen!=c.clauses||c.variables!=2248832||c.clauses!=4534906)fail("complete original input mismatch");
 return c;
}
static tactix_direct_csp_guarded_cover_parameters allowances(const InputCounts &c) {
 // Exact selected shape:16 domains*7 premises,32 ORs*6,64 links,one cover.
 constexpr uint64_t D=16,C=32,O=32,I=5,P=369,L=928,W=32;
 const uint64_t shape=1+C+4*C+D*(1+4)+O*(1+I);
 const uint64_t image=bounded(__uint128_t(P)*(1+(W+P)*(W+1))+D*(1+4)+O*(1+I)+C*(1+C)+P+L);
 // Public input ingest also collects exact original literal/sort upper bounds.
 // Include all three possible publications even for the first request.
 const uint64_t M=c.clauses+3,T=c.literals+3*D,S=c.sort_pairs+3*D*(D-1)/2;
 const uint64_t capture=bounded(__uint128_t(image)+(P+1)*(L+1)+L+4*__uint128_t(M)+T+S+__uint128_t(P+1)*(M+T)+shape+P+L);
 const uint64_t plan=bounded(2*__uint128_t(image)+(P+1)*(L+1)+D*(1+D)+C*(1+I*(1+D)+3*P)+1+D*(1+D)+P);
 const uint64_t journal=256+100*P+16*L+16*D+24*P;
 const uint64_t recognition=bounded(3*__uint128_t(capture)+plan+4*__uint128_t(M)+T+S+2*(shape+P+L)+journal+2*D);
 const uint64_t key=8+16*(L+P+D);
 const uint64_t session=bounded(__uint128_t(image)+8+16*L+D*(D+16)+3*(1+key)+2*(P+L)+shape+D);
 std::cout<<"{\"record\":\"derived-profile\",\"original_clauses\":"<<c.clauses<<",\"original_literals\":"<<c.literals<<",\"original_sort_pair_bound\":"<<c.sort_pairs<<",\"maximum_original_width\":"<<c.max_width<<",\"recognition\":"<<recognition<<",\"session\":"<<session<<",\"source_clauses\":"<<P<<",\"source_literals\":"<<L<<",\"proof_work\":"<<1+D+P<<",\"journal\":"<<journal<<"}\n";
 return {P,L,recognition,D,4,C,O,I,D,1+D+P,1,D,journal,uint32_t(c.variables+1),3,session};
}
static int id(const char *name) {for(int i=0;i<tactix_rule_count();++i)if(std::string(tactix_rule_name(i))==name)return i;fail("missing public action");}
static bool publicly_applicable(tactix_solver *s,int action) {
 std::vector<int> choices(static_cast<size_t>(tactix_rule_count()),0);
 const int count=tactix_rules_applicable(s,choices.data(),int(choices.size()));
 if(count<0)fail("public guard query failed");
 return std::find(choices.begin(),choices.begin()+count,action)!=choices.begin()+count;
}
static void counters(const tactix_solver *s,const char *label) {
 const auto &k=s->kernel; const auto &t=k.stats;
 std::cout<<"{\"record\":\""<<label<<"\",\"level\":"<<k.level<<",\"unassigned\":"<<k.unassigned<<",\"root_assignments\":"<<t.root_assignments<<",\"decisions\":"<<t.decisions<<",\"propagations\":"<<t.propagations<<",\"conflicts\":"<<t.conflicts<<",\"search_ticks\":"<<t.search_ticks<<",\"next_clause_id\":"<<k.next_clause_id<<",\"arena_words\":"<<k.arena.size()<<",\"attempts\":"<<t.direct_csp_support_attempts<<",\"applied\":"<<t.direct_csp_support_applied<<",\"source_clauses\":"<<t.direct_csp_support_source_clauses<<",\"source_literals\":"<<t.direct_csp_support_source_literals<<",\"owner_capture_work\":"<<t.direct_csp_support_owner_capture_work<<",\"recognition_work\":"<<t.direct_csp_support_recognition_work<<",\"source_scan_work\":"<<t.direct_csp_support_source_scan_work<<",\"planning_work\":"<<t.direct_csp_support_planning_work<<",\"revalidation_work\":"<<t.direct_csp_support_revalidation_work<<",\"proof_work\":"<<t.direct_csp_support_proof_work<<",\"emitted_additions\":"<<t.direct_csp_support_emitted_additions<<",\"proof_literal_occurrences\":"<<t.direct_csp_support_proof_literal_occurrences<<",\"journal_bytes\":"<<t.direct_csp_support_journal_bytes<<",\"session_work\":"<<t.direct_csp_support_session_work<<",\"owner_commits\":"<<t.direct_csp_support_owner_commits<<",\"session_commits\":"<<t.direct_csp_support_session_commits<<",\"resource_exhausted\":"<<t.direct_csp_support_resource_exhausted<<",\"interrupted\":"<<t.direct_csp_support_interrupted<<",\"stale\":"<<t.direct_csp_support_stale<<",\"invalid\":"<<t.direct_csp_support_invalid<<"}\n";
}
static void startup_fingerprint(const tactix_solver *s) {
 // Hash exact initialized live words and identity/phase arrays. This is a
 // matched-context diagnostic; original source authentication stays separate.
 SHA256_CTX context;SHA256_Init(&context);
 const auto bytes=[&](const char *tag,const void *data,size_t count){SHA256_Update(&context,tag,std::strlen(tag));SHA256_Update(&context,&count,sizeof(count));if(count)SHA256_Update(&context,data,count);};
 const auto vector=[&](const char *tag,const auto &v){bytes(tag,v.data(),v.size()*sizeof(*v.data()));};
 vector("arena",s->kernel.arena);vector("values",s->kernel.values);
 vector("external_to_internal",s->kernel.external_to_internal);
 vector("saved",s->heuristics.saved);vector("target",s->heuristics.target);vector("best",s->heuristics.best);
 vector("target_candidate",s->heuristics.target_candidate);
 unsigned char digest[SHA256_DIGEST_LENGTH];SHA256_Final(digest,&context);
 std::cout<<"{\"record\":\"common-startup-fingerprint\",\"sha256\":\"";
 for(auto c:digest)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(c);
 std::cout<<std::dec<<"\"}\n";
}
static std::vector<int32_t> clause(const tactix_solver *s,Ref ref) {
 const auto *c=clause_at(s->kernel.arena,ref);std::vector<int32_t> out;
 for(uint32_t i=0;i<c->size;++i)out.push_back(kernel_externalize(s->kernel,c->lits[i]));
 std::sort(out.begin(),out.end());return out;
}
int main(int argc,char **argv) {
 if(argc!=5)return 2; // full original CNF, off/on, proof.dsr, provenance.jsonl
 const std::string arm=argv[2];active_arm=argv[2];if(arm!="off"&&arm!="on")return 2;
 tactix_solver *s=nullptr;FILE *provenance=nullptr;int rc=0;std::string context="prepared";
 std::vector<Ref> published;uint64_t unit_uses=0,conflict_uses=0,public_decisions=0,public_propagates=0,public_analysis=0;
 {
  s=tactix_init();active_solver=s;if(!s||tactix_proof(s,argv[3],TACTIX_PROOF_DSR_BINARY))fail("public proof attach");
  provenance=fopen(argv[4],"wb");active_provenance=provenance;if(!provenance||tactix_bind_structural_provenance(s,provenance))fail("public provenance attach");
  failure_stage="input";const auto counts=ingest(s,argv[1]);failure_stage="startup";
  const int propagate=id("Propagate"),analysis=id("ConflictAnalysis");
  const bool startup_needed=publicly_applicable(s,propagate);
  if(startup_needed){if(tactix_apply(s,propagate)<0)fail("startup propagation rejected after applicable guard");++public_propagates;}
  if(!kernel_propagation_fixpoint(s->kernel)&&!s->kernel.inconsistent&&!s->kernel.conflict_valid)fail("startup propagation remains pending");
  std::cout<<"{\"record\":\"startup-propagation-service\",\"applicable\":"<<(startup_needed?"true":"false")<<",\"called\":"<<public_propagates<<"}\n";
  const auto profile=allowances(counts);counters(s,"common-startup");startup_fingerprint(s);
  std::cout<<"{\"record\":\"selected-root-values\",\"values\":[";
  for(int i=0;i<16;++i){if(i)std::cout<<',';std::cout<<int(kernel_external_value(s->kernel,internalize(-selected::expected0[i])));}
  std::cout<<"]}\n";
  if(s->kernel.level||s->kernel.inconsistent||s->kernel.conflict_valid||!s->kernel.trail.empty()||!kernel_propagation_fixpoint(s->kernel))fail("unaltered empty-root owner boundary unavailable");
  failure_stage="native-publication";
  if(arm=="on") {
   tactix_direct_csp_guarded_cover_selection payload[3];
   for(int r=0;r<3;++r)payload[r]={selected::domains,selected::tuples[r].data(),16,{selected::cover,32},selected::ors,32,selected::blockers,32};
   tactix_online_step_v14 step{};auto &base=step.base.base.base.base.base.base.base.base.base.base.base.base.base;
   base.step_id=1;base.rule_id=35;base.granularity=TACTIX_ONLINE_GRANULARITY_BATCH;base.repeat_mode=TACTIX_ONLINE_REPEAT_ONCE;base.repeat_limit=1;base.work_unit=TACTIX_ONLINE_WORK_APPLICATIONS;base.budget=3;
   step.direct_csp_profile=TACTIX_DIRECT_CSP_PROFILE_GUARDED_COVER_V2;step.guarded_cover_parameters=profile;step.guarded_cover_instances=payload;step.guarded_cover_instance_count=3;
   tactix_online_program_v14 program{&step,1,TACTIX_ONLINE_REPEAT_ONCE,1};
   if(tactix_online_configure_v14(s,&program,1,TACTIX_ONLINE_RECONFIGURE_RESET))fail("C14 configure declined");
   for(int row=0;row<3;++row) {
    const auto before=s->kernel.arena.size();tactix_online_result out{};
    if(tactix_online_advance(s,&out))fail("C14 advance failure");
    std::cout<<"{\"record\":\"C14-publication\",\"row\":"<<row<<",\"status\":"<<out.status<<",\"applications\":"<<out.applications<<",\"progress\":"<<out.made_rule_progress<<",\"selection_complete\":"<<out.selection_complete<<"}\n";
    if(out.status!=TACTIX_ONLINE_COMPLETED||!out.made_rule_progress||out.applications!=1)fail("native publication declined; no fallback");
    std::vector<int32_t> expected;for(int d=0;d<16;++d)expected.push_back(-selected::domains[d].literals[selected::tuples[row][d]]);std::sort(expected.begin(),expected.end());
    bool exact=false;for(Ref ref=before;ref<s->kernel.arena.size();){const auto *c=clause_at(s->kernel.arena,ref);if(!c->garbage()&&clause(s,ref)==expected){published.push_back(ref);exact=true;std::cout<<"{\"record\":\"live-native-clause\",\"row\":"<<row<<",\"ref\":"<<ref<<",\"proof_id\":"<<c->id()<<"}\n";}ref+=clause_actual_words(c);}
    if(!exact)fail("exact published original-variable clause missing");
   }
  }
  counters(s,"after-publication");
  auto propagate_step=[&] {
   const auto begin=s->kernel.trail.size();int result=1;
   if(publicly_applicable(s,propagate)){result=tactix_apply(s,propagate);++public_propagates;if(result<0)fail("public propagation rejected after applicable guard");}
   if(!kernel_propagation_fixpoint(s->kernel)&&!s->kernel.inconsistent&&!s->kernel.conflict_valid)fail("public propagation remains pending");
   for(size_t i=begin;i<s->kernel.trail.size();++i){const Lit l=s->kernel.trail[i];const auto &reason=s->kernel.assigned[var_of(l)];if(!reason.binary&&std::find(published.begin(),published.end(),reason.reason)!=published.end()){++unit_uses;std::cout<<"{\"record\":\"native-unit-reason\",\"literal\":"<<kernel_externalize(s->kernel,l)<<",\"ref\":"<<reason.reason<<"}\n";}}
   if(s->kernel.conflict_valid&&std::find(published.begin(),published.end(),s->kernel.conflict_ref)!=published.end())++conflict_uses;
   return result;
  };
  failure_stage="cdcl";context="schedule-complete";
  for(int step=0;step<15;++step) {
   const int32_t l=step<14?selected::prefix[step]:93;
   const int8_t assigned=kernel_external_value(s->kernel,internalize(l));
   std::cout<<"{\"record\":\"scheduled-public-decision\",\"position\":"<<step<<",\"literal\":"<<l<<",\"already_assigned\":"<<int(assigned)<<"}\n";
   if(assigned<0){context="scheduled-guard-already-false";break;}
   if(!assigned){if(tactix_decide(s,l))fail("public Decide rejected");++public_decisions;}
   propagate_step();
   if(s->kernel.conflict_valid||s->kernel.inconsistent){context="scheduled-prefix-conflict";if(s->kernel.level&&tactix_apply(s,analysis)>=0)++public_analysis;break;}
   if(tactix_status(s)==10){context="complete-original-model";break;}
  }
  counters(s,"after-CDCL-consumption");
  if(arm=="on"&&(!unit_uses&&!conflict_uses)){context += ":native-clause-not-consumed";rc=3;}
 }
 int verdict=s?tactix_status(s):0;
 if(s&&verdict==10){std::cout<<"s SATISFIABLE\nv ";for(uint32_t v=1;v<=2248832;++v){const int value=tactix_value(s,int(v));if(!value){rc=3;context="incomplete-public-model";break;}std::cout<<(value>0?int(v):-int(v))<<' ';}std::cout<<"0\n";}
 std::cout<<"{\"record\":\"terminal\",\"arm\":\""<<arm<<"\",\"context\":\""<<context<<"\",\"verdict\":"<<verdict<<",\"driver_exit\":"<<rc<<",\"public_decisions\":"<<public_decisions<<",\"public_propagates\":"<<public_propagates<<",\"public_conflict_analysis\":"<<public_analysis<<",\"published\":"<<published.size()<<",\"native_unit_uses\":"<<unit_uses<<",\"native_conflict_uses\":"<<conflict_uses<<"}\n";
 if(s)tactix_release(s);
 if(provenance)fclose(provenance);
 return rc;
}
