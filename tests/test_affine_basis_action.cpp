#undef NDEBUG
#include "affine_formula_support.hpp"
#include "test_io.hpp"
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

static Kernel direct_fixture() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 4);
  add_equation(kernel, {1, 2, 3}, false);
  add_equation(kernel, {1, 2, 4}, true);
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
  kernel.structural_provenance = records;
  kernel.structural_action_sequence = 40;
}

// The planned rows an unbound session's owner offers.
static std::vector<AffineBasisRowSelection> rows(Kernel &kernel, FILE *records) {
  const auto bounds = limits();
  AffineLiveSnapshot owner;
  return planned_rows(kernel, bounds.image, bounds.basis,
                      bounds.maximum_snapshot_literals,
                      bounds.maximum_snapshot_work, records, owner);
}

// The immutable owner publishes one row per action, in offer order, and
// later non-owner clauses never extend it.
static void immutable_owner_publishes_one_row_at_a_time() {
  Kernel kernel = double_direct_fixture();
  FILE *records = temporary_records();
  attach(kernel, records);
  AffineBasisActionSession session;

  const auto first = affine_basis_action_apply(kernel, session, limits());
  assert(first.status == AffineBasisActionStatus::Applied &&
         first.transaction_keys == 1 && session.committed_keys() == 1);
  assert(first.selection.variables == std::vector<int32_t>({3, 4}) &&
         first.selection.source_equation_ordinals.size() == 2);
  assert(session.owner_version() == 1 && session.owner());
  strategy::begin_semantic_variable_candidate_action(kernel);
  const auto second = affine_basis_action_apply(kernel, session, limits());
  assert(second.status == AffineBasisActionStatus::Applied &&
         second.result == ApplyResult::Ok);
  assert(second.live.excluded_rows == 0 && second.transaction_keys == 1 &&
         second.committed_keys == 2 && session.committed_keys() == 2);
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
  add_clause(kernel, {9});
  assert(propagate_service(kernel) == ApplyResult::Ok);
  FILE *records = temporary_records();
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
  const auto second = affine_basis_action_apply(kernel, session, limits());
  assert(second.status == AffineBasisActionStatus::Applied &&
         second.selection.variables == std::vector<int32_t>({7, 8}));
  std::fclose(records);
}

static void exact_offer_selection_and_forgery_rejection() {
  Kernel kernel = double_direct_fixture();
  FILE *records = temporary_records();
  attach(kernel, records);
  AffineBasisActionSession session;
  const auto planned = rows(kernel, records);
  const auto later = planned_row(planned, {7, 8});
  const auto selected =
      affine_basis_action_apply(kernel, session, limits(), later);
  assert(selected.status == AffineBasisActionStatus::Applied &&
         selected.selection == later);
  assert(kernel_binary_live(kernel, internalize(7), internalize(8)) &&
         !kernel_binary_live(kernel, internalize(3), internalize(4)));
  strategy::begin_semantic_variable_candidate_action(kernel);

  auto forged = planned_row(planned, {3, 4});
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

static void retained_owner_accepts_wide_direct_wide_sequence() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 313);
  // The three selected consequences have the same shapes as the first three
  // Rank263 requests: wide (with helpers), direct (without extent growth),
  // then another wide row from the original immutable owner.
  add_equation(kernel, {1, 2, 4}, false);
  add_equation(kernel, {1, 3, 119}, false);
  add_equation(kernel, {7, 11, 124}, false);
  add_equation(kernel, {7, 118, 120}, false);
  add_equation(kernel, {111, 115, 208}, false);
  add_equation(kernel, {111, 211, 313}, false);
  add_equation(kernel, {115, 211, 212}, false);
  FILE *records = temporary_records();
  attach(kernel, records);
  AffineBasisActionSession session;
  const auto planned = rows(kernel, records);
  const auto first = planned_row(planned, {11, 118, 120, 124});
  const auto second = planned_row(planned, {208, 212, 313});
  const auto third = planned_row(planned, {2, 3, 4, 119});

  auto applied =
      affine_basis_action_apply(kernel, session, limits(), first);
  assert(applied.status == AffineBasisActionStatus::Applied &&
         applied.live.extent_growth > 0 && session.committed_keys() == 1);
  strategy::finish_semantic_variable_candidate_action(kernel,
                                                       ApplyResult::Ok);
  assert(propagate_service(kernel) == ApplyResult::Ok);

  applied = affine_basis_action_apply(kernel, session, limits(), second);
  assert(applied.status == AffineBasisActionStatus::Applied &&
         applied.live.extent_growth == 0 && session.committed_keys() == 2);
  strategy::finish_semantic_variable_candidate_action(kernel,
                                                       ApplyResult::Ok);

  applied = affine_basis_action_apply(kernel, session, limits(), third);
  assert(applied.status == AffineBasisActionStatus::Applied &&
         applied.live.extent_growth > 0 && session.committed_keys() == 3);
  std::fclose(records);
}

static void escaped_offer_cannot_rebind_to_recaptured_sources() {
  Kernel kernel = direct_fixture();
  FILE *records = temporary_records();
  attach(kernel, records);
  AffineBasisActionSession session;
  const auto offered = planned_row(rows(kernel, records), {3, 4});
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
  for (bool proof_bound : {false, true}) {
    Kernel kernel = direct_fixture();
    FILE *records = temporary_records();
    attach(kernel, records);
    AffineBasisActionSession session;
    auto tiny = limits();
    (proof_bound ? tiny.maximum_proof_additions : tiny.maximum_session_work) = 1;
    const auto result = affine_basis_action_apply(kernel, session, tiny);
    assert(result.status == AffineBasisActionStatus::ResourceExhausted);
    assert(result.proposed_action_id == 41 && !result.committed_action_id);
    assert(!session.bound() && !session.committed_keys());
    assert(kernel.structural_action_sequence == 40 &&
           std::ftell(records) == 0);
    std::fclose(records);
  }
  Kernel first = direct_fixture();
  FILE *first_records = temporary_records();
  attach(first, first_records);
  AffineBasisActionSession session;
  assert(affine_basis_action_apply(first, session, limits()).status ==
         AffineBasisActionStatus::Applied);
  strategy::begin_semantic_variable_candidate_action(first);

  Kernel second = direct_fixture();
  FILE *second_records = temporary_records();
  attach(second, second_records);
  const auto stale = affine_basis_action_apply(second, session, limits());
  assert(stale.status == AffineBasisActionStatus::Stale);
  assert(!stale.proposed_action_id && !stale.committed_action_id);
  assert(second.structural_action_sequence == 40 &&
         std::ftell(second_records) == 0);
  std::fclose(first_records);
  std::fclose(second_records);
}

// A row beyond the bounded proof scope (its four sources span 17 variables)
// is a per-row decline: the offer still tries the later row.
static void unsupported_proof_row_does_not_hide_later_row() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 23);
  add_equation(kernel, {1, 2, 3, 4, 5, 6, 7, 8}, false);
  add_equation(kernel, {1, 2, 3, 4, 9, 10, 11, 12}, false);
  add_equation(kernel, {5, 6, 7, 8, 13, 14, 15, 16}, false);
  add_equation(kernel, {9, 10, 11, 12, 13, 14, 15, 17}, true);
  add_equation(kernel, {20, 21, 22}, false);
  add_equation(kernel, {20, 21, 23}, true);
  FILE *records = temporary_records();
  attach(kernel, records);
  const auto planned = rows(kernel, records);
  const auto unsupported = planned_row(planned, {16, 17});
  const auto later = planned_row(planned, {22, 23});
  assert(unsupported.source_equation_ordinals.size() == 4 &&
         unsupported < later);
  AffineBasisActionSession session;
  const auto applied = affine_basis_action_apply(kernel, session, limits());
  assert(applied.status == AffineBasisActionStatus::Applied &&
         applied.selection == later && session.committed_keys() == 1);
  assert(kernel_binary_live(kernel, internalize(22), internalize(23)) &&
         !kernel_binary_live(kernel, internalize(16), internalize(17)));
  std::fclose(records);
}

static void both_parities_fit_one_recognition_group() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 3);
  add_equation(kernel, {1, 2, 3}, false);
  add_equation(kernel, {1, 2, 3}, true);
  FILE *records = temporary_records();
  attach(kernel, records);
  AffineBasisActionSession session;
  const auto applied = affine_basis_action_apply(kernel, session, limits());
  assert(applied.status == AffineBasisActionStatus::Applied &&
         applied.result == ApplyResult::Unsat);
  assert(applied.selection.kind == BasisRowKind::DirectContradiction);
  assert(applied.selection.source_equation_ordinals ==
         std::vector<size_t>({0, 1}));
  auto one_group = limits();
  one_group.image.maximum_groups = 1;
  assert(affine_basis_row_selection_within_limits(applied.selection, one_group));
  std::fclose(records);
}

int main() {
  immutable_owner_publishes_one_row_at_a_time();
  source_local_invalidation_and_root_trail();
  exact_offer_selection_and_forgery_rejection();
  retained_owner_accepts_wide_direct_wide_sequence();
  escaped_offer_cannot_rebind_to_recaptured_sources();
  declines_do_not_commit_session_or_action();
  unsupported_proof_row_does_not_hide_later_row();
  both_parities_fit_one_recognition_group();
  std::puts("test_affine_basis_action: passed");
}
