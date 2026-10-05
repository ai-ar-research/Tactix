// Bounded graph-interval equivalence publication.
//
// The action recognizes two independently encoded interval-absence literals
// with exactly the same authenticated row interval and publishes their Boolean
// equivalence as two RUP binary clauses.  It introduces no variables and does
// not propagate the published clauses; Propagate remains a separate action.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

RuleEligibility graph_interval_equivalence_guard(const Kernel &);
ApplyResult graph_interval_equivalence_apply(Kernel &);

} // namespace tactix
