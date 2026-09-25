#pragma once
#include "kernel/kernel.hpp"
#include <iterator>
#include <vector>
namespace tactix {
// Candidate validation is independent of recognition and of every model-search
// strategy. Root assignments need their own check: they need not remain clauses.
template<class Charge>
bool lucky_model_valid(const Kernel &k, const std::vector<uint8_t> &assignment,
                       Charge &&charge) {
  if (assignment.size() != k.nvars) return false;
  const auto true_lit = [&](Lit lit) {
    // Publication omits unassigned retired variables. A private completion
    // may choose their values, but those values cannot satisfy the published
    // live CNF (for example a redundant clause retained by caller-owned BVE).
    if (kernel_var_retired(k, var_of(lit)) && !k.value(lit)) return false;
    return bool(assignment[var_of(lit)]) != negated(lit);
  };
  for (Var v = 0; v < k.nvars; ++v) {
    if (!charge(1)) return false;
    if (assignment[v] > 1) return false;
    const int value = k.value(make_lit(v, false));
    if (value && (value > 0) != bool(assignment[v])) return false;
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!charge(1)) return false;
    if (!clause->garbage()) {
      bool sat = false;
      for (Lit lit : *clause) {
        if (!charge(1)) return false;
        if (true_lit(lit)) { sat = true; break; }
      }
      if (!sat) return false;
    }
    ref += clause_actual_words(clause);
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
// Private completion of a partial signed assignment (-1/0/+1 by variable):
// root values are imposed, then binary and large clauses are propagated in
// rounds. Returns false on a falsified clause or an interrupt. Bounded rounds
// keep a chained encoding's completion finite under a caller-dosed limit.
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
                                          std::vector<int8_t> &assignment) {
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
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *clause = clause_at(k.arena, ref);
      if (!clause->garbage()) {
        const int result =
            lucky_propagate_clause(assignment, clause->begin(), clause->end());
        if (result < 0) return false;
        changed |= result > 0;
      }
      ref += clause_actual_words(clause);
    }
    if (!changed) break;
  }
  return true;
}
// A terminal transaction: the caller has validated the whole assignment and
// admitted publication work. Do not poll or stop partway through publication.
inline void lucky_publish_model(Kernel &k,
                                const std::vector<uint8_t> &assignment) {
  // A whole-CNF check certifies this total assignment as a model, but does
  // not make its individual decisions BCP consequences. After a partial
  // restart removes a later satisfying decision, a clause watched by an
  // earlier false literal can become unit. Treat this trail like an
  // off-fixpoint Decide sequence so search backtracking rescans the retained
  // prefix before claiming another propagation fixpoint.
  k.trail_tainted = true;
  for (Var v = 0; v < k.nvars; ++v)
    if (!k.value(make_lit(v, false)) && !kernel_var_retired(k, v))
      kernel_assign_decision(k, make_lit(v, assignment[v] == 0));
  // lucky_model_valid checked every live binary and large clause against this
  // complete assignment. A binary-only probe may have left a large-watch scan
  // debt below the old trail end; publishing the validated model discharges
  // that debt as well as the ordinary propagation cursor.
  k.propagate_head = static_cast<uint32_t>(k.trail.size());
  k.binary_only_head = UINT32_MAX;
}
}
