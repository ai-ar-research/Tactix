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
#include "strategy/online_sequence.hpp"

#include <cmath>
#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <fstream>
#include <iterator>
#include <unistd.h>
#include <string>
#include <unordered_set>

using namespace tactix;
using namespace tactix::strategy;

namespace tactix {
SubstitutionRedundancyApplicationResult
substitution_redundancy_apply_orbitopal_prefix(
    Kernel &, const SubstitutionRedundancyInstance &,
    const SubstitutionRedundancyParameters &, volatile sig_atomic_t *);
}

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
  bad = good;
  bad.sequence[0].post_application =
      static_cast<PostApplication>(255);
  assert(!executor.configure(bad, 11, ReconfigureMode::ResetRuntime, &error));
  assert(error == "post-application action is invalid");
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

static void budget_continuation_preserves_limits_and_stops() {
  OnlineInvocation limited;
  limited.id = 901;
  limited.rule = "SubstitutionRedundancy";
  SubstitutionRedundancyParameters sr;
  sr.producer = SubstitutionRedundancyProducer::Orbitopal;
  sr.maximum_detection_ticks = 1; // Zero disables; one forces an incomplete scan.
  limited.parameters = sr;
  OnlineInvocation later;
  later.id = 902;
  later.rule = "SubsumeForward";
  later.budget = WorkLimit::bounded(1);
  OnlineStrategyProgram program;
  program.sequence = {limited, later};
  program.repeat = {RepeatMode::UntilScopedFixpoint, 16};

  Kernel stopped = forward_fixture();
  OnlineStrategyExecutor original;
  assert(original.configure(program, 1));
  const auto halted = original.advance(stopped);
  assert(halted.status == InvocationStatus::BudgetExhausted);
  assert(original.step_consumed(later.id) == 0);

  Kernel k = forward_fixture();
  kernel_reserve_vars(k, 4);
  const Lit other[] = {internalize(1), internalize(2), internalize(4)};
  kernel_add_large_clause(k, other, 3, false, 0);
  OnlineStrategySequence continued;
  continued.configure(program, 73);
  volatile sig_atomic_t stop = 1;
  const auto interrupted = continued.advance(k, &stop);
  assert(interrupted.status == InvocationStatus::Interrupted &&
         interrupted.configuration_revision == 73);
  assert(continued.dispatches(0) == 0 && continued.dispatches(1) == 0);
  stop = 0;
  InvocationResult last;
  unsigned advances = 0;
  bool progressed = false;
  while (!continued.complete()) {
    assert(++advances < 30);
    last = continued.advance(k, &stop);
    assert(last.configuration_revision == 73);
    progressed |= last.made_rule_progress;
  }
  assert(progressed && continued.consumed(1) == 1);
  assert(continued.rounds() == 2);
  assert(last.status == InvocationStatus::BudgetExhausted);
  assert(!last.selection_complete);
  // One candidate remains: action credit was not replenished for round two.
  unsigned live = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    live += !clause->garbage();
    ref += clause_actual_words(clause);
  }
  assert(live == 1);
  kernel_check_invariants(k);

  // Explicit Count repetition cannot re-enter unchanged exhausted discovery.
  Kernel unchanged = forward_fixture();
  program.sequence = {limited};
  program.repeat = {RepeatMode::Count, 4};
  continued.configure(program, 74);
  while (!continued.complete()) {
    last = continued.advance(unchanged);
    assert(last.configuration_revision == 74);
  }
  assert(continued.dispatches(0) == 1 && continued.rounds() == 4);
  assert(last.status == InvocationStatus::BudgetExhausted);

  // A complete published AMO prefix is retained when a stop arrives; no later
  // producer runs until explicit resumption, and its action cap survives rounds.
  Kernel prefix;
  kernel_reserve_vars(prefix, 3);
  const Lit clause[] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(prefix, clause, 3, false, 0, ++prefix.next_clause_id);
  sr.maximum_detection_ticks = UINT64_MAX;
  sr.complete_unique_literal_amos = true;
  sr.maximum_orbitopal_instances = 1;
  sr.maximum_orbitopal_matrices = 0;
  limited.parameters = sr;
  limited.budget = WorkLimit::bounded(1);
  program.sequence = {limited, later};
  program.repeat = {RepeatMode::UntilScopedFixpoint, 3};
  continued.configure(program);
  assert(continued.advance(prefix).made_rule_progress);
  assert(prefix.stats.sr_applied == 1);
  stop = 1;
  assert(continued.advance(prefix, &stop).status == InvocationStatus::Interrupted);
  assert(continued.dispatches(1) == 0 && prefix.stats.sr_applied == 1);
  stop = 0;
  advances = 0;
  while (!continued.complete()) {
    assert(++advances < 30);
    last = continued.advance(prefix);
  }
  assert(continued.dispatches(1) > 0 && continued.consumed(0) == 1);
  assert(prefix.stats.sr_applied == 1);
  assert(last.status == InvocationStatus::BudgetExhausted);
  kernel_check_invariants(prefix);
}

static void shared_sr_discovery_budget_does_not_refill() {
  OnlineInvocation limited;
  limited.id = 903;
  limited.rule = "SubstitutionRedundancy";
  SubstitutionRedundancyParameters sr;
  sr.producer = SubstitutionRedundancyProducer::Orbitopal;
  sr.maximum_detection_ticks = UINT64_MAX;
  limited.parameters = sr;
  OnlineStrategyProgram program;
  program.sequence = {limited};

  Kernel k = forward_fixture();
  Allowance shared(WorkUnit::SrDiscoveryTicks, WorkLimit::bounded(1));
  OnlineStrategyExecutor first;
  assert(first.configure(program, 74));
  const InvocationResult spent = first.advance(k, nullptr, &shared);
  assert(spent.status == InvocationStatus::BudgetExhausted);
  assert(spent.budget_status == BudgetStatus::Exhausted);
  assert(!spent.discovery_exhausted &&
         "bounded orbitopal siblings must not earn producer-boundary credit");
  assert(shared.consumed() == 1 && !shared.accounting_overflow());

  OnlineStrategyExecutor later;
  assert(later.configure(program, 75));
  const InvocationResult refused = later.advance(k, nullptr, &shared);
  assert(refused.status == InvocationStatus::BudgetExhausted);
  assert(refused.budget_status == BudgetStatus::Exhausted);
  assert(shared.consumed() == 1 &&
         "a new executor must not replenish the enclosing allowance");

  Allowance wrong_currency(WorkUnit::Applications, WorkLimit::bounded(1));
  OnlineStrategyExecutor invalid;
  assert(invalid.configure(program, 76));
  const InvocationResult rejected =
      invalid.advance(k, nullptr, &wrong_currency);
  assert(rejected.status == InvocationStatus::AccountingError);
  assert(rejected.budget_status == BudgetStatus::CurrencyMismatch);
  assert(!wrong_currency.consumed());

  SubstitutionRedundancyParameters graph_parameters = sr;
  graph_parameters.producer = SubstitutionRedundancyProducer::GraphSymmetry;
  limited.parameters = graph_parameters;
  program.sequence = {limited};
  Allowance graph_pool(WorkUnit::SrDiscoveryTicks, WorkLimit::bounded(0));
  OnlineStrategyExecutor graph;
  assert(graph.configure(program, 77));
  const DiscoveryFeedbackPolicy feedback{100, 1000};
  const InvocationResult seed =
      graph.advance(k, nullptr, &graph_pool, feedback);
  assert(seed.status == InvocationStatus::BudgetExhausted &&
         seed.discovery_entry_required);
  graph_pool.set_limit(WorkLimit::bounded(100));
  const InvocationResult attempted =
      graph.advance(k, nullptr, &graph_pool, feedback);
  assert(!attempted.discovery_entry_required &&
         "a graph producer receives at most one entry seed per scope");

  Allowance too_small_pool(WorkUnit::SrDiscoveryTicks,
                           WorkLimit::bounded(0));
  OnlineStrategyExecutor too_small;
  assert(too_small.configure(program, 78));
  const InvocationResult skipped = too_small.advance(
      k, nullptr, &too_small_pool, DiscoveryFeedbackPolicy{1, 100});
  assert(!skipped.discovery_entry_required &&
         "a window below the exact binary snapshot lower bound cannot yield detector feedback");
}

static void finish_round_budget_policy_preserves_tail_and_resume() {
  for (const auto repeat : {RepeatSpec{RepeatMode::Once, 1},
                            RepeatSpec{RepeatMode::Count, 4},
                            RepeatSpec{RepeatMode::UntilScopedFixpoint, 0}}) {
    Kernel k = forward_fixture();
    kernel_reserve_vars(k, 4);
    const Lit extra[] = {internalize(1), internalize(2), internalize(4)};
    kernel_add_large_clause(k, extra, 3, false, 0);
    OnlineInvocation limited;
    limited.id = 911;
    limited.rule = "SubstitutionRedundancy";
    SubstitutionRedundancyParameters sr;
    sr.producer = SubstitutionRedundancyProducer::Orbitopal;
    sr.maximum_detection_ticks = 1;
    limited.parameters = sr;
    OnlineInvocation later;
    later.id = 912;
    later.rule = "SubsumeForward";
    later.budget = WorkLimit::bounded(1);
    OnlineInvocation tail;
    tail.id = 913;
    tail.rule = "SubstitutionRedundancy";
    tail.parameters = SubstitutionRedundancyParameters{};
    tail.granularity = InvocationGranularity::CanonicalApplication;
    OnlineStrategyProgram program;
    program.sequence = {limited, later, tail};
    program.repeat = repeat;
    OnlineStrategySequence sequence;
    assert(sequence.configure(program, 117, ReconfigureMode::ResetRuntime,
                              BudgetExhaustionPolicy::FinishRoundThenStop));
    assert(sequence.advance(k).status == InvocationStatus::BudgetExhausted);
    assert(!sequence.complete() && sequence.dispatches(1) == 0);
    volatile sig_atomic_t stop = 1;
    assert(sequence.advance(k, &stop).status == InvocationStatus::Interrupted);
    assert(!sequence.complete() && sequence.dispatches(1) == 0);
    stop = 0;
    InvocationResult last;
    unsigned advances = 0;
    bool progress = false;
    while (!sequence.complete()) {
      assert(++advances < 20);
      last = sequence.advance(k, &stop);
      progress |= last.made_rule_progress;
    }
    assert(progress && sequence.rounds() == 1 && sequence.dispatches(0) == 1);
    assert(sequence.dispatches(2) > 0 && sequence.consumed(1) == 1);
    assert(last.status == InvocationStatus::BudgetExhausted && !last.selection_complete);
    const auto dispatches = sequence.dispatches(1);
    assert(sequence.advance(k).status == InvocationStatus::BudgetExhausted);
    assert(sequence.dispatches(1) == dispatches);
    // Reconfigure onto remaining work with a deliberately increased allowance;
    // the completed-round latch resets, but the old credit remains spent.
    later.budget = WorkLimit::bounded(2);
    program.sequence = {later};
    program.repeat = {RepeatMode::Once, 1};
    assert(sequence.configure(program, 118, ReconfigureMode::RetainMatchingStepBudgets,
                              BudgetExhaustionPolicy::FinishRoundThenStop));
    assert(sequence.consumed(0) == 1);
    assert(sequence.advance(k).made_rule_progress && sequence.consumed(0) == 2);
    kernel_check_invariants(k);
  }
}

static void sequence_configuration_is_transactional() {
  Kernel k = forward_fixture();
  kernel_reserve_vars(k, 4);
  const Lit other[] = {internalize(1), internalize(2), internalize(4)};
  kernel_add_large_clause(k, other, 3, false, 0);
  OnlineInvocation work;
  work.id = 904;
  work.rule = "SubsumeForward";
  work.budget = WorkLimit::bounded(1);
  OnlineStrategyProgram program;
  program.sequence = {work};
  OnlineStrategySequence executor;
  assert(executor.advance(k).status == InvocationStatus::InvalidRequest);
  assert(executor.configure(program));
  assert(executor.advance(k).made_rule_progress);
  assert(executor.consumed(0) == 1);
  auto invalid = program;
  invalid.sequence.push_back(work);
  assert(!executor.configure(invalid));
  assert(executor.consumed(0) == 1);
  assert(executor.configure(program, 2, ReconfigureMode::RetainMatchingStepBudgets));
  assert(executor.advance(k).status == InvocationStatus::BudgetExhausted);
  assert(executor.consumed(0) == 1);
  // ResetRuntime is the caller's explicit authorization to replenish credit.
  assert(executor.configure(program, 3));
  assert(executor.advance(k).made_rule_progress);

  OnlineInvocation limited;
  limited.id = 905;
  limited.rule = "SubstitutionRedundancy";
  SubstitutionRedundancyParameters sr;
  sr.producer = SubstitutionRedundancyProducer::Orbitopal;
  sr.maximum_detection_ticks = 1;
  limited.parameters = sr;
  program.sequence = {limited, work};
  Kernel stopped = forward_fixture();
  assert(executor.configure(program, 4, ReconfigureMode::ResetRuntime,
                            BudgetExhaustionPolicy::Stop));
  assert(executor.advance(stopped).status == InvocationStatus::BudgetExhausted);
  assert(executor.advance(stopped).status == InvocationStatus::BudgetExhausted);
  assert(executor.dispatches(1) == 0);
  assert(executor.configure(program, 5, ReconfigureMode::RetainMatchingStepBudgets,
                            BudgetExhaustionPolicy::Continue));
  assert(executor.advance(stopped).status == InvocationStatus::BudgetExhausted);
  assert(executor.advance(stopped).made_rule_progress);

  // External mutation must invalidate no-progress retry suppression too.
  Kernel changed = forward_fixture();
  program.sequence = {limited};
  program.repeat = {RepeatMode::Count, 3};
  assert(executor.configure(program));
  assert(executor.advance(changed).status == InvocationStatus::BudgetExhausted);
  kernel_add_binary_clause(changed, internalize(-1), internalize(3));
  while (!executor.complete())
    (void)executor.advance(changed);
  assert(executor.dispatches(0) == 2);
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
  // The unrestricted invocation does what the restricted one could not:
  // builds the environment, proves the equivalence and commits it.  The
  // fixture's clauses carry no proof ids, which only LRAT would need.
  assert(selected.stats.sweep_equivalences > 0);
  assert(selected.stats.sweep_not_rup == 0);
  assert(result.made_rule_progress);
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

static void zero_victim_compaction_reports_semantic_progress() {
  for (unsigned kind = 0; kind < 3; ++kind) {
    Kernel k;
    kernel_reserve_vars(k, 4);
    const Lit first = internalize(1);
    if (kind == 0)
      kernel_add_binary_clause(k, first, internalize(2));
    else {
      const Lit clause[] = {kind == 1 ? first : negate(first),
                            internalize(2), internalize(3)};
      kernel_add_large_clause(k, clause, 3, false, 0);
    }
    kernel_assign_unit(k, first);
    assert(propagate_apply(k) == ApplyResult::Ok);
    ReduceParameters parameters;
    parameters.victim_numerator = 0;
    parameters.compact_collection = true;
    const auto result = apply_parameterized(k, "Reduce", parameters, 109);
    assert(result.rule_result == ApplyResult::Ok && result.made_rule_progress);
    assert(k.stats.deleted == 0); // No selected learned victim was needed.
    assert(k.live_irredundant_large == 0);
    if (kind == 2)
      assert(kernel_binary_live(k, internalize(2), internalize(3)));
    else
      assert(!kernel_binary_live(k, first, internalize(2)));
    const auto unchanged = apply_parameterized(k, "Reduce", parameters, 110);
    assert(unchanged.rule_result == ApplyResult::NoOp &&
           !unchanged.made_rule_progress);
    kernel_check_invariants(k);
  }
  // Material added after the previous propagation closure can become unit
  // or empty during collection. Exercise the collector's terminal outcomes,
  // not only Reduce's preparatory propagation path.
  for (bool refuted : {false, true}) {
    Kernel k;
    kernel_reserve_vars(k, 4);
    for (int variable = 1; variable <= 3; ++variable)
      kernel_assign_unit(k, internalize(-variable));
    assert(propagate_apply(k) == ApplyResult::Ok);
    const Lit clause[] = {internalize(1), internalize(2),
                          internalize(refuted ? 3 : 4)};
    kernel_add_large_clause(k, clause, 3, false, 0);
    ReduceParameters parameters;
    parameters.victim_numerator = 0;
    parameters.compact_collection = true;
    const auto result = apply_parameterized(k, "Reduce", parameters, 111);
    assert(result.rule_result ==
           (refuted ? ApplyResult::Unsat : ApplyResult::Ok));
    if (refuted)
      assert(result.status == InvocationStatus::Unsat && k.inconsistent);
    else
      assert(result.made_rule_progress && k.value(internalize(4)) > 0);
    kernel_check_invariants(k);
  }
  {
    Kernel k;
    kernel_reserve_vars(k, 2);
    kernel_add_binary_clause(k, internalize(1), internalize(2));
    kernel_assign_unit(k, internalize(1));
    assert(propagate_apply(k) == ApplyResult::Ok);
    ReduceParameters parameters;
    parameters.victim_numerator = 0;
    parameters.compact_collection = true;
    OnlineInvocation step;
    step.id = 112;
    step.rule = "Reduce";
    step.parameters = parameters;
    step.granularity = InvocationGranularity::CanonicalApplication;
    step.repeat = {RepeatMode::UntilScopedFixpoint, 0};
    OnlineStrategyProgram program;
    program.sequence.push_back(step);
    OnlineStrategyExecutor executor;
    assert(executor.configure(program, 112));
    unsigned applications = 0, effective = 0;
    for (unsigned attempts = 0; attempts < 4 && !executor.complete(); ++attempts) {
      const auto result = executor.advance(k);
      applications += result.applications;
      effective += result.made_rule_progress;
    }
    assert(executor.complete() && applications == 2 && effective == 1);
  }
}

static void collection_does_not_age_the_search_database() {
  for (bool bookkeeping : {false, true}) {
    Kernel k;
    kernel_reserve_vars(k, 5);
    const Lit survivor[] = {internalize(3), internalize(4), internalize(5)};
    const Ref ref = kernel_add_large_clause(k, survivor, 3, true, 4);
    clause_at(k.arena, ref)->set_used(2);
    k.kept_glue = 9;
    k.kept_size = 11;
    kernel_add_binary_clause(k, internalize(1), internalize(2));
    kernel_assign_unit(k, internalize(1));
    assert(propagate_apply(k) == ApplyResult::Ok);
    ReduceParameters p;
    p.victim_numerator = 0;
    p.compact_collection = true;
    p.reduction_bookkeeping = bookkeeping;
    const auto result = apply_parameterized(k, "Reduce", p, 906);
    assert(result.made_rule_progress && k.stats.deleted == 0);
    assert(k.stats.reductions == unsigned(bookkeeping));
    assert(clause_at(k.arena, 0)->used() == (bookkeeping ? 1u : 2u));
    if (!bookkeeping)
      assert(k.kept_glue == 9 && k.kept_size == 11);
    assert(k.reduce_bookkeeping); // per-call configuration restored
    kernel_check_invariants(k);
  }
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
  k.reduce_victim_numerator = 7;
  k.reduce_victim_denominator = 9;
  k.reduce_protect_tiers = true;
  k.reduce_compact_collection = false;
  assert(apply_parameterized(
             k, "Reduce", ReduceParameters{1, 4, 1, 2, false, true}, 106)
             .applications == 1);
  assert(k.tier1 == 3 && k.tier2 == 8 &&
         k.reduce_victim_numerator == 7 &&
         k.reduce_victim_denominator == 9 && k.reduce_protect_tiers &&
         !k.reduce_compact_collection);

  k.work_budget = 81;
  k.walk_random_seed = 82;
  k.walk_even_break_base_milli = 83;
  k.walk_score_curve = {{0, 101}, {1000, 202}};
  WalkParameters walk_parameters;
  walk_parameters.step_budget = 1;
  walk_parameters.random_seed = 2;
  walk_parameters.even_break_base_milli = 303;
  walk_parameters.score_curve = {{0, 404}, {2000, 505}};
  assert(apply_parameterized(k, "Walk", walk_parameters, 108)
             .applications == 1);
  assert(k.work_budget == 81 && k.walk_random_seed == 82 &&
         k.walk_even_break_base_milli == 83 &&
         (k.walk_score_curve ==
          std::vector<WalkParameters::ScorePoint>{{0, 101}, {1000, 202}}));

  k.reorder_maximum_clause_size = 84;
  k.reorder_clause_decay_milli = 85;
  k.reorder_binary_weight_milli = 86;
  k.reorder_balanced_polarity_milli = 87;
  assert(apply_parameterized(k, "Reorder",
                             ReorderParameters{3, 400, 900, 1700}, 109)
             .applications == 1);
  assert(k.reorder_maximum_clause_size == 84 &&
         k.reorder_clause_decay_milli == 85 &&
         k.reorder_binary_weight_milli == 86 &&
         k.reorder_balanced_polarity_milli == 87);
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
  // Each orientation contains six 2x2, four 2x3/3x2, and one full grid. The
  // first rewrite makes overlapping snapshot entries NoOp, but every legal
  // entry is consumed.
  assert(applications == 22);
  // The ascending 2x2 frontier first consumes columns {3,4}, then the
  // disjoint {5,6}; every overlapping selection is stale by application.
  assert(progress == 2 && k.stats.factorings == 2);
  assert(executor.step_consumed(62) == 22);
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

static void exact_sr_instance_uses_shared_online_composition() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const auto add_binary = [&](int a, int b) {
    kernel_add_binary_clause(k, internalize(a), internalize(b),
                             ++k.next_clause_id);
  };
  add_binary(1, 2);
  add_binary(3, 4);
  add_binary(-1, -3);
  add_binary(-2, -4);

  SubstitutionRedundancyInstance sr;
  sr.clause = {internalize(-1)};
  sr.witness = {internalize(-1), internalize(3)};
  sr.substitution = {{internalize(2), internalize(4)},
                     {internalize(4), internalize(2)}};
  OnlineInvocation step;
  step.id = 64;
  step.rule = "SubstitutionRedundancy";
  step.parameters = SubstitutionRedundancyParameters{};
  step.granularity = InvocationGranularity::OneInstance;
  step.substitution_redundancy_instances.push_back(sr);
  step.budget = WorkLimit::bounded(1);
  OnlineInprocessProgram program;
  program.sequence.push_back(step);
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 14));

  const InvocationResult applied = executor.advance(k);
  assert(applied.status == InvocationStatus::Completed);
  assert(applied.instance == 0 && applied.applications == 1);
  assert(k.value(internalize(-1)) > 0);
  assert(executor.advance(k).status == InvocationStatus::Completed);
  kernel_check_invariants(k);

  OnlineInprocessProgram invalid = program;
  invalid.sequence.front().rule = "Factor";
  std::string error;
  assert(!executor.configure(invalid, 15, ReconfigureMode::ResetRuntime,
                             &error));
  assert(error == "parameters do not match rule");
}

static void post_application_propagates_and_publishes_root_units() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const auto add_binary = [&](int a, int b) {
    kernel_add_binary_clause(k, internalize(a), internalize(b),
                             ++k.next_clause_id);
  };
  add_binary(1, 2);
  add_binary(3, 4);
  add_binary(-1, -3);
  add_binary(-2, -4);
  ScopedHeuristics heuristics(k);

  SubstitutionRedundancyInstance sr;
  sr.clause = {internalize(-1)};
  sr.witness = {internalize(-1), internalize(3)};
  sr.substitution = {{internalize(2), internalize(4)},
                     {internalize(4), internalize(2)}};
  OnlineInvocation step;
  step.id = 74;
  step.rule = "SubstitutionRedundancy";
  step.parameters = SubstitutionRedundancyParameters{};
  step.granularity = InvocationGranularity::OneInstance;
  step.substitution_redundancy_instances.push_back(sr);
  step.post_application = PostApplication::Propagate;
  OnlineInprocessProgram program;
  program.sequence.push_back(step);
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 25));

  const InvocationResult applied = executor.advance(k);
  assert(applied.status == InvocationStatus::Completed);
  assert(applied.applications == 1 && applied.made_rule_progress);
  assert(kernel_propagation_fixpoint(k));
  assert(k.value(internalize(-1)) > 0);
  assert(k.value(internalize(2)) > 0);
  assert(!heuristics_var_active(k, var_of(internalize(1))));
  assert(!heuristics_var_active(k, var_of(internalize(2))));
  // Publication and the composed Propagate happened before expected_state was
  // retained, so completing the program cannot spuriously report StaleRequest.
  assert(executor.advance(k).status == InvocationStatus::Completed);
  assert(executor.complete());
  kernel_check_invariants(k);
}

static void signed_sr_sources_emit_canonical_dsr_mappings() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const auto add_binary = [&](int a, int b) {
    kernel_add_binary_clause(k, internalize(a), internalize(b),
                             ++k.next_clause_id);
  };
  add_binary(1, 2);
  add_binary(3, 4);
  add_binary(-1, -3);
  add_binary(-2, -4);

  char path[] = "/tmp/tactix-sr-signed-source-XXXXXX";
  const int descriptor = mkstemp(path);
  assert(descriptor >= 0);
  close(descriptor);
  {
    Proof proof;
    assert(proof.open(path, ProofFormat::DsrAscii));
    proof.bind_variable_map(&k.internal_to_external);
    k.proof = &proof;
    SubstitutionRedundancyInstance instance;
    instance.clause = {internalize(-1)};
    instance.witness = {internalize(-1), internalize(3)};
    // (-2 -> -4) is the same variable substitution as (+2 -> +4).
    instance.substitution = {{internalize(-2), internalize(-4)},
                             {internalize(4), internalize(2)}};
    const auto result = substitution_redundancy_apply_detailed(k, instance);
    assert(result.status == SubstitutionRedundancyApplicationStatus::Applied);
    proof.flush();
    k.proof = nullptr;
  }
  FILE *file = fopen(path, "rb");
  assert(file);
  char buffer[128] = {};
  const size_t size = fread(buffer, 1, sizeof buffer - 1, file);
  fclose(file);
  unlink(path);
  // A root unit takes the serializer's general encoding (no witnessed line
  // may propagate); the canonical mapping is carried by its witnessed line.
  assert(std::string(buffer, size) ==
         "-5 -1 0\n5 -1 5 -1 3 5 2 4 4 2 0\n-1 0\nd 5 -1 0\nd -5 -1 0\n");
}

static void sr_failure_statuses_preserve_the_pending_application() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const auto add_binary = [&](int a, int b) {
    kernel_add_binary_clause(k, internalize(a), internalize(b),
                             ++k.next_clause_id);
  };
  add_binary(1, 2);
  add_binary(3, 4);
  add_binary(-1, -3);
  add_binary(-2, -4);

  SubstitutionRedundancyInstance sr;
  sr.clause = {internalize(-1)};
  sr.witness = {internalize(-1), internalize(3)};
  sr.substitution = {{internalize(2), internalize(4)},
                     {internalize(4), internalize(2)}};
  SubstitutionRedundancyParameters limited;
  limited.maximum_validation_ticks = 1;
  OnlineInvocation step;
  step.id = 66;
  step.rule = "SubstitutionRedundancy";
  step.parameters = limited;
  step.granularity = InvocationGranularity::OneInstance;
  step.substitution_redundancy_instances = {sr};
  step.budget = WorkLimit::bounded(1);
  OnlineInprocessProgram program;
  program.sequence = {step};
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 17));
  const StateVersion before = state_version(k);
  const InvocationResult exhausted = executor.advance(k);
  assert(exhausted.status == InvocationStatus::ResourceExhausted);
  assert(exhausted.applications == 0 && executor.step_consumed(66) == 0);
  assert(k.next_clause_id == 4 && state_version(k).arena_generation ==
                                      before.arena_generation);

  program.sequence.front().parameters = SubstitutionRedundancyParameters{};
  assert(executor.configure(program, 18,
                            ReconfigureMode::RetainMatchingStepBudgets));
  const InvocationResult resumed = executor.advance(k);
  assert(resumed.status == InvocationStatus::Completed);
  assert(resumed.applications == 1 && resumed.made_rule_progress);
  assert(k.value(internalize(-1)) > 0);
  kernel_check_invariants(k);

  Kernel malformed;
  kernel_reserve_vars(malformed, 1);
  OnlineInvocation bad = step;
  bad.id = 67;
  bad.parameters = SubstitutionRedundancyParameters{};
  bad.substitution_redundancy_instances = {
      SubstitutionRedundancyInstance{}};
  program.sequence = {bad};
  OnlineInprocessExecutor invalid;
  assert(invalid.configure(program, 19));
  const InvocationResult rejected = invalid.advance(malformed);
  assert(rejected.status == InvocationStatus::InvalidRequest);
  assert(rejected.applications == 0 && invalid.step_consumed(67) == 0);
}

static void orbitopal_complete_pass_discovers_structured_sr() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const auto add_binary = [&](int a, int b) {
    kernel_add_binary_clause(k, internalize(a), internalize(b),
                             ++k.next_clause_id);
  };
  add_binary(1, 2);
  add_binary(3, 4);
  add_binary(-1, -3);
  add_binary(-2, -4);

  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
  OnlineInvocation step;
  step.id = 65;
  step.rule = "SubstitutionRedundancy";
  step.parameters = parameters;
  step.granularity = InvocationGranularity::CompleteCurrentPass;
  step.budget = WorkLimit::unlimited();
  OnlineInprocessProgram program;
  program.sequence.push_back(step);
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 16));
  const InvocationResult result = executor.advance(k);
  assert(result.status == InvocationStatus::Completed);
  assert(result.made_rule_progress && result.instance == 0);
  assert(k.value(internalize(-1)) > 0);
  kernel_check_invariants(k);
}

static void orbitopal_screen_is_budgeted_and_interruptible() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2),
                           ++k.next_clause_id);
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
  parameters.maximum_detection_ticks = 1;
  uint64_t visited = 0;
  const auto limited = substitution_redundancy_enumerate_orbitopal(
      k, parameters,
      [&](const SubstitutionRedundancyInstance &) {
        ++visited;
        return true;
      });
  assert(limited.status == OrbitopalEnumerationStatus::LimitReached);
  assert(limited.ticks == parameters.maximum_detection_ticks && !visited);

  parameters.maximum_detection_ticks = UINT64_MAX;
  volatile sig_atomic_t stop = 1;
  const auto interrupted = substitution_redundancy_enumerate_orbitopal(
      k, parameters,
      [&](const SubstitutionRedundancyInstance &) {
        ++visited;
        return true;
      },
      &stop);
  assert(interrupted.status == OrbitopalEnumerationStatus::Interrupted);
  assert(!visited);
}

static void unique_literal_amo_phase_is_checked_bounded_and_composable() {
  Kernel enumerated;
  kernel_reserve_vars(enumerated, 3);
  const Lit source[] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(enumerated, source, 3, false, 0,
                          ++enumerated.next_clause_id);
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
  parameters.complete_unique_literal_amos = true;
  // AMO completion is an independently useful generated X7 phase.
  parameters.maximum_orbitopal_matrices = 0;
  std::vector<SubstitutionRedundancyInstance> instances;
  const OrbitopalEnumerationResult complete =
      substitution_redundancy_enumerate_orbitopal(
          enumerated, parameters,
          [&](const SubstitutionRedundancyInstance &instance) {
            instances.push_back(instance);
            return true;
          });
  assert(complete.status == OrbitopalEnumerationStatus::Complete);
  assert(complete.matrices == 0 && complete.instances == 3 &&
         instances.size() == 3);
  for (const auto &instance : instances)
    assert(instance.clause.size() == 2 && instance.witness.empty() &&
           instance.substitution.empty() &&
           instance.unique_literal_amo_certificate &&
           !instance.orbitopal_prefix_certificate);

  parameters.maximum_orbitopal_instances = 2;
  uint64_t bounded_instances = 0;
  const OrbitopalEnumerationResult bounded =
      substitution_redundancy_enumerate_orbitopal(
          enumerated, parameters,
          [&](const SubstitutionRedundancyInstance &) {
            ++bounded_instances;
            return true;
          });
  assert(bounded.status == OrbitopalEnumerationStatus::LimitReached &&
         bounded.instances == 2 && bounded_instances == 2);

  parameters.maximum_orbitopal_instances = UINT64_MAX;
  OnlineInvocation step;
  step.id = 651;
  step.rule = "SubstitutionRedundancy";
  step.parameters = parameters;
  step.granularity = InvocationGranularity::CompleteCurrentPass;
  step.repeat = {RepeatMode::UntilScopedFixpoint, 0};
  step.post_application = PostApplication::Propagate;
  step.budget = WorkLimit::unlimited();
  OnlineInprocessProgram program;
  program.sequence = {step};
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 160));
  const std::vector<uint64_t> touched_before = enumerated.touched;
  for (unsigned advances = 0; advances < 8 && !executor.complete();
       ++advances) {
    const InvocationResult result = executor.advance(enumerated);
    assert(result.status != InvocationStatus::InvalidRequest &&
           result.status != InvocationStatus::StaleRequest &&
           result.status != InvocationStatus::ResourceExhausted);
  }
  assert(executor.complete());
  assert(kernel_binary_live(enumerated, internalize(-1), internalize(-2)) &&
         kernel_binary_live(enumerated, internalize(-1), internalize(-3)) &&
         kernel_binary_live(enumerated, internalize(-2), internalize(-3)));
  assert(enumerated.stats.sr_applied == 3 &&
         enumerated.stats.sr_unique_literal_amo_binaries == 3 &&
         enumerated.stats.sr_validation_ticks == 0);
  // A preprocessor's completed AMO reaches the owned solver as input.  Its
  // in-process equivalent must therefore not masquerade as newly derived
  // material to incremental subsumption, Factor, or BVE scheduling.
  assert(enumerated.touched == touched_before);
  kernel_check_invariants(enumerated);

  // Producer provenance on caller-owned input never bypasses X7 validation.
  Kernel forged;
  kernel_reserve_vars(forged, 4);
  kernel_add_large_clause(forged, source, 3, false, 0,
                          ++forged.next_clause_id);
  const Lit second_source[] = {internalize(1), internalize(4)};
  kernel_add_binary_clause(forged, second_source[0], second_source[1],
                           ++forged.next_clause_id);
  SubstitutionRedundancyInstance invalid = instances.front();
  const auto rejected =
      substitution_redundancy_apply_detailed(forged, invalid, parameters);
  assert(rejected.status ==
         SubstitutionRedundancyApplicationStatus::Inapplicable);
  assert(rejected.validation_ticks > 0 && forged.stats.sr_applied == 0);
  assert(forged.stats.sr_unique_literal_amo_binaries == 0);

  // Root units are clauses too. A ULC pair certified before propagation may
  // have a now-false pivot; its pivot-only witness would falsify a live unit.
  // Fall back to full X7 validation rather than treating the certificate as
  // timeless or forbidding all requests with an assigned pivot.
  for (const bool rup : {false, true}) {
    Kernel root;
    kernel_reserve_vars(root, 4);
    kernel_add_large_clause(root, source, 3, false, 0,
                            ++root.next_clause_id);
    kernel_assign_unit(root, internalize(1));
    if (rup) {
      kernel_add_binary_clause(root, internalize(-2), internalize(4),
                               ++root.next_clause_id);
      kernel_add_binary_clause(root, internalize(-2), internalize(-4),
                               ++root.next_clause_id);
    }
    const auto result = substitution_redundancy_apply_orbitopal_prefix(
        root, instances.front(), parameters, nullptr);
    assert(result.validation_ticks > 0);
    assert(root.value(internalize(1)) > 0);
    if (rup) {
      assert(result.status == SubstitutionRedundancyApplicationStatus::Applied);
      assert(root.value(internalize(-2)) > 0);
    } else {
      assert(result.status == SubstitutionRedundancyApplicationStatus::Inapplicable);
      assert(root.stats.sr_applied == 0 && !root.value(internalize(2)));
      auto reversed = instances.front();
      std::reverse(reversed.clause.begin(), reversed.clause.end());
      const auto legal = substitution_redundancy_apply_orbitopal_prefix(
          root, reversed, parameters, nullptr);
      assert(legal.status == SubstitutionRedundancyApplicationStatus::Applied);
      assert(legal.validation_ticks == 0 && root.value(internalize(-2)) > 0);
    }
    kernel_check_invariants(root);
  }

  // Admission is fresh at each clause boundary.  Both clauses are ULCs in the
  // input snapshot, but completing the first adds binaries containing -1, so
  // the later clause containing -1 is no longer unique.  Reversing clause
  // order reverses which complete AMO is selected.
  const auto sequential_completion = [&](bool reverse) {
    Kernel ordered;
    kernel_reserve_vars(ordered, 5);
    const Lit positive[] = {internalize(1), internalize(2), internalize(3)};
    const Lit negative[] = {internalize(-1), internalize(4), internalize(5)};
    const auto add = [&](const Lit *clause) {
      kernel_add_large_clause(ordered, clause, 3, false, 0,
                              ++ordered.next_clause_id);
    };
    if (reverse) {
      add(negative);
      add(positive);
    } else {
      add(positive);
      add(negative);
    }
    std::vector<std::vector<Lit>> additions;
    const OrbitopalEnumerationResult result =
        substitution_redundancy_enumerate_orbitopal(
            ordered, parameters,
            [&](const SubstitutionRedundancyInstance &instance) {
              additions.push_back(instance.clause);
              return true;
            });
    assert(result.status == OrbitopalEnumerationStatus::Complete);
    assert(result.instances == 3 && additions.size() == 3);
    for (auto &addition : additions)
      std::sort(addition.begin(), addition.end());
    if (reverse) {
      assert(std::find(additions.begin(), additions.end(),
                       std::vector<Lit>{internalize(1), internalize(-4)}) !=
             additions.end());
    } else {
      assert(std::find(additions.begin(), additions.end(),
                       std::vector<Lit>{internalize(-1), internalize(-2)}) !=
             additions.end());
    }
  };
  sequential_completion(false);
  sequential_completion(true);
}

static Kernel symmetry_fixture() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const auto add_binary = [&](int first, int second) {
    kernel_add_binary_clause(k, internalize(first), internalize(second),
                             ++k.next_clause_id);
  };
  add_binary(1, 2);
  add_binary(3, 4);
  add_binary(-1, -3);
  add_binary(-2, -4);
  return k;
}

static void orbitopal_column_orientation_is_typed_and_effective() {
  Kernel k = symmetry_fixture();
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
  parameters.orbitopal_orientation = OrbitopalOrientation::Columns;
  std::vector<SubstitutionRedundancyInstance> instances;
  const OrbitopalEnumerationResult enumeration =
      substitution_redundancy_enumerate_orbitopal(
          k, parameters,
          [&](const SubstitutionRedundancyInstance &instance) {
            instances.push_back(instance);
            return true;
          });
  assert(enumeration.status == OrbitopalEnumerationStatus::Complete);
  assert(enumeration.matrices == 1 && instances.size() == 1);
  assert(instances.front().clause ==
         std::vector<Lit>{internalize(-1)});
  assert(instances.front().witness.back() == internalize(3));
  // Provenance attached to an explicit payload never bypasses the public
  // semantic validator.
  instances.front().orbitopal_prefix_certificate = true;
  const auto applied =
      substitution_redundancy_apply_detailed(k, instances.front(), parameters);
  assert(applied.status == SubstitutionRedundancyApplicationStatus::Applied);
  assert(applied.validation_ticks > 0);
  assert(k.value(internalize(-1)) > 0);
}

static Kernel orbitopal_product_fixture(bool complete = true) {
  Kernel k;
  kernel_reserve_vars(k, 9);
  const auto variable = [](unsigned row, unsigned column) {
    return static_cast<int>(3 * row + column + 1);
  };
  for (unsigned row = 0; row < 3; ++row) {
    Lit clause[3];
    for (unsigned column = 0; column < 3; ++column)
      clause[column] = internalize(variable(row, column));
    kernel_add_large_clause(k, clause, 3, false, 0, ++k.next_clause_id);
  }
  for (unsigned column = 0; column < 3; ++column) {
    if (!complete && column == 2)
      continue;
    for (unsigned first = 0; first < 3; ++first)
      for (unsigned second = first + 1; second < 3; ++second)
        kernel_add_binary_clause(
            k, internalize(-variable(first, column)),
            internalize(-variable(second, column)), ++k.next_clause_id);
  }
  return k;
}

static Kernel orbitopal_product_union_fixture() {
  Kernel k;
  kernel_reserve_vars(k, 18);
  const auto variable = [](unsigned component, unsigned row,
                           unsigned column) {
    return static_cast<int>(9 * component + 3 * row + column + 1);
  };
  for (unsigned component = 0; component < 2; ++component) {
    for (unsigned row = 0; row < 3; ++row) {
      Lit clause[3];
      for (unsigned column = 0; column < 3; ++column)
        clause[column] = internalize(variable(component, row, column));
      kernel_add_large_clause(k, clause, 3, false, 0, ++k.next_clause_id);
    }
    for (unsigned column = 0; column < 3; ++column)
      for (unsigned first = 0; first < 3; ++first)
        for (unsigned second = first + 1; second < 3; ++second)
          kernel_add_binary_clause(
              k, internalize(-variable(component, first, column)),
              internalize(-variable(component, second, column)),
              ++k.next_clause_id);
  }
  return k;
}

static void orbitopal_product_refinement_requires_exact_two_axis_action() {
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
  parameters.orbitopal_orientation = OrbitopalOrientation::Both;
  parameters.orbitopal_completion =
      OrbitopalCompletion::IndividualizedGraphCompletion;
  parameters.complete_unique_literal_amos = false;
  parameters.graph_symmetry.maximum_graph_vertices = 128;
  parameters.graph_symmetry.maximum_graph_edges = 256;

  Kernel symmetric = orbitopal_product_fixture();
  OrbitopalEnumerationResult result;
  std::vector<SubstitutionRedundancyInstance> instances;
  result = substitution_redundancy_enumerate_orbitopal(
      symmetric, parameters,
      [&](const SubstitutionRedundancyInstance &instance) {
        instances.push_back(instance);
        return true;
      });
  assert(result.status == OrbitopalEnumerationStatus::Complete);
  assert(result.matrices == 1 && instances.size() == 3);
  for (const auto &instance : instances)
    assert(substitution_redundancy_apply_detailed(symmetric, instance,
                                                  parameters)
               .status ==
           SubstitutionRedundancyApplicationStatus::Applied);
  assert(symmetric.stats.sr_applied == 3);
  kernel_check_invariants(symmetric);

  // Two indistinguishable product components share one root refinement cell.
  // Individualizing in the first leaves the untouched component as the
  // largest remainder; a pointwise-stabilizer descent must expose and certify
  // that nested product rather than requiring a root-level four-cell split.
  Kernel nested = orbitopal_product_union_fixture();
  std::vector<SubstitutionRedundancyInstance> nested_instances;
  const OrbitopalEnumerationResult nested_result =
      substitution_redundancy_enumerate_orbitopal(
          nested, parameters,
          [&](const SubstitutionRedundancyInstance &instance) {
            nested_instances.push_back(instance);
            return true;
          });
  assert(nested_result.status == OrbitopalEnumerationStatus::Complete);
  assert(nested_result.product_stabilizer_descents >= 1);
  assert(nested_result.product_shapes >= 1 &&
         nested_result.largest_product_rows == 3 &&
         nested_result.largest_product_columns == 3 &&
         nested_result.product_exact_actions >= 1 &&
         nested_result.product_ulc_actions >= 1);
  assert(nested_result.matrices == 1 && nested_instances.size() == 3);
  kernel_check_invariants(nested);

  Kernel bounded_nested = orbitopal_product_union_fixture();
  auto bounded_parameters = parameters;
  bounded_parameters.maximum_orbitopal_refinement_orbits = 1;
  uint64_t bounded_instances = 0;
  const OrbitopalEnumerationResult bounded_nested_result =
      substitution_redundancy_enumerate_orbitopal(
          bounded_nested, bounded_parameters,
          [&](const SubstitutionRedundancyInstance &) {
            ++bounded_instances;
            return true;
          });
  assert(bounded_nested_result.status ==
         OrbitopalEnumerationStatus::LimitReached);
  assert(bounded_nested_result.refinement_orbits == 1 &&
         bounded_nested_result.product_stabilizer_descents == 1 &&
         bounded_instances == 0);
  kernel_check_invariants(bounded_nested);

  Kernel bounded_reconstruction = orbitopal_product_union_fixture();
  bounded_parameters.maximum_orbitopal_refinement_orbits = 2;
  const OrbitopalEnumerationResult bounded_reconstruction_result =
      substitution_redundancy_enumerate_orbitopal(
          bounded_reconstruction, bounded_parameters,
          [&](const SubstitutionRedundancyInstance &) {
            assert(false && "bounded reconstruction must not emit");
            return false;
          });
  assert(bounded_reconstruction_result.status ==
         OrbitopalEnumerationStatus::LimitReached);
  assert(bounded_reconstruction_result.refinement_orbits == 2 &&
         bounded_reconstruction_result.product_shapes == 1 &&
         bounded_reconstruction_result.largest_product_rows == 3 &&
         bounded_reconstruction_result.largest_product_columns == 3 &&
         bounded_reconstruction_result.product_exact_actions == 0);
  kernel_check_invariants(bounded_reconstruction);

  Kernel asymmetric = orbitopal_product_fixture(false);
  OrbitopalEnumerationResult rejected;
  uint64_t rejected_instances = 0;
  rejected = substitution_redundancy_enumerate_orbitopal(
      asymmetric, parameters,
      [&](const SubstitutionRedundancyInstance &) {
        ++rejected_instances;
        return true;
      });
  assert(rejected.status == OrbitopalEnumerationStatus::Complete);
  assert(rejected.matrices == 0 && rejected_instances == 0);
  kernel_check_invariants(asymmetric);
}

static void orbitopal_product_extends_coupled_variables() {
  for (const int sign : {1, -1}) {
    const auto fixture = [&] {
      Kernel k;
      kernel_reserve_vars(k, 12);
      for (int row = 0; row < 3; ++row) {
        Lit clause[3];
        for (int column = 0; column < 3; ++column) {
          clause[column] = internalize(sign * (3 * row + column + 1));
          // A column swap must also swap its auxiliary variable. The
          // positive matrix literals still occur in exactly one source clause.
          kernel_add_binary_clause(k, negate(clause[column]),
              internalize(sign * (10 + column)), ++k.next_clause_id);
        }
        kernel_add_large_clause(k, clause, 3, false, 0, ++k.next_clause_id);
      }
      for (int column = 0; column < 3; ++column)
        for (int first = 0; first < 3; ++first)
          for (int second = first + 1; second < 3; ++second)
            kernel_add_binary_clause(k,
                internalize(-sign * (3 * first + column + 1)),
                internalize(-sign * (3 * second + column + 1)),
                ++k.next_clause_id);
      return k;
    };
    SubstitutionRedundancyParameters p;
    p.producer = SubstitutionRedundancyProducer::Orbitopal;
    p.orbitopal_orientation = OrbitopalOrientation::Both;
    p.orbitopal_completion = OrbitopalCompletion::IndividualizedGraphCompletion;
    p.orbitopal_discovery_order = OrbitopalDiscoveryOrder::RefinementOnly;
    p.complete_unique_literal_amos = false;
    p.orbitopal_generator_fallback = false;
    Kernel k = fixture();
    const auto pure = substitution_redundancy_enumerate_orbitopal(
        k, p, [](const SubstitutionRedundancyInstance &) { return true; });
    assert(pure.product_shapes && !pure.product_exact_actions);
    p.orbitopal_generator_fallback = true;
    std::vector<SubstitutionRedundancyInstance> instances;
    const auto extended = substitution_redundancy_enumerate_orbitopal(
        k, p, [&](const SubstitutionRedundancyInstance &instance) {
          instances.push_back(instance);
          return true;
        });
    assert(extended.status == OrbitopalEnumerationStatus::Complete);
    assert(extended.product_exact_actions && extended.generator_fallbacks);
    assert(!instances.empty());
    bool moves_auxiliary = false;
    for (const auto &instance : instances) {
      for (const auto &[source, target] : instance.substitution)
        moves_auxiliary |= var_of(source) >= 9 && source != target;
      assert(substitution_redundancy_apply_detailed(k, instance, p).status ==
             SubstitutionRedundancyApplicationStatus::Applied);
    }
    assert(moves_auxiliary && k.stats.sr_applied == instances.size());
    kernel_check_invariants(k);

    // Explicit coupled actions remain legal independently of the discovery
    // limits that found them. Replay, interrupt, budget and reconfigure on the
    // same live kernel, then compare with uninterrupted direct application.
    Kernel online = fixture();
    OnlineInvocation step;
    step.id = 703;
    step.rule = "SubstitutionRedundancy";
    auto explicit_parameters = p;
    explicit_parameters.producer = SubstitutionRedundancyProducer::ExplicitOnly;
    explicit_parameters.orbitopal_generator_fallback = false;
    explicit_parameters.graph_symmetry.maximum_generators = 0;
    step.parameters = explicit_parameters;
    step.granularity = InvocationGranularity::SelectedBatch;
    step.substitution_redundancy_instances = instances;
    step.budget = WorkLimit::bounded(1);
    OnlineStrategyProgram program;
    program.sequence = {step};
    OnlineStrategyProgram replay;
    assert(deserialize_online_strategy_program(
        serialize_online_strategy_program(program), replay));
    OnlineStrategyExecutor executor;
    assert(executor.configure(replay, 1));
    volatile sig_atomic_t online_stop = 1;
    const auto before_id = online.next_clause_id;
    const auto interrupted = executor.advance(online, &online_stop);
    assert(interrupted.status == InvocationStatus::Interrupted);
    assert(!interrupted.applications && !online.stats.sr_applied &&
           online.next_clause_id == before_id && !executor.step_consumed(703));
    online_stop = 0;
    const auto first = executor.advance(online, &online_stop);
    assert(first.applications == 1 && first.made_rule_progress);
    assert(executor.advance(online).status == InvocationStatus::BudgetExhausted);
    assert(executor.step_consumed(703) == 1);
    auto &remaining = replay.sequence.front();
    remaining.substitution_redundancy_instances.erase(
        remaining.substitution_redundancy_instances.begin());
    remaining.budget = WorkLimit::bounded(instances.size());
    OnlineStrategyProgram resumed;
    assert(deserialize_online_strategy_program(
        serialize_online_strategy_program(replay), resumed));
    assert(executor.configure(resumed, 2,
                             ReconfigureMode::RetainMatchingStepBudgets));
    assert(executor.step_consumed(703) == 1);
    for (size_t calls = 0; calls < instances.size() + 2 && !executor.complete();
         ++calls) {
      const auto result = executor.advance(online);
      assert(result.status != InvocationStatus::StaleRequest &&
             result.status != InvocationStatus::ResourceExhausted &&
             result.status != InvocationStatus::BudgetExhausted);
      kernel_check_invariants(online);
    }
    assert(executor.complete() && online.stats.sr_applied == instances.size());
    assert(executor.step_consumed(703) == instances.size());
    for (Var variable = 0; variable < k.nvars; ++variable)
      assert(online.value(make_lit(variable, false)) ==
             k.value(make_lit(variable, false)));

    Kernel stopped_kernel = fixture();
    volatile sig_atomic_t stop = 1;
    const auto stopped = substitution_redundancy_enumerate_orbitopal(
        stopped_kernel, p, [](const SubstitutionRedundancyInstance &) {
          assert(false && "interrupted discovery must not publish");
          return false;
        }, &stop);
    assert(stopped.status == OrbitopalEnumerationStatus::Interrupted);
    p.graph_symmetry.maximum_generators = 0;
    const auto bounded = substitution_redundancy_enumerate_orbitopal(
        stopped_kernel, p, [](const SubstitutionRedundancyInstance &) {
          assert(false && "incomplete product axes must not publish");
          return false;
        });
    assert(!bounded.product_exact_actions && !bounded.instances);
    kernel_check_invariants(stopped_kernel);
  }
}

static void orbitopal_certificate_rechecks_propagated_root_closure() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_assign_unit(k, internalize(-3));

  SubstitutionRedundancyInstance instance;
  instance.clause = {internalize(-1)};
  instance.witness = {internalize(-1), internalize(2)};
  instance.substitution = {{internalize(3), internalize(2)}};
  instance.orbitopal_prefix_certificate = true;

  SubstitutionRedundancyParameters parameters;
  const auto result = substitution_redundancy_apply_orbitopal_prefix(
      k, instance, parameters, nullptr);
  assert(result.status ==
         SubstitutionRedundancyApplicationStatus::Inapplicable);
  assert(result.validation_ticks > 0);
  assert(k.value(internalize(-1)) == 0);
}

static bool root_assignments_match(const Kernel &k,
                                   const std::array<int, 4> &model) {
  for (const int external : model) {
    const Lit literal = internalize(external);
    const int8_t value = k.value(literal);
    if (value < 0)
      return false;
  }
  return true;
}

static void orbitopal_certified_prefix_preserves_small_sat_fixture() {
  // Certified prefixes must retain a model of this two-model formula after
  // online reconfiguration and every generated pass. See the counterexample in
  // benchmarks/reports/2026-09-14-orbitopal-certified-claude-fuzz/terminal-report.md.
  const std::array<int, 4> first_model{-1, 2, 3, -4};
  const std::array<int, 4> second_model{1, -2, -3, 4};
  uint64_t productive_profiles = 0;
  for (const OrbitopalOrientation orientation :
       {OrbitopalOrientation::Rows, OrbitopalOrientation::Columns,
        OrbitopalOrientation::Both})
    for (const uint32_t seed : {0u, 740590720u}) {
      Kernel k = symmetry_fixture();
      SubstitutionRedundancyParameters parameters;
      parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
      parameters.orbitopal_orientation = orientation;
      parameters.orbitopal_completion =
          OrbitopalCompletion::IndividualizedGraphCompletion;
      parameters.graph_symmetry.seed = seed;
      parameters.graph_symmetry.error_bound = 20;
      parameters.graph_symmetry.fixing_mode =
          SymmetryFixingMode::GeneratorsOnly;
      parameters.graph_symmetry.decompose = false;
      parameters.graph_symmetry.strong_certification = true;
      parameters.graph_symmetry.clause_order =
          SymmetryTraversalOrder::Reverse;
      parameters.graph_symmetry.literal_order =
          SymmetryTraversalOrder::Seeded;
      parameters.graph_symmetry.clause_scope =
          SymmetryClauseScope::UniqueLiteralsOnly;
      parameters.graph_symmetry.pair_orientation =
          SymmetryPairOrientation::Seeded;

      OnlineInvocation step;
      step.id = 86;
      step.rule = "SubstitutionRedundancy";
      step.parameters = parameters;
      step.granularity = InvocationGranularity::CompleteCurrentPass;
      step.budget = WorkLimit::unlimited();
      OnlineStrategyProgram program;
      program.sequence = {step};
      OnlineStrategyExecutor executor;
      assert(executor.configure(program, 26));
      for (unsigned advances = 0; advances < 32 && !executor.complete();
           ++advances) {
        const InvocationResult result = executor.advance(k);
        assert(result.status != InvocationStatus::Unsat &&
               result.status != InvocationStatus::Conflict &&
               result.status != InvocationStatus::InvalidRequest &&
               result.status != InvocationStatus::StaleRequest &&
               result.status != InvocationStatus::ResourceExhausted);
      }
      assert(executor.complete());
      productive_profiles += k.stats.sr_applied != 0;
      assert(root_assignments_match(k, first_model) ||
             root_assignments_match(k, second_model));
      kernel_check_invariants(k);
    }
  assert(productive_profiles >= 2);
}

static void orbitopal_alignment_spans_connected_columns() {
  Kernel k;
  kernel_reserve_vars(k, 9);
  const auto add_binary = [&](int first, int second) {
    kernel_add_binary_clause(k, internalize(first), internalize(second),
                             ++k.next_clause_id);
  };
  const auto add_large = [&](std::initializer_list<int> external) {
    std::vector<Lit> clause;
    for (const int literal : external)
      clause.push_back(internalize(literal));
    kernel_add_large_clause(k, clause.data(), clause.size(), false, 0,
                            ++k.next_clause_id);
  };
  // Three ULC columns and row-preserving links A--B--C. Every row
  // transposition is a whole-formula automorphism, but neither two-column
  // prefix is: alignment has to compose through the middle column.
  add_large({1, 2, 3});
  add_large({4, 5, 6});
  add_large({7, 8, 9});
  add_binary(-1, -4);
  add_binary(-2, -5);
  add_binary(-3, -6);
  add_binary(-4, -7);
  add_binary(-5, -8);
  add_binary(-6, -9);

  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
  parameters.orbitopal_orientation = OrbitopalOrientation::Rows;
  std::vector<SubstitutionRedundancyInstance> instances;
  const auto enumeration = substitution_redundancy_enumerate_orbitopal(
      k, parameters,
      [&](const SubstitutionRedundancyInstance &instance) {
        instances.push_back(instance);
        return true;
      });
  assert(enumeration.status == OrbitopalEnumerationStatus::Complete);
  assert(enumeration.matrices == 1 && !instances.empty());
  const auto applied =
      substitution_redundancy_apply_detailed(k, instances.front(), parameters);
  assert(applied.status == SubstitutionRedundancyApplicationStatus::Applied);
  kernel_check_invariants(k);
}

static void orbitopal_row_group_certificate_rejects_missing_images() {
  const auto fixture = [](bool complete, bool outside_clauses) {
    Kernel k;
    kernel_reserve_vars(k, 8);
    const auto add_large = [&](std::initializer_list<int> external) {
      std::vector<Lit> clause;
      for (const int literal : external)
        clause.push_back(internalize(literal));
      kernel_add_large_clause(k, clause.data(), clause.size(), false, 0,
                              ++k.next_clause_id);
    };
    add_large({1, 2, 3});
    add_large({4, 5, 6});
    kernel_add_binary_clause(k, internalize(-1), internalize(-4),
                             ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(-2), internalize(-5),
                             ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(-3), internalize(-6),
                             ++k.next_clause_id);
    add_large({-1, 7, 8});
    add_large({-2, 7, 8});
    if (complete)
      add_large({-3, 7, 8});
    if (outside_clauses) {
      // These clauses are fixed by every candidate row action. The mixed
      // clauses above still require all three images: the outside-only
      // shortcut must not accidentally skip clauses containing both kinds.
      kernel_add_binary_clause(k, internalize(7), internalize(8),
                               ++k.next_clause_id);
      kernel_add_binary_clause(k, internalize(-7), internalize(-8),
                               ++k.next_clause_id);
    }
    return k;
  };
  const auto matrices = [](Kernel &k) {
    SubstitutionRedundancyParameters parameters;
    parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
    parameters.orbitopal_orientation = OrbitopalOrientation::Rows;
    uint64_t instances = 0;
    const auto enumeration = substitution_redundancy_enumerate_orbitopal(
        k, parameters,
        [&](const SubstitutionRedundancyInstance &) {
          ++instances;
          return true;
        });
    assert(enumeration.status == OrbitopalEnumerationStatus::Complete);
    kernel_check_invariants(k);
    return std::pair{enumeration.matrices, instances};
  };

  Kernel symmetric = fixture(true, false);
  // A duplicate physical clause is semantically redundant and must not hide
  // an otherwise complete row orbit.
  {
    std::vector<Lit> clause;
    for (const int literal : {-1, 7, 8})
      clause.push_back(internalize(literal));
    kernel_add_large_clause(symmetric, clause.data(), clause.size(), false, 0,
                            ++symmetric.next_clause_id);
  }
  const auto accepted = matrices(symmetric);
  assert(accepted.first == 1 && accepted.second != 0);

  // One missing image breaks set invariance even though the same unique
  // positive clause still identifies the candidate matrix.
  Kernel asymmetric = fixture(false, false);
  const auto rejected = matrices(asymmetric);
  assert(rejected.first == 0 && rejected.second == 0);

  Kernel symmetric_with_fixed = fixture(true, true);
  assert(matrices(symmetric_with_fixed) == accepted);
  Kernel asymmetric_with_fixed = fixture(false, true);
  assert(matrices(asymmetric_with_fixed) == rejected);
}

static void orbitopal_reuses_only_equivalent_failed_matrices() {
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
  parameters.orbitopal_orientation = OrbitopalOrientation::Rows;
  for (const bool negative : {false, true}) {
    Kernel k;
    kernel_reserve_vars(k, 37);
    const auto literal = [&](int variable) {
      return internalize(negative ? -variable : variable);
    };
    const auto binary = [&](int first, int second) {
      kernel_add_binary_clause(k, negate(literal(first)), negate(literal(second)),
                               ++k.next_clause_id);
    };
    const auto columns = [&](int first, unsigned count, unsigned rows) {
      for (unsigned column = 0; column < count; ++column) {
        std::vector<Lit> clause;
        for (unsigned row = 0; row < rows; ++row)
          clause.push_back(literal(first + column * rows + row));
        kernel_add_large_clause(k, clause.data(), clause.size(), false, 0,
                                ++k.next_clause_id);
        for (unsigned previous = 0; previous < column; ++previous)
          for (unsigned row = 0; row < rows; ++row)
            binary(first + previous * rows + row, first + column * rows + row);
      }
    };
    columns(1, 8, 3);
    binary(1, 25); // Outside variable breaks the first component's row action.
    columns(26, 3, 4); // Three columns avoid opposite-polarity binary ULCs.
    std::vector<SubstitutionRedundancyInstance> instances;
    const auto collect = [&](const SubstitutionRedundancyInstance &instance) {
      instances.push_back(instance);
      return true;
    };
    const auto first = substitution_redundancy_enumerate_orbitopal(
        k, parameters, collect);
    assert(first.status == OrbitopalEnumerationStatus::Complete);
    assert(first.reused_inexact_matrices == 7 && first.matrices == 1);
    assert(first.column_neighborhoods == 11);
    assert(first.reused_column_neighborhoods == 56);
    assert(!instances.empty());
    for (const auto &instance : instances)
      for (const Lit lit : instance.clause)
        assert(var_of(lit) >= 25);

    auto bounded = parameters;
    bounded.maximum_detection_ticks = first.ticks - 1;
    const auto limited = substitution_redundancy_enumerate_orbitopal(
        k, bounded, [](const SubstitutionRedundancyInstance &) { return true; });
    assert(limited.status == OrbitopalEnumerationStatus::LimitReached);
    assert(limited.reused_inexact_matrices == 7);
    assert(limited.ticks == bounded.maximum_detection_ticks);

    // Re-enter after a formula change: the former failed action is now legal.
    binary(2, 25);
    binary(3, 25);
    instances.clear();
    const auto repaired = substitution_redundancy_enumerate_orbitopal(
        k, parameters, collect);
    assert(repaired.status == OrbitopalEnumerationStatus::Complete);
    assert(repaired.matrices == 2 && !repaired.reused_inexact_matrices);
    assert(!instances.empty());
    const auto applied = substitution_redundancy_apply_detailed(
        k, instances.front(), parameters);
    assert(applied.status == SubstitutionRedundancyApplicationStatus::Applied);
    kernel_check_invariants(k);
  }

  // Inconsistent transport around a triangle changes the matrix with the
  // seed. Component membership alone must not cause a cached rejection.
  Kernel cycle;
  kernel_reserve_vars(cycle, 9);
  for (int column = 0; column < 3; ++column) {
    const Lit clause[] = {internalize(1 + 3 * column),
                          internalize(2 + 3 * column),
                          internalize(3 + 3 * column)};
    kernel_add_large_clause(cycle, clause, 3, false, 0, ++cycle.next_clause_id);
  }
  for (int row = 0; row < 3; ++row) {
    for (const auto &pair : {std::pair{1 + row, 4 + row},
                            std::pair{4 + row, 7 + row},
                            std::pair{1 + row, 7 + (row < 2 ? 1 - row : row)}})
      kernel_add_binary_clause(cycle, internalize(-pair.first),
                               internalize(-pair.second), ++cycle.next_clause_id);
  }
  uint64_t visited = 0;
  const auto distinct = substitution_redundancy_enumerate_orbitopal(
      cycle, parameters, [&](const SubstitutionRedundancyInstance &) {
        ++visited;
        return true;
      });
  assert(distinct.status == OrbitopalEnumerationStatus::Complete);
  assert(!visited && !distinct.reused_inexact_matrices);
  kernel_check_invariants(cycle);
}

static void orbitopal_column_transport_requires_a_bijection() {
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
  parameters.orbitopal_orientation = OrbitopalOrientation::Rows;
  for (const bool negative : {false, true}) {
    // Complete, duplicate physical edge, missing edge, ambiguous source,
    // and a total but noninjective relation, respectively.
    for (unsigned variant = 0; variant < 5; ++variant) {
      Kernel k;
      kernel_reserve_vars(k, 7);
      const auto literal = [&](int variable) {
        return internalize(negative ? -variable : variable);
      };
      const auto binary = [&](int first, int second) {
        kernel_add_binary_clause(k, negate(literal(first)), negate(literal(second)),
                                 ++k.next_clause_id);
      };
      for (int first : {1, 4}) {
        const Lit clause[] = {literal(first), literal(first + 1), literal(first + 2)};
        kernel_add_large_clause(k, clause, 3, false, 0, ++k.next_clause_id);
      }
      for (int variable = 1; variable <= 6; ++variable)
        binary(variable, 7); // Exclude negative binary ULC anchors uniformly.
      binary(1, 4);
      binary(2, variant == 4 ? 4 : 5);
      if (variant != 2)
        binary(3, 6);
      if (variant == 1)
        binary(1, 4);
      if (variant == 3)
        binary(1, 5);
      const auto result = substitution_redundancy_enumerate_orbitopal(
          k, parameters, [](const SubstitutionRedundancyInstance &) { return true; });
      assert(result.status == OrbitopalEnumerationStatus::Complete);
      assert(result.column_neighborhoods == 2);
      assert(result.column_transports == (variant < 2 ? 2 : 0));
      assert(result.matrices == (variant < 2 ? 1 : 0));

      volatile sig_atomic_t stop = 1;
      bool visited = false;
      const auto interrupted = substitution_redundancy_enumerate_orbitopal(
          k, parameters, [&](const SubstitutionRedundancyInstance &) {
            visited = true;
            return true;
          }, &stop);
      assert(interrupted.status == OrbitopalEnumerationStatus::Interrupted);
      assert(!visited && !interrupted.column_transports);
      kernel_check_invariants(k);
    }
  }
}

static Kernel clique_coloring_orbitopal_fixture() {
  Kernel k;
  constexpr unsigned vertices = 5, colors = 3;
  kernel_reserve_vars(k, vertices * colors);
  const auto variable = [](unsigned vertex, unsigned color) {
    return static_cast<int>(vertex * colors + color + 1);
  };
  const auto add_binary = [&](int first, int second) {
    kernel_add_binary_clause(k, internalize(first), internalize(second),
                             ++k.next_clause_id);
  };
  for (unsigned vertex = 0; vertex < vertices; ++vertex) {
    Lit atleast[colors];
    for (unsigned color = 0; color < colors; ++color)
      atleast[color] = internalize(variable(vertex, color));
    kernel_add_large_clause(k, atleast, colors, false, 0, ++k.next_clause_id);
    for (unsigned first = 0; first < colors; ++first)
      for (unsigned second = first + 1; second < colors; ++second)
        add_binary(-variable(vertex, first), -variable(vertex, second));
  }
  // Vertex zero is a canonical-order distractor. Vertices 1..4 form K4;
  // edge 0--1 keeps the column-alignment graph connected.
  constexpr unsigned edges[][2] = {
      {0, 1}, {1, 2}, {1, 3}, {1, 4}, {2, 3}, {2, 4}, {3, 4}};
  for (const auto &edge : edges)
    for (unsigned color = 0; color < colors; ++color)
      add_binary(-variable(edge[0], color), -variable(edge[1], color));
  return k;
}

static Kernel orbitopal_refinement_fixture() {
  Kernel k;
  kernel_reserve_vars(k, 9);
  const auto a = [](unsigned row) { return 1 + row; };
  const auto b = [](unsigned row) { return 4 + row; };
  const auto hidden = [](unsigned row) { return 7 + row; };
  Lit first[3], second[3];
  for (unsigned row = 0; row < 3; ++row) {
    first[row] = internalize(a(row));
    second[row] = internalize(b(row));
  }
  kernel_add_large_clause(k, first, 3, false, 0, ++k.next_clause_id);
  kernel_add_large_clause(k, second, 3, false, 0, ++k.next_clause_id);
  const auto add_binary = [&](int x, int y) {
    kernel_add_binary_clause(k, internalize(x), internalize(y));
  };
  for (unsigned row = 0; row < 3; ++row) {
    add_binary(-a(row), -b(row));
    add_binary(-a(row), hidden(row));
  }
  return k;
}

static Kernel orbitopal_single_seed_refinement_fixture() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const auto anchor = [](unsigned row) { return 1 + row; };
  const auto hidden = [](unsigned row) { return 4 + row; };
  Lit unique[3];
  for (unsigned row = 0; row < 3; ++row)
    unique[row] = internalize(anchor(row));
  kernel_add_large_clause(k, unique, 3, false, 0, ++k.next_clause_id);
  for (unsigned row = 0; row < 3; ++row)
    kernel_add_binary_clause(k, internalize(-anchor(row)),
                             internalize(hidden(row)), ++k.next_clause_id);
  return k;
}

static bool apply_all_orbitopal(Kernel &kernel,
                                SubstitutionRedundancyParameters parameters,
                                OrbitopalEnumerationResult *observed = nullptr,
                                std::vector<Lit> *candidates = nullptr) {
  std::vector<SubstitutionRedundancyInstance> instances;
  const auto enumeration = substitution_redundancy_enumerate_orbitopal(
      kernel, parameters,
      [&](const SubstitutionRedundancyInstance &instance) {
        instances.push_back(instance);
        return true;
      });
  if (observed)
    *observed = enumeration;
  for (const auto &instance : instances) {
    if (candidates)
      candidates->push_back(instance.clause.front());
    const auto applied = substitution_redundancy_apply_detailed(
        kernel, instance, parameters);
    if (applied.status == SubstitutionRedundancyApplicationStatus::Unsat)
      return true;
  }
  return false;
}

static void orbitopal_clique_order_is_typed_exact_and_budgeted() {
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
  parameters.orbitopal_orientation = OrbitopalOrientation::Rows;
  Kernel canonical = clique_coloring_orbitopal_fixture();
  OrbitopalEnumerationResult canonical_result;
  std::vector<Lit> canonical_candidates;
  assert(!apply_all_orbitopal(canonical, parameters, &canonical_result,
                              &canonical_candidates));
  assert(canonical_result.status == OrbitopalEnumerationStatus::Complete &&
         canonical_result.matrices == 1);
  assert(canonical_candidates.front() == internalize(-1));

  parameters.orbitopal_column_order =
      OrbitopalColumnOrder::CooccurrenceMaximumClique;
  parameters.maximum_orbitopal_clique_vertices = 5;
  parameters.maximum_orbitopal_clique_ticks = 100'000;
  Kernel clique = clique_coloring_orbitopal_fixture();
  OrbitopalEnumerationResult clique_result;
  std::vector<Lit> clique_candidates;
  assert(!apply_all_orbitopal(clique, parameters, &clique_result,
                              &clique_candidates));
  assert(clique_result.status == OrbitopalEnumerationStatus::Complete &&
         clique_candidates.front() == internalize(-4));

  parameters.minimum_orbitopal_clique_columns = 6;
  Kernel skipped = clique_coloring_orbitopal_fixture();
  OrbitopalEnumerationResult skipped_result;
  std::vector<Lit> skipped_candidates;
  assert(!apply_all_orbitopal(skipped, parameters, &skipped_result,
                              &skipped_candidates));
  assert(skipped_result.status == OrbitopalEnumerationStatus::Complete &&
         skipped_candidates.front() == internalize(-1));

  parameters.minimum_orbitopal_clique_columns = 5;
  parameters.maximum_orbitopal_clique_ticks = 0;
  Kernel bounded = clique_coloring_orbitopal_fixture();
  OrbitopalEnumerationResult bounded_result;
  assert(!apply_all_orbitopal(bounded, parameters, &bounded_result));
  assert(bounded_result.status == OrbitopalEnumerationStatus::LimitReached &&
         !bounded.inconsistent);

  const uint64_t before_clique = bounded_result.ticks;
  parameters.orbitopal_column_order =
      OrbitopalColumnOrder::CooccurrenceBoundedGreedyClique;
  Kernel retained = clique_coloring_orbitopal_fixture();
  OrbitopalEnumerationResult retained_result;
  std::vector<Lit> retained_candidates;
  assert(!apply_all_orbitopal(retained, parameters, &retained_result,
                              &retained_candidates));
  assert(retained_result.status == OrbitopalEnumerationStatus::Complete &&
         retained_result.clique_budget_stops == 1 &&
         retained_candidates == canonical_candidates);

  parameters.maximum_detection_ticks = before_clique;
  Kernel globally_limited = clique_coloring_orbitopal_fixture();
  OrbitopalEnumerationResult global_result;
  assert(!apply_all_orbitopal(globally_limited, parameters, &global_result));
  assert(global_result.status == OrbitopalEnumerationStatus::LimitReached &&
         !global_result.clique_budget_stops && !global_result.instances);

  parameters.maximum_detection_ticks = UINT64_MAX;
  for (uint64_t local_limit : {uint64_t{0}, uint64_t{5}}) {
    parameters.maximum_orbitopal_clique_ticks = local_limit;
    Kernel stopped = clique_coloring_orbitopal_fixture();
    volatile sig_atomic_t stop = 1;
    bool visited = false;
    const auto interrupted = substitution_redundancy_enumerate_orbitopal(
        stopped, parameters, [&](const SubstitutionRedundancyInstance &) {
          visited = true;
          return true;
        }, &stop);
    assert(interrupted.status == OrbitopalEnumerationStatus::Interrupted &&
           !interrupted.clique_budget_stops && !visited);
  }
}

static void orbitopal_refinement_completes_hidden_columns() {
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
  parameters.orbitopal_orientation = OrbitopalOrientation::Rows;
  Kernel partial = orbitopal_refinement_fixture();
  OrbitopalEnumerationResult partial_result;
  assert(!apply_all_orbitopal(partial, parameters, &partial_result));
  assert(partial_result.status == OrbitopalEnumerationStatus::Complete &&
         partial_result.matrices == 0 && partial.stats.sr_applications == 0);

  parameters.orbitopal_completion =
      OrbitopalCompletion::IndividualizedGraphCompletion;
  parameters.graph_symmetry.maximum_graph_vertices = 128;
  parameters.graph_symmetry.maximum_graph_edges = 256;
  Kernel completed = orbitopal_refinement_fixture();
  OrbitopalEnumerationResult completed_result;
  assert(!apply_all_orbitopal(completed, parameters, &completed_result));
  assert(completed_result.status == OrbitopalEnumerationStatus::Complete &&
         completed_result.matrices == 1 && completed.stats.sr_applied > 0);

  Kernel certified = orbitopal_refinement_fixture();
  SubstitutionRedundancyParameters certified_parameters = parameters;
  certified_parameters.maximum_validation_ticks = 0;
  OnlineInvocation step;
  step.id = 79;
  step.rule = "SubstitutionRedundancy";
  step.parameters = certified_parameters;
  step.granularity = InvocationGranularity::CompleteCurrentPass;
  OnlineInprocessProgram program;
  program.sequence = {step};
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 25));
  const InvocationResult first = executor.advance(certified);
  assert(first.status == InvocationStatus::Completed &&
         first.made_rule_progress && certified.stats.sr_applied == 1 &&
         certified.stats.sr_validation_ticks == 0);
  const uint64_t applications = certified.stats.sr_applied;
  volatile sig_atomic_t stop = 1;
  const InvocationResult interrupted = executor.advance(certified, &stop);
  assert(interrupted.status == InvocationStatus::Interrupted &&
         certified.stats.sr_applied == applications);
  stop = 0;
  const InvocationResult resumed = executor.advance(certified, &stop);
  assert(resumed.status == InvocationStatus::Completed &&
         resumed.made_rule_progress &&
         certified.stats.sr_applied == applications + 1 &&
         certified.stats.sr_validation_ticks == 0);

  parameters.graph_symmetry.maximum_graph_vertices = 1;
  Kernel bounded = orbitopal_refinement_fixture();
  OrbitopalEnumerationResult bounded_result;
  assert(!apply_all_orbitopal(bounded, parameters, &bounded_result));
  assert(bounded_result.status == OrbitopalEnumerationStatus::LimitReached &&
         bounded.stats.sr_applications == 0 && !bounded.inconsistent);
}

static void orbitopal_refinement_cells_need_no_generic_generators() {
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
  parameters.orbitopal_orientation = OrbitopalOrientation::Rows;
  parameters.orbitopal_completion = OrbitopalCompletion::IndividualizedGraphCompletion;
  parameters.orbitopal_discovery_order = OrbitopalDiscoveryOrder::RefinementOnly;
  parameters.orbitopal_generator_fallback = false;
  parameters.graph_symmetry.maximum_generators = 0;
  Kernel legacy = orbitopal_refinement_fixture();
  OrbitopalEnumerationResult legacy_result;
  assert(!apply_all_orbitopal(legacy, parameters, &legacy_result));
  assert(!legacy_result.instances && !legacy.stats.sr_applied);

  parameters.orbitopal_row_orbit_source = OrbitopalRowOrbitSource::RefinementCells;
  Kernel direct = orbitopal_refinement_fixture();
  OrbitopalEnumerationResult direct_result;
  assert(!apply_all_orbitopal(direct, parameters, &direct_result));
  assert(direct_result.status == OrbitopalEnumerationStatus::Complete &&
         direct_result.refinement_orbits && direct_result.matrices &&
         direct.stats.sr_applied);
  kernel_check_invariants(direct);

  // A triangle and a square share equitable degree cells but not one orbit.
  // Refinement cells must never stand in for exact whole-formula row actions.
  Kernel coarse;
  kernel_reserve_vars(coarse, 14);
  Lit positive[7];
  for (int v = 1; v <= 7; ++v)
    positive[v - 1] = internalize(v);
  kernel_add_large_clause(coarse, positive, 7, false, 0, ++coarse.next_clause_id);
  for (int v = 1; v <= 7; ++v)
    kernel_add_binary_clause(coarse, internalize(-v), internalize(v + 7),
                             ++coarse.next_clause_id);
  for (const auto &edge : {std::pair{1, 2}, {2, 3}, {3, 1},
                          {4, 5}, {5, 6}, {6, 7}, {7, 4}})
    kernel_add_binary_clause(coarse, internalize(-edge.first),
                             internalize(-edge.second), ++coarse.next_clause_id);
  OrbitopalEnumerationResult coarse_result;
  assert(!apply_all_orbitopal(coarse, parameters, &coarse_result));
  assert(coarse_result.status == OrbitopalEnumerationStatus::Complete &&
         coarse_result.refinement_orbits && coarse_result.refinement_exact_failures &&
         !coarse_result.instances && !coarse.stats.sr_applied);
  kernel_check_invariants(coarse);

  Kernel stopped = orbitopal_refinement_fixture();
  volatile sig_atomic_t stop = 1;
  bool visited = false;
  const auto interrupted = substitution_redundancy_enumerate_orbitopal(
      stopped, parameters, [&](const SubstitutionRedundancyInstance &) {
        visited = true;
        return true;
      }, &stop);
  assert(interrupted.status == OrbitopalEnumerationStatus::Interrupted && !visited);
  parameters.maximum_detection_ticks = 0;
  OrbitopalEnumerationResult bounded;
  assert(!apply_all_orbitopal(stopped, parameters, &bounded));
  assert(bounded.status == OrbitopalEnumerationStatus::Complete && !bounded.instances);
  parameters.maximum_detection_ticks = 1;
  assert(!apply_all_orbitopal(stopped, parameters, &bounded));
  assert(bounded.status == OrbitopalEnumerationStatus::LimitReached && !bounded.instances);

  parameters.orbitopal_row_orbit_source = static_cast<OrbitopalRowOrbitSource>(2);
  assert(!parameters.valid());
  parameters.producer = SubstitutionRedundancyProducer::ExplicitOnly;
  assert(parameters.valid());
}

static void orbitopal_discovery_scopes_are_independent() {
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
  parameters.orbitopal_orientation = OrbitopalOrientation::Rows;
  parameters.orbitopal_completion =
      OrbitopalCompletion::IndividualizedGraphCompletion;
  parameters.graph_symmetry.maximum_graph_vertices = 128;
  parameters.graph_symmetry.maximum_graph_edges = 256;
  parameters.orbitopal_generator_fallback = false;
  parameters.orbitopal_discovery_order = OrbitopalDiscoveryOrder::RefinementOnly;
  Kernel refined = orbitopal_refinement_fixture();
  OrbitopalEnumerationResult refined_result;
  assert(!apply_all_orbitopal(refined, parameters, &refined_result));
  assert(refined_result.status == OrbitopalEnumerationStatus::Complete &&
         refined_result.refinement_orbits > 0 && refined.stats.sr_applied > 0);

  // Global refinement is exhausted before examining an orbit. ULC-anchored
  // discovery has an independent scope and can still complete the same action.
  parameters.maximum_orbitopal_refinement_orbits = 0;
  Kernel bounded = orbitopal_refinement_fixture();
  OrbitopalEnumerationResult bounded_result;
  assert(!apply_all_orbitopal(bounded, parameters, &bounded_result));
  assert(bounded_result.status == OrbitopalEnumerationStatus::LimitReached &&
         bounded.stats.sr_applied == 0);
  parameters.orbitopal_discovery_order = OrbitopalDiscoveryOrder::UniqueLiteralOnly;
  Kernel anchored = orbitopal_refinement_fixture();
  OrbitopalEnumerationResult anchored_result;
  assert(!apply_all_orbitopal(anchored, parameters, &anchored_result));
  assert(anchored_result.status == OrbitopalEnumerationStatus::Complete &&
         anchored_result.refinement_orbits == 0 && anchored.stats.sr_applied > 0);

  // Inactive combinations remain legal; independent AMO completion still runs.
  parameters.orbitopal_discovery_order = OrbitopalDiscoveryOrder::RefinementOnly;
  parameters.orbitopal_orientation = OrbitopalOrientation::Columns;
  assert(parameters.valid());
  Kernel columns = orbitopal_refinement_fixture();
  OrbitopalEnumerationResult columns_result;
  assert(!apply_all_orbitopal(columns, parameters, &columns_result));
  assert(columns_result.status == OrbitopalEnumerationStatus::Complete &&
         columns_result.instances == 0);
  parameters.orbitopal_orientation = OrbitopalOrientation::Rows;
  parameters.orbitopal_completion = OrbitopalCompletion::MatrixOnly;
  assert(parameters.valid());
  Kernel empty = orbitopal_single_seed_refinement_fixture();
  OrbitopalEnumerationResult empty_result;
  assert(!apply_all_orbitopal(empty, parameters, &empty_result));
  assert(empty_result.status == OrbitopalEnumerationStatus::Complete &&
         empty_result.instances == 0);
  parameters.complete_unique_literal_amos = true;
  Kernel amo = orbitopal_single_seed_refinement_fixture();
  OrbitopalEnumerationResult amo_result;
  assert(!apply_all_orbitopal(amo, parameters, &amo_result));
  assert(amo_result.status == OrbitopalEnumerationStatus::Complete &&
         amo_result.instances > 0 && amo.stats.sr_applied > 0);
}

static void orbitopal_refinement_starts_from_one_unique_column() {
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
  parameters.orbitopal_orientation = OrbitopalOrientation::Rows;

  Kernel matrix_only = orbitopal_single_seed_refinement_fixture();
  OrbitopalEnumerationResult matrix_only_result;
  assert(!apply_all_orbitopal(matrix_only, parameters, &matrix_only_result));
  assert(matrix_only_result.status == OrbitopalEnumerationStatus::Complete &&
         matrix_only_result.matrices == 0 &&
         matrix_only.stats.sr_applications == 0);

  parameters.orbitopal_completion =
      OrbitopalCompletion::IndividualizedGraphCompletion;
  parameters.graph_symmetry.maximum_graph_vertices = 128;
  parameters.graph_symmetry.maximum_graph_edges = 256;
  Kernel completed = orbitopal_single_seed_refinement_fixture();
  OrbitopalEnumerationResult completed_result;
  assert(!apply_all_orbitopal(completed, parameters, &completed_result));
  assert(completed_result.status == OrbitopalEnumerationStatus::Complete &&
         completed_result.matrices == 1 && completed.stats.sr_applied > 0);
  kernel_check_invariants(completed);
}

static void orbitopal_clique_order_matches_exhaustive_connected_graphs() {
  constexpr unsigned vertices = 5, colors = 6, possible_edges = 10;
  uint64_t noncanonical_incumbents = 0, connected_graphs = 0;
  const auto edge_bit = [](unsigned first, unsigned second) {
    assert(first < second);
    unsigned bit = 0;
    for (unsigned i = 0; i < first; ++i)
      bit += vertices - i - 1;
    return bit + second - first - 1;
  };
  const auto adjacent = [&](unsigned mask, unsigned first, unsigned second) {
    if (first > second)
      std::swap(first, second);
    return first != second && (mask & (1u << edge_bit(first, second)));
  };
  for (unsigned mask = 0; mask < (1u << possible_edges); ++mask) {
    unsigned reached = 1;
    for (bool changed = true; changed;) {
      changed = false;
      for (unsigned first = 0; first < vertices; ++first)
        if (reached & (1u << first))
          for (unsigned second = 0; second < vertices; ++second)
            if (!(reached & (1u << second)) &&
                adjacent(mask, first, second)) {
              reached |= 1u << second;
              changed = true;
            }
    }
    if (reached != (1u << vertices) - 1)
      continue;
    ++connected_graphs;

    unsigned maximum_size = 0;
    for (unsigned selected = 1; selected < (1u << vertices); ++selected) {
      bool clique = true;
      for (unsigned first = 0; first < vertices; ++first)
        if (selected & (1u << first))
          for (unsigned second = first + 1; second < vertices; ++second)
            if ((selected & (1u << second)) &&
                !adjacent(mask, first, second))
              clique = false;
      if (clique)
        maximum_size = std::max(maximum_size,
                                static_cast<unsigned>(__builtin_popcount(selected)));
    }

    Kernel kernel;
    kernel_reserve_vars(kernel, vertices * colors);
    const auto variable = [](unsigned vertex, unsigned color) {
      return static_cast<int>(vertex * colors + color + 1);
    };
    const auto add_binary = [&](int first, int second) {
      kernel_add_binary_clause(kernel, internalize(first),
                               internalize(second), ++kernel.next_clause_id);
    };
    for (unsigned vertex = 0; vertex < vertices; ++vertex) {
      Lit atleast[colors];
      for (unsigned color = 0; color < colors; ++color)
        atleast[color] = internalize(variable(vertex, color));
      kernel_add_large_clause(kernel, atleast, colors, false, 0,
                              ++kernel.next_clause_id);
      for (unsigned first = 0; first < colors; ++first)
        for (unsigned second = first + 1; second < colors; ++second)
          add_binary(-variable(vertex, first), -variable(vertex, second));
    }
    for (unsigned first = 0; first < vertices; ++first)
      for (unsigned second = first + 1; second < vertices; ++second)
        if (adjacent(mask, first, second))
          for (unsigned color = 0; color < colors; ++color)
            add_binary(-variable(first, color), -variable(second, color));

    SubstitutionRedundancyParameters parameters;
    parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
    parameters.orbitopal_orientation = OrbitopalOrientation::Rows;
    parameters.orbitopal_column_order =
        OrbitopalColumnOrder::CooccurrenceMaximumClique;
    parameters.maximum_orbitopal_clique_vertices = vertices;
    parameters.maximum_orbitopal_clique_ticks = 1'000'000;
    std::vector<Lit> candidates;
    const auto enumeration = substitution_redundancy_enumerate_orbitopal(
        kernel, parameters,
        [&](const SubstitutionRedundancyInstance &instance) {
          candidates.push_back(instance.clause.front());
          return true;
        });
    assert(enumeration.status == OrbitopalEnumerationStatus::Complete &&
           enumeration.matrices == 1 && candidates.size() == 15);
    std::vector<unsigned> ordered;
    for (const size_t offset : {size_t{0}, size_t{5}, size_t{9}, size_t{12},
                                size_t{14}})
      ordered.push_back(var_of(negate(candidates[offset])) / colors);
    for (unsigned first = 0; first < maximum_size; ++first)
      for (unsigned second = first + 1; second < maximum_size; ++second)
        assert(adjacent(mask, ordered[first], ordered[second]));

    const unsigned identity_tie =
        (1u << edge_bit(0, 2)) | (1u << edge_bit(0, 3)) |
        (1u << edge_bit(1, 2)) | (1u << edge_bit(3, 4));
    if (mask == identity_tie)
      assert(ordered[0] == 0 && ordered[1] == 2 &&
             "identity-ordered clique ties must match the structured source");

    const auto get_order = [&](OrbitopalColumnOrder order, uint64_t local_limit,
                               OrbitopalEnumerationResult &result) {
      parameters.orbitopal_column_order = order;
      parameters.maximum_orbitopal_clique_ticks = local_limit;
      std::vector<Lit> emitted;
      result = substitution_redundancy_enumerate_orbitopal(
          kernel, parameters, [&](const SubstitutionRedundancyInstance &instance) {
            emitted.push_back(instance.clause.front());
            return true;
          });
      std::vector<unsigned> columns;
      if (result.status == OrbitopalEnumerationStatus::Complete) {
        assert(emitted.size() == 15);
        for (size_t offset : {size_t{0}, size_t{5}, size_t{9}, size_t{12}, size_t{14}})
          columns.push_back(var_of(emitted[offset]) / colors);
      }
      return columns;
    };
    OrbitopalEnumerationResult canonical_result, greedy_result;
    const auto canonical = get_order(OrbitopalColumnOrder::Canonical, 0,
                                     canonical_result);
    const auto greedy = get_order(OrbitopalColumnOrder::CooccurrenceGreedyClique,
                                  1'000'000, greedy_result);
    for (uint64_t limit : {0ull, 1ull, 5ull, 8ull, 16ull, 32ull, 64ull, 1'000'000ull}) {
      OrbitopalEnumerationResult bounded_result;
      const auto bounded = get_order(
          OrbitopalColumnOrder::CooccurrenceBoundedGreedyClique, limit,
          bounded_result);
      assert(bounded_result.status == OrbitopalEnumerationStatus::Complete);
      auto sorted = bounded;
      std::sort(sorted.begin(), sorted.end());
      assert((sorted == std::vector<unsigned>{0, 1, 2, 3, 4}));
      const size_t clique_size = bounded_result.largest_ordering_clique;
      assert(clique_size && clique_size <= vertices);
      for (size_t first = 0; first < clique_size; ++first)
        for (size_t second = first + 1; second < clique_size; ++second)
          assert(adjacent(mask, bounded[first], bounded[second]));
      if (!limit)
        assert(bounded == canonical && bounded_result.clique_budget_stops == 1);
      if (limit == 1'000'000) {
        assert(bounded == greedy && !bounded_result.clique_budget_stops);
        assert(bounded_result.ticks == greedy_result.ticks);
      }
      if (bounded_result.clique_budget_stops && clique_size > 1 &&
          bounded != canonical)
        ++noncanonical_incumbents;
    }
  }
  assert(connected_graphs == 728 && noncanonical_incumbents > 0);
}

static void graph_symmetry_is_exact_bounded_and_interruptible(bool preserve) {
  Kernel k = symmetry_fixture();
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::GraphSymmetry;
  parameters.graph_symmetry.preserve_detector_order = preserve;
  parameters.graph_symmetry.backend_search_budget = 64;
  parameters.graph_symmetry.maximum_graph_vertices = 128;
  parameters.graph_symmetry.maximum_graph_edges = 256;
  parameters.graph_symmetry.maximum_generators = 64;
  parameters.graph_symmetry.maximum_fixing_instances = 64;
  parameters.graph_symmetry.maximum_group_ticks = 100'000;
  std::vector<SubstitutionRedundancyInstance> instances;
  const auto enumeration = substitution_redundancy_enumerate_symmetry(
      k, parameters,
      [&](const SubstitutionRedundancyInstance &instance) {
        instances.push_back(instance);
        return true;
      });
  assert(enumeration.status == SymmetryEnumerationStatus::Complete);
  assert(enumeration.generators_proposed > 0);
  assert(enumeration.generators_proposed ==
         enumeration.generators_validated);
  assert(enumeration.instances == instances.size() && !instances.empty());
  const auto applied =
      substitution_redundancy_apply_detailed(k, instances.front(), parameters);
  assert(applied.status == SubstitutionRedundancyApplicationStatus::Applied);
  kernel_check_invariants(k);

  Kernel bounded = symmetry_fixture();
  parameters.graph_symmetry.maximum_graph_vertices = 1;
  uint64_t visited = 0;
  const auto limited = substitution_redundancy_enumerate_symmetry(
      bounded, parameters,
      [&](const SubstitutionRedundancyInstance &) {
        ++visited;
        return true;
      });
  assert(limited.status == SymmetryEnumerationStatus::LimitReached);
  assert(!visited);

  parameters.graph_symmetry.maximum_graph_vertices = 0;
  const auto disabled = substitution_redundancy_enumerate_symmetry(
      bounded, parameters,
      [&](const SubstitutionRedundancyInstance &) {
        ++visited;
        return true;
      });
  assert(disabled.status == SymmetryEnumerationStatus::Complete);
  assert(!visited && !disabled.instances);

  parameters.graph_symmetry.maximum_graph_vertices = 128;
  volatile sig_atomic_t stop = 1;
  const auto interrupted = substitution_redundancy_enumerate_symmetry(
      bounded, parameters,
      [&](const SubstitutionRedundancyInstance &) {
        ++visited;
        return true;
      },
      &stop);
  assert(interrupted.status == SymmetryEnumerationStatus::Interrupted);
  assert(!visited);

  Kernel dense;
  kernel_reserve_vars(dense, 4);
  for (int first = 1; first <= 4; ++first)
    for (int second = first + 1; second <= 4; ++second)
      kernel_add_binary_clause(dense, internalize(first), internalize(second),
                               ++dense.next_clause_id);
  // Keep a symmetric nonbinary part in the fixture.  Direct binary edges must
  // still allocate one clause vertex for each of these four ternaries.
  for (int omitted = 1; omitted <= 4; ++omitted) {
    Lit clause[3];
    unsigned at = 0;
    for (int literal = 1; literal <= 4; ++literal)
      if (literal != omitted)
        clause[at++] = internalize(literal);
    kernel_add_large_clause(dense, clause, 3, false, 0,
                            ++dense.next_clause_id);
  }
  parameters.graph_symmetry.maximum_graph_vertices = 16;
  parameters.graph_symmetry.maximum_graph_edges = 26;
  parameters.graph_symmetry.clause_scope = SymmetryClauseScope::All;
  parameters.graph_symmetry.binary_graph_mode =
      SymmetryBinaryGraphMode::ClauseVertices;
  visited = 0;
  const auto clause_vertex_limited =
      substitution_redundancy_enumerate_symmetry(
          dense, parameters,
          [&](const SubstitutionRedundancyInstance &) {
            ++visited;
            return true;
          });
  assert(clause_vertex_limited.status ==
             SymmetryEnumerationStatus::LimitReached &&
         !visited);
  parameters.graph_symmetry.binary_graph_mode =
      SymmetryBinaryGraphMode::Automatic;
  parameters.graph_symmetry.binary_graph_density_permille = 1000;
  const auto direct_binary = substitution_redundancy_enumerate_symmetry(
      dense, parameters,
      [&](const SubstitutionRedundancyInstance &) {
        ++visited;
        return true;
      });
  assert(direct_binary.generators_proposed > 0 &&
         direct_binary.generators_proposed ==
             direct_binary.generators_validated &&
         visited == direct_binary.instances && visited > 0);
}

static Kernel sst_clause_fixture(bool amo) {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit clause[] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, clause, 3, false, 0, ++k.next_clause_id);
  // Symmetric private consequences keep each representative unit from being
  // an SR step on its own; the complete SST prefix is load-bearing.
  kernel_add_binary_clause(k, internalize(-1), internalize(4),
                           ++k.next_clause_id);
  kernel_add_binary_clause(k, internalize(-2), internalize(5),
                           ++k.next_clause_id);
  kernel_add_binary_clause(k, internalize(-3), internalize(6),
                           ++k.next_clause_id);
  if (amo) {
    kernel_add_binary_clause(k, internalize(-1), internalize(-2),
                             ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(-1), internalize(-3),
                             ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(-2), internalize(-3),
                             ++k.next_clause_id);
  }
  return k;
}

static SubstitutionRedundancyParameters sst_parameters() {
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::GraphSymmetry;
  parameters.graph_symmetry.backend_search_budget = 64;
  parameters.graph_symmetry.maximum_graph_vertices = 128;
  parameters.graph_symmetry.maximum_graph_edges = 256;
  parameters.graph_symmetry.maximum_generators = 64;
  parameters.graph_symmetry.maximum_fixing_instances = 64;
  parameters.graph_symmetry.maximum_group_ticks = 1'000'000;
  parameters.graph_symmetry.fixing_mode = SymmetryFixingMode::StabilizerOnly;
  parameters.graph_symmetry.clause_scope = SymmetryClauseScope::All;
  parameters.graph_symmetry.clause_selection =
      SymmetryClauseSelection::AmoThenShortest;
  return parameters;
}

static void large_domain_generator_fixing_uses_units_without_a_chain() {
  const auto enumerate = [](Kernel k, std::vector<std::vector<Lit>> images) {
    SubstitutionRedundancyParameters parameters = sst_parameters();
    parameters.graph_symmetry.maximum_generators = images.size();
    parameters.graph_symmetry.schreier_domain_limit = 0;
    SymmetryDiscoveryHints hints;
    hints.generators = std::move(images);
    std::vector<SubstitutionRedundancyInstance> instances;
    const auto enumeration = substitution_redundancy_enumerate_symmetry(
        k, parameters,
        [&](const SubstitutionRedundancyInstance &instance) {
          instances.push_back(instance);
          return true;
        }, nullptr, &hints);
    // Supplying the complete test generator consumes the configured detector
    // prefix, so group completeness is deliberately not claimed.
    assert(enumeration.status == SymmetryEnumerationStatus::LimitReached);
    assert(enumeration.generators_validated == hints.generators.size());
    assert(enumeration.instances == instances.size());
    return std::pair{std::move(k), std::move(instances)};
  };

  const auto check = [&](Kernel k, std::vector<std::vector<Lit>> images,
                         std::vector<Lit> expected) {
    auto [checked, instances] = enumerate(std::move(k), std::move(images));
    assert(instances.size() == expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
      assert(instances[i].clause == std::vector<Lit>{expected[i]});
      assert(instances[i].symmetry_chain_certificate);
      // The public path ignores the private certificate and rechecks X7 fully.
      assert(substitution_redundancy_apply_detailed(checked, instances[i],
                                                    sst_parameters())
                 .status == SubstitutionRedundancyApplicationStatus::Applied);
      assert(checked.value(expected[i]) > 0);
    }
    kernel_check_invariants(checked);
  };

  Kernel negation;
  kernel_reserve_vars(negation, 2);
  kernel_add_binary_clause(negation, internalize(1), internalize(2),
                           ++negation.next_clause_id);
  kernel_add_binary_clause(negation, internalize(-1), internalize(2),
                           ++negation.next_clause_id);
  check(std::move(negation),
        {{internalize(-1), internalize(1), internalize(2), internalize(-2)}},
        {internalize(1)});

  // Binary fixing follows negation fixing unconditionally, as in the owner: a
  // formula without any negation orbit still fixes the swapped binary's target.
  Kernel binary;
  kernel_reserve_vars(binary, 2);
  kernel_add_binary_clause(binary, internalize(1), internalize(2),
                           ++binary.next_clause_id);
  check(std::move(binary),
        {{internalize(2), internalize(-2), internalize(1), internalize(-1)}},
        {internalize(2)});

  // Once negation fixing establishes a unit, binary fixing may refine the
  // surviving subgroup in the same pass.
  Kernel refined;
  kernel_reserve_vars(refined, 4);
  kernel_add_binary_clause(refined, internalize(1), internalize(2),
                           ++refined.next_clause_id);
  kernel_add_binary_clause(refined, internalize(-1), internalize(2),
                           ++refined.next_clause_id);
  kernel_add_binary_clause(refined, internalize(3), internalize(4),
                           ++refined.next_clause_id);
  check(std::move(refined),
        {{internalize(-1), internalize(1), internalize(2), internalize(-2),
          internalize(3), internalize(-3), internalize(4), internalize(-4)},
         {internalize(1), internalize(-1), internalize(2), internalize(-2),
          internalize(4), internalize(-4), internalize(3), internalize(-3)}},
        {internalize(1), internalize(4)});
}

static void stabilizer_chain_adds_all_cuts_before_rup_unit() {
  Kernel k = sst_clause_fixture(false);
  SubstitutionRedundancyParameters parameters = sst_parameters();
  parameters.graph_symmetry.nonforcing_cut_admission =
      SymmetryNonforcingCutAdmission::RequireForcingMember;
  std::vector<SubstitutionRedundancyInstance> instances;
  const auto enumeration = substitution_redundancy_enumerate_symmetry(
      k, parameters,
      [&](const SubstitutionRedundancyInstance &instance) {
        instances.push_back(instance);
        return true;
      });
  assert(enumeration.status == SymmetryEnumerationStatus::Complete);
  assert(enumeration.generators_validated > 0 && instances.size() == 3);
  assert(!enumeration.nonforcing_orbit_cuts_declined);
  assert(instances[0].clause.size() == 2 &&
         instances[1].clause.size() == 2 &&
         instances[2].clause.size() == 1);
  const Lit representative = instances[2].clause.front();
  assert(instances[0].clause[0] == representative &&
         instances[1].clause[0] == representative);
  for (size_t i = 0; i < 2; ++i)
    assert(std::any_of(instances[i].substitution.begin(),
                       instances[i].substitution.end(),
                       [&](const auto &mapping) {
                         return var_of(mapping.first) ==
                                var_of(representative);
                       }));

  // Internal chain provenance is unavailable on the public explicit path.
  // Before the binary prefix, the representative unit is not RUP.
  Kernel premature = sst_clause_fixture(false);
  assert(substitution_redundancy_apply_detailed(premature, instances.back(),
                                                parameters)
             .status == SubstitutionRedundancyApplicationStatus::Inapplicable);

  for (const auto &instance : instances)
    assert(substitution_redundancy_apply_detailed(k, instance, parameters)
               .status == SubstitutionRedundancyApplicationStatus::Applied);
  assert(k.value(representative) > 0);
  kernel_check_invariants(k);
}

static void fix_and_cut_reaches_nonclausal_orbits() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit clauses[][2] = {
      {internalize(1), internalize(3)},
      {internalize(2), internalize(4)},
      {internalize(-3), internalize(5)},
      {internalize(-4), internalize(6)},
  };
  for (const auto &clause : clauses)
    kernel_add_binary_clause(k, clause[0], clause[1], ++k.next_clause_id);

  SubstitutionRedundancyParameters parameters = sst_parameters();
  parameters.graph_symmetry.chain_mode = SymmetryChainMode::FixAndCut;
  parameters.graph_symmetry.base_selection =
      SymmetryBaseSelection::CommonClauseAmo;
  std::vector<SubstitutionRedundancyInstance> instances;
  const SymmetryEnumerationResult enumeration =
      substitution_redundancy_enumerate_symmetry(
          k, parameters,
          [&](const SubstitutionRedundancyInstance &instance) {
            instances.push_back(instance);
            return true;
          });
  assert(enumeration.status == SymmetryEnumerationStatus::Complete);
  assert(enumeration.generators_validated > 0 && !instances.empty());
  // No clause in the fixture lies within one literal orbit, so fix-only mode
  // has no action. The complete producer reaches the remaining group through
  // binary cuts and does not manufacture a representative RUP unit.
  assert(std::all_of(instances.begin(), instances.end(), [](const auto &instance) {
    return instance.clause.size() == 2 &&
           instance.symmetry_chain_certificate;
  }));

  SubstitutionRedundancyParameters forcing_only = parameters;
  forcing_only.graph_symmetry.nonforcing_cut_admission =
      SymmetryNonforcingCutAdmission::RequireForcingMember;
  uint64_t admitted = 0;
  const SymmetryEnumerationResult declined =
      substitution_redundancy_enumerate_symmetry(
          k, forcing_only,
          [&](const SubstitutionRedundancyInstance &) {
            ++admitted;
            return true;
          });
  assert(declined.status == SymmetryEnumerationStatus::Complete && !admitted &&
         declined.nonforcing_orbit_cuts_declined == instances.size());

  SubstitutionRedundancyParameters compound = parameters;
  compound.graph_symmetry.nonforcing_cut_admission =
      SymmetryNonforcingCutAdmission::RequireCompoundProgress;
  uint64_t compound_admitted = 0;
  const SymmetryEnumerationResult compound_result =
      substitution_redundancy_enumerate_symmetry(
          k, compound, [&](const SubstitutionRedundancyInstance &) {
            ++compound_admitted;
            return true;
          });
  assert(instances.size() > 1);
  assert(compound_result.status == SymmetryEnumerationStatus::Complete &&
         compound_admitted == instances.size() &&
         !compound_result.nonforcing_orbit_cuts_declined);

  SubstitutionRedundancyParameters legacy = parameters;
  legacy.graph_symmetry.chain_mode = SymmetryChainMode::ClausalFixing;
  uint64_t legacy_instances = 0;
  const SymmetryEnumerationResult stopped =
      substitution_redundancy_enumerate_symmetry(
          k, legacy,
          [&](const SubstitutionRedundancyInstance &) {
            ++legacy_instances;
            return true;
          });
  assert(stopped.status == SymmetryEnumerationStatus::Complete &&
         !legacy_instances);
  kernel_check_invariants(k);
}

static void amo_stabilizer_chain_propagates_complements_online() {
  Kernel k = sst_clause_fixture(true);
  OnlineInvocation step;
  step.id = 1003;
  step.rule = "SubstitutionRedundancy";
  step.parameters = sst_parameters();
  step.granularity = InvocationGranularity::CompleteCurrentPass;
  step.post_application = PostApplication::Propagate;
  OnlineInprocessProgram program;
  program.sequence = {step};
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 31));
  uint64_t progress = 0;
  while (!executor.complete()) {
    const InvocationResult result = executor.advance(k);
    assert(result.status == InvocationStatus::Completed ||
           result.status == InvocationStatus::NoChange);
    progress += result.made_rule_progress;
  }
  assert(progress == 3 && kernel_propagation_fixpoint(k));
  assert(k.stats.sr_validation_ticks == 0);
  unsigned true_literals = 0;
  for (int external = 1; external <= 3; ++external)
    true_literals += k.value(internalize(external)) > 0;
  assert(true_literals == 1);
  kernel_check_invariants(k);
}

static void every_root_assignment_constrains_graph_symmetry() {
  Kernel k = symmetry_fixture();
  assert(!k.unit_ids[0]);
  kernel_assign_unit(k, internalize(1));
  assert(k.value(internalize(1)) > 0 && !k.unit_ids[0]);

  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::GraphSymmetry;
  parameters.graph_symmetry.backend_search_budget = 64;
  parameters.graph_symmetry.maximum_graph_vertices = 128;
  parameters.graph_symmetry.maximum_graph_edges = 256;
  parameters.graph_symmetry.maximum_generators = 64;
  parameters.graph_symmetry.maximum_fixing_instances = 64;
  parameters.graph_symmetry.maximum_group_ticks = 100'000;
  uint64_t visited = 0;
  const auto enumeration = substitution_redundancy_enumerate_symmetry(
      k, parameters,
      [&](const SubstitutionRedundancyInstance &) {
        ++visited;
        return true;
      });
  assert(enumeration.status == SymmetryEnumerationStatus::Complete);
  assert(!enumeration.generators_validated && !enumeration.instances &&
         !visited);
}

static void explicit_pivot_mapping_must_repair_candidate() {
  for (const int phase : {1, -1}) {
    Kernel k;
    kernel_reserve_vars(k, 2);
    kernel_assign_unit(k, internalize(phase));
    kernel_assign_unit(k, internalize(2 * phase));
    assert(propagate_apply(k) != ApplyResult::Unsat);
    // F is satisfiable and sigma preserves it, but C is false in its only
    // model and C|sigma is false too. Checking F|sigma alone is insufficient.
    SubstitutionRedundancyInstance instance;
    instance.clause = {internalize(-phase)};
    instance.substitution = {{internalize(1), internalize(2)}};
    const auto before = state_version(k);
    const auto clauses = k.next_clause_id;
    const auto result = substitution_redundancy_apply_detailed(
        k, instance, SubstitutionRedundancyParameters{});
    assert(result.status == SubstitutionRedundancyApplicationStatus::Inapplicable);
    assert(!k.inconsistent && k.value(internalize(phase)) > 0 &&
           k.value(internalize(2 * phase)) > 0);
    assert(k.next_clause_id == clauses &&
           k.arena_generation == before.arena_generation &&
           k.touch_epoch == before.touch_epoch &&
           k.strategy_epoch == before.strategy_epoch);
    kernel_check_invariants(k);
  }

  // An explicit pivot image remains legal when !C implies the image of C.
  // This is the SST-cut case; requiring sigma(pivot)=true would exclude it.
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2), ++k.next_clause_id);
  SubstitutionRedundancyInstance cut;
  cut.clause = {internalize(1), internalize(-2)};
  cut.substitution = {{internalize(1), internalize(2)},
                      {internalize(2), internalize(1)}};
  assert(substitution_redundancy_apply_detailed(
      k, cut, SubstitutionRedundancyParameters{}).status ==
      SubstitutionRedundancyApplicationStatus::Applied);
  kernel_check_invariants(k);
}

// Explicit pivot mappings remain legal with any number of open images.
// Proof serialization may expand one application into temporary proof clauses.
static void explicit_pivot_mappings_remain_applicable() {
  {
    Kernel k;
    kernel_reserve_vars(k, 2);
    kernel_add_binary_clause(k, internalize(1), internalize(2),
                             ++k.next_clause_id);
    SubstitutionRedundancyInstance unit;
    unit.clause = {internalize(1)};
    unit.substitution = {{internalize(1), internalize(2)}};
    assert(substitution_redundancy_apply_detailed(
               k, unit, SubstitutionRedundancyParameters{})
               .status == SubstitutionRedundancyApplicationStatus::Applied);
    assert(k.value(internalize(1)) > 0);
    kernel_check_invariants(k);
  }
  Kernel k;
  kernel_reserve_vars(k, 5);
  kernel_add_binary_clause(k, internalize(3), internalize(4),
                           ++k.next_clause_id);
  SubstitutionRedundancyInstance open;
  const Lit clause[] = {internalize(1), internalize(2), internalize(5)};
  open.clause.assign(clause, clause + 3);
  open.substitution = {{internalize(1), internalize(3)},
                       {internalize(2), internalize(4)}};
  const auto clauses = k.next_clause_id;
  assert(substitution_redundancy_apply_detailed(
             k, open, SubstitutionRedundancyParameters{})
             .status == SubstitutionRedundancyApplicationStatus::Applied);
  assert(k.next_clause_id == clauses + 1);
  assert(substitution_redundancy_apply_detailed(k, open).status ==
         SubstitutionRedundancyApplicationStatus::NoChange);
  kernel_check_invariants(k);
}

static void fixed_root_units_do_not_consume_graph_admission() {
  for (const auto mode : {SymmetryBinaryGraphMode::ClauseVertices,
                          SymmetryBinaryGraphMode::DirectEdges})
    for (const int phase : {1, -1})
      for (const unsigned copies : {1u, 2u}) {
        Kernel k;
        kernel_reserve_vars(k, 4);
        kernel_assign_unit(k, internalize(phase));
        const Lit root = internalize(-2 * phase);
        kernel_assign_unit(k, root);
        for (unsigned i = 0; i < copies; ++i)
          kernel_add_binary_clause(k, internalize(3), internalize(4),
                                   ++k.next_clause_id);
        // Keep satisfied clauses uncollected: root colors must still fix the
        // original root while discovering the independent open-variable swap.
        for (const int open : {3, 4})
          kernel_add_binary_clause(k, root, internalize(open), ++k.next_clause_id);
        const Lit clause[] = {root, internalize(3), internalize(4)};
        kernel_add_large_clause(k, clause, 3, false, 0);
        SubstitutionRedundancyParameters p;
        p.producer = SubstitutionRedundancyProducer::GraphSymmetry;
        auto &graph = p.graph_symmetry;
        graph.binary_graph_mode = mode;
        graph.clause_scope = SymmetryClauseScope::All;
        graph.pointwise_root_stabilizer = true;
        const bool direct = mode == SymmetryBinaryGraphMode::DirectEdges;
        graph.maximum_graph_vertices = (direct ? 12u : 11u) + copies;
        graph.maximum_graph_edges = (direct ? 12u : 11u) + 2 * copies;
        std::vector<SubstitutionRedundancyInstance> instances;
        const auto enumerate = [&](volatile sig_atomic_t *stop = nullptr) {
          instances.clear();
          return substitution_redundancy_enumerate_symmetry(
              k, p, [&](const SubstitutionRedundancyInstance &instance) {
                instances.push_back(instance);
                return true;
              }, stop);
        };
        volatile sig_atomic_t stop = 1;
        assert(enumerate(&stop).status == SymmetryEnumerationStatus::Interrupted);
        assert(instances.empty());
        const auto result = enumerate();
        assert(result.status == SymmetryEnumerationStatus::Complete &&
               result.generators_validated == 1 && !instances.empty());
        --graph.maximum_graph_edges;
        assert(enumerate().status == SymmetryEnumerationStatus::LimitReached);
        ++graph.maximum_graph_edges;
        --graph.maximum_graph_vertices;
        assert(enumerate().status == SymmetryEnumerationStatus::LimitReached);
        ++graph.maximum_graph_vertices;
        graph.pointwise_root_stabilizer = false;
        assert(enumerate().status == SymmetryEnumerationStatus::LimitReached);
        graph.pointwise_root_stabilizer = true;

        // Flipping variable 1 preserves every nonunit clause but violates its
        // root unit. Graph compaction must never remove that SR obligation.
        SubstitutionRedundancyInstance cut;
        cut.clause = {internalize(3), internalize(-4)};
        cut.substitution = {{internalize(3), internalize(4)},
                            {internalize(4), internalize(3)},
                            {internalize(1), internalize(-1)}};
        assert(substitution_redundancy_apply_detailed(k, cut, p).status ==
               SubstitutionRedundancyApplicationStatus::Inapplicable);
        cut.substitution.pop_back();
        assert(substitution_redundancy_apply_detailed(k, cut, p).status ==
               SubstitutionRedundancyApplicationStatus::Applied);
        assert(k.value(internalize(phase)) > 0 && k.value(root) > 0);
        kernel_check_invariants(k);
      }
}

static void pointwise_root_discovery_preserves_explicit_sr() {
  // Interchangeable assigned variables consume a finite generator allowance.
  // Restricted discovery must retain the open symmetry with no root-only maps.
  for (const int phase : {1, -1}) {
    Kernel k;
    kernel_reserve_vars(k, 134);
    for (int variable = 1; variable <= 132; ++variable)
      kernel_assign_unit(k, internalize(phase * variable));
    kernel_add_binary_clause(k, internalize(133), internalize(134),
                             ++k.next_clause_id);
    SubstitutionRedundancyParameters parameters;
    parameters.producer = SubstitutionRedundancyProducer::GraphSymmetry;
    auto &graph = parameters.graph_symmetry;
    graph.clause_scope = SymmetryClauseScope::All;
    graph.maximum_generators = 128;
    graph.backend_search_budget = 64;
    graph.maximum_group_ticks = 1'000'000;
    std::vector<SubstitutionRedundancyInstance> instances;
    const auto enumerate = [&](volatile sig_atomic_t *stop = nullptr) {
      instances.clear();
      return substitution_redundancy_enumerate_symmetry(
          k, parameters, [&](const SubstitutionRedundancyInstance &instance) {
            instances.push_back(instance);
            return true;
          }, stop);
    };
    const auto unrestricted = enumerate();
    assert(unrestricted.generators_validated == 128 &&
           unrestricted.status == SymmetryEnumerationStatus::LimitReached);
    const auto unrestricted_parameters = parameters;
    graph.pointwise_root_stabilizer = true;
    assert(parameters != unrestricted_parameters);
    const StateVersion before = state_version(k);
    volatile sig_atomic_t stop = 1;
    assert(enumerate(&stop).status == SymmetryEnumerationStatus::Interrupted);
    assert(instances.empty() && k.arena_generation == before.arena_generation &&
           k.touch_epoch == before.touch_epoch &&
           k.strategy_epoch == before.strategy_epoch);
    stop = 0;
    const auto restricted = enumerate(&stop);
    assert(restricted.status == SymmetryEnumerationStatus::Complete &&
           restricted.generators_validated == 1 && !instances.empty());
    assert(!restricted.complete_group);
    for (const auto &instance : instances)
      for (const auto &mapping : instance.substitution)
        assert(var_of(mapping.first) >= 132);
    // Reconfiguration cannot narrow the explicit action surface. Include a
    // legal permutation of assigned variables that restricted discovery omits.
    auto explicit_instance = instances.back();
    explicit_instance.substitution.push_back(
        {internalize(1), internalize(2)});
    explicit_instance.substitution.push_back(
        {internalize(2), internalize(1)});
    const auto applied = substitution_redundancy_apply_detailed(
        k, explicit_instance, parameters);
    assert(applied.status == SubstitutionRedundancyApplicationStatus::Applied);
    assert(k.value(internalize(133)) || k.value(internalize(134)));
    kernel_check_invariants(k);
  }

  // Without root compaction, assigned and open variables can move together.
  // The option intentionally discovers a subgroup and may omit those maps.
  Kernel mixed;
  kernel_reserve_vars(mixed, 4);
  kernel_assign_unit(mixed, internalize(1));
  kernel_assign_unit(mixed, internalize(2));
  kernel_add_binary_clause(mixed, internalize(-1), internalize(3),
                           ++mixed.next_clause_id);
  kernel_add_binary_clause(mixed, internalize(-2), internalize(4),
                           ++mixed.next_clause_id);
  SubstitutionRedundancyParameters p;
  p.producer = SubstitutionRedundancyProducer::GraphSymmetry;
  p.graph_symmetry.clause_scope = SymmetryClauseScope::All;
  p.graph_symmetry.maximum_group_ticks = 1'000'000;
  const auto ignore = [](const SubstitutionRedundancyInstance &) { return true; };
  const auto full = substitution_redundancy_enumerate_symmetry(mixed, p, ignore);
  p.graph_symmetry.pointwise_root_stabilizer = true;
  const auto fixed = substitution_redundancy_enumerate_symmetry(mixed, p, ignore);
  assert(full.generators_validated > 0 && fixed.generators_validated == 0);
}

static void generated_sr_noops_do_not_stall_complete_passes() {
  Kernel k = symmetry_fixture();
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
  parameters.orbitopal_orientation = OrbitopalOrientation::Both;
  OnlineInvocation step;
  step.id = 85;
  step.rule = "SubstitutionRedundancy";
  step.parameters = parameters;
  step.granularity = InvocationGranularity::CompleteCurrentPass;
  step.repeat = {RepeatMode::Once, 1};
  step.budget = WorkLimit::unlimited();
  OnlineInprocessProgram program;
  program.sequence = {step};
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 25));
  bool progress = false, skipped = false;
  for (unsigned advances = 0; advances < 16 && !executor.complete();
       ++advances) {
    const InvocationResult result = executor.advance(k);
    assert(result.status != InvocationStatus::InvalidRequest &&
           result.status != InvocationStatus::StaleRequest &&
           result.status != InvocationStatus::Inapplicable &&
           result.status != InvocationStatus::ResourceExhausted);
    progress |= result.made_rule_progress;
    skipped |= result.status == InvocationStatus::NoChange;
  }
  assert(executor.complete() && progress && skipped);
  kernel_check_invariants(k);
}

static void graph_symmetry_reconfigures_and_resumes_online() {
  Kernel k = symmetry_fixture();
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::GraphSymmetry;
  parameters.graph_symmetry.backend_search_budget = 64;
  parameters.graph_symmetry.maximum_graph_vertices = 128;
  parameters.graph_symmetry.maximum_graph_edges = 256;
  parameters.graph_symmetry.maximum_generators = 64;
  parameters.graph_symmetry.maximum_fixing_instances = 64;
  parameters.graph_symmetry.maximum_group_ticks = 100'000;

  OnlineInvocation step;
  step.id = 84;
  step.rule = "SubstitutionRedundancy";
  step.parameters = parameters;
  step.granularity = InvocationGranularity::CompleteCurrentPass;
  step.repeat = {RepeatMode::UntilScopedFixpoint, 8};
  step.budget = WorkLimit::unlimited();
  OnlineInprocessProgram program;
  program.sequence = {step};
  OnlineInprocessExecutor executor;
  std::string error;
  assert(executor.configure(program, 23, ReconfigureMode::ResetRuntime,
                            &error));
  const InvocationResult first = executor.advance(k);
  assert(first.status == InvocationStatus::Completed &&
         first.made_rule_progress && first.configuration_revision == 23);
  const uint64_t consumed = executor.step_consumed(step.id);
  const StateVersion changed = state_version(k);

  auto reconfigured = program;
  auto &changed_parameters = std::get<SubstitutionRedundancyParameters>(
      reconfigured.sequence.front().parameters);
  changed_parameters.graph_symmetry.pointwise_root_stabilizer = true;
  changed_parameters.graph_symmetry.preserve_detector_order = true;
  changed_parameters.graph_symmetry.use_structured_generators = true;
  changed_parameters.graph_symmetry.stabilize_structured_variables = true;
  changed_parameters.graph_symmetry.structured_scope = SymmetryStructuredScope::AxisAnchors;
  changed_parameters.graph_symmetry.seed = 0x6a09e667u;
  changed_parameters.graph_symmetry.fixing_mode =
      SymmetryFixingMode::GeneratorsOnly;
  assert(executor.configure(reconfigured, 24,
                            ReconfigureMode::RetainMatchingStepBudgets,
                            &error));
  assert(executor.step_consumed(step.id) == consumed);
  assert(state_version(k).arena_generation == changed.arena_generation &&
         state_version(k).touch_epoch == changed.touch_epoch &&
         state_version(k).strategy_epoch == changed.strategy_epoch);

  const InvocationResult resumed = executor.advance(k);
  assert(resumed.configuration_revision == 24);
  assert(resumed.status != InvocationStatus::InvalidRequest &&
         resumed.status != InvocationStatus::StaleRequest &&
         resumed.status != InvocationStatus::Interrupted &&
         resumed.status != InvocationStatus::ResourceExhausted);
  kernel_check_invariants(k);
}

static void asymmetric_graph_has_no_symmetry_instance() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2),
                           ++k.next_clause_id);
  kernel_add_binary_clause(k, internalize(-2), internalize(3),
                           ++k.next_clause_id);
  k.unit_ids[2] = ++k.next_clause_id;
  kernel_assign_unit(k, internalize(-3));
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::GraphSymmetry;
  parameters.graph_symmetry.backend_search_budget = 64;
  uint64_t visited = 0;
  const auto enumeration = substitution_redundancy_enumerate_symmetry(
      k, parameters,
      [&](const SubstitutionRedundancyInstance &) {
        ++visited;
        return true;
      });
  assert(enumeration.status == SymmetryEnumerationStatus::Complete);
  assert(!enumeration.generators_validated && !visited);
}

// DejaVu accepts only simple graphs. A repeated binary clause keeps one direct
// literal edge and represents each further copy by a clause vertex, so the
// graph group is the clause-multiset group.
struct RepeatedBinaryRun {
  SymmetryEnumerationResult result;
  uint64_t visited = 0;
};

static RepeatedBinaryRun
repeated_binary_run(unsigned first_copies, unsigned second_copies,
                    bool interleave, uint64_t maximum_ticks = UINT64_MAX,
                    uint64_t maximum_vertices = 128,
                    SymmetryBinaryGraphMode mode =
                        SymmetryBinaryGraphMode::DirectEdges,
                    bool finish_admitted_group = false,
                    uint64_t progress_ticks_per_generator = 0,
                    uint64_t hard_detection_ticks = 0) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const auto add = [&](int a, int b) {
    kernel_add_binary_clause(k, internalize(a), internalize(b),
                             ++k.next_clause_id);
  };
  for (unsigned i = 0; i < std::max(first_copies, second_copies); ++i) {
    if (interleave) {
      if (i < first_copies)
        add(1, 2);
      if (i < second_copies)
        add(4, 3);
    } else if (i < first_copies) {
      add(1, 2);
    }
  }
  if (!interleave)
    for (unsigned i = 0; i < second_copies; ++i)
      add(4, 3);
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::GraphSymmetry;
  parameters.maximum_detection_ticks = maximum_ticks;
  parameters.graph_symmetry.backend_search_budget = 64;
  parameters.graph_symmetry.maximum_graph_vertices = maximum_vertices;
  parameters.graph_symmetry.maximum_graph_edges = 256;
  parameters.graph_symmetry.maximum_generators = 64;
  parameters.graph_symmetry.maximum_fixing_instances = 64;
  parameters.graph_symmetry.maximum_group_ticks = 100'000;
  parameters.graph_symmetry.binary_graph_mode = mode;
  RepeatedBinaryRun run;
  run.result = substitution_redundancy_enumerate_symmetry(
      k, parameters, [&](const SubstitutionRedundancyInstance &) {
        ++run.visited;
        return true;
      }, nullptr, nullptr,
      SymmetryEnumerationScheduling{finish_admitted_group,
                                    progress_ticks_per_generator,
                                    hard_detection_ticks});
  return run;
}

static void direct_binary_edges_encode_repeated_binary_clauses() {
  // Every multiplicity pair and watch order must finish graph detection
  // whose generators all preserve the clause multiset, with the same fixing
  // instances as the clause-vertex encoding of the same formula.
  for (unsigned first = 1; first <= 3; ++first)
    for (unsigned second = 1; second <= 3; ++second)
      for (const bool interleave : {false, true}) {
        const RepeatedBinaryRun direct =
            repeated_binary_run(first, second, interleave);
        const RepeatedBinaryRun vertices = repeated_binary_run(
            first, second, interleave, UINT64_MAX, 128,
            SymmetryBinaryGraphMode::ClauseVertices);
        assert(direct.result.status == SymmetryEnumerationStatus::Complete);
        // Duplicate-sensitive graph completeness does not establish the full
        // semantic clause-set group. Conservatively decline that claim.
        assert(direct.result.complete_group == (first == 1 && second == 1));
        assert(vertices.result.complete_group == direct.result.complete_group);
        assert(direct.result.generators_proposed > 0 &&
               direct.result.generators_proposed ==
                   direct.result.generators_validated);
        assert(direct.visited == direct.result.instances);
        assert(direct.result.instances == vertices.result.instances);
      }

  // Admission and work accounting for (1 2)x2, (3 4): the snapshot charges
  // six watch entries and four variables; the repeat-free direct graph has
  // 12 vertices, and repeat classification charges one tick per binary
  // before the graph's edge charge. Generic discovery is admitted only when
  // the remaining allowance can pay the conservative exact-validation bound
  // for one generator and reserve the same derived quantum for group work.
  const uint64_t snapshot_ticks = 10, binaries = 3, edges = 12;
  const RepeatedBinaryRun below_minimum =
      repeated_binary_run(2, 1, false, UINT64_MAX, 11);
  assert(below_minimum.result.status ==
             SymmetryEnumerationStatus::LimitReached &&
         below_minimum.result.ticks == snapshot_ticks &&
         !below_minimum.visited);
  const RepeatedBinaryRun repeat_over_limit =
      repeated_binary_run(2, 1, false, UINT64_MAX, 12);
  assert(repeat_over_limit.result.status ==
             SymmetryEnumerationStatus::LimitReached &&
         repeat_over_limit.result.ticks == snapshot_ticks + binaries &&
         !repeat_over_limit.visited);
  const RepeatedBinaryRun classification_budget =
      repeated_binary_run(2, 1, false, snapshot_ticks + binaries - 1);
  assert(classification_budget.result.status ==
             SymmetryEnumerationStatus::LimitReached &&
         !classification_budget.result.generators_proposed);
  const RepeatedBinaryRun graph_budget =
      repeated_binary_run(2, 1, false, snapshot_ticks + binaries + edges);
  assert(graph_budget.result.status ==
             SymmetryEnumerationStatus::LimitReached &&
         !graph_budget.result.generators_proposed);
  const uint64_t generator_quantum = 3 + 8 + 2 * 6;
  const RepeatedBinaryRun unpublishable = repeated_binary_run(
      2, 1, false,
      snapshot_ticks + binaries + edges + 2 * generator_quantum - 1);
  assert(!unpublishable.result.generators_proposed);
  const RepeatedBinaryRun detected = repeated_binary_run(
      2, 1, false,
      snapshot_ticks + binaries + edges + 2 * generator_quantum);
  assert(detected.result.status == SymmetryEnumerationStatus::LimitReached &&
         detected.result.ticks ==
             snapshot_ticks + binaries + edges + 2 * generator_quantum &&
         detected.result.generators_proposed > 0 &&
         detected.result.discovery_progress_units ==
             detected.result.generators_proposed &&
         !detected.result.generators_validated && !detected.result.instances &&
         !detected.visited);
  const RepeatedBinaryRun inline_feedback = repeated_binary_run(
      2, 1, false,
      snapshot_ticks + binaries + edges + 2 * generator_quantum,
      128, SymmetryBinaryGraphMode::DirectEdges, false,
      2 * generator_quantum, 100'000);
  const RepeatedBinaryRun inline_full = repeated_binary_run(2, 1, false);
  assert(inline_feedback.result.status == inline_full.result.status &&
         inline_feedback.result.discovery_progress_units > 0 &&
         inline_feedback.result.generators_validated ==
             inline_full.result.generators_validated &&
         inline_feedback.result.instances == inline_full.result.instances &&
         inline_feedback.visited == inline_full.visited);

  // A scope-owned allowance controls admission of fresh discovery. Once the
  // fixing batch is admitted, its proof-bearing result is atomic: finish it,
  // report the true overrun, and leave the enclosing pool exhausted.
  const RepeatedBinaryRun full = repeated_binary_run(1, 1, false);
  assert(full.result.status == SymmetryEnumerationStatus::Complete &&
         full.result.instances && full.result.ticks > 1);
  constexpr uint64_t admission_allowance = 100;
  const RepeatedBinaryRun strict =
      repeated_binary_run(1, 1, false, admission_allowance);
  const RepeatedBinaryRun finalized = repeated_binary_run(
      1, 1, false, admission_allowance, 128,
      SymmetryBinaryGraphMode::DirectEdges, true);
  assert(strict.result.status == SymmetryEnumerationStatus::LimitReached &&
         !strict.result.instances && !strict.visited);
  assert(finalized.result.status == full.result.status);
  assert(finalized.result.ticks == full.result.ticks &&
         finalized.result.ticks > admission_allowance);
  assert(finalized.result.instances == full.result.instances &&
         finalized.visited == full.visited &&
         finalized.result.complete_group == full.result.complete_group);
}

static void general_sr_cursor_is_lazy_complete_and_resumable() {
  Kernel k;
  kernel_reserve_vars(k, 1);
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::GeneralComplete;
  OnlineInvocation step;
  step.id = 68;
  step.rule = "SubstitutionRedundancy";
  step.parameters = parameters;
  step.granularity = InvocationGranularity::CompleteCurrentPass;
  step.budget = WorkLimit::bounded(8);
  OnlineInprocessProgram program;
  program.sequence = {step};
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 20));
  const InvocationResult first = executor.advance(k);
  assert(first.status == InvocationStatus::Completed);
  assert(first.applications == 1 && first.made_rule_progress);
  assert(k.value(internalize(1)) > 0);
  for (unsigned calls = 0; calls < 8 && !executor.complete(); ++calls) {
    const InvocationResult result = executor.advance(k);
    assert(result.status != InvocationStatus::InvalidRequest);
    assert(result.status != InvocationStatus::StaleRequest);
    assert(result.status != InvocationStatus::ResourceExhausted);
  }
  assert(executor.complete());

  Kernel limited;
  kernel_reserve_vars(limited, 1);
  limited.unit_ids[0] = ++limited.next_clause_id;
  kernel_assign_unit(limited, internalize(-1));
  parameters.maximum_general_candidates_per_advance = 1;
  SubstitutionRedundancyGeneralCursor cursor;
  SubstitutionRedundancyInstance instance;
  const auto a = substitution_redundancy_next_general(
      limited, parameters, cursor, instance);
  assert(a.status == SubstitutionRedundancyGeneralStatus::LimitReached);
  const std::vector<uint32_t> after_first = cursor.candidate_digits;
  const std::vector<uint32_t> witness_after_first = cursor.witness_digits;
  const std::vector<uint32_t> substitution_after_first =
      cursor.substitution_digits;
  const auto b = substitution_redundancy_next_general(
      limited, parameters, cursor, instance);
  assert(b.status == SubstitutionRedundancyGeneralStatus::LimitReached ||
         b.status == SubstitutionRedundancyGeneralStatus::Complete);
  assert(cursor.candidate_digits != after_first ||
         cursor.witness_digits != witness_after_first ||
         cursor.substitution_digits != substitution_after_first ||
         cursor.complete);
}

static void repeated_nonunit_sr_is_a_semantic_noop() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  SubstitutionRedundancyInstance instance;
  instance.clause = {internalize(1), internalize(2)};
  instance.witness = {internalize(1)};
  const auto first = substitution_redundancy_apply_detailed(k, instance);
  assert(first.status == SubstitutionRedundancyApplicationStatus::Applied);
  const StateVersion after_first = state_version(k);
  const uint64_t clause_id_after_first = k.next_clause_id;
  const auto repeated = substitution_redundancy_apply_detailed(k, instance);
  assert(repeated.status ==
         SubstitutionRedundancyApplicationStatus::NoChange);
  const StateVersion after_repeat = state_version(k);
  assert(after_repeat.arena_generation == after_first.arena_generation &&
         after_repeat.touch_epoch == after_first.touch_epoch &&
         after_repeat.strategy_epoch == after_first.strategy_epoch);
  assert(k.next_clause_id == clause_id_after_first);
  kernel_check_invariants(k);
}

// A program may retire a variable before its later explicit SR step begins.
// The immutable payload remains syntactically valid, but is no longer live.
static void retired_sr_payload_is_inapplicable_and_reconfigurable() {
  for (unsigned location = 0; location < 4; ++location) {
    Kernel k;
    kernel_reserve_vars(k, 4);
    kernel_add_binary_clause(k, internalize(1), internalize(2), ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(-1), internalize(3), ++k.next_clause_id);
    SubstitutionRedundancyInstance sr;
    sr.clause = {internalize(2)};
    sr.witness = {internalize(3)};
    if (location == 0) sr.clause = {internalize(1)};
    if (location == 1) sr.witness.push_back(internalize(1));
    if (location == 2) sr.substitution = {{internalize(1), internalize(3)}};
    if (location == 3) sr.substitution = {{internalize(4), internalize(1)}};
    OnlineInvocation eliminate;
    eliminate.id = 501;
    eliminate.rule = "BVE";
    eliminate.parameters = BveParameters::from_kernel(k);
    eliminate.work_unit = WorkUnit::BveTicks;
    eliminate.granularity = InvocationGranularity::OneInstance;
    eliminate.instances = {lit_instance(internalize(1))};
    OnlineInvocation selected;
    selected.id = 502;
    selected.rule = "SubstitutionRedundancy";
    selected.parameters = SubstitutionRedundancyParameters{};
    selected.granularity = InvocationGranularity::OneInstance;
    selected.substitution_redundancy_instances = {sr};
    OnlineStrategyProgram program;
    program.sequence = {eliminate, selected};
    OnlineStrategyExecutor executor;
    assert(executor.configure(program, 1));
    const auto eliminated = executor.advance(k);
    assert(eliminated.made_rule_progress && kernel_var_retired(k, 0));
    const auto before = state_version(k);
    const uint64_t ids = k.next_clause_id;
    const uint64_t calls = k.stats.sr_applications;
    assert(substitution_redundancy_apply_detailed(k, sr).status ==
           SubstitutionRedundancyApplicationStatus::Inapplicable);
    assert(state_version(k).strategy_epoch == before.strategy_epoch);
    for (unsigned retry = 0; retry < 2; ++retry) {
      const auto result = executor.advance(k);
      assert(result.status == InvocationStatus::Inapplicable);
      assert(!result.applications && !result.made_rule_progress);
      assert(executor.step_consumed(502) == 0);
      const auto after = state_version(k);
      assert(before.arena_generation == after.arena_generation &&
             before.touch_epoch == after.touch_epoch &&
             after.strategy_epoch == before.strategy_epoch + retry + 1);
      // The registry records each attempted action, including an inapplicable
      // one; formula/proof state and the semantic application budget stay put.
      assert(k.next_clause_id == ids && k.stats.sr_applications == calls);
    }
    // Retirement must not hide bad storage or contradictory witness syntax.
    for (unsigned defect = 0; defect < 4; ++defect) {
      auto bad = sr;
      if (defect == 0) bad.clause.push_back(INVALID_LIT);
      if (defect == 1) bad.clause.push_back(negate(bad.clause.front()));
      if (defect == 2) bad.witness.push_back(internalize(-3));
      if (defect == 3) bad.substitution.push_back({internalize(4), internalize(4)});
      const auto invalid = substitution_redundancy_apply_detailed(k, bad);
      assert(invalid.status == SubstitutionRedundancyApplicationStatus::Invalid);
      assert(k.next_clause_id == ids && k.stats.sr_applications == calls);
    }
    selected.substitution_redundancy_instances = {
        SubstitutionRedundancyInstance{{internalize(2)}, {internalize(3)}, {}}};
    program.sequence = {selected};
    assert(executor.configure(program, 2, ReconfigureMode::RetainMatchingStepBudgets));
    const auto resumed = executor.advance(k);
    assert(resumed.status == InvocationStatus::Completed && resumed.made_rule_progress);
    assert(k.value(internalize(2)) > 0 && executor.step_consumed(502) == 1);
    kernel_check_invariants(k);
  }
}

static void root_fixing_after_amo_completion_is_recorded() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2),
                           ++k.next_clause_id);
  kernel_add_binary_clause(k, internalize(-1), internalize(-2),
                           ++k.next_clause_id);

  // Model an earlier generated ULC pair without relying on a particular
  // producer fixture.  The two source clauses make -1 RUP, so this exercises
  // the ordinary checked SR path used by the later symmetry producers.
  k.stats.sr_unique_literal_amo_binaries = 1;
  SubstitutionRedundancyInstance unit;
  unit.clause = {internalize(-1)};
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::GraphSymmetry;
  const auto result =
      substitution_redundancy_apply_detailed(k, unit, parameters);
  assert(result.status == SubstitutionRedundancyApplicationStatus::Applied);
  assert(k.value(internalize(-1)) > 0);
  assert(k.stats.sr_post_amo_nonhelper_applied == 1);
  assert(k.stats.sr_post_amo_graph_root_units == 1);
  kernel_check_invariants(k);
}

static void malformed_sr_is_rejected_before_true_clause_noop() {
  Kernel k;
  kernel_reserve_vars(k, 1);
  k.unit_ids[0] = ++k.next_clause_id;
  kernel_assign_unit(k, internalize(1));
  SubstitutionRedundancyInstance instance;
  instance.clause = {internalize(1), INVALID_LIT};
  const StateVersion before = state_version(k);
  const uint64_t clause_id_before = k.next_clause_id;
  const uint32_t propagation_head_before = k.propagate_head;
  const uint64_t applications_before = k.stats.sr_applications;
  const auto result = substitution_redundancy_apply_detailed(k, instance);
  assert(result.status == SubstitutionRedundancyApplicationStatus::Invalid);
  assert(result.rule_result == ApplyResult::NoOp);
  const StateVersion after = state_version(k);
  assert(after.arena_generation == before.arena_generation &&
         after.touch_epoch == before.touch_epoch &&
         after.strategy_epoch == before.strategy_epoch);
  assert(k.next_clause_id == clause_id_before);
  assert(k.propagate_head == propagation_head_before);
  assert(k.stats.sr_applications == applications_before);
}

static void unsupported_sr_proof_format_is_transactional() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const auto add_binary = [&](int a, int b) {
    kernel_add_binary_clause(k, internalize(a), internalize(b),
                             ++k.next_clause_id);
  };
  add_binary(1, 2);
  add_binary(3, 4);
  add_binary(-1, -3);
  add_binary(-2, -4);

  char path[] = "/tmp/tactix-sr-drat-XXXXXX";
  const int descriptor = mkstemp(path);
  assert(descriptor >= 0);
  close(descriptor);
  {
    Proof proof;
    assert(proof.open(path, ProofFormat::DratAscii));
    k.proof = &proof;
    proof.bind_variable_map(&k.internal_to_external);

    SubstitutionRedundancyInstance instance;
    instance.clause = {internalize(-1)};
    instance.witness = {internalize(-1), internalize(3)};
    instance.substitution = {{internalize(2), internalize(4)},
                             {internalize(4), internalize(2)}};
    const StateVersion before = state_version(k);
    const uint64_t clause_id_before = k.next_clause_id;
    const auto result = substitution_redundancy_apply_detailed(k, instance);
    assert(result.status ==
           SubstitutionRedundancyApplicationStatus::UnsupportedProofFormat);
    assert(result.rule_result == ApplyResult::NoOp);
    const StateVersion after = state_version(k);
    assert(after.arena_generation == before.arena_generation &&
           after.touch_epoch == before.touch_epoch &&
           after.strategy_epoch == before.strategy_epoch);
    assert(k.next_clause_id == clause_id_before);
    assert(k.value(internalize(-1)) == 0);
    proof.flush();
    k.proof = nullptr;
  }
  FILE *written = fopen(path, "rb");
  assert(written);
  assert(fgetc(written) == EOF);
  fclose(written);
  unlink(path);
}

static void later_sr_batch_member_can_become_inapplicable() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const auto add_binary = [&](int a, int b) {
    kernel_add_binary_clause(k, internalize(a), internalize(b),
                             ++k.next_clause_id);
  };
  add_binary(1, 2);
  add_binary(3, 4);
  add_binary(-1, -3);
  add_binary(-2, -4);

  SubstitutionRedundancyInstance first;
  first.clause = {internalize(-1)};
  first.witness = {internalize(-1), internalize(3)};
  first.substitution = {{internalize(2), internalize(4)},
                        {internalize(4), internalize(2)}};
  SubstitutionRedundancyInstance second;
  second.clause = {internalize(-3)};
  second.witness = {internalize(-3), internalize(1)};
  second.substitution = {{internalize(4), internalize(2)},
                         {internalize(2), internalize(4)}};

  OnlineInvocation step;
  step.id = 70;
  step.rule = "SubstitutionRedundancy";
  step.parameters = SubstitutionRedundancyParameters{};
  step.granularity = InvocationGranularity::SelectedBatch;
  step.substitution_redundancy_instances = {first, second};
  step.budget = WorkLimit::unlimited();
  OnlineInprocessProgram program;
  program.sequence = {step};
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 22));
  const InvocationResult applied = executor.advance(k);
  assert(applied.status == InvocationStatus::Completed &&
         applied.made_rule_progress);
  const InvocationResult inapplicable = executor.advance(k);
  assert(inapplicable.status == InvocationStatus::Inapplicable);
  assert(inapplicable.applications == 0);
  assert(!inapplicable.made_rule_progress);
}

// satsuma.h iteration_max: one pass plus clamp(ceil((2^23 / size)^1.5), 1, limit)
// repetitions, size = literals + variables of the live formula.
static void size_scaled_fixpoint_follows_the_owner_scale() {
  using namespace tactix::strategy;
  Kernel small;
  kernel_reserve_vars(small, 4);
  kernel_add_binary_clause(small, internalize(1), internalize(2),
                           ++small.next_clause_id);
  assert(repeat_round_limit(small, {RepeatMode::UntilScopedFixpoint, 7}) == 7);
  assert(repeat_round_limit(small, {RepeatMode::UntilSizeScaledFixpoint, 128}) ==
         129);
  assert(repeat_round_limit(small, {RepeatMode::UntilSizeScaledFixpoint, 0}) > 129);

  // 600 variables and 170,000 distinct binaries: size 340,600, scale 123.
  Kernel large;
  constexpr int variables = 600;
  kernel_reserve_vars(large, variables);
  uint64_t added = 0;
  for (int a = 1; a <= variables && added < 170'000; ++a)
    for (int b = a + 1; b <= variables && added < 170'000; ++b) {
      kernel_add_binary_clause(large, internalize(a), internalize(b),
                               ++large.next_clause_id);
      ++added;
    }
  assert(added == 170'000);
  const double size = 2.0 * 170'000 + variables;
  const uint64_t scale = static_cast<uint64_t>(
      std::ceil(std::pow(static_cast<double>(1u << 23) / size, 1.5)));
  assert(scale > 1 && scale < 128);
  assert(repeat_round_limit(large, {RepeatMode::UntilSizeScaledFixpoint, 128}) ==
         scale + 1);
  assert(repeat_round_limit(large, {RepeatMode::UntilSizeScaledFixpoint, 16}) == 17);
  assert(fixpoint_repeat(RepeatMode::UntilSizeScaledFixpoint) &&
         !fixpoint_repeat(RepeatMode::Count));
}

static void sr_producer_settings_are_validated() {
  OnlineInprocessProgram program;
  OnlineInvocation step;
  step.id = 69;
  step.rule = "SubstitutionRedundancy";
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
  parameters.minimum_orbitopal_rows = 1;
  step.parameters = parameters;
  step.granularity = InvocationGranularity::CanonicalApplication;
  step.budget = WorkLimit::bounded(1);
  program.sequence = {step};
  OnlineInprocessExecutor executor;
  std::string error;
  assert(!executor.configure(program, 21, ReconfigureMode::ResetRuntime,
                             &error));
  assert(error == "SR parameters are invalid");

  // Settings for an inactive producer cannot narrow the explicit X7 surface.
  parameters = {};
  parameters.graph_symmetry.error_bound = 0;
  assert(parameters.valid());
  parameters.producer = SubstitutionRedundancyProducer::GraphSymmetry;
  assert(!parameters.valid());
}

// Native-checker witness mode is run on SLURM. The normal micro-test entry
// below never launches a solver. Input includes exactly twenty selected victims
// containing variable 1, with an independent SAT/UNSAT component for the tail.
static int proof_witness(const char *input, const char *output,
                         const char *format, const char *rule_name,
                         const char *symmetry_profile = "unique",
                         std::optional<uint64_t> generator_cap = std::nullopt,
                         std::optional<bool> root_stabilizer = std::nullopt,
                         const char *prefix_path = nullptr) {
  OnlineStrategyProgram prefix_program;
  OnlineStrategyExecutor prefix_executor;
  if (prefix_path) {
    // The ordinary wire format names internal literals at the boundary after
    // initial preprocessing. This is caller-supplied diagnostic data, not a
    // producer certificate or an alternate rule-validation path.
    std::ifstream stream(prefix_path);
    const std::string encoded{std::istreambuf_iterator<char>(stream),
                              std::istreambuf_iterator<char>()};
    std::string error;
    if (std::strcmp(rule_name, "CombinedSR") || !stream ||
        !deserialize_online_strategy_program(encoded, prefix_program, &error) ||
        !prefix_executor.configure(prefix_program, 1,
                                   ReconfigureMode::ResetRuntime, &error)) {
      std::fprintf(stderr, "invalid diagnostic prefix: %s\n", error.c_str());
      return 2;
    }
  }
  Kernel k;
  Proof proof;
  const std::string proof_format = format;
  assert(proof.open(output, proof_format == "lrat" ? ProofFormat::Lrat
                            : proof_format == "dsr" ? ProofFormat::DsrAscii
                                                    : ProofFormat::DratAscii));
  k.proof = &proof;
  proof.bind_variable_map(&k.internal_to_external);
  FILE *cnf = fopen(input, "r");
  assert(cnf && !parse_dimacs(cnf, k));
  fclose(cnf);
  if (!std::strcmp(rule_name, "OrbitopalSR") ||
      !std::strcmp(rule_name, "OrbitopalProductSR") ||
      !std::strcmp(rule_name, "OrbitopalCliqueSR") ||
      !std::strcmp(rule_name, "OrbitopalAmoSR") ||
      !std::strcmp(rule_name, "SymmetrySR") ||
      !std::strcmp(rule_name, "SstSR") ||
      !std::strcmp(rule_name, "FixAndCutSR") ||
      !std::strcmp(rule_name, "FixAndCutClauseSR") ||
      !std::strcmp(rule_name, "CombinedSR")) {
    const bool product_diagnostic = !std::strcmp(rule_name, "OrbitopalProductSR");
    const bool sst_only = !std::strcmp(rule_name, "SstSR");
    const bool fix_and_cut_only = !std::strcmp(rule_name, "FixAndCutSR") ||
                                  !std::strcmp(rule_name,
                                               "FixAndCutClauseSR") ||
                                  !std::strcmp(rule_name, "CombinedSR");
    const bool combined = !std::strcmp(rule_name, "CombinedSR");
    SubstitutionRedundancyParameters orbitopal_parameters;
    const bool amo_diagnostic = !std::strcmp(rule_name, "OrbitopalAmoSR");
    OnlineInprocessProgram initial_program;
    OnlineInprocessProgram producer_program;
    OnlineInprocessProgram unrestricted_program;
    if (combined) {
      auto stages = DefaultStrategyConfig::structured_root_preprocessing_stages();
      // CombinedSR is an isolated SR diagnostic, not DefaultPolicy's preceding
      // SAT-model opportunity. Retain its three SR phases explicitly.
      std::erase_if(stages, [](const auto &stage) {
        return std::all_of(stage.program.sequence.begin(), stage.program.sequence.end(),
            [](const auto &step) { return step.rule == "Lucky"; });
      });
      assert(stages.size() == 3);
      for (auto &stage : stages)
        for (auto &step : stage.program.sequence)
          if (auto *p = std::get_if<SubstitutionRedundancyParameters>(&step.parameters)) {
            if (generator_cap)
              p->graph_symmetry.maximum_generators = *generator_cap;
            p->graph_symmetry.pointwise_root_stabilizer = root_stabilizer.value_or(true);
          }
      initial_program = std::move(stages[0].program);
      producer_program = std::move(stages[1].program);
      unrestricted_program = std::move(stages[2].program);
    } else {
    orbitopal_parameters.producer =
        SubstitutionRedundancyProducer::Orbitopal;
    const bool clique_order =
        !std::strcmp(rule_name, "OrbitopalCliqueSR") ||
        !std::strcmp(rule_name, "OrbitopalAmoSR") ||
        !std::strcmp(symmetry_profile, "clique");
    // OrbitopalAmoSR mirrors Satsuma's startup formula construction. Its
    // `ulc_add_amo` pass inserts every missing pairwise AMO clause into the
    // internal formula before graph refinement and symmetry detection.
    orbitopal_parameters.complete_unique_literal_amos = true;
    orbitopal_parameters.orbitopal_orientation =
        amo_diagnostic ? OrbitopalOrientation::Rows
                       : !std::strcmp(rule_name, "SymmetrySR") && !clique_order
                             ? OrbitopalOrientation::Columns
                             : OrbitopalOrientation::Rows;
    if (clique_order) {
      orbitopal_parameters.orbitopal_column_order =
          OrbitopalColumnOrder::CooccurrenceMaximumClique;
      orbitopal_parameters.orbitopal_completion =
          OrbitopalCompletion::IndividualizedGraphCompletion;
      orbitopal_parameters.maximum_orbitopal_clique_vertices = 100'000;
      orbitopal_parameters.maximum_orbitopal_clique_ticks = 10'000'000;
      orbitopal_parameters.graph_symmetry.maximum_graph_vertices = 100'000;
      orbitopal_parameters.graph_symmetry.maximum_graph_edges = 1'000'000;
      orbitopal_parameters.graph_symmetry.maximum_generators = 128;
      orbitopal_parameters.graph_symmetry.backend_search_budget = 64;
      orbitopal_parameters.graph_symmetry.backend_support_budget = 1'000'000;
      orbitopal_parameters.graph_symmetry.backend_component_size_limit =
          100'000;
    }
    if (amo_diagnostic && !clique_order)
      orbitopal_parameters.orbitopal_column_order =
          OrbitopalColumnOrder::Canonical;
    if (amo_diagnostic) {
      // This witness mode isolates Satsuma-style color-refinement rows.  A
      // zero generic-search budget makes an inexact refined candidate return
      // immediately instead of spending the five-minute diagnostic envelope
      // in the fallback group search.
      orbitopal_parameters.minimum_orbitopal_rows = 3;
      orbitopal_parameters.orbitopal_generator_fallback = false;
      // The pinned Satsuma trace shows that MVRoundRobin uses row symmetry and
      // rejects every reconstructed row-column candidate. Keep this
      // witness-only probe bounded while checking the corrected group-orbit
      // row candidates.
      orbitopal_parameters.maximum_orbitopal_refinement_orbits = 16;
    }
    if (product_diagnostic) {
      orbitopal_parameters.orbitopal_orientation = OrbitopalOrientation::Both;
      orbitopal_parameters.orbitopal_completion =
          OrbitopalCompletion::IndividualizedGraphCompletion;
      orbitopal_parameters.orbitopal_discovery_order = OrbitopalDiscoveryOrder::RefinementOnly;
      orbitopal_parameters.orbitopal_generator_fallback = true;
      orbitopal_parameters.complete_unique_literal_amos = false;
    }
    OnlineInvocation orbitopal;
    orbitopal.id = 82;
    orbitopal.rule = "SubstitutionRedundancy";
    orbitopal.parameters = orbitopal_parameters;
    orbitopal.granularity = InvocationGranularity::CompleteCurrentPass;
    orbitopal.budget = WorkLimit::unlimited();


    if (!sst_only && (!fix_and_cut_only || combined))
      producer_program.sequence.push_back(orbitopal);
    if (amo_diagnostic)
      producer_program.repeat = {RepeatMode::Once, 1};
    if (product_diagnostic)
      producer_program.repeat = {RepeatMode::UntilScopedFixpoint, 16};
    if (!std::strcmp(rule_name, "SymmetrySR") || sst_only ||
        fix_and_cut_only) {
      assert(sst_only || !std::strcmp(symmetry_profile, "unique") ||
             !std::strcmp(symmetry_profile, "all") ||
             !std::strcmp(symmetry_profile, "clique"));
      SubstitutionRedundancyParameters symmetry_parameters;

      symmetry_parameters.producer =
          SubstitutionRedundancyProducer::GraphSymmetry;
      symmetry_parameters.graph_symmetry.clause_scope =
          sst_only || fix_and_cut_only ||
                  !std::strcmp(symmetry_profile, "all")
              ? SymmetryClauseScope::All
              : SymmetryClauseScope::UniqueLiteralsOnly;
      symmetry_parameters.graph_symmetry.clause_selection =
          sst_only ? SymmetryClauseSelection::InputOrder
                   : SymmetryClauseSelection::AmoThenShortest;
      symmetry_parameters.graph_symmetry.maximum_graph_vertices = 100'000;
      symmetry_parameters.graph_symmetry.maximum_graph_edges = 1'000'000;
      symmetry_parameters.graph_symmetry.maximum_generators = 128;
      symmetry_parameters.graph_symmetry.backend_search_budget = 64;
      symmetry_parameters.graph_symmetry.backend_support_budget = 64ull << 20;
      symmetry_parameters.graph_symmetry.backend_component_size_limit =
          100'000;
      symmetry_parameters.graph_symmetry.maximum_fixing_instances =
          128;
      symmetry_parameters.graph_symmetry
          .maximum_generator_instances_per_pass = 1;
      symmetry_parameters.graph_symmetry.maximum_group_ticks =
          20'000'000'000ull;
      if (fix_and_cut_only) {
        symmetry_parameters.graph_symmetry.chain_mode =
            SymmetryChainMode::FixAndCut;
        symmetry_parameters.graph_symmetry.base_selection =
            !std::strcmp(rule_name, "FixAndCutClauseSR")
                ? SymmetryBaseSelection::ClauseOrder
                : SymmetryBaseSelection::CommonClauseAmo;
      }
      OnlineInvocation symmetry;
      symmetry.id = 83;
      symmetry.rule = "SubstitutionRedundancy";
      symmetry.parameters = symmetry_parameters;
      symmetry.granularity = InvocationGranularity::CompleteCurrentPass;
      symmetry.budget = WorkLimit::unlimited();

      producer_program.sequence.push_back(symmetry);

      // A root preprocessing invocation exhausts the configured symmetry
      // producer to its scoped fixpoint.  Fix-and-cut may expose a fresh
      // clausal fixing or a smaller stabilizer after each complete cut level,
      // so stopping after its first pass does not exercise the default
      // schedule and can leave most of the chain unapplied.
      producer_program.repeat = {
          RepeatMode::UntilScopedFixpoint, 16u};
    }
    // Diagnostic CLI maps directly to the existing typed per-call parameter.
    // No environment-dependent rule behavior or production default changes.
    for (auto *program : {&initial_program, &producer_program}) {
      for (auto &invocation : program->sequence)
        if (auto *parameters = std::get_if<SubstitutionRedundancyParameters>(
                &invocation.parameters)) {

          if (generator_cap)
            parameters->graph_symmetry.maximum_generators = *generator_cap;
          parameters->graph_symmetry.pointwise_root_stabilizer =
              root_stabilizer.value_or(combined);
        }
    }
    }
    OnlineInprocessExecutor producer_executor;
    assert(producer_executor.configure(producer_program, 1));
    OnlineStrategySequence continuing_executor;
    OnlineStrategySequence initial_executor;
    OnlineStrategySequence unrestricted_executor;
    if (combined) {
      continuing_executor.configure(producer_program);
      initial_executor.configure(initial_program);
      unrestricted_executor.configure(unrestricted_program);
    }
    const auto producer_complete = [&] {
      return combined ? initial_executor.complete() &&
                            (!prefix_path || prefix_executor.complete()) &&
                            continuing_executor.complete() &&
                            unrestricted_executor.complete()
                      : producer_executor.complete();
    };
    const auto producer_started = std::chrono::steady_clock::now();
    uint64_t effective = 0;
    int terminal = 0;
    while (!amo_diagnostic && !producer_complete()) {
      const auto step_started = std::chrono::steady_clock::now();
      const uint64_t applications_before = k.stats.sr_applications;
      const uint64_t applied_before = k.stats.sr_applied;
      const uint64_t validation_before = k.stats.sr_validation_ticks;
      const bool prefix_step = combined && initial_executor.complete() &&
                               prefix_path && !prefix_executor.complete();
      const InvocationResult advanced = combined
          ? (!initial_executor.complete() ? initial_executor.advance(k)
             : prefix_step ? prefix_executor.advance(k)
             : !continuing_executor.complete() ? continuing_executor.advance(k)
                                               : unrestricted_executor.advance(k))
          : producer_executor.advance(k);
      const std::chrono::duration<double> step_elapsed =
          std::chrono::steady_clock::now() - step_started;
      if (!amo_diagnostic) {
        std::printf("c structured step %llu seconds %.6f progress %u status %u "
                    "sr-attempts %llu sr-applied %llu sr-validation %llu\n",
                    static_cast<unsigned long long>(advanced.step_id),
                    step_elapsed.count(), advanced.made_rule_progress,
                    static_cast<unsigned>(advanced.status),
                    static_cast<unsigned long long>(k.stats.sr_applications -
                                                    applications_before),
                    static_cast<unsigned long long>(k.stats.sr_applied -
                                                    applied_before),
                    static_cast<unsigned long long>(k.stats.sr_validation_ticks -
                                                    validation_before));
        std::fflush(stdout);
      }
      effective += advanced.made_rule_progress;
      if (prefix_step) {
        std::printf("c diagnostic prefix step %llu status %u progress %u root %llu\n",
                    static_cast<unsigned long long>(advanced.step_id),
                    static_cast<unsigned>(advanced.status),
                    advanced.made_rule_progress,
                    static_cast<unsigned long long>(k.stats.root_assignments));
        std::fflush(stdout);
        if (advanced.status != InvocationStatus::Completed &&
            advanced.status != InvocationStatus::ScopedFixpoint &&
            advanced.status != InvocationStatus::NoChange &&
            advanced.status != InvocationStatus::Unsat) {
          std::fputs("diagnostic prefix did not complete; refusing continuation\n",
                     stderr);
          return 2;
        }
      }
      if (advanced.status == InvocationStatus::Unsat) {
        terminal = 20;
        break;
      }
      if (advanced.status == InvocationStatus::BudgetExhausted && combined &&
          !producer_complete()) {
        std::printf("c structured producer budget stop %llu; continue next producer\n",
                    static_cast<unsigned long long>(advanced.step_id));
        continue;
      }
      if (advanced.status == InvocationStatus::BudgetExhausted ||
          advanced.status == InvocationStatus::ResourceExhausted ||
          advanced.status == InvocationStatus::Interrupted)
        break;
      assert(advanced.status != InvocationStatus::InvalidRequest &&
             advanced.status != InvocationStatus::StaleRequest);
    }
    std::printf("c effective structured SR transitions %llu\n",
                static_cast<unsigned long long>(effective));
    const std::chrono::duration<double> producer_elapsed =
        std::chrono::steady_clock::now() - producer_started;
    std::printf("c structured preprocessing seconds %.6f\n",
                producer_elapsed.count());
    std::printf("c structured totals applications %llu applied %llu "
                "validation %llu root %llu\n",
                static_cast<unsigned long long>(k.stats.sr_applications),
                static_cast<unsigned long long>(k.stats.sr_applied),
                static_cast<unsigned long long>(k.stats.sr_validation_ticks),
                static_cast<unsigned long long>(k.stats.root_assignments));
    if (amo_diagnostic) {
      uint64_t post_instances = 0;
      const OrbitopalEnumerationResult post =
          substitution_redundancy_enumerate_orbitopal(
              k, orbitopal_parameters,
              [&](const SubstitutionRedundancyInstance &) {
                ++post_instances;
                return true;
              });
      std::printf("c structured post-enumeration status %u matrices %llu "
                  "instances %llu ticks %llu refinement-orbits %llu "
                  "reduced %llu max-orbit %llu max-reduced %llu "
                  "max-columns %llu ulc-columns %llu exact-failures %llu "
                  "fallbacks %llu product-descents %llu product-shapes %llu "
                  "product-rows %llu product-columns %llu product-exact %llu "
                  "product-ulc %llu\n",
                  static_cast<unsigned>(post.status),
                  static_cast<unsigned long long>(post.matrices),
                  static_cast<unsigned long long>(post_instances),
                  static_cast<unsigned long long>(post.ticks),
                  static_cast<unsigned long long>(post.refinement_orbits),
                  static_cast<unsigned long long>(
                      post.reduced_refinement_orbits),
                  static_cast<unsigned long long>(
                      post.largest_refinement_orbit),
                  static_cast<unsigned long long>(post.largest_reduced_orbit),
                  static_cast<unsigned long long>(
                      post.largest_refinement_columns),
                  static_cast<unsigned long long>(
                      post.refinement_unique_literal_columns),
                  static_cast<unsigned long long>(
                      post.refinement_exact_failures),
                  static_cast<unsigned long long>(post.generator_fallbacks),
                  static_cast<unsigned long long>(
                      post.product_stabilizer_descents),
                  static_cast<unsigned long long>(post.product_shapes),
                  static_cast<unsigned long long>(post.largest_product_rows),
                  static_cast<unsigned long long>(
                      post.largest_product_columns),
                  static_cast<unsigned long long>(post.product_exact_actions),
                  static_cast<unsigned long long>(post.product_ulc_actions));
    }
    std::fflush(stdout);
    if (amo_diagnostic) {
      proof.flush();
      std::puts(terminal == 20 ? "s UNSATISFIABLE" : "s UNKNOWN");
      return terminal;
    }
    const int result = terminal ? terminal : DefaultPolicy{}.solve(k, nullptr);
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
      std::puts(result == 20 ? "s UNSATISFIABLE" : "s UNKNOWN");
    }
    return result;
  }
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
  std::optional<uint64_t> generator_cap;
  std::optional<bool> root_stabilizer;
  if (argc == 8 || argc == 9) {
    if (std::strcmp(argv[7], "0") && std::strcmp(argv[7], "1"))
      return 2;
    root_stabilizer = !std::strcmp(argv[7], "1");
  }
  if (argc >= 7 && argc <= 9) {
    uint64_t value = 0;
    const char *end = argv[6] + std::strlen(argv[6]);
    const auto parsed = std::from_chars(argv[6], end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end) {
      std::fputs("invalid diagnostic generator cap\n", stderr);
      return 2;
    }
    generator_cap = value;
  }
  if (argc >= 5 && argc <= 9)
    return proof_witness(argv[1], argv[2], argv[3], argv[4],
                         argc >= 6 ? argv[5] : "unique", generator_cap,
                         root_stabilizer, argc == 9 ? argv[8] : nullptr);
  assert(argc == 1);
  validation_is_transactional();
  invalid_discriminants_do_not_replace_configuration();
  rule_work_budget_is_per_call_and_scoped();
  material_settings_are_per_call_and_scoped();
  complete_pass_reaches_a_scoped_fixpoint();
  caller_selected_fixpoint_is_scoped_and_budgeted();
  budget_survives_compatible_online_reconfiguration();
  budget_continuation_preserves_limits_and_stops();
  shared_sr_discovery_budget_does_not_refill();
  finish_round_budget_policy_preserves_tail_and_resume();
  sequence_configuration_is_transactional();
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
  exact_sr_instance_uses_shared_online_composition();
  post_application_propagates_and_publishes_root_units();
  signed_sr_sources_emit_canonical_dsr_mappings();
  sr_failure_statuses_preserve_the_pending_application();
  orbitopal_complete_pass_discovers_structured_sr();
  orbitopal_screen_is_budgeted_and_interruptible();
  unique_literal_amo_phase_is_checked_bounded_and_composable();
  orbitopal_column_orientation_is_typed_and_effective();
  orbitopal_product_refinement_requires_exact_two_axis_action();
  orbitopal_product_extends_coupled_variables();
  orbitopal_certificate_rechecks_propagated_root_closure();
  orbitopal_certified_prefix_preserves_small_sat_fixture();
  orbitopal_alignment_spans_connected_columns();
  orbitopal_row_group_certificate_rejects_missing_images();
  orbitopal_reuses_only_equivalent_failed_matrices();
  orbitopal_column_transport_requires_a_bijection();
  orbitopal_clique_order_is_typed_exact_and_budgeted();
  orbitopal_clique_order_matches_exhaustive_connected_graphs();
  orbitopal_refinement_completes_hidden_columns();
  orbitopal_refinement_starts_from_one_unique_column();
  orbitopal_discovery_scopes_are_independent();
  orbitopal_refinement_cells_need_no_generic_generators();
  for (bool preserve : {false, true})
    graph_symmetry_is_exact_bounded_and_interruptible(preserve);
  stabilizer_chain_adds_all_cuts_before_rup_unit();
  large_domain_generator_fixing_uses_units_without_a_chain();
  fix_and_cut_reaches_nonclausal_orbits();
  amo_stabilizer_chain_propagates_complements_online();
  every_root_assignment_constrains_graph_symmetry();
  explicit_pivot_mapping_must_repair_candidate();
  explicit_pivot_mappings_remain_applicable();
  pointwise_root_discovery_preserves_explicit_sr();
  fixed_root_units_do_not_consume_graph_admission();
  generated_sr_noops_do_not_stall_complete_passes();
  graph_symmetry_reconfigures_and_resumes_online();
  asymmetric_graph_has_no_symmetry_instance();
  direct_binary_edges_encode_repeated_binary_clauses();
  general_sr_cursor_is_lazy_complete_and_resumable();
  repeated_nonunit_sr_is_a_semantic_noop();
  retired_sr_payload_is_inapplicable_and_reconfigurable();
  root_fixing_after_amo_completion_is_recorded();
  malformed_sr_is_rejected_before_true_clause_noop();
  unsupported_sr_proof_format_is_transactional();
  later_sr_batch_member_can_become_inapplicable();
  size_scaled_fixpoint_follows_the_owner_scale();
  sr_producer_settings_are_validated();
  complete_pass_reenumerates_after_its_own_collection();
  sweep_parameters_are_exact_per_invocation();
  zero_victim_compaction_reports_semantic_progress();
  rule_parameters_are_scoped_per_call();
  collection_does_not_age_the_search_database();
  std::puts("Online sequence, granularity, fixpoint, budget and reconfigure PASS");
}
