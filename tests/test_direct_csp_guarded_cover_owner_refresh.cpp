#undef NDEBUG
#include "dimacs_text.hpp"
#include "test_io.hpp"
#include "rules/semantic_extension/direct_csp_guarded_cover_action.hpp"
#include "proof/proof.hpp"
#include <algorithm>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <sys/stat.h>

using namespace tactix;
namespace {
using Selection=DirectCspGuardedCoverActionSelection;
using ActionStatus=DirectCspSupportActionStatus;
const direct_csp_guarded_cover::Bounds bounds{100,1000,10,10,10,10,10,10};
const DirectCspGuardedCoverActionLimits limits{100,1000,1000000,10,10,10,10,10,10,10000,10,100,100000,100,10,1000000};
Selection selection(bool signed_domain){
 Selection s{{{1,2,3},{4,5},{11,12}},{0,0,0},{8,9,10},
             {{6,{4,7}}},{{8,0,0,0},{9,0,1,0},{10,0,2,0}}};
 if(signed_domain){s.domains[1]={-4,-5};s.exact_or[0].inputs[0]=-4;}
 return s;
}
bool unbounded(uint64_t){return true;}
std::string dimacs(const Selection &s){
 const auto expected=direct_csp_guarded_cover::expected_clauses(s,bounds,unbounded);
 assert(expected.status==direct_csp_guarded_cover::Status::Planned);
 return dimacs_text(12,expected.clauses);
}
Ref find(Kernel &k,uint64_t id){
 for(Ref ref=0;ref<k.arena.size();){auto *c=clause_at(k.arena,ref);if(!c->garbage()&&c->id()==id)return ref;ref+=clause_actual_words(c);}return INVALID_REF;
}
void exercise(bool signed_domain,const char *output){
 const auto first_selection=selection(signed_domain);
 Kernel k;Proof proof;std::string proof_path;
 if(output){const auto base=std::string(output)+(signed_domain?"-signed":"-positive");
   write_file((base+".cnf").c_str(),dimacs(first_selection));
   proof_path=base+".dsr";assert(proof.open(proof_path.c_str(),ProofFormat::DsrAscii));k.proof=&proof;
 }
 parse_dimacs_text(dimacs(first_selection),k);
 FILE *journal=std::tmpfile();assert(journal);k.structural_provenance=journal;
 DirectCspGuardedCoverActionSession session;
 const auto first=direct_csp_guarded_cover_action_apply(k,session,limits,first_selection);
 assert(first.status==ActionStatus::Applied&&session.committed_keys()==1);
 auto replay=[&](const Selection &s){return direct_csp_guarded_cover_action_apply(k,session,limits,s).status;};
 // Domain presentation permutations preserve the existing normalized image+
 // full tuple identity, including signed values. Blocker indices move with it.
 auto permuted=first_selection;std::swap(permuted.domains[0],permuted.domains[2]);std::swap(permuted.selected_values[0],permuted.selected_values[2]);
 for(auto &b:permuted.blockers)b.domain=2;
 assert(replay(permuted)==ActionStatus::AlreadyPublished);
 auto invalid=first_selection;invalid.selected_values[0]=99;
 assert(replay(invalid)==ActionStatus::Invalid&&session.committed_keys()==1);
 // Learn an exact duplicate while the old cover still proves it, then retire
 // the old occurrence. There is exactly one alive cover at action entry.
 std::vector<Lit> cover;for(auto l:first_selection.cover)cover.push_back(kernel_internalize(k,l));
 auto sorted_cover=cover;std::sort(sorted_cover.begin(),sorted_cover.end());
 Ref old_ref=INVALID_REF;
 for(const auto [ref,c]:arena_records(k.arena)){std::vector<Lit> got(c->lits,c->lits+c->size);std::sort(got.begin(),got.end());if(!c->garbage()&&got==sorted_cover)old_ref=ref;}
 assert(old_ref!=INVALID_REF);const uint64_t old_cover=clause_at(k.arena,old_ref)->id();
 const auto replacement_id=++k.next_clause_id;
 if(k.proof)assert(k.proof->add_external_rup(replacement_id,first_selection.cover.data(),first_selection.cover.size()));
 kernel_add_large_clause(k,cover.data(),cover.size(),true,3,replacement_id);
 kernel_delete_clause(k,clause_at(k.arena,old_ref));
 assert(find(k,replacement_id)!=INVALID_REF);

 auto second_selection=first_selection;second_selection.selected_values[0]=1;
 auto proof_bytes=[&](){if(proof_path.empty())return uint64_t(0);proof.flush();struct stat image{};assert(!stat(proof_path.c_str(),&image));return uint64_t(image.st_size);};
 auto neutral=[&](const DirectCspGuardedCoverActionLimits &bounded,ActionStatus expected){
   const auto arena=k.arena;const auto watch_arena=k.watches.arena;const auto id=k.next_clause_id,sequence=k.structural_action_sequence;
   const auto extent=k.external_to_internal;const auto position=std::ftell(journal);const auto initial_proof_bytes=proof_bytes();
   const auto outcome=direct_csp_guarded_cover_action_apply(k,session,bounded,second_selection);
   assert(outcome.status==expected&&outcome.result==ApplyResult::NoOp);
   assert(k.arena==arena&&k.watches.arena==watch_arena&&k.next_clause_id==id&&k.structural_action_sequence==sequence&&k.external_to_internal==extent&&std::ftell(journal)==position);
   // Only the public session shape is checked: the retained owner's large-clause
   // ids are refresh-tolerant and never read back, so their content is not.
   assert(session.committed_keys()==1&&session.retained_owners()==1&&proof_bytes()==initial_proof_bytes);
   assert(outcome.effort.recognition_work==outcome.effort.owner_capture_work+outcome.live.recognition_work);
   return outcome;
 };
 auto bounded=limits;bounded.maximum_journal_bytes=1;
 const auto failed_journal=neutral(bounded,ActionStatus::ResourceExhausted);
 assert(failed_journal.effort.owner_capture_work>0); // Refresh was staged first.
 bounded=limits;bounded.maximum_recognition_work=1;neutral(bounded,ActionStatus::ResourceExhausted);
 volatile sig_atomic_t stopped=1;k.stop=&stopped;neutral(limits,ActionStatus::Interrupted);k.stop=nullptr;
 // No stale namespace or retired-variable forgiveness while refreshing IDs.
 const auto original_mapping=k.external_to_internal[1];k.external_to_internal[1]=k.external_to_internal[2];neutral(limits,ActionStatus::Stale);k.external_to_internal[1]=original_mapping;
 const Var variable=var_of(kernel_internalize(k,1));const auto original_statuses=k.var_status;k.var_status.resize(k.nvars,VarStatus::Active);k.var_status[variable]=VarStatus::Eliminated;neutral(limits,ActionStatus::Stale);k.var_status=original_statuses;
 const long before_second=std::ftell(journal);
 const auto second=direct_csp_guarded_cover_action_apply(k,session,limits,second_selection);
 assert(second.status==ActionStatus::Applied&&session.committed_keys()==2&&session.retained_owners()==1);
 assert(second.effort.owner_capture_work&&second.effort.recognition_work<=limits.maximum_recognition_work);
 assert(second.effort.recognition_work==second.effort.owner_capture_work+second.live.recognition_work);
 // The refreshed owner cites the replacement cover, not the retired one.
 const std::string record=written_bytes(journal).substr(size_t(before_second));
 assert(record.find("\"native_id\":"+std::to_string(replacement_id)+",")!=std::string::npos);
 assert(record.find("\"native_id\":"+std::to_string(old_cover)+",")==std::string::npos);
 // Durable committed identity survives removal of its published consequence.
 const auto output_ref=find(k,first.live.published_clause_id);assert(output_ref!=INVALID_REF);kernel_delete_clause(k,clause_at(k.arena,output_ref));
 assert(replay(first_selection)==ActionStatus::AlreadyPublished&&replay(second_selection)==ActionStatus::AlreadyPublished);
 assert(session.committed_keys()==2);
 proof.flush();k.proof=nullptr;std::fclose(journal);
}
}
int main(int argc,char **argv){exercise(false,argc>1?argv[1]:nullptr);exercise(true,argc>1?argv[1]:nullptr);return 0;}
