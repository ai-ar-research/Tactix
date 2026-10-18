#undef NDEBUG
#include "rules/semantic_extension/affine_matrix.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"
#include "policies/default_policy.hpp"
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include <cassert>
#include <cstdio>
#include <unistd.h>
using namespace tactix;
using namespace tactix::affine_semantic_extension;
namespace tactix {
struct DefaultAffineSearchTestAccess {
 static AffineMatrixInitResult initialize(DefaultPolicy &p,Kernel &k){return p.initialize_affine_search(k,false,true);}
 static AffineMatrixServiceResult step(DefaultPolicy &p,Kernel &k){return p.service_affine_search(k,!k.level);}
};
}
void alias_deferred_caps(uint64_t cap,bool accepted,bool shared_tighter=false){
 Kernel k;kernel_reserve_vars(k,8);
 auto original=clausal_form({2,3,4},false);for(auto c:clausal_form({3,4,5},true))original.push_back(c);
 original.push_back({-2,-6});original.push_back({6,-1});original.push_back({1,2});
 for(const auto &clause:original){std::vector<Lit> body;for(int x:clause)body.push_back(kernel_internalize(k,x));const auto id=++k.next_clause_id;if(body.size()==2)kernel_add_binary_clause(k,body[0],body[1],id);else kernel_add_large_clause(k,body.data(),body.size(),false,0,id);}
 const std::string path="/tmp/tactix-alias-reservoir-"+std::to_string(getpid())+"-"+std::to_string(cap)+"-"+std::to_string(shared_tighter);
 Proof proof;assert(proof.open(path.c_str(),ProofFormat::DsrAscii));proof.bind_variable_map(&k.internal_to_external);proof.bind_external_variable_map(&k.external_to_internal);
 for(size_t i=0;i<original.size();++i){std::vector<Lit> body;for(int x:original[i])body.push_back(kernel_internalize(k,x));proof.seed_population(i+1,body.data(),body.size());}proof.arm_population_shadow();k.proof=&proof;
 FILE *journal=std::tmpfile();assert(journal);k.structural_provenance=journal;
 AffineMatrixSearchSession session;k.affine_matrix_search_session=&session;
 strategy::AffineConsequenceParameters p;
 p.maximum_clauses=1000;p.maximum_snapshot_literals=10000;p.maximum_snapshot_work=1000000;
 p.maximum_groups=100;p.maximum_patterns=1000;p.maximum_elimination_work=1000000;
 p.maximum_arity=4;p.maximum_intermediate_width=16;p.maximum_sources=32;p.maximum_output_width=4;
 p.maximum_proof_additions=100000;p.maximum_proof_deletions=100000;p.maximum_journal_bytes=1000000;
 p.semantics=strategy::AffineConsequenceSemantics::CertifiedBasisV2;
 p.basis_maximum_work=10000000;p.basis_maximum_intermediate_width=16;p.basis_maximum_sources=100000;
 p.basis_maximum_premises=10000;p.basis_maximum_nodes=10000;p.basis_maximum_direct_width=3;
 p.maximum_external_extent=128;p.maximum_session_keys=1000;p.maximum_session_work=1000000;assert(p.valid());
 assert(affine_matrix_search_initialize(k,session,p,AffineMatrixAdmission::AuthenticatedSubimage,{100000000,cap,100000}).status==AffineMatrixInitStatus::Applied);
 std::vector<uint64_t> shared_handles;
 if(shared_tighter)assert(proof.retain_premise_groups({{{-2,-6}}},100,10000,100000,[](auto){return true;},shared_handles,{1,100000}));
 const auto arena=k.arena;const auto watches=k.watches.arena;const auto id=k.next_clause_id,owner=session.owner_version(),bytes=proof.premise_bytes(),epoch=proof.premise_epoch(),retained=session.retained_bytes();const auto position=std::ftell(journal);
 proof.flush();FILE *f=std::fopen(path.c_str(),"rb");assert(f);std::fseek(f,0,SEEK_END);const auto stream_size=std::ftell(f);std::fclose(f);
 const auto result=affine_matrix_search_preserve_aliases(k,session,{{kernel_internalize(k,2),kernel_internalize(k,-1)}});
 if(!accepted){assert(result.status==AffineMatrixServiceStatus::ResourceExhausted&&!result.proof_additions&&!result.retained_aliases);assert(k.arena==arena&&k.watches.arena==watches&&k.next_clause_id==id&&session.owner_version()==owner&&proof.premise_bytes()==bytes&&proof.premise_epoch()==epoch&&session.retained_bytes()==retained&&std::ftell(journal)==position&&!proof.premise_pending_deletions());proof.flush();f=std::fopen(path.c_str(),"rb");assert(f);std::fseek(f,0,SEEK_END);assert(std::ftell(f)==stream_size);std::fclose(f);}
 else{assert(result.status==AffineMatrixServiceStatus::Applied&&result.retained_aliases==1&&result.proof_additions==2);assert(proof.premise_epoch()==epoch&&proof.premise_pending_deletions()==2&&proof.premise_pending_literal_work()==6&&session.active_sources()==3);const auto duplicate=affine_matrix_search_preserve_aliases(k,session,{{kernel_internalize(k,2),kernel_internalize(k,-1)}});assert(duplicate.status==AffineMatrixServiceStatus::NoEvent);}
 session.disable(k);proof.release_premise_groups(shared_handles,k.next_clause_id);proof.flush();assert(!proof.unbacked_deletions());k.proof=nullptr;k.structural_provenance=nullptr;std::fclose(journal);std::remove(path.c_str());
}
int main(int argc,char **argv){
 alias_deferred_caps(1,false);alias_deferred_caps(2,true);alias_deferred_caps(2,false,true);
 std::vector<Equation> equations;
 std::vector<std::vector<int32_t>> clauses;
 for(int i=0;i<40;++i){Equation e{i==0?std::vector<int32_t>{1,2,3}:i==1?std::vector<int32_t>{2,3,4}:std::vector<int32_t>{3*i-1,3*i,3*i+1},false,{},{}};equations.push_back(e);for(auto c:clausal_form(e.variables,false))clauses.push_back(c);}
 MatrixLimits bounds{10000000,100000,100000,100000,100000,100000};AffineMatrix matrix;
 assert(build_affine_matrix(equations,1,bounds,[](auto){return true;},matrix,true)==MatrixStatus::Complete);
 assert(matrix.row_width_bound()==118&&matrix.row_source_bound()==40);
 AffineMatrix legacy;assert(build_affine_matrix(equations,1,bounds,[](auto){return true;},legacy)==MatrixStatus::Complete);
 assert(legacy.row_width_bound()==100000&&legacy.row_source_bound()==100000);
 const std::string base=argc>1?argv[1]:"/tmp/tactix-actual-geometry-"+std::to_string(getpid());
 FILE *input=std::tmpfile();assert(input);std::fprintf(input,"p cnf 120 %zu\n",clauses.size());
 for(const auto &c:clauses){for(int x:c)std::fprintf(input,"%d ",x);std::fputs("0\n",input);}std::rewind(input);
 if(argc>1){FILE *out=std::fopen((base+".cnf").c_str(),"w");assert(out);int ch;while((ch=std::fgetc(input))!=EOF)std::fputc(ch,out);std::fclose(out);std::rewind(input);}
 Kernel k;assert(!parse_dimacs(input,k));std::fclose(input);
 FILE *journal=std::tmpfile();assert(journal);k.structural_provenance=journal;
 Proof proof;assert(proof.open((base+".dsr").c_str(),ProofFormat::DsrAscii));proof.bind_variable_map(&k.internal_to_external);proof.bind_external_variable_map(&k.external_to_internal);
 for(size_t i=0;i<clauses.size();++i){std::vector<Lit> body;for(int x:clauses[i])body.push_back(kernel_internalize(k,x));proof.seed_population(i+1,body.data(),body.size());}proof.arm_population_shadow();k.proof=&proof;
 AffineMatrixSearchSession session;k.affine_matrix_search_session=&session;
 DefaultStrategyConfig config;config.affine_search.enabled=true;DefaultPolicy policy(config);
 auto initialized=DefaultAffineSearchTestAccess::initialize(policy,k);assert(initialized.status==AffineMatrixInitStatus::Applied&&session.active_sources()==40);
 assert(session.retained_bytes()<100000000);
 const auto arena=k.arena;const auto watches=k.watches.arena;const auto id=k.next_clause_id,owner=session.owner_version(),bytes=proof.premise_bytes();const auto position=std::ftell(journal);
 // Changed positive byte profile cannot replace/release the admitted owner.
 auto tiny=session.persistence_limits();tiny.maximum_retained_bytes=256;
 AffineMatrixSearchSession declined;
 const auto rejected=affine_matrix_search_initialize(k,declined,session.parameters(),AffineMatrixAdmission::AuthenticatedSubimage,tiny);
 assert(rejected.status==AffineMatrixInitStatus::ResourceExhausted&&!declined.enabled());
 assert(k.arena==arena&&k.watches.arena==watches&&k.next_clause_id==id&&session.owner_version()==owner&&proof.premise_bytes()==bytes&&std::ftell(journal)==position);
 const auto service=DefaultAffineSearchTestAccess::step(policy,k);assert(service.synchronized&&service.status==AffineMatrixServiceStatus::NoEvent);
 assert(policy.affine_search_diagnostics().root_services>0);
 MatrixEvent conditioned;
 assert(matrix.synchronize({{1,true}},[](uint64_t){return true;},conditioned)==MatrixStatus::Complete);
 assert(conditioned.kind==MatrixEventKind::Implication&&conditioned.implied_variable==4&&
        conditioned.implied_value&&(conditioned.reason==std::vector<int32_t>{-1,4}));
 kernel_assign_decision(k,kernel_internalize(k,1));assert(propagate_apply(k)==ApplyResult::Ok);
 const auto pending_owner=session.owner_version(),pending_id=k.next_clause_id;
 const auto pending_bytes=proof.premise_bytes();const std::vector<Lit> pending_trail(k.trail.begin(),k.trail.end());
 const auto useful=DefaultAffineSearchTestAccess::step(policy,k);
 assert(useful.status==AffineMatrixServiceStatus::Applied&&
        useful.installed_reason==std::vector<int32_t>({-1,4})&&session.enabled());
 assert(session.owner_version()==pending_owner&&k.next_clause_id>pending_id&&
        proof.premise_bytes()>=pending_bytes&&k.trail==pending_trail&&
        !k.value(kernel_internalize(k,4)));
 kernel_backtrack(k,0);
 assert(DefaultAffineSearchTestAccess::step(policy,k).status==AffineMatrixServiceStatus::NoEvent);
 // Complete planted model is checked against every original input clause.
 for(const auto &clause:clauses){bool satisfied=false;for(int x:clause)satisfied|=x<0;assert(satisfied);}
 if(argc>1){FILE *out=std::fopen((base+".model").c_str(),"w");assert(out);std::fputs("s SATISFIABLE\nv ",out);for(int i=1;i<=120;++i)std::fprintf(out,"-%d ",i);std::fputs("0\n",out);std::fclose(out);}
 session.disable(k);proof.flush();assert(!proof.unbacked_deletions());k.proof=nullptr;k.structural_provenance=nullptr;std::fclose(journal);
 if(argc==1)std::remove((base+".dsr").c_str());
}
