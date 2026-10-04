#undef NDEBUG
#include "rules/semantic_extension/row_cover.hpp"
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include <cassert>
#include <cstdio>
#include <sstream>

using namespace tactix;
static std::string formula(bool complete=true,bool fixed=false) {
  std::ostringstream out;out<<"p cnf 11 "<<((complete?16:15)+(fixed?1:0))<<'\n';
  for(int c=1;c<=3;++c)out<<c<<' '<<c+3<<' '<<c+6<<" 0\n";
  for(int r=0;r<3;++r)for(int a=1;a<=3;++a)for(int b=a+1;b<=3;++b)
    if(complete||r||a!=1||b!=2)out<<-(r*3+a)<<' '<<-(r*3+b)<<" 0\n";
  out<<"10 11 0\n10 -11 0\n-10 11 0\n-10 -11 0\n";
  if(fixed)out<<"-1 0\n";
  return out.str();
}
static void load(Kernel &k,const std::string &cnf) {
  FILE *f=std::tmpfile();assert(f);std::fwrite(cnf.data(),1,cnf.size(),f);std::rewind(f);
  assert(!parse_dimacs(f,k));std::fclose(f);
}
static uint64_t exercise(const char *proof_path,const char *cnf_path,bool original=false,bool fixed=false) {
  Kernel k;Proof proof;
  if(proof_path) {
    assert(proof.open(proof_path,ProofFormat::DsrAscii));
    proof.bind_variable_map(&k.internal_to_external);
    proof.bind_external_variable_map(&k.external_to_internal);k.proof=&proof;
  }
  const auto cnf=formula(true,fixed);load(k,cnf);
  if(cnf_path) {FILE *f=std::fopen(cnf_path,"w");assert(f);std::fwrite(cnf.data(),1,cnf.size(),f);std::fclose(f);}
  FILE *records=std::tmpfile();assert(records);
  RowCoverRequest request{100,1000,100000,10,1000,10000,1,records};
  request.original_input=original;
  auto short_request=request;short_request.maximum_additions=2;
  const auto before=k.next_clause_id;
  assert(derive_row_cover(k,short_request).result==ApplyResult::NoOp);
  assert(k.next_clause_id==before&&k.nvars==11&&std::ftell(records)==0);
  auto result=derive_row_cover(k,request);
  assert(result.result==ApplyResult::Ok&&result.width==3&&result.published==3);
  assert(result.helpers==13&&k.nvars==11&&k.external_to_internal.size()==24);
  assert(kernel_num_output_vars(k)==11);
  const auto prior_map=k.external_to_internal;const auto prior_id=k.next_clause_id;
  assert(derive_row_cover(k,request).result==ApplyResult::NoOp);
  assert(k.external_to_internal==prior_map&&k.next_clause_id==prior_id);
  // Proved row-cover clauses, unlike phase preferences, propagate this
  // last possible original choice without branching on the semantic helper.
  if(!k.value(kernel_internalize(k,-1)))kernel_assign_decision(k,kernel_internalize(k,-1));
  assert(propagate_service(k)==ApplyResult::Ok);
  kernel_assign_decision(k,kernel_internalize(k,-2));assert(propagate_service(k)==ApplyResult::Ok);
  assert(k.value(kernel_internalize(k,3))==1);
  if(proof_path) {
    const int32_t gadget=10;
    assert(proof.add_external_rup(++k.next_clause_id,&gadget,1));
    assert(proof.add_external_rup(++k.next_clause_id,nullptr,0));proof.flush();k.proof=nullptr;
  }
  std::fclose(records);return result.additions;
}
int main(int argc,char **argv) {
  const uint64_t additions=exercise(nullptr,nullptr);
  const uint64_t original_additions=exercise(nullptr,nullptr,true,true);
  if(argc>=3)assert(exercise(argv[2],argv[1])==additions);
  if(argc==5)assert(exercise(argv[4],argv[3],true,true)==original_additions);
  Kernel k;load(k,formula(false));FILE *records=std::tmpfile();assert(records);
  RowCoverRequest request{100,1000,100000,10,1000,10000,1,records};
  assert(derive_row_cover(k,request).result==ApplyResult::NoOp&&k.nvars==11);
  assert(std::ftell(records)==0);std::fclose(records);
  std::puts("test_row_cover: passed");
}
