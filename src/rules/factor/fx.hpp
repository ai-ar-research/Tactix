#pragma once
#include "kernel/kernel.hpp"
#include "rules/factor/fx_options.hpp"
#include <array>
#include <optional>
#include <span>
#include <vector>

namespace tactix {
struct FxContext {
  Ref positive, negative;
  uint64_t positive_id, negative_id;
  std::vector<Lit> remainder;
  bool negate_output;
};
// Only discovery constructs plans. References remain authenticated while this
// Kernel/arena generation stays current; replacing that Kernel ends the plan
// lifetime. Publication still rechecks source IDs and bodies.
struct FxPlan {
  FxGate gate_kind() const { return gate; }
  Lit left_literal() const { return left; }
  Lit right_literal() const { return right; }
  Lit else_literal() const { return otherwise; }
  std::span<const FxContext> pairs() const { return contexts; }
  int64_t reduction() const { return int64_t(contexts.size()) - 4; }
  uint64_t publication_work() const {
    uint64_t work = gate_work();
    for (const auto &context : contexts) work += context_work(context.remainder.size());
    return work;
  }

private:
  static uint64_t gate_work() { return 4 * uint64_t(clause_words(3)); }
  static uint64_t context_work(size_t size) {
    const uint64_t n = size;
    return (1 + n) + 4 * (n + 2) + (n + 1) + 6 * (n + 2);
  }
  FxPlan(FxGate g, Lit a, Lit b, Lit c, const Kernel &k)
      : gate(g), left(a), right(b), otherwise(c), owner(&k),
        arena_generation(k.arena_generation) {}
  friend std::optional<FxPlan> factor_find_fx(Kernel &, Lit,
      std::span<const Ref>, const FxOptions &, uint64_t &, uint64_t *);
  friend int32_t factor_publish_fx(Kernel &, const FxPlan &, uint64_t &, uint64_t *);
  FxGate gate;
  // XOR: left XOR right. ITE: left ? right : otherwise.
  Lit left, right, otherwise;
  std::vector<FxContext> contexts;
  const Kernel *owner;
  uint64_t arena_generation;
};
// Sources are live arena references from the caller's current occurrence
// image. Share that image with AND discovery rather than rebuilding it here.
// Changes only the Factor work ledger; revalidate the plan before publication.
std::optional<FxPlan> factor_find_fx(Kernel &, Lit selector,
                                     std::span<const Ref> sources,
                                     const FxOptions &, uint64_t &remaining_work,
                                     uint64_t *work_meter = nullptr);
// Publish one revalidated plan at a sparse, propagated root. Validation and
// publication work is reserved up front; a decline never changes the formula.
// The caller owns the registered action and its semantic-variable completion.
int32_t factor_publish_fx(Kernel &, const FxPlan &, uint64_t &remaining_work,
                          uint64_t *work_meter = nullptr);
}
