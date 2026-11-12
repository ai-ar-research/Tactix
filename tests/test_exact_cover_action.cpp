#undef NDEBUG
#include "exact_cover_test_support.hpp"
#include "rules/semantic_extension/exact_cover_action.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <algorithm>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

using namespace tactix;
using Parameters = strategy::ExactCoverExtensionParameters;

namespace {

Parameters parameters() {
  Parameters result;
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

// The fixture with an attached session, a journal and structural sequence 40.
struct Fixture {
  Kernel kernel;
  ExactCoverExtensionActionSession session;
  FILE *records = std::tmpfile();

  explicit Fixture(bool extra = false) : kernel(exact_cover_kernel(extra)) {
    assert(records);
    kernel.exact_cover_extension_action_session = &session;
    kernel.exact_cover_extension_parameters = parameters();
    kernel.structural_provenance = records;
    kernel.structural_action_sequence = 40;
  }
  ~Fixture() {
    kernel.structural_provenance = nullptr;
    std::fclose(records);
  }
  ExactCoverExtensionActionResult apply() {
    return exact_cover_extension_action_apply(kernel, session);
  }
  // A decline with `expected` changes neither state, journal nor identity.
  ExactCoverLiveResult declines(ExactCoverLiveStatus expected) {
    const ExactCoverUnchanged unchanged{kernel, records};
    const uint64_t sequence = kernel.structural_action_sequence;
    const auto result = apply();
    assert(result.live.status == expected &&
           result.live.result == ApplyResult::NoOp && !result.action_id);
    assert(!result.semantic_candidate_staged &&
           kernel.structural_action_sequence == sequence &&
           session.live_session.committed_keys() == 0);
    unchanged.check();
    return result.live;
  }
};

void remove_definitions(Kernel &kernel) {
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

void success_stages_candidate_and_commits_identity() {
  Fixture f;
  strategy::begin_semantic_variable_candidate_action(f.kernel);
  const auto applied = f.apply();
  const ExactCoverLiveResult &live = applied.live;
  assert(live.status == ExactCoverLiveStatus::Applied);
  assert(live.result == ApplyResult::Ok && applied.action_id == 41);
  assert(live.source_clauses == 15 && live.source_literals == 36);
  assert(live.definitions == 3 && live.additions == 12);
  assert(live.literal_occurrences == 30 && live.extent_growth == 3);
  assert(live.recognition_work > 0 && live.revalidation_work > 0);
  assert(live.introduced_theory_variables ==
         std::vector<uint32_t>({10, 11, 12}));
  assert(f.kernel.nvars == 12 && kernel_num_output_vars(f.kernel) == 9);
  assert(applied.semantic_candidate_staged);
  assert(f.kernel.structural_action_sequence == 41);
  assert(f.session.live_session.committed_keys() == 3);
  assert(f.kernel.stats.exact_cover_extension_attempts == 1 &&
         f.kernel.stats.exact_cover_extension_applied == 1 &&
         f.kernel.stats.exact_cover_extension_definitions == 3 &&
         f.kernel.stats.exact_cover_extension_candidate_variables == 3);
  const std::string journal = stream_contents(f.records);
  assert(live.journal_bytes == journal.size());
  // Proofless kernels name binaries by literals, while large source
  // clauses retain their native identities. Both cases remain explicit.
  assert(journal.find("\"native_proof_id\":0") != std::string::npos);
  assert(journal.find("\"native_proof_id\":10") != std::string::npos);

  strategy::finish_semantic_variable_candidate_action(f.kernel,
                                                       ApplyResult::Ok);
  const auto candidates = strategy::semantic_variable_candidates(f.kernel);
  assert(candidates.size() == 1);
  const auto *candidate =
      strategy::semantic_variable_candidate(f.kernel, candidates.front().id);
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

  // Every occupancy definition is live: an item implies its block output.
  kernel_assign_decision(f.kernel, kernel_internalize(f.kernel, 1));
  assert(propagate_service(f.kernel) == ApplyResult::Ok);
  assert(f.kernel.value(kernel_internalize(f.kernel, 10)) > 0);
  kernel_backtrack(f.kernel, 0);

  // The definitions make the source unrecognizable; once they are gone the
  // committed keys still decline the same batch.
  const ExactCoverUnchanged repeat{f.kernel, f.records};
  assert(f.apply().live.status == ExactCoverLiveStatus::NoStructure);
  repeat.check();
  remove_definitions(f.kernel);
  const ExactCoverUnchanged unchanged{f.kernel, f.records};
  const auto duplicate = f.apply();
  assert(duplicate.live.status == ExactCoverLiveStatus::AlreadyPublished);
  assert(duplicate.live.result == ApplyResult::NoOp &&
         duplicate.action_id == 0);
  assert(f.kernel.structural_action_sequence == 41);
  assert(f.session.live_session.committed_keys() == 3);
  unchanged.check();
  assert(f.kernel.stats.exact_cover_extension_attempts == 3 &&
         f.kernel.stats.exact_cover_extension_already_published == 1);
}

void final_revalidation_obeys_recognition_limit() {
  uint64_t complete_work = 0;
  {
    Fixture f;
    const auto applied = f.apply();
    assert(applied.live.status == ExactCoverLiveStatus::Applied);
    complete_work = applied.live.recognition_work;
  }
  assert(complete_work > 1);
  Fixture f;
  f.kernel.exact_cover_extension_parameters.maximum_recognition_work =
      complete_work - 1;
  assert(f.declines(ExactCoverLiveStatus::ResourceExhausted)
             .revalidation_work > 0);
}

void declines_do_not_consume_identity() {
  {
    // Eligibility is state-only: disabled parameters do not hide the action.
    Fixture f;
    f.kernel.exact_cover_extension_parameters = {};
    assert(exact_cover_extension_action_guard(f.kernel));
    f.declines(ExactCoverLiveStatus::Invalid);
  }
  {
    Fixture f;
    f.kernel.structural_action_sequence = std::numeric_limits<uint64_t>::max();
    f.declines(ExactCoverLiveStatus::Inapplicable);
  }
  {
    Fixture f(true);
    f.declines(ExactCoverLiveStatus::NoStructure);
  }
  {
    Fixture f;
    volatile sig_atomic_t stop = 1;
    f.kernel.stop = &stop;
    f.declines(ExactCoverLiveStatus::Interrupted);
  }
  // Each bound one below what the fixture needs.
  const std::vector<void (*)(Parameters &)> bounds = {
      [](Parameters &p) { p.maximum_recognition_work = 1; },
      [](Parameters &p) { p.maximum_source_clauses = 14; },
      [](Parameters &p) { p.maximum_source_literals = 35; },
      [](Parameters &p) { p.maximum_items = 8; },
      [](Parameters &p) { p.maximum_premise_references = 26; },
      [](Parameters &p) { p.maximum_definitions = 2; },
      [](Parameters &p) { p.maximum_additions = 11; },
      [](Parameters &p) { p.maximum_literal_occurrences = 29; },
      [](Parameters &p) { p.maximum_extent_growth = 2; },
      [](Parameters &p) { p.maximum_external_extent = 11; },
      [](Parameters &p) { p.maximum_journal_bytes = 1; },
  };
  for (size_t i = 0; i < bounds.size(); ++i) {
    Fixture f;
    bounds[i](f.kernel.exact_cover_extension_parameters);
    const auto live = f.declines(ExactCoverLiveStatus::ResourceExhausted);
    if (i == 0) assert(live.recognition_work > 0);
  }
}

void state_guard_rejects_assignments() {
  Fixture f;
  assert(exact_cover_extension_action_guard(f.kernel));
  kernel_assign_decision(f.kernel, kernel_internalize(f.kernel, 1));
  assert(!exact_cover_extension_action_guard(f.kernel));
}

} // namespace

int main() {
  success_stages_candidate_and_commits_identity();
  final_revalidation_obeys_recognition_limit();
  declines_do_not_consume_identity();
  state_guard_rejects_assignments();
  std::puts("test_exact_cover_action: passed");
}
