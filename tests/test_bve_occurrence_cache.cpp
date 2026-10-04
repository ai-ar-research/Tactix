#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/lucky/affine_model.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/online_inprocess.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;
using namespace tactix::strategy;

static Ref clause(Kernel &k, int a, int b, int c) {
  const Lit lits[] = {internalize(a), internalize(b), internalize(c)};
  const Ref ref = static_cast<Ref>(k.arena.size());
  kernel_add_large_clause(k, lits, 3, false, 0, ++k.next_clause_id);
  return ref;
}

static Kernel fixture() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  clause(k, 1, 2, 3);
  clause(k, 4, -2, -3);
  assert(propagate_service(k) == ApplyResult::Ok);
  return k;
}

static OnlineStrategyProgram program() {
  OnlineInvocation step;
  step.id = 1;
  step.rule = "BVE";
  BveParameters p;
  p.candidate_scope = BveCandidateScope::PureOnly;
  step.parameters = p;
  step.work_unit = WorkUnit::BveTicks;
  step.post_application = PostApplication::Propagate;
  OnlineStrategyProgram result;
  result.sequence = {step};
  return result;
}

static void cache_lifetime_and_invalidation() {
  Kernel k = fixture();
  BveOccurrenceCache cache;
  OccIndex *first;
  {
    BveOccurrenceCache::Scope scope(cache, k);
    first = k.dense;
    assert(first && !first->track_order);
    clause(k, -1, 2, 4);
    assert(first->large_size(internalize(-1)) == 1);
    kernel_check_invariants(k);
  }
  assert(!k.dense && affine_model_guard(k));
  {
    BveOccurrenceCache::Scope scope(cache, k);
    assert(k.dense == first); // The image survives an atomic mutation.
    assert(k.dense->large_size(internalize(-1)) == 1);
  }
  // A copied executor/cache must not share a mutable occurrence image.
  BveOccurrenceCache copied = cache;
  {
    BveOccurrenceCache::Scope scope(copied, k);
    assert(k.dense != first);
    kernel_check_invariants(k);
  }
  // Detached images do not receive external insertions. A fresh scope must
  // reconstruct their full incidence lists, not merely reuse the old owner.
  clause(k, -4, 2, 3);
  {
    BveOccurrenceCache::Scope scope(cache, k);
    assert(k.dense->large_size(internalize(-4)) == 1);
    kernel_collect_garbage(k);
    kernel_check_invariants(k);
  }
  {
    BveOccurrenceCache::Scope scope(cache, k);
    assert(k.dense->large_size(internalize(-4)) == 1);
    kernel_check_invariants(k);
  }
  assert(!k.dense);
  Kernel other = fixture();
  {
    BveOccurrenceCache::Scope scope(cache, other);
    assert(other.dense->large_size(internalize(-4)) == 0);
  }
  assert(!other.dense);
  for (bool mixed : {false, true}) {
    OccIndex borrowed;
    kernel_dense_connect(k, borrowed, mixed);
    {
      BveOccurrenceCache::Scope scope(cache, k);
      assert(k.dense == &borrowed && k.dense->track_order == mixed);
    }
    assert(k.dense == &borrowed);
    kernel_dense_disconnect(k);
  }
}

static void online_rejects_retired_lucky() {
  Kernel k = fixture();
  OnlineStrategyExecutor executor;
  auto p = program();
  OnlineInvocation lucky;
  lucky.id = 2;
  lucky.rule = "Lucky";
  lucky.parameters = AffineModelParameters{};
  lucky.granularity = InvocationGranularity::OneInstance;
  lucky.instances = {8};
  p.sequence.push_back(lucky);
  std::string error;
  assert(!executor.configure(p, 1, ReconfigureMode::ResetRuntime, &error));
  assert(error.find("retired") != std::string::npos);
  assert(k.stats.bve_eliminated == 0 && k.stats.affine_model_calls == 0);
  assert(!k.dense && k.dense_order_scratch.empty());
  kernel_check_invariants(k);
}

static void external_changes_and_budget() {
  Kernel k = fixture();
  auto p = program();
  OnlineStrategyExecutor executor;
  assert(executor.configure(p, 1));
  assert(executor.advance(k).made_rule_progress);
  clause(k, -4, 2, 3);
  assert(executor.advance(k).status == InvocationStatus::StaleRequest);
  assert(!k.dense && !kernel_var_retired(k, 3));
  assert(executor.configure(p, 2, ReconfigureMode::RetainMatchingStepBudgets));
  while (!executor.complete()) {
    const auto result = executor.advance(k);
    assert(result.status == InvocationStatus::Completed ||
           result.status == InvocationStatus::NoChange);
    assert(!k.dense);
  }
  assert(!kernel_var_retired(k, 3)); // Newly mixed pivot is not pure.
  Kernel untouched = fixture();
  p.sequence[0].budget = WorkLimit::bounded(0);
  assert(executor.configure(p, 3));
  assert(executor.advance(untouched).status == InvocationStatus::BudgetExhausted);
  assert(!untouched.dense && untouched.dense_order_scratch.empty());
  assert(untouched.stats.bve_eliminated == 0);
  kernel_check_invariants(k);
  kernel_check_invariants(untouched);
}

static void redundant_fallback() {
  Kernel k = fixture();
  const Lit lits[] = {internalize(1), internalize(2), internalize(3)};
  const Ref red = static_cast<Ref>(k.arena.size());
  kernel_add_large_clause(k, lits, 3, true, 1, ++k.next_clause_id);
  BveOccurrenceCache cache;
  {
    BveOccurrenceCache::Scope scope(cache, k);
    assert(!k.dense);
  }
  OnlineStrategyExecutor executor;
  assert(executor.configure(program(), 1));
  assert(executor.advance(k).made_rule_progress);
  assert(clause_at(k.arena, red)->garbage());
  assert(!k.dense);
  kernel_check_invariants(k);
}

int main() {
  cache_lifetime_and_invalidation();
  online_rejects_retired_lucky();
  external_changes_and_budget();
  redundant_fallback();
  std::puts("BVE occurrence cache: lifetime, invalidation, borrowing and sparse boundaries passed");
}
