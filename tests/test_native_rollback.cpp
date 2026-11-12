#undef NDEBUG
#include "query_test_support.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/bce/bce.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"
#include <cstdio>
#include <string>
using namespace tactix;
using namespace tactix::session;

static const auto limits = query_test::large_limits;

using query_test::writer;

static void run(bool append,const std::string &variant,const std::string &prefix,
                const std::string &mode,bool dynamic) {
  QueryHistory h;
  for(auto c:{std::vector<int32_t>{1,2},{-1,-2},{-1,3},{-2,4},
              {5,-5},{6,-6},{7,8},{9},{10,11},{-1,12,13}}) {
    for(int32_t lit:c)assert(h.add(lit)==InputStatus::Ok);
    assert(h.add(0)==InputStatus::Ok);
  }
  assert(h.begin_query()==InputStatus::Ok);
  QueryKernel s(limits());assert(s.replay(h)==ReplayStatus::Ok);
  Kernel *identity=s.kernel(s.ticket());Heuristics *guidance=identity->h;
  auto &k=*identity;
  guidance->stable=true;
  heuristics_update_scores(k);
  heuristics_score_set(k,var_of(kernel_internalize(k,7)),0.75);
  guidance->saved[var_of(kernel_internalize(k,7))]=-1;
  const auto original_binary=kernel_binary_id(k,kernel_internalize(k,7),kernel_internalize(k,8));
  assert(bce_try(k,kernel_internalize(k,10))==ApplyResult::Ok);
  heuristics_consume_retirements(k);
  assert(!k.reconstruction.empty());
  SubstitutionRedundancyInstance sr;
  sr.clause={kernel_internalize(k,1)};
  sr.witness={kernel_internalize(k,-2),kernel_internalize(k,3),kernel_internalize(k,12)};
  assert(substitution_redundancy_apply_detailed(k,sr).rule_result==ApplyResult::Ok);
  propagate_service(k);
  assert(kernel_external_value(k,internalize(3))>0);
  // These descendants are not entailed by permanent input. Duplicate copies
  // exercise binary retirement by population rather than the last ID alone.
  const Lit bin[]{kernel_internalize(k,3),kernel_internalize(k,5)};
  for(unsigned copy=0;copy<2;++copy) {
    const auto id=++k.next_clause_id;k.proof->add_rup(id,bin,2);
    kernel_add_binary_clause(k,bin[0],bin[1],id);
  }
  const Lit large[]{bin[0],bin[1],kernel_internalize(k,6)};
  const auto bad_large=++k.next_clause_id;k.proof->add_rup(bad_large,large,3);
  kernel_add_large_clause(k,large,3,true,3,bad_large);
  if(variant!="plain") {
    kernel_sparse_collect_root(k);
    if(variant=="compact") {
      assert(kernel_compact_variables(k,true));
      assert(k.fixed_external_count && kernel_internalize(k,3)==INVALID_LIT);
    }
  }
  assert(h.finish_query(1,QueryResult::Unknown)==InputStatus::Ok);
  for(unsigned q=2;q<=5;++q) {
    if(q==2 && append)query_test::clause(h,{-3});
    if((q==2 || q==3) && !append)assert(h.assume(-3)==InputStatus::Ok);
    if(q==3)assert(h.assume(1)==InputStatus::Ok);
    if(q==5)assert(h.assume(-10)==InputStatus::Ok);
    assert(h.begin_query()==InputStatus::Ok);
    const auto stem=prefix.empty()?"":prefix+"-"+std::to_string(q);
    query_test::export_query(h, stem);
    const auto advanced=s.advance(h,writer(stem,mode));
    assert(advanced.status==NativeAdvanceStatus::Ok && s.kernel(s.ticket())==identity && k.h==guidance);
    if(q==2) {
      assert(advanced.retracted>=2 && k.reconstruction.empty());
      assert(kernel_binary_copies(k,kernel_internalize(k,3),kernel_internalize(k,5))==0);
      for(const auto [ref,c]:arena_records(k.arena)) { (void)ref;assert(c->id()!=bad_large || c->garbage()); }
      assert(kernel_binary_id(k,kernel_internalize(k,7),kernel_internalize(k,8))==original_binary);
      const Var v=var_of(kernel_internalize(k,7));
      assert(guidance->scores.score[v]==0.75 && guidance->saved[v]==-1);
      assert(kernel_external_value(k,internalize(9))>0);
      // Witnessed continuation must use the repaired source census. Variable
      // 5 is now free; its old restriction-dependent binary/large sources are
      // absent, so choosing 5 is a legal fresh representative restriction.
      SubstitutionRedundancyInstance next;
      next.clause={kernel_internalize(k,5)};
      assert(substitution_redundancy_apply_detailed(k,next).rule_result==ApplyResult::Ok);
    }
    const int answer=q==3?20:10;
    assert(query_test::drive(s,h,dynamic)==answer);
    if (answer==10) query_test::export_model(h, k, stem);
    if(q==4) {
      // A fresh weakening group must remain fully tracked after the old repair
      // owner was consumed. The next query forces its selective restoration.
      restart_apply_to(k,0);
      assert(bce_try(k,kernel_internalize(k,10))==ApplyResult::Ok);
      assert(!k.reconstruction.empty());
    }
    assert(s.flush_proof(s.ticket()));
    kernel_check_invariants(k);
    std::printf("q=%u result=%d retracted=%llu restored=%llu rho=%zu conflicts=%llu decisions=%llu\n",
      q,answer,(unsigned long long)advanced.retracted,(unsigned long long)advanced.restored,k.reconstruction.size(),
      (unsigned long long)k.stats.conflicts,(unsigned long long)k.stats.decisions);
    assert(h.finish_query(q,answer==10?QueryResult::Sat:QueryResult::Unsat)==InputStatus::Ok);
  }
}
int main(int argc,char **argv) {
  if(argc==6)run(std::string(argv[1])=="append",argv[2],argv[3],argv[4],std::string(argv[5])=="dynamic");
  else {
    assert(argc==1);
    for(bool append:{false,true})for(const auto *variant:{"plain","collect","compact"})
      run(append,variant,"","none",false);
  }
}
