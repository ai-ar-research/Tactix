#pragma once

#include "rules/reduce/reduce.hpp"

#include <span>

namespace tactix::execution {

struct ReductionPreparationRequest {};
struct ReductionRequest {
  std::span<const Ref> victims;
  bool compact_collection = false;
};

template <class RootPublisher>
void execute_action(Kernel &k, ReductionPreparationRequest,
                    RootPublisher &&publish_roots) {
  const uint64_t before = k.stats.root_assignments;
  reduce_prepare(k);
  const uint64_t roots = k.stats.root_assignments - before;
  if (roots) {
    // Preparation may truncate its incoming trail. Newly derived roots are
    // the final appended suffix, including when propagation refutes the root.
    assert(roots <= k.trail.size());
    publish_roots(k.trail.size() - roots);
  }
}

inline ApplyResult execute_action(Kernel &k, const ReductionRequest &request) {
  return reduce_apply_refs(k, request.victims.data(), request.victims.size(),
                            request.compact_collection);
}

} // namespace tactix::execution
