// E2 Factor — bounded variable addition (BVA), the binary-clause-driven variant
// that generalizes SBVA: a fresh x factors an m×n grid {fᵢ ∨ rⱼ} into m
// dividers {x∨fᵢ} + n quotients {¬x∨rⱼ}, the inverse of BVE. Precondition,
// effect, proof delta (add-RAT, emission order dividers→quotients→delete) and
// the model projection: `docs/calculus.md` E2; cross-rule invariants for
// extension variables: `docs/implementation-notes.md` §5. Design
// `docs/design/e2-factor-design.md`; transliterated from kissat factor.c (MIT).
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool factor_guard(const Kernel &); // pure quiescent-root precondition
ApplyResult factor_apply(Kernel &);

} // namespace tactix
