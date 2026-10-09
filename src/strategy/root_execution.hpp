#pragma once

#include "strategy/action_execution.hpp"
#include "strategy/execution_scope.hpp"

#include <optional>

namespace tactix::execution {

struct RootPreparationRequest {
  bool probing = false;
  bool clear_unflushed_root = false;
};

// The public support argument (TACTIX_ROOT_PREPARATION_* flags) of a root
// preparation whose pre-restart callback captures target phases or not.
constexpr uint64_t root_preparation_argument(const RootPreparationRequest &request,
                                          bool capture_target) {
  return (request.probing ? 1u : 0u) | (capture_target ? 2u : 0u) |
         (request.clear_unflushed_root ? 4u : 0u);
}

struct SparseResumeRequest {
  uint32_t trail_begin = 0;
  bool charge_search_ticks = false;
};

template <class Propagate>
ApplyResult execute_action(Kernel &k, const SparseResumeRequest &request,
                           Propagate &&propagate) {
  assert(k.level == 0 && request.trail_begin <= k.trail.size());
  k.propagate_head = request.trail_begin;
  k.binary_only_head = UINT32_MAX;
  const bool inprocessing = k.inprocessing;
  if (request.charge_search_ticks) k.inprocessing = false;
  const ApplyResult result = propagate();
  k.inprocessing = inprocessing;
  return result;
}

// Internal callbacks retain native guidance and diagnostic boundaries. The
// executor owns the preparation bracket, full backtrack, closure and cleanup.
// This is a preparation transition, never a scheduled restart or an external
// learner boundary between backtracking and root propagation.
template <class BeforeRestart, class Restart, class Propagate>
ApplyResult execute_action(Kernel &k, const RootPreparationRequest &request,
                           BeforeRestart &&before_restart, Restart &&restart,
                           Propagate &&propagate) {
  std::optional<ExecutionScope<ExecutionContext::Inprocessing>> probing;
  if (request.probing) probing.emplace(k);
  if (k.level > 0 && restart_guard(k)) {
    before_restart();
    // Preserve the native caller's unconditional propagation after Restart,
    // including its existing root-refutation publication path.
    (void)restart(RestartRequest{0, RestartPurpose::Preparation});
  }
  const ApplyResult result = propagate();
  if (result == ApplyResult::Unsat) return result;
  assert(result == ApplyResult::Ok && k.level == 0 &&
         kernel_propagation_fixpoint(k));
  if (request.clear_unflushed_root) k.unflushed_root = 0;
  return result;
}

} // namespace tactix::execution
