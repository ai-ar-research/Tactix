#pragma once
#include "rules/registry.hpp"
#include <vector>

namespace tactix {
struct RupConsequencePlan {
  std::vector<Lit> clause;
  bool already_true = false;
};
RuleEligibility rup_consequence_guard(const Kernel &);
// Authenticate a caller-selected live subset and check RUP using only its
// support. No main-formula copy, speculative main trail, or search.
bool rup_consequence_prepare(const Kernel &, const std::vector<int32_t> &clause,
    const std::vector<std::vector<int32_t>> &premises, uint64_t maximum_work,
    RupConsequencePlan &);
// Only commit a plan prepared against the unchanged kernel, through dispatch.
ApplyResult rup_consequence_commit(Kernel &, const RupConsequencePlan &);
} // namespace tactix
