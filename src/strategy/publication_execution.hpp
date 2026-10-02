#pragma once

#include "kernel/kernel.hpp"

namespace tactix::execution {

enum class RootPublication : uint8_t { Deferred, NewAssignments };

struct RuleApplicationRequest {
  RootPublication roots = RootPublication::Deferred;
  bool retirements = true;
};

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
  publisher.advice(k);
  if (request.roots == RootPublication::NewAssignments &&
      k.stats.root_assignments != out.roots_before)
    publisher.roots(k, out.trail_before);
  if (request.retirements) publisher.retirements(k);
  return out;
}

} // namespace tactix::execution
