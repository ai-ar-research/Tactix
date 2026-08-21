// S3 Rephase (plan §5.2, kissat rephase.c): rewrite the saved phases. The
// instance space is the phase type (payloads below) and the canonical instance
// is Best. Pure `H`, no proof delta, sound in any non-refuted state. When to
// rephase and which type rotation to use are policy state; the kissat-coupled
// root backtrack is likewise the policy's, composed from Restart's shared
// backtrack mechanism rather than baked in here.  It is not a scheduled
// Restart and therefore does not advance restart cadence accounting.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

// instance payloads (plan §5.1 v1 encoding for this rule)
constexpr uint64_t REPHASE_BEST = 0;     // overwrite saved from best[]
constexpr uint64_t REPHASE_INVERTED = 1; // all saved = -initial (-1)
constexpr uint64_t REPHASE_ORIGINAL = 2; // all saved = +initial (+1)
// (Walking joins with S6 Walk in phase 4)

RuleEligibility rephase_guard(const Kernel &);
ApplyResult rephase_apply_type(Kernel &, uint64_t type);
ApplyResult rephase_apply(Kernel &); // canonical: Best

} // namespace tactix
