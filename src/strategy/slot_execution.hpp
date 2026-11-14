#pragma once

// Typed inprocessing-slot rule state and follow-ups shared by DefaultPolicy's
// slot scheduler, tactix_support and the online executor; scheduling stays with
// the caller.
#include "rules/strengthen/strengthen.hpp"
#include "rules/subsume_forward/subsume_forward.hpp"
#include "policies/heuristics.hpp"

#include <algorithm>

namespace tactix::execution {

// A new forward-subsumption round sets SubsumeForward's and Strengthen's
// completeness flags; an interrupted pass clears its own, which a round-end
// hold reads (Kissat forward_subsume_all_clauses).
struct BeginForwardRoundRequest {};

// Every variable becomes a BVE candidate again (eliminate.c
// try_to_eliminate_all_variables_again); fixed and eliminated ones are filtered
// at schedule build.
struct RescheduleBveCandidatesRequest {};

// One variable leaves the BVE candidates until re-armed (fastel.c).
struct RetireBveCandidateRequest {
  Var var = INVALID_VAR;
};

// Strengthen's forward closure from `trail_from`.
struct ForwardClosureRequest {
  uint32_t trail_from = 0;
};

// Compact the scanned prefix of `lit`'s occurrences (as Kissat does).
struct CompactFastelPrefixRequest {
  Lit lit = INVALID_LIT;
  size_t prefix = 0;
};
inline bool execute_action(Kernel &k, const CompactFastelPrefixRequest &request) {
  return subsume_forward_compact_fastel_prefix(k, request.lit, request.prefix);
}

// A slot boundary's retirement publication into guidance (H only).
struct PublishRetirementsRequest {};
inline void execute_action(Kernel &k, PublishRetirementsRequest) {
  heuristics_consume_retirements(k);
}

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

inline bool execute_action(Kernel &k, const RetireBveCandidateRequest &request) {
  if (request.var >= k.bve_retry.size() || !k.bve_retry[request.var])
    return false;
  k.bve_retry[request.var] = 0;
  return true;
}

inline ApplyResult execute_action(Kernel &k, const ForwardClosureRequest &request) {
  return strengthen_reconnect_and_flush(k, request.trail_from);
}

} // namespace tactix::execution
