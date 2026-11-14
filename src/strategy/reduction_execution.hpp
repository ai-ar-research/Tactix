#pragma once

#include "rules/reduce/reduce.hpp"

namespace tactix::execution {

struct ReductionPreparationRequest {};

template <class RootPublisher>
void execute_action(Kernel &k, ReductionPreparationRequest,
                    RootPublisher &&publish_roots) {
  const uint64_t before = k.stats.root_assignments;
  reduce_prepare(k);
  const uint64_t roots = k.stats.root_assignments - before;
  if (roots) {
    // Preparation may truncate the trail; new roots are the final suffix.
    assert(roots <= k.trail.size());
    publish_roots(k.trail.size() - roots);
  }
}

} // namespace tactix::execution
