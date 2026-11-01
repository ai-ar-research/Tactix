#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

namespace tactix {

inline RuleEligibility lucky_root_eligibility(const Kernel &kernel) {
  RuleEligibility result;
  return result.fact("consistent", !kernel.inconsistent)
      .fact("no-conflict", !kernel.conflict_valid)
      .fact("no-pending", !kernel.pending.valid)
      .fact("root-level", kernel.level == 0)
      .fact("propagated", kernel.propagate_head == kernel.trail.size())
      .fact("sparse-mode", !kernel.dense)
      .fact("no-probe-transaction", !kernel.probing.recording)
      .fact("large-watches-connected", kernel.large_watches_connected);
}

} // namespace tactix
