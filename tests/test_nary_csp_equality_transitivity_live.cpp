#undef NDEBUG
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/semantic_extension/nary_csp_equality_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_transitivity_action.hpp"
#include <cassert>
#include <cstdio>
#include <sstream>
using namespace tactix;
namespace {
using Selection=NaryCspEqualityTransitivitySelection;
using Status=NaryCspEqualityTransitivityStatus;
NaryCspEqualityTransitivityLimits limits(){return {1000,10000,1000000,1000000,16,1000,10000,1000,10000,100000,100,100,1000000,1000,1000,1000000};}
std::string fixture(size_t n){
 std::vector<std::vector<int32_t>> clauses;
 for(size_t d=0;d<6;++d){std::vector<int32_t> cover;for(size_t j=0;j<n;++j)cover.push_back(int32_t(d*n+j+1));clauses.push_back(cover);
   for(size_t a=0;a<n;++a)for(size_t b=a+1;b<n;++b)clauses.push_back({-cover[a],-cover[b]});}
 for(auto triple:std::array<std::array<size_t,3>,3>{{{0,1,3},{0,2,4},{1,2,5}}})for(size_t j=0;j<n;++j)
   clauses.push_back({-int32_t(triple[0]*n+j+1),-int32_t(triple[1]*n+j+1),-int32_t(triple[2]*n+j+1)});
 std::ostringstream s;s<<"p cnf "<<6*n<<' '<<clauses.size()<<'\n';for(const auto &c:clauses){for(auto l:c)s<<l<<' ';s<<"0\n";}return s.str();
}
void parse(Kernel &k,size_t n){auto text=fixture(n);FILE *f=std::tmpfile();assert(f);assert(std::fwrite(text.data(),1,text.size(),f)==text.size());std::rewind(f);assert(!parse_dimacs(f,k));std::fclose(f);}
void settled(Kernel &k){if(propagate_guard(k))assert(propagate_apply(k)==ApplyResult::Ok);assert(kernel_propagation_fixpoint(k));}
Selection prepare(Kernel &k,NaryCspRelationOwnerSession &relations,NaryCspEqualitySession &equality,FILE *journal,size_t n){
 k.structural_provenance=journal;
 NaryCspEqualityLimits el{1000,10000,1000000,16,1000,10000,1000,10000,100000,1,100,100};
 Selection selection;
 for(size_t d=0;d<3;++d)for(size_t j=0;j<n;++j)selection.domains[d].push_back(int32_t(d*n+j+1));
 size_t index=0;
 for(auto triple:std::array<std::array<size_t,3>,3>{{{0,1,3},{0,2,4},{1,2,5}}}){
   std::vector<std::vector<int32_t>> owner;for(auto d:triple){std::vector<int32_t> domain;for(size_t j=0;j<n;++j)domain.push_back(int32_t(d*n+j+1));owner.push_back(domain);}
   auto e=nary_csp_equality_action_apply(k,relations,equality,el,{owner,{0,1,2}});assert(e.status==NaryCspEqualityActionStatus::Applied);settled(k);
   for(const auto &r:equality.records())if(r.key.domains==owner){selection.equalities[index]=r.key;selection.equality_variables[index]=r.extension_variable;}
   ++index;
 }assert(nary_csp_equality_transitivity_selection_valid(selection));return selection;
}
void run(size_t n,const char *output){
 Kernel k;Proof proof;
 if(output){const auto base=std::string(output)+"-"+std::to_string(n);FILE *f=std::fopen((base+".cnf").c_str(),"w");assert(f);auto text=fixture(n);assert(std::fwrite(text.data(),1,text.size(),f)==text.size());std::fclose(f);assert(proof.open((base+".dsr").c_str(),ProofFormat::DsrAscii));k.proof=&proof;}
 parse(k,n);FILE *journal=std::tmpfile();assert(journal);
 NaryCspRelationOwnerSession relations;NaryCspEqualitySession equality;NaryCspEqualityTransitivitySession session;
 auto selection=prepare(k,relations,equality,journal,n);auto l=limits();
 auto unchanged=[&](NaryCspEqualityTransitivityLimits bounded,Selection choice,Status status){
   auto arena=k.arena;auto watches=k.watches.arena;std::vector<Lit> trail(k.trail.begin(),k.trail.end());
   auto id=k.next_clause_id,action=k.structural_action_sequence;auto keys=session.keys();auto position=std::ftell(journal);
   auto result=nary_csp_equality_transitivity_apply(k,relations,equality,session,bounded,choice,journal);
   if(result.status!=status||result.result!=ApplyResult::NoOp)std::fprintf(stderr,"arity=%zu expected=%u actual=%u recognition=%llu source_clauses=%llu\n",n,unsigned(status),unsigned(result.status),(unsigned long long)result.recognition_work,(unsigned long long)result.source_clauses);
   assert(result.status==status&&result.result==ApplyResult::NoOp);
   assert(k.arena==arena&&k.watches.arena==watches&&k.trail==trail&&k.next_clause_id==id&&k.structural_action_sequence==action&&session.keys()==keys&&std::ftell(journal)==position);
 };
 auto bound=l;bound.maximum_recognition_work=1;unchanged(bound,selection,Status::ResourceExhausted);
 bound=l;bound.maximum_proof_work=1;unchanged(bound,selection,Status::ResourceExhausted);
 bound=l;bound.maximum_journal_bytes=1;unchanged(bound,selection,Status::ResourceExhausted);
 bound=l;bound.maximum_arena_words=1;unchanged(bound,selection,Status::ResourceExhausted);
 bound=l;bound.maximum_session_bytes=1;unchanged(bound,selection,Status::ResourceExhausted);
 volatile sig_atomic_t stopped=1;k.stop=&stopped;unchanged(l,selection,Status::Interrupted);k.stop=nullptr;
 auto wrong=selection;++wrong.equality_variables[2];unchanged(l,wrong,Status::MissingEquality);
 // A removed complete-definition premise must decline without repairing it.
 Ref stale_ref=INVALID_REF;
 const Lit first_equality=kernel_internalize(k,int32_t(selection.equality_variables[0]));
 for(Ref ref=0;ref<k.arena.size();){auto *c=clause_at(k.arena,ref);if(!c->garbage()&&c->size==3&&c->lits[0]==first_equality){stale_ref=ref;break;}ref+=clause_actual_words(c);}
 assert(stale_ref!=INVALID_REF);auto *stale=clause_at(k.arena,stale_ref);const auto saved_flags=stale->flags;stale->flags|=Clause::GARBAGE;
 unchanged(l,selection,Status::Stale);stale=clause_at(k.arena,stale_ref);stale->flags=saved_flags;

 NaryCspEqualityTransitivityOfferWindow w;w.maximum_candidates=100;w.maximum_offers=100;w.maximum_enumeration_work=1000000;
 auto offered=nary_csp_equality_transitivity_offer(k,equality,session,l,w);
 assert(offered.status==Status::Ready&&offered.complete&&offered.offers.size()==1&&offered.offers[0]==selection);
 const auto extent=k.external_to_internal.size();
 const auto id=k.next_clause_id;
 auto applied=nary_csp_equality_transitivity_apply(k,relations,equality,session,l,selection,journal);
 assert(applied.status==Status::Applied&&applied.result==ApplyResult::Ok&&applied.proof_additions==3*n+3&&applied.proof_deletions==3*n&&applied.proof_literal_occurrences==24*n+9);
 assert(k.external_to_internal.size()==extent&&k.next_clause_id==id+6*n+3&&applied.published_consequences.size()==3);
 for(const auto &source:applied.sources){if(source.literals.size()==2&&!k.binary_ids.tracks_values())assert(source.native_proof_id==0&&!source.exact_proof_id);else assert(source.native_proof_id&&source.exact_proof_id);}
 unchanged(l,selection,Status::Duplicate);settled(k);
 // Two equality decisions produce the third through an actual native triangle.
 kernel_assign_decision(k,kernel_internalize(k,int32_t(selection.equality_variables[0])));settled(k);
 kernel_assign_decision(k,kernel_internalize(k,int32_t(selection.equality_variables[1])));settled(k);
 const auto consequence=kernel_internalize(k,int32_t(selection.equality_variables[2]));assert(k.values[consequence]>0);
 const auto &assigned=k.assigned[var_of(consequence)];assert(!assigned.binary&&assigned.reason!=DECISION_REASON);
 const auto *reason=clause_at(k.arena,assigned.reason);assert(reason->id()==applied.published_clause_ids[0]);
 proof.flush();k.proof=nullptr;std::fclose(journal);
}
}
int main(int argc,char **argv){const char *output=argc>1?argv[1]:nullptr;run(2,output);run(3,output);run(4,output);return 0;}
