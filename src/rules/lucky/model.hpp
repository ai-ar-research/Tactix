#pragma once
#include "kernel/kernel.hpp"
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
// A terminal transaction: the caller has validated the whole assignment and
// admitted publication work. Do not poll or stop partway through publication.
inline void lucky_publish_model(Kernel &k,
                                const std::vector<uint8_t> &assignment) {
  for (Var v = 0; v < k.nvars; ++v)
    if (!k.value(make_lit(v, false)) && !kernel_var_retired(k, v))
      kernel_assign_decision(k, make_lit(v, assignment[v] == 0));
  k.propagate_head = static_cast<uint32_t>(k.trail.size());
}
}
