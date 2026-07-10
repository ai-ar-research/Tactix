// M1 Propagate (plan §5.2): unit propagation with blocking literals.
// Canonical instance: propagate the trail to fixpoint or first conflict.
// Proof delta: none (reasons recorded for Analyze's future LRAT hints).
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool propagate_guard(const Kernel &);
ApplyResult propagate_apply(Kernel &);

} // namespace tactix
