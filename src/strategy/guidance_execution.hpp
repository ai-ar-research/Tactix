#pragma once

#include "policies/ops/local_search/local_search.hpp"
#include "policies/ops/reorder/reorder.hpp"
#include "policies/ops/rephase/rephase.hpp"
#include "policies/ops/switchmode/switchmode.hpp"
#include "policies/ops/walk/walk.hpp"
#include "strategy/work_execution.hpp"
#include "rules/factor/factor.hpp"

namespace tactix::execution {

struct ReorderRequest { uint64_t target = INSTANCE_CANONICAL; };
struct RephaseRequest { uint64_t type = REPHASE_BEST; };
struct SwitchModeRequest {};
struct CompactRandomOrderRequest {};
struct FactorSelectionInstallationRequest {
  FactorSelection &selection;
  std::optional<uint32_t> bound;
};
struct ClearFactorSelectionRequest {};
struct LocalSearchRotationRequest {
  const std::vector<strategy::LocalSearchKind> &rotation;
  uint64_t &cursor;
};
struct WalkRequest {
  uint64_t allowance;
  uint64_t seed;
};

inline ApplyResult execute_action(Kernel &k, const ReorderRequest &request) {
  return request.target == INSTANCE_CANONICAL
             ? reorder_apply(k) : reorder_apply_target(k, request.target);
}
inline ApplyResult execute_action(Kernel &k, const RephaseRequest &request) {
  return rephase_apply_type(k, request.type);
}
inline ApplyResult execute_action(Kernel &k, SwitchModeRequest) {
  return switchmode_apply(k);
}
inline void execute_action(Kernel &k, CompactRandomOrderRequest) {
  heuristics_compact_random_order_fixed(k);
}
inline void execute_action(Kernel &k, const FactorSelectionInstallationRequest &request) {
  k.factor_selection = &request.selection;
  if (request.bound) request.selection.set_bound(*request.bound);
}
inline void execute_action(Kernel &k, ClearFactorSelectionRequest) {
  k.factor_selection = nullptr;
}
inline ApplyResult execute_action(Kernel &k,
                                  const LocalSearchRotationRequest &request) {
  return local_search_guidance_apply_next(k, request.rotation, request.cursor);
}
inline ApplyResult execute_action(Kernel &k, const WalkRequest &request) {
  // This native profile owns only the handoff and clears it on completion.
  // Caller ceiling modes and offsets stay inherited; the chosen seed persists.
  WorkSettingsScope work(k, {request.allowance, std::nullopt, std::nullopt,
                            std::nullopt}, WorkSettingsExit::ClearBudget);
  k.walk_random_seed = request.seed;
  return walk_apply(k);
}

struct WarmupRequest {};
struct WarmupResult {
  bool interrupted = false;
  uint64_t decisions = 0;
  uint64_t propagations = 0;
  uint64_t conflicts = 0;
  uint64_t ticks = 0;
};

// Complete a speculative assignment while preserving assignment-time phases.
// Pick and diagnostic callbacks are internal seams, not external observations.
// No speculative trail or conflict record escapes this support transaction.
template <class Pick, class DecisionPublisher, class ImplicationPublisher>
WarmupResult execute_action(Kernel &k, WarmupRequest, Pick &&pick,
                            DecisionPublisher &&decision_published,
                            ImplicationPublisher &&implications_published) {
  assert(k.level == 0 && kernel_propagation_fixpoint(k));
  const uint64_t decisions = k.stats.warming_decisions;
  const uint64_t propagations = k.stats.warming_propagations;
  const uint64_t conflicts = k.stats.warming_conflicts;
  const uint64_t ticks = k.stats.ticks;
  WarmupResult out;
  k.warming = true;
  ++k.stats.warmups;
  while (k.unassigned_active) {
    if (kernel_terminating(k)) { out.interrupted = true; break; }
    const Lit decision = pick();
    kernel_assign_decision(k, decision);
    ++k.stats.warming_decisions;
    decision_published(decision);
    const size_t trail_begin = k.trail.size();
    kernel_propagate_beyond_conflicts(k);
    implications_published(trail_begin);
  }
  if (k.level) kernel_backtrack(k, 0);
  k.warming = false;
  out.decisions = k.stats.warming_decisions - decisions;
  out.propagations = k.stats.warming_propagations - propagations;
  out.conflicts = k.stats.warming_conflicts - conflicts;
  out.ticks = k.stats.ticks - ticks;
  return out;
}

} // namespace tactix::execution
