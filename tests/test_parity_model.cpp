#include "unit_test.hpp"
#include "xor_test_support.hpp"
#include "rule_test_utils.hpp"
#include "test_io.hpp"
#include "policies/default_policy.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/lucky/opportunities.hpp"

#include "proof/proof.hpp"

#include <initializer_list>
#include <limits>

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

static Kernel xor_fixture(bool complete) {
  Kernel k = kernel_with_vars(3);
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

static Kernel inconsistent_xor_cycle() {
  Kernel k = kernel_with_vars(3);
  add_xor_equation(k, {0, 1}, true);
  add_xor_equation(k, {1, 2}, false);
  add_xor_equation(k, {0, 2}, false);
  return k;
}

int main() {
  {
    // DefaultPolicy refutes a complete parity contradiction at the untouched
    // root, before structured-root mutation runs.
    Kernel k = inconsistent_xor_cycle();
    DefaultPolicy policy;
    assert(policy.solve(k) == 20);
    assert(k.stats.parity_refutation_recognized == 1);
    assert(k.stats.parity_refutation_unsat == 1);
    assert(policy.inprocess().structured_root.runs == 0);
  }
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
    Kernel k = kernel_with_vars(4);
    for (const unsigned mask : {0u, 3u, 5u}) {
      add_mask_on(k, 0, 1, 2, mask);
      add_mask_on(k, 0, 1, 3, mask);
    }
    strategy::ParityModelParameters parameters;
    parameters.maximum_groups = 1;
    strategy::ScopedRuleParameters<strategy::ParityModelParameters> posed(k, parameters);
    assert(parity_model_apply(k) == ApplyResult::NoOp);
    assert(k.trail.empty());
  }
  {
    Kernel k = xor_fixture(false);
    assert(parity_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.parity_model_equations == 1);
    assert(live_formula_satisfied(k));
  }
  {
    Kernel k = kernel_with_vars(3);
    add_mask(k, 0); // even and odd forbidden patterns: not one equation
    add_mask(k, 1);
    add_mask(k, 3);
    assert(parity_model_apply(k) == ApplyResult::NoOp);
    assert(k.trail.empty());
  }
  // No candidate budget, or too few constraints, declines without assigning.
  for (const auto change : {+[](strategy::ParityModelParameters &p) {
                              p.candidate_budget = 0;
                            },
                            +[](strategy::ParityModelParameters &p) {
                              p.maximum_constraints = 4;
                            }}) {
    Kernel k = xor_fixture(true);
    strategy::ParityModelParameters parameters;
    change(parameters);
    strategy::ScopedRuleParameters<strategy::ParityModelParameters> posed(k, parameters);
    assert(parity_model_apply(k) == ApplyResult::NoOp);
    assert(k.trail.empty());
  }
  {
    Kernel k = xor_fixture(true);
    kernel_reserve_vars(k, 5);
    kernel_add_binary_clause(k, make_lit(3, false), make_lit(4, false));
    strategy::ParityModelParameters parameters;
    parameters.candidate_budget = 1;
    strategy::ScopedRuleParameters<strategy::ParityModelParameters> posed(k, parameters);
    assert(parity_model_apply(k) == ApplyResult::NoOp);
    const auto &hint = k.local_search_hints[
        static_cast<size_t>(strategy::LocalSearchKind::Parity)];
    assert(hint.available());
    assert(hint.external_phases[k.internal_to_external[3]] == 0);
    assert(hint.external_phases[k.internal_to_external[4]] == 0);
  }
  {
    Kernel k = xor_fixture(true);
    volatile sig_atomic_t stop = 1;
    k.stop = &stop;
    assert(parity_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.parity_model_interrupted == 1 && k.trail.empty());
  }
  {
    // Free-variable enumeration uses 64-bit masks.
    strategy::ParityModelParameters parameters;
    assert(parameters.valid());
    parameters.maximum_free_variables = 64;
    assert(!parameters.valid());
  }
  {
    Kernel k = inconsistent_xor_cycle();
    const RuleVTable &lucky = tactix_rules()[tactix_rule_id("Lucky")];
    assert(apply_rule_instance(lucky, k, 6) == ApplyResult::NoOp);
    assert(!k.inconsistent && !k.stats.parity_refutation_recognized);
    assert(apply_rule_instance(lucky, k, LUCKY_PARITY_PROOF) ==
           ApplyResult::Unsat);
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
    Kernel k = kernel_with_vars(6);
    add_xor_equation(k, {0, 1, 2, 3}, true);
    add_xor_equation(k, {0, 1, 4, 5}, false);
    add_xor_equation(k, {2, 3, 4, 5}, false);
    const strategy::ParityModelParameters parameters;
    assert(parity_refutation_apply(k, parameters) == ApplyResult::Unsat);
    assert(k.inconsistent && k.stats.parity_refutation_recognized == 1);
  }
  {
    Kernel k = kernel_with_vars(3);
    add_xor_equation(k, {0, 1}, false);
    add_xor_equation(k, {1, 2}, false);
    add_xor_equation(k, {0, 2}, false);
    const strategy::ParityModelParameters parameters;
    assert(parity_refutation_apply(k, parameters) == ApplyResult::NoOp);
    assert(!k.inconsistent && !k.stats.parity_refutation_recognized);
  }
  {
    Kernel k = kernel_with_vars(3);
    add_xor_equation(k, {0, 1}, true);
    add_xor_equation(k, {1, 2}, false);
    const strategy::ParityModelParameters parameters;
    assert(parity_refutation_apply(k, parameters) == ApplyResult::NoOp);
  }
  {
    Kernel k = kernel_with_vars(8);
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
    const std::string contents = read_file(path);
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
    assert(read_file(path).empty() && !k.inconsistent);
    assert(std::remove(path.c_str()) == 0);
  }
}
