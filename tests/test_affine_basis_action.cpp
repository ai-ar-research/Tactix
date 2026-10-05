#undef NDEBUG
#include "rules/semantic_extension/affine_basis_action.hpp"

#include "rules/propagate/propagate.hpp"
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

static Kernel double_direct_fixture() {
  Kernel kernel = direct_fixture();
  kernel_reserve_vars(kernel, 8);
  add_equation(kernel, {5, 6, 7}, false);
  add_equation(kernel, {5, 6, 8}, true);
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
  assert(applied.live.extent_growth == 0);
  assert(applied.selection.owner_version == 1 &&
         applied.selection.elimination_profile == 1 &&
         applied.selection.kind == BasisRowKind::DirectBinary &&
         applied.selection.variables == std::vector<int32_t>({5, 6}) &&
         applied.selection.rhs &&
         applied.selection.source_equation_ordinals.size() == 2);
  assert(applied.live.existing_theory_variables ==
         std::vector<int32_t>({5, 6}));
  assert(applied.live.introduced_theory_variables.empty());
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
  assert(candidate->existing_variables == 2 &&
         candidate->introduced_variables == 0);
  for (size_t i = 0; i < candidate->variables.size(); ++i) {
    assert(candidate->variables[i].role ==
           strategy::SemanticVariableRole::ExistingTheory);
    assert(candidate->variables[i].priority == 1);
  }
  const std::string journal = contents(records);
  assert(journal.find("\"action\":41,\"semantic\":41") !=
         std::string::npos);
  assert(journal.find("\"owner\":1") != std::string::npos);
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
  assert(!second.transaction_keys && second.live.excluded_rows == 1);
  assert(kernel.structural_action_sequence == action_id);
  assert(kernel.next_clause_id == next_clause_id &&
         kernel.arena.size() == arena_words && std::ftell(records) == journal);
  assert(session.committed_keys() == 1);
  std::fclose(records);
}

static void immutable_owner_pages_and_publishes_one_row_at_a_time() {
  Kernel kernel = double_direct_fixture();
  FILE *records = std::tmpfile();
  attach(kernel, records);
  AffineBasisActionSession session;

  const uint64_t action_sequence = kernel.structural_action_sequence;
  const long journal = std::ftell(records);
  const auto first_page =
      affine_basis_action_offers(kernel, session, limits(), {}, 1);
  assert(first_page.status == AffineBasisOfferStatus::Complete);
  assert(first_page.offers.size() == 1 && first_page.more);
  assert(first_page.offers[0].variables == std::vector<int32_t>({3, 4}));
  assert(first_page.offers[0].source_equation_ordinals.size() == 2);
  assert(first_page.continuation.present && !session.bound());
  assert(kernel.structural_action_sequence == action_sequence &&
         std::ftell(records) == journal);
  const auto second_page = affine_basis_action_offers(
      kernel, session, limits(), first_page.continuation, 1);
  assert(second_page.status == AffineBasisOfferStatus::Complete);
  assert(second_page.offers.size() == 1 && !second_page.more);
  assert(second_page.offers[0].variables == std::vector<int32_t>({7, 8}));
  const auto exhausted_page = affine_basis_action_offers(
      kernel, session, limits(), second_page.continuation, 1);
  assert(exhausted_page.status == AffineBasisOfferStatus::NoStructure &&
         exhausted_page.offers.empty() &&
         exhausted_page.candidates_examined == 0);
  // Four one-source width-three rows are already represented by their source
  // CNF.  They never enter pagination, while both two-source cancellations do.
  assert(first_page.candidates_examined == 1 &&
         second_page.candidates_examined == 1);
  auto forged_cursor = first_page.continuation;
  forged_cursor.last_examined.variables = {3, 5};
  assert(affine_basis_action_offers(kernel, session, limits(), forged_cursor, 1)
             .status == AffineBasisOfferStatus::Invalid);

  const auto first = affine_basis_action_apply(kernel, session, limits());
  assert(first.status == AffineBasisActionStatus::Applied &&
         first.transaction_keys == 1 && session.committed_keys() == 1);
  assert(first.selection == first_page.offers[0]);
  assert(session.owner_version() == 1 && session.owner());
  strategy::begin_semantic_variable_candidate_action(kernel);
  const auto second = affine_basis_action_apply(kernel, session, limits());
  assert(second.status == AffineBasisActionStatus::Applied &&
         second.result == ApplyResult::Ok);
  assert(second.live.excluded_rows == 0 && second.transaction_keys == 1 &&
         second.committed_keys == 2 && session.committed_keys() == 2);
  assert(second.selection == second_page.offers[0]);
  assert(second.live.existing_theory_variables ==
         std::vector<int32_t>({7, 8}));
  assert(kernel_binary_live(kernel, internalize(7), internalize(8)));
  assert(kernel_binary_live(kernel, internalize(-7), internalize(-8)));

  strategy::begin_semantic_variable_candidate_action(kernel);
  // Later non-owner clauses do not enter this owner's equation ordinals.
  kernel_reserve_vars(kernel, 12);
  add_equation(kernel, {9, 10, 11}, false);
  add_equation(kernel, {9, 10, 12}, true);
  const auto exhausted = affine_basis_action_apply(kernel, session, limits());
  assert(exhausted.status == AffineBasisActionStatus::AlreadyPublished);
  assert(session.committed_keys() == 2);
  std::fclose(records);
}

static void source_local_invalidation_and_root_trail() {
  Kernel kernel = double_direct_fixture();
  kernel_reserve_vars(kernel, 9);
  kernel.unit_ids[var_of(internalize(9))] = ++kernel.next_clause_id;
  kernel_assign_unit(kernel, internalize(9));
  assert(propagate_service(kernel) == ApplyResult::Ok);
  FILE *records = std::tmpfile();
  attach(kernel, records);
  AffineBasisActionSession session;
  assert(affine_basis_action_guard(kernel));
  assert(affine_basis_action_apply(kernel, session, limits()).status ==
         AffineBasisActionStatus::Applied);
  strategy::begin_semantic_variable_candidate_action(kernel);

  // Retire one source used only by the consumed first row.  The second row's
  // exact source set remains eligible; validation is row-local.
  bool retired = false;
  for (const auto &source : session.owner()->sources) {
    bool first_scope = false;
    for (int32_t literal : source.clause)
      first_scope |= std::abs(literal) == 1;
    if (first_scope && source.kind == AffineSourceKind::Large) {
      clause_at(kernel.arena, source.ref)->flags |= Clause::GARBAGE;
      retired = true;
      break;
    }
  }
  assert(retired);
  const auto page = affine_basis_action_offers(kernel, session, limits(), {}, 1);
  assert(page.status == AffineBasisOfferStatus::Complete);
  assert(page.offers.size() == 1 &&
         page.offers[0].variables == std::vector<int32_t>({7, 8}));
  assert(affine_basis_action_apply(kernel, session, limits()).status ==
         AffineBasisActionStatus::Applied);
  std::fclose(records);
}

static void exact_offer_selection_and_forgery_rejection() {
  Kernel kernel = double_direct_fixture();
  FILE *records = std::tmpfile();
  attach(kernel, records);
  AffineBasisActionSession session;
  const auto page =
      affine_basis_action_offers(kernel, session, limits(), {}, 2);
  assert(page.status == AffineBasisOfferStatus::Complete &&
         page.offers.size() == 2);
  const auto selected = affine_basis_action_apply(
      kernel, session, limits(), page.offers[1]);
  assert(selected.status == AffineBasisActionStatus::Applied &&
         selected.selection == page.offers[1]);
  assert(kernel_binary_live(kernel, internalize(7), internalize(8)) &&
         !kernel_binary_live(kernel, internalize(3), internalize(4)));
  strategy::begin_semantic_variable_candidate_action(kernel);

  auto forged = page.offers[0];
  forged.source_equation_ordinals = {0};
  const uint64_t sequence = kernel.structural_action_sequence;
  const uint64_t clause_id = kernel.next_clause_id;
  const long journal = std::ftell(records);
  const auto rejected =
      affine_basis_action_apply(kernel, session, limits(), forged);
  assert(rejected.status == AffineBasisActionStatus::Stale);
  assert(!rejected.committed_action_id &&
         kernel.structural_action_sequence == sequence &&
         kernel.next_clause_id == clause_id && std::ftell(records) == journal &&
         session.committed_keys() == 1);
  std::fclose(records);
}

static void escaped_offer_cannot_rebind_to_recaptured_sources() {
  Kernel kernel = direct_fixture();
  FILE *records = std::tmpfile();
  attach(kernel, records);
  AffineBasisActionSession session;
  const auto page =
      affine_basis_action_offers(kernel, session, limits(), {}, 1);
  assert(page.status == AffineBasisOfferStatus::Complete &&
         page.offers.size() == 1 && !session.bound());
  const auto offered = page.offers.front();
  assert(offered.source_equations.size() == 2);

  // Replace one complete source equation with byte-identical clauses carrying
  // new native proof identities.  Equation ordinals and the derived x3/x4 row
  // remain the same, but the escaped offer belongs to the old owner.
  const std::vector<int32_t> first_scope{1, 2, 3};
  uint32_t retired = 0;
  for (Ref ref = 0; ref < kernel.arena.size();) {
    Clause *clause = clause_at(kernel.arena, ref);
    ref += clause_actual_words(clause);
    if (clause->garbage() || clause->size != 3) continue;
    std::vector<int32_t> scope;
    for (Lit literal : *clause)
      scope.push_back(std::abs(kernel_externalize(kernel, literal)));
    std::sort(scope.begin(), scope.end());
    if (scope == first_scope) {
      clause->flags |= Clause::GARBAGE;
      ++retired;
    }
  }
  assert(retired == 4);
  add_equation(kernel, {1, 2, 3}, false);
  const uint64_t sequence = kernel.structural_action_sequence;
  const uint64_t clause_id = kernel.next_clause_id;
  const long journal = std::ftell(records);
  const auto stale =
      affine_basis_action_apply(kernel, session, limits(), offered);
  assert(stale.status == AffineBasisActionStatus::Stale &&
         !stale.committed_action_id && !session.bound());
  assert(kernel.structural_action_sequence == sequence &&
         kernel.next_clause_id == clause_id && std::ftell(records) == journal);
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

static void both_parities_fit_one_recognition_group() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 3);
  add_equation(kernel, {1, 2, 3}, false);
  add_equation(kernel, {1, 2, 3}, true);
  FILE *records = std::tmpfile();
  attach(kernel, records);
  AffineBasisActionSession session;
  const auto page =
      affine_basis_action_offers(kernel, session, limits(), {}, 1);
  assert(page.status == AffineBasisOfferStatus::Complete);
  assert(page.offers.size() == 1);
  assert(page.offers[0].kind == BasisRowKind::DirectContradiction);
  assert(page.offers[0].source_equation_ordinals ==
         std::vector<size_t>({0, 1}));
  auto one_group = limits();
  one_group.image.maximum_groups = 1;
  assert(affine_basis_row_selection_within_limits(page.offers[0], one_group));
  std::fclose(records);
}

int main() {
  success_preserves_identity_and_metadata();
  repeated_selection_is_durable_and_atomic();
  immutable_owner_pages_and_publishes_one_row_at_a_time();
  source_local_invalidation_and_root_trail();
  exact_offer_selection_and_forgery_rejection();
  escaped_offer_cannot_rebind_to_recaptured_sources();
  declines_do_not_commit_session_or_action();
  guard_is_state_only();
  both_parities_fit_one_recognition_group();
  std::puts("test_affine_basis_action: passed");
}
