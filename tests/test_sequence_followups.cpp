#undef NDEBUG
#include "policies/default_policy.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/online_sequence.hpp"
#include <cassert>
#include <cstdio>

using namespace tactix;
using namespace tactix::strategy;

static Kernel fixture(bool rooted) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit clause[] = {internalize(-1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, clause, 3, false, 0, ++k.next_clause_id);
  // Together these binaries imply 1, making the explicit SR unit RUP.
  kernel_add_binary_clause(k, internalize(1), internalize(2), ++k.next_clause_id);
  kernel_add_binary_clause(k, internalize(1), internalize(-2), ++k.next_clause_id);
  if (rooted) kernel_assign_unit(k, internalize(1));
  assert(propagate_service(k) == ApplyResult::Ok);
  return k;
}

static OnlineInvocation cleanup(StepId id) {
  OnlineInvocation step;
  step.id = id;
  step.rule = "Reduce";
  ReduceParameters p;
  p.victim_numerator = 0;
  p.compact_collection = true;
  p.reduction_bookkeeping = false;
  step.parameters = p;
  step.granularity = InvocationGranularity::CanonicalApplication;
  return step;
}

static OnlineInvocation producer() {
  OnlineInvocation step;
  step.id = 1;
  step.rule = "SubstitutionRedundancy";
  step.parameters = SubstitutionRedundancyParameters{};
  step.granularity = InvocationGranularity::OneInstance;
  step.substitution_redundancy_instances = {{{internalize(1)}, {}, {}}};
  step.post_application = PostApplication::Propagate;
  return step;
}

static void finish(OnlineStrategySequence &s, Kernel &k) {
  unsigned calls = 0;
  while (!s.complete()) {
    assert(++calls < 100);
    const auto r = s.advance(k);
    assert(r.status == InvocationStatus::Completed ||
           r.status == InvocationStatus::ScopedFixpoint ||
           r.status == InvocationStatus::NoChange);
  }
  kernel_check_invariants(k);
}

static void before_progress_admission() {
  OnlineStrategyStage stage;
  stage.on_budget_exhaustion = BudgetExhaustionPolicy::Continue;
  stage.program.sequence = {producer(), cleanup(2)};
  stage.before_progress = {2};
  Kernel k = fixture(false);
  OnlineStrategySequence sequence;
  assert(sequence.configure(stage));
  assert(sequence.advance(k).made_rule_progress);
  volatile sig_atomic_t stop = 1;
  assert(sequence.advance(k, &stop).status == InvocationStatus::Interrupted);
  stop = 0;
  finish(sequence, k);
  assert(sequence.dispatches(1) == 0 && sequence.consumed(1) == 0);
  assert(!k.arena.empty());

  // A NoOp producer does not close admission. Cleanup itself may progress.
  k = fixture(true);
  assert(sequence.configure(stage));
  finish(sequence, k);
  assert(sequence.consumed(1) == 1 && k.arena.empty());

  // Own progress must not truncate an admitted batch, even across a stop.
  // A subsequent round is a new admission in the same progressed scope.
  auto batch = producer();
  batch.granularity = InvocationGranularity::SelectedBatch;
  batch.substitution_redundancy_instances.push_back(
      batch.substitution_redundancy_instances.front());
  stage.program.sequence = {batch};
  stage.program.repeat = {RepeatMode::UntilScopedFixpoint, 0};
  stage.before_progress = {1};
  k = fixture(false);
  assert(sequence.configure(stage));
  assert(sequence.advance(k).made_rule_progress);
  assert(sequence.consumed(0) == 1);
  stop = 1;
  assert(sequence.advance(k, &stop).status == InvocationStatus::Interrupted);
  stop = 0;
  finish(sequence, k);
  assert(sequence.consumed(0) == 2 && sequence.rounds() == 2);

  // Invalid reconfiguration preserves the old scope. A valid replacement
  // explicitly opens a new one while retaining spent application budgets.
  auto invalid = stage;
  invalid.before_progress = {1, 1};
  assert(!sequence.configure(invalid, 2, ReconfigureMode::RetainMatchingStepBudgets));
  assert(sequence.consumed(0) == 2);
  invalid.before_progress = {99};
  assert(!sequence.configure(invalid));
  invalid.before_progress = {1};
  invalid.progress_followups = {1};
  assert(!sequence.configure(invalid));
  assert(sequence.configure(stage, 3, ReconfigureMode::RetainMatchingStepBudgets));
  finish(sequence, k);
  assert(sequence.consumed(0) == 4);

  DefaultStrategyConfig config;
  config.inprocess.structured_root_stages = {stage};
  assert(config.valid());
  assert(config.serialize().find("before_progress=1") != std::string::npos);
  config.inprocess.structured_root_stages[0] = invalid;
  assert(!config.valid());

  // Clearing the annotation restores unconditional N1 action selection.
  stage.program.sequence = {producer(), cleanup(2)};
  stage.program.repeat = {RepeatMode::Once, 1};
  stage.before_progress.clear();
  k = fixture(false);
  assert(sequence.configure(stage));
  finish(sequence, k);
  assert(sequence.consumed(1) == 1 && k.arena.empty());
}

static void decorated_dispatch_preserves_sequence() {
  OnlineStrategyStage stage;
  stage.on_budget_exhaustion = BudgetExhaustionPolicy::Continue;
  stage.program.sequence = {producer(), cleanup(2), cleanup(3)};
  stage.progress_followups = {2, 3};
  Kernel original = fixture(false), decorated = fixture(false);
  OnlineStrategySequence first, second;
  assert(first.configure(stage) && second.configure(stage));
  std::vector<StepId> dispatched;
  auto dispatch = [&](const OnlineInvocation &step, auto &&apply) {
    dispatched.push_back(step.id);
    assert(step.rule == stage.program.sequence[step.id - 1].rule);
    if (step.id == 1) {
      assert(step.substitution_redundancy_instances.size() == 1);
      assert(step.substitution_redundancy_instances.front().clause ==
             std::vector<Lit>{internalize(1)});
    }
    return apply();
  };
  volatile sig_atomic_t stop = 1;
  assert(second.advance_with(decorated, dispatch, &stop).status == InvocationStatus::Interrupted);
  assert(dispatched.empty());
  stop = 0;
  unsigned calls = 0;
  while (!first.complete()) {
    assert(++calls < 20);
    const auto before = first.advance(original);
    const auto after = second.advance_with(decorated, dispatch);
    assert(before.status == after.status && before.step_id == after.step_id);
    assert(before.work_unit == after.work_unit && before.work == after.work);
    assert(before.applications == after.applications);
    assert(before.made_rule_progress == after.made_rule_progress);
    assert(before.selection_complete == after.selection_complete);
    assert(first.complete() == second.complete());
  }
  assert(dispatched.size() == second.dispatches(0) + second.dispatches(1));
  assert(second.dispatches(0) && second.dispatches(1) && !second.dispatches(2));
  assert(std::find(dispatched.begin(), dispatched.end(), 3) == dispatched.end());
  assert(original.trail.size() == decorated.trail.size());
  assert(std::equal(original.trail.begin(), original.trail.end(), decorated.trail.begin()));
  assert(original.arena.empty() && decorated.arena.empty());
  assert(original.stats.sr_applied == decorated.stats.sr_applied);
  kernel_check_invariants(original);
  kernel_check_invariants(decorated);
}

static OnlineInvocation graph_detection(StepId id, uint64_t vertices,
                                        uint64_t generators) {
  OnlineInvocation step;
  step.id = id;
  step.rule = "SubstitutionRedundancy";
  SubstitutionRedundancyParameters p;
  p.producer = SubstitutionRedundancyProducer::GraphSymmetry;
  p.graph_symmetry.maximum_graph_vertices = vertices;
  p.graph_symmetry.maximum_generators = generators;
  step.parameters = p;
  step.granularity = InvocationGranularity::CompleteCurrentPass;
  return step;
}

// satsuma.h:809: only symmetry-breaking output earns another pass. Formula
// progress of another kind (here a compaction) does not.
static void repeat_trigger_requires_symmetry_output() {
  for (const auto trigger : {RepeatTrigger::AnyProgress,
                             RepeatTrigger::SymmetryBreakingOutput}) {
    OnlineStrategyStage stage;
    stage.on_budget_exhaustion = BudgetExhaustionPolicy::Continue;
    stage.program.sequence = {cleanup(2)};
    stage.program.repeat = {RepeatMode::UntilScopedFixpoint, 2};
    stage.repeat_trigger = trigger;
    Kernel k = fixture(true);
    OnlineStrategySequence sequence;
    assert(sequence.configure(stage));
    unsigned calls = 0;
    while (!sequence.complete()) {
      assert(++calls < 100);
      sequence.advance(k);
    }
    kernel_check_invariants(k);
    assert(k.arena.empty());
    assert(sequence.rounds() ==
           (trigger == RepeatTrigger::AnyProgress ? 2u : 1u));
  }
  // A committed SR unit is symmetry-breaking output and earns a round.
  OnlineStrategyStage stage;
  stage.on_budget_exhaustion = BudgetExhaustionPolicy::Continue;
  stage.program.sequence = {producer()};
  stage.program.repeat = {RepeatMode::UntilScopedFixpoint, 0};
  stage.repeat_trigger = RepeatTrigger::SymmetryBreakingOutput;
  Kernel k = fixture(false);
  OnlineStrategySequence sequence;
  assert(sequence.configure(stage));
  finish(sequence, k);
  assert(k.stats.sr_applied == 1 && symmetry_breaking_outputs(k) == 1);
  assert(sequence.rounds() == 2);
}

// satsuma.h:194, 267, 810: a pass that reached a detection limit is the last,
// even when it also produced symmetry-breaking output.
static void detection_limit_stops_repetition() {
  for (const bool stop_at_limit : {false, true}) {
    OnlineStrategyStage stage;
    stage.on_budget_exhaustion = BudgetExhaustionPolicy::Continue;
    // Two literal vertices per variable exceed a one-vertex graph limit.
    stage.program.sequence = {graph_detection(3, 1, UINT64_MAX), producer()};
    stage.program.repeat = {RepeatMode::UntilScopedFixpoint, 0};
    stage.repeat_trigger = RepeatTrigger::SymmetryBreakingOutput;
    stage.stop_repeat_at_detection_limit = stop_at_limit;
    Kernel k = fixture(false);
    OnlineStrategySequence sequence;
    assert(sequence.configure(stage));
    unsigned calls = 0;
    InvocationResult last;
    while (!sequence.complete()) {
      assert(++calls < 100);
      last = sequence.advance(k);
    }
    assert(sequence.detection_limit_reached());
    assert(k.stats.sr_applied == 1);
    assert(sequence.rounds() == (stop_at_limit ? 1u : 2u));
    assert(last.status == InvocationStatus::BudgetExhausted);
    kernel_check_invariants(k);
  }
  // A caller-disabled producer completes without reaching a limit.
  OnlineStrategyStage stage;
  stage.on_budget_exhaustion = BudgetExhaustionPolicy::Continue;
  stage.program.sequence = {graph_detection(3, UINT64_MAX, 0), producer()};
  stage.program.repeat = {RepeatMode::UntilScopedFixpoint, 0};
  stage.repeat_trigger = RepeatTrigger::SymmetryBreakingOutput;
  stage.stop_repeat_at_detection_limit = true;
  Kernel k = fixture(false);
  OnlineStrategySequence sequence;
  assert(sequence.configure(stage));
  finish(sequence, k);
  assert(!sequence.detection_limit_reached() && sequence.rounds() == 2);
}

// The root scheduler's renewable progress window is not an owner limit: a
// generic detection that runs out of a window still below the cumulative hard
// ceiling must not end repetition (satsuma.h:810 reads only owner limits).
// The same stop at the hard ceiling is Satsuma's cost limit and does.
static void progress_window_is_not_a_detection_limit() {
  for (const bool window_below_hard : {true, false}) {
    OnlineStrategyStage stage;
    stage.on_budget_exhaustion = BudgetExhaustionPolicy::Continue;
    stage.program.sequence = {graph_detection(8, UINT64_MAX, UINT64_MAX),
                              producer()};
    stage.program.repeat = {RepeatMode::UntilScopedFixpoint, 0};
    stage.repeat_trigger = RepeatTrigger::SymmetryBreakingOutput;
    stage.stop_repeat_at_detection_limit = true;
    Kernel k = fixture(false);
    // Five ticks cannot pay the formula snapshot; a one-tick feedback quantum
    // cannot pay it either, so no graph-entry renewal is requested.
    Allowance window(WorkUnit::SrDiscoveryTicks, WorkLimit::bounded(5));
    const DiscoveryFeedbackPolicy feedback{1, window_below_hard ? UINT64_MAX : 5};
    OnlineStrategySequence sequence;
    assert(sequence.configure(stage));
    unsigned calls = 0;
    while (!sequence.complete()) {
      assert(++calls < 100);
      sequence.advance(k, nullptr, &window, feedback);
    }
    assert(k.stats.sr_applied == 1);
    assert(sequence.detection_limit_reached() == !window_below_hard);
    assert(sequence.rounds() == (window_below_hard ? 2u : 1u));
    kernel_check_invariants(k);
  }
}

// An identical generated detection is not paid again on an unchanged
// formula; any effective transition makes it current work again.
static void unchanged_detection_is_not_repeated() {
  UnchangedDetections record;
  OnlineStrategyStage stage;
  stage.on_budget_exhaustion = BudgetExhaustionPolicy::Continue;
  stage.program.sequence = {graph_detection(4, UINT64_MAX, 0),
                            graph_detection(5, UINT64_MAX, 0)};
  Kernel k = fixture(true);
  OnlineStrategySequence first;
  assert(first.configure(stage));
  first.share_unchanged_detections(&record);
  finish(first, k);
  assert(first.dispatches(0) == 1 && first.dispatches(1) == 0 &&
         first.unchanged_detection_skips() == 1);
  // Another sequence sharing the record inherits the completed detection.
  OnlineStrategySequence second;
  assert(second.configure(stage));
  second.share_unchanged_detections(&record);
  finish(second, k);
  assert(second.dispatches(0) == 0 && second.unchanged_detection_skips() == 2);
  // Effective output between identical detections makes the later one run.
  stage.program.sequence = {graph_detection(4, UINT64_MAX, 0), producer(),
                            graph_detection(5, UINT64_MAX, 0)};
  k = fixture(false);
  record.transition();
  OnlineStrategySequence third;
  assert(third.configure(stage));
  third.share_unchanged_detections(&record);
  finish(third, k);
  assert(third.dispatches(0) == 1 && third.dispatches(2) == 1 &&
         third.unchanged_detection_skips() == 0);
  // A detection that stopped at a limit is not repeated either: the same
  // formula and a no larger allowance stop it again.
  stage.program.sequence = {graph_detection(6, 1, UINT64_MAX),
                            graph_detection(7, 1, UINT64_MAX)};
  record.transition();
  OnlineStrategySequence fourth;
  assert(fourth.configure(stage));
  fourth.share_unchanged_detections(&record);
  unsigned calls = 0;
  while (!fourth.complete()) {
    assert(++calls < 100);
    fourth.advance(k);
  }
  assert(fourth.dispatches(0) == 1 && fourth.dispatches(1) == 0 &&
         fourth.unchanged_detection_skips() == 1 &&
         fourth.detection_limit_reached());
  // Without a shared record the ordinary N1 schedule runs every step.
  stage.program.sequence = {graph_detection(4, UINT64_MAX, 0),
                            graph_detection(5, UINT64_MAX, 0)};
  OnlineStrategySequence plain;
  assert(plain.configure(stage));
  finish(plain, k);
  assert(plain.dispatches(0) == 1 && plain.dispatches(1) == 1);
}

int main() {
  decorated_dispatch_preserves_sequence();
  before_progress_admission();
  repeat_trigger_requires_symmetry_output();
  detection_limit_stops_repetition();
  progress_window_is_not_a_detection_limit();
  unchanged_detection_is_not_repeated();
  OnlineStrategyStage stage;
  stage.on_budget_exhaustion = BudgetExhaustionPolicy::Continue;
  stage.program.sequence = {cleanup(2)};
  stage.progress_followups = {2};
  Kernel k = fixture(true);
  const std::vector<uint32_t> arena(k.arena.begin(), k.arena.end());
  const auto watches = k.watches.arena;
  const auto version = state_version(k);
  OnlineStrategySequence s;
  assert(s.configure(stage));
  volatile sig_atomic_t stop = 1;
  assert(s.advance(k, &stop).status == InvocationStatus::Interrupted);
  assert(s.dispatches(0) == 0);
  stop = 0;
  auto skipped = s.advance(k, &stop);
  assert(skipped.status == InvocationStatus::NoChange && skipped.step_id == 2);
  assert(!skipped.applications && !skipped.made_rule_progress && skipped.selection_complete);
  finish(s, k);
  assert(s.dispatches(0) == 0 && s.consumed(0) == 0);
  assert(std::vector<uint32_t>(k.arena.begin(), k.arena.end()) == arena);
  assert(k.watches.arena == watches && state_version(k).arena_generation == version.arena_generation);
  // An external change does not invent producer credit in this declared scope.
  assert(s.configure(stage));
  kernel_add_binary_clause(k, internalize(-2), internalize(3), ++k.next_clause_id);
  finish(s, k);
  assert(s.dispatches(0) == 0);
  // No followup annotation retains the ordinary explicit Reduce capability.
  stage.progress_followups.clear();
  assert(s.configure(stage));
  finish(s, k);
  assert(s.consumed(0) == 1 && k.arena.empty());

  stage.program.sequence = {producer(), cleanup(2), cleanup(3)};
  stage.progress_followups = {2, 3};
  k = fixture(false);
  assert(s.configure(stage));
  assert(s.advance(k).made_rule_progress);
  // Producer completion can itself be a separate boundary.
  while (s.dispatches(0) < 2) s.advance(k);
  stop = 1;
  assert(s.advance(k, &stop).status == InvocationStatus::Interrupted);
  assert(s.consumed(1) == 0);
  stop = 0;
  finish(s, k);
  assert(s.consumed(1) == 1 && s.dispatches(2) == 0);
  assert(k.arena.empty());

  // Followup roles are rule-neutral. A caller-selected SR batch must remain
  // admitted across an interrupt after its first (already implied) unit.
  auto batch = producer();
  batch.id = 2;
  batch.granularity = InvocationGranularity::SelectedBatch;
  batch.substitution_redundancy_instances.push_back(
      batch.substitution_redundancy_instances.front());
  stage.program.sequence = {producer(), batch, cleanup(3)};
  k = fixture(false);
  assert(s.configure(stage));
  unsigned calls = 0;
  while (!s.consumed(1)) { assert(++calls < 10); s.advance(k); }
  assert(s.consumed(1) == 1);
  stop = 1;
  assert(s.advance(k, &stop).status == InvocationStatus::Interrupted);
  assert(s.consumed(1) == 1);
  stop = 0;
  finish(s, k);
  assert(s.consumed(1) == 2 && s.dispatches(2) == 0);
  assert(!k.arena.empty());

  // Invalid replacement is transactional; valid replacement retains spent
  // applications but deliberately starts a fresh producer-credit scope.
  k = fixture(false);
  stage.program.sequence = {producer(), cleanup(2), cleanup(3)};
  stage.program.sequence[0].budget = WorkLimit::bounded(1);
  assert(s.configure(stage));
  assert(s.advance(k).made_rule_progress);
  auto invalid = stage;
  invalid.progress_followups = {2, 2};
  assert(!s.configure(invalid, 2, ReconfigureMode::RetainMatchingStepBudgets));
  assert(s.consumed(0) == 1);
  invalid.progress_followups = {99};
  assert(!s.configure(invalid));
  assert(s.configure(stage, 3, ReconfigureMode::RetainMatchingStepBudgets));
  assert(s.consumed(0) == 1);
  calls = 0;
  while (!s.complete()) {
    assert(++calls < 10);
    const auto result = s.advance(k);
    assert(result.status == InvocationStatus::BudgetExhausted ||
           result.status == InvocationStatus::NoChange ||
           result.status == InvocationStatus::Completed);
  }
  assert(s.consumed(0) == 1 && s.dispatches(1) == 0 &&
         s.dispatches(2) == 0 && !k.arena.empty());

  DefaultStrategyConfig config;
  config.inprocess.structured_root_stages = {stage};
  assert(config.valid());
  assert(config.serialize().find("progress_followups=2,3") != std::string::npos);
  config.inprocess.structured_root_stages[0].progress_followups = {99};
  assert(!config.valid());
  std::puts("sequence followups: neutral, effective, interruption, reconfiguration and N1 controls passed");
}
