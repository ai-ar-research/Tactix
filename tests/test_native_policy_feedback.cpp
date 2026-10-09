#undef NDEBUG
#include "policies/default_policy.hpp"
#include "strategy/feedback_trace.hpp"
#include "strategy/native_feedback.hpp"
#include "strategy/action_execution.hpp"

#include <cassert>
#include <cstring>
#include <functional>
#include <unistd.h>

using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;

struct Capture final : FeedbackReceiver {
  unsigned begun = 0, ended = 0;
  std::vector<ActionFeedback> actions;
  std::vector<SearchIntervalFeedback> intervals;
  std::function<void()> check;
  void session_begin(const Observation &) override { ++begun; }
  void after_action(const ActionFeedback &value) override {
    assert(value.sequence == actions.size() + 1);
    assert(value.native_request && !value.native_request->effort_currency.empty());
    if (value.kind == FeedbackKind::Support) {
      assert(!value.outcome.invocation.applications && !value.outcome.invocation.made_rule_progress);
      if (value.outcome.invocation.work_unit == WorkUnit::Applications)
        assert(!value.outcome.invocation.work && !value.outcome.composition_overshoot);
    }
    actions.push_back(value);
    if (check) check();
  }
  void search_interval(const SearchIntervalFeedback &value) override {
    assert(value.sequence == intervals.size() + 1);
    intervals.push_back(value);
    if (check) check();
  }
  void session_end(const Observation &, int, StopReason) override { ++ended; }
};

static Kernel pigeonhole(bool cold = false) {
  Kernel k;
  if (cold) kernel_defer_cold_state(k);
  constexpr unsigned pigeons = 5, holes = 4;
  kernel_reserve_vars(k, pigeons * holes);
  auto cell = [](unsigned p, unsigned h) { return make_lit(p * holes + h, false); };
  for (unsigned p = 0; p < pigeons; ++p) {
    std::vector<Lit> clause;
    for (unsigned h = 0; h < holes; ++h) clause.push_back(cell(p, h));
    kernel_add_large_clause(k, clause.data(), holes, false, 0, ++k.next_clause_id);
    for (unsigned q = p + 1; q < pigeons; ++q)
      for (unsigned h = 0; h < holes; ++h)
        kernel_add_binary_clause(k, negate(cell(p, h)), negate(cell(q, h)), ++k.next_clause_id);
  }
  return k;
}

static DefaultStrategyConfig search_profile() {
  DefaultStrategyConfig config;
  config.inprocess.program.clear();
  config.inprocess.append_unlisted_rules = false;
  config.inprocess.structured_root_stages.clear();
  config.inprocess.structured_root_bounded_backtracking_stages.clear();
  config.inprocess.structured_root_large_domain_stages.clear();
  config.inprocess.initial_fast_elimination = false;
  config.rephase.late_lucky = false;
  config.rephase.stable_only = false;
  config.rephase.initial_interval = config.rephase.interval = 1;
  config.rephase.rotation = {DefaultRephaseSlot::Walk};
  config.rephase.walk_permille = 1;
  config.mode.initial_conflicts = config.mode.interval = 1;
  config.restart.reluctant_period = 2;
  config.reduction.initial_interval = 5;
  return config;
}

static void test_completion_accounting() {
  Kernel k;
  Capture capture;
  unsigned reads = 0, builds = 0;
  auto observe = [](const Kernel &kernel, const void *owner) {
    ++*static_cast<unsigned *>(const_cast<void *>(owner));
    return observe_kernel(kernel);
  };
  execution::PolicyCompletion disabled(k, nullptr, observe, &reads);
  assert(disabled.execute(FeedbackKind::Support, "native", 0,
      [&] { ++builds; return execution::NativeFeedbackSpec{}; },
      [] { return ApplyResult::NoOp; }) == ApplyResult::NoOp);
  assert(!reads && !builds);
  execution::PolicyCompletion enabled(k, &capture, observe, &reads);
  enabled.begin();
  auto build = [&] {
    execution::NativeFeedbackSpec spec;
    spec.meter = [](const Kernel &kernel) { return kernel.stats.ticks; };
    spec.description.description = "native atomic overshoot";
    spec.description.effort_currency = "ticks";
    spec.description.effort_allowance = WorkLimit::bounded(2);
    return spec;
  };
  enabled.execute(FeedbackKind::Support, "native", 0, build,
      [&] { k.stats.ticks += 7; return ApplyResult::Ok; });
  const auto &request = *capture.actions.back().native_request;
  assert(request.work == 7 && request.overshoot == 5 && !request.accounting_overflow);
  const auto &support = capture.actions.back().outcome;
  assert(!support.invocation.applications && !support.invocation.work);
  assert(!support.invocation.made_rule_progress && !support.composition_overshoot);
  assert(capture.actions.back().stop == StopReason::Completed);
  assert(capture.actions.back().after.cumulative.ticks -
         capture.actions.back().before.cumulative.ticks == 7);
  enabled.execute(FeedbackKind::Support, "overflow", 0, build,
      [&] { k.stats.ticks = 0; return ApplyResult::NoOp; });
  assert(capture.actions.back().native_request->accounting_overflow);
  assert(capture.actions.back().native_request->work == 0);
  enabled.execute(FeedbackKind::Support, "without-meter", 0,
      [] { return execution::NativeFeedbackSpec{}; },
      [&] { k.stats.ticks += 3; return ApplyResult::Ok; });
  const auto &unmetered = capture.actions.back();
  assert(unmetered.native_request->effort_currency == "ticks" &&
         unmetered.native_request->work == 3);
  enabled.execute(FeedbackKind::SearchService, "dynamic-native", 0,
      [] {
        execution::NativeFeedbackSpec spec;
        spec.description.version = 2;
        spec.description.description = "result-populated-native-description";
        spec.description.effort_currency = "applications";
        spec.description.have_affine_matrix_event = true;
        return spec;
      }, [] { return uint8_t{7}; },
      [](uint8_t result, NativeActionDescription &description) {
        description.affine_matrix_status = result;
        description.affine_matrix_event_identity = "event-after-apply";
        FeedbackOutcome outcome;
        outcome.invocation.status = InvocationStatus::Completed;
        outcome.invocation.selection_complete = true;
        return outcome;
      });
  const auto &dynamic = *capture.actions.back().native_request;
  assert(dynamic.version == 2 && dynamic.have_affine_matrix_event &&
         dynamic.affine_matrix_status == 7 &&
         dynamic.affine_matrix_event_identity == "event-after-apply");
  assert(!unmetered.outcome.invocation.work && !unmetered.outcome.invocation.applications &&
         !unmetered.outcome.invocation.made_rule_progress);
  enabled.execute(FeedbackKind::Support, "native-composition", 0,
      [] { return execution::NativeFeedbackSpec{}; }, [] {
        InvocationResult result;
        result.work_unit = WorkUnit::Propagations;
        result.work = 5;
        result.applications = 1;
        result.made_rule_progress = true;
        result.status = InvocationStatus::BudgetExhausted;
        return result;
      });
  const auto &composed = capture.actions.back();
  assert(composed.outcome.invocation.work == 5 && !composed.outcome.invocation.applications &&
         !composed.outcome.invocation.made_rule_progress);
  assert(composed.native_request->effort_currency == "propagations" &&
         composed.native_request->work == 5 && composed.stop == StopReason::Budget);
  enabled.execute(FeedbackKind::Rule, "rule-control", 0,
      [] { return execution::NativeFeedbackSpec{}; }, [] { return ApplyResult::Ok; });
  const auto &rule = capture.actions.back();
  assert(rule.outcome.invocation.work == 1 && rule.outcome.invocation.applications == 1 &&
         rule.outcome.invocation.made_rule_progress);
  assert(rule.native_request->effort_currency == "applications" && rule.native_request->work == 1);
  for (const auto result : {ApplyResult::NoOp, ApplyResult::Conflict, ApplyResult::Sat, ApplyResult::Unsat}) {
    enabled.execute(FeedbackKind::Support, "status-control", 0,
        [] { return execution::NativeFeedbackSpec{}; }, [=] { return result; });
    const auto &record = capture.actions.back();
    assert(record.outcome.invocation.rule_result == result);
    assert(record.stop == stop_reason(feedback_outcome(result).invocation.status));
    assert(!record.outcome.invocation.work && !record.outcome.invocation.applications &&
           !record.outcome.invocation.made_rule_progress);
    assert(record.native_request->work == 0 && record.native_request->effort_currency == "ticks");
  }
  enabled.resume(IntervalStart::ResumeSearch, "initial");
  enabled.end_interval(StopReason::Restart);
  const auto count = capture.actions.size();
  enabled.continue_search(IntervalStart::NativeContinuation);
  assert(capture.actions.size() == count); // Continuation does not select/install.
  enabled.finish(0, StopReason::Interrupted);
  assert(capture.begun == 1 && capture.ended == 1 && capture.intervals.size() == 2);
  assert(capture.intervals.back().preceding_interval == 1);
}

static void test_graph_labeling_constructor_feedback() {
  Kernel k;
  ScopedHeuristics h(k);
  constexpr uint32_t n = 4;
  kernel_reserve_vars(k, n * n);
  for (uint32_t column = 0; column < n; ++column) {
    std::vector<Lit> cover;
    for (uint32_t row = 0; row < n; ++row)
      cover.push_back(make_lit(row * n + column, false));
    kernel_add_large_clause(k, cover.data(), cover.size(), false, 0);
  }
  for (uint32_t row = 0; row < n; ++row)
    for (uint32_t a = 0; a < n; ++a)
      for (uint32_t b = a + 1; b < n; ++b)
        kernel_add_binary_clause(k, make_lit(row * n + a, true),
                                make_lit(row * n + b, true));
  // Separation two makes this projected objective deliberately too strong,
  // so the constructor publishes its best partial candidate instead of a
  // terminal model.
  for (uint32_t a = 0; a < n; ++a)
    for (uint32_t b = a + 1; b < n; ++b)
      kernel_add_binary_clause(k, make_lit(a * n + 2, true),
                              make_lit(b * n + 2, true));
  k.graph_labeling_parameters.work.fill(100000);
  k.graph_labeling_parameters.workspace_bytes = 1 << 20;
  k.graph_labeling_parameters.repair_rounds = 3;
  k.graph_labeling_parameters.anneal_iterations = 20;
  k.graph_labeling_parameters.stall_anneal_iterations = 3;
  const auto configured = k.graph_labeling_parameters;

  Capture capture;
  execution::PolicyCompletion completion(k, &capture,
      [](const Kernel &kernel, const void *) { return observe_kernel(kernel); },
      nullptr);
  completion.begin();
  const auto result = completion.execute(
      FeedbackKind::Rule, "Lucky", LUCKY_GRAPH_LABELING,
      [&] {
        return execution::native_rule_feedback(
            k, "Lucky", LUCKY_GRAPH_LABELING, "graph-labeling");
      },
      [&] {
        return execution::execute_action(
            k, execution::LuckyRequest<
                   execution::LuckyOpportunity::GraphLabeling>{});
      });
  completion.finish(0, StopReason::Completed);

  assert(result == ApplyResult::Ok && capture.actions.size() == 1);
  const auto &record = capture.actions.front();
  assert(record.identity == "Lucky" && record.instance == LUCKY_GRAPH_LABELING);
  assert(record.native_request && record.native_request->invocation);
  const auto &invocation = *record.native_request->invocation;
  assert(invocation.rule == "Lucky" &&
         invocation.granularity == InvocationGranularity::OneInstance);
  assert(invocation.instances == std::vector<uint64_t>{LUCKY_GRAPH_LABELING});
  assert(std::holds_alternative<GraphLabelingParameters>(invocation.parameters));
  const auto &parameters = std::get<GraphLabelingParameters>(invocation.parameters);
  assert(parameters.work == configured.work &&
         parameters.workspace_bytes == configured.workspace_bytes &&
         parameters.repair_rounds == configured.repair_rounds);

  assert(record.after.cumulative.graph_labeling_calls ==
         record.before.cumulative.graph_labeling_calls + 1);
  const uint64_t work = record.after.cumulative.graph_labeling_work -
                        record.before.cumulative.graph_labeling_work;
  assert(work > 0 && record.native_request->work == work);
  assert(record.after.cumulative.local_search_hints_offered ==
         record.before.cumulative.local_search_hints_offered + 1);
  assert(record.before.local_search_candidates.empty() &&
         record.after.local_search_candidates.size() == 1);
  const auto &candidate = record.after.local_search_candidates.front();
  assert(local_search_candidate_kind(candidate.id) ==
         LocalSearchKind::GraphLabeling);
  assert(candidate.objective > 0 && candidate.objective_exact);
  assert(candidate.effort.unit == LocalSearchEffortUnit::GraphWork &&
         candidate.effort.amount == work);
  assert(candidate.formula_touch_epoch == record.after.state.touch_epoch &&
         candidate.formula_root_assignments == record.after.root_population &&
         candidate.external_variables == record.after.variables);
  assert(candidate.known_phases == record.after.variables);
}

// Leaf measurements precede the enclosing native publication, but receiver
// code must only run after temporary parameters and watch scopes are restored.
static void test_deferred_publication_and_native_units() {
  Kernel k;
  Capture capture;
  execution::PolicyCompletion *active = nullptr;
  auto observe = [](const Kernel &kernel, const void *owner) {
    auto result = observe_kernel(kernel);
    const auto *completion = *static_cast<execution::PolicyCompletion *const *>(owner);
    if (completion) {
      result.sequence = completion->sequence();
      result.last_action = completion->last_action();
    }
    return result;
  };
  execution::PolicyCompletion completion(k, &capture, observe, &active);
  active = &completion;
  completion.begin();
  capture.check = [&] {
    assert(k.vivify_scope == 0);
    const auto &record = capture.actions.back();
    assert(record.after.last_action && record.after.last_action->sequence == record.sequence);
  };
  {
    execution::CompletionPublication outer(&completion);
    k.vivify_scope = 3;
    completion.execute(FeedbackKind::Rule, "BVE", 0,
        [] { return execution::NativeFeedbackSpec{}; }, [] {
          InvocationResult result;
          result.rule_id = 1;
          result.instance = 42;
          result.work_unit = WorkUnit::BveTicks;
          result.work = 9;
          result.applications = 1;
          result.status = InvocationStatus::BudgetExhausted;
          return result;
        });
    {
      execution::CompletionPublication nested(&completion);
      completion.execute(FeedbackKind::Rule, "Propagate", 0,
          [] { return execution::NativeFeedbackSpec{}; }, [] {
            InvocationResult result;
            result.work_unit = WorkUnit::Propagations;
            result.work = 5;
            result.status = InvocationStatus::Completed;
            return result;
          });
    }
    assert(capture.actions.empty());
    k.vivify_scope = 0;
  }
  assert(capture.actions.size() == 2);
  assert(capture.actions[0].instance == 42);
  assert(capture.actions[0].native_request->effort_currency == "bve-ticks");
  assert(capture.actions[0].native_request->work == 9);
  assert(capture.actions[0].stop == StopReason::Budget);
  assert(capture.actions[1].native_request->effort_currency == "propagations");
  assert(capture.actions[1].native_request->work == 5);
  assert(capture.actions[1].before.last_action->sequence == 1);
  completion.finish(0, StopReason::Completed);
}

static Kernel inprocess_fixture(bool vivify) {
  Kernel k;
  k.stats.conflicts = 1;
  k.stats.search_ticks = 1000000000;
  if (vivify) {
    kernel_reserve_vars(k, 160);
    for (uint32_t clause = 0; clause < 8; ++clause) {
      Lit lits[20];
      for (uint32_t i = 0; i < 20; ++i) lits[i] = make_lit(clause * 20 + i, true);
      kernel_add_large_clause(k, lits, 20, true, 2, 0);
    }
    k.tier1_by_mode[0] = k.tier2_by_mode[0] = 2;
    k.tier1_by_mode[1] = k.tier2_by_mode[1] = 2;
    k.tier1 = k.tier2 = 2;
  } else {
    kernel_reserve_vars(k, 5);
    kernel_add_binary_clause(k, internalize(-1), internalize(2));
    kernel_add_binary_clause(k, internalize(-2), internalize(1));
    const Lit first[] = {internalize(1), internalize(-3), internalize(2)};
    const Lit second[] = {internalize(-1), internalize(3), internalize(-2)};
    kernel_add_large_clause(k, first, 3, false, 0);
    kernel_add_large_clause(k, second, 3, false, 0);
  }
  return k;
}

static void test_native_batches(bool vivify, bool budgeted) {
  DefaultStrategyConfig config;
  config.inprocess.minimum_effort_ticks = 1000;
  config.inprocess.phase_rounds = 1;
  config.inprocess.vivify_permille = 1000;
  config.inprocess.substitute_rounds = 2;
  config.inprocess.substitute_search_rounds = 2;
  const char *name = vivify ? "Vivify" : "Substitute";
  std::string preserved;
  for (bool feedback : {false, true}) {
    Kernel k = inprocess_fixture(vivify);
    ScopedHeuristics h(k);
    DefaultPolicy policy(config);
    policy.inprocess().build(k);
    bool selected = false;
    for (auto &entry : policy.inprocess().entries) {
      entry.enabled = !selected && !std::strcmp(tactix_rules()[entry.rule].name, name);
      if (entry.enabled) {
        selected = true;
        entry.budget_aware = budgeted;
      }
    }
    assert(selected);
    Capture capture;
    execution::PolicyCompletion completion(k, feedback ? &capture : nullptr,
        [](const Kernel &kernel, const void *owner) {
          return static_cast<const DefaultPolicy *>(owner)->observe(kernel);
        }, &policy);
    capture.check = [&] {
      assert(k.vivify_scope == 0 && !k.work_budget && !k.work_budget_offset);
      assert(k.large_watches_connected && !k.pending.valid && !k.warming);
      assert(!k.factor_selection && !k.dense && !k.inprocessing);
    };
    completion.begin();
    assert(policy.inprocess().run_phase(k, SlotGroup::Probe, nullptr, &completion) == 0);
    completion.finish(0, StopReason::Completed);
    kernel_check_invariants(k);
    const auto snapshot = serialize_observation(policy.observe(k));
    if (!feedback) preserved = snapshot;
    else assert(snapshot == preserved);
    if (feedback) {
      std::vector<std::string> installed;
      unsigned entered = 0, exited = 0;
      for (const auto &record : capture.actions) {
        if (record.kind == FeedbackKind::Support) {
          assert(!record.outcome.invocation.applications && !record.outcome.invocation.made_rule_progress);
          if (record.outcome.invocation.work_unit == WorkUnit::Applications)
            assert(!record.outcome.invocation.work && !record.outcome.composition_overshoot);
        }
        if (record.identity == "InstallRuleParameters") {
          assert(record.native_request->invocation);
          installed.push_back(record.native_request->invocation->rule);
          assert(!record.outcome.invocation.applications && !record.outcome.invocation.made_rule_progress);
        } else if (record.identity == "InprocessPhaseEntry" || record.identity == "InprocessPhaseExit") {
          assert(!record.outcome.invocation.applications && !record.outcome.invocation.made_rule_progress);
          entered += record.identity == "InprocessPhaseEntry";
          exited += record.identity == "InprocessPhaseExit";
        }
      }
      std::vector<std::string> expected{"Congruence", "Instantiate", "Backbone", "BCE",
          "HyperTernaryRes", "Factor", "TransitiveReduce", "Substitute", "Vivify", "BVE"};
      // A Vivify dose first poses its tier boundaries as a Reduce profile.
      if (vivify) expected.push_back("Reduce");
      assert(installed == expected);
      assert(entered == 1 && exited == 1);
    }
    if (vivify) {
      assert(k.stats.vivify_rounds == 3 && k.stats.vivify_checked == 8);
      if (feedback) {
        std::vector<uint32_t> scopes;
        uint64_t work = 0;
        for (const auto &record : capture.actions) {
          if (record.identity != name) continue;
          const auto &request = *record.native_request;
          scopes.push_back(std::get<VivifyParameters>(request.invocation->parameters).scope);
          assert(!request.effort_allowance.is_unlimited());
          work += request.work;
        }
        assert((scopes == std::vector<uint32_t>{3, 5, 2}));
        assert(work > 0);
      }
    } else {
      assert(k.stats.substitute_rounds == 2);
      assert(kernel_var_retired(k, 1) && kernel_var_retired(k, 2));
      if (feedback) {
        unsigned calls = 0;
        for (const auto &record : capture.actions) {
          if (record.identity != name) continue;
          ++calls;
          assert(record.native_request->work > 0);
          assert(record.native_request->effort_currency == "substitute-cache-lines");
          assert(record.native_request->effort_allowance.is_unlimited() == !budgeted);
          assert(record.native_request->description.find("max-rounds=2") != std::string::npos);
        }
        assert(calls == 1); // One authored call, two native rounds, one watch session.
      }
    }
  }
}

static void test_partial_native_outcomes() {
  Kernel k;
  k.factor_completed = false;
  auto outcome = execution::native_rule_outcome(k, "Factor", ApplyResult::Ok);
  assert(outcome.invocation.status == InvocationStatus::BudgetExhausted);
  assert(outcome.invocation.budget_status == BudgetStatus::Exhausted);
  assert(!outcome.invocation.selection_complete && outcome.invocation.made_rule_progress);
  volatile sig_atomic_t stop = 1;
  k.stop = &stop;
  assert(execution::native_rule_outcome(k, "Factor", ApplyResult::Ok).invocation.status ==
         InvocationStatus::Interrupted);
  assert(execution::native_rule_outcome(k, "Factor", ApplyResult::Unsat).invocation.status ==
         InvocationStatus::Unsat);
  k.stop = nullptr;
  k.stop_fuzz = 1;
  const auto state = k.stop_fuzz_state;
  const auto fires = k.stats.stop_fuzz_fires;
  for (unsigned read = 0; read < 3; ++read)
    assert(execution::native_rule_outcome(k, "Factor", ApplyResult::Ok).invocation.status ==
           InvocationStatus::BudgetExhausted);
  assert(k.stop_fuzz_state == state && k.stats.stop_fuzz_fires == fires);
}

static void test_native_interruption_feedback_purity() {
  // Poll inside the actual native body. Describing its result must not add a
  // draw, even when stop-fuzz fires every time or when an action is terminal.
  Kernel original, observed;
  original.stop_fuzz = observed.stop_fuzz = 1;
  Capture capture;
  execution::PolicyCompletion completion(observed, &capture,
      [](const Kernel &kernel, const void *) { return observe_kernel(kernel); }, nullptr);
  completion.begin();
  auto operation = [](Kernel &kernel, ApplyResult result) {
    assert(kernel_terminating(kernel));
    return result;
  };
  for (const auto result : {ApplyResult::NoOp, ApplyResult::Ok, ApplyResult::Unsat}) {
    assert(operation(original, result) == result);
    assert(completion.execute(FeedbackKind::Rule, "Vivify", INSTANCE_CANONICAL,
        [] { return execution::NativeFeedbackSpec{}; },
        [&] { return operation(observed, result); },
        [&](ApplyResult value) { return execution::native_rule_outcome(observed, "Vivify", value); }) == result);
    assert(original.stop_fuzz_state == observed.stop_fuzz_state);
    assert(serialize_observation(observe_kernel(original)) ==
           serialize_observation(observe_kernel(observed)));
    assert(capture.actions.back().stop ==
           (result == ApplyResult::Unsat ? StopReason::Unsat : StopReason::Interrupted));
  }
  completion.finish(20, StopReason::Unsat);
}

static void test_dosed_bve_publication() {
  DefaultStrategyConfig config;
  config.inprocess.phase_rounds = 1;
  config.inprocess.minimum_effort_ticks = 1000;
  std::string preserved;
  for (bool feedback : {false, true}) {
    Kernel k;
    kernel_reserve_vars(k, 3);
    kernel_add_binary_clause(k, internalize(1), internalize(2));
    kernel_add_binary_clause(k, internalize(-1), internalize(3));
    k.stats.conflicts = 1;
    ScopedHeuristics h(k);
    DefaultPolicy policy(config);
    policy.inprocess().build(k);
    bool selected = false;
    for (auto &entry : policy.inprocess().entries) {
      entry.enabled = !selected && !std::strcmp(tactix_rules()[entry.rule].name, "BVE");
      selected |= entry.enabled;
    }
    assert(selected);
    Capture capture;
    execution::PolicyCompletion completion(k, feedback ? &capture : nullptr,
        [](const Kernel &kernel, const void *owner) {
          return static_cast<const DefaultPolicy *>(owner)->observe(kernel);
        }, &policy);
    capture.check = [&] {
      assert(!k.work_budget && !k.work_budget_offset);
      for (Var v = 0; v < k.nvars; ++v)
        if (kernel_var_retired(k, v)) assert(!heuristics_var_active(k, v));
    };
    completion.begin();
    assert(policy.inprocess().run_phase(k, SlotGroup::Eliminate, nullptr, &completion) == 0);
    completion.finish(0, StopReason::Completed);
    assert(k.stats.bve_eliminated > 0);
    assert(k.bve_definition_session == 1);
    kernel_check_invariants(k);
    const auto snapshot = serialize_observation(policy.observe(k));
    if (!feedback) preserved = snapshot;
    else {
      assert(snapshot == preserved);
      bool changed = false;
      unsigned calls = 0;
      unsigned sessions = 0;
      for (const auto &record : capture.actions) {
        if (record.identity == "BeginBveDefinitionSession") {
          ++sessions;
          assert(record.native_request->description == "session=0;next=1");
          assert(record.sequence == 1);
        }
        if (record.identity != "BVE") continue;
        ++calls;
        changed |= record.after.cumulative.bve_eliminated > record.before.cumulative.bve_eliminated;
        assert(record.native_request->invocation->granularity == InvocationGranularity::OneInstance);
        assert(record.native_request->effort_currency == "resolutions");
        assert(!record.native_request->effort_allowance.is_unlimited());
      }
      assert(calls && changed && sessions == 1);
    }
  }
}

static void test_structured_root_feedback() {
  DefaultStrategyConfig config;
  OnlineStrategyStage affine;
  affine.on_budget_exhaustion = BudgetExhaustionPolicy::Continue;
  OnlineInvocation affine_step;
  affine_step.id = 109;
  affine_step.rule = "Lucky";
  affine_step.parameters = AffineModelParameters{};
  affine_step.granularity = InvocationGranularity::OneInstance;
  affine_step.instances = {8};
  affine_step.budget = WorkLimit::bounded(1);
  affine.program.sequence = {affine_step};
  OnlineStrategyStage stage;
  stage.on_budget_exhaustion = BudgetExhaustionPolicy::Continue;
  OnlineInvocation producer;
  producer.id = 1;
  producer.rule = "SubstitutionRedundancy";
  producer.parameters = SubstitutionRedundancyParameters{};
  producer.granularity = InvocationGranularity::OneInstance;
  producer.substitution_redundancy_instances = {{{internalize(1)}, {}, {}}};
  producer.post_application = PostApplication::Propagate;
  OnlineInvocation cleanup;
  cleanup.id = 2;
  cleanup.rule = "Reduce";
  ReduceParameters parameters;
  parameters.victim_numerator = 0;
  parameters.compact_collection = true;
  parameters.reduction_bookkeeping = false;
  cleanup.parameters = parameters;
  cleanup.granularity = InvocationGranularity::CanonicalApplication;
  stage.program.sequence = {producer, cleanup};
  cleanup.id = 3;
  stage.program.sequence.push_back(cleanup);
  stage.progress_followups = {2, 3};
  config.inprocess.structured_root_stages = {affine, stage};
  std::string preserved;
  for (bool feedback : {false, true}) {
    Kernel k;
    kernel_reserve_vars(k, 3);
    const Lit clause[] = {internalize(-1), internalize(2), internalize(3)};
    kernel_add_large_clause(k, clause, 3, false, 0, ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(1), internalize(2), ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(1), internalize(-2), ++k.next_clause_id);
    ScopedHeuristics h(k);
    DefaultPolicy policy(config);
    Capture capture;
    execution::PolicyCompletion completion(k, feedback ? &capture : nullptr,
        [](const Kernel &kernel, const void *owner) {
          return static_cast<const DefaultPolicy *>(owner)->observe(kernel);
        }, &policy);
    capture.check = [&] { assert(!k.pending.valid && !k.probing.recording); };
    completion.begin();
    assert(policy.inprocess().run_structured_root(k, nullptr, &completion) == 0);
    completion.finish(0, StopReason::Completed);
    assert(k.stats.affine_model_calls == 1);
    assert(k.stats.sr_applied == 1 && k.arena.empty());
    assert(policy.inprocess().structured_root.stages.size() == 2);
    assert(policy.inprocess().structured_root.stages[0].runs == 1);
    assert(policy.inprocess().structured_root.stages[0].terminal ==
           InvocationStatus::Completed);
    kernel_check_invariants(k);
    const auto snapshot = serialize_observation(policy.observe(k));
    if (!feedback) preserved = snapshot;
    else {
      assert(snapshot == preserved);
      unsigned applications = 0;
      unsigned affine_records = 0;
      bool affine_called = false, witness = false, cleanup_called = false;
      for (const auto &record : capture.actions) {
        const auto &request = *record.native_request;
        assert(request.invocation && request.invocation->id != 3);
        assert(request.invocation->rule == record.identity);
        assert(request.work == record.outcome.invocation.work);
        applications += record.outcome.invocation.applications;
        if (request.invocation->id == 109) {
          ++affine_records;
          assert(record.identity == "Lucky");
          assert(request.invocation->instances == std::vector<uint64_t>{8});
          if (record.outcome.invocation.applications) {
            assert(!affine_called && !witness && !cleanup_called);
            affine_called = true;
            assert(record.outcome.invocation.status == InvocationStatus::NoChange);
            assert(record.before.state.arena_generation ==
                   record.after.state.arena_generation);
            assert(record.before.state.touch_epoch == record.after.state.touch_epoch);
            assert(record.after.state.strategy_epoch ==
                   record.before.state.strategy_epoch + 1);
          } else {
            assert(affine_called && !witness && !cleanup_called);
            assert(record.outcome.invocation.status == InvocationStatus::Completed);
          }
        } else if (request.invocation->id == 1) {
          assert(affine_called);
          witness = true;
          assert(request.invocation->substitution_redundancy_instances.front().clause ==
                 producer.substitution_redundancy_instances.front().clause);
        } else cleanup_called = true;
      }
      assert(affine_records == 2 && affine_called && witness && cleanup_called);
      assert(applications == policy.inprocess().structured_root.applications);
    }
  }
}

static void test_default_observation_and_replay(const char *path) {
  const auto config = search_profile();
  Kernel native = pigeonhole(), observed = pigeonhole();
  ScopedHeuristics first_h(native), second_h(observed);
  DefaultPolicy first(config), second(config);
  Capture capture;
  second.set_feedback(&capture);
  capture.check = [&] {
    assert(!observed.pending.valid && !observed.warming);
    const auto before = serialize_observation(second.observe(observed));
    assert(before == serialize_observation(second.observe(observed)));
  };
  assert(first.solve(native) == 20 && second.solve(observed) == 20);
  assert(serialize_observation(first.observe(native)) == serialize_observation(second.observe(observed)));
  assert(native.trail.size() == observed.trail.size());
  assert(std::equal(native.trail.begin(), native.trail.end(), observed.trail.begin()));
  assert(native.h->saved == observed.h->saved && native.h->target == observed.h->target);
  assert(native.h->scores.score == observed.h->scores.score);
  assert(capture.begun == 1 && capture.ended == 1);
  bool analysis = false, warmup = false, mode = false, work = false, root = false;
  for (const auto &record : capture.actions) {
    assert(record.after.last_action && record.after.last_action->sequence == record.sequence);
    analysis |= record.identity == "ConflictAnalysis";
    warmup |= record.identity == "Warmup";
    mode |= record.identity == "SwitchMode";
    if (record.identity == "Propagate")
      work |= record.native_request->work > 0 && record.native_request->effort_currency == "propagations";
  }
  for (const auto &interval : capture.intervals) {
    if (interval.start != IntervalStart::ResumeSearch)
      assert(interval.preceding_interval + 1 == interval.sequence);
    root |= interval.stop == StopReason::RootPreparation;
  }
  assert(analysis && warmup && mode && work && root);
  for (const bool replay : {false, true}) {
    Kernel k = pigeonhole();
    DefaultPolicy policy(config);
    FeedbackTrace trace;
    assert(replay ? trace.replay(path, "native-default-test") : trace.write(path, "native-default-test"));
    policy.set_feedback(&trace);
    assert(policy.solve(k) == 20);
    assert(trace.terminal() && trace.error().empty());
    assert(!k.h); // Owned guidance is detached only after completion delivery.
  }
}

static void test_early_terminal() {
  Kernel k;
  k.inconsistent = true;
  DefaultPolicy policy;
  Capture capture;
  policy.set_feedback(&capture);
  assert(policy.solve(k) == 20);
  assert(capture.begun == 1 && capture.ended == 1 && capture.actions.empty());
}

static void test_interrupted_entry_and_fresh_session() {
  Kernel k = pigeonhole();
  ScopedHeuristics h(k);
  assert(execution::execute_action(k, execution::DecisionRequest{make_lit(0, false)}) == ApplyResult::Ok);
  DefaultPolicy policy(search_profile());
  Capture interrupted;
  policy.set_feedback(&interrupted);
  volatile sig_atomic_t stop = 1;
  assert(policy.solve(k, &stop) == 0);
  assert(interrupted.begun == 1 && interrupted.ended == 1);
  assert(interrupted.actions.front().identity == "Restart");
  assert(interrupted.actions.front().native_request->description.find("purpose=preparation") != std::string::npos);
  assert(!k.level && !k.pending.valid);
  Capture resumed;
  policy.set_feedback(&resumed);
  stop = 0;
  assert(policy.solve(k, &stop) == 20);
  assert(resumed.begun == 1 && resumed.ended == 1);
  assert(resumed.actions.front().sequence == 1);
}

static void test_startup_support_feedback() {
  std::string preserved;
  for (bool feedback : {false, true}) {
    Kernel k = pigeonhole(true);
    DefaultPolicy policy(search_profile());
    Capture capture;
    if (feedback) policy.set_feedback(&capture);
    assert(policy.solve(k) == 20);
    assert(!k.h && !k.saved_phases && !k.cold_state_deferred);
    const auto state = serialize_observation(observe_kernel(k));
    if (!feedback) preserved = state;
    else {
      assert(state == preserved);
      std::vector<std::string> startup;
      for (const auto &record : capture.actions)
        if (record.identity == "MaterializeColdState" || record.identity == "AttachHeuristics" || record.identity == "SparseCollect" ||
            record.identity == "CompactRandomOrder") {
          startup.push_back(record.identity);
          assert(!record.before.searching && !record.after.searching);
          assert(!record.outcome.invocation.applications);
        }
      assert((startup == std::vector<std::string>{"MaterializeColdState", "AttachHeuristics", "SparseCollect", "CompactRandomOrder"}));
      assert(capture.begun == 1 && capture.ended == 1);
    }
  }
  // A completed cold Lucky opportunity never reaches the support consumer.
  Kernel early;
  kernel_defer_cold_state(early);
  kernel_reserve_vars(early, 2);
  kernel_add_binary_clause(early, internalize(1), internalize(2));
  Capture capture;
  DefaultPolicy policy(search_profile());
  policy.set_feedback(&capture);
  assert(policy.solve(early) == 10 && early.cold_state_deferred);
  assert(!early.h && !early.saved_phases);
  for (const auto &record : capture.actions)
    assert(record.identity != "MaterializeColdState" && record.identity != "AttachHeuristics" && record.identity != "SparseCollect" &&
           record.identity != "CompactRandomOrder");
}

static void test_post_structured_bce_feedback() {
  std::string preserved;
  for (bool feedback : {false, true}) {
    Kernel k;
    kernel_reserve_vars(k, 7);
    const Lit source[] = {internalize(1), internalize(2), internalize(3)};
    kernel_add_large_clause(k, source, 3, false, 0, ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(4), internalize(6), ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(-4), internalize(7), ++k.next_clause_id);
    auto config = search_profile();
    OnlineInvocation complete;
    complete.id = 1;
    complete.rule = "SubstitutionRedundancy";
    SubstitutionRedundancyParameters ulc;
    ulc.producer = SubstitutionRedundancyProducer::Orbitopal;
    ulc.maximum_orbitopal_matrices = 0;
    ulc.complete_unique_literal_amos = true;
    complete.parameters = ulc;
    complete.granularity = InvocationGranularity::CompleteCurrentPass;
    OnlineInvocation added;
    added.id = 2;
    added.rule = complete.rule;
    added.parameters = SubstitutionRedundancyParameters{};
    added.granularity = InvocationGranularity::OneInstance;
    // Positive 5 has no opposite occurrence, so this is a valid blocked
    // addition with no graph-fixing/root consequence. It triggers cleanup.
    added.substitution_redundancy_instances = {{{internalize(5), internalize(7)}, {}, {}}};
    OnlineStrategyProgram program;
    program.sequence = {complete, added};
    config.inprocess.structured_root_stages = {{program, BudgetExhaustionPolicy::Continue}};
    config.inprocess.structured_root_before_lucky = true;
    Capture capture;
    capture.check = [&] {
      if (!capture.actions.empty() && capture.actions.back().identity == "BCE")
        // Startup still owns tick attribution; only BCE's nested speculative
        // phase suppression and occurrence attachment must have been restored.
        assert(k.inprocessing && !k.phase_saving_suppressed && !k.dense);
    };
    DefaultPolicy policy(config);
    if (feedback) policy.set_feedback(&capture);
    assert(policy.solve(k) == 10);
    assert(k.stats.sr_unique_literal_amo_binaries && k.stats.sr_post_amo_nonhelper_applied);
    assert(!k.stats.sr_post_amo_graph_root_units && k.stats.bce_deleted);
    const auto state = serialize_observation(observe_kernel(k));
    if (!feedback) preserved = state;
    else {
      assert(state == preserved);
      unsigned calls = 0;
      for (const auto &record : capture.actions)
        if (record.identity == "BCE") {
          ++calls;
          assert(record.native_request->description == "post-structured-amo");
          assert(record.native_request->effort_currency == "bce-ticks");
          assert(record.after.cumulative.bce_deleted > record.before.cumulative.bce_deleted);
        }
      assert(calls == 1);
    }
    kernel_extend_model(k, false);
    assert(k.model_values[internalize(1)] > 0 || k.model_values[internalize(2)] > 0 || k.model_values[internalize(3)] > 0);
    assert(k.model_values[internalize(4)] > 0 || k.model_values[internalize(6)] > 0);
    assert(k.model_values[internalize(-4)] > 0 || k.model_values[internalize(7)] > 0);
  }
}

int main() {
  char temporary[] = "/tmp/tactix-native-feedback-XXXXXX";
  const int fd = mkstemp(temporary);
  assert(fd >= 0 && close(fd) == 0);
  test_completion_accounting();
  test_graph_labeling_constructor_feedback();
  test_deferred_publication_and_native_units();
  test_native_batches(true, true);
  test_native_batches(false, true);
  test_native_batches(false, false);
  test_partial_native_outcomes();
  test_native_interruption_feedback_purity();
  test_dosed_bve_publication();
  test_structured_root_feedback();
  test_default_observation_and_replay(temporary);
  test_early_terminal();
  test_interrupted_entry_and_fresh_session();
  test_startup_support_feedback();
  test_post_structured_bce_feedback();
  assert(unlink(temporary) == 0);
}
