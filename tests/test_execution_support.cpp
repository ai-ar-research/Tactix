#undef NDEBUG
#include "policies/heuristics.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/execution_scope.hpp"
#include "strategy/representation_execution.hpp"

#include <cassert>

using namespace tactix;
using namespace tactix::execution;

template <ExecutionContext Context> static void test_work_and_phases() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 2);
  k.h->saved.assign(2, 1);
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  {
    ExecutionScope<Context> scope(k);
    kernel_assign_decision(k, internalize(-1));
    assert(propagate_apply(k) == ApplyResult::Ok);
    assert(k.value(internalize(-2)) > 0);
    assert(k.stats.ticks > 0);
    const bool search = Context == ExecutionContext::Speculation;
    assert((k.stats.search_ticks > 0) == search);
    const int phase = Context == ExecutionContext::Startup ? -1 : 1;
    assert(k.h->saved[0] == phase && k.h->saved[1] == phase);
  }
  assert(!k.inprocessing && !k.phase_saving_suppressed);
  kernel_check_invariants(k);
}

static void test_nested_scopes_and_unowned_wires() {
  for (bool work : {false, true})
    for (bool phases : {false, true}) {
      Kernel k;
      k.inprocessing = work;
      k.phase_saving_suppressed = phases;
      {
        ExecutionScope<ExecutionContext::Startup> startup(k);
        assert(k.inprocessing && k.phase_saving_suppressed == phases);
        {
          ExecutionScope<ExecutionContext::Speculation> speculation(k);
          assert(k.inprocessing && k.phase_saving_suppressed);
          {
            ExecutionScope<ExecutionContext::Inprocessing> inprocessing(k);
            assert(k.inprocessing && k.phase_saving_suppressed);
          }
          assert(k.inprocessing && k.phase_saving_suppressed);
        }
        assert(k.inprocessing && k.phase_saving_suppressed == phases);
      }
      assert(k.inprocessing == work && k.phase_saving_suppressed == phases);
    }
  Kernel k;
  {
    ExecutionScope<ExecutionContext::Speculation> speculation(k);
    k.inprocessing = true;
  }
  assert(k.inprocessing); // this context does not own the work wire
  {
    ExecutionScope<ExecutionContext::Startup> startup(k);
    k.phase_saving_suppressed = true;
  }
  assert(k.phase_saving_suppressed); // nor does startup own the phase wire
}

static void test_collection_outcomes() {
  for (unsigned fixed : {0u, 1u, 2u, 3u}) {
    Kernel k;
    kernel_reserve_vars(k, 3);
    const Lit clause[] = {internalize(1), internalize(2), internalize(3)};
    kernel_add_large_clause(k, clause, 3, false, 0, 10);
    for (unsigned i = 1; i <= fixed; ++i)
      kernel_assign_unit(k, internalize(-int(i)));
    const auto outcome = execute_action(k, SparseCollectRequest{});
    const RootCollectOutcome expected[] = {
        RootCollectOutcome::NoChange, RootCollectOutcome::Changed,
        RootCollectOutcome::Unit, RootCollectOutcome::Refuted};
    assert(outcome == expected[fixed]);
    if (fixed == 1) assert(kernel_binary_live(k, clause[1], clause[2]));
    if (fixed == 2) assert(k.value(clause[2]) > 0);
    if (fixed == 3) assert(k.inconsistent);
    kernel_check_invariants(k);
  }
}

static void test_materialization_and_collection_identity() {
  Kernel k;
  kernel_defer_cold_state(k);
  kernel_reserve_vars(k, 4);
  const Lit clause[] = {internalize(1), internalize(2), internalize(3)};
  const Ref garbage = kernel_add_large_clause(k, clause, 3, true, 2, 10);
  const Ref live = kernel_add_large_clause(k, clause, 3, false, 0, 11);
  kernel_delete_clause(k, clause_at(k.arena, garbage));
  assert(k.cold_state_deferred && k.seen.empty());
  execute_action(k, MaterializeColdStateRequest{});
  assert(!k.cold_state_deferred && k.seen.size() == k.nvars);
  const auto marks = k.stats.touch_marks;
  const auto generation = k.arena_generation;
  execute_action(k, MaterializeColdStateRequest{});
  assert(k.stats.touch_marks == marks && k.arena_generation == generation);
  assert(clause_at(k.arena, live)->id() == 11);
  execute_action(k, CollectGarbageRequest{});
  assert(k.arena_generation > generation && clause_at(k.arena, 0)->id() == 11);
  assert(k.arena.size() == clause_words(3));
  assert(!execute_action(k, CompactVariablesRequest{}));
  kernel_check_invariants(k);
}

int main() {
  test_work_and_phases<ExecutionContext::Startup>();
  test_work_and_phases<ExecutionContext::Inprocessing>();
  test_work_and_phases<ExecutionContext::Speculation>();
  test_nested_scopes_and_unowned_wires();
  test_collection_outcomes();
  test_materialization_and_collection_identity();
}
