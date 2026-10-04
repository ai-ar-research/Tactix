#undef NDEBUG
#include "rules/semantic_extension/independent_set_cover_action.hpp"

#include "frontend/parse.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace tactix;

namespace {

struct Encoding {
  uint32_t vertices = 0;
  uint32_t target = 0;
  uint32_t variables = 0;
  std::vector<std::vector<int32_t>> layers;
  std::vector<std::vector<int32_t>> clauses;
};

Encoding make_encoding() {
  Encoding result;
  result.vertices = 4;
  result.target = 2;
  result.layers.resize(5);
  int32_t next = 5;
  for (uint32_t layer = 1; layer <= result.vertices; ++layer)
    for (uint32_t state = 0; state <= std::min(layer, result.target); ++state)
      result.layers[layer].push_back(next++);
  result.variables = next - 1;
  const auto add = [&](std::initializer_list<int32_t> literals) {
    result.clauses.emplace_back(literals);
  };
  add({-1, -2});
  add({1, result.layers[1][0]});
  add({-1, result.layers[1][1]});
  for (uint32_t layer = 2; layer <= result.vertices; ++layer) {
    const auto &previous = result.layers[layer - 1];
    const auto &current = result.layers[layer];
    for (uint32_t state = 0; state < previous.size(); ++state) {
      add({-previous[state], static_cast<int32_t>(layer), current[state]});
      add({-previous[state], -static_cast<int32_t>(layer),
           current[std::min(state + 1, result.target)]});
    }
    for (uint32_t first = 0; first < current.size(); ++first)
      for (uint32_t second = first + 1; second < current.size(); ++second)
        add({-current[first], -current[second]});
  }
  const auto &terminal = result.layers[result.vertices];
  for (uint32_t state = 0; state < result.target; ++state)
    add({-terminal[state], terminal[result.target]});
  result.clauses.push_back(terminal);
  return result;
}

void load(Kernel &kernel, const Encoding &encoding) {
  std::ostringstream text;
  text << "p cnf " << encoding.variables << ' ' << encoding.clauses.size()
       << '\n';
  for (const auto &clause : encoding.clauses) {
    for (int32_t literal : clause)
      text << literal << ' ';
    text << "0\n";
  }
  FILE *input = std::tmpfile();
  assert(input);
  const std::string dimacs = text.str();
  assert(std::fwrite(dimacs.data(), 1, dimacs.size(), input) ==
         dimacs.size());
  std::rewind(input);
  assert(parse_dimacs(input, kernel) == nullptr);
  assert(!std::fclose(input));
  assert(kernel.trail.empty() && kernel_propagation_fixpoint(kernel));
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

void exercise() {
  const Encoding encoding = make_encoding();
  Kernel kernel;
  load(kernel, encoding);
  FILE *records = std::tmpfile();
  assert(records);
  configure(kernel, records);
  IndependentSetCoverActionSession session;
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
  load(other, encoding);
  FILE *other_records = std::tmpfile();
  assert(other_records);
  configure(other, other_records);
  declined = independent_set_cover_action_apply(other, session, {1, 2, 3});
  assert(declined.status == IndependentSetLiveStatus::Stale &&
         !other.structural_action_sequence && session.belongs_to(kernel));
  other.structural_provenance = nullptr;
  std::fclose(other_records);

  kernel.structural_provenance = nullptr;
  std::fclose(records);
}

} // namespace

int main() {
  exercise();
  std::puts("test_independent_set_cover_action OK");
  return 0;
}
