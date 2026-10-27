#undef NDEBUG
#include "policies/default_policy.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"
#include "rules/propagate/propagate.hpp"
#include "kernel/clause.hpp"
#include "proof/proof.hpp"
#include <cassert>
#include <cstdio>
#include <unistd.h>
using namespace tactix;
using namespace tactix::affine_semantic_extension;
namespace tactix {
struct DefaultAffineSearchTestAccess {
 static AffineMatrixInitResult initialize(DefaultPolicy &p,Kernel &k){return p.initialize_affine_search(k,false,true);}
 static AffineMatrixInitResult refresh(DefaultPolicy &p,Kernel &k,bool recognize=false){return p.initialize_affine_search(k,true,recognize);}
 static AffineMatrixServiceResult step(DefaultPolicy &p,Kernel &k){return p.service_affine_search(k,!k.level);}
};
}
static std::vector<Lit> body(Kernel &k,const std::vector<int32_t>&c){std::vector<Lit> r;for(int x:c)r.push_back(kernel_internalize(k,x));return r;}
static void exercise(ProofFormat format,const char *prefix){
 const std::string proof_suffix=format==ProofFormat::DsrAscii?".dsr":".drat";
 const std::string base=std::string(prefix?prefix:"/tmp/tactix-root-normalized-")+std::to_string(getpid())+"-"+(format==ProofFormat::DsrAscii?"dsr":"drat");
 std::vector<std::vector<int32_t>> original,shorts;
 const std::vector<int32_t> roots={121,122,123,124,-125,126};
 for(int i=0;i<40;++i){const auto variables=i==0?std::vector<int32_t>{1,2,3}:i==1?std::vector<int32_t>{2,3,4}:std::vector<int32_t>{3*i-1,3*i,3*i+1};for(auto clause:clausal_form(variables,i==1)){shorts.push_back(clause);for(int root:roots)clause.push_back(-root);original.push_back(std::move(clause));}}
 for(int root:roots)original.push_back({root});
 FILE *cnf=std::fopen((base+".cnf").c_str(),"w");assert(cnf);std::fprintf(cnf,"p cnf 126 %zu\n",original.size());for(const auto &c:original){for(int x:c)std::fprintf(cnf,"%d ",x);std::fputs("0\n",cnf);}std::fclose(cnf);
 Kernel k;kernel_reserve_vars(k,126);std::vector<Ref> old_refs;
 for(const auto &c:shorts){auto wide=c;for(int root:roots)wide.push_back(-root);auto native=body(k,wide);old_refs.push_back(k.arena.size());kernel_add_large_clause(k,native.data(),native.size(),false,0,++k.next_clause_id);}
 for(int root:roots)k.unit_ids[var_of(kernel_internalize(k,root))]=++k.next_clause_id;
 Proof proof;assert(proof.open((base+proof_suffix).c_str(),format));proof.bind_variable_map(&k.internal_to_external);proof.bind_external_variable_map(&k.external_to_internal);for(size_t i=0;i<original.size();++i){auto native=body(k,original[i]);proof.seed_population(i+1,native.data(),native.size());}proof.arm_population_shadow();k.proof=&proof;
 FILE *records=std::tmpfile();assert(records);k.structural_provenance=records;
 for(int root:roots)kernel_assign_unit(k,kernel_internalize(k,root));assert(propagate_apply(k)==ApplyResult::Ok);
 AffineMatrixSearchSession session;k.affine_matrix_search_session=&session;
 DefaultStrategyConfig cfg;cfg.affine_search.enabled=true;DefaultPolicy policy(cfg);
 const auto initialized=DefaultAffineSearchTestAccess::initialize(policy,k);
 assert(initialized.status==AffineMatrixInitStatus::Applied&&initialized.equations==40&&initialized.shortening_additions==160&&initialized.normalization_rup_checks>=160);
 assert(session.active_sources()==40&&session.basis_rank()==40&&session.support_variables()==118&&session.retained_bytes()<100000000);
 const auto arena_before=k.arena;const auto watches_before=k.watches.arena;
 const auto id_before=k.next_clause_id,owner_before=session.owner_version(),registry_before=proof.premise_bytes(),epoch_before=proof.premise_epoch();const auto journal_before=std::ftell(records);
 proof.flush();FILE *stream=std::fopen((base+proof_suffix).c_str(),"rb");assert(stream);std::fseek(stream,0,SEEK_END);const auto stream_before=std::ftell(stream);std::fclose(stream);
 const auto unchanged=[&](){proof.flush();assert(k.arena==arena_before&&k.watches.arena==watches_before&&k.next_clause_id==id_before&&session.owner_version()==owner_before&&proof.premise_bytes()==registry_before&&proof.premise_epoch()==epoch_before&&std::ftell(records)==journal_before);FILE *f=std::fopen((base+proof_suffix).c_str(),"rb");assert(f);std::fseek(f,0,SEEK_END);assert(std::ftell(f)==stream_before);std::fclose(f);};
 auto profile=session.parameters();auto persistence=session.persistence_limits();
 for(int failure=0;failure<3;++failure){AffineMatrixSearchSession declined;auto p=profile;auto cap=persistence;if(failure==0)cap.maximum_retained_bytes=256;if(failure==1)p.maximum_proof_deletions=1;if(failure==2)cap.maximum_deferred_deletions=1;const auto result=affine_matrix_search_initialize(k,declined,p,AffineMatrixAdmission::AuthenticatedSubimage,cap);assert(result.status==AffineMatrixInitStatus::ResourceExhausted&&!declined.enabled()&&!result.shortening_additions);unchanged();}
 volatile sig_atomic_t stop=1;k.stop=&stop;AffineMatrixSearchSession interrupted;
 assert(affine_matrix_search_initialize(k,interrupted,profile,AffineMatrixAdmission::AuthenticatedSubimage,persistence).status==AffineMatrixInitStatus::Interrupted);k.stop=nullptr;unchanged();
 assert(DefaultAffineSearchTestAccess::step(policy,k).status==AffineMatrixServiceStatus::NoEvent);
 // Native root cleanup installs equivalent short clauses before deleting each
 // original wide clause. Certified helper copies remain independently leased.
 for(size_t i=0;i<shorts.size();++i){auto native=body(k,shorts[i]);const uint64_t id=++k.next_clause_id;assert(proof.add_external_rup(id,shorts[i].data(),shorts[i].size()));kernel_add_large_clause(k,native.data(),native.size(),false,0,id);kernel_delete_clause(k,clause_at(k.arena,old_refs[i]));}
 kernel_collect_garbage(k);assert(propagate_apply(k)==ApplyResult::Ok);
 const auto repeated=affine_matrix_search_refresh(k,session,session.parameters(),session.persistence_limits(),true);
 assert(repeated.status==AffineMatrixInitStatus::NoStructure&&repeated.unchanged&&session.enabled());
 kernel_assign_decision(k,kernel_internalize(k,1));
 // A dose before propagation reaches its fixpoint must not destroy durable
 // authentication; after propagation, the retained owner can now reason at
 // this decision level and install an ordinary conditional clause.
 const auto pending_owner=session.owner_version();
 const auto pending_rank=session.basis_rank();
 const auto pending_bytes=proof.premise_bytes(),pending_epoch=proof.premise_epoch();
 const auto pending_id=k.next_clause_id;const std::vector<Lit> pending_trail(k.trail.begin(),k.trail.end());
 const auto premature=affine_matrix_search_step(k,session);
 assert(premature.status==AffineMatrixServiceStatus::Inapplicable&&session.enabled());
 assert(session.owner_version()==pending_owner&&session.basis_rank()==pending_rank&&session.attached_to(k));
 assert(proof.premise_bytes()==pending_bytes&&proof.premise_epoch()==pending_epoch&&k.next_clause_id==pending_id&&k.trail==pending_trail);
 assert(propagate_apply(k)==ApplyResult::Ok);
 const std::vector<Lit> propagated_trail(k.trail.begin(),k.trail.end());
 const auto used=DefaultAffineSearchTestAccess::step(policy,k);
 assert(used.status==AffineMatrixServiceStatus::Applied&&
        used.installed_reason==std::vector<int32_t>({-1,-4})&&
        used.live.implication_consumed&&used.bcp_pending&&session.enabled());
 assert(session.owner_version()==pending_owner&&session.basis_rank()==pending_rank&&session.attached_to(k));
 const Lit implied=kernel_internalize(k,-4);
 assert(k.next_clause_id>pending_id&&
        k.trail.size()==propagated_trail.size()+1&&k.trail.back()==implied&&
        k.value(implied)>0);
 kernel_backtrack(k,0);assert(!k.value(kernel_internalize(k,1))&&!k.value(kernel_internalize(k,4)));
 assert(DefaultAffineSearchTestAccess::step(policy,k).status==AffineMatrixServiceStatus::NoEvent);
 // Verify a complete model of the original gated input, including signed root.
 auto value=[](int variable){return variable==4||(variable>=121&&variable<=126&&variable!=125);};
 for(const auto &c:original){bool sat=false;for(int literal:c)sat|=literal>0?value(literal):!value(-literal);assert(sat);}
 FILE *model=std::fopen((base+".model").c_str(),"w");assert(model);std::fputs("s SATISFIABLE\nv ",model);for(int v=1;v<=126;++v)std::fprintf(model,"%d ",value(v)?v:-v);std::fputs("0\n",model);std::fclose(model);
 // A distinct checker stream cannot authorize this retained session, even
 // when format, variable map and native owner are unchanged. Reattaching the
 // original still-live stream restores ownership before any stale service.
 // Servicing a mismatched stream instead deliberately disables the session.
 Proof replacement;const std::string replacement_path=base+".replacement";
 assert(replacement.open(replacement_path.c_str(),format));
 const auto registry_at_swap=proof.premise_bytes();
 k.proof=&replacement;assert(!session.attached_to(k));
 k.proof=&proof;assert(session.attached_to(k)&&proof.premise_bytes()==registry_at_swap);
 k.proof=&replacement;assert(!session.attached_to(k));
 const auto stale=affine_matrix_search_step(k,session);
 assert(stale.status==AffineMatrixServiceStatus::Stale&&!session.enabled()&&replacement.premise_bytes()==0&&proof.premise_bytes()==registry_at_swap);
 k.proof=&proof;assert(!session.enabled());std::remove(replacement_path.c_str());
 session.disable(k);proof.flush();assert(!proof.unbacked_deletions());k.proof=nullptr;k.structural_provenance=nullptr;std::fclose(records);
 if(!prefix){std::remove((base+".cnf").c_str());std::remove((base+proof_suffix).c_str());std::remove((base+".model").c_str());}
}
static void retained_root_maintenance(ProofFormat format,const char *prefix){
 const std::string suffix=format==ProofFormat::DsrAscii?".dsr":".drat";
 const std::string base=std::string(prefix?prefix:"/tmp/tactix-root-normalized-")+std::to_string(getpid())+"-once-"+(format==ProofFormat::DsrAscii?"dsr":"drat");
 std::vector<std::vector<int32_t>> initial,original;
 for(auto variables:{std::vector<int32_t>{1,2,3},std::vector<int32_t>{2,3},std::vector<int32_t>{1,4}})
  for(auto c:clausal_form(variables,variables==std::vector<int32_t>{2,3}))initial.push_back(c);
 auto discovered_group=clausal_form({5,6,7},false);
 for(auto &clause:discovered_group)clause.push_back(-4);
 original=initial;original.insert(original.end(),discovered_group.begin(),discovered_group.end());
 FILE *cnf=std::fopen((base+".cnf").c_str(),"w");assert(cnf);std::fprintf(cnf,"p cnf 7 %zu\n",original.size());
 for(const auto &c:original){for(int x:c)std::fprintf(cnf,"%d ",x);std::fputs("0\n",cnf);}assert(!std::fclose(cnf));
 Kernel k;kernel_reserve_vars(k,7);std::vector<Ref> large_refs;
 for(const auto &c:initial){auto native=body(k,c);const auto id=++k.next_clause_id;
  if(native.size()==2)kernel_add_binary_clause(k,native[0],native[1],id);
  else{large_refs.push_back(k.arena.size());kernel_add_large_clause(k,native.data(),native.size(),false,0,id);}}
 std::vector<uint64_t> discovered_ids;for(size_t i=0;i<discovered_group.size();++i)discovered_ids.push_back(++k.next_clause_id);
 Proof proof;assert(proof.open((base+suffix).c_str(),format));proof.bind_variable_map(&k.internal_to_external);proof.bind_external_variable_map(&k.external_to_internal);
 for(size_t i=0;i<original.size();++i){auto native=body(k,original[i]);proof.seed_population(i+1,native.data(),native.size());}proof.arm_population_shadow();k.proof=&proof;
 FILE *records=std::tmpfile();assert(records);k.structural_provenance=records;
 AffineMatrixSearchSession session;k.affine_matrix_search_session=&session;
 DefaultStrategyConfig cfg;cfg.affine_search.enabled=true;DefaultPolicy policy(cfg);
 assert(propagate_apply(k)==ApplyResult::Ok&&k.trail.empty());
 const auto admitted=DefaultAffineSearchTestAccess::initialize(policy,k);
 assert(admitted.status==AffineMatrixInitStatus::Applied&&session.active_sources()==3);
 for(const auto &c:clausal_form({1,4},false)){auto native=body(k,c);assert(kernel_delete_binary(k,native[0],native[1]));}
 const long first_record=std::ftell(records);assert(first_record>=0);
 // A full-restart root window performs retained maintenance before publishing.
 // Gauss-Jordan first exposes x1 = 1 from the live E1 + E2.
 const auto first_window=DefaultAffineSearchTestAccess::refresh(policy,k);
 assert(first_window.root_service);const auto &first=*first_window.root_service;
 assert(first.status==AffineMatrixServiceStatus::Applied&&first.installed_reason==std::vector<int32_t>{1});
 assert(propagate_apply(k)==ApplyResult::Ok&&k.value(kernel_internalize(k,1))>0&&!k.value(kernel_internalize(k,4)));
 const long leased_record=std::ftell(records);assert(leased_record>first_record);
 // The next window derives d from root x1 through the leased x <-> d.
 const auto leased_window=DefaultAffineSearchTestAccess::refresh(policy,k);
 assert(leased_window.root_service);const auto &leased=*leased_window.root_service;
 assert(leased.status==AffineMatrixServiceStatus::Applied&&leased.installed_reason==std::vector<int32_t>({-1,4}));
 const long record_end=std::ftell(records);assert(record_end>leased_record);
 assert(!std::fseek(records,leased_record,SEEK_SET));std::string certificate(static_cast<size_t>(record_end-leased_record),'\0');
 assert(std::fread(certificate.data(),1,certificate.size(),records)==certificate.size());
 assert(certificate.find("\"source_ordinals\":")!=std::string::npos);
 assert(certificate.find("\"clause_id\":7,")!=std::string::npos||certificate.find("\"clause_id\":8,")!=std::string::npos);
 assert(!std::fseek(records,record_end,SEEK_SET));
 assert(propagate_apply(k)==ApplyResult::Ok&&k.value(kernel_internalize(k,1))>0&&k.value(kernel_internalize(k,4))>0);
 // Native x <-> d was removed before the previous restart window, yet its
 // leased premises produced d. Now remove E1/E2 as well: maintenance must keep
 // the authenticated owner alive without duplicating an already published fact.
 for(Ref ref:large_refs)kernel_delete_clause(k,clause_at(k.arena,ref));
 for(const auto &c:clausal_form({2,3},true)){auto native=body(k,c);assert(kernel_delete_binary(k,native[0],native[1]));}
 assert(propagate_apply(k)==ApplyResult::Ok&&k.value(kernel_internalize(k,1))>0&&k.value(kernel_internalize(k,4))>0);
 const auto snapshot_before=k.stats.affine_matrix_init_snapshot_work;
 const auto recognition_before=k.stats.affine_matrix_init_recognition_work;
 const auto checks_before=k.stats.affine_matrix_init_normalization_rup_checks;
 const auto maintained=DefaultAffineSearchTestAccess::refresh(policy,k);
 assert(maintained.snapshot_work==0&&maintained.recognition_work==0&&maintained.normalization_rup_checks==0);
 assert(maintained.root_service&&maintained.root_service->status==AffineMatrixServiceStatus::NoEvent);
 assert(!maintained.root_reason_additions&&maintained.maintenance_service_work>0&&session.active_sources()==3);
 assert(k.stats.affine_matrix_init_snapshot_work==snapshot_before&&k.stats.affine_matrix_init_recognition_work==recognition_before&&k.stats.affine_matrix_init_normalization_rup_checks==checks_before);
 assert(propagate_apply(k)==ApplyResult::Ok&&k.value(kernel_internalize(k,4))>0);
 assert(k.value(kernel_internalize(k,1))>0);
 // Fresh public proof premises/native IDs are genuinely new discovery input.
 // In DRAT, root4 has only its old binary reason, not an emitted unit ID.
 for(size_t i=0;i<discovered_group.size();++i){auto native=body(k,discovered_group[i]);
  const auto id=++k.next_clause_id;assert(proof.add_external_rup(id,discovered_group[i].data(),discovered_group[i].size()));
  kernel_add_large_clause(k,native.data(),native.size(),false,0,id);}
 assert(propagate_apply(k)==ApplyResult::Ok);
 const auto fresh=affine_matrix_search_refresh(k,session,session.parameters(),session.persistence_limits(),true);
 assert(fresh.status==AffineMatrixInitStatus::Applied&&session.active_sources()==4&&fresh.recognition_work>0&&fresh.discovery_recognized_groups==1);
 const auto unchanged=affine_matrix_search_refresh(k,session,session.parameters(),session.persistence_limits(),true);
 assert(unchanged.unchanged&&unchanged.snapshot_work==0&&unchanged.recognition_work==0&&unchanged.normalization_rup_checks==0);
 const auto value=[](int v){return v==1||v==3||v==4;};
 for(const auto &c:original){bool sat=false;for(int x:c)sat|=x>0?value(x):!value(-x);assert(sat);}
 FILE *model=std::fopen((base+".model").c_str(),"w");assert(model);std::fputs("s SATISFIABLE\nv ",model);for(int v=1;v<=7;++v)std::fprintf(model,"%d ",value(v)?v:-v);std::fputs("0\n",model);assert(!std::fclose(model));
 session.disable(k);proof.flush();assert(!proof.unbacked_deletions());k.proof=nullptr;k.structural_provenance=nullptr;std::fclose(records);
 if(!prefix){std::remove((base+".cnf").c_str());std::remove((base+suffix).c_str());std::remove((base+".model").c_str());}
}
static void incremental_opposite_parity(ProofFormat format) {
 const std::string path="/tmp/tactix-affine-opposite-"+std::to_string(getpid())+
     (format==ProofFormat::DsrAscii?".dsr":".drat");
 const auto first=clausal_form({1,2,3},false),second=clausal_form({1,2,3},true);
 Kernel k;kernel_reserve_vars(k,3);
 for(const auto &c:first){auto native=body(k,c);kernel_add_large_clause(k,native.data(),native.size(),false,0,++k.next_clause_id);}
 {auto native=body(k,second.front());kernel_add_large_clause(k,native.data(),native.size(),false,0,++k.next_clause_id);}
 // Every later physical clause is an original checker premise, but is not
 // installed natively until the explicit additional-discovery stage.
 std::vector<uint64_t> reserved;
 for(size_t i=1;i<second.size();++i)reserved.push_back(++k.next_clause_id);
 Proof proof;assert(proof.open(path.c_str(),format));proof.bind_variable_map(&k.internal_to_external);proof.bind_external_variable_map(&k.external_to_internal);
 uint64_t seed=0;for(const auto &population:{first,second})for(const auto &c:population){auto native=body(k,c);proof.seed_population(++seed,native.data(),native.size());}
 proof.arm_population_shadow();k.proof=&proof;FILE *records=std::tmpfile();assert(records);k.structural_provenance=records;
 AffineMatrixSearchSession session;k.affine_matrix_search_session=&session;
 DefaultStrategyConfig cfg;cfg.affine_search.enabled=true;DefaultPolicy policy(cfg);
 assert(propagate_apply(k)==ApplyResult::Ok);
 const auto initialized=DefaultAffineSearchTestAccess::initialize(policy,k);
 assert(initialized.status==AffineMatrixInitStatus::Applied&&session.active_sources()==1&&initialized.discovery_recognized_groups==1);
 for(size_t i=1;i<second.size();++i){auto native=body(k,second[i]);const auto id=++k.next_clause_id;
  assert(proof.add_external_rup(id,second[i].data(),second[i].size()));
  kernel_add_large_clause(k,native.data(),native.size(),false,0,id);}
 assert(propagate_apply(k)==ApplyResult::Ok&&k.trail.empty());
 const auto admitted=affine_matrix_search_refresh(k,session,session.parameters(),session.persistence_limits(),true);
 assert(admitted.status==AffineMatrixInitStatus::Applied&&session.active_sources()==2&&admitted.discovery_recognized_groups==1);
 const auto conflict=affine_matrix_search_step(k,session);
 assert(conflict.status==AffineMatrixServiceStatus::Applied&&conflict.installed_reason.empty());
 assert(k.inconsistent);
 session.disable(k);proof.flush();assert(!proof.unbacked_deletions());k.proof=nullptr;k.structural_provenance=nullptr;std::fclose(records);std::remove(path.c_str());
}
int main(int argc,char **argv){for(auto format:{ProofFormat::DsrAscii,ProofFormat::DratAscii}){exercise(format,argc>1?argv[1]:nullptr);retained_root_maintenance(format,argc>1?argv[1]:nullptr);incremental_opposite_parity(format);}}
