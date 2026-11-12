// The DSR follow-on solve: a DSR proof cannot spell a startup counting
// refutation, so the original formula is solved again without it and the
// second search writes the DSR proof (Proof::refute_by_counting). The first
// run's search, verdict and statistics match every other proof mode (N2).
#pragma once

#include "kernel/kernel.hpp"
#include "policies/default_strategy_config.hpp"

#include <csignal>
#include <cstdint>
#include <functional>

namespace tactix {

struct DsrFollowOn {
  int result = 0; // 20 only once the DSR proof is complete
  uint64_t conflicts = 0, decisions = 0, propagations = 0, sr = 0, nanos = 0;
};

// `pose` poses the fresh kernel as the refuted run's was; stops with the proof
// error when the follow-on proof cannot be opened or leaves no complete proof.
DsrFollowOn run_dsr_follow_on(const Kernel &refuted, Proof &proof,
                              DefaultStrategyConfig config,
                              const std::function<void(Kernel &)> &pose,
                              volatile sig_atomic_t *stop);

} // namespace tactix
