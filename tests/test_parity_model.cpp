#undef NDEBUG
#include "rules/lucky/parity_model.hpp"
#include "rules/lucky/parity_refutation.hpp"

#include "proof/proof.hpp"

#include <bit>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <string>
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

static void add_binary_xor(Kernel &k, Var a, Var b, bool rhs) {
  for (unsigned mask = 0; mask != 4; ++mask) {
    if ((std::popcount(mask) & 1) == rhs)
      continue;
    kernel_add_binary_clause(k, make_lit(a, mask & 1),
                             make_lit(b, mask & 2));
  }
}

static void add_xor(Kernel &k, std::initializer_list<Var> variables,
                    bool rhs) {
  assert(variables.size() >= 2 && variables.size() <= 4);
  const std::vector<Var> scope(variables);
  for (unsigned mask = 0; mask != (1u << scope.size()); ++mask) {
    if ((std::popcount(mask) & 1) == rhs)
      continue;
    Lit clause[4];
    for (size_t i = 0; i < scope.size(); ++i)
      clause[i] = make_lit(scope[i], mask & (1u << i));
    if (scope.size() == 2)
      kernel_add_binary_clause(k, clause[0], clause[1]);
    else
      kernel_add_large_clause(k, clause, scope.size(), false, 0);
  }
}

static Kernel inconsistent_xor_cycle() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_binary_xor(k, 0, 1, true);
  add_binary_xor(k, 1, 2, false);
  add_binary_xor(k, 0, 2, false);
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
    invalid.maximum_constraints = 4;
    invalid.refutation_minimum_probe_repeats = 0;
    assert(!invalid.valid());
  }
  {
    Kernel k = inconsistent_xor_cycle();
    const strategy::ParityModelParameters parameters;
    assert(parity_refutation_apply(k, parameters) == ApplyResult::Unsat);
    assert(k.inconsistent && k.stats.parity_refutation_recognized == 1 &&
           k.stats.parity_refutation_unsat == 1);
  }
  {
    // Caller bounds limit work; they must not become eager allocation sizes.
    // A tiny formula remains tiny even at every legal uint32 boundary.
    Kernel k = inconsistent_xor_cycle();
    strategy::ParityModelParameters parameters;
    parameters.refutation_maximum_variables =
        std::numeric_limits<uint32_t>::max();
    parameters.refutation_maximum_groups =
        std::numeric_limits<uint32_t>::max();
    parameters.refutation_maximum_constraints =
        std::numeric_limits<uint32_t>::max();
    parameters.refutation_probe_constraints =
        std::numeric_limits<uint32_t>::max();
    assert(parity_refutation_apply(k, parameters) == ApplyResult::Unsat);
    assert(k.inconsistent && k.stats.parity_refutation_recognized == 1);
  }
  {
    Kernel k;
    kernel_reserve_vars(k, 6);
    add_xor(k, {0, 1, 2, 3}, true);
    add_xor(k, {0, 1, 4, 5}, false);
    add_xor(k, {2, 3, 4, 5}, false);
    const strategy::ParityModelParameters parameters;
    assert(parity_refutation_apply(k, parameters) == ApplyResult::Unsat);
    assert(k.inconsistent && k.stats.parity_refutation_recognized == 1);
  }
  {
    Kernel k;
    kernel_reserve_vars(k, 3);
    add_binary_xor(k, 0, 1, false);
    add_binary_xor(k, 1, 2, false);
    add_binary_xor(k, 0, 2, false);
    const strategy::ParityModelParameters parameters;
    assert(parity_refutation_apply(k, parameters) == ApplyResult::NoOp);
    assert(!k.inconsistent && !k.stats.parity_refutation_recognized);
  }
  {
    Kernel k;
    kernel_reserve_vars(k, 3);
    add_binary_xor(k, 0, 1, true);
    add_binary_xor(k, 1, 2, false);
    const strategy::ParityModelParameters parameters;
    assert(parity_refutation_apply(k, parameters) == ApplyResult::NoOp);
  }
  {
    Kernel k;
    kernel_reserve_vars(k, 8);
    for (Var variable = 0; variable != 5; ++variable)
      kernel_add_binary_clause(k, make_lit(variable, false),
                               make_lit(variable + 1, false));
    k.input_nonunit_clause_count = 5;
    strategy::ParityModelParameters parameters;
    parameters.refutation_probe_constraints = 4;
    parameters.refutation_minimum_probe_repeats = 1;
    assert(parity_refutation_apply(k, parameters) == ApplyResult::NoOp);
    assert(k.stats.parity_refutation_probe_declines == 1);
  }
  {
    const std::string path = "/tmp/tactix-parity-refutation.dsr";
    Kernel k = inconsistent_xor_cycle();
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::DsrAscii));
    proof.bind_variable_map(&k.internal_to_external);
    proof.arm_population_shadow();
    k.proof = &proof;
    const strategy::ParityModelParameters parameters;
    assert(parity_refutation_apply(k, parameters) == ApplyResult::Unsat);
    k.proof = nullptr;
    proof.flush();
    std::ifstream input(path);
    const std::string contents((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
    assert(!contents.empty() && contents.ends_with("0\n"));
    assert(k.stats.parity_refutation_proof_additions > 0);
    assert(std::remove(path.c_str()) == 0);
  }
  {
    const std::string path = "/tmp/tactix-parity-preflight.dsr";
    Kernel k = inconsistent_xor_cycle();
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::DsrAscii));
    proof.bind_variable_map(&k.internal_to_external);
    k.proof = &proof;
    strategy::ParityModelParameters parameters;
    parameters.refutation_maximum_proof_additions = 1;
    assert(parity_refutation_apply(k, parameters) == ApplyResult::NoOp);
    k.proof = nullptr;
    proof.flush();
    std::ifstream input(path);
    const std::string contents((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
    assert(contents.empty() && !k.inconsistent);
    assert(std::remove(path.c_str()) == 0);
  }
}
