#pragma once

#include "rules/registry.hpp"
#include "strategy/rule_parameters.hpp"

namespace tactix {

// Exact extended-resolution refutation for complete bounded-arity XOR systems
// whose selected equations sum to 0 = 1.  It is the UNSAT half of Lucky's
// public parity strategy; NoOp means either no such contradiction or a clean
// resource/proof-format decline.
ApplyResult parity_refutation_apply(
    Kernel &, const strategy::ParityModelParameters &);

} // namespace tactix
