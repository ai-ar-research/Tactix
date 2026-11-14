#include "unit_test.hpp"
#include "rules/semantic_extension/independent_set_cover_action.hpp"

#include "independent_set_test_support.hpp"
#include "dimacs_text.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/semantic_variable_advice.hpp"

namespace {

void load(Kernel &kernel) {
  const auto encoding = independent_set_encoding(4, 2, {{1, 2}});
  parse_dimacs_text(dimacs_text(encoding.variables, encoding.clauses), kernel,
                    ParsedRoot::EmptyTrail);
}

void configure(Kernel &kernel, FILE *records) {
  auto &parameters = kernel.independent_set_cover_parameters;
  parameters.maximum_clauses = 10000;
  parameters.maximum_literals = 100000;
  parameters.maximum_recognition_work = 10000000;
  parameters.maximum_proof_work = 10000000;
  parameters.maximum_proof_additions = 100000;
  parameters.maximum_proof_deletions = 100000;
  parameters.maximum_proof_literal_occurrences = 10000000;
  parameters.maximum_journal_bytes = 10000000;
  parameters.maximum_vertices = 64;
  parameters.maximum_target = 64;
  kernel.structural_provenance = records;
}

void publish_staged_candidate(Kernel &kernel, uint64_t action_id) {
  assert(kernel.semantic_variable_candidate_pending);
  assert(kernel.staged_semantic_variable_candidate.action_id == action_id);
  assert(kernel.staged_semantic_variable_candidate.producer ==
         strategy::SemanticVariableProducer::IndependentSet);
  assert(kernel.staged_semantic_variable_candidate.variables.size() == 4);
  for (const auto &variable :
       kernel.staged_semantic_variable_candidate.variables) {
    assert(variable.role == strategy::SemanticVariableRole::ExistingTheory);
    assert(variable.priority == 1);
  }
  ++kernel.strategy_epoch; // the eventual registry dispatch owns this edge
  strategy::finish_semantic_variable_candidate_action(kernel,
                                                       ApplyResult::Ok);
  const auto candidates = strategy::semantic_variable_candidates(kernel);
  assert(candidates.size() == 1 && candidates[0].action_id == action_id &&
         candidates[0].existing_variables == 4 &&
         !candidates[0].introduced_variables);
}

TEST(exercise) {
  Kernel kernel;
  load(kernel);
  FILE *records = std::tmpfile();
  assert(records);
  configure(kernel, records);
  IndependentSetCoverActionSession session;
  kernel.independent_set_cover_action_session = &session;
  assert(independent_set_cover_action_guard(kernel));

  const long initial_records = std::ftell(records);
  const size_t initial_arena = kernel.arena.size();
  assert(!kernel.structural_action_sequence);
  auto declined =
      independent_set_cover_action_apply(kernel, session, {2, 1, 3});
  assert(declined.status == IndependentSetLiveStatus::Invalid);
  assert(!declined.action_id);
  assert(!session.retained());
  assert(!kernel.structural_action_sequence);
  assert(!kernel.semantic_variable_candidate_pending);
  assert(kernel.arena.size() == initial_arena);
  assert(std::ftell(records) == initial_records);

  kernel.structural_provenance = nullptr;
  assert(!independent_set_cover_action_guard(kernel));
  declined = independent_set_cover_action_apply(kernel, session, {1, 2, 3});
  assert(declined.status == IndependentSetLiveStatus::Inapplicable &&
         !declined.action_id && !session.retained());
  kernel.structural_provenance = records;

  const uint64_t recognition_limit =
      kernel.independent_set_cover_parameters.maximum_recognition_work;
  kernel.independent_set_cover_parameters.maximum_recognition_work = 2;
  declined = independent_set_cover_action_apply(kernel, session, {1, 2, 3});
  assert(declined.status == IndependentSetLiveStatus::ResourceExhausted &&
         declined.effort.recognition_work <= 2 && !session.retained() &&
         !kernel.structural_action_sequence &&
         kernel.arena.size() == initial_arena &&
         std::ftell(records) == initial_records);
  kernel.independent_set_cover_parameters.maximum_recognition_work =
      recognition_limit;

  const auto first =
      independent_set_cover_action_apply(kernel, session, {1, 2, 3});
  assert(first.status == IndependentSetLiveStatus::Applied &&
         first.result == ApplyResult::Ok && first.action_id == 1 &&
         first.semantic_id == 1 && first.owner_retained &&
         first.candidate_staged && first.published_clause_id &&
         first.effort.recognition_work ==
             first.effort.source_scan_work +
                 first.effort.theory_recognition_work &&
         session.retained() && session.belongs_to(kernel) &&
         kernel.structural_action_sequence == 1);
  assert(kernel.stats.independent_set_cover_attempts == 4 &&
         kernel.stats.independent_set_cover_invalid == 1 &&
         kernel.stats.independent_set_cover_inapplicable == 1 &&
         kernel.stats.independent_set_cover_resource_exhausted == 1 &&
         kernel.stats.independent_set_cover_applied == 1 &&
         kernel.stats.independent_set_cover_candidate_variables == 4 &&
         kernel.stats.independent_set_cover_source_clauses ==
             first.effort.source_clauses + declined.effort.source_clauses);
  const uint64_t captured_work =
      session.snapshot.capture_source_scan_work +
      session.snapshot.capture_theory_recognition_work;
  assert(captured_work && first.effort.recognition_work > captured_work);
  publish_staged_candidate(kernel, 1);

  // Retained owner admission ignores later unrelated clauses but charges no
  // second copy of the initial capture.
  const Lit unrelated[] = {kernel_internalize(kernel, 1),
                           kernel_internalize(kernel, 2),
                           kernel_internalize(kernel, 3),
                           kernel_internalize(kernel, 4)};
  kernel_add_large_clause(kernel, unrelated, 4, true, 4,
                          ++kernel.next_clause_id);
  const auto second =
      independent_set_cover_action_apply(kernel, session, {1, 2, 4});
  assert(second.status == IndependentSetLiveStatus::Applied &&
         second.result == ApplyResult::Ok && second.action_id == 2 &&
         second.candidate_staged &&
         second.effort.recognition_work < first.effort.recognition_work &&
         kernel.structural_action_sequence == 2);
  publish_staged_candidate(kernel, 2);

  const long before_duplicate = std::ftell(records);
  const size_t arena_before_duplicate = kernel.arena.size();
  declined =
      independent_set_cover_action_apply(kernel, session, {1, 2, 4});
  assert(declined.status == IndependentSetLiveStatus::Duplicate &&
         !declined.action_id && kernel.structural_action_sequence == 2 &&
         !kernel.semantic_variable_candidate_pending &&
         kernel.arena.size() == arena_before_duplicate &&
         std::ftell(records) == before_duplicate);

  assert(kernel_delete_binary(kernel, kernel_internalize(kernel, -1),
                              kernel_internalize(kernel, -2)));
  const long before_stale = std::ftell(records);
  declined =
      independent_set_cover_action_apply(kernel, session, {1, 3, 4});
  assert(declined.status == IndependentSetLiveStatus::Stale &&
         !declined.action_id && session.retained() &&
         kernel.structural_action_sequence == 2 &&
         std::ftell(records) == before_stale);

  Kernel other;
  load(other);
  FILE *other_records = std::tmpfile();
  assert(other_records);
  configure(other, other_records);
  other.independent_set_cover_action_session = &session;
  declined = independent_set_cover_action_apply(other, session, {1, 2, 3});
  assert(declined.status == IndependentSetLiveStatus::Stale &&
         !other.structural_action_sequence && session.belongs_to(kernel));
  other.structural_provenance = nullptr;
  std::fclose(other_records);

  kernel.structural_provenance = nullptr;
  std::fclose(records);
}

} // namespace
