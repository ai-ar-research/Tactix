// V2 Backbone (plan §5.2): binary-only failed-literal probing on one round
// trail; a binary conflict resolves back to its first repeated predecessor, a
// RUP root unit. Units replay at root and rounds repeat to a bounded fixpoint:
// the chained binary-UIP of Kissat `backbone.c` and CaDiCaL `backbone.cpp`.
// Contract: `docs/dev/calculus.md` §V2.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: quiescent root; `!conflict_valid` is abstract, as in V1.
RuleEligibility backbone_guard(const Kernel &);

// Instance space (plan §5.1): payload = probe literal; BIG heads, var
// ascending, positive first (as V1 Probe). INSTANCE_CANONICAL = the full round.

// Canonical: chained rounds within `work_budget` (Backbone ticks). The literal
// surface is the independent single probe N1 requires.
ApplyResult backbone_apply(Kernel &);

} // namespace tactix
