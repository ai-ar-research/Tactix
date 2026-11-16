#pragma once
#include "pigeonhole_formula.hpp"
#include "kernel/kernel.hpp"

// Add PHP without ingestion/normalization. Fresh IDs are optional, matching
// the existing fixtures; callers may already have reserved more variables.
inline void add_pigeonhole(tactix::Kernel &kernel, uint32_t pigeons,
                           uint32_t holes, PhpOrder order = PhpOrder::HoleMajor,
                           bool fresh_ids = false) {
  tactix::kernel_reserve_vars(kernel, pigeons * holes);
  for (const auto &external : pigeonhole_clauses(pigeons, holes, order)) {
    std::vector<tactix::Lit> clause;
    for (int32_t literal : external)
      clause.push_back(tactix::internalize(literal));
    const uint64_t id = fresh_ids ? ++kernel.next_clause_id : 0;
    if (clause.size() == 2)
      tactix::kernel_add_binary_clause(kernel, clause[0], clause[1], id);
    else
      tactix::kernel_add_large_clause(kernel, clause.data(), clause.size(),
                                      false, 0, id);
  }
}
