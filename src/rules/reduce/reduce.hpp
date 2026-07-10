// S2 Reduce (plan §5.2): delete the least valuable half of the redundant
// (learned) clauses — keeping low-glue tier-1 clauses and clauses locked as
// reasons — then compact the arena. Proof delta: `delete` per removed
// clause (from the closed vocabulary).
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool reduce_guard(const Kernel &);
bool reduce_advised(const Kernel &);
ApplyResult reduce_apply(Kernel &);

} // namespace tactix
