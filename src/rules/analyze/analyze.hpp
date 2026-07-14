// M3 Analyze (plan §5.2): first-UIP conflict analysis with recursive
// minimization, all-UIP block shrinking, and on-the-fly subsumption.
// Learns the clause, emits its add-RUP proof delta, stores it in the
// arena (F/R/π changes), and hands the trail change to Backjump via
// kernel.pending (τ is Backjump's). Completes the M3 catalog row.
//
// API-freeze decision (2026-07-14): Analyze is ATOMIC — its instance
// space is the recorded conflict alone; minimization/shrinking/OTFS
// intensity (kissat options minimize/shrink/otfs) are NOT instance
// parameters. Knobs can join post-v1 additively if a policy preset needs
// them (phase-4 MAB); splitting the passes into separate rules is
// rejected — it would expose half-analyzed intermediate states to N1
// schedules (same reasoning as the OTFS fresh-allocation deviation).
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool analyze_guard(const Kernel &);
ApplyResult analyze_apply(Kernel &);

} // namespace tactix
