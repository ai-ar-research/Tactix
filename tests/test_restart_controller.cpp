#undef NDEBUG
#include "policies/restart_controller.hpp"
#include "policies/ops/rephase/rephase.hpp"
#include "rules/decide/decide.hpp"

#include <cassert>
#include <cmath>

using namespace tactix;
using namespace tactix::strategy::controller;

int main() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  ScopedHeuristics embedding(k);
  embedding.h.saved[0] = -1;
  embedding.h.target[1] = 1;
  const auto *previous = k.h;
  const auto *saved = k.saved_phases;
  {
    RestartController controller(k);
    assert(k.h != previous);
    assert(k.h->saved[0] == -1 && k.h->target[1] == 1);
    const auto links = k.h->links;
    const auto scores = k.h->scores.score;
    const auto phases = k.h->saved;
    const auto touch = k.touch_epoch;
    const auto strategy = k.strategy_epoch;
    const auto calls = k.stats.rule_calls[0];
    const auto first = controller.observe();
    const auto second = controller.observe();
    assert(first.version == 1 && first.variables == 3);
    assert(first.irredundant_clauses == 1);
    assert(first.state.touch_epoch == second.state.touch_epoch);
    assert(first.sequence == second.sequence && first.sequence == 0);
    assert(!first.last_action && !first.searching);
    assert(!first.have_search_configuration);
    assert(k.touch_epoch == touch && k.strategy_epoch == strategy);
    assert(k.stats.rule_calls[0] == calls);
    assert(k.h->saved == phases && k.h->scores.score == scores);
    assert(k.h->links.size() == links.size());
    for (size_t i = 0; i < links.size(); ++i) {
      assert(k.h->links[i].prev == links[i].prev);
      assert(k.h->links[i].next == links[i].next);
    }
    // An action can add variables: detach must return the grown current H.
    kernel_reserve_vars(k, 4);
    k.h->saved[3] = -1;
    assert(controller.observe().variables == 4);
    ActionRequest request;
    request.sequence = 1;
    request.expected = tactix::strategy::state_version(k);
    request.invocation.rule = "Rephase";
    request.invocation.granularity =
        tactix::strategy::InvocationGranularity::OneInstance;
    request.invocation.instances = {REPHASE_ORIGINAL};
    auto stale = request;
    ++stale.expected.strategy_epoch;
    assert(controller.apply_rule(stale, {}).invocation.status ==
           tactix::strategy::InvocationStatus::StaleRequest);
    assert(controller.observe().sequence == 0);
    auto excluded = request;
    excluded.invocation.rule = "Decide";
    assert(controller.apply_rule(excluded, {}).invocation.status ==
           tactix::strategy::InvocationStatus::InvalidRequest);
    assert(controller.observe().sequence == 0);
    const auto rephased = k.stats.rephased;
    const auto applied = controller.apply_rule(request, {});
    assert(applied.invocation.applications == 1);
    assert(k.stats.rephased == rephased + 1);
    assert(controller.observe().last_action->sequence == 1);
    for (const auto phase : k.h->saved)
      assert(phase == 1);
    // Repetition is a fresh request, not fallback or undo of the first one.
    request.sequence = 2;
    request.expected = tactix::strategy::state_version(k);
    request.invocation.instances = {REPHASE_INVERTED};
    assert(controller.apply_rule(request, {}).invocation.applications == 1);
    for (const auto phase : k.h->saved)
      assert(phase == -1);
  }
  assert(k.h == previous && k.saved_phases == saved);
  assert(embedding.h.nvars == 4 && embedding.h.saved[3] == -1);
  {
    Kernel unattached;
    kernel_reserve_vars(unattached, 2);
    {
      RestartController controller(unattached);
      assert(unattached.h && unattached.saved_phases);
      assert(controller.observe().variables == 2);
    }
    assert(!unattached.h && !unattached.saved_phases);
  }
  {
    Kernel search;
    kernel_reserve_vars(search, 3);
    DefaultStrategyConfig settings;
    settings.restart.reluctant_period = 2;
    RestartController controller(search, settings);
    auto resume = [&](SearchConfiguration configuration) {
      ActionRequest request;
      request.type = ActionType::ResumeSearch;
      request.sequence = controller.observe().sequence + 1;
      request.expected = tactix::strategy::state_version(search);
      request.search = configuration;
      return controller.resume_search(request);
    };
    assert(resume({Branching::Vmtf, Restart::Reluctant, Phase::Negative})
               .resumed_search);
    assert(!search.h->stable);
    const auto first = controller.next_decision();
    assert(negated(first));
    assert(decide_apply(search, first, false) == ApplyResult::Ok);
    controller.capture_before_backtrack();
    assert(search.h->target[var_of(first)] == -1);
    assert(search.h->best[var_of(first)] == -1);
    assert(search.stats.target_saved == 1);
    // Strategy switching is rejected while search is active.
    assert(!resume({Branching::Evsids, Restart::AdaptiveGlue, Phase::Positive})
                .resumed_search);
    ++search.stats.conflicts;
    controller.search_conflict(2);
    assert(!controller.restart_due());
    assert(controller.observe().reluctant_wait == 1);
    controller.full_restart();
    assert(search.level == 0 && !controller.observe().searching);
    // Consecutive reluctant intervals retain sequence even across branch and
    // phase changes, and target/best survive the full restart.
    assert(resume({Branching::Evsids, Restart::Reluctant, Phase::Positive})
               .resumed_search);
    assert(search.h->stable);
    assert(controller.observe().reluctant_wait == 1);
    assert(!negated(controller.next_decision()));
    ++search.stats.conflicts;
    controller.search_conflict(2);
    assert(controller.restart_due());
    const auto ticked = controller.observe();
    assert(ticked.reluctant_u == 2 && ticked.reluctant_v == 1);
    controller.full_restart();
    assert(resume({Branching::Evsids, Restart::AdaptiveGlue, Phase::Target})
               .resumed_search);
    controller.search_conflict(7);
    const auto adaptive = controller.observe();
    assert(std::abs(adaptive.fast_glue - 7) < 1e-8);
    assert(std::abs(adaptive.slow_glue - 7) < 1e-8);
    controller.full_restart();
    assert(resume({Branching::Vmtf, Restart::Reluctant, Phase::Saved})
               .resumed_search);
    const auto reset = controller.observe();
    assert(reset.reluctant_u == 1 && reset.reluctant_v == 1);
    assert(reset.reluctant_wait == 2);
    assert(reset.fast_glue == adaptive.fast_glue);
    controller.full_restart();
    // Exercise all named combinations through actual installation boundaries.
    for (const auto configuration : search_configurations()) {
      assert(resume(configuration).resumed_search);
      assert(controller.observe().search == configuration);
      assert(search.h->stable == (configuration.branching == Branching::Evsids));
      const auto literal = controller.next_decision();
      if (configuration.phase == Phase::Positive)
        assert(!negated(literal));
      if (configuration.phase == Phase::Negative)
        assert(negated(literal));
      controller.full_restart();
    }
    assert(controller.observe().rotation_intervals == 4);
  }
  {
    Kernel k;
    kernel_reserve_vars(k, 2);
    RestartController controller(k);
    ActionRequest batch;
    batch.sequence = 1;
    batch.expected = tactix::strategy::state_version(k);
    batch.invocation.rule = "Rephase";
    batch.invocation.granularity = tactix::strategy::InvocationGranularity::SelectedBatch;
    batch.invocation.instances = {REPHASE_ORIGINAL, REPHASE_INVERTED};
    const auto first = controller.apply_rule(batch, {});
    assert(first.invocation.applications == 1 && !first.invocation.selection_complete);
    assert(k.h->saved[0] == 1);
    batch.sequence = 2;
    batch.expected = tactix::strategy::state_version(k);
    const auto second = controller.apply_rule(batch, {});
    assert(second.invocation.applications == 1 && second.invocation.selection_complete);
    assert(k.h->saved[0] == -1); // second payload, not a restarted first payload

    batch.sequence = 3;
    batch.expected = tactix::strategy::state_version(k);
    assert(controller.apply_rule(batch, {}).invocation.applications == 1);
    // External ABA invalidates the retained explicit selection, even though
    // the formula and trail shape are identical.
    ++k.strategy_epoch;
    batch.sequence = 4;
    batch.expected = tactix::strategy::state_version(k);
    const auto stale = controller.apply_rule(batch, {});
    assert(stale.invocation.status == tactix::strategy::InvocationStatus::StaleRequest);
    assert(stale.invocation.applications == 0 && k.h->saved[0] == 1);
    batch.sequence = 5;
    batch.expected = tactix::strategy::state_version(k);
    assert(controller.apply_rule(batch, {}).invocation.applications == 1);

    batch.sequence = 6;
    batch.expected = tactix::strategy::state_version(k);
    ActionAllowance zero;
    zero.amount = 0;
    const auto exhausted = controller.apply_rule(batch, zero);
    assert(exhausted.invocation.status == tactix::strategy::InvocationStatus::BudgetExhausted);
    assert(exhausted.invocation.applications == 0 && k.h->saved[0] == 1);
    batch.sequence = 7;
    batch.expected = tactix::strategy::state_version(k);
    volatile sig_atomic_t stop = 1;
    const auto interrupted = controller.apply_rule(batch, {}, &stop);
    assert(interrupted.invocation.status == tactix::strategy::InvocationStatus::Interrupted);
    assert(interrupted.invocation.applications == 0 && k.h->saved[0] == 1);
  }
}
