#undef NDEBUG
#include "policies/action_execution.hpp"
#include "policies/heuristics.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>

using namespace tactix;
using namespace tactix::execution;

static void test_prefix_and_purpose(RestartPurpose purpose) {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 5);
  kernel_assign_unit(k, internalize(1));
  const Lit learned[] = {internalize(3), internalize(4), internalize(5)};
  const Ref ref = kernel_add_large_clause(k, learned, 3, true, 2, 101);
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(execute_action(k, DecisionRequest{internalize(2)}) == ApplyResult::Ok);
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(execute_action(k, DecisionRequest{internalize(-3)}) == ApplyResult::Ok);
  assert(propagate_apply(k) == ApplyResult::Ok);
  const auto root_count = k.stats.root_assignments;
  assert(execute_action(k, RestartRequest{1, purpose}) == ApplyResult::Ok);
  assert(k.level == 1 && k.value(internalize(1)) > 0);
  assert(k.value(internalize(2)) > 0 && k.value(internalize(3)) == 0);
  assert(k.stats.root_assignments == root_count);
  assert(!clause_at(k.arena, ref)->garbage());
  assert(clause_at(k.arena, ref)->id() == 101);
  assert(k.stats.decisions == 2 && k.stats.api_decisions == 0);
  const bool scheduled = purpose == RestartPurpose::Scheduled;
  assert(k.stats.restarts == uint64_t(scheduled));
  assert(k.stats.restarts_reused == uint64_t(scheduled));
  assert(execute_action(k, RestartRequest{0, purpose}) == ApplyResult::Ok);
  assert(k.level == 0 && k.value(internalize(1)) > 0);
  assert(k.value(internalize(2)) == 0);
  assert(k.stats.restarts == 2 * uint64_t(scheduled));
  assert(k.stats.restarts_reused == uint64_t(scheduled));
  kernel_check_invariants(k);
}

static void test_decision_origin_and_no_change() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  assert(execute_action(k, DecisionRequest{internalize(1), DecisionOrigin::PublicAPI})
         == ApplyResult::Ok);
  assert(k.stats.decisions == 1 && k.stats.api_decisions == 1);
  assert(execute_action(k, DecisionRequest{internalize(-1)}) == ApplyResult::NoOp);
  assert(k.stats.decisions == 1 && k.stats.api_decisions == 1);
}

static void test_root_conflict_discharge(RestartPurpose purpose) {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_assign(k, internalize(-1), 0, UNIT_REASON, false);
  kernel_assign(k, internalize(-2), 0, UNIT_REASON, false);
  kernel_assign_decision(k, internalize(3));
  k.conflict_valid = true;
  k.conflict_ref = INVALID_REF;
  k.conflict_bin[0] = internalize(1);
  k.conflict_bin[1] = internalize(2);
  assert(execute_action(k, RestartRequest{0, purpose}) == ApplyResult::Unsat);
  assert(k.level == 0 && k.inconsistent && !k.conflict_valid);
  assert(k.stats.restarts == uint64_t(purpose == RestartPurpose::Scheduled));
  kernel_check_invariants(k);
}

int main() {
  for (const auto purpose : {RestartPurpose::Scheduled, RestartPurpose::Preparation}) {
    test_prefix_and_purpose(purpose);
    test_root_conflict_discharge(purpose);
  }
  test_decision_origin_and_no_change();
}
