#undef NDEBUG
#include "policies/heuristics.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/execution_scope.hpp"
#include "strategy/representation_execution.hpp"
#include "strategy/feedback_trace.hpp"

#include <cassert>

using namespace tactix;
using namespace tactix::execution;
using namespace tactix::strategy::controller;

struct SupportCapture final : FeedbackReceiver {
  std::vector<ActionFeedback> records;
  void after_action(const ActionFeedback &record) override { records.push_back(record); }
  void search_interval(const SearchIntervalFeedback &) override { assert(false); }
};

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
    std::string preserved;
    for (bool feedback : {false, true}) {
    Kernel k;
    kernel_reserve_vars(k, 3);
    const Lit clause[] = {internalize(1), internalize(2), internalize(3)};
    kernel_add_large_clause(k, clause, 3, false, 0, 10);
    for (unsigned i = 1; i <= fixed; ++i)
      kernel_assign_unit(k, internalize(-int(i)));
    SupportCapture capture;
    PolicyCompletion completion(k, feedback ? &capture : nullptr,
        [](const Kernel &kernel, const void *) { return observe_kernel(kernel); }, nullptr);
    completion.begin();
    const auto outcome = execute_action(k, SparseCollectRequest{}, feedback ? &completion : nullptr);
    const RootCollectOutcome expected[] = {
        RootCollectOutcome::NoChange, RootCollectOutcome::Changed,
        RootCollectOutcome::Unit, RootCollectOutcome::Refuted};
    assert(outcome == expected[fixed]);
    if (fixed == 1) assert(kernel_binary_live(k, clause[1], clause[2]));
    if (fixed == 2) assert(k.value(clause[2]) > 0);
    if (fixed == 3) assert(k.inconsistent);
    kernel_check_invariants(k);
    completion.finish(fixed == 3 ? 20 : 0, StopReason::Completed);
    const auto snapshot = serialize_observation(observe_kernel(k));
    if (!feedback) preserved = snapshot;
    else {
      assert(snapshot == preserved && capture.records.size() == 1);
      const auto &record = capture.records.front();
      assert(record.kind == FeedbackKind::Support && record.identity == "SparseCollect");
      assert(record.native_request->description == "start=0");
      assert(record.native_request->effort_currency == "ticks");
      assert(record.native_request->work == record.after.cumulative.ticks - record.before.cumulative.ticks);
      assert(record.after.cumulative.sparse_collections == record.before.cumulative.sparse_collections + 1);
      assert(!record.outcome.invocation.applications);
      assert(record.stop == (fixed == 3 ? StopReason::Unsat : fixed ? StopReason::Completed : StopReason::NoChange));
      assert(!record.outcome.invocation.made_rule_progress);
      assert(!record.outcome.invocation.work && !record.outcome.composition_overshoot);
      assert(record.after.cumulative.root_assignments ==
             record.before.cumulative.root_assignments + (fixed == 2 ? 1 : 0));
    }
    }
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
  SupportCapture capture;
  PolicyCompletion completion(k, &capture,
      [](const Kernel &kernel, const void *) { return observe_kernel(kernel); }, nullptr);
  completion.begin();
  execute_action(k, MaterializeColdStateRequest{}, &completion);
  assert(!k.cold_state_deferred && k.seen.size() == k.nvars);
  const auto marks = k.stats.touch_marks;
  const auto generation = k.arena_generation;
  execute_action(k, MaterializeColdStateRequest{}, &completion);
  assert(k.stats.touch_marks == marks && k.arena_generation == generation);
  assert(clause_at(k.arena, live)->id() == 11);
  execute_action(k, CollectGarbageRequest{});
  assert(k.arena_generation > generation && clause_at(k.arena, 0)->id() == 11);
  assert(k.arena.size() == clause_words(3));
  assert(!execute_action(k, CompactVariablesRequest{}));
  kernel_check_invariants(k);
  completion.finish(0, StopReason::Completed);
  assert(capture.records.size() == 2);
  assert(capture.records[0].stop == StopReason::Completed);
  assert(capture.records[1].stop == StopReason::NoChange);
  for (const auto &record : capture.records) {
    assert(record.identity == "MaterializeColdState");
    assert(!record.outcome.invocation.applications && !record.outcome.invocation.made_rule_progress);
    assert(record.native_request->effort_currency == "ticks" && !record.native_request->work);
  }
}

int main() {
  test_work_and_phases<ExecutionContext::Startup>();
  test_work_and_phases<ExecutionContext::Inprocessing>();
  test_work_and_phases<ExecutionContext::Speculation>();
  test_nested_scopes_and_unowned_wires();
  test_collection_outcomes();
  test_materialization_and_collection_identity();
}
