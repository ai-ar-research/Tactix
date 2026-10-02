#pragma once

#include "kernel/kernel.hpp"
#include "strategy/policy_completion.hpp"

namespace tactix::execution {

struct MaterializeColdStateRequest {};
struct DisconnectLargeWatchesRequest {};
struct ReconnectLargeWatchesRequest {
  bool arena_order = false;
};
struct FlushRetiredRedundantRequest {};
struct SparseCollectRequest {
  Ref start = 0;
};
struct CompactVariablesRequest {
  bool fold_fixed = false;
};
struct CollectGarbageRequest {
  Ref watch_start = 0;
  bool sparse_collection = false;
  bool preserve_eager_recent = false;
};

inline void execute_action(Kernel &k, MaterializeColdStateRequest) {
  kernel_materialize_cold_state(k);
}
inline void execute_action(Kernel &k, DisconnectLargeWatchesRequest) {
  kernel_flush_large_watches(k);
}
inline void execute_action(Kernel &k, const ReconnectLargeWatchesRequest &request) {
  kernel_rebuild_large_watches(k, request.arena_order);
}
inline void execute_action(Kernel &k, FlushRetiredRedundantRequest) {
  kernel_flush_retired_redundant(k);
}
inline RootCollectOutcome execute_action(Kernel &k,
                                         const SparseCollectRequest &request) {
  return kernel_sparse_collect_root(k, request.start);
}
inline bool execute_action(Kernel &k, const CompactVariablesRequest &request) {
  return kernel_compact_variables(k, request.fold_fixed);
}
inline void execute_action(Kernel &k, const CollectGarbageRequest &request) {
  kernel_collect_garbage(k, request.watch_start, request.sparse_collection,
                         request.preserve_eager_recent);
}

// Optional completion describes the existing support transition, without
// turning a representation change into a proof-rule application. The disabled
// materialization path retains its original single native call.
inline void execute_action(Kernel &k, MaterializeColdStateRequest request,
                           PolicyCompletion *completion) {
  if (!completion) { execute_action(k, request); return; }
  (void)complete_action(completion, FeedbackKind::Support,
      "MaterializeColdState", INSTANCE_CANONICAL, [&] {
        NativeFeedbackSpec spec;
        spec.description.description = std::string("deferred=") +
            (k.cold_state_deferred ? "1" : "0");
        spec.description.effort_currency = "ticks";
        spec.meter = [](const Kernel &kernel) { return kernel.stats.ticks; };
        return spec;
      }, [&] {
        const bool deferred = k.cold_state_deferred;
        execute_action(k, request);
        return deferred ? ApplyResult::Ok : ApplyResult::NoOp;
      }, [](ApplyResult result) {
        auto outcome = strategy::controller::feedback_outcome(result);
        outcome.invocation.applications = 0;
        outcome.invocation.made_rule_progress = false;
        return outcome;
      });
}

inline RootCollectOutcome execute_action(Kernel &k,
                                         const SparseCollectRequest &request,
                                         PolicyCompletion *completion) {
  return complete_action(completion, FeedbackKind::Support,
      "SparseCollect", INSTANCE_CANONICAL, [&] {
        NativeFeedbackSpec spec;
        spec.description.description = "start=" + std::to_string(request.start);
        spec.description.effort_currency = "ticks";
        spec.meter = [](const Kernel &kernel) { return kernel.stats.ticks; };
        return spec;
      }, [&] { return execute_action(k, request); }, [](RootCollectOutcome result) {
        using strategy::controller::feedback_outcome;
        auto outcome = feedback_outcome(result == RootCollectOutcome::Refuted
            ? ApplyResult::Unsat : result == RootCollectOutcome::NoChange
                ? ApplyResult::NoOp : ApplyResult::Ok);
        outcome.invocation.applications = 0;
        return outcome;
      });
}

} // namespace tactix::execution
