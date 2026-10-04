#pragma once

#include "kernel/kernel.hpp"
#include "strategy/rule_invocation.hpp"

namespace tactix::strategy {

// Shared completion interpretation for native and online constructor/guidance
// actions. Counter reads are pure: do not poll interruption again while
// reporting a completed operation, or overwrite a certified terminal result.
struct GuidanceOutcomeSnapshot {
  uint64_t stale, walk_interrupted, walk_budget, graph_interrupted, graph_budget;
  uint64_t ramsey_interrupted, ramsey_budget;
  explicit GuidanceOutcomeSnapshot(const Kernel &k)
      : stale(k.stats.local_search_hints_stale),
        walk_interrupted(k.stats.walk_seed_interrupted),
        walk_budget(k.stats.walk_seed_budget_exhausted),
        graph_interrupted(k.stats.graph_labeling_interrupted),
        graph_budget(k.stats.graph_labeling_budget),
        ramsey_interrupted(k.stats.ramsey_startup_interrupted),
        ramsey_budget(k.stats.ramsey_startup_exhausted) {}

  InvocationStatus refine(const Kernel &k, ApplyResult result,
                          InvocationStatus ordinary) const {
    if (result == ApplyResult::Sat || result == ApplyResult::Unsat ||
        result == ApplyResult::Conflict) return ordinary;
    if (stale != k.stats.local_search_hints_stale) return InvocationStatus::StaleRequest;
    if (walk_interrupted != k.stats.walk_seed_interrupted ||
        graph_interrupted != k.stats.graph_labeling_interrupted ||
        ramsey_interrupted != k.stats.ramsey_startup_interrupted)
      return InvocationStatus::Interrupted;
    if (walk_budget != k.stats.walk_seed_budget_exhausted ||
        graph_budget != k.stats.graph_labeling_budget ||
        ramsey_budget != k.stats.ramsey_startup_exhausted)
      return InvocationStatus::BudgetExhausted;
    return ordinary;
  }
};

} // namespace tactix::strategy
