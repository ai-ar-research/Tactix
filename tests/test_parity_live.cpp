#undef NDEBUG
#include "rules/lucky/parity_live.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include <cassert>
#include <cstdio>
using namespace tactix;
using namespace tactix::parity_certificate;
static void import(Kernel &k) {
  kernel_reserve_vars(k,7);
  const std::vector<Equation> equations={{{1,3,4,0},3,false},{{4,5,6,0},3,false},{{6,7,3,0},3,false},{{2,5,7,0},3,false}};
  for(const auto &e:equations)for(const auto &c:xor_equation(e.variables.data(),e.size,e.rhs)) {
    Lit lits[4];for(unsigned i=0;i<c.size;++i)lits[i]=internalize(c.lits[i]);
    kernel_add_large_clause(k,lits,c.size,false,0,++k.next_clause_id);
  }
}
static std::vector<Var> exercise(const char *proof_path,const char *trace_path) {
  Kernel k;import(k);Proof proof;
  if(proof_path) {assert(proof.open(proof_path,ProofFormat::DsrAscii));proof.bind_variable_map(&k.internal_to_external);proof.bind_external_variable_map(&k.external_to_internal);k.proof=&proof;}
  FILE *trace=std::fopen(trace_path,"w");assert(trace);
  ParityLiveRequest request{{100,100},{10000,32,32,2},true,10000,100,0,1,trace};
  auto low=request;low.maximum_proof_additions=1;
  const auto before=k.external_to_internal;
  assert(parity_live_apply(k,low).result==ApplyResult::NoOp);assert(k.external_to_internal==before);
  request.action_id=2;const auto result=parity_live_apply(k,request);
  assert(result.result==ApplyResult::Ok&&result.published==2&&result.helpers>0);
  assert(k.nvars==7&&kernel_num_output_vars(k)==7);
  assert(kernel_binary_live(k,internalize(-1),internalize(2)));
  assert(kernel_binary_live(k,internalize(1),internalize(-2)));
  const auto allocated=k.external_to_internal.size();request.action_id=3;
  assert(parity_live_apply(k,request).result==ApplyResult::NoOp);assert(k.external_to_internal.size()==allocated);
  const Var live=kernel_new_extension_vars(k,1);assert(k.internal_to_external[live]==allocated);
  // Continue search without adding an unproved root assumption to the proof.
  kernel_assign_decision(k,internalize(1));assert(propagate_service(k)==ApplyResult::Ok);
  assert(k.value(internalize(2))==1);
  const int values[]={1,1,0,1,0,1,1};
  for(unsigned i=0;i<7;++i)if(!k.value(make_lit(i,false))) {
    kernel_assign_decision(k,make_lit(i,!values[i]));assert(propagate_service(k)==ApplyResult::Ok);
  }
  kernel_assign_decision(k,make_lit(live,false));kernel_extend_model(k);
  assert(kernel_num_output_vars(k)==7);
  for(Ref ref=0;ref<k.arena.size();) {const Clause *c=clause_at(k.arena,ref);ref+=clause_actual_words(c);bool sat=false;for(Lit l:*c)sat|=k.value(l)>0;assert(sat);}
  std::fflush(trace);assert(!std::ferror(trace));std::fclose(trace);if(proof_path)proof.flush();k.proof=nullptr;
  return k.external_to_internal;
}
int main(int argc,char **argv) {
  const char *proof=argc>1?argv[1]:"/tmp/tactix-parity-live.dsr";
  const char *trace=argc>2?argv[2]:"/tmp/tactix-parity-live.jsonl";
  assert(exercise(nullptr,"/tmp/tactix-parity-live-no-proof.jsonl")==exercise(proof,trace));
  // Two complete encodings with opposite parity yield a certified terminal
  // contradiction, not a successful no-op with an empty proof left behind.
  { Kernel k;kernel_reserve_vars(k,3);const External vars[]={1,2,3};
    for(bool rhs:{false,true})for(const auto &c:xor_equation(vars,3,rhs)) {
      Lit lits[3];for(unsigned i=0;i<3;++i)lits[i]=internalize(c.lits[i]);
      kernel_add_large_clause(k,lits,3,false,0,++k.next_clause_id);
    }
    FILE *records=std::tmpfile();assert(records);
    ParityLiveRequest r{{100,100},{10000,32,32,2},true,10000,100,0,1,records};
    assert(parity_live_apply(k,r).result==ApplyResult::Unsat&&k.inconsistent);std::fclose(records);
  }
  std::puts("test_parity_live: all passed");
}
