#pragma once

#include "policies/heuristics.hpp"

namespace tactix {

// Bind execution events to the policy-owned optional H. Native consumers
// already drain event streams when H is absent, so no alternate path is needed.
struct HeuristicRulePublisher {
  void advice(Kernel &k) const { heuristics_consume_rule_advice(k); }
  void roots(Kernel &k, size_t begin) const {
    heuristics_consume_root_assignments(k, begin);
  }
  void retirements(Kernel &k) const { heuristics_consume_retirements(k); }
};

// The v1 public profile: rule events leave the kernel without reaching H.
// H then keeps eliminated, substituted and root-fixed variables active and
// never demotes Factor's fresh variables (gap plan finding 2).
struct DiscardedRulePublisher {
  void advice(Kernel &k) const { heuristics_discard_rule_events(k); }
  void roots(Kernel &, size_t) const {}
  void retirements(Kernel &) const {}
};

} // namespace tactix
