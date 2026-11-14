#pragma once
// Load external DIMACS clauses into a Kernel (one literal: a root unit, two: a
// binary, more: an irredundant large clause) and check extended models.
#ifdef NDEBUG
#error "clause_load_support.hpp asserts have side effects; #undef NDEBUG before including it"
#endif
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

using Cnf = std::vector<std::vector<int>>;

inline void load_clause(tactix::Kernel &k, const std::vector<int> &clause,
                        uint64_t id = 0) {
  std::vector<tactix::Lit> lits;
  for (const int literal : clause) lits.push_back(tactix::internalize(literal));
  assert(!lits.empty());
  if (lits.size() == 1)
    tactix::kernel_assign_unit(k, lits[0]);
  else if (lits.size() == 2)
    tactix::kernel_add_binary_clause(k, lits[0], lits[1], id);
  else
    tactix::kernel_add_large_clause(k, lits.data(), static_cast<uint32_t>(lits.size()),
                                    false, 0, id);
}

inline void load_cnf(tactix::Kernel &k, uint32_t variables, const Cnf &cnf) {
  tactix::kernel_reserve_vars(k, variables);
  for (const auto &clause : cnf) load_clause(k, clause);
}

// Whether the current assignment satisfies the external clause.
inline bool clause_satisfied(const tactix::Kernel &k, const std::vector<int> &clause) {
  for (const int literal : clause)
    if (k.values[tactix::internalize(literal)] > 0) return true;
  return false;
}

// Solve to SAT, extend the model, and require every original clause.
inline void assert_extended_model(tactix::Kernel &k, const Cnf &original) {
  assert(tactix::default_solve(k) == 10);
  tactix::kernel_extend_model(k);
  for (const auto &clause : original)
    assert(clause_satisfied(k, clause) &&
           "every original clause satisfied by the extended model");
}
