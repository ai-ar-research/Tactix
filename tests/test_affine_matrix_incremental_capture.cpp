#undef NDEBUG
#include "kernel/clause.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"
#include <cassert>
#include <cstdio>
#include <string>
#include <unistd.h>
using namespace tactix;
using namespace tactix::affine_semantic_extension;
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
std::vector<Lit> physical(Kernel &k,const std::vector<int32_t> &clause) {
  std::vector<Lit> result;
  for(int32_t x:clause){auto l=kernel_internalize(k,x);assert(l!=INVALID_LIT);result.push_back(l);}
  return result;
}
void add(Kernel &k,const std::vector<int32_t> &clause) {
  const auto body=physical(k,clause);const auto id=++k.next_clause_id;
  if(body.size()==2)kernel_add_binary_clause(k,body[0],body[1],id);
  else kernel_add_large_clause(k,body.data(),body.size(),false,0,id);
}
void capture() {
  Kernel k;kernel_reserve_vars(k,32);
  auto original=clausal_form({1,2,3},false);
  for(const auto &c:clausal_form({2,3,4},true))original.push_back(c);
  for(const auto &c:original)add(k,c);
  FILE *journal=std::tmpfile();assert(journal);k.structural_provenance=journal;
  const std::string path="/tmp/tactix-affine-capture-"+std::to_string(getpid());
  Proof proof;assert(proof.open(path.c_str(),ProofFormat::DsrAscii));
  proof.bind_variable_map(&k.internal_to_external);proof.bind_external_variable_map(&k.external_to_internal);
  for(size_t i=0;i<original.size();++i){auto body=physical(k,original[i]);proof.seed_population(i+1,body.data(),body.size());}
  proof.arm_population_shadow();k.proof=&proof;
  AffineMatrixSearchSession session;k.affine_matrix_search_session=&session;
  auto p=parameters();p.basis_maximum_nodes=32;
  assert(affine_matrix_search_initialize(k,session,p,AffineMatrixAdmission::AuthenticatedSubimage,
      {64*1024*1024,1000,10000}).status==AffineMatrixInitStatus::Applied);
  auto first=affine_matrix_search_step(k,session);assert(first.status==AffineMatrixServiceStatus::NoEvent);
  auto repeat=affine_matrix_search_step(k,session);
  assert(repeat.status==AffineMatrixServiceStatus::NoEvent && repeat.incremental_capture && repeat.unchanged_support && repeat.cached_event);
  assert(repeat.lifecycle_work==0 && repeat.trail_capture_work==0 && repeat.synchronization_work<=2);
  kernel_assign_decision(k,kernel_internalize(k,8));assert(propagate_apply(k)==ApplyResult::Ok);
  auto outside=affine_matrix_search_step(k,session);
  assert(outside.status==AffineMatrixServiceStatus::NoEvent && outside.incremental_capture && outside.unchanged_support && outside.cached_event);
  assert(outside.capture_delta_literals==1 && outside.lifecycle_work==0);
  kernel_assign_decision(k,kernel_internalize(k,1));assert(propagate_apply(k)==ApplyResult::Ok);
  volatile sig_atomic_t stop=1;k.stop=&stop;
  const auto before=k.next_clause_id; const auto journal_before=std::ftell(journal);
  assert(affine_matrix_search_step(k,session).status==AffineMatrixServiceStatus::Interrupted);
  assert(k.next_clause_id==before && std::ftell(journal)==journal_before);k.stop=nullptr;
  // A fixed stop schedule fires at the first synchronization charge after
  // capture has committed. This is one causal microcase, not a fuzz campaign.
  k.stop_fuzz_state=156;k.stop_fuzz=32;
  const auto late=affine_matrix_search_step(k,session);
  assert(late.status==AffineMatrixServiceStatus::Interrupted && late.incremental_capture &&
         late.capture_delta_literals==1 && !late.synchronized && !late.cached_event);
  assert(k.next_clause_id==before && std::ftell(journal)==journal_before);k.stop_fuzz=0;
  auto positive=affine_matrix_search_step(k,session);
  assert(positive.status==AffineMatrixServiceStatus::Applied && positive.incremental_capture &&
         positive.capture_delta_literals==0 && positive.unchanged_support && !positive.cached_event && positive.synchronized);
  assert((positive.installed_reason==std::vector<int32_t>{-1,-4}));assert(propagate_apply(k)==ApplyResult::Ok);
  kernel_backtrack(k,0);
  // Same trail length after opposite reassignment must not reuse the old
  // support value or unconditionalize the previous guarded consequence.
  kernel_assign_decision(k,kernel_internalize(k,8));assert(propagate_apply(k)==ApplyResult::Ok);
  kernel_assign_decision(k,kernel_internalize(k,-1));assert(propagate_apply(k)==ApplyResult::Ok);
  auto negative=affine_matrix_search_step(k,session);
  assert(!negative.incremental_capture && negative.status==AffineMatrixServiceStatus::Applied);
  assert((negative.installed_reason==std::vector<int32_t>{1,4}));assert(propagate_apply(k)==ApplyResult::Ok);
  kernel_backtrack(k,0);assert(!kernel_external_value(k,internalize(1)) && !kernel_external_value(k,internalize(4)));
  auto rooted=affine_matrix_search_step(k,session);assert(rooted.status==AffineMatrixServiceStatus::NoEvent);
  kernel_affine_note_namespace_change(k);
  auto remapped=affine_matrix_search_step(k,session);
  assert(!remapped.incremental_capture && remapped.lifecycle_work>=4 && remapped.status==AffineMatrixServiceStatus::NoEvent);
  k.affine_capture_epochs_trusted=false;
  auto untrusted=affine_matrix_search_step(k,session);
  assert(!untrusted.incremental_capture && untrusted.trail_capture_work>=4 && untrusted.status==AffineMatrixServiceStatus::NoEvent);
  session.disable(k);k.proof=nullptr;k.structural_provenance=nullptr;std::fclose(journal);proof.flush();std::remove(path.c_str());
}
}
int main(){capture();}
