// M3 Analyze (plan §5.2): first-UIP conflict analysis. Learns the clause,
// emits its add-RUP proof delta, stores it in the arena (F/R/π changes),
// and hands the trail change to Backjump via kernel.pending (τ is
// Backjump's). Minimization/shrinking/OTFS are later phase-1 increments.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool analyze_guard(const Kernel &);
ApplyResult analyze_apply(Kernel &);

} // namespace tactix
