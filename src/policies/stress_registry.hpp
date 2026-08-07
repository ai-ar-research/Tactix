// Flag table for the N1 stress policies, and the single in-tree home for the
// shared stress-policy contract below. A per-policy header states only its own
// risky schedule and knobs. Procedure: the stress-policy skill.
#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// The contract every stress policy owes. Each one maximizes a rare-but-legal
// rule schedule, against the bug family "a free schedule breaks a precondition
// DefaultPolicy meets by accident" — which `--random-policy` samples too rarely
// to trust. Each must: apply only rules whose guard holds (the N1 contract);
// still reach SAT/UNSAT, since the model and proof oracles fire only on a
// verdict, so a leg over zero applications is vacuous; be deterministic per
// seed (xorshift on the seed arg, no wall clock); call
// `kernel_check_invariants` per step rather than the every-256 default; and
// take knobs from env vars so a fuzz sweep needs no rebuild, with
// `TACTIX_TRACE` gating the per-step trace.
using StressSolveFn = int (*)(Kernel &, uint64_t, volatile sig_atomic_t *);

// Match one argv entry against the table's `--<name>[=SEED]` flags. Returns the
// policy's solve function (and overwrites *seed when `=SEED` is present), or
// nullptr when `arg` is not one of these flags. Policies predating the table
// still carry a hand-written branch in `main`; both paths must keep working,
// so `main` consults the table first and a match short-circuits.
StressSolveFn stress_policy_flag(const char *arg, unsigned long long *seed);

// Space-separated flag list, for the usage line.
const char *stress_policy_flags();

} // namespace tactix
