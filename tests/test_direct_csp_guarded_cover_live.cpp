#include "unit_test.hpp"
#include "dimacs_text.hpp"
#include "kernel_image.hpp"
#include "test_io.hpp"
#include "rules/semantic_extension/direct_csp_guarded_cover_action.hpp"
#include "proof/proof.hpp"
#include <csignal>
int main(int argc, char **argv) {
  DirectCspGuardedCoverActionSelection selection{
      {{1,2,3},{4,5},{11,12}}, {0,0,0}, {8,9,10},
      {{6,{4,7}}}, {{8,0,0,0},{9,0,1,0},{10,0,2,0}}};
  direct_csp_guarded_cover::Bounds bounds{100,1000,10,10,10,10,10,10};
  auto expected=direct_csp_guarded_cover::expected_clauses(selection,bounds,[](uint64_t){return true;});
  const std::string text=dimacs_text(12,expected.clauses);
  Kernel kernel; parse_dimacs_text(text,kernel);
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
  if (argc == 7) {
    // CTest direct_csp_guarded_cover_proof_model: the checker CNF adds the
    // tuple assumptions; the action sees only the original subset and emits
    // two theory lemmas before the clausal refutation uses the first.
    auto original = expected.clauses;
    original.insert(original.end(), {{1}, {4}, {11}});
    write_file(argv[1], dimacs_text(12, original));
    for (const bool refute : {true, false}) {
      Kernel proof_kernel; parse_dimacs_text(text,proof_kernel);
      proof_kernel.next_clause_id = expected.clauses.size()+3;
      Proof proof;
      assert(proof.open(refute ? argv[2] : argv[5], ProofFormat::DsrAscii));
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
      if (refute) assert(proof.add_external_rup(++proof_kernel.next_clause_id,nullptr,0));
      proof.flush(); proof_kernel.proof=nullptr;
      proof_kernel.structural_provenance=nullptr; std::fclose(journal);
    }
    write_file(argv[3], text);
    // A checked original-CNF model with key 2, value 4 false, q false, b9.
    write_file(argv[4], "s SATISFIABLE\nv -1 2 -3 -4 5 -6 -7 -8 9 -10 11 -12 0\n");
    write_file(argv[6], "-11 -5 -1 0\n");
  }
}
