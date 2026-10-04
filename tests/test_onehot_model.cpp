#undef NDEBUG
#include "proof/proof.hpp"
#include "rules/lucky/exact_one_refutation.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/lucky/onehot_model.hpp"

#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

using namespace tactix;

static Var indicator(unsigned domain, unsigned value) {
  return static_cast<Var>(3 * domain + value);
}

static void add_domain(Kernel &k, unsigned domain) {
  Lit some[3];
  for (unsigned value = 0; value != 3; ++value)
    some[value] = make_lit(indicator(domain, value), false);
  kernel_add_large_clause(k, some, 3, false, 0);
  for (unsigned a = 0; a != 3; ++a)
    for (unsigned b = a + 1; b != 3; ++b)
      kernel_add_binary_clause(k, make_lit(indicator(domain, a), true),
                              make_lit(indicator(domain, b), true));
}

static void add_equation(Kernel &k, std::array<unsigned, 3> coefficients,
                         unsigned rhs, int omit = -1) {
  int ordinal = 0;
  for (unsigned a = 0; a != 3; ++a)
    for (unsigned b = 0; b != 3; ++b)
      for (unsigned c = 0; c != 3; ++c) {
        if ((coefficients[0] * a + coefficients[1] * b +
             coefficients[2] * c) % 3 == rhs)
          continue;
        if (ordinal++ == omit)
          continue;
        const Lit clause[3] = {make_lit(indicator(0, a), true),
                               make_lit(indicator(1, b), true),
                               make_lit(indicator(2, c), true)};
        kernel_add_large_clause(k, clause, 3, false, 0);
      }
}

static Kernel fixture(bool complete = true) {
  Kernel k;
  kernel_reserve_vars(k, 9);
  for (unsigned domain = 0; domain != 3; ++domain)
    add_domain(k, domain);
  add_equation(k, {1, 2, 1}, 2, complete ? -1 : 0);
  return k;
}

static Kernel direct_csp_fixture() {
  constexpr unsigned domains = 4, width = 4;
  Kernel k;
  kernel_reserve_vars(k, domains * width);
  const auto encoded = [](unsigned domain, unsigned value) {
    return make_lit(static_cast<Var>(domain * width + value),
                    ((domain + value) & 1) != 0);
  };
  for (unsigned domain = 0; domain != domains; ++domain) {
    Lit clause[width];
    for (unsigned value = 0; value != width; ++value)
      clause[value] = encoded(domain, value);
    kernel_add_large_clause(k, clause, width, false, 0);
    for (unsigned a = 0; a != width; ++a)
      for (unsigned b = a + 1; b != width; ++b)
        kernel_add_binary_clause(k, negate(encoded(domain, a)),
                                negate(encoded(domain, b)));
  }
  // Adjacent CSP objects must choose the same value.  This produces a direct
  // encoding with signed one-hot literals and cross-domain forbidden pairs.
  for (unsigned domain = 0; domain + 1 != domains; ++domain)
    for (unsigned a = 0; a != width; ++a)
      for (unsigned b = 0; b != width; ++b)
        if (a != b)
          kernel_add_binary_clause(k, negate(encoded(domain, a)),
                                  negate(encoded(domain + 1, b)));
  return k;
}

static Kernel direct_csp_unsat_fixture() {
  constexpr unsigned domains = 2, width = 3;
  Kernel k;
  kernel_reserve_vars(k, domains * width);
  const auto lit = [](unsigned domain, unsigned value) {
    return make_lit(static_cast<Var>(domain * width + value), false);
  };
  for (unsigned domain = 0; domain != domains; ++domain) {
    Lit clause[width];
    for (unsigned value = 0; value != width; ++value)
      clause[value] = lit(domain, value);
    kernel_add_large_clause(k, clause, width, false, 0);
  }
  for (unsigned a = 0; a != width; ++a)
    for (unsigned b = 0; b != width; ++b)
      kernel_add_binary_clause(k, negate(lit(0, a)), negate(lit(1, b)));
  return k;
}

static Kernel direct_csp_alldifferent_pigeonhole_fixture() {
  constexpr unsigned domains = 5, clique_domains = 4, width = 3;
  Kernel k;
  kernel_reserve_vars(k, domains * width);
  const auto lit = [](unsigned domain, unsigned value) {
    return make_lit(static_cast<Var>(domain * width + value), false);
  };
  for (unsigned domain = 0; domain != domains; ++domain) {
    Lit clause[width];
    for (unsigned value = 0; value != width; ++value)
      clause[value] = lit(domain, value);
    kernel_add_large_clause(k, clause, width, false, 0);
    for (unsigned a = 0; a != width; ++a)
      for (unsigned b = a + 1; b != width; ++b)
        kernel_add_binary_clause(k, negate(lit(domain, a)),
                                negate(lit(domain, b)));
  }
  for (unsigned a = 0; a != clique_domains; ++a)
    for (unsigned b = a + 1; b != clique_domains; ++b)
      for (unsigned value = 0; value != width; ++value)
        kernel_add_binary_clause(k, negate(lit(a, value)),
                                negate(lit(b, value)));
  // Keep the all-different graph connected but non-complete.  Admission must
  // find the oversized clique itself rather than rely on component density.
  for (unsigned value = 0; value != width; ++value)
    kernel_add_binary_clause(k, negate(lit(0, value)),
                            negate(lit(4, value)));
  return k;
}

static void add_input_binary(Kernel &k, Lit a, Lit b) {
  kernel_add_binary_clause(k, a, b);
  k.input_binary_order.push_back(kernel_binary_key(a, b));
  ++k.input_binary_clause_count;
  ++k.input_nonunit_clause_count;
}

static void add_exact_one(Kernel &k, const std::vector<Var> &variables,
                          int omitted_pair = -1) {
  std::vector<Lit> positive;
  for (const Var variable : variables)
    positive.push_back(make_lit(variable, false));
  if (positive.size() == 2)
    add_input_binary(k, positive[0], positive[1]);
  else {
    kernel_add_large_clause(k, positive.data(), positive.size(), false, 0);
    ++k.input_nonunit_clause_count;
  }
  int pair = 0;
  for (size_t i = 0; i < variables.size(); ++i)
    for (size_t j = i + 1; j < variables.size(); ++j, ++pair)
      if (pair != omitted_pair)
        add_input_binary(k, make_lit(variables[i], true),
                        make_lit(variables[j], true));
}

// Exact-one constraints are the vertices of K_{left,right}; variables are
// its edges. Unequal color-class cardinalities make the system inconsistent.
static Kernel exact_one_bipartite_fixture(unsigned left, unsigned right,
                                          int omitted_pair = -1) {
  Kernel k;
  kernel_reserve_vars(k, left * right);
  k.input_binary_order_captured = true;
  for (unsigned row = 0; row < left; ++row) {
    std::vector<Var> variables;
    for (unsigned column = 0; column < right; ++column)
      variables.push_back(static_cast<Var>(row * right + column));
    add_exact_one(k, variables, row == 0 ? omitted_pair : -1);
  }
  for (unsigned column = 0; column < right; ++column) {
    std::vector<Var> variables;
    for (unsigned row = 0; row < left; ++row)
      variables.push_back(static_cast<Var>(row * right + column));
    add_exact_one(k, variables);
  }
  return k;
}

static Kernel exact_one_odd_cycle_fixture() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  k.input_binary_order_captured = true;
  add_exact_one(k, {0, 1});
  add_exact_one(k, {1, 2});
  add_exact_one(k, {2, 0});
  return k;
}

static bool satisfied(const Kernel &k) {
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage()) {
      bool sat = false;
      for (const Lit lit : *clause)
        sat |= k.value(lit) > 0;
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

int main() {
  {
    Kernel k = exact_one_bipartite_fixture(3, 2);
    assert(onehot_model_guard(k));
    const RuleVTable &lucky = tactix_rules()[tactix_rule_id("Lucky")];
    assert(apply_rule_instance(lucky, k, 7) == ApplyResult::NoOp);
    assert(!k.inconsistent && !k.stats.exact_one_refutation_recognized);
    assert(apply_rule_instance(lucky, k, LUCKY_EXACT_ONE_PROOF) ==
           ApplyResult::Unsat);
    assert(k.inconsistent && k.stats.exact_one_refutation_recognized == 1 &&
           k.stats.exact_one_refutation_equations == 5 &&
           k.stats.exact_one_refutation_unsat == 1);
  }
  {
    Kernel k = exact_one_bipartite_fixture(2, 2);
    strategy::OnehotModelParameters parameters;
    assert(exact_one_refutation_apply(k, parameters) == ApplyResult::NoOp);
    assert(!k.inconsistent && !k.stats.exact_one_refutation_recognized);
  }
  {
    Kernel k = exact_one_bipartite_fixture(3, 2);
    k.input_binary_order_captured = false;
    k.input_binary_order.clear();
    k.input_binary_clause_count = 0;
    k.input_nonunit_clause_count = 0;
    strategy::OnehotModelParameters parameters;
    assert(exact_one_refutation_apply(k, parameters) == ApplyResult::Unsat);
    assert(k.inconsistent && k.stats.exact_one_refutation_recognized == 1);
  }
  {
    Kernel k = exact_one_bipartite_fixture(3, 2, 0);
    strategy::OnehotModelParameters parameters;
    assert(exact_one_refutation_apply(k, parameters) == ApplyResult::NoOp);
    assert(!k.inconsistent);
  }
  {
    Kernel k = exact_one_odd_cycle_fixture();
    strategy::OnehotModelParameters parameters;
    assert(exact_one_refutation_apply(k, parameters) == ApplyResult::NoOp);
    assert(!k.inconsistent);
  }
  {
    const std::string path = "/tmp/tactix-exact-one-refutation.dsr";
    Kernel k = exact_one_bipartite_fixture(3, 2);
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::DsrAscii));
    proof.bind_variable_map(&k.internal_to_external);
    proof.arm_population_shadow();
    k.proof = &proof;
    strategy::OnehotModelParameters parameters;
    assert(exact_one_refutation_apply(k, parameters) == ApplyResult::Unsat);
    k.proof = nullptr;
    proof.flush();
    std::ifstream input(path);
    const std::string contents((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
    assert(!contents.empty() && contents.ends_with("0\n"));
    assert(k.stats.exact_one_refutation_proof_additions > 0);
    assert(std::remove(path.c_str()) == 0);
  }
  {
    const std::string path = "/tmp/tactix-exact-one-preflight.dsr";
    Kernel k = exact_one_bipartite_fixture(3, 2);
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::DsrAscii));
    proof.bind_variable_map(&k.internal_to_external);
    k.proof = &proof;
    strategy::OnehotModelParameters parameters;
    parameters.refutation_maximum_proof_additions = 1;
    assert(exact_one_refutation_apply(k, parameters) == ApplyResult::NoOp);
    k.proof = nullptr;
    proof.flush();
    std::ifstream input(path);
    const std::string contents((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
    assert(contents.empty() && !k.inconsistent);
    assert(std::remove(path.c_str()) == 0);
  }
  {
    const std::string path = "/tmp/tactix-exact-one-refutation.lrat";
    Kernel k = exact_one_bipartite_fixture(3, 2);
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::Lrat));
    k.proof = &proof;
    strategy::OnehotModelParameters parameters;
    assert(exact_one_refutation_apply(k, parameters) == ApplyResult::NoOp);
    assert(!k.inconsistent);
    k.proof = nullptr;
    proof.flush();
    assert(std::remove(path.c_str()) == 0);
  }
  {
    Kernel k = direct_csp_fixture();
    assert(onehot_model_guard(k));
    assert(direct_csp_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.direct_csp_model_calls == 1);
    assert(k.stats.direct_csp_model_domains == 4);
    assert(k.stats.direct_csp_model_edges == 3);
    assert(k.stats.direct_csp_model_restarts == 1);
    assert(k.stats.direct_csp_model_sat == 1);
    assert(k.unassigned_active == 0 && satisfied(k));
  }
  {
    Kernel k = direct_csp_unsat_fixture();
    kernel_reserve_vars(k, 7);
    k.var_status.assign(k.nvars, VarStatus::Active);
    k.var_status[6] = VarStatus::Eliminated;
    strategy::OnehotModelParameters parameters;
    parameters.candidate_budget = 1;
    strategy::ScopedOnehotModelParameters posed(k, parameters);
    const size_t arena_words = k.arena.size();
    assert(direct_csp_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.direct_csp_model_restarts == 1);
    const auto &hint = k.local_search_hints[
        static_cast<size_t>(strategy::LocalSearchKind::DirectCsp)];
    assert(k.stats.local_search_hints_offered == 1 && hint.available());
    assert(hint.external_phases[k.internal_to_external[6]] == 0);
    assert(k.trail.empty() && k.arena.size() == arena_words);
  }
  {
    Kernel k = direct_csp_alldifferent_pigeonhole_fixture();
    const size_t arena_words = k.arena.size();
    assert(direct_csp_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.direct_csp_model_infeasible == 1);
    assert(k.stats.direct_csp_model_restarts == 0);
    assert(k.stats.local_search_hints_offered == 0);
    assert(k.trail.empty() && k.arena.size() == arena_words);
  }
  {
    Kernel k = direct_csp_fixture();
    k.input_binary_order_captured = true;
    k.input_binary_order = {1, 2, 3};
    strategy::OnehotModelParameters parameters;
    parameters.candidate_budget = 0;
    strategy::ScopedOnehotModelParameters posed(k, parameters);
    assert(direct_csp_model_apply(k) == ApplyResult::NoOp);
    assert(!k.input_binary_order_captured && k.input_binary_order.empty());
  }
  {
    Kernel k = fixture();
    assert(onehot_model_guard(k));
    assert(onehot_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.onehot_model_calls == 1);
    assert(k.stats.onehot_model_domains == 3);
    assert(k.stats.onehot_model_equations == 1);
    assert(k.stats.onehot_model_rank == 1);
    assert(k.stats.onehot_model_candidates == 1);
    assert(k.stats.onehot_model_sat == 1);
    assert(k.unassigned_active == 0 && satisfied(k));
  }
  {
    Kernel k = fixture();
    kernel_reserve_vars(k, 11);
    kernel_add_binary_clause(k, make_lit(9, false), make_lit(10, false));
    strategy::OnehotModelParameters parameters;
    parameters.candidate_budget = 27;
    strategy::ScopedOnehotModelParameters posed(k, parameters);
    assert(onehot_model_apply(k) == ApplyResult::NoOp);
    const auto &hint = k.local_search_hints[
        static_cast<size_t>(strategy::LocalSearchKind::Onehot)];
    assert(hint.available());
    assert(hint.external_phases[k.internal_to_external[9]] == 0);
    assert(hint.external_phases[k.internal_to_external[10]] == 0);
  }
  {
    Kernel k = fixture();
    // The first affine candidate has domain 1 at value zero. Force the next
    // free assignment and prove that candidate enumeration is real.
    kernel_add_binary_clause(k, make_lit(indicator(1, 1), false),
                            make_lit(indicator(2, 1), false));
    assert(onehot_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.onehot_model_candidates > 1 && satisfied(k));
  }
  {
    Kernel k = fixture();
    // The all-zero free assignment chooses value zero in domain 1. A root
    // unit forbidding precisely that indicator must reject the candidate;
    // testing it as true and then skipping it during materialization used to
    // return a model with this domain entirely false.
    kernel_assign_unit(k, make_lit(indicator(1, 0), true));
    assert(!kernel_propagate(k));
    assert(onehot_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.onehot_model_candidates > 1 && satisfied(k));
    assert(k.value(make_lit(indicator(1, 0), false)) < 0);
  }
  {
    Kernel k = fixture(false);
    assert(onehot_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.onehot_model_equations == 0 && k.trail.empty());
  }
  {
    Kernel k = fixture();
    strategy::OnehotModelParameters parameters;
    parameters.maximum_constraints = 4;
    strategy::ScopedOnehotModelParameters posed(k, parameters);
    assert(onehot_model_apply(k) == ApplyResult::NoOp && k.trail.empty());
  }
  {
    Kernel k = fixture();
    strategy::OnehotModelParameters parameters;
    parameters.candidate_budget = 0;
    strategy::ScopedOnehotModelParameters posed(k, parameters);
    assert(onehot_model_apply(k) == ApplyResult::NoOp && k.trail.empty());
  }
  {
    Kernel k = fixture();
    volatile sig_atomic_t stop = 1;
    k.stop = &stop;
    assert(onehot_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.onehot_model_interrupted == 1 && k.trail.empty());
  }
  {
    strategy::OnehotModelParameters invalid;
    invalid.maximum_domains = 0;
    assert(!invalid.valid());
    invalid.maximum_domains = 3;
    invalid.minimum_domains = 4;
    assert(!invalid.valid());
    invalid.minimum_domains = 1;
    invalid.maximum_equation_arity = 0;
    assert(!invalid.valid());
    invalid.maximum_equation_arity = 3;
    invalid.maximum_constraints = 0;
    assert(!invalid.valid());
  }
}
