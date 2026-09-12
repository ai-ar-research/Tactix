#undef NDEBUG
#include "kernel/kernel.hpp"
#include "frontend/parse.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/strengthen/strengthen.hpp"
#include "rules/instantiate/instantiate.hpp"
#include "rules/sweep/sweep.hpp"
#include "strategy/online_inprocess.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <string>
#include <unordered_set>

using namespace tactix;
using namespace tactix::strategy;

static Kernel forward_fixture() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  const Lit victim[] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, victim, 3, false, 0);
  return k;
}

static Kernel sweep_fixture() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  for (int zs = -1; zs <= 1; zs += 2)
    for (int ws = -1; ws <= 1; ws += 2) {
      const Lit forward[] = {internalize(-1), internalize(2),
                             internalize(3 * zs), internalize(4 * ws)};
      const Lit reverse[] = {internalize(1), internalize(-2),
                             internalize(3 * zs), internalize(4 * ws)};
      kernel_add_large_clause(k, forward, 4, false, 0);
      kernel_add_large_clause(k, reverse, 4, false, 0);
    }
  return k;
}

static void add3(Kernel &k, int a, int b, int c) {
  const Lit clause[] = {internalize(a), internalize(b), internalize(c)};
  kernel_add_large_clause(k, clause, 3, false, 0);
}

static void and_gate(Kernel &k, int output, int first, int second) {
  kernel_add_binary_clause(k, internalize(-output), internalize(first));
  kernel_add_binary_clause(k, internalize(-output), internalize(second));
  add3(k, output, -first, -second);
}

static void xor_gate(Kernel &k, int output, int first, int second) {
  add3(k, -output, first, second);
  add3(k, output, -first, second);
  add3(k, output, first, -second);
  add3(k, -output, -first, -second);
}

static void validation_is_transactional() {
  OnlineInprocessProgram good;
  good.sequence.push_back({
      .id = 1,
      .rule = "Strengthen",
      .parameters = StrengthenParameters{0, 0},
      .granularity = InvocationGranularity::CompleteCurrentPass,
      .instances = {},
      .factor_grids = {},
      .repeat = {RepeatMode::UntilScopedFixpoint, 0},
      .rule_work_budget = {},
      .material = {},
      .work_unit = WorkUnit::Applications,
      .budget = WorkLimit::bounded(8),
  });
  OnlineInprocessExecutor executor;
  std::string error;
  assert(executor.configure(good, 7, ReconfigureMode::ResetRuntime, &error));
  assert(executor.revision() == 7);

  OnlineInprocessProgram bad = good;
  bad.sequence[0].instances.push_back(17);
  assert(!executor.configure(bad, 8, ReconfigureMode::ResetRuntime, &error));
  assert(error == "complete pass carries explicit payloads");
  assert(executor.revision() == 7);

  bad = good;
  bad.sequence[0].parameters = BveParameters{};
  assert(!executor.configure(bad, 9, ReconfigureMode::ResetRuntime, &error));
  assert(error == "parameters do not match rule");
  assert(executor.revision() == 7);

  bad = good;
  bad.sequence[0].rule = "ConflictAnalysis";
  bad.sequence[0].parameters = AnalyzeParameters{};
  std::get<AnalyzeParameters>(bad.sequence[0].parameters).eager_subsume = 5;
  assert(!executor.configure(bad, 10, ReconfigureMode::ResetRuntime, &error));
  assert(error == "Analyze parameters are invalid");
  assert(executor.revision() == 7);

  bad = good;
  bad.sequence[0].granularity = InvocationGranularity::OneInstance;
  bad.sequence[0].instances = {INSTANCE_CANONICAL};
  bad.sequence[0].repeat = {RepeatMode::Once, 1};
  assert(!executor.configure(bad, 11, ReconfigureMode::ResetRuntime, &error));
  assert(error ==
         "explicit selection contains the canonical whole-rule payload");
  assert(executor.revision() == 7);
}

static void invalid_discriminants_do_not_replace_configuration() {
  OnlineInprocessProgram good;
  OnlineInvocation step;
  step.id = 1;
  step.rule = "Strengthen";
  good.sequence.push_back(step);
  OnlineInprocessExecutor executor;
  assert(executor.configure(good, 7));
  std::string error;
  auto bad = good;
  bad.repeat.mode = static_cast<RepeatMode>(255);
  assert(!executor.configure(bad, 8, ReconfigureMode::ResetRuntime, &error));
  assert(executor.revision() == 7);
  bad = good;
  bad.sequence[0].repeat.mode = static_cast<RepeatMode>(255);
  assert(!executor.configure(bad, 9, ReconfigureMode::ResetRuntime, &error));
  assert(executor.revision() == 7);
  bad = good;
  bad.sequence[0].granularity = static_cast<InvocationGranularity>(255);
  assert(!executor.configure(bad, 10, ReconfigureMode::ResetRuntime, &error));
  assert(executor.revision() == 7);
  assert(!executor.configure(good, 11, static_cast<ReconfigureMode>(255), &error));
  assert(executor.revision() == 7);
  bad = good;
  bad.sequence[0].work_unit = static_cast<WorkUnit>(255);
  assert(!executor.configure(bad, 12, ReconfigureMode::ResetRuntime, &error));
  assert(executor.revision() == 7);
  bad = good;
  bad.sequence[0].rule_work_budget.mode =
      static_cast<RuleWorkBudgetMode>(255);
  assert(!executor.configure(bad, 13, ReconfigureMode::ResetRuntime, &error));
  assert(executor.revision() == 7);
  bad = good;
  bad.sequence[0].rule_work_budget =
      {RuleWorkBudgetMode::Unlimited, 1};
  assert(!executor.configure(bad, 14, ReconfigureMode::ResetRuntime, &error));
  assert(executor.revision() == 7);
  bad = good;
  bad.sequence[0].material.touch_filter =
      static_cast<SettingOverride>(255);
  assert(!executor.configure(bad, 15, ReconfigureMode::ResetRuntime, &error));
  assert(executor.revision() == 7);
}

static void material_settings_are_per_call_and_scoped() {
  Kernel k = forward_fixture();
  k.touch_filter = true;
  k.touch_redundant = true;
  k.subsume_touch_mark = k.touch_epoch;

  OnlineInvocation step;
  step.id = 17;
  step.rule = "SubsumeForward";
  step.parameters = SubsumeForwardParameters{};
  step.granularity = InvocationGranularity::CanonicalApplication;
  step.material.touch_filter = SettingOverride::Disable;
  step.material.likely_kept_redundant = SettingOverride::Disable;
  OnlineStrategyProgram program;
  program.sequence.push_back(step);
  OnlineStrategyExecutor executor;
  assert(executor.configure(program, 17));
  const InvocationResult result = executor.advance(k);
  assert(result.applications == 1 && result.made_rule_progress &&
         "disabling canonical material filtering must expose old clauses");
  assert(k.touch_filter && k.touch_redundant &&
         "material overrides must restore the caller profile");
  kernel_check_invariants(k);
}

static void rule_work_budget_is_per_call_and_scoped() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  for (int output = 3; output <= 6; ++output)
    and_gate(k, output, 1, 2);
  k.stop_fuzz = 1000000000u; // poll each safe point without stopping
  k.work_budget = 77;
  k.work_budget_bounded = false;
  k.midround_budget = false;

  OnlineInvocation step;
  step.id = 15;
  step.rule = "Congruence";
  step.parameters = CongruenceParameters{};
  step.granularity = InvocationGranularity::CanonicalApplication;
  step.rule_work_budget = {RuleWorkBudgetMode::Bounded, 0};
  OnlineStrategyProgram program;
  program.sequence.push_back(step);
  OnlineStrategyExecutor executor;
  assert(executor.configure(program, 15));
  const InvocationResult result = executor.advance(k);
  assert(result.applications == 1);
  assert(k.stats.congruence_budget_stops == 1 &&
         "bounded zero must differ from unlimited at the first safe point");
  assert(k.work_budget == 77 && !k.work_budget_bounded &&
         !k.midround_budget && "the native allowance is call-scoped");
  kernel_check_invariants(k);

  Kernel walk;
  ScopedHeuristics heuristics(walk);
  kernel_reserve_vars(walk, 8);
  for (int first = 1; first <= 8; ++first)
    for (int second = first + 1; second <= 8; ++second)
      kernel_add_binary_clause(walk, internalize(-first),
                              internalize(-second));
  std::vector<Lit> cover;
  for (int variable = 1; variable <= 8; ++variable)
    cover.push_back(internalize(variable));
  kernel_add_large_clause(walk, cover.data(), cover.size(), false, 0);
  walk.work_budget = 77;

  step.id = 16;
  step.rule = "Walk";
  step.parameters = WalkParameters{100, 19};
  step.rule_work_budget = {RuleWorkBudgetMode::Bounded, 0};
  program.sequence = {step};
  assert(executor.configure(program, 16));
  const InvocationResult walked = executor.advance(walk);
  assert(walked.applications == 1 && walk.stats.walks == 1);
  assert(walk.stats.walk_steps == 0 &&
         "invocation allowance must override Walk's parameter budget");
  assert(walk.work_budget == 77 && !walk.work_budget_bounded);

  step.id = 17;
  step.parameters = WalkParameters{0, 23};
  step.rule_work_budget = {RuleWorkBudgetMode::Inherit, 0};
  program.sequence = {step};
  assert(executor.configure(program, 17));
  const uint64_t zero_before = walk.stats.walk_steps;
  const InvocationResult zero_walk = executor.advance(walk);
  assert(zero_walk.applications == 1 &&
         walk.stats.walk_steps == zero_before &&
         "a typed zero step budget must not fall back to the reference dose");
  assert(walk.work_budget == 77 && !walk.work_budget_bounded &&
         walk.walk_random_seed == 0 &&
         "the exact Walk parameter scope must restore amount, mode and seed");
  kernel_check_invariants(walk);
}

static void complete_pass_reaches_a_scoped_fixpoint() {
  Kernel k = forward_fixture();
  OnlineInprocessProgram program;
  program.sequence.push_back({
      .id = 11,
      .rule = "Strengthen",
      .parameters = StrengthenParameters{0, 0},
      .granularity = InvocationGranularity::CompleteCurrentPass,
      .instances = {},
      .factor_grids = {},
      .repeat = {RepeatMode::UntilScopedFixpoint, 0},
      .rule_work_budget = {},
      .material = {},
      .work_unit = WorkUnit::Applications,
      .budget = WorkLimit::bounded(8),
  });
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 3));
  const InvocationResult changed = executor.advance(k);
  assert(changed.applications == 1 && changed.made_rule_progress);
  assert(changed.configuration_revision == 3);
  const InvocationResult fixed = executor.advance(k);
  assert(fixed.applications == 0);
  assert(fixed.status == InvocationStatus::ScopedFixpoint);
  assert(fixed.selection_complete);
  assert(executor.step_consumed(11) == 1);
  kernel_check_invariants(k);
}

static void caller_selected_fixpoint_is_scoped_and_budgeted() {
  for (bool sequence_fixpoint : {false, true}) {
    for (auto granularity : {InvocationGranularity::OneInstance,
                             InvocationGranularity::SelectedBatch}) {
      for (uint64_t budget : {1u, 2u}) {
        Kernel k = forward_fixture();
        kernel_reserve_vars(k, 4);
        const Lit other[] = {internalize(1), internalize(2), internalize(4)};
        const Ref unselected = kernel_add_large_clause(k, other, 3, false, 0);
        OnlineInvocation step;
        step.id = 12;
        step.rule = "SubsumeForward";
        step.granularity = granularity;
        step.instances = {0};
        step.budget = WorkLimit::bounded(budget);
        if (!sequence_fixpoint)
          step.repeat = {RepeatMode::UntilScopedFixpoint, 0};
        OnlineInprocessProgram program;
        program.sequence.push_back(step);
        if (sequence_fixpoint)
          program.repeat = {RepeatMode::UntilScopedFixpoint, 0};
        OnlineInprocessExecutor executor;
        assert(executor.configure(program, 1));
        assert(executor.advance(k).made_rule_progress);
        const auto next = executor.advance(k);
        if (budget == 1) {
          assert(next.status == InvocationStatus::BudgetExhausted);
          assert(next.applications == 0 && !next.selection_complete);
        } else {
          assert(next.status == InvocationStatus::NoChange && next.applications == 1);
          const auto fixed = executor.advance(k);
          assert(fixed.status == InvocationStatus::ScopedFixpoint);
          assert(fixed.selection_complete && fixed.applications == 0);
        }
        assert(executor.step_consumed(12) == budget);
        // This is a fixpoint only over the caller's selection. Another legal
        // victim remains available, so global closure was neither run nor claimed.
        assert(!clause_at(k.arena, unselected)->garbage());
        kernel_check_invariants(k);
      }
    }
  }
}

static void budget_survives_compatible_online_reconfiguration() {
  Kernel k = forward_fixture();
  std::vector<Ref> candidates;
  strengthen_candidates(k, candidates, true);
  assert(candidates.size() == 1);
  const uint64_t selection =
      strengthen_literal_instance(candidates[0], INVALID_LIT);

  OnlineInprocessProgram program;
  program.sequence.push_back({
      .id = 21,
      .rule = "Strengthen",
      .parameters = StrengthenParameters{0, 0},
      .granularity = InvocationGranularity::SelectedBatch,
      .instances = {selection, selection},
      .factor_grids = {},
      .repeat = {RepeatMode::Once, 1},
      .rule_work_budget = {},
      .material = {},
      .work_unit = WorkUnit::Applications,
      .budget = WorkLimit::bounded(1),
  });
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 1));
  assert(executor.advance(k).applications == 1);
  InvocationResult exhausted = executor.advance(k);
  assert(exhausted.status == InvocationStatus::BudgetExhausted);
  assert(executor.step_consumed(21) == 1);

  program.sequence[0].budget = WorkLimit::bounded(2);
  assert(executor.configure(program, 2,
                            ReconfigureMode::RetainMatchingStepBudgets));
  assert(executor.step_consumed(21) == 1);
  const InvocationResult resumed = executor.advance(k);
  assert(resumed.applications == 1);
  assert(resumed.status == InvocationStatus::NoChange);
  assert(resumed.configuration_revision == 2);
  assert(executor.step_consumed(21) == 2);
  kernel_check_invariants(k);

  // StepId denotes one occurrence, not a transferable credit account. A
  // different rule with the same currency cannot inherit its consumption.
  program.sequence[0].rule = "Vivify";
  program.sequence[0].parameters = VivifyParameters{};
  program.sequence[0].granularity =
      InvocationGranularity::CompleteCurrentPass;
  program.sequence[0].instances.clear();
  assert(executor.configure(program, 3,
                            ReconfigureMode::RetainMatchingStepBudgets));
  assert(executor.step_consumed(21) == 0);
}

static void complete_pass_reaches_beyond_a_fixed_prefix() {
  Kernel k;
  kernel_reserve_vars(k, 22);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  for (int variable = 3; variable <= 22; ++variable) {
    const Lit victim[] = {internalize(1), internalize(2),
                          internalize(variable)};
    kernel_add_large_clause(k, victim, 3, false, 0);
  }
  OnlineInprocessProgram program;
  program.sequence.push_back({
      .id = 31,
      .rule = "SubsumeForward",
      .parameters = SubsumeForwardParameters{0, 0, 0},
      .granularity = InvocationGranularity::CompleteCurrentPass,
      .instances = {},
      .factor_grids = {},
      .repeat = {RepeatMode::Once, 1},
      .rule_work_budget = {},
      .material = {},
      .work_unit = WorkUnit::Applications,
      .budget = WorkLimit::bounded(20),
  });
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 4));
  unsigned changed = 0;
  std::unordered_set<uint64_t> applied;
  for (unsigned i = 0; i < 20; ++i) {
    const InvocationResult result = executor.advance(k);
    assert(result.applications == 1);
    assert(result.step_id == 31 && result.rule_id != UINT32_MAX);
    applied.insert(result.instance);
    changed += result.made_rule_progress;
  }
  assert(applied.size() == 20);
  assert(changed == 20);
  assert(executor.step_consumed(31) == 20);
  const InvocationResult done = executor.advance(k);
  assert(done.status == InvocationStatus::Completed);
  assert(done.selection_complete);
  kernel_check_invariants(k);
}

// Exercise both in-place large shrink and large-to-binary collapse while
// later selected offsets remain live. Counting attempts alone misses this bug.
static void selected_strengthening_preserves_pending_offsets() {
  for (unsigned size : {3u, 4u}) {
    Kernel k;
    kernel_reserve_vars(k, 23);
    kernel_add_binary_clause(k, internalize(-1), internalize(2));
    std::vector<uint64_t> victims;
    for (int variable = 4; variable <= 23; ++variable) {
      const Lit clause[] = {internalize(1), internalize(2),
                            internalize(variable), internalize(3)};
      victims.push_back(kernel_add_large_clause(k, clause, size, false, 0));
    }
    std::reverse(victims.begin(), victims.end());
    std::vector<uint64_t> selections;
    for (const uint64_t victim : victims)
      selections.push_back(strengthen_literal_instance(
          static_cast<Ref>(victim), internalize(1)));
    OnlineInvocation step;
    step.id = 32;
    step.rule = "Strengthen";
    step.parameters = StrengthenParameters{0, 0};
    step.granularity = InvocationGranularity::SelectedBatch;
    step.instances = selections;
    OnlineInprocessProgram program;
    program.sequence.push_back(step);
    OnlineInprocessExecutor executor;
    assert(executor.configure(program, 1));
    const auto generation = k.arena_generation;
    for (size_t i = 0; i < victims.size(); ++i) {
      const uint64_t victim = victims[i];
      const auto result = executor.advance(k);
      assert(result.applications == 1 && result.made_rule_progress);
      assert(result.instance == selections[i]);
      assert(k.arena_generation == generation);
      const Clause *clause = clause_at(k.arena, static_cast<Ref>(victim));
      if (size == 4)
        assert(!clause->garbage() && clause->size == 3);
      else
        assert(clause->garbage());
      kernel_check_invariants(k);
    }
    assert(k.stats.strengthened == 20);
    assert(executor.advance(k).status == InvocationStatus::Completed);
    kernel_collect_garbage(k);
    kernel_check_invariants(k);
  }
}

static void interruption_and_inapplicability_do_not_consume_scope() {
  Kernel k = forward_fixture();
  OnlineInprocessProgram program;
  program.sequence.push_back({
      .id = 41,
      .rule = "Strengthen",
      .parameters = StrengthenParameters{},
      .granularity = InvocationGranularity::CompleteCurrentPass,
      .instances = {},
      .factor_grids = {},
      .repeat = {RepeatMode::UntilScopedFixpoint, 0},
      .rule_work_budget = {},
      .material = {},
      .work_unit = WorkUnit::Applications,
      .budget = WorkLimit::unlimited(),
  });
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 5));
  volatile sig_atomic_t stop = 1;
  assert(executor.advance(k, &stop).status == InvocationStatus::Interrupted);
  assert(executor.step_index() == 0 && executor.step_consumed(41) == 0);
  stop = 0;
  k.level = 1;
  assert(executor.advance(k, &stop).status == InvocationStatus::Inapplicable);
  assert(executor.step_index() == 0 && !executor.complete());
  k.level = 0;
  assert(executor.advance(k, &stop).applications == 1);
}

static volatile sig_atomic_t empty_repeat_stop = 0;
static void stop_empty_repeat(int) { empty_repeat_stop = 1; }

static void empty_repeated_scopes_observe_stop() {
  Kernel k;
  OnlineInvocation step;
  step.id = 73;
  step.rule = "SubsumeForward";
  step.repeat = {RepeatMode::Count, UINT64_MAX};
  OnlineInprocessProgram program;
  program.sequence.push_back(step);
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 1));
  empty_repeat_stop = 0;
  k.stop = &empty_repeat_stop;
  const auto saved = std::signal(SIGALRM, stop_empty_repeat);
  assert(saved != SIG_ERR);
  alarm(1);
  const auto interrupted = executor.advance(k);
  alarm(0);
  std::signal(SIGALRM, saved);
  assert(interrupted.status == InvocationStatus::Interrupted);
  assert(interrupted.applications == 0 && !interrupted.selection_complete);
  assert(executor.step_consumed(73) == 0 && !executor.complete());
  k.stop = nullptr;
  program.sequence[0].repeat = {RepeatMode::Once, 1};
  assert(executor.configure(program, 2,
                            ReconfigureMode::RetainMatchingStepBudgets));
  assert(executor.advance(k).status == InvocationStatus::Completed);
}

static void pending_selection_rechecks_guard_and_kernel_stop() {
  Kernel k = forward_fixture();
  std::vector<Ref> candidates;
  strengthen_candidates(k, candidates, true);
  const uint64_t selection =
      strengthen_literal_instance(candidates[0], INVALID_LIT);
  OnlineInvocation step;
  step.id = 72;
  step.rule = "Strengthen";
  step.granularity = InvocationGranularity::SelectedBatch;
  step.instances = {selection, selection};
  OnlineInprocessProgram program;
  program.sequence.push_back(step);
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 1));
  assert(executor.advance(k).applications == 1);
  k.level = 1;
  const auto blocked = executor.advance(k);
  assert(blocked.status == InvocationStatus::Inapplicable);
  assert(blocked.applications == 0 && !blocked.selection_complete);
  assert(executor.step_consumed(72) == 1);
  k.level = 0;
  volatile sig_atomic_t stop = 1;
  k.stop = &stop;
  assert(executor.advance(k).status == InvocationStatus::Interrupted);
  assert(executor.step_consumed(72) == 1 && !executor.complete());
  stop = 0;
  assert(executor.advance(k).applications == 1);
  assert(executor.step_consumed(72) == 2);
  k.stop = nullptr;
  kernel_check_invariants(k);
}

static void pending_selection_rejects_collection_and_external_mutation() {
  for (bool collect : {false, true}) {
    Kernel k = forward_fixture();
    std::vector<Ref> candidates;
    strengthen_candidates(k, candidates, true);
    assert(candidates.size() == 1);
    const uint64_t selection =
        strengthen_literal_instance(candidates[0], INVALID_LIT);
    OnlineInvocation step;
    step.id = 71;
    step.rule = "Strengthen";
    step.parameters = StrengthenParameters{0, 0};
    step.granularity = InvocationGranularity::SelectedBatch;
    step.instances = {selection, selection};
    step.budget = WorkLimit::bounded(2);
    OnlineInprocessProgram program;
    program.sequence.push_back(step);
    OnlineInprocessExecutor executor;
    assert(executor.configure(program, 1));
    assert(executor.advance(k).applications == 1);
    if (collect)
      kernel_collect_garbage(k);
    else {
      const Lit added[] = {internalize(-1), internalize(-2), internalize(3)};
      kernel_add_large_clause(k, added, 3, false, 0);
    }
    const auto stale = executor.advance(k);
    assert(stale.status == InvocationStatus::StaleRequest);
    assert(stale.applications == 0 && !stale.selection_complete);
    assert(executor.step_consumed(71) == 1 && !executor.complete());
    // Staleness remains explicit until a caller supplies a fresh selection.
    assert(executor.advance(k).status == InvocationStatus::StaleRequest);
    program.sequence[0].granularity = InvocationGranularity::CompleteCurrentPass;
    program.sequence[0].instances.clear();
    assert(executor.configure(program, 2,
                              ReconfigureMode::RetainMatchingStepBudgets));
    assert(executor.step_consumed(71) == 1);
    assert(executor.advance(k).status != InvocationStatus::StaleRequest);
    kernel_check_invariants(k);
  }
}

static void semantic_instances_and_parameters_compose() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit candidate[] = {internalize(1), internalize(2), internalize(3),
                           internalize(6)};
  kernel_add_large_clause(k, candidate, 4, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(4));
  kernel_add_binary_clause(k, internalize(-1), internalize(-4));
  kernel_add_binary_clause(k, internalize(-2), internalize(5));
  kernel_add_binary_clause(k, internalize(-2), internalize(-5));
  const int id = tactix_rule_id("Instantiate");
  assert(id >= 0);
  std::vector<uint64_t> instances;
  assert(enumerate_complete_instances(k, tactix_rules()[id], instances) ==
         InvocationStatus::Completed);
  assert(instances.size() == 4);
  uint64_t selected = INSTANCE_CANONICAL;
  for (uint64_t instance : instances) {
    Ref ref;
    Lit literal;
    instantiate_pair_of_instance(instance, &ref, &literal);
    if (kernel_externalize(k, literal) == 2)
      selected = instance;
  }
  assert(selected != INSTANCE_CANONICAL);

  OnlineInprocessProgram program;
  program.sequence.push_back({
      .id = 51,
      .rule = "Instantiate",
      .parameters = InstantiateParameters{UINT32_MAX, 0, UINT32_MAX, false},
      .granularity = InvocationGranularity::OneInstance,
      .instances = {selected},
      .factor_grids = {},
      .repeat = {RepeatMode::Count, 2},
      .rule_work_budget = {},
      .material = {},
      .work_unit = WorkUnit::Applications,
      .budget = WorkLimit::bounded(2),
  });
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 6));
  const auto generation = k.arena_generation;
  const InvocationResult result = executor.advance(k);
  assert(result.status == InvocationStatus::Completed);
  assert(result.step_id == 51 && result.instance == selected);
  assert(result.made_rule_progress && result.applications == 1);
  assert(k.arena_generation != generation);
  // Repeating a completed pass cannot silently rebind old packed payloads.
  assert(executor.advance(k).status == InvocationStatus::StaleRequest);
  assert(executor.step_consumed(51) == 1 && !executor.complete());
  // Scoped parameters are restored even though this action bypasses them.
  assert(k.instantiate_parameters.minimum_clause_size ==
         InstantiateParameters::CLAUSE_SIZE_DEFAULT);
  bool found = false;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!clause->garbage() && clause->size == 3) {
      found = true;
      assert(std::find(clause->begin(), clause->end(), internalize(2)) ==
             clause->end());
      assert(std::find(clause->begin(), clause->end(), internalize(1)) !=
             clause->end());
    }
    ref += words;
  }
  assert(found);
  kernel_check_invariants(k);
}

static void congruence_semantic_actions_reach_online_fixpoint() {
  const auto build = [] {
    Kernel k;
    kernel_reserve_vars(k, 5);
    and_gate(k, 3, 1, 2);
    and_gate(k, 4, 1, 2);
    xor_gate(k, 5, 3, 4);
    return k;
  };
  const auto program = [](uint64_t budget) {
    OnlineInprocessProgram result;
    result.sequence.push_back({
        .id = 52,
        .rule = "Congruence",
        .parameters = CongruenceParameters{0, 0,
            CongruenceParameters::XOR_ARITY_CAPACITY},
        .granularity = InvocationGranularity::CompleteCurrentPass,
        .instances = {},
        .factor_grids = {},
        .repeat = {RepeatMode::UntilScopedFixpoint, 0},
        .rule_work_budget = {},
        .material = {},
        .work_unit = WorkUnit::Applications,
        .budget = WorkLimit::bounded(budget),
    });
    return result;
  };

  Kernel complete = build();
  OnlineInprocessExecutor executor;
  assert(executor.configure(program(4), 17));
  unsigned applications = 0, effective = 0;
  bool saw_fixpoint = false;
  InvocationResult last;
  for (unsigned guard = 0; guard < 8 && !executor.complete(); ++guard) {
    last = executor.advance(complete);
    applications += static_cast<unsigned>(last.applications);
    effective += last.made_rule_progress;
    saw_fixpoint |= last.status == InvocationStatus::ScopedFixpoint;
  }
  assert(executor.complete());
  assert(saw_fixpoint && last.status == InvocationStatus::Completed);
  assert(applications == 2 && effective == 2);
  assert(complete.stats.congruence_rounds == 0 &&
         "fine actions must not invoke the canonical closure");
  assert(complete.stats.congruence_merges == 1);
  assert(complete.stats.congruence_units == 1);
  assert(complete.value(internalize(-5)) > 0);
  kernel_check_invariants(complete);

  Kernel stopped = build();
  OnlineInprocessExecutor bounded;
  assert(bounded.configure(program(1), 18));
  const auto first = bounded.advance(stopped);
  assert(first.applications == 1 && first.made_rule_progress);
  const auto exhausted = bounded.advance(stopped);
  assert(exhausted.status == InvocationStatus::BudgetExhausted);
  assert(stopped.value(internalize(5)) == 0 &&
         "the one-action budget must stop before the exposed unit");
  assert(stopped.stats.congruence_rounds == 0);
  kernel_check_invariants(stopped);
}

static void complete_pass_reenumerates_after_its_own_collection() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit candidate[] = {internalize(1), internalize(2), internalize(3),
                           internalize(6)};
  kernel_add_large_clause(k, candidate, 4, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(4));
  kernel_add_binary_clause(k, internalize(-1), internalize(-4));
  kernel_add_binary_clause(k, internalize(-2), internalize(5));
  kernel_add_binary_clause(k, internalize(-2), internalize(-5));

  OnlineStrategyProgram program;
  program.sequence.push_back({
      .id = 52,
      .rule = "Instantiate",
      .parameters = InstantiateParameters{},
      .granularity = InvocationGranularity::CompleteCurrentPass,
      .instances = {},
      .factor_grids = {},
      .repeat = {RepeatMode::Once, 1},
      .rule_work_budget = {},
      .material = {},
      .work_unit = WorkUnit::Applications,
      .budget = WorkLimit::bounded(32),
  });
  OnlineStrategyExecutor executor;
  assert(executor.configure(program, 8));
  const uint64_t initial_generation = k.arena_generation;
  bool collected = false;
  for (unsigned advances = 0; advances < 33 && !executor.complete();
       ++advances) {
    const InvocationResult result = executor.advance(k);
    assert(result.status != InvocationStatus::StaleRequest);
    assert(result.status != InvocationStatus::ResourceExhausted);
    collected |= k.arena_generation != initial_generation;
    kernel_check_invariants(k);
  }
  assert(collected);
  assert(executor.complete());
}

static void sweep_parameters_are_exact_per_invocation() {
  Kernel selected = sweep_fixture();
  const int id = tactix_rule_id("Sweep");
  assert(id >= 0);
  std::vector<uint64_t> instances;
  assert(enumerate_complete_instances(selected, tactix_rules()[id],
                                      instances) ==
         InvocationStatus::Completed);
  assert(!instances.empty() && instances.front() != INSTANCE_CANONICAL);

  // A restrictive typed call must not leak into the next invocation.
  Kernel restricted = sweep_fixture();
  SweepParameters no_vars;
  no_vars.max_env_vars = 0;
  assert(sweep_try_with_limits(restricted,
                               static_cast<Var>(instances.front()), no_vars) ==
         ApplyResult::NoOp);

  OnlineInprocessProgram program;
  program.sequence.push_back({
      .id = 61,
      .rule = "Sweep",
      .parameters = SweepParameters{},
      .granularity = InvocationGranularity::OneInstance,
      .instances = {instances.front()},
      .factor_grids = {},
      .repeat = {RepeatMode::Once, 1},
      .rule_work_budget = {},
      .material = {},
      .work_unit = WorkUnit::Applications,
      .budget = WorkLimit::bounded(1),
  });
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 7));
  const InvocationResult result = executor.advance(selected);
  assert(result.applications == 1 && result.step_id == 61);
  assert(result.made_rule_progress);
  assert(selected.stats.sweep_environments > 0);
  kernel_check_invariants(selected);
}

static InvocationResult apply_parameterized(Kernel &k, const char *rule,
                                            ActionParameters parameters,
                                            StepId id) {
  OnlineStrategyProgram program;
  program.sequence.push_back({
      .id = id,
      .rule = rule,
      .parameters = std::move(parameters),
      .granularity = InvocationGranularity::CanonicalApplication,
      .instances = {},
      .factor_grids = {},
      .repeat = {RepeatMode::Once, 1},
      .rule_work_budget = {},
      .material = {},
      .work_unit = WorkUnit::Applications,
      .budget = WorkLimit::unlimited(),
  });
  OnlineStrategyExecutor executor;
  assert(executor.configure(program, id));
  return executor.advance(k);
}

static void rule_parameters_are_scoped_per_call() {
  Kernel k;
  ScopedHeuristics heuristics(k);
  kernel_reserve_vars(k, 3);

  k.bce_occ_limit = 31;
  assert(apply_parameterized(k, "BCE", BceParameters{7}, 101).applications ==
         1);
  assert(k.bce_occ_limit == 31);

  k.ternary_occ_limit = 32;
  k.ternary_max_add_percent = 33;
  assert(apply_parameterized(k, "HyperTernaryRes",
                             TernaryParameters{8, 9}, 102)
             .applications == 1);
  assert(k.ternary_occ_limit == 32 && k.ternary_max_add_percent == 33);

  k.factor_size = 34;
  k.factor_bound = 35;
  k.factor_cand_rounds = 36;
  k.factor_composite = false;
  k.factor_demote_fresh = true;
  assert(apply_parameterized(k, "Factor",
                             FactorParameters{4, 5, 6, true, false}, 103)
             .applications == 1);
  assert(k.factor_size == 34 && k.factor_bound == 35 &&
         k.factor_cand_rounds == 36 && !k.factor_composite &&
         k.factor_demote_fresh);

  k.transitive_early_exit = true;
  k.transitive_live_meter = false;
  k.transitive_budget_checked = true;
  assert(apply_parameterized(
             k, "TransitiveReduce",
             TransitiveReduceParameters{false, true, false}, 104)
             .applications == 1);
  assert(k.transitive_early_exit && !k.transitive_live_meter &&
         k.transitive_budget_checked);

  k.subst_demote = false;
  assert(apply_parameterized(k, "Substitute", SubstituteParameters{true}, 105)
             .applications == 1);
  assert(!k.subst_demote);

  k.tier1 = 3;
  k.tier2 = 8;
  assert(apply_parameterized(k, "Reduce", ReduceParameters{1, 4}, 106)
             .applications == 1);
  assert(k.tier1 == 3 && k.tier2 == 8);

  k.work_budget = 81;
  k.walk_random_seed = 82;
  assert(apply_parameterized(k, "Walk", WalkParameters{1, 2}, 108)
             .applications == 1);
  assert(k.work_budget == 81 && k.walk_random_seed == 82);

  k.reorder_maximum_clause_size = 83;
  assert(apply_parameterized(k, "Reorder", ReorderParameters{3}, 109)
             .applications == 1);
  assert(k.reorder_maximum_clause_size == 83);
  kernel_check_invariants(k);

  Kernel a;
  ScopedHeuristics analysis_heuristics(a);
  kernel_reserve_vars(a, 3);
  const Lit conflict[] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(a, conflict, 3, false, 0);
  assert(decide_apply(a, internalize(-1), true) == ApplyResult::Ok);
  assert(decide_apply(a, internalize(-2), true) == ApplyResult::Ok);
  assert(decide_apply(a, internalize(-3), true) == ApplyResult::Ok);
  assert(propagate_service(a) == ApplyResult::Conflict);
  a.analyze_parameters.minimize_depth_limit = 77;
  a.conflict_reuse = false;
  a.conflict_reuse_chrono_levels = 78;
  a.bump_reason_limit = 79;
  a.bump_reason_depth = 80;
  a.eager_subsume = 1;
  a.jump_reasons = false;
  a.jump_reasons_bigbig = true;
  a.shrink_glue_promote = false;
  a.no_ext_reason_bump = true;
  assert(apply_parameterized(
             a, "ConflictAnalysis",
             AnalyzeParameters{3, true, 4, 5, 6, 2, true, false, true, true},
             107)
             .applications == 1);
  assert(a.analyze_parameters.minimize_depth_limit == 77);
  assert(!a.conflict_reuse && a.conflict_reuse_chrono_levels == 78);
  assert(a.bump_reason_limit == 79 && a.bump_reason_depth == 80);
  assert(a.eager_subsume == 1);
  assert(!a.jump_reasons && a.jump_reasons_bigbig);
  assert(!a.shrink_glue_promote && a.no_ext_reason_bump);
  kernel_check_invariants(a);
}

static void exact_factor_grids_compose_as_a_selected_batch() {
  Kernel k;
  kernel_reserve_vars(k, 10);
  const auto add_grid = [&](int a, int b, int p, int q, int r) {
    FactorGrid grid;
    grid.factors = {internalize(a), internalize(b)};
    grid.remainders = {
        {internalize(p)}, {internalize(q)}, {internalize(r)}};
    for (const Lit factor : grid.factors)
      for (const std::vector<Lit> &remainder : grid.remainders)
        kernel_add_binary_clause(k, factor, remainder.front());
    return grid;
  };
  OnlineInvocation step;
  step.id = 61;
  step.rule = "Factor";
  step.granularity = InvocationGranularity::SelectedBatch;
  step.factor_grids.push_back(add_grid(1, 2, 3, 4, 5));
  step.factor_grids.push_back(add_grid(6, 7, 8, 9, 10));
  step.budget = WorkLimit::bounded(2);
  OnlineInprocessProgram program;
  program.sequence.push_back(step);
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 12));

  const InvocationResult first = executor.advance(k);
  assert(first.status == InvocationStatus::Completed && first.instance == 0);
  assert(first.applications == 1 && k.stats.factorings == 1);
  const InvocationResult second = executor.advance(k);
  assert(second.status == InvocationStatus::Completed && second.instance == 1);
  assert(second.applications == 1 && k.stats.factorings == 2);
  assert(k.strategy_epoch == 2);
  assert(executor.advance(k).status == InvocationStatus::Completed);
}

static Kernel complete_factor_fixture() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  for (const int factor : {1, 2})
    for (const int remainder : {3, 4, 5, 6})
      kernel_add_binary_clause(k, internalize(factor),
                              internalize(remainder));
  return k;
}

static void complete_factor_pass_uses_exact_grid_enumeration() {
  Kernel k = complete_factor_fixture();
  OnlineInvocation step;
  step.id = 62;
  step.rule = "Factor";
  step.granularity = InvocationGranularity::CompleteCurrentPass;
  step.budget = WorkLimit::unlimited();
  OnlineInprocessProgram program;
  program.sequence.push_back(step);
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 13));

  uint64_t applications = 0;
  uint64_t progress = 0;
  while (!executor.complete()) {
    const InvocationResult result = executor.advance(k);
    assert(result.status != InvocationStatus::ResourceExhausted);
    assert(result.status != InvocationStatus::StaleRequest);
    applications += result.applications;
    progress += result.made_rule_progress;
  }
  // K(2,4) contains four 2x3 subgrids and one 2x4 grid in one orientation,
  // four 3x2 subgrids and one 4x2 grid in the other. The first rewrite makes
  // the overlapping snapshot entries NoOp, but every legal entry is consumed.
  assert(applications == 10);
  assert(progress == 1 && k.stats.factorings == 1);
  assert(executor.step_consumed(62) == 10);
  kernel_check_invariants(k);

  Kernel bounded = complete_factor_fixture();
  step.id = 63;
  step.budget = WorkLimit::bounded(2);
  program.sequence = {step};
  OnlineInprocessExecutor limited;
  assert(limited.configure(program, 14));
  assert(limited.advance(bounded).applications == 1);
  assert(limited.advance(bounded).applications == 1);
  const InvocationResult exhausted = limited.advance(bounded);
  assert(exhausted.status == InvocationStatus::BudgetExhausted);
  assert(!exhausted.selection_complete && limited.step_consumed(63) == 2);
}

// Native-checker witness mode is run on SLURM. The normal micro-test entry
// below never launches a solver. Input includes exactly twenty selected victims
// containing variable 1, with an independent SAT/UNSAT component for the tail.
static int proof_witness(const char *input, const char *output,
                         const char *format, const char *rule_name) {
  Kernel k;
  Proof proof;
  assert(proof.open(output, std::string(format) == "lrat"
                               ? ProofFormat::Lrat : ProofFormat::DratAscii));
  k.proof = &proof;
  FILE *cnf = fopen(input, "r");
  assert(cnf && !parse_dimacs(cnf, k));
  fclose(cnf);
  OnlineInvocation step;
  step.id = 81;
  step.rule = rule_name;
  step.granularity = InvocationGranularity::SelectedBatch;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage() &&
        std::find(clause->begin(), clause->end(), internalize(1)) != clause->end())
      step.instances.push_back(ref);
    ref += clause_actual_words(clause);
  }
  assert(step.instances.size() == 20);
  std::reverse(step.instances.begin(), step.instances.end());
  OnlineInprocessProgram program;
  program.sequence.push_back(step);
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 1));
  for (unsigned i = 0; i < 20; ++i) {
    const auto result = executor.advance(k);
    assert(result.applications == 1 && result.made_rule_progress);
    kernel_check_invariants(k);
  }
  assert(executor.advance(k).status == InvocationStatus::Completed);
  std::puts("c effective selected transitions 20");
  DefaultPolicy policy;
  const int result = policy.solve(k, nullptr);
  proof.flush();
  if (result == 10) {
    kernel_extend_model(k);
    std::puts("s SATISFIABLE");
    std::printf("v");
    for (Var v = 0; v < kernel_num_output_vars(k); ++v) {
      const Lit lit = make_lit(v, false);
      const int external = static_cast<int>(v) + 1;
      std::printf(" %d", k.model_values[lit] > 0 ? external : -external);
    }
    std::puts(" 0");
  } else {
    assert(result == 20);
    std::puts("s UNSATISFIABLE");
  }
  return result;
}

int main(int argc, char **argv) {
  if (argc == 5)
    return proof_witness(argv[1], argv[2], argv[3], argv[4]);
  assert(argc == 1);
  validation_is_transactional();
  invalid_discriminants_do_not_replace_configuration();
  rule_work_budget_is_per_call_and_scoped();
  material_settings_are_per_call_and_scoped();
  complete_pass_reaches_a_scoped_fixpoint();
  caller_selected_fixpoint_is_scoped_and_budgeted();
  budget_survives_compatible_online_reconfiguration();
  complete_pass_reaches_beyond_a_fixed_prefix();
  selected_strengthening_preserves_pending_offsets();
  interruption_and_inapplicability_do_not_consume_scope();
  pending_selection_rejects_collection_and_external_mutation();
  pending_selection_rechecks_guard_and_kernel_stop();
  empty_repeated_scopes_observe_stop();
  semantic_instances_and_parameters_compose();
  congruence_semantic_actions_reach_online_fixpoint();
  exact_factor_grids_compose_as_a_selected_batch();
  complete_factor_pass_uses_exact_grid_enumeration();
  complete_pass_reenumerates_after_its_own_collection();
  sweep_parameters_are_exact_per_invocation();
  rule_parameters_are_scoped_per_call();
  std::puts("Online sequence, granularity, fixpoint, budget and reconfigure PASS");
}
