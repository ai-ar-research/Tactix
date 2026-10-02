#pragma once

#include "kernel/kernel.hpp"

namespace tactix::execution {

struct MaterializeColdStateRequest {};
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

} // namespace tactix::execution
