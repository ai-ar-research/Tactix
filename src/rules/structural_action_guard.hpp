#pragma once

#include "proof/proof.hpp"
#include "rules/registry.hpp"

#include <utility>

namespace tactix {

// Shared construction for structural action guards.  Call order is the
// journal order, so action-specific facts can remain in their original
// position between the common guard segments.
class StructuralActionGuard {
  const Kernel &kernel_;
  RuleEligibility result_;

public:
  explicit StructuralActionGuard(const Kernel &kernel,
                                 RuleEligibility result = {})
      : kernel_(kernel), result_(std::move(result)) {}

  StructuralActionGuard &fact(const char *name, bool value) {
    result_.fact(name, value);
    return *this;
  }

  StructuralActionGuard &quiescent() {
    return fact("no-conflict",
                !kernel_.conflict_valid && !kernel_.pending.valid)
        .fact("propagated", kernel_propagation_fixpoint(kernel_))
        .fact("sparse", !kernel_.dense && kernel_.large_watches_connected)
        .fact("no-probe-transaction", !kernel_.probing.recording);
  }

  StructuralActionGuard &provenance() {
    return fact("provenance-bound", kernel_.structural_provenance != nullptr);
  }

  StructuralActionGuard &identity() {
    return provenance().fact("action-id-available",
                             kernel_.structural_action_sequence != UINT64_MAX);
  }

  StructuralActionGuard &semantic_candidate() {
    return fact("candidate-slot-empty",
                !kernel_.semantic_variable_candidate_pending)
        .fact("candidate-id-available",
              kernel_.semantic_variable_publication != (UINT64_MAX >> 8));
  }

  StructuralActionGuard &clausal_certificate() {
    return fact("clausal-certificate",
                !kernel_.proof ||
                    (!kernel_.proof->lrat() &&
                     !kernel_.proof->terminal_pbp()));
  }

  RuleEligibility finish() { return std::move(result_); }
};

inline StructuralActionGuard action_guard(const Kernel &kernel) {
  StructuralActionGuard result(kernel);
  result.fact("consistent", !kernel.inconsistent).quiescent();
  return result;
}

inline StructuralActionGuard root_action_guard(const Kernel &kernel,
                                               bool assignment_free) {
  StructuralActionGuard result(kernel);
  result.fact("consistent", !kernel.inconsistent)
      .fact("root", !kernel.level);
  if (assignment_free)
    result.fact("assignment-free", kernel.trail.empty());
  result.quiescent();
  return result;
}

inline StructuralActionGuard extend_action_guard(
    const Kernel &kernel, RuleEligibility result) {
  return StructuralActionGuard(kernel, std::move(result));
}

} // namespace tactix
