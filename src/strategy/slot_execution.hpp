#pragma once

// Typed inprocessing-slot rule state and follow-ups shared by DefaultPolicy's
// slot scheduler, tactix_support and the online executor. Scheduling (when a
// round starts, when a bound grows) stays with the caller.
#include "rules/strengthen/strengthen.hpp"

#include <algorithm>

namespace tactix::execution {

// A new forward-subsumption round: SubsumeForward and Strengthen start with
// their completeness flags set; an interrupted pass clears its own flag, which
// is what a round-end hold reads (Kissat forward_subsume_all_clauses).
struct BeginForwardRoundRequest {};

// Every variable becomes a BVE candidate again (eliminate.c
// try_to_eliminate_all_variables_again, after a growth-bound increase).
// Fixed and eliminated variables are filtered when a schedule is built.
struct RescheduleBveCandidatesRequest {};

// Strengthen's forward closure (PostApplication::ForwardClosure): reconnect
// the watches the pass disconnected and propagate the root suffix from
// `trail_from`, the application's entry propagation head.
struct ForwardClosureRequest {
  uint32_t trail_from = 0;
};

inline bool execute_action(Kernel &k, BeginForwardRoundRequest) {
  const bool changed = !k.subsume_complete || !k.strengthen_complete;
  k.subsume_complete = true;
  k.strengthen_complete = true;
  return changed;
}

inline bool execute_action(Kernel &k, RescheduleBveCandidatesRequest) {
  const bool changed =
      std::find(k.bve_retry.begin(), k.bve_retry.end(), uint8_t{0}) != k.bve_retry.end();
  std::fill(k.bve_retry.begin(), k.bve_retry.end(), uint8_t{1});
  return changed;
}

inline ApplyResult execute_action(Kernel &k, const ForwardClosureRequest &request) {
  return strengthen_reconnect_and_flush(k, request.trail_from);
}

} // namespace tactix::execution
