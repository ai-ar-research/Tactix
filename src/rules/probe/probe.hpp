// V1 Probe (plan §5.2): failed-literal probing. Assume p, propagate; a conflict
// makes {¬p} a RUP unit. Units are asserted un-propagated and the round ends;
// M1 Propagate runs the cascade. Contract: `docs/dev/calculus.md` §V1; lineage:
// `docs/design/independent-lineage-survey.md` (kissat backbone.c/proprobe.c,
// cadical probe.cpp).
#pragma once

#include "rules/registry.hpp"

namespace tactix {

// Guard: quiescent root. `!conflict_valid` is abstract here — failed-literal
// reasoning assumes p atop a consistent trail.
RuleEligibility probe_guard(const Kernel &);

// Instance space (plan §5.1): payload = probe literal; every unassigned
// literal, var ascending, positive first. The BIG-head schedule
// (failed_literal_candidates) is only advice.

// Canonical: the full round against one clean root without re-propagation;
// a variable is skipped once one polarity fails, and both-fail refutation is
// left to M1. No clause is added (probehbr is rule X6) and no budget.
ApplyResult probe_apply(Kernel &);

} // namespace tactix
