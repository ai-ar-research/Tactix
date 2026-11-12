#pragma once
#include "kernel/kernel.hpp"
#include <iterator>
#include <type_traits>
#include <vector>
namespace tactix {
// Candidate validation is independent of recognition and of every model-search
// strategy. Root assignments need their own check: they need not remain clauses.
template<class Charge>
bool lucky_model_valid(const Kernel &k, const std::vector<uint8_t> &assignment,
                       Charge &&charge) {
  if (assignment.size() != k.nvars) return false;
  const auto true_lit = [&](Lit lit) {
    // Publication omits unassigned retired variables, so their private values
    // cannot satisfy a live clause.
    if (kernel_var_retired(k, var_of(lit)) && !k.value(lit)) return false;
    return bool(assignment[var_of(lit)]) != negated(lit);
  };
  for (Var v = 0; v < k.nvars; ++v) {
    if (!charge(1)) return false;
    if (assignment[v] > 1) return false;
    const int value = k.value(make_lit(v, false));
    if (value && (value > 0) != bool(assignment[v])) return false;
  }
  for (const auto [ref, clause] : arena_records(k.arena)) {
    if (!charge(1)) return false;
    if (!clause->garbage()) {
      bool sat = false;
      for (Lit lit : *clause) {
        if (!charge(1)) return false;
        if (true_lit(lit)) { sat = true; break; }
      }
      if (!sat) return false;
    }
  }
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit) {
    if (!charge(1)) return false;
    for (const WatchEntry watch : k.watches.entries(lit)) {
      if (!charge(1)) return false;
      if (watch.binary() && lit <= watch.other() &&
          !true_lit(lit) && !true_lit(watch.other())) return false;
    }
  }
  return true;
}
inline bool lucky_model_valid(const Kernel &k,
                              const std::vector<uint8_t> &assignment) {
  return lucky_model_valid(k, assignment, [](uint64_t) { return true; });
}
// Private completion of a signed assignment (-1/0/+1): impose root values,
// then propagate binary and large clauses for at most `rounds` rounds. False
// on a falsified clause or an interrupt.
inline bool lucky_assign_literal(std::vector<int8_t> &assignment, Lit lit) {
  const Var variable = var_of(lit);
  const int8_t value = negated(lit) ? -1 : 1;
  if (assignment[variable] && assignment[variable] != value) return false;
  assignment[variable] = value;
  return true;
}
template <class It>
int lucky_propagate_clause(std::vector<int8_t> &assignment, It begin, It end) {
  Lit last = INVALID_LIT;
  uint32_t unassigned = 0;
  for (It it = begin; it != end; ++it) {
    const Lit lit = *it;
    const int8_t value = assignment[var_of(lit)];
    if (!value) {
      last = lit;
      ++unassigned;
    } else if ((value > 0) != negated(lit)) {
      return 0;
    }
  }
  if (!unassigned) return -1;
  if (unassigned == 1) return lucky_assign_literal(assignment, last) ? 1 : -1;
  return 0;
}
inline bool lucky_complete_by_propagation(Kernel &k, uint64_t rounds,
                                          uint64_t &interrupted_counter,
                                          uint64_t &rounds_counter,
                                          std::vector<int8_t> &assignment,
                                          bool require_fixpoint = false) {
  for (Var variable = 0; variable < k.nvars; ++variable) {
    const int value = k.value(make_lit(variable, false));
    if (value &&
        !lucky_assign_literal(assignment, make_lit(variable, value < 0)))
      return false;
  }
  for (uint64_t round = 0; round < rounds; ++round) {
    ++rounds_counter;
    bool changed = false;
    uint64_t visited = 0;
    for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
      for (const WatchEntry watch : k.watches.entries(lit)) {
        if (!watch.binary() || lit > watch.other()) continue;
        const Lit pair[2] = {lit, watch.other()};
        const int result =
            lucky_propagate_clause(assignment, std::begin(pair), std::end(pair));
        if (result < 0) return false;
        changed |= result > 0;
        if (!(++visited & ((uint64_t{1} << 20) - 1)) &&
            kernel_rule_interrupt(k, interrupted_counter))
          return false;
      }
    for (const auto [ref, clause] : arena_records(k.arena)) {
      if (!clause->garbage()) {
        const int result =
            lucky_propagate_clause(assignment, clause->begin(), clause->end());
        if (result < 0) return false;
        changed |= result > 0;
      }
    }
    if (!changed) return true;
  }
  return !require_fixpoint;
}
inline std::vector<uint8_t>
lucky_boolean_assignment(const std::vector<int8_t> &values) {
  std::vector<uint8_t> result(values.size());
  for (size_t i = 0; i < values.size(); ++i)
    result[i] = values[i] > 0;
  return result;
}
// A terminal transaction: the caller has validated the whole assignment and
// admitted publication work. Do not poll or stop partway through publication.
inline void lucky_publish_model(Kernel &k,
                                const std::vector<uint8_t> &assignment) {
  // A model check does not make the decisions BCP consequences: taint the
  // trail so backtracking rescans the retained prefix.
  k.trail_tainted = true;
  for (Var v = 0; v < k.nvars; ++v)
    if (!k.value(make_lit(v, false)) && !kernel_var_retired(k, v))
      kernel_assign_decision(k, make_lit(v, assignment[v] == 0));
  // The validated model also discharges any large-watch scan debt.
  k.propagate_head = static_cast<uint32_t>(k.trail.size());
  k.binary_only_head = UINT32_MAX;
}
// Validate, publish and count one candidate; false leaves the kernel untouched.
inline bool lucky_publish_if_model(Kernel &k,
                                   const std::vector<uint8_t> &assignment,
                                   uint64_t &sat_counter) {
  if (!lucky_model_valid(k, assignment)) return false;
  lucky_publish_model(k, assignment);
  ++sat_counter;
  return true;
}
// The internal variable of a one-based DIMACS variable, if it is still mapped.
inline bool lucky_internal_variable(const Kernel &k, uint64_t dimacs,
                                    Var &internal) {
  if (!dimacs || dimacs > k.external_to_internal.size()) return false;
  internal = k.external_to_internal[dimacs - 1];
  return internal != INVALID_VAR && internal < k.nvars;
}
// Sets a mapped one-based DIMACS variable in `assignment`, sized k.nvars.
template <class Value>
bool lucky_set_external(const Kernel &k, std::vector<Value> &assignment,
                        uint64_t dimacs, std::type_identity_t<Value> value) {
  Var internal = INVALID_VAR;
  if (!lucky_internal_variable(k, dimacs, internal)) return false;
  assignment[internal] = value;
  return true;
}
// 3^exponent, or false when it exceeds 64 bits.
inline bool lucky_power_of_three(uint32_t exponent, uint64_t &result) {
  result = 1;
  for (uint32_t i = 0; i < exponent; ++i) {
    if (result > UINT64_MAX / 3) return false;
    result *= 3;
  }
  return true;
}
}
