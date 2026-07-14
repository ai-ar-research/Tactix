// S3 Rephase (plan §5.2, kissat rephase.c): rewrite the saved phases —
// the instance space is the phase TYPE (payloads below); the canonical
// instance is Best. Pure H (proof delta: none); sound in any
// non-refuted state. WHEN to rephase and WHICH type rotation to use are
// policy state (rev 17: DefaultPolicy runs kissat's stable-only trigger
// and B,I,B,O rotation); the kissat-coupled root backtrack is likewise
// the policy's, composed from the same rule vocabulary (Restart with
// keep-level 0), not baked in here.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

// instance payloads (plan §5.1 v1 encoding for this rule)
constexpr uint64_t REPHASE_BEST = 0;     // overwrite saved from best[]
constexpr uint64_t REPHASE_INVERTED = 1; // all saved = -initial (-1)
constexpr uint64_t REPHASE_ORIGINAL = 2; // all saved = +initial (+1)
// (Walking joins with S6 Walk in phase 4)

bool rephase_guard(const Kernel &);
ApplyResult rephase_apply_type(Kernel &, uint64_t type);
ApplyResult rephase_apply(Kernel &); // canonical: Best

} // namespace tactix
