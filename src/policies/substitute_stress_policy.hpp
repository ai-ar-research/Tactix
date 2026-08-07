#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// N1 stress policy for V3 Substitute; shared contract in stress_registry.hpp.
// Both oracles are live: the rewrite adds transient equivalence lemmas to
// justify itself in LRAT, so a wrong witness is a wrong SAT answer and a wrong
// lemma a rejected proof. Drives it at every root to a fixpoint; the per-SCC
// `instances()` surface one representative at a time, the only way to reach a
// stale payload since `apply_instance` re-derives SCCs from the current graph;
// an interleave with the rules that rewrite the graph under it and with the
// other witness pushers; and Substitute off a propagation fixpoint, where
// `apply` completes the root closure itself and a conflict there refutes.
int substitute_stress_solve(Kernel &k, uint64_t seed,
                           volatile sig_atomic_t *stop);

} // namespace tactix
