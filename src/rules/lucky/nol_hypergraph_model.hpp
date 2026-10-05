#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

namespace tactix {

// SAT-only construction for the noL family: active variables occur in
// consecutive triples, one positive triple requires a color for each active
// vertex, and every negative triple forbids a monochromatic hyperedge.  The
// action recognizes that language from the live CNF, runs bounded generic
// 3-color local search, and publishes only a whole-CNF-validated model.
RuleEligibility nol_hypergraph_model_guard(const Kernel &);
ApplyResult nol_hypergraph_model_apply(Kernel &);

} // namespace tactix
