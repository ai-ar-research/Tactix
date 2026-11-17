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
struct FxPlan {
  FxGate gate;
  // XOR: left XOR right. ITE: left ? right : otherwise.
  Lit left, right, otherwise;
  std::vector<FxContext> contexts;
  const Kernel *owner = nullptr;
  uint64_t arena_generation = 0;
  int64_t reduction() const { return int64_t(contexts.size()) - 4; }
};
// Sources are live arena references from the caller's current occurrence
// image. Share that image with AND discovery rather than rebuilding it here.
// Changes only the Factor work ledger; revalidate the plan before publication.
std::optional<FxPlan> factor_find_fx(Kernel &, Lit selector,
                                     std::span<const Ref> sources,
                                     const FxOptions &, uint64_t &remaining_work,
                                     Lit partner_limit = INVALID_LIT);
// Publish one revalidated plan at a sparse, propagated root. A decline may
// consume discovery/validation work but never changes clauses or variables.
// The caller owns the registered action and its semantic-variable completion.
int32_t factor_publish_fx(Kernel &, const FxPlan &, uint64_t &remaining_work);
}
