#pragma once

#include "strategy/controller_action_space.hpp"

#include <optional>

namespace tactix::strategy::controller {

// A value snapshot: acquiring it never enumerates instances, drains advice,
// changes scheduler state or consumes a random draw. Stats remain cumulative;
// consumers can choose their own measurement windows without resetting them.
struct Observation {
  uint32_t version = ACTION_SPACE_VERSION;
  StateVersion state;
  uint64_t sequence = 0;
  uint32_t variables = 0;
  uint32_t unassigned = 0;
  uint32_t active_unassigned = 0;
  uint32_t level = 0;
  uint64_t trail_assignments = 0;
  uint64_t root_population = 0;
  uint64_t irredundant_clauses = 0;
  bool inconsistent = false;
  bool conflict = false;
  bool propagation_fixpoint = false;
  bool searching = false;
  bool have_search_configuration = false;
  SearchConfiguration search;
  uint64_t rotation_intervals = 0;
  uint64_t next_restart_conflicts = 0;
  uint64_t reluctant_u = 1, reluctant_v = 1, reluctant_wait = 0;
  double fast_glue = 0, slow_glue = 0;
  Stats cumulative;
  std::optional<ActionResult> last_action;
  // Optional native-policy scheduling state. Root-controller snapshots keep
  // their named configuration fields; other policies can retain native clocks.
  std::string native_policy_state;
};

// Shared kernel half of the observation. Controller/policy scheduling fields
// are filled by their owner; this query never enumerates semantic instances.
inline Observation observe_kernel(const Kernel &k) {
  Observation result;
  result.state = state_version(k);
  result.variables = k.nvars;
  result.unassigned = k.unassigned;
  result.active_unassigned = k.unassigned_active;
  result.level = k.level;
  result.trail_assignments = k.trail.size();
  for (const Lit lit : k.trail)
    result.root_population += k.assigned[var_of(lit)].level == 0;
  result.irredundant_clauses = kernel_irredundant_clauses(k);
  result.inconsistent = k.inconsistent;
  result.conflict = k.conflict_valid;
  result.propagation_fixpoint = kernel_propagation_fixpoint(k);
  result.cumulative = k.stats;
  return result;
}

} // namespace tactix::strategy::controller
