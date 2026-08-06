// Flag table for the temporary N1 stress policies (stress-policy skill).
//
// A stress policy is reachable only through the CLI (the fuzz harness drives the
// competition binary), and each one used to cost four separate additions to
// `main`: an include, a bool, a parse branch and a dispatch arm. That is one of
// the three separable units inside the 788-line `main` that
// benchmarks/reports/2026-08-06-review-post-track-sched.md files as `RV-S29`,
// and it grows every time a rule family lands.
//
// So the flag lives WITH the policies: one row per policy here, and `main` keeps
// a single parse branch plus a single dispatch. The nine policies that predate
// this table still have their own branches in `main`; moving them belongs to the
// parser split, not here, and until then both paths must keep working — the
// table is consulted first and a match short-circuits.
#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

using StressSolveFn = int (*)(Kernel &, uint64_t, volatile sig_atomic_t *);

// Match one argv entry against the table's `--<name>[=SEED]` flags. Returns the
// policy's solve function (and overwrites *seed when `=SEED` is present), or
// nullptr when `arg` is not one of these flags.
StressSolveFn stress_policy_flag(const char *arg, unsigned long long *seed);

// Space-separated flag list, for the usage line.
const char *stress_policy_flags();

} // namespace tactix
