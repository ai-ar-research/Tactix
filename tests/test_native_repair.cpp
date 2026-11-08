#undef NDEBUG
#include "query_test_support.hpp"
#include "frontend/parse.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/bce/bce.hpp"
#include "rules/cce/cce.hpp"
#include "rules/condition/condition.hpp"
#include "rules/substitute/substitute.hpp"
#include "session/weakening_history.hpp"
#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
using namespace tactix;
using namespace tactix::session;
using query_test::clause;
static NativeQueryLimits limits() { return {{1024,32768,2048},{64,8192,2048,32768,128,2048,32768},4000000}; }
static ConditionInstance cube(Kernel &k, std::initializer_list<int> c, std::initializer_list<int> w) {
  ConditionInstance out;
  for(int l:c) out.clause.push_back(kernel_internalize(k,l));
  for(int l:w) out.witness.push_back(internalize(l));
  return out;
}
static void scenario(const std::string &name,const std::string &prefix,const std::string &mode,bool capture,bool dynamic) {
  QueryHistory h;
  if(name=="condition") { clause(h,{1,2});clause(h,{-1,3});clause(h,{-3,4}); }
  else if(name=="cce") { clause(h,{1,2,3});clause(h,{-1,4});clause(h,{-4,-2});clause(h,{-3,4}); }
  else if(name=="binary") { clause(h,{1,2});clause(h,{-1,-2}); }
  else if(name=="ata") { clause(h,{1,2,3});clause(h,{1,2}); }
  else { assert(name=="substitute");clause(h,{-1,2});clause(h,{1,-2});clause(h,{-2,3});clause(h,{2,-3});clause(h,{3,4}); }
  const int unrelated=name=="binary"?3:5;
  clause(h,{unrelated,unrelated+1});
  std::unique_ptr<QueryKernel> storage=capture?std::make_unique<QueryKernel>(limits()):std::make_unique<QueryKernel>();
  QueryKernel &s=*storage;
  Kernel *identity=nullptr;
  Heuristics *guidance=nullptr;
  const auto solve=[&](int expected) {
    assert(h.begin_query()==InputStatus::Ok);
    const auto id=h.active_query()->id;
    std::string file=prefix.empty()?"":prefix+"-"+std::to_string(id);
    if(!file.empty()) { FILE*f=std::fopen((file+".cnf").c_str(),"w");assert(f&&h.write_query_dimacs(id,f)&&std::fclose(f)==0); }
    std::unique_ptr<Proof> writer;
    if(mode!="none") { writer=std::make_unique<Proof>();assert(writer->open((file+".proof").c_str(),mode=="pbp"?ProofFormat::PbpStreaming:ProofFormat::DsrAscii)); }
    if(!identity) {
      assert(s.replay(h,std::move(writer))==ReplayStatus::Ok);
      identity=s.kernel(s.ticket());guidance=identity->h;
      Kernel &k=*identity;
      if(name=="condition"||name=="binary") {
        const uint64_t duplicate=++k.next_clause_id;
        const Lit pair[]={kernel_internalize(k,1),kernel_internalize(k,2)};
        k.proof->add_rup(duplicate,pair,2);
        kernel_add_binary_clause(k,pair[0],pair[1],duplicate);
      }
      if(name=="condition") assert(condition_apply_explicit(k,cube(k,{1,2},{1,3,4})));
      else if(name=="substitute") { assert(substitute_apply(k)==ApplyResult::Ok);assert(kernel_compact_variables(k)); }
      else {
        const auto &r=tactix_rules()[tactix_rule_id("CCE")];
        assert(r.apply_semantic_instance(k,name=="binary"?cce_binary_instance(kernel_internalize(k,1),kernel_internalize(k,2)):cce_large_instance(0))==ApplyResult::Ok);
        if(name=="cce") assert(k.stats.cce_cla>0&&k.stats.cce_blocked>0);
        if(name=="ata") assert(k.reconstruction.empty());
      }
      assert(bce_try(k,kernel_internalize(k,unrelated))==ApplyResult::Ok);
    } else {
      const auto result=s.advance(h,std::move(writer));
      if(result.status!=NativeAdvanceStatus::Ok) std::fprintf(stderr,"advance %s q%llu status=%u\n",name.c_str(),(unsigned long long)id,unsigned(result.status));
      assert(result.status==NativeAdvanceStatus::Ok);
      assert(s.kernel(s.ticket())==identity&&identity->h==guidance);
      if(id==2) assert((result.restored>0)==(name!="ata"));
    }
    assert(query_test::drive(s,h,dynamic)==expected&&!identity->inconsistent);
    if(expected==10) {
      assert(s.check_model(s.ticket()).status==ModelStatus::Valid);
      if(!file.empty()) { FILE*f=std::fopen((file+".model").c_str(),"w");assert(f);for(uint32_t v=1;v<=h.active_query()->query_variables;v++)assert(std::fprintf(f,"%d ",identity->model_values[internalize(v)]>0?int(v):-int(v))>0);assert(std::fprintf(f,"0\n")>0&&std::fclose(f)==0); }
    }
    assert(s.flush_proof(s.ticket()));
    std::printf("q=%llu result=%d vars=%u rho=%zu ids=%llu conflicts=%llu decisions=%llu\n",(unsigned long long)id,expected,identity->nvars,identity->reconstruction.size(),(unsigned long long)identity->next_clause_id,(unsigned long long)identity->stats.conflicts,(unsigned long long)identity->stats.decisions);
    assert(h.finish_query(id,expected==10?QueryResult::Sat:QueryResult::Unsat)==InputStatus::Ok);
  };
  solve(10);
  if(!capture) return;
  if(name=="condition") { assert(h.assume(-3)==InputStatus::Ok);assert(h.assume(-2)==InputStatus::Ok); }
  else if(name=="substitute") { assert(h.assume(2)==InputStatus::Ok);assert(h.assume(-3)==InputStatus::Ok); }
  else { assert(h.assume(-1)==InputStatus::Ok);assert(h.assume(-2)==InputStatus::Ok);if(name=="cce")assert(h.assume(-3)==InputStatus::Ok); }
  solve(20);
  assert(h.assume(-unrelated)==InputStatus::Ok);assert(h.assume(-unrelated-1)==InputStatus::Ok);
  solve(20);
  if(name=="condition")for(int l:{1,3,4,5})assert(h.assume(l)==InputStatus::Ok);
  else if(name=="substitute")for(int l:{-1,-2,-3,5})assert(h.assume(l)==InputStatus::Ok);
  else { assert(h.assume(1)==InputStatus::Ok);assert(h.assume(unrelated)==InputStatus::Ok); }
  solve(10);
}
static void group_authentication() {
  Kernel k; WeakeningHistory history(k,limits().weakening);ScopedPopulationObserverBinding binding(k,history);
  const std::vector<std::vector<int>> f={{1,2,3},{-1,4},{-4,-2},{-3,4},{5,6}};
  declare_input(k,6,f.size());
  for(const auto &c:f){std::vector<Lit> body;for(int l:c)body.push_back(internalize(l));ingest_clause(k,body);}finish_input(k);
  const auto &r=tactix_rules()[tactix_rule_id("CCE")];
  assert(r.apply_semantic_instance(k,cce_large_instance(0))==ApplyResult::Ok);
  // Admit this compatible permanent clause after CCE: putting it in the
  // initial formula instead changes ALA and avoids the covered-prefix path.
  const Lit added[]={internalize(-4),internalize(1),internalize(3)};
  kernel_add_large_clause(k,added,3,false,0,++k.next_clause_id);
  assert(condition_apply_explicit(k,cube(k,{-4,1,3},{-4})));
  assert(bce_try(k,internalize(5))==ApplyResult::Ok);
  assert(history.capture_complete()&&history.records().size()==3);
  const int32_t used[]={-2};const auto plan=history.plan_restoration(used);
  if (plan.status()!=RestorationPlanStatus::Ready||plan.selected().size()!=2||plan.kept()!=1) {
    std::fprintf(stderr,"plan=%u selected=%zu kept=%zu\n",unsigned(plan.status()),plan.selected().size(),plan.kept());
    for (const auto &record:history.records()) { std::fprintf(stderr,"family=%u witnesses:",unsigned(record.family));for(int w:record.witness)std::fprintf(stderr," %d",w);std::fprintf(stderr,"\n"); }
  }
  assert(plan.status()==RestorationPlanStatus::Ready&&plan.selected().size()==2&&plan.kept()==1);
  assert(plan.selected()[0].family==WeakeningFamily::Cce&&plan.selected()[1].family==WeakeningFamily::Condition);
  const auto rho=k.reconstruction;k.reconstruction[0]^=1;
  assert(history.plan_restoration(used).status()==RestorationPlanStatus::Incomplete);
  k.reconstruction=rho;
}
static void unlicensed_repair() {
  QueryHistory h;clause(h,{-1,-2});assert(h.begin_query()==InputStatus::Ok);
  QueryKernel s(limits());assert(s.replay(h)==ReplayStatus::Ok);Kernel &k=*s.kernel(s.ticket());
  const Lit rest[]={internalize(2)};const auto begin=k.reconstruction.size();kernel_push_witness(k,internalize(1),rest,1);kernel_record_reconstruction(k,WeakeningFamily::Substitute,begin);
  assert(h.finish_query(1,QueryResult::Unknown)==InputStatus::Ok);assert(h.assume(-1)==InputStatus::Ok&&h.assume(-2)==InputStatus::Ok&&h.begin_query()==InputStatus::Ok);
  const auto id=k.next_clause_id;const auto rho=k.reconstruction;
  assert(s.advance(h).status==NativeAdvanceStatus::ReplayRequired&&k.next_clause_id==id&&k.reconstruction==rho);
  assert(s.replay(h)==ReplayStatus::Ok&&query_test::drive(s,h,true)==10);
}

static void malformed_groups_and_storage() {
  // CCE must contain the complete original source in every covered prefix;
  // Condition must retain its exact full cube, including literals outside C.
  for (auto family : {WeakeningFamily::Cce, WeakeningFamily::Condition}) {
    Kernel k;
    WeakeningHistory history(k, limits().weakening);
    ScopedPopulationObserverBinding binding(k, history);
    std::vector<Lit> source = {internalize(1), internalize(2)};
    ingest_clause(k, source);
    finish_input(k);
    const Lit expected = internalize(1), wrong = internalize(2);
    ScopedWeakeningCapture capture(k, family, kernel_population_clause(k, source.data(), 2),
        PopulationClauseView::internal_lits(&expected, 1), 0, true, true,
        family == WeakeningFamily::Cce ? 2 : 6);
    assert(kernel_delete_binary_all_copies(k, source[0], source[1]));
    if (family == WeakeningFamily::Cce)
      kernel_push_witness(k, expected, nullptr, 0); // omits original literal 2
    else
      kernel_push_cube_witness(k, source.data(), 2, &wrong, 1);
    capture.commit();
    assert(!history.capture_complete() && history.records().empty());
  }

  Kernel k;
  // One Condition record costs C(2)+W(1)+rho(6)=9 literal words.
  WeakeningHistory history(k, {2, 9, 2});
  ScopedPopulationObserverBinding binding(k, history);
  std::vector<Lit> source = {internalize(1), internalize(2)};
  ingest_clause(k, source);
  finish_input(k);
  for (unsigned repeat = 0; repeat < 2; ++repeat) {
    ScopedWeakeningCapture cancelled(k, WeakeningFamily::Condition,
        kernel_population_clause(k, source.data(), 2),
        kernel_population_clause(k, source.data(), 1), 0, true, true, 6);
    assert(history.pending() == 1);
  }
  assert(history.capture_complete() && history.pending() == 0);
  assert(condition_apply_explicit(k, cube(k, {1,2}, {1})));
  assert(history.capture_complete() && history.records().size() == 1);

  Kernel bounded;
  WeakeningHistory missing(bounded, {2, 8, 2});
  ScopedPopulationObserverBinding bounded_binding(bounded, missing);
  ingest_clause(bounded, source);
  finish_input(bounded);
  assert(condition_apply_explicit(bounded, cube(bounded, {1,2}, {1})));
  assert(!missing.capture_complete() && missing.records().empty());
  assert(!kernel_binary_live(bounded, source[0], source[1]) && !bounded.reconstruction.empty());
}

int main(int argc,char**argv) {
  if(argc==6){scenario(argv[1],argv[2],argv[3],std::string(argv[4])=="capture",std::string(argv[5])=="dynamic");return 0;}
  assert(argc==1);group_authentication();unlicensed_repair();malformed_groups_and_storage();
  for(bool dynamic:{false,true})for(const char *name:{"condition","cce","binary","ata","substitute"})scenario(name,"","none",true,dynamic);
}
