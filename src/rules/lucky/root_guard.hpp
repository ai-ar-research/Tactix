#pragma once

#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/registry.hpp"

namespace tactix {

// The quiescent root of the Lucky opportunities. binary_fixpoint selects
// kernel_propagation_fixpoint, which also waits for the binary-only head.
inline RuleEligibility lucky_root_eligibility(const Kernel &kernel,
                                              bool binary_fixpoint = false) {
  RuleEligibility result;
  return result.fact("consistent", !kernel.inconsistent)
      .fact("no-conflict", !kernel.conflict_valid)
      .fact("no-pending", !kernel.pending.valid)
      .fact("root-level", kernel.level == 0)
      .fact("propagated", binary_fixpoint
                              ? kernel_propagation_fixpoint(kernel)
                              : kernel.propagate_head == kernel.trail.size())
      .fact("sparse-mode", !kernel.dense)
      .fact("no-probe-transaction", !kernel.probing.recording)
      .fact("large-watches-connected", kernel.large_watches_connected);
}

// The startup boundary of the original-input Lucky refutations.
inline RuleEligibility lucky_startup_eligibility(const Kernel &kernel) {
  RuleEligibility result;
  return result.fact("consistent", !kernel.inconsistent)
      .fact("root", kernel.level == 0 && !kernel.conflict_valid)
      .fact("original-input-boundary", kernel.startup_certificate_open);
}

} // namespace tactix
