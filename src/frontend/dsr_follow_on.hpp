// The DSR follow-on solve. A DSR proof cannot spell a startup counting
// refutation, so a run with an explicit DSR proof that ends on one solves the
// original formula again, with that opportunity off, and the second search writes
// the DSR proof to the same destination (Proof::refute_by_counting). The first
// run's search, verdict and statistics are those of every other proof mode (N2).
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

// `refuted` is the refuted run's kernel and `proof` its pending proof. `config` is
// the run's configuration, and `pose` poses the fresh kernel as the run's was.
// Stops with the proof error when the follow-on proof cannot be opened or a 20
// leaves no complete DSR proof.
DsrFollowOn run_dsr_follow_on(const Kernel &refuted, Proof &proof,
                              DefaultStrategyConfig config,
                              const std::function<void(Kernel &)> &pose,
                              volatile sig_atomic_t *stop);

} // namespace tactix
