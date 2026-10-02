#pragma once

#include "strategy/action_execution.hpp"
#include "strategy/execution_scope.hpp"

#include <optional>

namespace tactix::execution {

struct RootPreparationRequest {
  bool probing = false;
  bool clear_unflushed_root = false;
};

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
