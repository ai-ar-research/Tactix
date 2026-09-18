#pragma once

#include "rules/registry.hpp"
#include "strategy/rule_parameters.hpp"

namespace tactix {

// Extended-resolution refutation for bipartite exact-one incidence systems
// whose two color classes have unequal cardinality modulo three.
ApplyResult exact_one_refutation_apply(
    Kernel &, const strategy::OnehotModelParameters &);

} // namespace tactix
