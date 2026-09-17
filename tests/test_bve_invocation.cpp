#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/bve/bve.hpp"
#include "strategy/bve_invocation.hpp"

#include <cassert>
#include <csignal>
#include <cstdio>
#include <vector>

using namespace tactix;
using namespace tactix::strategy;

static const RuleVTable &bve_rule() {
  return tactix_rules()[static_cast<size_t>(tactix_rule_id("BVE"))];
}

static void add_pair(Kernel &k, int pivot, int positive_partner,
                     int negative_partner) {
  kernel_add_binary_clause(k, internalize(pivot),
                           internalize(positive_partner));
  kernel_add_binary_clause(k, internalize(-pivot),
                           internalize(negative_partner));
}

static void build_two_pivots(Kernel &k) {
  kernel_reserve_vars(k, 6);
  add_pair(k, 1, 3, 4);
  add_pair(k, 2, 5, 6);
}

static void test_empty_pivot_progress_distinguishes_retirement() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  const uint64_t first = lit_instance(k, internalize(1));
  assert(apply_rule_instance(bve_rule(), k, first) == ApplyResult::Ok);
  const auto witness = k.reconstruction;
  const auto retired = k.retired_events;
  const uint64_t clauses = k.next_clause_id;
  assert(apply_rule_instance(bve_rule(), k, first) == ApplyResult::NoOp);
  assert(k.stats.bve_eliminated == 1 && k.reconstruction == witness);
  assert(k.retired_events == retired && k.next_clause_id == clauses);
  // An active variable with no occurrences still leaves the active population;
  // the no-op repair must not turn that legal elimination into a restriction.
  assert(apply_rule_instance(bve_rule(), k, lit_instance(k, internalize(3))) ==
         ApplyResult::Ok);
  assert(kernel_var_retired(k, var_of(internalize(3))));
  assert(apply_rule(bve_rule(), k) == ApplyResult::Ok);
  assert(apply_rule(bve_rule(), k) == ApplyResult::NoOp);
  assert(k.stats.bve_eliminated == 3 && k.unassigned_active == 0);
  kernel_check_invariants(k);
}

static void test_one_instance_matches_legacy_and_restores_parameters() {
  Kernel legacy, configured;
  kernel_reserve_vars(legacy, 3);
  kernel_reserve_vars(configured, 3);
  add_pair(legacy, 1, 2, 3);
  add_pair(configured, 1, 2, 3);
  legacy.bve_gate_mask = configured.bve_gate_mask = 0;
  legacy.bve_bound = configured.bve_bound = 7;
  legacy.bve_occurrence_limit = configured.bve_occurrence_limit = 19;
  legacy.bve_clause_size_limit = configured.bve_clause_size_limit = 23;
  legacy.bve_xor_arity_limit = configured.bve_xor_arity_limit = 4;
  legacy.bve_resolvent_size_limit = configured.bve_resolvent_size_limit = 29;
  legacy.bve_backward_occurrence_limit =
      configured.bve_backward_occurrence_limit = 31;
  legacy.bve_definition_budget = configured.bve_definition_budget = 37;
  legacy.bve_definition_cores = configured.bve_definition_cores = 3;
  legacy.bve_definition_core_budget_multiplier =
      configured.bve_definition_core_budget_multiplier = 43;
  legacy.bve_backward = configured.bve_backward = false;

  const uint64_t pivot = lit_instance(internalize(1));
  const ApplyResult expected = apply_rule_instance(bve_rule(), legacy, pivot);
  const BveParameters saved = BveParameters::from_kernel(configured);
  const uint64_t selected[] = {pivot};
  BveInvocationRequest request{saved, InvocationGranularity::OneInstance,
                               selected, 41};
  InvocationBudget budget(WorkUnit::BveTicks, WorkLimit::unlimited());
  const InvocationResult actual = invoke_bve(bve_rule(), configured, request,
                                             budget);

  assert(expected == ApplyResult::Ok && actual.rule_result == expected);
  assert(actual.status == InvocationStatus::Completed);
  assert(actual.selection_complete && actual.applications == 1);
  assert(actual.made_rule_progress && actual.configuration_revision == 41);
  assert(actual.work == configured.stats.bve_resolutions &&
         budget.consumed() == actual.work);
  assert(configured.stats.bve_resolutions == legacy.stats.bve_resolutions);
  assert(configured.stats.bve_eliminated == legacy.stats.bve_eliminated);
  assert(configured.reconstruction == legacy.reconstruction);
  assert(kernel_binary_live(configured, internalize(2), internalize(3)) ==
         kernel_binary_live(legacy, internalize(2), internalize(3)));
  const BveParameters restored = BveParameters::from_kernel(configured);
  assert(restored.gate_mask == saved.gate_mask);
  assert(restored.growth_bound == saved.growth_bound);
  assert(restored.occurrence_limit == saved.occurrence_limit);
  assert(restored.clause_size_limit == saved.clause_size_limit);
  assert(restored.xor_arity_limit == saved.xor_arity_limit);
  assert(restored.resolvent_size_limit == saved.resolvent_size_limit);
  assert(restored.backward_occurrence_limit ==
         saved.backward_occurrence_limit);
  assert(restored.definition_budget == saved.definition_budget);
  assert(restored.definition_cores == saved.definition_cores);
  assert(restored.definition_core_budget_multiplier ==
         saved.definition_core_budget_multiplier);
  assert(restored.backward == saved.backward);
  kernel_check_invariants(configured);
}

static void test_per_call_bound_is_effective_and_scoped() {
  Kernel k;
  kernel_reserve_vars(k, 12);
  for (int i = 2; i <= 3; ++i)
    kernel_add_binary_clause(k, internalize(1), internalize(i));
  for (int i = 4; i <= 12; ++i)
    kernel_add_binary_clause(k, internalize(-1), internalize(i));
  k.bve_gate_mask = 0;
  k.bve_bound = 0;
  BveParameters posed = BveParameters::from_kernel(k);
  posed.growth_bound = 16;
  const uint64_t pivot[] = {lit_instance(internalize(1))};
  BveInvocationRequest request{posed, InvocationGranularity::OneInstance,
                               pivot, 0};
  InvocationBudget budget(WorkUnit::BveTicks, WorkLimit::unlimited());
  const InvocationResult result = invoke_bve(bve_rule(), k, request, budget);
  assert(result.status == InvocationStatus::Completed);
  assert(result.rule_result == ApplyResult::Ok && result.work == 18);
  assert(kernel_var_retired(k, var_of(internalize(1))));
  assert(k.bve_bound == 0 && "the invocation must restore the caller's pose");
}

static void build_xor3(Kernel &k) {
  kernel_reserve_vars(k, 4);
  // Forbid every assignment of odd parity to x,a,b,c.
  for (uint32_t assignment = 0; assignment < 16; ++assignment) {
    if (__builtin_popcount(assignment) % 2 == 0)
      continue;
    Lit clause[4];
    for (uint32_t v = 0; v < 4; ++v)
      clause[v] = make_lit(v, (assignment >> v) & 1u);
    kernel_add_large_clause(k, clause, 4, false, 0);
  }
}

static void test_per_call_xor_arity_is_effective_and_scoped() {
  Kernel low, high;
  build_xor3(low);
  build_xor3(high);
  const uint64_t pivot[] = {lit_instance(internalize(1))};

  BveParameters low_parameters = BveParameters::from_kernel(low);
  low_parameters.gate_mask = BVE_GATE_XOR;
  low_parameters.xor_arity_limit = 2;
  BveInvocationRequest low_request{low_parameters,
                                   InvocationGranularity::OneInstance, pivot,
                                   0};
  InvocationBudget low_budget(WorkUnit::BveTicks, WorkLimit::unlimited());
  assert(invoke_bve(bve_rule(), low, low_request, low_budget).rule_result ==
         ApplyResult::Ok);
  assert(low.stats.bve_gates_xor == 0);

  high.bve_xor_arity_limit = 2; // caller state restored after the posed call
  BveParameters high_parameters = BveParameters::from_kernel(high);
  high_parameters.gate_mask = BVE_GATE_XOR;
  high_parameters.xor_arity_limit = 3;
  BveInvocationRequest high_request{high_parameters,
                                    InvocationGranularity::OneInstance, pivot,
                                    0};
  InvocationBudget high_budget(WorkUnit::BveTicks, WorkLimit::unlimited());
  assert(invoke_bve(bve_rule(), high, high_request, high_budget).rule_result ==
         ApplyResult::Ok);
  assert(high.stats.bve_gates_xor == 1);
  assert(high.bve_xor_arity_limit == 2);
}

static void test_batch_budget_stops_between_atomic_pivots() {
  Kernel k;
  build_two_pivots(k);
  BveParameters parameters = BveParameters::from_kernel(k);
  parameters.gate_mask = 0;
  const uint64_t selected[] = {lit_instance(internalize(1)),
                               lit_instance(internalize(2))};
  BveInvocationRequest request{parameters,
                               InvocationGranularity::SelectedBatch, selected,
                               0};
  InvocationBudget budget(WorkUnit::BveTicks, WorkLimit::bounded(1));
  const InvocationResult result = invoke_bve(bve_rule(), k, request, budget);
  assert(result.status == InvocationStatus::BudgetExhausted);
  assert(result.budget_status == BudgetStatus::Exhausted);
  assert(result.applications == 1 && result.work == 1);
  assert(!result.selection_complete && result.made_rule_progress);
  assert(kernel_var_retired(k, var_of(internalize(1))));
  assert(!kernel_var_retired(k, var_of(internalize(2))));
}

static void test_pass_session_poses_once_and_keeps_cumulative_budget() {
  Kernel k;
  build_two_pivots(k);
  k.bve_gate_mask = 0;
  k.bve_bound = 0;
  BveParameters parameters = BveParameters::from_kernel(k);
  parameters.growth_bound = 16;
  InvocationBudget budget(WorkUnit::BveTicks, WorkLimit::bounded(1));
  {
    BveInvocationSession session(bve_rule(), k, parameters, budget, 53);
    assert(session.valid() && k.bve_bound == 16);
    const InvocationResult first =
        session.apply_one(lit_instance(internalize(1)));
    assert(first.status == InvocationStatus::Completed);
    assert(first.applications == 1 && first.work == 1);
    assert(first.configuration_revision == 53 && budget.consumed() == 1);
    const InvocationResult second =
        session.apply_one(lit_instance(internalize(2)));
    assert(second.status == InvocationStatus::BudgetExhausted);
    assert(second.applications == 0 && budget.consumed() == 1);
    assert(!kernel_var_retired(k, var_of(internalize(2))));
  }
  assert(k.bve_bound == 0 && "the session must restore the caller pose");
  kernel_check_invariants(k);
}

static void test_complete_pass_and_stale_batch_are_distinct() {
  Kernel pass;
  build_two_pivots(pass);
  BveParameters parameters = BveParameters::from_kernel(pass);
  parameters.gate_mask = 0;
  BveInvocationRequest complete{parameters,
                                InvocationGranularity::CompleteCurrentPass,
                                {}, 7};
  InvocationBudget unlimited(WorkUnit::BveTicks, WorkLimit::unlimited());
  const InvocationResult full =
      invoke_bve(bve_rule(), pass, complete, unlimited);
  assert(full.status == InvocationStatus::Completed);
  assert(full.selection_complete && full.applications == 6);
  assert(pass.stats.bve_eliminated == 6);

  Kernel stale;
  kernel_reserve_vars(stale, 2);
  kernel_add_binary_clause(stale, internalize(1), internalize(2));
  kernel_add_binary_clause(stale, internalize(-1), internalize(2));
  const uint64_t selected[] = {lit_instance(internalize(1)),
                               lit_instance(internalize(2))};
  BveInvocationRequest batch{BveParameters::from_kernel(stale),
                             InvocationGranularity::SelectedBatch, selected,
                             0};
  InvocationBudget batch_budget(WorkUnit::BveTicks, WorkLimit::unlimited());
  const InvocationResult changed =
      invoke_bve(bve_rule(), stale, batch, batch_budget);
  assert(changed.status == InvocationStatus::StaleRequest);
  assert(changed.applications == 1 && !changed.selection_complete);
  assert(stale.value(internalize(2)) > 0);
}

static void test_pure_scope_filters_only_generated_complete_passes() {
  Kernel generated;
  kernel_reserve_vars(generated, 3);
  kernel_add_binary_clause(generated, internalize(1), internalize(2));
  kernel_add_binary_clause(generated, internalize(-2), internalize(3));
  BveParameters pure = BveParameters::from_kernel(generated);
  pure.candidate_scope = BveCandidateScope::PureOnly;
  pure.gate_mask = 0;
  BveInvocationRequest complete{pure,
                                InvocationGranularity::CompleteCurrentPass,
                                {}, 0};
  InvocationBudget complete_budget(WorkUnit::BveTicks,
                                   WorkLimit::unlimited());
  const InvocationResult filtered =
      invoke_bve(bve_rule(), generated, complete, complete_budget);
  assert(filtered.status == InvocationStatus::Completed);
  assert(filtered.selection_complete && filtered.applications == 2);
  assert(kernel_var_retired(generated, var_of(internalize(1))));
  assert(kernel_var_retired(generated, var_of(internalize(3))));
  assert(!kernel_var_retired(generated, var_of(internalize(2))));

  Kernel explicit_instance;
  kernel_reserve_vars(explicit_instance, 3);
  add_pair(explicit_instance, 1, 2, 3);
  pure = BveParameters::from_kernel(explicit_instance);
  pure.candidate_scope = BveCandidateScope::PureOnly;
  pure.gate_mask = 0;
  const uint64_t pivot[] = {lit_instance(internalize(1))};
  BveInvocationRequest one{pure, InvocationGranularity::OneInstance, pivot, 0};
  InvocationBudget one_budget(WorkUnit::BveTicks, WorkLimit::unlimited());
  const InvocationResult unfiltered =
      invoke_bve(bve_rule(), explicit_instance, one, one_budget);
  assert(unfiltered.status == InvocationStatus::Completed);
  assert(unfiltered.applications == 1 && unfiltered.made_rule_progress);
  assert(kernel_var_retired(explicit_instance, var_of(internalize(1))));
  kernel_check_invariants(generated);
  kernel_check_invariants(explicit_instance);
}

static void test_pure_scope_inside_unordered_dense_state() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(3));

  OccIndex occurrences;
  kernel_dense_connect(k, occurrences, /*track_order=*/false);
  BveParameters pure = BveParameters::from_kernel(k);
  pure.candidate_scope = BveCandidateScope::PureOnly;
  pure.gate_mask = 0;
  BveInvocationRequest request{pure,
                               InvocationGranularity::CompleteCurrentPass,
                               {}, 0};
  InvocationBudget budget(WorkUnit::BveTicks, WorkLimit::unlimited());
  const InvocationResult result = invoke_bve(bve_rule(), k, request, budget);
  assert(result.status == InvocationStatus::Completed);
  assert(result.selection_complete && result.applications == 2);
  assert(kernel_var_retired(k, var_of(internalize(1))));
  assert(kernel_var_retired(k, var_of(internalize(3))));
  assert(!kernel_var_retired(k, var_of(internalize(2))));
  kernel_check_invariants(k);
  kernel_dense_disconnect(k);
}

static void test_invalid_zero_budget_and_stop_do_not_mutate() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_pair(k, 1, 2, 3);
  const uint64_t pivot[] = {lit_instance(internalize(1))};
  BveParameters invalid = BveParameters::from_kernel(k);
  invalid.xor_arity_limit = BVE_XOR_ARITY_CAPACITY + 1;
  BveInvocationRequest bad{invalid, InvocationGranularity::OneInstance, pivot,
                           0};
  InvocationBudget bad_budget(WorkUnit::BveTicks, WorkLimit::unlimited());
  assert(invoke_bve(bve_rule(), k, bad, bad_budget).status ==
         InvocationStatus::InvalidRequest);
  assert(k.stats.bve_resolutions == 0);

  BveInvocationRequest request{BveParameters::from_kernel(k),
                               InvocationGranularity::OneInstance, pivot, 0};
  InvocationBudget zero(WorkUnit::BveTicks, WorkLimit::bounded(0));
  assert(invoke_bve(bve_rule(), k, request, zero).status ==
         InvocationStatus::BudgetExhausted);
  assert(k.stats.bve_resolutions == 0);

  volatile sig_atomic_t stop = 1;
  k.stop = &stop;
  InvocationBudget ready(WorkUnit::BveTicks, WorkLimit::unlimited());
  assert(invoke_bve(bve_rule(), k, request, ready).status ==
         InvocationStatus::Interrupted);
  assert(k.stats.bve_resolutions == 0);
  k.stop = nullptr;
}

static void test_terminal_result_is_not_overwritten_by_completion() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_assign_unit(k, internalize(-2));
  const uint64_t pivot[] = {lit_instance(internalize(1))};
  BveInvocationRequest request{BveParameters::from_kernel(k),
                               InvocationGranularity::OneInstance, pivot, 0};
  InvocationBudget budget(WorkUnit::BveTicks, WorkLimit::unlimited());
  const InvocationResult result = invoke_bve(bve_rule(), k, request, budget);
  assert(result.status == InvocationStatus::Unsat);
  assert(result.rule_result == ApplyResult::Unsat);
  assert(result.selection_complete && result.applications == 1);
}

int main() {
  test_empty_pivot_progress_distinguishes_retirement();
  test_one_instance_matches_legacy_and_restores_parameters();
  test_per_call_bound_is_effective_and_scoped();
  test_per_call_xor_arity_is_effective_and_scoped();
  test_batch_budget_stops_between_atomic_pivots();
  test_pass_session_poses_once_and_keeps_cumulative_budget();
  test_complete_pass_and_stale_batch_are_distinct();
  test_pure_scope_filters_only_generated_complete_passes();
  test_pure_scope_inside_unordered_dense_state();
  test_invalid_zero_budget_and_stop_do_not_mutate();
  test_terminal_result_is_not_overwritten_by_completion();
  std::puts("test_bve_invocation OK");
}
