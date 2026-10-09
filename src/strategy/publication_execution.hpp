#pragma once

#include "kernel/kernel.hpp"

namespace tactix::execution {

// Root publication origin. NewAssignments scans from the entry trail when the
// application assigned at level zero. RootTrail rescans the whole trail: an
// application that backtracks below its entry trail (ConflictAnalysis) leaves
// new root assignments before that offset. Consumers filter inactive
// variables, so a rescan is idempotent.
enum class RootPublication : uint8_t { Deferred, NewAssignments, RootTrail };

struct RuleApplicationRequest {
  RootPublication roots = RootPublication::Deferred;
  bool retirements = true;
  bool advice = true;
};

// DefaultPolicy's profile for a dosed Eliminate candidate (advice, new roots,
// retirements), which public NATIVE guidance publication applies after every
// registry dispatch (tactix_set_guidance_publication).
inline constexpr RuleApplicationRequest NATIVE_RULE_PUBLICATION{
    RootPublication::NewAssignments, true, true};

struct RuleApplicationResult {
  ApplyResult result;
  size_t trail_before;
  uint64_t roots_before;
  uint64_t eliminated_before;
  uint32_t propagation_begin;
};

// Native event publication is part of an application, before caller-owned
// parameter and work scopes unwind. Policies select the publication profile;
// the executor captures its boundary and drains advice, roots, then retirements.
// The publisher updates optional policy guidance, without exposing it to rules.
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
