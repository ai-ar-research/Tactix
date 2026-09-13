#undef NDEBUG
#include "rules/lucky/parity_model.hpp"

#include <cassert>
#include <vector>

using namespace tactix;

static void add_mask_on(Kernel &k, Var a, Var b, Var c, unsigned mask) {
  Lit clause[3];
  const Var vars[3] = {a, b, c};
  for (unsigned i = 0; i != 3; ++i)
    clause[i] = make_lit(vars[i], (mask >> i) & 1);
  kernel_add_large_clause(k, clause, 3, false, 0);
}

static void add_mask(Kernel &k, unsigned mask) {
  add_mask_on(k, 0, 1, 2, mask);
}

static bool live_formula_satisfied(const Kernel &k) {
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage()) {
      bool sat = false;
      for (const Lit lit : *clause)
        sat = sat || k.value(lit) > 0;
      if (!sat)
        return false;
    }
    ref += clause_actual_words(clause);
  }
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
    for (const WatchEntry watch : k.watches.entries(lit))
      if (watch.binary() && lit <= watch.other() && k.value(lit) <= 0 &&
          k.value(watch.other()) <= 0)
        return false;
  return true;
}

static Kernel xor_fixture(bool complete) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  // These clauses forbid even-parity assignments, so x0 xor x1 xor x2 = 1.
  for (const unsigned mask : {0u, 3u, 5u, 6u}) {
    if (!complete && mask == 6)
      continue;
    add_mask(k, mask);
  }
  // Select a witness among the residual affine assignments.
  kernel_add_binary_clause(k, make_lit(0, true), make_lit(1, true));
  return k;
}

int main() {
  {
    Kernel k = xor_fixture(true);
    assert(parity_model_guard(k));
    assert(parity_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.parity_model_calls == 1);
    assert(k.stats.parity_model_equations == 1);
    assert(k.stats.parity_model_rank == 1);
    assert(k.stats.parity_model_sat == 1);
    assert(k.unassigned_active == 0 && live_formula_satisfied(k));
  }
  {
    Kernel k;
    kernel_reserve_vars(k, 4);
    for (const unsigned mask : {0u, 3u, 5u}) {
      add_mask_on(k, 0, 1, 2, mask);
      add_mask_on(k, 0, 1, 3, mask);
    }
    strategy::ParityModelParameters parameters;
    parameters.maximum_groups = 1;
    strategy::ScopedParityModelParameters posed(k, parameters);
    assert(parity_model_apply(k) == ApplyResult::NoOp);
    assert(k.trail.empty());
  }
  {
    Kernel k = xor_fixture(true);
    k.probing.recording = true;
    assert(!parity_model_guard(k));
    k.probing.recording = false;
    k.large_watches_connected = false;
    assert(!parity_model_guard(k));
  }
  {
    Kernel k = xor_fixture(false);
    assert(parity_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.parity_model_equations == 1);
    assert(live_formula_satisfied(k));
  }
  {
    Kernel k;
    kernel_reserve_vars(k, 3);
    add_mask(k, 0); // even and odd forbidden patterns: not one equation
    add_mask(k, 1);
    add_mask(k, 3);
    assert(parity_model_apply(k) == ApplyResult::NoOp);
    assert(k.trail.empty());
  }
  {
    Kernel k = xor_fixture(true);
    strategy::ParityModelParameters parameters;
    parameters.candidate_budget = 0;
    strategy::ScopedParityModelParameters posed(k, parameters);
    assert(parity_model_apply(k) == ApplyResult::NoOp);
    assert(k.trail.empty());
  }
  {
    Kernel k = xor_fixture(true);
    strategy::ParityModelParameters parameters;
    parameters.maximum_constraints = 4;
    strategy::ScopedParityModelParameters posed(k, parameters);
    assert(parity_model_apply(k) == ApplyResult::NoOp);
    assert(k.trail.empty());
  }
  {
    Kernel k = xor_fixture(true);
    volatile sig_atomic_t stop = 1;
    k.stop = &stop;
    assert(parity_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.parity_model_interrupted == 1 && k.trail.empty());
  }
  {
    strategy::ParityModelParameters invalid;
    invalid.minimum_group_clauses = 0;
    assert(!invalid.valid());
    invalid.minimum_group_clauses = 5;
    assert(!invalid.valid());
    invalid.minimum_group_clauses = 3;
    invalid.maximum_free_variables = 64;
    assert(!invalid.valid());
    invalid.maximum_free_variables = 4;
    invalid.maximum_groups = 0;
    assert(!invalid.valid());
    invalid.maximum_groups = 4;
    invalid.maximum_constraints = 0;
    assert(!invalid.valid());
  }
}
