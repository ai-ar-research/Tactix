// E2 Factor — bounded variable addition (BVA), the binary-clause-driven variant
// that generalizes SBVA. Introduces a fresh variable x to factor an m×n clause
// grid {f_i ∨ r_j} into m dividers {x∨f_i} + n quotients {¬x∨r_j}, replacing m·n
// clauses by m+n. The inverse of BVE. Proof: add-RAT (dividers RAT-on-x vacuous,
// quotients RAT-on-¬x with the still-live originals as RUP witnesses) + delete —
// under DRAT via the ordinary add path with the pivot written first and the
// originals deleted LAST. The added variable is projected out of the model (no ρ
// witness). Design: docs/design/e2-factor-design.md. Transliterated from kissat
// factor.c (MIT). Native-LRAT (add_rat) is a later stage; gated off under --lrat.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool factor_guard(const Kernel &); // pure quiescent-root precondition
ApplyResult factor_apply(Kernel &);

} // namespace tactix
