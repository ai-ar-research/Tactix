// E2 Factor — bounded variable addition (BVA), the binary-clause-driven variant
// that generalizes SBVA: a fresh x factors an m×n grid {fᵢ ∨ rⱼ} into m
// dividers {x∨fᵢ} + n quotients {¬x∨rⱼ}, the inverse of BVE. Precondition,
// effect, proof delta (add-RAT, emission order dividers→quotients→delete) and
// the model projection: `docs/calculus.md` E2; cross-rule invariants for
// extension variables: `docs/implementation-notes.md` §5. Design
// `docs/design/e2-factor-design.md`; transliterated from kissat factor.c (MIT).
#pragma once

#include "rules/registry.hpp"

#include <csignal>
#include <functional>
#include <limits>
#include <vector>

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

RuleEligibility factor_guard(const Kernel &); // pure quiescent-root precondition
ApplyResult factor_apply(Kernel &);

// One exact E2 instance. `members`, when present, is row-major with
// factors.size()*remainders.size() entries and names the selected large-clause
// occurrence for each cell; binary cells use INVALID_REF.  An empty member
// vector asks the rule to choose the first distinct matching occurrences.
// Remainder literal order is immaterial.  The full grid is validated before
// any proof or formula mutation, and canonical matcher limits do not apply.
struct FactorGrid {
  std::vector<Lit> factors;
  std::vector<std::vector<Lit>> remainders;
  std::vector<Ref> members;
};

ApplyResult factor_apply_grid(Kernel &, const FactorGrid &);

// Exhaustive, deterministic enumeration of the abstract E2 action space.
// Factor sets and column sets are canonicalized, while `members` preserves the
// selected physical large-clause occurrence for every cell. This includes
// zero-gain 2x2 grids; profitability remains a canonical matcher decision. The
// visitor is called lazily, so an external strategy may stop or impose an explicit
// result budget without materializing the exponential grid space. The kernel
// must not be mutated from the visitor.
enum class FactorEnumerationStatus : uint8_t {
  Complete,
  Stopped,
  LimitReached,
};

struct FactorEnumerationOptions {
  uint64_t maximum_grids = std::numeric_limits<uint64_t>::max();
  volatile sig_atomic_t *stop = nullptr;
};

struct FactorEnumerationResult {
  FactorEnumerationStatus status = FactorEnumerationStatus::Complete;
  uint64_t grids = 0;
};

using FactorGridVisitor = std::function<bool(const FactorGrid &)>;
FactorEnumerationResult
factor_enumerate_grids(const Kernel &, const FactorGridVisitor &,
                       FactorEnumerationOptions = {});

} // namespace tactix
