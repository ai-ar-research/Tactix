#undef NDEBUG
#include "rules/lucky/graph_labeling_model.hpp"
#include "rules/lucky/model.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "strategy/guidance_outcome.hpp"
#include "strategy/online_inprocess.hpp"

#include <cassert>

using namespace tactix;

struct StopAfterGraphRoot final : BranchingPolicy {
  DefaultPolicy &policy;
  volatile sig_atomic_t &stop;
  unsigned picks = 0;
  StopAfterGraphRoot(DefaultPolicy &p, volatile sig_atomic_t &s)
      : policy(p), stop(s) {}
  const char *name() const override { return "graph-root-test"; }
  Lit pick(Kernel &kernel) override {
    assert(!picks++);
    const auto &diagnostics = policy.graph_labeling_diagnostics();
    assert(diagnostics.authentication_attempts == 1);
    assert(diagnostics.authentication_successes == 1);
    assert(diagnostics.slices == 1);
    assert(diagnostics.useful_hints == 1);
    assert(diagnostics.phase_install_attempts == 1);
    assert(diagnostics.phase_installs == 1);
    assert(kernel.stats.graph_labeling_recognized == 1);
    assert(kernel.stats.graph_labeling_calls == 2);
    assert(kernel.graph_labeling_session.authenticated());
    assert(kernel.graph_labeling_session.candidate_fingerprint());
    stop = 1;
    for (Var variable = 0; variable < kernel.nvars; ++variable)
      if (!kernel.value(make_lit(variable, false)) &&
          !kernel_var_retired(kernel, variable))
        return make_lit(variable, false);
    return INVALID_LIT;
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
    volatile sig_atomic_t stop = 0;
    StopAfterGraphRoot branching(policy, stop);
    policy.branching = &branching;
    assert(policy.solve(kernel, &stop) == 0);
    assert(branching.picks == 1);
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
