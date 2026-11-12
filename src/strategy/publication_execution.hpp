#pragma once

#include "kernel/kernel.hpp"

namespace tactix::execution {

// Root publication origin: NewAssignments scans from the entry trail; RootTrail
// rescans the whole trail for applications that backtrack below it
// (ConflictAnalysis). Consumers filter inactive variables, so rescans are
// idempotent.
enum class RootPublication : uint8_t { Deferred, NewAssignments, RootTrail };

struct RuleApplicationRequest {
  RootPublication roots = RootPublication::Deferred;
  bool retirements = true;
  bool advice = true;
};

// DefaultPolicy's dosed-Eliminate profile (advice, roots, retirements), applied
// after every dispatch by public NATIVE publication
// (tactix_set_guidance_publication).
inline constexpr RuleApplicationRequest NATIVE_RULE_PUBLICATION{
    RootPublication::NewAssignments, true, true};

struct RuleApplicationResult {
  ApplyResult result;
  size_t trail_before;
  uint64_t roots_before;
  uint64_t eliminated_before;
  uint32_t propagation_begin;
};

// Event publication is part of an application, before parameter and work scopes
// unwind; the executor drains advice, roots, then retirements into optional
// policy guidance.
template <class Apply, class Publisher>
RuleApplicationResult execute_action(Kernel &k,
                                     const RuleApplicationRequest &request,
                                     Apply &&apply, Publisher &&publisher) {
  RuleApplicationResult out{ApplyResult::NoOp, k.trail.size(),
                            k.stats.root_assignments, k.stats.bve_eliminated,
                            k.propagate_head};
  out.result = apply();
  if (request.advice) publisher.advice(k);
  if (request.roots == RootPublication::RootTrail)
    publisher.roots(k, 0);
  else if (request.roots == RootPublication::NewAssignments &&
           k.stats.root_assignments != out.roots_before)
    publisher.roots(k, out.trail_before);
  if (request.retirements) publisher.retirements(k);
  return out;
}

} // namespace tactix::execution
