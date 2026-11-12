#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "rules/lucky/graph_labeling_model.hpp"
#include "rules/lucky/graph_labeling_window_cursor.hpp"
#include "rules/lucky/model.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "strategy/guidance_outcome.hpp"
#include "strategy/lucky_execution.hpp"
#include "strategy/online_inprocess.hpp"
#include "strategy/work_execution.hpp"

#include <algorithm>
#include <cassert>

using namespace tactix;
using namespace tactix::strategy;

// Column covers and row at-most-one over 16 variables, plus redundant
// same-column exclusions; `difficult` makes the projected objective too strong.
static void permutation(Kernel &k, bool difficult) {
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
  // Redundant same-column exclusions; the difficult objective must remain
  // guidance rather than UNSAT.
  for (uint32_t a = 0; a < n; ++a)
    for (uint32_t b = a + 1; b < n; ++b)
      kernel_add_binary_clause(k, make_lit(a * n + (difficult ? 2 : 0), true),
                               make_lit(b * n + (difficult ? 2 : 0), true));
  k.graph_labeling_parameters.work.fill(100000);
  k.graph_labeling_parameters.workspace_bytes = 1 << 20;
  k.graph_labeling_parameters.repair_rounds = 3;
  k.graph_labeling_parameters.anneal_iterations = 20;
  k.graph_labeling_parameters.stall_anneal_iterations = 3;
}

namespace tactix {
struct DefaultGraphLabelingTestAccess {
  static GraphLabelingSliceResult service(DefaultPolicy &policy, Kernel &kernel) {
    return policy.service_graph_labeling(kernel);
  }
};
} // namespace tactix

static GraphLabelingSliceResult sliced(
    Kernel &kernel, GraphLabelingModelSession &session, uint64_t allowance) {
  ScopedRuleWorkBudget budget(kernel, {RuleWorkBudgetMode::Bounded, allowance});
  return graph_labeling_model_slice(kernel, session);
}

static execution::GraphLabelingActionResult act(Kernel &k, bool auth) {
  return execution::execute_action(k, execution::GraphLabelingActionRequest{
      auth ? execution::GraphLabelingOperation::Authenticate
           : execution::GraphLabelingOperation::Continue, k.graph_labeling_parameters});
}

static void assert_model(const Kernel &k) {
  std::vector<uint8_t> model(k.nvars);
  for (Var v = 0; v < k.nvars; ++v) model[v] = k.value(make_lit(v, false)) > 0;
  assert(lucky_model_valid(k, model));
}

// Stops the solve at its first search decision, observed as the Decide
// feedback record, and checks the graph-root state that decision follows.
struct StopAfterGraphRoot final : controller::FeedbackReceiver {
  DefaultPolicy &policy;
  const Kernel &kernel;
  volatile sig_atomic_t stop = 0;
  unsigned picks = 0;
  StopAfterGraphRoot(DefaultPolicy &p, const Kernel &k) : policy(p), kernel(k) {}
  void search_interval(const controller::SearchIntervalFeedback &) override {}
  void after_action(const controller::ActionFeedback &record) override {
    if (record.kind != controller::FeedbackKind::SearchService ||
        record.identity != "Decide")
      return;
    assert(!picks++);
    const auto &diagnostics = policy.graph_labeling_diagnostics();
    assert(diagnostics.authentication_attempts == 1);
    assert(diagnostics.authentication_successes == 1);
    // The root run continues until it stops being pending, before CDCL.
    assert(diagnostics.runs == 1 && diagnostics.fruitless_runs == 1);
    assert(diagnostics.slices >= 1 &&
           diagnostics.pending_slices == diagnostics.slices - 1);
    assert(diagnostics.useful_hints >= 1);
    assert(kernel.stats.graph_labeling_recognized == 1);
    assert(kernel.stats.graph_labeling_calls == diagnostics.slices + 1);
    assert(kernel.graph_labeling_session.authenticated());
    assert(kernel.graph_labeling_session.candidate_fingerprint());
    stop = 1;
  }
};

static void default_policy_runs_the_graph_root_before_search() {
  Kernel kernel;
  ScopedHeuristics heuristics(kernel);
  permutation(kernel, true);
  DefaultStrategyConfig config;
  for (bool *enabled : {&config.affine_root_closure.enabled,
           &config.affine_model.enabled, &config.parity_model.enabled,
           &config.onehot_model.enabled, &config.ramsey_startup.enabled,
           &config.signed_cardinality_proof.enabled,
           &config.balanced_signature_model.enabled,
           &config.exact_cover_model.enabled,
           &config.independent_set_model.enabled, &config.fermat_model.enabled,
           &config.cerny_model.enabled, &config.lookup_circuit_model.enabled,
           &config.simon_model.enabled, &config.stconn_proof.enabled,
           &config.inprocess.append_unlisted_rules,
           &config.inprocess.initial_fast_elimination,
           &config.rephase.late_lucky, &config.rephase.walk_warmup})
    *enabled = false;
  config.inprocess.program.clear();
  config.inprocess.structured_root_stages.clear();
  config.inprocess.structured_root_bounded_backtracking_stages.clear();
  config.inprocess.structured_root_large_domain_stages.clear();
  config.graph_labeling_model.enabled = true;
  config.graph_labeling_model.parameters = kernel.graph_labeling_parameters;
  DefaultPolicy policy(config);
  StopAfterGraphRoot stopper(policy, kernel);
  policy.set_feedback(&stopper);
  assert(policy.solve(kernel, &stopper.stop) == 0);
  assert(stopper.picks == 1);
}

// Authentication one work unit short of its measured cost declines cleanly.
static void authentication_is_bounded() {
  Kernel measured;
  ScopedHeuristics measured_h(measured);
  permutation(measured, false);
  GraphLabelingModelSession measured_session;
  const auto successful =
      graph_labeling_model_authenticate(measured, measured_session);
  assert(successful.status == GraphLabelingAuthenticationStatus::Authenticated);
  assert(successful.work && measured_session.authenticated());

  Kernel bounded;
  ScopedHeuristics bounded_h(bounded);
  permutation(bounded, false);
  bounded.work_budget_bounded = true;
  bounded.work_budget = successful.work - 1;
  GraphLabelingModelSession bounded_session;
  const auto exhausted =
      graph_labeling_model_authenticate(bounded, bounded_session);
  assert(exhausted.status == GraphLabelingAuthenticationStatus::Exhausted);
  assert(!bounded_session.authenticated() && bounded.trail.empty());
}

// Bounded continuations preserve the Window cursor exactly: many small slices
// reach the same frontier, random stream, work and candidate as one large one.
// The model defers Window setup it cannot afford: one unit short charges
// nothing and keeps the candidate; an exact allowance charges exactly setup.
// The workspace boundary covers graph plus cursor bytes to the byte.
static void window_cursor_survives_bounded_slices() {
  Kernel boundary;
  ScopedHeuristics boundary_h(boundary);
  permutation(boundary, true);
  boundary.graph_labeling_parameters.work[size_t(
      GraphLabelingStage::Construction)] = 64;
  graph_labeling::Shape footprint;
  footprint.vertices = 4;
  const uint64_t graph_bytes = 16 * 4 * 4 + 64 * 4;
  const uint64_t cursor_bytes =
      graph_labeling::WindowCursor::required_workspace(footprint) +
      sizeof(graph_labeling::Candidate) + 4 * sizeof(uint32_t);
  boundary.graph_labeling_parameters.workspace_bytes =
      graph_bytes + std::max<uint64_t>(16 * 4 * 4 + 256 * 4, cursor_bytes);
  Kernel boundary_small;
  ScopedHeuristics boundary_small_h(boundary_small);
  permutation(boundary_small, true);
  boundary_small.graph_labeling_parameters = boundary.graph_labeling_parameters;
  --boundary_small.graph_labeling_parameters.workspace_bytes;
  GraphLabelingModelSession boundary_small_session;
  assert(graph_labeling_model_authenticate(boundary_small,
             boundary_small_session).status ==
         GraphLabelingAuthenticationStatus::Exhausted);
  assert(!boundary_small_session.authenticated());
  GraphLabelingModelSession boundary_session;
  assert(graph_labeling_model_authenticate(boundary, boundary_session).status ==
         GraphLabelingAuthenticationStatus::Authenticated);
  assert(graph_labeling_model_slice(boundary, boundary_session).status ==
         GraphLabelingSliceStatus::Pending);
  const uint64_t setup =
      graph_labeling::WindowCursor::initialization_work(footprint);
  const uint64_t boundary_fingerprint = boundary_session.candidate_fingerprint();
  const auto short_setup = sliced(boundary, boundary_session, setup - 1);
  assert(short_setup.status == GraphLabelingSliceStatus::Pending);
  assert(!short_setup.work);
  assert(boundary_session.candidate_fingerprint() == boundary_fingerprint);
  const auto exact_setup = sliced(boundary, boundary_session, setup);
  assert(exact_setup.status == GraphLabelingSliceStatus::Pending);
  assert(exact_setup.work == setup);
  assert(boundary_session.candidate_fingerprint() == boundary_fingerprint);

  Kernel whole, split;
  ScopedHeuristics whole_h(whole), split_h(split);
  GraphLabelingModelSession whole_session, split_session;
  for (auto [kernel, session] : {std::pair{&whole, &whole_session},
                                 std::pair{&split, &split_session}}) {
    permutation(*kernel, true);
    kernel->graph_labeling_parameters.work[size_t(
        GraphLabelingStage::Construction)] = 64;
    assert(graph_labeling_model_authenticate(*kernel, *session).status ==
           GraphLabelingAuthenticationStatus::Authenticated);
    assert(graph_labeling_model_slice(*kernel, *session).status ==
           GraphLabelingSliceStatus::Pending);
    assert(session->recommended_continuation_work() == 64);
  }
  const auto whole_window = graph_labeling_model_slice(whole, whole_session);
  assert(whole_window.status == GraphLabelingSliceStatus::Pending);
  uint64_t split_calls = 0;
  while (split_session.recommended_continuation_work()) {
    const auto part = sliced(split, split_session, 64);
    assert(part.status == GraphLabelingSliceStatus::Pending);
    assert(part.work <= 64);
    assert(++split_calls < 10000);
  }
  assert(split_calls > 1);
  assert(!whole_session.recommended_continuation_work());
  assert(whole_session.candidate_fingerprint() ==
         split_session.candidate_fingerprint());
  assert(whole.stats.graph_labeling_nodes == split.stats.graph_labeling_nodes);
  assert(whole.stats.graph_labeling_work == split.stats.graph_labeling_work);

  // Default uses the retained action with its construction dose as the
  // Window invocation ceiling.
  Kernel policy_kernel;
  ScopedHeuristics policy_h(policy_kernel);
  permutation(policy_kernel, true);
  policy_kernel.graph_labeling_parameters = split.graph_labeling_parameters;
  GraphLabelingModelSession &policy_session = policy_kernel.graph_labeling_session;
  assert(graph_labeling_model_authenticate(policy_kernel, policy_session).status ==
         GraphLabelingAuthenticationStatus::Authenticated);
  assert(graph_labeling_model_slice(policy_kernel, policy_session).status ==
         GraphLabelingSliceStatus::Pending);
  DefaultStrategyConfig config;
  config.graph_labeling_model.enabled = true;
  config.graph_labeling_model.parameters = policy_kernel.graph_labeling_parameters;
  DefaultPolicy policy(config);
  const auto policy_slice =
      DefaultGraphLabelingTestAccess::service(policy, policy_kernel);
  assert(policy_slice.status == GraphLabelingSliceStatus::Pending);
  assert(policy_slice.work <= 64);
  assert(policy_session.recommended_continuation_work() == 64);
}

// The whole-run Lucky instance 21 through the public executor.
static void public_whole_run_finds_a_model() {
  Kernel k;
  ScopedHeuristics h(k);
  permutation(k, false);
  const auto original = k.graph_labeling_parameters;
  OnlineStrategyProgram program;
  OnlineInvocation step;
  step.id = 1;
  step.rule = "Lucky";
  step.granularity = InvocationGranularity::OneInstance;
  step.instances = {LUCKY_GRAPH_LABELING};
  step.parameters = original;
  program.sequence = {step};
  OnlineStrategyExecutor executor;
  std::string error;
  assert(executor.configure(program, 1, ReconfigureMode::ResetRuntime, &error));
  assert(executor.advance(k).status == InvocationStatus::Sat);
  assert(k.graph_labeling_parameters.work == original.work);
  assert(k.stats.graph_labeling_sat == 1);
  assert_model(k);
  assert(!graph_labeling_model_guard(k));
}

static void bounded_apply_publishes_guidance_only() {
  {
    Kernel k;
    ScopedHeuristics h(k);
    permutation(k, true);
    kernel_reserve_vars(k, 17);
    const auto saved = k.h->saved, target = k.h->target, best = k.h->best;
    assert(graph_labeling_model_apply(k) == ApplyResult::Ok);
    assert(!k.inconsistent && k.trail.empty());
    assert(k.stats.graph_labeling_sat == 0 && k.stats.graph_labeling_nodes > 0);
    assert(k.h->saved == saved && k.h->target == target && k.h->best == best);
    const auto &hint = k.local_search_hints[size_t(LocalSearchKind::GraphLabeling)];
    assert(hint.available() && hint.objective > 0 && hint.objective_exact);
    assert(hint.external_phases[k.internal_to_external[16]] == 0);
    assert(hint.effort.amount == k.stats.graph_labeling_work);
    // Each bounded call publishes a fresh identity without touching the trail.
    const uint64_t identity = hint.id;
    assert(graph_labeling_model_apply(k) == ApplyResult::Ok);
    assert(hint.id != identity && k.trail.empty());
  }
  {
    Kernel k;
    ScopedHeuristics h(k);
    permutation(k, false);
    k.graph_labeling_parameters.work.fill(0);
    assert(graph_labeling_model_apply(k) == ApplyResult::NoOp);
    assert(!k.stats.graph_labeling_work && k.stats.graph_labeling_budget == 1);
    assert(k.trail.empty() && !k.stats.local_search_hints_offered);
    k.graph_labeling_parameters.work.fill(100000);
    volatile sig_atomic_t stop = 1;
    k.stop = &stop;
    assert(graph_labeling_model_apply(k) == ApplyResult::NoOp);
    assert(!k.stats.graph_labeling_work && k.stats.graph_labeling_interrupted == 1);
  }
  {
    // An inherited whole-action ceiling bounds the sum of all stages.
    Kernel k;
    ScopedHeuristics h(k);
    permutation(k, false);
    k.work_budget = 1;
    k.work_budget_bounded = true;
    assert(graph_labeling_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.graph_labeling_work <= 1 && k.trail.empty());
  }
}

static void authenticate_and_continue_actions() {
  Kernel k;
  ScopedHeuristics h(k);
  permutation(k, false);
  assert(act(k, false).slice.status == GraphLabelingSliceStatus::NoStructure);
  auto auth = act(k, true);
  assert(auth.authentication.status == GraphLabelingAuthenticationStatus::Authenticated);
  assert(auth.authentication.work && k.graph_labeling_session.authenticated());
  const auto recognition = k.stats.graph_labeling_work;
  assert(act(k, true).authentication.work == 0);
  assert(k.stats.graph_labeling_work == recognition);
  auto slice = act(k, false);
  assert(slice.slice.status == GraphLabelingSliceStatus::Pending);
  assert(k.trail.empty() && !k.inconsistent && slice.slice.hint_published);
  const auto fingerprint = k.graph_labeling_session.candidate_fingerprint();
  const auto work = k.stats.graph_labeling_work;
  k.level = 1;
  assert(act(k, true).authentication.status == GraphLabelingAuthenticationStatus::NoStructure);
  assert(act(k, false).slice.status == GraphLabelingSliceStatus::NoStructure);
  assert(k.stats.graph_labeling_work == work);
  k.level = 0;
  volatile sig_atomic_t stop = 1;
  k.stop = &stop;
  assert(act(k, false).slice.status == GraphLabelingSliceStatus::Interrupted);
  assert(k.graph_labeling_session.candidate_fingerprint() == fingerprint);
  assert(k.trail.empty() && !k.inconsistent);
  k.stop = nullptr;
  Kernel copy = k;
  assert(!copy.graph_labeling_session.authenticated());
  assert(graph_labeling_model_slice(copy, k.graph_labeling_session).status ==
         GraphLabelingSliceStatus::NoStructure);
  assert(graph_labeling_model_authenticate(copy, k.graph_labeling_session).status ==
         GraphLabelingAuthenticationStatus::NoStructure);
  assert(k.graph_labeling_session.candidate_fingerprint() == fingerprint);
  ++k.graph_labeling_parameters.seed;
  assert(act(k, false).slice.status == GraphLabelingSliceStatus::NoStructure);
  assert(act(k, true).authentication.work == recognition);
  assert(!k.graph_labeling_session.candidate_fingerprint());
  k.graph_labeling_session.clear();
  assert(!k.graph_labeling_session.authenticated());
  assert(act(k, true).authentication.work == recognition);
  for (unsigned i = 0; i < 8 && slice.result != ApplyResult::Sat; ++i)
    slice = act(k, false);
  assert(slice.result == ApplyResult::Sat);
  assert_model(k);
}

// The C online API: authenticate then continue until SAT; the model satisfies
// every input clause.
static void c_api_reaches_a_checked_model() {
  auto *solver = tactix_init();
  std::vector<std::vector<int>> clauses;
  const auto add = [&](std::vector<int> c) {
    for (int lit : c) tactix_add(solver, lit);
    tactix_add(solver, 0);
    clauses.push_back(std::move(c));
  };
  for (int col = 0; col < 4; ++col) add({col + 1, col + 5, col + 9, col + 13});
  for (int row = 0; row < 4; ++row)
    for (int a = 0; a < 4; ++a)
      for (int b = a + 1; b < 4; ++b) add({-(4 * row + a + 1), -(4 * row + b + 1)});
  for (int a = 0; a < 4; ++a)
    for (int b = a + 1; b < 4; ++b) add({-(4 * a + 1), -(4 * b + 1)});
  uint64_t ids[] = {LUCKY_GRAPH_AUTHENTICATE, LUCKY_GRAPH_CONTINUE};
  tactix_online_step steps[8]{};
  for (unsigned i = 0; i < 8; ++i) {
    auto &s = steps[i];
    s.step_id = i + 1;
    s.rule_id = tactix_rule_id("Lucky");
    s.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
    s.instances = &ids[i ? 1 : 0];
    s.instance_count = 1;
    s.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
    s.repeat_limit = 1;
    s.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
    s.budget = 1;
    s.parameters.kind = TACTIX_ONLINE_PARAMETERS_GRAPH_LABELING;
    auto &p = s.parameters.value.graph_labeling;
    p.maximum_vertices = 16;
    p.workspace_bytes = 1 << 20;
    for (auto &dose : p.work) dose = 100000;
    p.maximum_dfs_vertices = 140;
    p.maximum_dfs_nodes = 50000000;
    p.maximum_dfs_starts = 12;
    p.early_dfs_choices = 28;
    p.repair_rounds = 3;
    p.anneal_iterations = 20;
    p.stall_anneal_iterations = 3;
  }
  tactix_online_program program{steps, 8, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(solver, &program, 0, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  tactix_online_result result{};
  bool sat = false;
  for (unsigned i = 0; i < 8 && !sat; ++i) {
    assert(tactix_online_advance(solver, &result) == 0);
    assert(result.instance == ids[i ? 1 : 0]);
    sat = result.status == TACTIX_ONLINE_SAT;
  }
  assert(sat);
  for (const auto &clause : clauses) {
    bool satisfied = false;
    for (int lit : clause) satisfied |= tactix_value(solver, lit) == lit;
    assert(satisfied);
  }
  tactix_release(solver);
}

int main() {
  default_policy_runs_the_graph_root_before_search();
  authentication_is_bounded();
  window_cursor_survives_bounded_slices();
  public_whole_run_finds_a_model();
  bounded_apply_publishes_guidance_only();
  authenticate_and_continue_actions();
  c_api_reaches_a_checked_model();
}
