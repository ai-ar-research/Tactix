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

// Strategy used to select a factoring grid.  Factor owns this neutral
// interface; policies may implement it, but the rule never imports policy
// types.
struct FactorSelection {
  virtual ~FactorSelection() = default;
  virtual void set_bound(uint32_t) {}
  virtual void begin_sweep(const Kernel &) {}
  virtual void end_sweep() {}
  virtual double seed_score(const Kernel &, Lit seed,
                            double reference_score) = 0;
  virtual double next_factor_score(const Kernel &, Lit candidate,
                                   uint32_t cofactor, Lit seed,
                                   double reference_score) = 0;
  virtual bool accept(uint32_t rows, uint32_t columns, int delta) = 0;
};

bool factor_guard(const Kernel &); // pure quiescent-root precondition
ApplyResult factor_apply(Kernel &);

} // namespace tactix
