#undef NDEBUG
#include "rules/semantic_extension/affine_basis_action.hpp"

#include "strategy/semantic_variable_advice.hpp"

#include <cassert>
#include <algorithm>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

using namespace tactix;
using namespace tactix::affine_semantic_extension;

static void add_equation(Kernel &kernel, const std::vector<External> &variables,
                         bool rhs) {
  for (const auto &external : clausal_form(variables, rhs)) {
    std::vector<Lit> internal;
    for (External literal : external) internal.push_back(internalize(literal));
    if (internal.size() == 2) {
      kernel_add_binary_clause(kernel, internal[0], internal[1],
                               ++kernel.next_clause_id);
    } else {
      assert(internal.size() > 2);
      kernel_add_large_clause(kernel, internal.data(), internal.size(), false,
                              0, ++kernel.next_clause_id);
    }
  }
}

static Kernel direct_fixture() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 4);
  add_equation(kernel, {1, 2, 3}, false);
  add_equation(kernel, {1, 2, 4}, true);
  return kernel;
}

static Kernel extension_fixture() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 6);
  add_equation(kernel, {1, 2, 3, 4, 5}, false);
  add_equation(kernel, {1, 2, 3, 4, 6}, true);
  return kernel;
}

static AffineBasisActionLimits limits() {
  AffineBasisActionLimits result;
  result.image = {10000, 1000, 10000, 10000000, 8, 32, 32, 4};
  result.basis = {10000000, 32, 32, 10000, 10000, 3};
  result.maximum_snapshot_literals = 100000;
  result.maximum_snapshot_work = 1000000;
  result.maximum_proof_additions = 1000000;
  result.maximum_proof_deletions = 1000000;
  result.maximum_journal_bytes = 10000000;
  result.maximum_external_extent = 100000;
  result.maximum_session_keys = 10000;
  result.maximum_session_work = 1000000;
  return result;
}

static void attach(Kernel &kernel, FILE *records) {
  assert(records);
  kernel.structural_provenance = records;
  kernel.structural_action_sequence = 40;
}

static std::string contents(FILE *file) {
  std::fflush(file);
  const long end = std::ftell(file);
  assert(end >= 0);
  std::rewind(file);
  std::string output(static_cast<size_t>(end), '\0');
  assert(!end || std::fread(output.data(), 1, output.size(), file) ==
                     output.size());
  return output;
}

static void success_preserves_identity_and_metadata() {
  Kernel kernel = extension_fixture();
  FILE *records = std::tmpfile();
  attach(kernel, records);
  AffineBasisActionSession session;

  const auto applied = affine_basis_action_apply(kernel, session, limits());
  assert(applied.status == AffineBasisActionStatus::Applied);
  assert(applied.result == ApplyResult::Ok);
  assert(applied.proposed_action_id == 41 &&
         applied.committed_action_id == 41 && applied.semantic_id == 41);
  assert(kernel.structural_action_sequence == 41);
  assert(session.belongs_to(kernel));
  assert(applied.transaction_keys &&
         applied.committed_keys == session.committed_keys());
  assert(applied.session_work > 0);
  assert(applied.live.extent_growth == 4);
  assert(applied.live.existing_theory_variables ==
         std::vector<int32_t>({1, 2, 3, 4, 5, 6}));
  assert(applied.live.introduced_theory_variables ==
         std::vector<int32_t>({7, 8, 9, 10}));
  assert(applied.candidate_staged &&
         kernel.semantic_variable_candidate_pending);

  const auto &key = session.keys().front();
  assert(!key.source_equations.empty());
  for (const auto &equation : key.source_equations) {
    assert(!equation.premises.empty());
    for (const auto &premise : equation.premises) {
      assert(premise.native_clause_id);
      assert(!premise.clause.empty());
    }
  }

  strategy::finish_semantic_variable_candidate_action(kernel,
                                                       ApplyResult::Ok);
  const auto candidates = strategy::semantic_variable_candidates(kernel);
  assert(candidates.size() == 1);
  const auto *candidate =
      strategy::semantic_variable_candidate(kernel, candidates.front().id);
  assert(candidate && candidate->action_id == 41);
  assert(candidate->existing_variables == 6 &&
         candidate->introduced_variables == 4);
  for (size_t i = 0; i < candidate->variables.size(); ++i) {
    const auto expected = i < 6
        ? strategy::SemanticVariableRole::ExistingTheory
        : strategy::SemanticVariableRole::IntroducedTheory;
    assert(candidate->variables[i].role == expected);
    assert(candidate->variables[i].priority == 1);
  }
  const std::string journal = contents(records);
  assert(journal.find("\"action\":41,\"semantic\":41") !=
         std::string::npos);
  assert(journal.find("\"projection\":\"omit-extension\"") !=
         std::string::npos);
  std::fclose(records);
}

static void repeated_selection_is_durable_and_atomic() {
  Kernel kernel = direct_fixture();
  FILE *records = std::tmpfile();
  attach(kernel, records);
  AffineBasisActionSession session;

  const auto first = affine_basis_action_apply(kernel, session, limits());
  assert(first.status == AffineBasisActionStatus::Applied);
  assert(first.result == ApplyResult::Ok && session.committed_keys() == 1);
  assert(kernel_binary_live(kernel, internalize(3), internalize(4)));
  assert(kernel_binary_live(kernel, internalize(-3), internalize(-4)));
  assert(session.keys().front().source_equations.size() == 2);
  std::vector<uint64_t> source_ids;
  for (const auto &equation : session.keys().front().source_equations)
    for (const auto &premise : equation.premises)
      source_ids.push_back(premise.native_clause_id);
  std::sort(source_ids.begin(), source_ids.end());
  assert(source_ids ==
         std::vector<uint64_t>({1, 2, 3, 4, 5, 6, 7, 8}));
  strategy::begin_semantic_variable_candidate_action(kernel);

  assert(kernel_delete_binary(kernel, internalize(3), internalize(4)));
  assert(kernel_delete_binary(kernel, internalize(-3), internalize(-4)));
  const uint64_t next_clause_id = kernel.next_clause_id;
  const uint64_t action_id = kernel.structural_action_sequence;
  const size_t arena_words = kernel.arena.size();
  const long journal = std::ftell(records);
  auto duplicate_limits = limits();
  duplicate_limits.maximum_session_keys = 1;
  const auto second =
      affine_basis_action_apply(kernel, session, duplicate_limits);
  assert(second.status == AffineBasisActionStatus::AlreadyPublished);
  assert(second.result == ApplyResult::NoOp);
  assert(second.proposed_action_id == 42 && !second.committed_action_id);
  assert(second.transaction_keys == 1 && second.session_work > 0);
  assert(kernel.structural_action_sequence == action_id);
  assert(kernel.next_clause_id == next_clause_id &&
         kernel.arena.size() == arena_words && std::ftell(records) == journal);
  assert(session.committed_keys() == 1);
  std::fclose(records);
}

static void declines_do_not_commit_session_or_action() {
  {
    Kernel kernel = direct_fixture();
    FILE *records = std::tmpfile();
    attach(kernel, records);
    AffineBasisActionSession session;
    auto tiny = limits();
    tiny.maximum_session_work = 1;
    const auto result = affine_basis_action_apply(kernel, session, tiny);
    assert(result.status == AffineBasisActionStatus::ResourceExhausted);
    assert(result.proposed_action_id == 41 && !result.committed_action_id);
    assert(!session.bound() && !session.committed_keys());
    assert(kernel.structural_action_sequence == 40 &&
           std::ftell(records) == 0);
    std::fclose(records);
  }
  {
    Kernel first = direct_fixture();
    FILE *first_records = std::tmpfile();
    attach(first, first_records);
    AffineBasisActionSession session;
    assert(affine_basis_action_apply(first, session, limits()).status ==
           AffineBasisActionStatus::Applied);
    strategy::begin_semantic_variable_candidate_action(first);

    Kernel second = direct_fixture();
    FILE *second_records = std::tmpfile();
    attach(second, second_records);
    const auto stale = affine_basis_action_apply(second, session, limits());
    assert(stale.status == AffineBasisActionStatus::Stale);
    assert(!stale.proposed_action_id && !stale.committed_action_id);
    assert(second.structural_action_sequence == 40 &&
           std::ftell(second_records) == 0);
    std::fclose(first_records);
    std::fclose(second_records);
  }
  {
    Kernel kernel = direct_fixture();
    FILE *records = std::tmpfile();
    attach(kernel, records);
    AffineBasisActionSession session;
    auto tiny = limits();
    tiny.maximum_proof_additions = 1;
    const auto result = affine_basis_action_apply(kernel, session, tiny);
    assert(result.status == AffineBasisActionStatus::ResourceExhausted);
    assert(result.proposed_action_id == 41 && !result.committed_action_id);
    assert(!session.bound() && kernel.structural_action_sequence == 40 &&
           std::ftell(records) == 0);
    std::fclose(records);
  }
}

static void guard_is_state_only() {
  Kernel kernel = direct_fixture();
  FILE *records = std::tmpfile();
  attach(kernel, records);
  assert(affine_basis_action_guard(kernel));
  kernel_assign_decision(kernel, internalize(1));
  assert(!affine_basis_action_guard(kernel));
  std::fclose(records);
}

int main() {
  success_preserves_identity_and_metadata();
  repeated_selection_is_durable_and_atomic();
  declines_do_not_commit_session_or_action();
  guard_is_state_only();
  std::puts("test_affine_basis_action: passed");
}
