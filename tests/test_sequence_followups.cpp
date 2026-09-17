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

int main() {
  before_progress_admission();
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
