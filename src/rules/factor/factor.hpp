// E2 Factor: bounded variable addition. A fresh x factors an m×n grid {fᵢ ∨ rⱼ}
// into dividers {x∨fᵢ} and quotients {¬x∨rⱼ}. Contract: `docs/dev/calculus.md`
// E2; extension invariants: implementation-notes §5; design
// `docs/design/e2-factor-design.md`; from kissat factor.c (MIT).
#pragma once

#include "kernel/indexed_max_heap.hpp"
#include "rules/registry.hpp"

#include <csignal>
#include <functional>
#include <limits>
#include <vector>

namespace tactix {

// Kissat's mutable max-heap: bubble-up stops at equality and bubble-down
// prefers the left child, so equal scores keep heap topology.
class FactorHeap {
  IndexedMaxHeap<Lit, uint32_t> heap_;

public:
  bool empty() const { return heap_.empty(); }

  void update(Lit lit, uint32_t score) {
    heap_.ensure(lit);
    if (heap_.contains(lit)) {
      if (score <= 1) {
        heap_.assign_score(lit, score);
        heap_.erase(lit);
      } else {
        heap_.update_score(lit, score);
      }
    } else if (score > 1) {
      heap_.insert(lit, score);
    } else
      heap_.assign_score(lit, score);
  }

  Lit pop_max() { return heap_.pop_max(); }
};

// One native Factor occasion can resume across multiple atomic FX actions.
struct FactorContinuation {
  Var selectors;
  FactorHeap heap;
  std::vector<Lit> order;
  uint64_t arena_generation = 0;
  uint32_t root_fixed = 0;

  explicit FactorContinuation(Var selector_limit) : selectors(selector_limit) {}
};

// Grid selection strategy interface; the rule never imports policy types.
struct FactorSelection {
  bool fx_allowed = true;
  bool seek_first_fx = false;
  uint64_t fx_discovery_ticks = 0;
  FactorContinuation *continuation = nullptr;
  virtual ~FactorSelection() = default;
  virtual void set_bound(uint32_t) {}
  virtual uint32_t extension_growth_bound() const { return 0; }
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

// One exact E2 instance. `members` (row-major, factors × remainders) names the
// large-clause occurrence per cell (INVALID_REF for binaries); empty lets the
// rule choose. Validated before any mutation; matcher limits do not apply.
struct FactorGrid {
  std::vector<Lit> factors;
  std::vector<std::vector<Lit>> remainders;
  std::vector<Ref> members;
};

ApplyResult factor_apply_grid(Kernel &, const FactorGrid &);

// Deterministic lazy enumeration of the E2 action space, including zero-gain
// 2x2 grids; `members` keeps physical occurrences. The visitor must not mutate
// the kernel and may stop early.
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
