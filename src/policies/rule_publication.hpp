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

} // namespace tactix
