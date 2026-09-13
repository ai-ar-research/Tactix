// Reduce preparation must consume newly derived roots before ranking again.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/reduce/reduce.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <string>
#include <unistd.h>
using namespace tactix;

static void prepare(Kernel &k) {
  default_prepare_reduction(k);
}

static void retained_chain(bool policy) {
  Kernel k;
  kernel_reserve_vars(k, 8);
  const Lit clause[] = {internalize(-1), internalize(3), internalize(4)};
  kernel_add_large_clause(k, clause, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  ScopedHeuristics h(k);
  k.h->stable = true;
  heuristics_update_scores(k);
  kernel_assign_decision(k, internalize(-1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(2)) > 0 && k.assigned[1].level == 1);
  kernel_assign_decision(k, internalize(5));
  kernel_assign_decision(k, internalize(6));
  const size_t root_begin = k.trail.size();
  kernel_assign_unit(k, internalize(-3));
  kernel_assign_unit(k, internalize(-4));
  assert(propagate_apply(k) == ApplyResult::Ok);
  heuristics_consume_root_assignments(k, root_begin);
  assert(k.unflushed_root && kernel_propagation_fixpoint(k));
  const size_t old_size = k.trail.size();
  const auto old_roots = k.stats.root_assignments;
  if (policy) prepare(k); else reduce_prepare(k);
  assert(!k.inconsistent && !k.level && kernel_propagation_fixpoint(k));
  assert(k.stats.root_assignments == old_roots + 2 && k.trail.size() < old_size);
  for (Var v : {Var(0), Var(1)}) {
    assert(k.assigned[v].level == 0);
    const bool expected_active = !policy;
    assert(heuristics_var_active(k, v) == expected_active);
    assert(heuristics_heap_contains(k, v) == expected_active);
  }
  for (Var v : {Var(4), Var(5), Var(6), Var(7)})
    assert(heuristics_var_active(k, v) && !k.value(make_lit(v, false)));
  const auto heap = k.h->scores.stack;
  const auto roots = k.stats.root_assignments;
  if (policy) prepare(k); else reduce_prepare(k);
  assert(k.h->scores.stack == heap && k.stats.root_assignments == roots);
  kernel_check_invariants(k);
  assert(default_solve(k) == 10);
  kernel_extend_model(k);
  assert(k.value(internalize(-1)) > 0 && k.value(internalize(2)) > 0);
  assert(k.value(internalize(-3)) > 0 && k.value(internalize(-4)) > 0);
  std::printf("policy=%d retained-chain roots=2 original SAT model PASS\n", policy);
}

static void no_root_work() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  ScopedHeuristics h(k);
  k.h->stable = true;
  heuristics_update_scores(k);
  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  const auto trail = k.trail;
  const auto heap = k.h->scores.stack;
  prepare(k);
  assert(k.level == 1 && k.trail == trail && k.h->scores.stack == heap);
  kernel_check_invariants(k);
}

static void refutation(unsigned kind, ProofFormat format, const std::string &stem) {
  const bool keep = !stem.empty();
  const std::string path = keep ? stem + ".proof" :
      "/tmp/tactix-reduce-root-" + std::to_string(getpid()) + ".proof";
  Proof proof;
  assert(proof.open(path.c_str(), format));
  Kernel k;
  kernel_reserve_vars(k, 8);
  k.proof = &proof;
  proof.bind_variable_map(&k.internal_to_external);
  std::vector<std::vector<int>> inputs;
  const auto add = [&](std::initializer_list<int> clause) {
    inputs.emplace_back(clause);
    std::vector<Lit> lits;
    for (int lit : clause) lits.push_back(internalize(lit));
    const uint64_t id = ++k.next_clause_id;
    if (lits.size() == 1) {
      k.unit_ids[var_of(lits[0])] = id;
      kernel_assign_unit(k, lits[0]);
    } else if (lits.size() == 2)
      kernel_add_binary_clause(k, lits[0], lits[1], id);
    else kernel_add_large_clause(k, lits.data(), lits.size(), false, 0, id);
  };
  if (kind == 0) { add({-1, 2}); add({-1, -2}); }
  else if (kind == 1) { add({-1, 2, 3}); add({-1, -2}); }
  else { add({-1, 2}); add({-2, 3}); add({-1, -3}); }
  ScopedHeuristics h(k);
  k.h->stable = true;
  heuristics_update_scores(k);
  kernel_assign_decision(k, internalize(5));
  kernel_assign_decision(k, internalize(6));
  size_t roots_from = k.trail.size();
  if (kind == 1) add({-3});
  add({1});
  heuristics_consume_root_assignments(k, roots_from);
  prepare(k);
  assert(k.inconsistent && !k.conflict_valid && !k.level);
  for (Lit lit : k.trail) assert(!heuristics_var_active(k, var_of(lit)));
  assert(heuristics_var_active(k, 4) && heuristics_var_active(k, 5));
  const auto id = k.next_clause_id;
  prepare(k);
  assert(k.next_clause_id == id);
  proof.flush();
  if (keep) {
    std::ofstream cnf(stem + ".cnf");
    cnf << "p cnf 8 " << inputs.size() << '\n';
    for (const auto &clause : inputs) {
      for (int lit : clause) cnf << lit << ' ';
      cnf << "0\n";
    }
  } else std::remove(path.c_str());
}

int main(int argc, char **argv) {
  retained_chain(true);
  retained_chain(false); // Public Reduce stays independent of decision policy.
  no_root_work();
  for (unsigned kind = 0; kind < 3; ++kind)
    for (ProofFormat format : {ProofFormat::DratAscii, ProofFormat::Lrat})
      refutation(kind, format, argc > 1 ? std::string(argv[1]) + "/reduce-root-" +
          std::to_string(kind) + (format == ProofFormat::Lrat ? "-lrat" : "-drat") : "");
  std::puts("PASS: Reduce root guidance, truncated suffix, policy neutrality, no-op and six proof cases");
}
