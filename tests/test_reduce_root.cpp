// Reduce at the root: preparation consumes newly derived roots before ranking
// again, and retained root reasons do not shrink the candidate population.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/reduce/reduce.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <unistd.h>
using namespace tactix;

using Cnf = std::vector<std::vector<int>>;

// A process-private DSR proof path.
static std::string proof_path(const char *name) {
  return "/tmp/tactix-" + std::string(name) + "-" + std::to_string(getpid()) +
         ".proof";
}

// Adds an input clause with the next id; a unit becomes a named root.
static void add_input(Kernel &k, const std::vector<int> &clause) {
  std::vector<Lit> lits;
  for (int lit : clause)
    lits.push_back(internalize(lit));
  const uint64_t id = ++k.next_clause_id;
  if (lits.size() == 1) {
    k.unit_ids[var_of(lits[0])] = id;
    kernel_assign_unit(k, lits[0]);
  } else if (lits.size() == 2)
    kernel_add_binary_clause(k, lits[0], lits[1], id);
  else
    kernel_add_large_clause(k, lits.data(), lits.size(), false, 0, id);
}

static const RuleVTable &rule() {
  const int id = tactix_rule_id("Reduce");
  assert(id >= 0);
  return tactix_rules()[id];
}

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

static void refutation(unsigned kind) {
  const std::string path = proof_path("reduce-root");
  Proof proof;
  assert(proof.open(path.c_str(), ProofFormat::DsrAscii));
  Kernel k;
  kernel_reserve_vars(k, 8);
  k.proof = &proof;
  proof.bind_variable_map(&k.internal_to_external);
  Cnf inputs;
  const auto add = [&](std::initializer_list<int> clause) {
    inputs.emplace_back(clause);
    add_input(k, inputs.back());
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
  std::remove(path.c_str());
}

static void pure_candidates(Kernel &k, std::vector<Ref> &out) {
  const auto arena = k.arena;
  const std::vector<Lit> trail(k.trail.begin(), k.trail.end());
  unsigned char stats[sizeof(Stats)];
  std::memcpy(stats, &k.stats, sizeof stats);
  const auto id = k.next_clause_id;
  reduce_candidates(k, out);
  uint64_t instances[8];
  assert(rule().instances(k, instances, 0) == 0);
  const auto n = rule().instances(k, instances, 8);
  assert(n == out.size() + 1);
  for (size_t count = 0; count <= out.size(); ++count)
    assert(std::find(instances, instances + n, count) != instances + n);
  assert(k.arena == arena && k.trail == trail && k.next_clause_id == id);
  assert(!std::memcmp(stats, &k.stats, sizeof stats));
}

static void root_reason(bool elevated, bool compact, bool unsat) {
  const std::string path = proof_path("root-lock");
  Proof proof;
  assert(proof.open(path.c_str(), ProofFormat::DsrAscii));
  Kernel k;
  kernel_reserve_vars(k, 8);
  ScopedHeuristics h(k);
  k.proof = &proof;
  proof.bind_variable_map(&k.internal_to_external);
  Cnf input = {{1,2,3,4}, {1,2,3,-4}, {-1}, {-2}};
  if (unsat) {
    input.push_back({-3,5,6}); input.push_back({-3,5,-6});
    input.push_back({-3,-5,6}); input.push_back({-3,-5,-6});
  } else input.push_back({-3,5,6});
  for (const auto &clause : input)
    add_input(k, clause);
  const Lit learned[] = {internalize(1),internalize(2),internalize(3)};
  const auto learned_id = ++k.next_clause_id;
  proof.add_rup(learned_id, learned, 3);
  const Ref ref = kernel_add_large_clause(k, learned, 3, true, 3, learned_id);
  clause_at(k.arena, ref)->set_used(3);
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(k.assigned[2].level == 0 && !k.assigned[2].binary && k.assigned[2].reason == ref);
  assert(k.value(internalize(3)) > 0 && !k.unflushed_root);
  if (elevated) {
    kernel_assign_decision(k, internalize(7));
    assert(propagate_apply(k) == ApplyResult::Ok);
  }
  reduce_prepare(k);
  assert(k.level == unsigned(elevated));
  std::vector<Ref> candidates;
  pure_candidates(k, candidates);
  assert(candidates == std::vector<Ref>{ref});
  assert(reduce_apply_refs(k, &ref, 1, compact) == ApplyResult::Ok);
  assert(k.stats.deleted == 1 && k.assigned[2].reason == UNIT_REASON);
  assert(k.value(internalize(3)) > 0 && k.assigned[2].level == 0);
  if (elevated) assert(k.value(internalize(7)) > 0);
  kernel_check_invariants(k);
  assert(default_solve(k) == (unsat ? 20 : 10));
  if (!unsat) {
    kernel_extend_model(k);
    for (const auto &clause : input) {
      bool satisfied = false;
      for (int lit : clause) satisfied |= k.model_values[internalize(lit)] > 0;
      assert(satisfied);
    }
  }
  proof.flush();
  std::remove(path.c_str());
}

static void protected_reasons(bool pending) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  ScopedHeuristics h(k);
  const Lit lits[] = {internalize(1),internalize(2),internalize(3)};
  const Ref ref = kernel_add_large_clause(k, lits, 3, true, 9);
  std::vector<Ref> candidates;
  pure_candidates(k, candidates);
  assert(candidates == std::vector<Ref>{ref});
  if (pending) {
    k.pending.valid = true;
    k.pending.ref = ref;
    k.pending.lits.assign(lits, lits + 3);
    k.pending.id = clause_at(k.arena, ref)->id();
  } else {
    kernel_assign_decision(k, internalize(-1));
    assert(propagate_apply(k) == ApplyResult::Ok);
    kernel_assign_decision(k, internalize(-2));
    assert(propagate_apply(k) == ApplyResult::Ok);
    assert(k.assigned[2].level == 2 && k.assigned[2].reason == ref);
  }
  pure_candidates(k, candidates);
  assert(candidates.empty());
  // Revalidate a payload captured before it became locked.
  assert(reduce_apply_refs(k, &ref, 1) == ApplyResult::NoOp);
  assert(!clause_at(k.arena, ref)->garbage() && !k.stats.deleted);
  assert(rule().apply_instance(k, 1) == ApplyResult::NoOp);
  k.pending.valid = false;
  assert(reduce_guard(k));
  // A conflict pins its antecedent, not the whole redundant database. Reduce
  // remains applicable and protects/forwards that antecedent during packing.
  k.conflict_valid = true; assert(reduce_guard(k)); k.conflict_valid = false;
  k.inconsistent = true; assert(!reduce_guard(k)); k.inconsistent = false;
  kernel_check_invariants(k);
}

int main() {
  retained_chain(true);
  retained_chain(false); // Public Reduce stays independent of decision policy.
  no_root_work();
  for (unsigned kind = 0; kind < 3; ++kind)
    refutation(kind);
  protected_reasons(false);
  protected_reasons(true);
  for (bool elevated : {false, true})
    for (bool compact : {false, true})
      for (bool unsat : {false, true})
        root_reason(elevated, compact, unsat);
  std::puts("test_reduce_root OK");
}
