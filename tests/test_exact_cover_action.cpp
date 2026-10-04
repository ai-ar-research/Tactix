#undef NDEBUG
#include "frontend/parse.hpp"
#include "rules/semantic_extension/exact_cover_action.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace tactix;

static std::string exact_cover_cnf() {
  std::ostringstream out;
  out << "p cnf 9 15\n";
  for (int block = 0; block < 3; ++block)
    for (int first = 0; first < 3; ++first)
      for (int second = first + 1; second < 3; ++second)
        out << -(3 * block + first + 1) << ' '
            << -(3 * block + second + 1) << " 0\n";
  for (int position = 0; position < 3; ++position) {
    for (int block = 0; block < 3; ++block)
      out << 3 * block + position + 1 << ' ';
    out << "0\n";
  }
  for (int diagonal = 0; diagonal < 3; ++diagonal) {
    for (int block = 0; block < 3; ++block)
      out << 3 * block + (block + diagonal) % 3 + 1 << ' ';
    out << "0\n";
  }
  return out.str();
}

static Kernel parse() {
  const std::string text = exact_cover_cnf();
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  Kernel kernel;
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  return kernel;
}

static strategy::ExactCoverExtensionParameters parameters() {
  strategy::ExactCoverExtensionParameters result;
  result.arity = 3;
  result.maximum_items = 100;
  result.maximum_source_clauses = 100;
  result.maximum_source_literals = 1000;
  result.maximum_recognition_work = 100000;
  result.maximum_premise_references = 1000;
  result.maximum_definitions = 10;
  result.maximum_additions = 100;
  result.maximum_literal_occurrences = 1000;
  result.maximum_extent_growth = 10;
  result.maximum_external_extent = 1000;
  result.maximum_journal_bytes = 100000;
  return result;
}

static void remove_definitions(Kernel &kernel) {
  uint32_t binaries = 0;
  for (int output : {10, 11, 12})
    for (int input = 1; input <= 9; ++input) {
      const Lit first = kernel_internalize(kernel, output);
      const Lit second = kernel_internalize(kernel, -input);
      if (kernel_binary_live(kernel, first, second)) {
        assert(kernel_delete_binary(kernel, first, second));
        ++binaries;
      }
    }
  uint32_t large = 0;
  for (Ref ref = 0; ref < kernel.arena.size();) {
    Clause *clause = clause_at(kernel.arena, ref);
    ref += clause_actual_words(clause);
    if (clause->garbage()) continue;
    if (std::any_of(clause->begin(), clause->end(), [&](Lit literal) {
          const int32_t external = kernel_externalize(kernel, literal);
          return external > 9 || external < -9;
        })) {
      kernel_delete_clause(kernel, clause);
      ++large;
    }
  }
  assert(binaries == 9 && large == 3);
}

static void declines_do_not_consume_identity() {
  Kernel kernel = parse();
  ExactCoverExtensionActionSession session;
  kernel.exact_cover_extension_action_session = &session;
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  kernel.structural_action_sequence = 40;

  // Eligibility is state-only: disabled parameters do not hide the action.
  assert(exact_cover_extension_action_guard(kernel));
  const auto disabled = exact_cover_extension_action_apply(kernel, session);
  assert(disabled.live.status == ExactCoverLiveStatus::Invalid);
  assert(disabled.action_id == 0 && kernel.structural_action_sequence == 40);
  assert(std::ftell(records) == 0);

  kernel.exact_cover_extension_parameters = parameters();
  kernel.exact_cover_extension_parameters.maximum_journal_bytes = 1;
  const auto exhausted = exact_cover_extension_action_apply(kernel, session);
  assert(exhausted.live.status == ExactCoverLiveStatus::ResourceExhausted);
  assert(exhausted.live.journal_bytes == 0 && exhausted.action_id == 0);
  assert(exhausted.live.recognition_work > 0);
  assert(kernel.structural_action_sequence == 40);
  assert(session.live_session.committed_keys() == 0);
  assert(std::ftell(records) == 0);

  kernel.exact_cover_extension_parameters = parameters();
  kernel.structural_action_sequence = std::numeric_limits<uint64_t>::max();
  const auto overflow = exact_cover_extension_action_apply(kernel, session);
  assert(overflow.live.status == ExactCoverLiveStatus::Inapplicable);
  assert(overflow.action_id == 0 &&
         kernel.structural_action_sequence ==
             std::numeric_limits<uint64_t>::max());
  assert(session.live_session.committed_keys() == 0);
  assert(std::ftell(records) == 0);
  std::fclose(records);
}

static void success_stages_candidate_and_commits_identity() {
  Kernel kernel = parse();
  kernel.exact_cover_extension_parameters = parameters();
  ExactCoverExtensionActionSession session;
  kernel.exact_cover_extension_action_session = &session;
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  kernel.structural_action_sequence = 40;

  strategy::begin_semantic_variable_candidate_action(kernel);
  const auto applied = exact_cover_extension_action_apply(kernel, session);
  assert(applied.live.status == ExactCoverLiveStatus::Applied);
  assert(applied.live.result == ApplyResult::Ok && applied.action_id == 41);
  assert(applied.live.recognition_work > 0 &&
         applied.live.revalidation_work > 0);
  assert(applied.live.additions == 12 &&
         applied.live.literal_occurrences == 30 &&
         applied.live.journal_bytes > 0);
  assert(applied.semantic_candidate_staged);
  assert(kernel.structural_action_sequence == 41);
  assert(session.live_session.committed_keys() == 3);
  assert(kernel.semantic_variable_candidate_pending);
  assert(kernel.stats.exact_cover_extension_attempts == 1 &&
         kernel.stats.exact_cover_extension_applied == 1 &&
         kernel.stats.exact_cover_extension_definitions == 3 &&
         kernel.stats.exact_cover_extension_candidate_variables == 3);

  strategy::finish_semantic_variable_candidate_action(kernel,
                                                       ApplyResult::Ok);
  const auto candidates = strategy::semantic_variable_candidates(kernel);
  assert(candidates.size() == 1);
  const auto *candidate =
      strategy::semantic_variable_candidate(kernel, candidates.front().id);
  assert(candidate && candidate->action_id == 41);
  assert(candidate->producer ==
         strategy::SemanticVariableProducer::ExactCover);
  assert(candidate->existing_variables == 0 &&
         candidate->introduced_variables == 3);
  assert(candidate->variables.size() == 3);
  for (uint32_t i = 0; i < candidate->variables.size(); ++i) {
    assert(candidate->variables[i].external == 10 + i);
    assert(candidate->variables[i].role ==
           strategy::SemanticVariableRole::IntroducedTheory);
    assert(candidate->variables[i].priority == 1);
  }

  remove_definitions(kernel);
  const auto mapping = kernel.external_to_internal;
  const uint64_t native_id = kernel.next_clause_id;
  const size_t arena = kernel.arena.size();
  const long journal = std::ftell(records);
  const auto duplicate = exact_cover_extension_action_apply(kernel, session);
  assert(duplicate.live.status == ExactCoverLiveStatus::AlreadyPublished);
  assert(duplicate.live.result == ApplyResult::NoOp &&
         duplicate.action_id == 0);
  assert(kernel.structural_action_sequence == 41);
  assert(session.live_session.committed_keys() == 3);
  assert(kernel.external_to_internal == mapping &&
         kernel.next_clause_id == native_id && kernel.arena.size() == arena);
  assert(std::ftell(records) == journal);
  assert(kernel.stats.exact_cover_extension_attempts == 2 &&
         kernel.stats.exact_cover_extension_already_published == 1);
  std::fclose(records);
}

static void state_guard_rejects_assignments() {
  Kernel kernel = parse();
  ExactCoverExtensionActionSession session;
  kernel.exact_cover_extension_action_session = &session;
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  assert(exact_cover_extension_action_guard(kernel));
  kernel_assign_decision(kernel, kernel_internalize(kernel, 1));
  assert(!exact_cover_extension_action_guard(kernel));
  std::fclose(records);
}

int main() {
  declines_do_not_consume_identity();
  success_stages_candidate_and_commits_identity();
  state_guard_rejects_assignments();
  std::puts("test_exact_cover_action: passed");
}
