#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// N1 stress policy (Track S · S3, stress-policy skill): hammers P2's
// snapshot/restore scope (kernel_probing_begin/end — watch-relocation and
// `searched` rollback) far harder than any uniform schedule: at every root
// visit it applies the probing-family rules (Probe, Backbone, Vivify,
// TransitiveReduce, Congruence — the kernel_probing scope consumers, most of
// which DefaultPolicy never schedules) in a seed-random order, so
// scope-open/scope-close cycles interleave densely with search that then
// REUSES the restored watch lists. Verdicts still reached via random DPLL;
// per-step kernel_check_invariants. Kept as the durable P2 stress axis
// (bve/congruence/factor precedent).
int probing_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop);

} // namespace tactix
