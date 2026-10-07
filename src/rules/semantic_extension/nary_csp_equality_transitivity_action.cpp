#include "rules/semantic_extension/nary_csp_equality_transitivity_action.hpp"
#include "proof/proof.hpp"
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <string>
namespace tactix {
struct NaryCspEqualityTransitivitySessionAccess {
 static void commit(NaryCspEqualityTransitivitySession &s,const Kernel &k,
                    std::vector<NaryCspEqualityTransitivityKey> keys) {
   s.owner_=&k;s.keys_=std::move(keys);
 }
};
namespace {
using Selection=NaryCspEqualityTransitivitySelection;
using Status=NaryCspEqualityTransitivityStatus;
using ExternalClause=equality_transitivity::Clause;
constexpr std::array<std::pair<size_t,size_t>,3> pairs{{{0,1},{0,2},{1,2}}};
struct Meter {
 const Kernel &kernel;uint64_t bound,work=0;bool exhausted=false,interrupted=false;
 bool charge(uint64_t n=1){
   if(kernel_terminating(kernel)){interrupted=true;return false;}
   if(n>bound-work){exhausted=true;return false;}work+=n;return true;
 }
 Status failure()const{return interrupted?Status::Interrupted:Status::ResourceExhausted;}
};
uint64_t equality_key_bytes(const NaryCspEqualityKey &key){
 uint64_t bytes=sizeof(key)+key.domains.size()*sizeof(ExternalClause);
 for(const auto &d:key.domains)bytes+=d.size()*sizeof(int32_t);
 return bytes;
}
uint64_t key_bytes(const Selection &s) {
 uint64_t n=sizeof(s);
 for(const auto &d:s.domains)n+=d.size()*sizeof(int32_t);
 for(const auto &key:s.equalities){n+=key.domains.size()*sizeof(ExternalClause);for(const auto &d:key.domains)n+=d.size()*sizeof(int32_t);}
 return n;
}
const NaryCspEqualityRecord *record(const NaryCspEqualitySession &session,
 const NaryCspEqualityKey &key,Meter &meter){
 uint64_t requested=1;for(const auto &d:key.domains)requested+=d.size();
 for(const auto &candidate:session.records()){
   uint64_t work=requested+1;for(const auto &d:candidate.key.domains)work+=d.size();
   if(!meter.charge(work))return nullptr;
   if(candidate.key==key)return &candidate;
 }return nullptr;
}
const NaryCspPrefixOwnerSnapshot *owner(const NaryCspPrefixActionSession &session,
 const NaryCspEqualityKey &key,Meter &meter){
 for(const auto &snapshot:session.snapshots()){
   bool equal=snapshot.relation.domains.size()==key.domains.size();
   if(!meter.charge())return nullptr;
   if(!equal)continue;
   for(size_t i=0;i<key.domains.size();++i){
     if(!meter.charge(snapshot.relation.domains[i].values.size()+key.domains[i].size()))return nullptr;
     equal&=snapshot.relation.domains[i].values==key.domains[i];
   }
   if(equal)return &snapshot;
 }return nullptr;
}
bool same_clause(ExternalClause a,ExternalClause b,Meter &m) {
 // Conservative quadratic bound is charged before sorting/copy comparisons.
 if(!m.charge((a.size()+b.size())*(a.size()+b.size()+1)))return false;
 std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());return a==b;
}
uint64_t live_id(const Kernel &k,const ExternalClause &clause,Meter &m,bool &exact) {
 std::vector<Lit> wanted;
 if(!m.charge(clause.size()*clause.size()+clause.size()))return 0;
 for(auto external:clause){auto l=kernel_internalize(k,external);
   if(l==INVALID_LIT||kernel_var_retired(k,var_of(l)))return 0;
   wanted.push_back(l);}
 std::sort(wanted.begin(),wanted.end());
 if(wanted.size()==2){
   Lit a=wanted[0],b=wanted[1];if(!m.charge(2))return 0;
   if(k.watches.binary_size(b)<k.watches.binary_size(a))std::swap(a,b);
   for(const auto &w:k.watches.entries(a)){
     if(!m.charge())return 0;
     if(!w.binary()){if(k.watches.heads[a].binaries_first())break;continue;}
     if(w.other()==b){const auto id=k.binary_ids.get(kernel_binary_key(a,b));exact=id!=0&&k.binary_ids.tracks_values();return id?id:1;}
   }return 0;
 }
 for(Ref ref=0;ref<k.arena.size();){
   const auto *c=clause_at(k.arena,ref);const auto words=clause_actual_words(c);
   if(!m.charge(words))return 0;
   if(!c->garbage()&&c->size==wanted.size()){
     if(!m.charge(c->size*(uint64_t(c->size)+1)))return 0;
     std::vector<Lit> got(c->lits,c->lits+c->size);std::sort(got.begin(),got.end());
     if(got==wanted){exact=true;return c->id();}
   }ref+=words;
 }return 0;
}
NaryCspPrefixLiveRequest capture_request(const NaryCspEqualityKey &key,
 const NaryCspEqualityTransitivityLimits &l,uint64_t work,uint64_t clauses,uint64_t literals){
 NaryCspPrefixLiveRequest r;r.domains=key.domains;r.selection={0,{{1,0},{2,0}}};
 r.maximum_source_clauses=clauses;r.maximum_source_literals=literals;
 r.maximum_recognition_work=work;r.maximum_domains=3;r.maximum_domain_values=l.maximum_domain_values;
 r.maximum_forbidden_tuples=l.maximum_forbidden_tuples;r.maximum_prefix_arity=2;
 r.maximum_definition_clause_literals=3;r.maximum_proof_work=l.maximum_proof_work;
 r.maximum_proof_additions=l.maximum_proof_additions;
 r.maximum_proof_literal_occurrences=l.maximum_proof_literal_occurrences;
 r.maximum_external_extent=l.maximum_external_extent;return r;
}
std::string clause_json(const ExternalClause &c){std::string s="[";for(auto l:c){if(s.size()>1)s+=',';s+=std::to_string(l);}return s+"]";}
}
bool NaryCspEqualityTransitivityLimits::valid()const{
 return maximum_source_clauses&&maximum_source_literals&&maximum_recognition_work&&maximum_planning_work&&
 maximum_domain_values>=2&&maximum_forbidden_tuples&&maximum_proof_work&&maximum_proof_additions&&
 maximum_proof_literal_occurrences&&maximum_journal_bytes&&maximum_external_extent&&maximum_session_keys&&
 maximum_session_bytes&&maximum_arena_words&&maximum_watch_words&&maximum_preparation_bytes;
}
RuleEligibility nary_csp_equality_transitivity_guard(const Kernel &k){return nary_csp_prefix_action_guard(k);}
bool nary_csp_equality_transitivity_selection_valid(const Selection &s){
 if(!equality_transitivity::domains_valid(s.domains))return false;
 for(size_t i=0;i<3;++i){const auto &key=s.equalities[i];
   if(key.domains.size()!=3||key.first_equal_domain>=3||key.second_equal_domain>=3||key.residual_domain>=3||
      key.first_equal_domain>=key.second_equal_domain||key.residual_domain==key.first_equal_domain||key.residual_domain==key.second_equal_domain)return false;
   // Validate complete owner's canonical, disjoint signed domains as well.
   equality_transitivity::Domains owner{key.domains[0],key.domains[1],key.domains[2]};
   if(!equality_transitivity::domains_valid(owner)||key.domains[key.first_equal_domain]!=s.domains[pairs[i].first]||
      key.domains[key.second_equal_domain]!=s.domains[pairs[i].second])return false;
   if(!s.equality_variables[i]||s.equality_variables[i]>INT32_MAX)return false;
   for(size_t j=0;j<i;++j)if(s.equality_variables[i]==s.equality_variables[j])return false;
 }
 return true;
}
NaryCspEqualityTransitivityResult nary_csp_equality_transitivity_apply(
 Kernel &k,const NaryCspPrefixActionSession &prefix,const NaryCspEqualitySession &equalities,
 NaryCspEqualityTransitivitySession &session,const NaryCspEqualityTransitivityLimits &limits,
 const Selection &selected,FILE *records){
 NaryCspEqualityTransitivityResult out;
 if(!nary_csp_equality_transitivity_guard(k)){out.status=Status::Inapplicable;return out;}
 if(!limits.valid()||!records||selected.domains[0].size()>limits.maximum_domain_values)return out;
 Meter meter{k,limits.maximum_recognition_work};
 auto finish=[&](){out.recognition_work=meter.work;return out;};
 // Before unbounded validation, price all caller-owned selection storage.
 const auto selection_bytes=key_bytes(selected);
 const uint64_t values=selected.domains[0].size();
 if(values && values>(UINT64_MAX-selection_bytes)/(18*values)){out.status=Status::ResourceExhausted;return finish();}
 if(selection_bytes>limits.maximum_preparation_bytes||!meter.charge(selection_bytes+18*values*values)){out.status=meter.failure();return finish();}
 if(!nary_csp_equality_transitivity_selection_valid(selected))return finish();
 if(!prefix.belongs_to(k)||!equalities.belongs_to(k)||(session.retained()&&!session.belongs_to(k))){out.status=Status::Inapplicable;return finish();}
 if(k.proof&&(k.proof->lrat()||k.proof->pb())){out.status=Status::UnsupportedProof;return finish();}
 for(const auto &key:session.keys()){
   if(!meter.charge(key_bytes(key)+selection_bytes)){out.status=meter.failure();return finish();}
   if(key==selected){out.status=Status::Duplicate;return finish();}
 }
 uint64_t session_bytes=selection_bytes;
 for(const auto &key:session.keys()){
   if(!meter.charge()||key_bytes(key)>limits.maximum_session_bytes-std::min(limits.maximum_session_bytes,session_bytes)){out.status=meter.failure();return finish();}
   session_bytes+=key_bytes(key);
 }
 if(session.keys().size()>=limits.maximum_session_keys||session_bytes>limits.maximum_session_bytes){out.status=Status::ResourceExhausted;return finish();}
 out.session_bytes=session_bytes;
 uint64_t preparation=selection_bytes+session_bytes;
 bool source_resource=false;
 auto source=[&](const ExternalClause &c,uint64_t expected_id,bool require_id){
   if(out.source_clauses==limits.maximum_source_clauses||c.size()>limits.maximum_source_literals-out.source_literals){source_resource=true;return false;}
   const auto bytes=3*sizeof(NaryCspEqualityTransitivitySource)+2*c.size()*sizeof(int32_t);
   if(bytes>limits.maximum_preparation_bytes-std::min(preparation,limits.maximum_preparation_bytes)){source_resource=true;return false;}
   bool exact=false;const auto id=live_id(k,c,meter,exact);
   if(!id||(require_id&&expected_id&&id!=expected_id))return false;
   // Fresh capture explicitly records id0 for unarchived binary premises.
   // Match their literals, never manufacture a unique proof occurrence ID.
   const bool unarchived_binary=require_id&&!expected_id&&c.size()==2;
   ++out.source_clauses;out.source_literals+=c.size();preparation+=bytes;
   out.sources.push_back({c,unarchived_binary?0:id,exact&&!unarchived_binary});return true;
 };
 for(size_t i=0;i<3;++i){
   const auto *r=record(equalities,selected.equalities[i],meter);
   if(meter.exhausted||meter.interrupted){out.status=meter.failure();return finish();}
   if(!r||r->extension_variable!=selected.equality_variables[i]){out.status=Status::MissingEquality;return finish();}
   const auto *saved=owner(prefix,r->key,meter);
   if(meter.exhausted||meter.interrupted){out.status=meter.failure();return finish();}
   if(!saved){out.status=Status::Stale;return finish();}
   if(saved->source.size()>limits.maximum_source_clauses-out.source_clauses||saved->source_literals>limits.maximum_source_literals-out.source_literals){out.status=Status::ResourceExhausted;return finish();}
   // Bound the simultaneous retained-source copy plus captured relation tables.
   uint64_t snapshot_bytes=sizeof(*saved)+saved->source.size()*sizeof(nary_csp_prefix::Premise)+saved->source_literals*sizeof(int32_t);
   for(const auto &d:saved->relation.domains)snapshot_bytes+=sizeof(d)+d.values.size()*sizeof(int32_t)+d.at_most_one.size()*sizeof(uint64_t);
   for(const auto &t:saved->relation.forbidden)snapshot_bytes+=sizeof(t)+t.value_indices.size()*sizeof(uint32_t);
   if(snapshot_bytes>limits.maximum_preparation_bytes/3||3*snapshot_bytes>limits.maximum_preparation_bytes-std::min(preparation,limits.maximum_preparation_bytes)){out.status=Status::ResourceExhausted;return finish();}
   preparation+=3*snapshot_bytes;
   NaryCspPrefixOwnerSnapshot fresh;
   const auto captured=nary_csp_prefix_live_capture_owner(k,capture_request(r->key,limits,meter.bound-meter.work,
     limits.maximum_source_clauses-out.source_clauses,limits.maximum_source_literals-out.source_literals),fresh);
   if(!meter.charge(captured.recognition_work)){out.status=meter.failure();return finish();}
   if(captured.status!=NaryCspPrefixLiveStatus::Applied){out.status=captured.status==NaryCspPrefixLiveStatus::Interrupted?Status::Interrupted:
     captured.status==NaryCspPrefixLiveStatus::ResourceExhausted?Status::ResourceExhausted:Status::Stale;out.recognition_work=meter.work;return finish();}
   fresh.capture_work=saved->capture_work;fresh.external_extent=saved->external_extent;
   if(fresh!=*saved){out.status=Status::Stale;return finish();}
   for(const auto &p:fresh.source)if(!source(p.literals,p.native_proof_id,true)){out.status=meter.exhausted||meter.interrupted?meter.failure():
      source_resource?Status::ResourceExhausted:Status::Stale;return finish();}
   const auto n=selected.domains[0].size();
   if(r->definition_clauses.size()!=3*n){out.status=Status::Stale;return finish();}
   std::vector<ExternalClause> expected;
   const auto &a=r->key.domains[r->key.first_equal_domain],&b=r->key.domains[r->key.second_equal_domain],&d=r->key.domains[r->key.residual_domain];
   for(size_t j=0;j<n;++j){const auto e=int32_t(r->extension_variable);expected.push_back({e,-a[j],-b[j]});expected.push_back({-e,-a[j],b[j]});expected.push_back({-e,-a[j],-d[j]});}
   // Retained vector order is defined by the existing planner; compare as a
   // multiset to avoid trusting an accidental storage-order convention.
   std::vector<bool> used(expected.size());
   for(const auto &c:r->definition_clauses){bool found=false;
     for(size_t j=0;j<expected.size();++j)if(!used[j]&&same_clause(c,expected[j],meter)){used[j]=true;found=true;break;}
     if(!found||!source(c,0,false)){out.status=meter.exhausted||meter.interrupted?meter.failure():source_resource?Status::ResourceExhausted:Status::Stale;return finish();}
   }
   preparation-=3*snapshot_bytes;
 }
 out.recognition_work=meter.work;
 equality_transitivity::Plan plan;
 if(!equality_transitivity::plan(selected.domains,selected.equality_variables,limits.maximum_planning_work,plan)){out.status=Status::ResourceExhausted;return finish();}
 out.planning_work=plan.planning_work;
 if(plan.additions>limits.maximum_proof_additions||plan.literal_occurrences>limits.maximum_proof_literal_occurrences||plan.publication_work>limits.maximum_proof_work||
   k.next_clause_id>uint64_t(INT64_MAX)-plan.additions-plan.deletions){out.status=Status::ResourceExhausted;return finish();}
 std::array<std::array<Lit,3>,3> internal{};
 for(size_t i=0;i<3;++i){bool exact=false;
   if(live_id(k,plan.triangles[i],meter,exact)){out.status=Status::Duplicate;return finish();}
   if(meter.exhausted||meter.interrupted){out.status=meter.failure();return finish();}
   for(size_t j=0;j<3;++j){const auto l=kernel_internalize(k,plan.triangles[i][j]);if(l==INVALID_LIT||kernel_var_retired(k,var_of(l))){out.status=Status::Stale;return finish();}internal[i][j]=l;}
 }
 out.recognition_work=meter.work;
 const uint64_t arena_words=3*clause_words(3),watch_words=12;
 if(k.arena.size()>UINT32_MAX-arena_words||arena_words>limits.maximum_arena_words||watch_words>limits.maximum_watch_words){out.status=Status::ResourceExhausted;return finish();}
 const uint64_t copy_words=k.watches.arena.size()+k.watches.heads.size()*uint64_t(sizeof(WatchHead)/sizeof(uint32_t));
 const uint64_t arena_copy_words=k.arena.capacity()<k.arena.size()+arena_words?k.arena.size():0;
 const uint64_t extra_bytes=(2*copy_words+watch_words+arena_words+arena_copy_words)*sizeof(uint32_t)+plan.literal_occurrences*sizeof(int32_t)+plan.additions*sizeof(ExternalClause)+3*sizeof(ExternalClause)+9*sizeof(int32_t);
 if(extra_bytes>limits.maximum_preparation_bytes-std::min(preparation,limits.maximum_preparation_bytes)){out.status=Status::ResourceExhausted;return finish();}
 // Prepare detached watch storage. Nothing authoritative changes on decline.
 if(!meter.charge(copy_words+watch_words+arena_copy_words)){out.status=meter.failure();return finish();}
 auto watches=k.watches;watches.begin_batch_reserve();
 uint64_t total=0;for(const auto &h:watches.heads)total+=h.size;
 if(total>UINT32_MAX-watch_words){out.status=Status::ResourceExhausted;return finish();}
 for(const auto &c:internal)for(size_t i=0;i<2;++i){if(watches.heads[c[i]].capacity>UINT32_MAX-2){out.status=Status::ResourceExhausted;return finish();}
   watches.reserve_batch_word(c[i]);watches.reserve_batch_word(c[i]);}
 watches.finish_batch_reserve();out.preparation_work=copy_words+watch_words+arena_copy_words;
 // Bound serialized storage and charge formatting before constructing it.
 const uint64_t journal_envelope=512+selection_bytes*12+out.source_clauses*128+out.source_literals*12;
 if(journal_envelope>limits.maximum_journal_bytes||journal_envelope>limits.maximum_preparation_bytes/3||3*journal_envelope>limits.maximum_preparation_bytes-std::min(preparation+extra_bytes,limits.maximum_preparation_bytes)){
   out.status=Status::ResourceExhausted;return finish();}
 if(!meter.charge(journal_envelope)){out.status=meter.failure();return finish();}
 std::string journal="{\"kind\":\"nary-csp-equality-transitivity\",\"action\":"+std::to_string(k.structural_action_sequence+1)+",\"projection\":\"identity\",\"new_variables\":0,\"domains\":[";
 for(size_t i=0;i<3;++i){if(i)journal+=',';journal+=clause_json(selected.domains[i]);}
 journal+="],\"owners\":[";
 for(size_t i=0;i<3;++i){if(i)journal+=',';const auto &key=selected.equalities[i];
   journal+="{\"domains\":[";for(size_t j=0;j<3;++j){if(j)journal+=',';journal+=clause_json(key.domains[j]);}
   journal+="],\"left\":"+std::to_string(key.first_equal_domain)+",\"right\":"+std::to_string(key.second_equal_domain)+",\"residual\":"+std::to_string(key.residual_domain)+",\"equality\":"+std::to_string(selected.equality_variables[i])+"}";}
 journal+="],\"sources\":[";
 for(size_t i=0;i<out.sources.size();++i){if(i)journal+=',';const auto &source=out.sources[i];
   journal+="{\"id\":"+std::to_string(source.native_proof_id)+",\"exact_id\":"+(source.exact_proof_id?"true":"false")+",\"clause\":"+clause_json(source.literals)+"}";}
 journal+="]}\n";
 if(journal.size()>journal_envelope)std::abort();

 std::vector<Selection> keys;keys.reserve(session.keys().size()+1);
 for(const auto &key:session.keys())keys.push_back(key);
 keys.insert(std::lower_bound(keys.begin(),keys.end(),selected),selected);
 std::vector<ExternalClause> consequences(plan.triangles.begin(),plan.triangles.end());
 if(kernel_terminating(k)){out.status=Status::Interrupted;return finish();}
 // Commit boundary: no recoverable failures or interruption after this point.
 k.arena.reserve(k.arena.size()+arena_words);k.watches=std::move(watches);k.close_startup_certificate();
 out.proposed_action_id=k.structural_action_sequence+1;
 for(size_t direction=0;direction<3;++direction){
   const auto start=k.next_clause_id;
   for(const auto &bridge:plan.bridges[direction]){const auto id=++k.next_clause_id;
     if(k.proof&&!k.proof->add_external_rup(id,bridge.data(),bridge.size()))std::abort();}
   const auto id=++k.next_clause_id;const auto &triangle=plan.triangles[direction];
   if(k.proof&&!k.proof->add_external_rup(id,triangle.data(),triangle.size()))std::abort();
   kernel_add_large_clause(k,internal[direction].data(),3,false,0,id,false);out.published_clause_ids[direction]=id;
   for(size_t i=0;i<plan.bridges[direction].size();++i){const auto &bridge=plan.bridges[direction][i];
     if(k.proof&&!k.proof->del_external(++k.next_clause_id,start+i+1,bridge.data(),bridge.size()))std::abort();
     if(!k.proof)++k.next_clause_id;}
 }
 if(std::fwrite(journal.data(),1,journal.size(),records)!=journal.size()||std::fflush(records)||std::ferror(records))std::abort();
 NaryCspEqualityTransitivitySessionAccess::commit(session,k,std::move(keys));k.structural_action_sequence=out.proposed_action_id;
 out.published_consequences=std::move(consequences);
 out.committed_action_id=out.proposed_action_id;out.status=Status::Applied;out.result=ApplyResult::Ok;
 out.proof_work=plan.publication_work;out.proof_additions=plan.additions;out.proof_deletions=plan.deletions;
 out.proof_literal_occurrences=plan.literal_occurrences;out.journal_bytes=journal.size();out.recognition_work=meter.work;
 out.propagation_pending=!kernel_propagation_fixpoint(k);return finish();
}
NaryCspEqualityTransitivityOfferResult nary_csp_equality_transitivity_offer(
 Kernel &k,const NaryCspEqualitySession &equalities,const NaryCspEqualityTransitivitySession &session,
 const NaryCspEqualityTransitivityLimits &limits,const NaryCspEqualityTransitivityOfferWindow &window){
 NaryCspEqualityTransitivityOfferResult out;
 if(!nary_csp_equality_transitivity_guard(k)){out.status=Status::Inapplicable;return out;}
 if(!limits.valid()||!window.maximum_candidates||!window.maximum_offers||!window.maximum_enumeration_work)return out;
 if(!equalities.belongs_to(k)||(session.retained()&&!session.belongs_to(k))){out.status=Status::Stale;return out;}
 Meter meter{k,window.maximum_enumeration_work};std::vector<Selection> candidates;
 uint64_t candidate_bytes=0,largest_key=1;
 const auto &records=equalities.records();
 for(size_t a=0;a<records.size();++a)for(size_t b=a+1;b<records.size();++b)for(size_t c=b+1;c<records.size();++c){
   if(out.candidates==window.maximum_candidates||!meter.charge()){
     out.status=meter.interrupted?Status::Interrupted:Status::ResourceExhausted;out.enumeration_work=meter.work;return out;}
   ++out.candidates;const std::array<size_t,3> indexes{a,b,c};
   std::vector<ExternalClause> domains;
   for(auto index:indexes){const auto &r=records[index];
     if(equality_key_bytes(r.key)>limits.maximum_preparation_bytes/3||!meter.charge(3*equality_key_bytes(r.key))){out.status=meter.failure();out.enumeration_work=meter.work;return out;}
     if(r.key.first_equal_domain>=r.key.domains.size()||r.key.second_equal_domain>=r.key.domains.size())continue;
     domains.push_back(r.key.domains[r.key.first_equal_domain]);domains.push_back(r.key.domains[r.key.second_equal_domain]);
   }
   std::sort(domains.begin(),domains.end());domains.erase(std::unique(domains.begin(),domains.end()),domains.end());
   if(domains.size()!=3||domains[0].size()>limits.maximum_domain_values)continue;
   Selection selection;std::copy(domains.begin(),domains.end(),selection.domains.begin());
   bool complete=true;
   for(size_t pair=0;pair<3;++pair){bool found=false;
     for(auto index:indexes){const auto &r=records[index];
       if(r.key.first_equal_domain<r.key.domains.size()&&r.key.second_equal_domain<r.key.domains.size()&&
          r.key.domains[r.key.first_equal_domain]==domains[pairs[pair].first]&&r.key.domains[r.key.second_equal_domain]==domains[pairs[pair].second]){
          selection.equalities[pair]=r.key;selection.equality_variables[pair]=r.extension_variable;found=true;break;}}
     complete&=found;
   }
   if(!complete)continue;
   const uint64_t n=selection.domains[0].size();
   if(n && (n>meter.bound/(18*n)||!meter.charge(18*n*n))){out.status=meter.failure();out.enumeration_work=meter.work;return out;}
   if(!nary_csp_equality_transitivity_selection_valid(selection))continue;
   if(window.have_after){
     if(!meter.charge(key_bytes(window.after)+key_bytes(selection))){out.status=meter.failure();out.enumeration_work=meter.work;return out;}
     if(!(window.after<selection))continue;
   }
   bool duplicate=false;
   for(const auto &key:session.keys()){
     if(!meter.charge(key_bytes(key)+key_bytes(selection))){out.status=meter.failure();out.enumeration_work=meter.work;return out;}
     if(key==selection){duplicate=true;break;}
   }
   if(duplicate)continue;
   // Explicit bound on temporary offer storage, independent of search policy.
   if(key_bytes(selection)>limits.maximum_preparation_bytes-std::min(candidate_bytes,limits.maximum_preparation_bytes)){
     out.status=Status::ResourceExhausted;out.enumeration_work=meter.work;return out;}
   candidate_bytes+=key_bytes(selection);largest_key=std::max(largest_key,key_bytes(selection));
   candidates.push_back(std::move(selection));
 }
 // Conservative sort bound; charge before comparison work or result mutation.
 const uint64_t count=candidates.size();
 if(count && (count>meter.bound/(count+1)||count*(count+1)>meter.bound/largest_key)){out.status=Status::ResourceExhausted;out.enumeration_work=meter.work;return out;}
 if(!meter.charge(count*(count+1)*largest_key)){
   out.status=meter.failure();out.enumeration_work=meter.work;return out;}
 std::sort(candidates.begin(),candidates.end());candidates.erase(std::unique(candidates.begin(),candidates.end()),candidates.end());
 out.complete=candidates.size()<=window.maximum_offers;
 if(candidates.size()>window.maximum_offers)candidates.resize(window.maximum_offers);
 out.offers=std::move(candidates);out.enumeration_work=meter.work;out.status=Status::Ready;
 if(!out.offers.empty()){out.have_resume_after=true;out.resume_after=out.offers.back();}return out;
}
} // namespace tactix
