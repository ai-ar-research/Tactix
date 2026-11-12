#undef NDEBUG
#include "affine_persistent_support.hpp"
#include "kernel/clause.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"
#include "rules/substitute/substitute.hpp"
#include "rules/bve/bve.hpp"
#include "rules/sweep/sweep.hpp"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <unistd.h>
using namespace tactix;
using namespace tactix::affine_semantic_extension;
namespace {
void write_original(const std::string &path,const std::vector<std::vector<int32_t>> &clauses) {
  FILE *out=std::fopen(path.c_str(),"w");assert(out);
  std::fprintf(out,"p cnf 10 %zu\n",clauses.size());
  for(const auto &c:clauses){for(auto x:c)std::fprintf(out,"%d ",x);std::fputs("0\n",out);}
  assert(!std::fclose(out));
}
// Incremental trail capture reuses an unchanged owner, conditions on new
// decisions and rebuilds after backtracking, a namespace change or untrusted epochs.
void capture() {
  auto original=clausal_form({1,2,3},false);
  for(const auto &c:clausal_form({2,3,4},true))original.push_back(c);
  const std::string path="/tmp/tactix-affine-capture-"+std::to_string(getpid());
  Kernel k;Proof proof;AffineMatrixSearchSession session;
  FILE *journal=attach_persistent(k,proof,session,32,original,path);
  auto p=parameters();p.basis_maximum_nodes=32;
  assert(affine_matrix_search_initialize(k,session,p,AffineMatrixAdmission::AuthenticatedSubimage,
      {64*1024*1024,1000,10000}).status==AffineMatrixInitStatus::Applied);
  auto first=affine_matrix_search_step(k,session);assert(first.status==AffineMatrixServiceStatus::NoEvent);
  auto repeat=affine_matrix_search_step(k,session);
  assert(repeat.status==AffineMatrixServiceStatus::NoEvent && repeat.incremental_capture && repeat.unchanged_support && repeat.cached_event);
  assert(repeat.lifecycle_work==0 && repeat.trail_capture_work==0 && repeat.synchronization_work<=2);
  kernel_assign_decision(k,kernel_internalize(k,8));assert(propagate_apply(k)==ApplyResult::Ok);
  const auto pending_owner=session.owner_version(),pending_id=k.next_clause_id;
  const auto pending_bytes=proof.premise_bytes();
  const auto pending_journal=std::ftell(journal);
  const std::vector<Lit> pending_trail(k.trail.begin(),k.trail.end());
  auto outside=affine_matrix_search_step(k,session);
  assert(outside.status==AffineMatrixServiceStatus::NoEvent && outside.unchanged_support &&
         session.enabled() && session.attached_to(k));
  assert(session.owner_version()==pending_owner && k.next_clause_id==pending_id &&
         proof.premise_bytes()==pending_bytes && std::ftell(journal)==pending_journal && k.trail==pending_trail);
  kernel_assign_decision(k,kernel_internalize(k,1));assert(propagate_apply(k)==ApplyResult::Ok);
  const auto conditioned_trail=std::vector<Lit>(k.trail.begin(),k.trail.end());
  const auto conditioned=affine_matrix_search_step(k,session);
  assert(conditioned.status==AffineMatrixServiceStatus::Applied &&
         conditioned.installed_reason.size()==2 &&
         conditioned.live.implication_consumed && conditioned.bcp_pending &&
         k.trail.size()==conditioned_trail.size()+1 &&
         k.trail.back()==kernel_internalize(k,-4));
  assert(propagate_apply(k)==ApplyResult::Ok && kernel_external_value(k,internalize(4))<0);
  kernel_backtrack(k,0);
  assert(affine_matrix_search_step(k,session).status==AffineMatrixServiceStatus::NoEvent);
  // Backtracking rebuilds the conditioned matrix before the opposite branch.
  kernel_assign_decision(k,kernel_internalize(k,8));assert(propagate_apply(k)==ApplyResult::Ok);
  kernel_assign_decision(k,kernel_internalize(k,-1));assert(propagate_apply(k)==ApplyResult::Ok);
  auto negative=affine_matrix_search_step(k,session);
  assert(negative.status==AffineMatrixServiceStatus::Applied &&
         negative.installed_reason.size()==2 &&
         negative.live.implication_consumed && negative.bcp_pending &&
         k.trail.back()==kernel_internalize(k,4) && session.enabled());
  assert(propagate_apply(k)==ApplyResult::Ok && kernel_external_value(k,internalize(4))>0);
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
// x2+x3+x4=0, x3+x4+x5=1 and the signed SCC 2 -> -6 -> -1 -> 2.
std::vector<std::vector<int32_t>> alias_original() {
  auto original=clausal_form({2,3,4},false);
  for(const auto &c:clausal_form({3,4,5},true))original.push_back(c);
  original.push_back({-2,-6});original.push_back({6,-1});original.push_back({1,2});
  return original;
}
strategy::AffineConsequenceParameters alias_parameters() {
  auto bounded=parameters();
  bounded.basis_maximum_work=10000;
  bounded.maximum_clauses=alias_original().size();
  return bounded;
}
// A lease retired just before an alias step stays visible: the next service removes
// only that equation and keeps the session.
void alias_step_leaves_a_retired_lease_for_the_service() {
  const std::string path="/tmp/tactix-alias-retired-"+std::to_string(getpid());
  Kernel k;Proof proof;AffineMatrixSearchSession session;
  FILE *journal=attach_persistent(k,proof,session,8,alias_original(),path);
  const auto bounded=alias_parameters();
  assert(affine_matrix_search_initialize(k,session,bounded,
      AffineMatrixAdmission::AuthenticatedSubimage,{64*1024*1024}).status==AffineMatrixInitStatus::Applied);
  const auto before=proof.premise_epoch();
  const auto line=physical(k,{5,7});
  proof.add_rat(++k.next_clause_id,line.data(),line.size());
  assert(proof.premise_epoch()!=before);
  const auto from=kernel_internalize(k,2),to=kernel_internalize(k,-1);
  assert(affine_matrix_search_preserve_aliases(k,session,{{from,to}}).status==AffineMatrixServiceStatus::Applied);
  const auto served=affine_matrix_search_step(k,session);
  assert(served.retired_source_groups>0 && session.enabled());
  session.disable(k);proof.flush();
  k.affine_matrix_search_session=nullptr;k.proof=nullptr;k.structural_provenance=nullptr;std::fclose(journal);
  std::remove(path.c_str());
}
void alias_failure_atomicity() {
  const std::string path="/tmp/tactix-alias-atomic-"+std::to_string(getpid());
  Kernel k;Proof proof;AffineMatrixSearchSession session;
  FILE *journal=attach_persistent(k,proof,session,8,alias_original(),path);
  const auto bounded=alias_parameters();
  assert(affine_matrix_search_initialize(k,session,bounded,
      AffineMatrixAdmission::AuthenticatedSubimage,{64*1024*1024}).status==AffineMatrixInitStatus::Applied);
  assert(!std::fflush(journal));
  const long initialized_journal_end=std::ftell(journal);assert(initialized_journal_end>0);
  std::rewind(journal);std::string initialized_journal(static_cast<size_t>(initialized_journal_end),'\0');
  assert(std::fread(initialized_journal.data(),1,initialized_journal.size(),journal)==initialized_journal.size());
  assert(!std::fseek(journal,initialized_journal_end,SEEK_SET));
  assert(initialized_journal.find("\"kind\":\"affine-matrix-retained-sources\"")!=std::string::npos &&
         initialized_journal.find("\"variables\":[")!=std::string::npos);
  proof.flush();
  const auto arena=k.arena;const auto watches=k.watches.arena;
  const auto id=k.next_clause_id,version=session.owner_version(),bytes=proof.premise_bytes(),literals=proof.premise_literals();
  const auto journal_position=std::ftell(journal);
  FILE *stream=std::fopen(path.c_str(),"rb");assert(stream);std::fseek(stream,0,SEEK_END);const auto proof_size=std::ftell(stream);std::fclose(stream);
  const auto unchanged=[&]{
    proof.flush();FILE *f=std::fopen(path.c_str(),"rb");assert(f);std::fseek(f,0,SEEK_END);assert(std::ftell(f)==proof_size);std::fclose(f);
    assert(k.arena==arena && k.watches.arena==watches && k.next_clause_id==id && session.owner_version()==version);
    assert(session.enabled() && proof.premise_bytes()==bytes && proof.premise_literals()==literals && std::ftell(journal)==journal_position);
  };
  const auto from=kernel_internalize(k,2),to=kernel_internalize(k,-1);
  // The first alias is valid and staged; a later non-SCC nomination must not
  // commit even that valid prefix or its proof-only helper copies.
  assert(affine_matrix_search_preserve_aliases(k,session,{{from,to},{from,kernel_internalize(k,7)}}).status==AffineMatrixServiceStatus::Invalid);
  unchanged();
  volatile sig_atomic_t stop=1;k.stop=&stop;
  assert(affine_matrix_search_preserve_aliases(k,session,{{from,to}}).status==AffineMatrixServiceStatus::Interrupted);
  k.stop=nullptr;unchanged();
  const std::vector<std::pair<Lit,Lit>> repetitions(1024,{from,to});
  assert(affine_matrix_search_preserve_aliases(k,session,repetitions).status==AffineMatrixServiceStatus::ResourceExhausted);
  unchanged();
  const auto accepted=affine_matrix_search_preserve_aliases(k,session,{{from,to}});
  assert(accepted.status==AffineMatrixServiceStatus::Applied && accepted.retained_aliases==1 && accepted.proof_additions==2);
  const auto accepted_version=session.owner_version(),accepted_bytes=proof.premise_bytes();
  assert(affine_matrix_search_preserve_aliases(k,session,{{from,to}}).status==AffineMatrixServiceStatus::NoEvent);
  assert(session.owner_version()==accepted_version && proof.premise_bytes()==accepted_bytes);
  // Extend this fixed source fixture with a new input equation. A failed
  // census must not stamp the new formula as fully inspected or mask retry.
  for(const auto &c:clausal_form({1,7,8},false)){
    add(k,c);const auto body=physical(k,c);proof.seed_population(k.next_clause_id,body.data(),body.size());
  }
  const auto prior_version=session.owner_version(),prior_bytes=proof.premise_bytes();
  stop=1;k.stop=&stop;
  const auto interrupted=affine_matrix_search_refresh(k,session,bounded,{64*1024*1024});
  assert(interrupted.status==AffineMatrixInitStatus::Interrupted && !interrupted.unchanged);
  k.stop=nullptr;
  // Force the retry to decline on an explicit capture bound.  The indexed
  // incremental census legitimately made the old 10k work assumption cheap,
  // so transaction atomicity must not depend on that implementation cost.
  auto limited=bounded;limited.maximum_clauses=1;
  for(unsigned retry=0;retry<2;++retry){
    const auto declined=affine_matrix_search_refresh(k,session,limited,{64*1024*1024});
    assert(declined.status==AffineMatrixInitStatus::ResourceExhausted && !declined.unchanged);
    assert(session.owner_version()==prior_version && proof.premise_bytes()==prior_bytes);
  }
  auto expanded=bounded;expanded.maximum_clauses=1000;
  const auto refreshed=affine_matrix_search_refresh(k,session,expanded,{64*1024*1024});
  assert(refreshed.status==AffineMatrixInitStatus::Applied && refreshed.snapshot_work && refreshed.equations==4);
  session.disable(k);assert(!proof.premise_bytes() && !proof.unbacked_deletions());
  proof.flush();k.proof=nullptr;std::fclose(journal);std::remove(path.c_str());
}
void refresh_root_assignment_stamp(){
 Kernel k;kernel_reserve_vars(k,6);
 const std::vector<std::vector<int32_t>> parity=clausal_form({1,2,4},false);
 const auto input_unit_id=++k.next_clause_id;
 const auto certified_four_id=++k.next_clause_id;
 for(const auto &c:parity)add(k,c);
 add(k,{-3,4});
 FILE *journal=std::tmpfile();assert(journal);k.structural_provenance=journal;
 const std::string path="/tmp/tactix-root-refresh-stamp-"+std::to_string(getpid());
 Proof proof;assert(proof.open(path.c_str(),ProofFormat::DsrAscii));
 proof.bind_variable_map(&k.internal_to_external);proof.bind_external_variable_map(&k.external_to_internal);
 const Lit unit=kernel_internalize(k,3);proof.seed_population(input_unit_id,&unit,1);
 const Lit certified_four=kernel_internalize(k,4);
 proof.seed_population(certified_four_id,&certified_four,1);
 k.unit_ids[var_of(certified_four)]=certified_four_id;
 uint64_t id=certified_four_id;
 for(const auto &c:parity){const auto body=physical(k,c);proof.seed_population(++id,body.data(),body.size());}
 const auto bridge=physical(k,{-3,4});proof.seed_population(++id,bridge.data(),bridge.size());proof.arm_population_shadow();
 proof.finish_input_transaction();k.proof=&proof;AffineMatrixSearchSession session;k.affine_matrix_search_session=&session;
 const auto p=parameters();const AffineMatrixPersistenceLimits limits{64*1024*1024,1000,100000};
 assert(affine_matrix_search_initialize(k,session,p,AffineMatrixAdmission::AuthenticatedSubimage,limits).status==AffineMatrixInitStatus::Applied);
 const auto census=affine_matrix_search_refresh(k,session,p,limits,true);
 assert(census.status==AffineMatrixInitStatus::Applied || census.status==AffineMatrixInitStatus::NoStructure);
 assert(affine_matrix_search_refresh(k,session,p,limits,false).unchanged);
 const auto unchanged_id=k.next_clause_id,old_root_assignments=k.stats.root_assignments;
 // Publish an already-authenticated original unit without allocating a new
 // source ID. Ordinary BCP derives admitted x4 from the original binary;
 // its separately authenticated input-unit certificate avoids minting a new
 // proof-only unit ID, isolating root assignment stamps from clause-ID stamps.
 k.unit_ids[var_of(unit)]=input_unit_id;kernel_assign_unit(k,unit);
 assert(propagate_apply(k)==ApplyResult::Ok && kernel_propagation_fixpoint(k));
 assert(k.next_clause_id==unchanged_id && k.stats.root_assignments>old_root_assignments);
 assert(kernel_root_value(k,kernel_internalize(k,4))>0);
 const auto maintained=affine_matrix_search_refresh(k,session,p,limits,false);
 // Active mapped root columns remain conditioned support. Only unavailable
 // root columns are absorbed as leased units; this refresh must revisit the
 // lifecycle without inventing an extra unconditional source equation.
 assert(!maintained.unchanged && maintained.status==AffineMatrixInitStatus::NoStructure);
 assert(!maintained.root_unit_additions && maintained.root_unit_work>0);
 // A fresh complete census may skip exactly unchanged maintenance; a real
 // decision/backtrack invalidates it even when the root prefix is identical.
 const auto completed=affine_matrix_search_refresh(k,session,p,limits,true);
 assert(completed.status==AffineMatrixInitStatus::Applied || completed.status==AffineMatrixInitStatus::NoStructure);
 assert(affine_matrix_search_refresh(k,session,p,limits,false).unchanged);
 kernel_assign_decision(k,kernel_internalize(k,6));assert(propagate_apply(k)==ApplyResult::Ok);kernel_backtrack(k,0);
 assert(propagate_apply(k)==ApplyResult::Ok);
 const auto retracted=affine_matrix_search_refresh(k,session,p,limits,false);
 assert(!retracted.unchanged && retracted.root_unit_work>0);
 session.disable(k);assert(!proof.premise_bytes() && !proof.unbacked_deletions());
 proof.flush();k.proof=nullptr;k.structural_provenance=nullptr;std::fclose(journal);std::remove(path.c_str());
}
void exercise(bool signed_alias,bool sweep,ProofFormat format,const char *prefix) {
  const auto suffix=std::string(signed_alias?"signed":"positive")+(sweep?"-sweep":"-scc")+"-"+std::to_string(unsigned(format));
  const std::string base=prefix?std::string(prefix)+"-"+suffix:
      "/tmp/tactix-persistent-lifecycle-"+std::to_string(getpid())+"-"+suffix;
  const std::string proof_path=base+".proof",cnf_path=base+".cnf";
  // The three directed implications form an SCC, but no pair is an exact
  // binary parity encoding. The original affine image cannot invent aliases.
  std::vector<std::vector<int32_t>> original=clausal_form({2,3,4},false);
  for(auto c:clausal_form({3,4,5},false)){
    for(auto &x:c)if(x==4 || x==-4)x=-x;
    original.push_back(std::move(c));
  }
  if(signed_alias){original.push_back({-2,-6});original.push_back({6,-1});original.push_back({1,2});}
  else{original.push_back({-2,6});original.push_back({-6,1});original.push_back({-1,2});}
  original.push_back({7,8,9});original.push_back({-9,10});
  write_original(cnf_path,original);
  Kernel k;Proof proof;AffineMatrixSearchSession session;
  FILE *journal=attach_persistent(k,proof,session,10,original,proof_path,format);
  const auto initialized=affine_matrix_search_initialize(k,session,parameters(),
      AffineMatrixAdmission::AuthenticatedSubimage,{64*1024*1024});
  assert(initialized.status==AffineMatrixInitStatus::Applied && initialized.equations==2);
  assert(session.persistent() && initialized.covered_clauses<initialized.snapshot_clauses);
  const auto representative=kernel_internalize(k,signed_alias?-1:1);
  if(sweep){
    // Actual proved Sweep-style bridge additions precede the public connected
    // substitution seam. Both directions are RUP through the original SCC.
    const std::vector<std::vector<int32_t>> bridges=signed_alias?
        std::vector<std::vector<int32_t>>{{-2,-1},{2,1}}:
        std::vector<std::vector<int32_t>>{{-2,1},{2,-1}};
    for(const auto &c:bridges){const auto body=physical(k,c);const auto id=k.next_clause_id+1;
      assert(proof.add_external_rup(id,c.data(),c.size()));add(k,c);}
    OccIndex occurrence;kernel_dense_connect(k,occurrence);
    SweepSubstitutionSession sweep_session;sweep_session.begin(k);
    assert(substitute_apply_sweep_equivalences(k,{{kernel_internalize(k,2),representative}},sweep_session)==ApplyResult::Ok);
    (void)sweep_session.collect(k);kernel_dense_disconnect(k);
  }
  assert(substitute_apply_round(k,true)==ApplyResult::Ok);
  assert(kernel_var_retired(k,var_of(kernel_internalize(k,2))));
  assert(session.enabled() && session.owner_version()>initialized.owner_version);
  assert(proof.premise_deletions_deferred()>0);
  // Real existential elimination also removes a source column. Its resolvents
  // leave the two-value consequence hidden behind the still-unassigned x3.
  const auto namespace_before_bve=k.affine_semantic_namespace_epoch;
  k.bve_gate_mask=0;
  const auto &bve=tactix_rules()[tactix_rule_id("BVE")];
  assert(bve.apply_instance(k,lit_instance(kernel_internalize(k,4)))==ApplyResult::Ok);
  assert(kernel_var_retired(k,var_of(kernel_internalize(k,4))));
  assert(k.affine_semantic_namespace_epoch>namespace_before_bve);
  kernel_collect_garbage(k);
  assert(propagate_apply(k)!=ApplyResult::Unsat && kernel_propagation_fixpoint(k));
  auto baseline=affine_matrix_search_step(k,session);
  assert(baseline.status==AffineMatrixServiceStatus::NoEvent && session.enabled());
  assert(baseline.projected_variables>=2);
  assert(k.trail.empty());
  const auto cached=affine_matrix_search_step(k,session);
  assert(cached.status==AffineMatrixServiceStatus::NoEvent && cached.synchronization_conditioned==0);
  assert(cached.installed_reason.empty() && cached.synchronization_work<=baseline.synchronization_work);
  kernel_assign_decision(k,kernel_internalize(k,1));
  assert(propagate_apply(k)!=ApplyResult::Unsat && kernel_propagation_fixpoint(k));
  assert(!k.value(kernel_internalize(k,5)));
  const auto pending_owner=session.owner_version(),pending_id=k.next_clause_id;
  const auto pending_bytes=proof.premise_bytes();
  const std::vector<Lit> pending_trail(k.trail.begin(),k.trail.end());
  const auto consequence=affine_matrix_search_step(k,session);
  assert(consequence.status==AffineMatrixServiceStatus::Applied &&
         consequence.installed_reason==std::vector<int32_t>({-1,signed_alias?5:-5}) &&
         consequence.live.implication_consumed && consequence.bcp_pending &&
         session.enabled() && session.attached_to(k));
  const Lit implied=kernel_internalize(k,signed_alias?5:-5);
  assert(session.owner_version()==pending_owner && k.next_clause_id>pending_id &&
         proof.premise_bytes()>=pending_bytes &&
         k.trail.size()==pending_trail.size()+1 && k.trail.back()==implied &&
         k.value(implied)>0);
  const auto retraction_before_backtrack=k.affine_trail_retraction_epoch;
  kernel_backtrack(k,0);assert(propagate_apply(k)!=ApplyResult::Unsat);
  assert(k.affine_trail_retraction_epoch>retraction_before_backtrack);
  assert(k.trail.empty() && !k.value(kernel_internalize(k,1)) && !k.value(kernel_internalize(k,5)));
  const auto restored=affine_matrix_search_step(k,session);
  assert(restored.status==AffineMatrixServiceStatus::NoEvent && session.enabled());
  // Complete a genuine original model, including both retired aliases and the
  // eliminated XOR column through ordinary reconstruction.
  for(int32_t external:{1,-3,7,-8,-9,-10}){
    const auto l=kernel_internalize(k,external);
    if(kernel_var_retired(k,var_of(l)) || k.value(l))continue;
    kernel_assign_decision(k,l);assert(propagate_apply(k)!=ApplyResult::Unsat);
  }
  assert(k.value(kernel_internalize(k,signed_alias?5:-5))>0);
  kernel_extend_model(k);
  for(const auto &clause:original){bool satisfied=false;
    for(int32_t x:clause){const auto l=internalize(x);assert(l<k.model_values.size());satisfied|=k.model_values[l]>0;}
    assert(satisfied);
  }
  FILE *model=std::fopen((base+".model").c_str(),"w");assert(model);
  std::fputs("s SATISFIABLE\nv ",model);
  for(int32_t variable=1;variable<=10;++variable){
    const int value=k.model_values[internalize(variable)];assert(value);
    std::fprintf(model,"%d ",value>0?variable:-variable);
  }
  std::fputs("0\n",model);assert(!std::fclose(model));
  kernel_backtrack(k,0);
  // Reconstructed retired-slot values are model choices, not root facts.
  const auto before_id=k.next_clause_id,before_version=session.owner_version(),before_bytes=proof.premise_bytes();
  const auto refused=affine_matrix_search_preserve_aliases(k,session,{});
  assert(refused.status==AffineMatrixServiceStatus::Inapplicable && !refused.proof_additions);
  assert(k.next_clause_id==before_id && session.owner_version()==before_version && proof.premise_bytes()==before_bytes);
  session.disable(k);assert(!session.enabled() && !proof.premise_bytes() && !proof.premise_literals());
  assert(!proof.unbacked_deletions());proof.flush();k.proof=nullptr;
  std::fclose(journal);k.structural_provenance=nullptr;
  if(!prefix){std::remove(cnf_path.c_str());std::remove(proof_path.c_str());std::remove((base+".model").c_str());}
}
// Handoff am-rel-23. The three sources sum to the unconditional 0 = 1 while
// the shared columns w=5 and v=7 cancel. Root Sweeps prove w and v as
// backbones; the next persistent root maintenance must publish the global
// refutation, not a reason weakened by the root-true columns that the
// certificate cannot export.
void root_maintenance_after_sweep_refutes(ProofFormat format,const char *prefix) {
  const std::string suffix=std::to_string(unsigned(format));
  const std::string base=prefix?std::string(prefix)+".rootmaint-"+suffix:
      "/tmp/tactix-persistent-rootmaint-"+std::to_string(getpid())+"-"+suffix;
  const std::string proof_path=base+".proof",cnf_path=base+".cnf";
  std::vector<std::vector<int32_t>> original=clausal_form({1,2,5,7},true);
  for(const auto &c:clausal_form({3,4,5,7},false))original.push_back(c);
  for(const auto &c:clausal_form({1,2,3,4},false))original.push_back(c);
  original.push_back({5,6});original.push_back({5,-6});
  original.push_back({7,8});original.push_back({7,-8});
  write_original(cnf_path,original);
  Kernel k;Proof proof;AffineMatrixSearchSession session;
  FILE *journal=attach_persistent(k,proof,session,10,original,proof_path,format);
  const auto p=parameters();const AffineMatrixPersistenceLimits limits{64*1024*1024,1000,100000};
  const auto initialized=affine_matrix_search_initialize(k,session,p,
      AffineMatrixAdmission::AuthenticatedSubimage,limits);
  assert((initialized.status==AffineMatrixInitStatus::Applied ||
          initialized.status==AffineMatrixInitStatus::AppliedPartial) && initialized.equations>=3);
  assert(session.persistent() && !k.inconsistent && k.trail.empty());
  // The theory service can inspect a propagated decision trail without
  // changing this owner when no new conditional consequence exists.
  kernel_assign_decision(k,kernel_internalize(k,-6));
  assert(propagate_apply(k)!=ApplyResult::Unsat && k.value(kernel_internalize(k,5))>0);
  const auto above=affine_matrix_search_step(k,session);
  assert(above.status==AffineMatrixServiceStatus::NoEvent &&
         above.installed_reason.empty() && session.enabled());
  kernel_backtrack(k,0);assert(propagate_apply(k)!=ApplyResult::Unsat && k.trail.empty());
  // Depth one keeps each environment to the seed's own clauses: satisfiable,
  // with the seed forced.
  strategy::SweepParameters sweep;sweep.max_depth=1;sweep.max_depth_ceiling=1;
  for(int32_t seed:{5,7}){
    const auto swept=sweep_try_with_limits(k,var_of(kernel_internalize(k,seed)),sweep);
    assert(swept==ApplyResult::Ok && !k.inconsistent);
    assert(propagate_apply(k)!=ApplyResult::Unsat && kernel_propagation_fixpoint(k));
    assert(kernel_root_value(k,kernel_internalize(k,seed))>0);
  }
  assert(session.enabled() && session.persistent());
  // A root-unit absorption round may precede the service round; neither may
  // reject this well-formed request.
  AffineMatrixInitResult maintained;
  for(unsigned round=0;round<2 && !k.inconsistent;++round){
    maintained=affine_matrix_search_refresh(k,session,p,limits,false);
    assert(maintained.status!=AffineMatrixInitStatus::Invalid);
    if(!k.inconsistent)assert(propagate_apply(k)!=ApplyResult::Unsat);
  }
  assert(maintained.status==AffineMatrixInitStatus::AppliedPartial);
  assert(maintained.result==ApplyResult::Unsat && k.inconsistent);
  assert(maintained.root_service && maintained.root_service->installed_reason.empty());
  session.disable(k);assert(!proof.premise_bytes() && !proof.unbacked_deletions());
  proof.flush();k.proof=nullptr;k.structural_provenance=nullptr;std::fclose(journal);
  if(!prefix){std::remove(cnf_path.c_str());std::remove(proof_path.c_str());}
}
// Handoffs am-rel-548/am-rel-206. A root Substitute round turns the failed
// root 6 (6 -> 7 and 6 -> -7) into the unit -6. Its PendingUnits batch carries
// no unit id, so the unit is known to proof-visible consumers only through the
// root-unit table. BVE on 7 then removes the binaries that
// implied it (before the table recorded such units this refresh returned
// Invalid in every format); the Substitute variant keeps them and covers the
// second-round path. A persistent refresh (op 2) and root maintenance (op 3)
// certify every root fact against the proof-visible view and must not reject
// the well-formed request.
void refresh_after_root_unit_premises_removed(ProofFormat format,bool eliminate) {
  const std::string base="/tmp/tactix-persistent-refresh-"+std::to_string(getpid())+"-"+
      std::to_string(unsigned(format))+(eliminate?"-bve":"-subst");
  const std::string proof_path=base+".proof";
  std::vector<std::vector<int32_t>> original=clausal_form({1,2,3},true);
  original.push_back({-6,7});original.push_back({-6,-7});original.push_back({6,7,8});
  // An equivalence 9 = 10 keeps the second Substitute round productive.
  original.push_back({-9,10});original.push_back({9,-10});original.push_back({8,9,-10,4});
  Kernel k;Proof proof;AffineMatrixSearchSession session;
  FILE *journal=attach_persistent(k,proof,session,10,original,proof_path,format);
  const auto p=parameters();const AffineMatrixPersistenceLimits limits{64*1024*1024,1000,100000};
  const auto initialized=affine_matrix_search_initialize(k,session,p,
      AffineMatrixAdmission::AuthenticatedSubimage,limits);
  assert((initialized.status==AffineMatrixInitStatus::Applied ||
          initialized.status==AffineMatrixInitStatus::AppliedPartial) && session.persistent());
  assert(substitute_apply_round(k,true)!=ApplyResult::Unsat && !k.inconsistent);
  assert(propagate_apply(k)!=ApplyResult::Unsat);
  const Lit unit=kernel_internalize(k,-6);
  assert(kernel_root_value(k,unit)>0 && k.assigned[var_of(unit)].reason==UNIT_REASON);
  // DSR names the asserted unit only through the root-unit table.
  assert(!k.unit_ids[var_of(unit)] && var_of(unit)<k.dsr_root_values.size() &&
         k.dsr_root_values[var_of(unit)]==-1);
  if(eliminate){
    k.bve_gate_mask=0;
    const auto &bve=tactix_rules()[tactix_rule_id("BVE")];
    (void)bve.apply_instance(k,lit_instance(kernel_internalize(k,7)));
  } else assert(substitute_apply_round(k,true)!=ApplyResult::Unsat);
  kernel_collect_garbage(k);
  assert(propagate_apply(k)!=ApplyResult::Unsat && kernel_propagation_fixpoint(k));
  const auto refreshed=affine_matrix_search_refresh(k,session,p,limits,true);
  assert(refreshed.status!=AffineMatrixInitStatus::Invalid);
  const auto maintained=affine_matrix_search_refresh(k,session,p,limits,false);
  assert(maintained.status!=AffineMatrixInitStatus::Invalid);
  session.disable(k);assert(!proof.premise_bytes() && !proof.unbacked_deletions());
  proof.flush();k.proof=nullptr;k.structural_provenance=nullptr;std::fclose(journal);
  std::remove(proof_path.c_str());
}
}
int main(int argc,char **argv){

 capture();
 alias_step_leaves_a_retired_lease_for_the_service(); alias_failure_atomicity();refresh_root_assignment_stamp();
 for(auto format:{ProofFormat::DsrAscii,ProofFormat::DsrBinary})
  root_maintenance_after_sweep_refutes(format,argc>1?argv[1]:nullptr);
 for(auto format:{ProofFormat::DsrAscii,ProofFormat::DsrBinary})
  for(bool eliminate:{false,true})refresh_after_root_unit_premises_removed(format,eliminate);
 for(auto format:{ProofFormat::DsrAscii,ProofFormat::DsrBinary})
  for(bool signed_alias:{false,true})for(bool sweep:{false,true})
    exercise(signed_alias,sweep,format,argc>1?argv[1]:nullptr);
}
