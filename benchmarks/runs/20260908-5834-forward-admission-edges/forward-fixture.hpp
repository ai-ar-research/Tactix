// C2 continuation: actual mid-round units, the partial connection image,
// candidate root rechecks, proof-safe reasons, and full-propagation debt.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/strengthen/strengthen.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <string>
#include <unistd.h>

using namespace tactix;

struct Fixture {
  Kernel k;
  std::vector<std::vector<int>> inputs;
  uint32_t entry = 0;
  explicit Fixture(Proof *proof = nullptr) {
    kernel_reserve_vars(k, 16);
    k.proof = proof;
    if (proof) proof->bind_variable_map(&k.internal_to_external);
  }
  void add(std::initializer_list<int> external) {
    inputs.emplace_back(external);
    std::vector<Lit> lits;
    for (int lit : external) lits.push_back(internalize(lit));
    const uint64_t id = ++k.next_clause_id;
    if (lits.size() == 1) {
      k.unit_ids[var_of(lits[0])] = id;
      kernel_assign_unit(k, lits[0]);
    } else if (lits.size() == 2)
      kernel_add_binary_clause(k, lits[0], lits[1], id);
    else
      kernel_add_large_clause(k, lits.data(), lits.size(), false, 0, id);
  }
  void seed() { add({1, 2, 3}); add({-2, 3}); add({-1}); }
  void enter(bool dense) {
    assert(propagate_apply(k) == ApplyResult::Ok);
    assert(k.value(internalize(3)) == 0);
    entry = k.propagate_head;
    if (dense) kernel_flush_large_watches(k);
  }
  int finish(bool dense) {
    if (dense) {
      kernel_rebuild_large_watches(k);
      if (default_resume_sparse_propagation(k, entry, true) == ApplyResult::Unsat)
        return 20;
    }
    const int result = default_solve(k);
    if (result == 10) {
      kernel_extend_model(k);
      for (const auto &clause : inputs) {
        bool satisfied = false;
        for (int lit : clause) satisfied |= k.value(internalize(lit)) > 0;
        assert(satisfied);
      }
    }
    return result;
  }
};

