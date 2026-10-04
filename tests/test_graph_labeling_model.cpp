#undef NDEBUG
#include "rules/lucky/graph_labeling_model.hpp"
#include "rules/lucky/model.hpp"
#include "policies/heuristics.hpp"
#include "strategy/guidance_outcome.hpp"
#include "strategy/online_inprocess.hpp"

#include <cassert>

using namespace tactix;

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
    step.parameters = strategy::GraphLabelingParameters{};
    program.sequence = {step};
    strategy::OnlineStrategyExecutor executor;
    std::string error;
    assert(!executor.configure(program, 1,
                               strategy::ReconfigureMode::ResetRuntime,
                               &error));
    assert(error.find("retired") != std::string::npos);
    assert(k.graph_labeling_parameters.work == original.work);
    assert(k.trail.empty() && !k.stats.local_search_hints_offered);
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
    const auto saved = k.h->saved, target = k.h->target, best = k.h->best;
    assert(graph_labeling_model_apply(k) == ApplyResult::Ok);
    assert(!k.inconsistent && k.trail.empty());
    assert(k.stats.graph_labeling_sat == 0 && k.stats.graph_labeling_nodes > 0);
    assert(k.h->saved == saved && k.h->target == target && k.h->best == best);
    const auto &hint = k.local_search_hints[size_t(strategy::LocalSearchKind::GraphLabeling)];
    assert(hint.available() && hint.objective > 0 && hint.objective_exact);
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
