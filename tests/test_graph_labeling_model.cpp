#undef NDEBUG
#include "rules/lucky/graph_labeling_model.hpp"
#include "rules/lucky/graph_labeling_window_cursor.hpp"
#include "rules/lucky/model.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "strategy/guidance_outcome.hpp"
#include "strategy/online_inprocess.hpp"
#include "strategy/work_execution.hpp"

#include <algorithm>
#include <cassert>

using namespace tactix;

namespace tactix {
struct DefaultGraphLabelingTestAccess {
  static GraphLabelingSliceResult service(DefaultPolicy &policy, Kernel &kernel) {
    return policy.service_graph_labeling(kernel);
  }
};
} // namespace tactix

static GraphLabelingSliceResult sliced(
    Kernel &kernel, GraphLabelingModelSession &session, uint64_t allowance) {
  strategy::ScopedRuleWorkBudget budget(
      kernel, {strategy::RuleWorkBudgetMode::Bounded, allowance});
  return graph_labeling_model_slice(kernel, session);
}

// Stops the solve at its first search decision, observed as the Decide
// feedback record, and checks the graph-root state that decision follows.
struct StopAfterGraphRoot final : strategy::controller::FeedbackReceiver {
  DefaultPolicy &policy;
  const Kernel &kernel;
  volatile sig_atomic_t stop = 0;
  unsigned picks = 0;
  StopAfterGraphRoot(DefaultPolicy &p, const Kernel &k) : policy(p), kernel(k) {}
  void search_interval(const strategy::controller::SearchIntervalFeedback &) override {}
  void after_action(const strategy::controller::ActionFeedback &record) override {
    if (record.kind != strategy::controller::FeedbackKind::SearchService ||
        record.identity != "Decide")
      return;
    assert(!picks++);
    const auto &diagnostics = policy.graph_labeling_diagnostics();
    assert(diagnostics.authentication_attempts == 1);
    assert(diagnostics.authentication_successes == 1);
    // The initial root run continues the trajectory until it stops being
    // pending (the owner runs its solver to completion before CDCL).
    assert(diagnostics.runs == 1 && diagnostics.fruitless_runs == 1);
    assert(diagnostics.slices >= 1 &&
           diagnostics.pending_slices == diagnostics.slices - 1);
    assert(diagnostics.useful_hints >= 1);
    assert(diagnostics.phase_install_attempts >= 1);
    assert(diagnostics.phase_installs == diagnostics.phase_install_attempts);
    assert(kernel.stats.graph_labeling_recognized == 1);
    assert(kernel.stats.graph_labeling_calls == diagnostics.slices + 1);
    assert(kernel.graph_labeling_session.authenticated());
    assert(kernel.graph_labeling_session.candidate_fingerprint());
    stop = 1;
  }
};

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
  // Redundant same-column graph exclusions. The first fixture's separation
  // one is solved by a permutation; the second deliberately induces an overly
  // strong projected objective, which must remain guidance rather than UNSAT.
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

int main() {
  {
    Kernel kernel;
    ScopedHeuristics heuristics(kernel);
    permutation(kernel, true);
    DefaultStrategyConfig config;
    config.affine_root_closure.enabled = false;
    config.affine_model.enabled = false;
    config.parity_model.enabled = false;
    config.onehot_model.enabled = false;
    config.ramsey_startup.enabled = false;
    config.signed_cardinality_proof.enabled = false;
    config.balanced_signature_model.enabled = false;
    config.exact_cover_model.enabled = false;
    config.independent_set_model.enabled = false;
    config.fermat_model.enabled = false;
    config.cerny_model.enabled = false;
    config.lookup_circuit_model.enabled = false;
    config.simon_model.enabled = false;
    config.stconn_proof.enabled = false;
    config.inprocess.program.clear();
    config.inprocess.append_unlisted_rules = false;
    config.inprocess.structured_root_stages.clear();
    config.inprocess.structured_root_bounded_backtracking_stages.clear();
    config.inprocess.structured_root_large_domain_stages.clear();
    config.inprocess.initial_fast_elimination = false;
    config.rephase.late_lucky = false;
    config.rephase.walk_warmup = false;
    config.graph_labeling_model.enabled = true;
    config.graph_labeling_model.parameters = kernel.graph_labeling_parameters;
    DefaultPolicy policy(config);
    StopAfterGraphRoot stopper(policy, kernel);
    policy.set_feedback(&stopper);
    assert(policy.solve(kernel, &stopper.stop) == 0);
    assert(stopper.picks == 1);
  }
  {
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

    Kernel replay;
    ScopedHeuristics replay_h(replay);
    permutation(replay, false);
    GraphLabelingModelSession replay_session;
    assert(graph_labeling_model_authenticate(replay, replay_session).status ==
           GraphLabelingAuthenticationStatus::Authenticated);
    auto first = graph_labeling_model_slice(measured, measured_session);
    auto second = graph_labeling_model_slice(replay, replay_session);
    assert(first.status == GraphLabelingSliceStatus::Pending);
    assert(second.status == GraphLabelingSliceStatus::Pending);
    assert(measured_session.candidate_fingerprint() ==
           replay_session.candidate_fingerprint());
    assert(first.hint_published && second.hint_published);
    assert(measured.trail.empty() && replay.trail.empty());
    for (unsigned i = 0; i < 8 && first.status != GraphLabelingSliceStatus::Sat;
         ++i)
      first = graph_labeling_model_slice(measured, measured_session);
    assert(first.status == GraphLabelingSliceStatus::Sat);
    assert(measured_session.complete() && measured.stats.graph_labeling_recognized == 1);
    std::vector<uint8_t> model(measured.nvars);
    for (Var v = 0; v < measured.nvars; ++v)
      model[v] = measured.value(make_lit(v, false)) > 0;
    assert(lucky_model_valid(measured, model));
  }
  {
    // The retained model adapter must preserve the Window cursor exactly:
    // many bounded public-style continuations reach the same frontier,
    // random stream, work count and candidate as one large continuation.
    Kernel boundary;
    ScopedHeuristics boundary_h(boundary);
    permutation(boundary, true);
    boundary.graph_labeling_parameters.work.fill(100000);
    boundary.graph_labeling_parameters.work[size_t(
        strategy::GraphLabelingStage::Construction)] = 64;
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
    boundary_small.graph_labeling_parameters =
        boundary.graph_labeling_parameters;
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
    const uint64_t boundary_fingerprint =
        boundary_session.candidate_fingerprint();
    const auto short_setup =
        sliced(boundary, boundary_session, setup - 1);
    assert(short_setup.status == GraphLabelingSliceStatus::Pending);
    assert(!short_setup.work);
    assert(boundary_session.candidate_fingerprint() == boundary_fingerprint);
    const auto exact_setup = sliced(boundary, boundary_session, setup);
    assert(exact_setup.status == GraphLabelingSliceStatus::Pending);
    assert(exact_setup.work == setup);
    assert(boundary_session.candidate_fingerprint() == boundary_fingerprint);

    Kernel whole, split;
    ScopedHeuristics whole_h(whole), split_h(split);
    permutation(whole, true);
    permutation(split, true);
    for (Kernel *kernel : {&whole, &split}) {
      kernel->graph_labeling_parameters.work.fill(100000);
      kernel->graph_labeling_parameters.work[size_t(
          strategy::GraphLabelingStage::Construction)] = 64;
    }
    GraphLabelingModelSession whole_session, split_session;
    assert(graph_labeling_model_authenticate(whole, whole_session).status ==
           GraphLabelingAuthenticationStatus::Authenticated);
    assert(graph_labeling_model_authenticate(split, split_session).status ==
           GraphLabelingAuthenticationStatus::Authenticated);
    assert(graph_labeling_model_slice(whole, whole_session).status ==
           GraphLabelingSliceStatus::Pending);
    assert(graph_labeling_model_slice(split, split_session).status ==
           GraphLabelingSliceStatus::Pending);
    assert(whole_session.recommended_continuation_work() == 64);
    assert(split_session.recommended_continuation_work() == 64);

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

    // Default uses the same retained action and its serialized construction
    // dose as the Window invocation ceiling; no separate hidden profile exists.
    Kernel policy_kernel;
    ScopedHeuristics policy_h(policy_kernel);
    permutation(policy_kernel, true);
    policy_kernel.graph_labeling_parameters = split.graph_labeling_parameters;
    GraphLabelingModelSession &policy_session =
        policy_kernel.graph_labeling_session;
    assert(graph_labeling_model_authenticate(policy_kernel, policy_session).status ==
           GraphLabelingAuthenticationStatus::Authenticated);
    assert(graph_labeling_model_slice(policy_kernel, policy_session).status ==
           GraphLabelingSliceStatus::Pending);
    DefaultStrategyConfig config;
    config.graph_labeling_model.enabled = true;
    config.graph_labeling_model.parameters =
        policy_kernel.graph_labeling_parameters;
    DefaultPolicy policy(config);
    const auto policy_slice =
        DefaultGraphLabelingTestAccess::service(policy, policy_kernel);
    assert(policy_slice.status == GraphLabelingSliceStatus::Pending);
    assert(policy_slice.work <= 64);
    assert(policy_session.recommended_continuation_work() == 64);
  }
  {
    Kernel k;
    ScopedHeuristics h(k);
    permutation(k, false);
    const auto original = k.graph_labeling_parameters;
    strategy::OnlineStrategyProgram program;
    strategy::OnlineInvocation step;
    step.id = 1;
    step.rule = "Lucky";
    step.granularity = strategy::InvocationGranularity::OneInstance;
    step.instances = {21};
    step.parameters = original;
    program.sequence = {step};
    strategy::OnlineStrategyExecutor executor;
    std::string error;
    assert(executor.configure(program, 1,
                              strategy::ReconfigureMode::ResetRuntime,
                              &error));
    assert(executor.advance(k).status == strategy::InvocationStatus::Sat);
    assert(k.graph_labeling_parameters.work == original.work);
    assert(k.stats.graph_labeling_sat == 1);
  }
  {
    Kernel k;
    ScopedHeuristics h(k);
    permutation(k, false);
    assert(graph_labeling_model_guard(k));
    assert(graph_labeling_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.graph_labeling_sat == 1 && k.stats.graph_labeling_seeds == 24);
    std::vector<uint8_t> model(k.nvars);
    for (Var v = 0; v < k.nvars; ++v) model[v] = k.value(make_lit(v, false)) > 0;
    assert(lucky_model_valid(k, model));
    assert(!graph_labeling_model_guard(k));
  }
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
    const auto &hint = k.local_search_hints[size_t(strategy::LocalSearchKind::GraphLabeling)];
    assert(hint.available() && hint.objective > 0 && hint.objective_exact);
    assert(hint.external_phases[k.internal_to_external[16]] == 0);
    assert(hint.effort.amount == k.stats.graph_labeling_work);
    // Repeated bounded calls publish a fresh exact identity, without sharing
    // old scores as a logical premise or mutating any trail/proof state.
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
    Kernel k;
    ScopedHeuristics h(k);
    permutation(k, false);
    // An inherited whole-action ceiling also constrains the sum of stages;
    // publication reservation cannot permit spending beyond that ceiling.
    k.work_budget = 1;
    k.work_budget_bounded = true;
    assert(graph_labeling_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.graph_labeling_work <= 1 && k.trail.empty());
  }
}
