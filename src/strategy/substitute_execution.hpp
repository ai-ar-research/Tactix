#pragma once

#include "rules/substitute/substitute.hpp"
#include "strategy/rule_parameters.hpp"

namespace tactix::execution {

struct SubstituteRoundRequest {
  bool reconnect_large_watches = true;
};
struct SubstituteReconnectRequest {
  uint32_t trail_begin = 0;
};
struct SubstituteBatchRequest {
  uint32_t max_rounds = 1;
};
struct SubstituteBatchResult {
  ApplyResult result = ApplyResult::NoOp;
  uint32_t rounds = 0;
  bool reconnected = false;
};

// Preserve the complete helper's historical continuation profile. Budgeted
// default calls instead snapshot the kernel's live active population, which
// remains valid across compaction and fresh-variable activation.
inline uint64_t substitute_complete_active_count(const Kernel &k) {
  const uint64_t retired = static_cast<uint64_t>(kernel_root_fixed(k)) +
                           k.stats.bve_eliminated + k.stats.substitute_vars;
  return uint64_t{k.nvars} > retired ? uint64_t{k.nvars} - retired : 0;
}

inline ApplyResult execute_action(Kernel &k,
                                  const SubstituteRoundRequest &request) {
  return substitute_apply_round(k, request.reconnect_large_watches);
}
inline ApplyResult execute_action(Kernel &k,
                                  const SubstituteReconnectRequest &request) {
  return substitute_reconnect_large_watches(k, request.trail_begin);
}

// Policies supply round snapshots, internal publication and continuation
// advice. Execution owns the round call and one final watch reconnection.
// These callbacks are internal seams, not learner observation/selection points:
// a disconnected watch session must complete before public action feedback.
template <class Snapshot, class Publisher, class Advice>
SubstituteBatchResult execute_action(Kernel &k,
                                     const SubstituteBatchRequest &request,
                                     Snapshot &&snapshot, Publisher &&publish,
                                     Advice &&advise) {
  const bool connected_at_entry = k.large_watches_connected;
  const uint32_t trail_at_entry = static_cast<uint32_t>(k.trail.size());
  SubstituteBatchResult out;
  for (uint32_t round = 0; round < request.max_rounds; ++round) {
    const auto before = snapshot();
    const size_t trail_begin = k.trail.size();
    const ApplyResult current = execute_action(k, SubstituteRoundRequest{false});
    publish(trail_begin);
    ++out.rounds;
    const bool more = advise(before, current);
    if (current == ApplyResult::Unsat) {
      out.result = current;
      break; // An owned disconnected session still needs its final restoration.
    }
    if (current == ApplyResult::Ok)
      out.result = current;
    if (!more)
      break;
  }
  if (connected_at_entry && !k.large_watches_connected) {
    const ApplyResult closure =
        execute_action(k, SubstituteReconnectRequest{trail_at_entry});
    out.reconnected = true;
    if (closure == ApplyResult::Unsat)
      out.result = closure;
  }
  return out;
}

// A typed Substitute application (SubstituteParameters::max_rounds > 0):
// the round batch, its continuation and its per-round effort ceiling are the
// invocation's, in Substitute's own currency (stats.substitute_ticks) against
// the application's rule work budget. The publisher consumes each round's
// roots and retirements before the next round or the final reconnection.
struct SubstituteRoundsResult {
  ApplyResult result = ApplyResult::NoOp;
  uint32_t rounds = 0;
  uint64_t last_round_effort = 0;
  bool round_budget_break = false;
};

template <class Publisher>
SubstituteRoundsResult execute_substitute_rounds(
    Kernel &k, const strategy::SubstituteParameters &parameters,
    Publisher &&publish) {
  struct RoundSnapshot {
    uint64_t active;
    uint64_t effort;
  };
  SubstituteRoundsResult out;
  const uint64_t ceiling = k.work_budget;
  out.result = execute_action(
      k, SubstituteBatchRequest{parameters.max_rounds},
      [&] { return RoundSnapshot{kernel_active_variables(k), k.stats.substitute_ticks}; },
      [&](size_t trail_begin) { publish(trail_begin); },
      [&](const RoundSnapshot &before, ApplyResult current) {
        out.last_round_effort = k.stats.substitute_ticks - before.effort;
        ++out.rounds;
        if (current == ApplyResult::Unsat) return false;
        if (kernel_active_variables(k) >= before.active) return false;
        if (parameters.round_effort_ceiling && out.last_round_effort > ceiling) {
          out.round_budget_break = true;
          return false;
        }
        return true;
      }).result;
  return out;
}

} // namespace tactix::execution
