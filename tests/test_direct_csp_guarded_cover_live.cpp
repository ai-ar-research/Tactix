#undef NDEBUG
#include "rules/semantic_extension/direct_csp_guarded_cover_action.hpp"
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include <cassert>
#include <cstdio>
#include <csignal>
#include <sstream>
using namespace tactix;
struct Image {
  uint32_t nvars = 0;
  uint64_t next_id = 0;
  size_t arena = 0;
  size_t extent = 0;
  size_t trail = 0;
  uint64_t generation = 0;
  std::vector<Var> internal_to_external;
  std::vector<Var> external_to_internal;
  std::vector<size_t> watches;
};

Image image(const Kernel &kernel) {
  Image result;
  result.nvars = kernel.nvars;
  result.next_id = kernel.next_clause_id;
  result.arena = kernel.arena.size();
  result.extent = kernel.external_to_internal.size();
  result.trail = kernel.trail.size();
  result.generation = kernel.arena_generation;
  result.internal_to_external = kernel.internal_to_external;
  result.external_to_internal = kernel.external_to_internal;
  for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal)
    result.watches.push_back(kernel.watches.size(literal));
  return result;
}

bool operator==(const Image &a, const Image &b) {
  return a.nvars == b.nvars && a.next_id == b.next_id &&
         a.arena == b.arena && a.extent == b.extent &&
         a.trail == b.trail && a.generation == b.generation &&
         a.internal_to_external == b.internal_to_external &&
         a.external_to_internal == b.external_to_internal &&
         a.watches == b.watches;
}

int main(int argc, char **argv) {
  DirectCspGuardedCoverActionSelection selection{
      {{1,2,3},{4,5},{11,12}}, {0,0,0}, {8,9,10},
      {{6,{4,7}}}, {{8,0,0,0},{9,0,1,0},{10,0,2,0}}};
  direct_csp_guarded_cover::Bounds bounds{100,1000,10,10,10,10,10,10};
  auto expected=direct_csp_guarded_cover::expected_clauses(selection,bounds,[](uint64_t){return true;});
  std::ostringstream dimacs; dimacs<<"p cnf 12 "<<expected.clauses.size()<<'\n';
  for(const auto& clause:expected.clauses) {for(int32_t x:clause)dimacs<<x<<' ';dimacs<<"0\n";}
  FILE* input=std::tmpfile(); assert(input);
  auto text=dimacs.str(); assert(std::fwrite(text.data(),1,text.size(),input)==text.size());
  std::rewind(input); Kernel kernel; assert(!parse_dimacs(input,kernel)); std::fclose(input);
  DirectCspGuardedCoverActionLimits limits{100,1000,1000000,10,10,10,10,10,10,10000,10,100,100000,100,10,1000000};
  FILE *records = std::tmpfile(); assert(records);
  kernel.structural_provenance = records;
  DirectCspGuardedCoverActionSession session;
  const Image initial_image = image(kernel);
  const auto initial_sequence = kernel.structural_action_sequence;
  const long initial_records = std::ftell(records);
  volatile sig_atomic_t stop = 1;
  kernel.stop = &stop;
  auto interrupted = direct_csp_guarded_cover_action_apply(kernel, session, limits, selection);
  assert(interrupted.status == DirectCspSupportActionStatus::Interrupted);
  assert(image(kernel) == initial_image && !session.retained());
  kernel.stop = nullptr;
  DirectCspGuardedCoverLiveRequest request;
  request.selection=selection; request.bounds=bounds; request.maximum_recognition_work=1000000;
  request.maximum_proof_work=10000; request.maximum_proof_additions=10;
  request.maximum_proof_literal_occurrences=100; request.maximum_journal_bytes=100000;
  request.maximum_external_extent=100; request.action_id=1; request.semantic_id=1;
  request.records=records;
  DirectCspGuardedCoverOwnerSnapshot owner;
  auto capture=direct_csp_guarded_cover_live_capture_owner(kernel,request,owner);
  assert(capture.status==DirectCspSupportLiveStatus::Applied);
  assert(image(kernel)==initial_image && std::ftell(records)==initial_records);
  request.retained_owner=&owner;
  auto preflight=direct_csp_guarded_cover_live_preflight(kernel,request);
  assert(preflight.status==DirectCspSupportLiveStatus::Applied && !preflight.emitted_additions);
  assert(image(kernel)==initial_image && std::ftell(records)==initial_records);
  auto bounded=request; bounded.maximum_journal_bytes=1;
  auto no_journal=direct_csp_guarded_cover_live_apply(kernel,bounded);
  assert(no_journal.status==DirectCspSupportLiveStatus::ResourceExhausted);
  assert(image(kernel)==initial_image && std::ftell(records)==initial_records);
  bounded=request; bounded.maximum_recognition_work=1;
  auto no_work=direct_csp_guarded_cover_live_apply(kernel,bounded);
  assert(no_work.status==DirectCspSupportLiveStatus::ResourceExhausted && no_work.recognition_limit_exhausted);
  assert(image(kernel)==initial_image && std::ftell(records)==initial_records);
  bounded=request; bounded.maximum_proof_work=1;
  auto no_proof=direct_csp_guarded_cover_live_apply(kernel,bounded);
  assert(no_proof.status==DirectCspSupportLiveStatus::ResourceExhausted);
  assert(image(kernel)==initial_image && std::ftell(records)==initial_records);
  auto original_native_id=owner.source[0].native_proof_id;
  ++owner.source[0].native_proof_id;
  auto stale=direct_csp_guarded_cover_live_apply(kernel,request);
  assert(stale.status==DirectCspSupportLiveStatus::Stale);
  assert(image(kernel)==initial_image && std::ftell(records)==initial_records);
  owner.source[0].native_proof_id=original_native_id;
  auto malformed=selection; malformed.selected_values[0]=99;
  auto invalid=direct_csp_guarded_cover_action_apply(kernel,session,limits,malformed);
  assert(invalid.status==DirectCspSupportActionStatus::Invalid);
  assert(image(kernel)==initial_image && std::ftell(records)==initial_records);
  auto tiny=limits; tiny.maximum_session_work=1;
  auto decline=direct_csp_guarded_cover_action_apply(kernel,session,tiny,selection);
  assert(decline.status==DirectCspSupportActionStatus::ResourceExhausted);
  assert(image(kernel)==initial_image && !session.retained());
  assert(kernel.structural_action_sequence==initial_sequence && std::ftell(records)==initial_records);
  auto applied=direct_csp_guarded_cover_action_apply(kernel,session,limits,selection);
  assert(applied.status==DirectCspSupportActionStatus::Applied);
  assert(applied.live.emitted_additions==1 && applied.semantic_variables.size()==3);
  assert(session.committed_keys()==1 && session.retained_owners()==1);
  const Image committed_image = image(kernel);
  const auto committed_sequence = kernel.structural_action_sequence;
  const long committed_records = std::ftell(records);
  auto duplicate=direct_csp_guarded_cover_action_apply(kernel,session,limits,selection);
  assert(duplicate.status==DirectCspSupportActionStatus::AlreadyPublished);
  assert(image(kernel)==committed_image);
  assert(kernel.structural_action_sequence==committed_sequence && std::ftell(records)==committed_records);
  selection.selected_values[0]=1;
  auto second=direct_csp_guarded_cover_action_apply(kernel,session,limits,selection);
  assert(second.status==DirectCspSupportActionStatus::Applied);
  assert(session.committed_keys()==2 && session.retained_owners()==1);
  kernel.structural_provenance = nullptr; std::fclose(records);
  if (argc == 9) {
    // Original input carries the tuple assumptions. The native adapter sees
    // only its original subset, then emits two nonempty theory lemmas before
    // the ordinary clausal refutation consumes the first one.
    std::ostringstream original;
    original << "p cnf 12 " << expected.clauses.size()+3 << '\n';
    for (const auto &clause : expected.clauses) {
      for (int32_t literal : clause) original << literal << ' ';
      original << "0\n";
    }
    original << "1 0\n4 0\n11 0\n";
    FILE *cnf = std::fopen(argv[1], "w"); assert(cnf);
    auto original_text = original.str();
    assert(std::fwrite(original_text.data(),1,original_text.size(),cnf)==original_text.size());
    std::fclose(cnf);
    for (unsigned format = 0; format != 4; ++format) {
      FILE *subset = std::tmpfile(); assert(subset);
      assert(std::fwrite(text.data(),1,text.size(),subset)==text.size());
      std::rewind(subset); Kernel proof_kernel;
      assert(!parse_dimacs(subset,proof_kernel)); std::fclose(subset);
      proof_kernel.next_clause_id = expected.clauses.size()+3;
      Proof proof;
      assert(proof.open(argv[format < 2 ? 2+format : 4+format], (format & 1) ? ProofFormat::DratAscii : ProofFormat::DsrAscii));
      proof.bind_variable_map(&proof_kernel.internal_to_external);
      proof.bind_external_variable_map(&proof_kernel.external_to_internal);
      proof_kernel.proof = &proof;
      FILE *journal = std::tmpfile(); assert(journal);
      proof_kernel.structural_provenance = journal;
      DirectCspGuardedCoverActionSession proof_session;
      selection.selected_values = {0,0,0};
      auto first = direct_csp_guarded_cover_action_apply(proof_kernel,proof_session,limits,selection);
      assert(first.status == DirectCspSupportActionStatus::Applied);
      selection.selected_values[0] = 1;
      auto next = direct_csp_guarded_cover_action_apply(proof_kernel,proof_session,limits,selection);
      assert(next.status == DirectCspSupportActionStatus::Applied);
      if (format < 2) assert(proof.add_external_rup(++proof_kernel.next_clause_id,nullptr,0));
      proof.flush(); proof_kernel.proof=nullptr;
      proof_kernel.structural_provenance=nullptr; std::fclose(journal);
    }
    FILE *sat = std::fopen(argv[4], "w"); assert(sat);
    assert(std::fwrite(text.data(),1,text.size(),sat)==text.size()); std::fclose(sat);
    // A checked original-CNF model with key 2, value 4 false, q false, b9.
    FILE *model = std::fopen(argv[5], "w"); assert(model);
    std::fputs("s SATISFIABLE\nv -1 2 -3 -4 5 -6 -7 -8 9 -10 11 -12 0\n",model);
    std::fclose(model);
    FILE *bad = std::fopen(argv[8], "w"); assert(bad);
    std::fputs("-11 -5 -1 0\n",bad); std::fclose(bad);
  }
}
