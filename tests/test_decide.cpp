// Decide's guard carries no propagation-fixpoint conjunct, so a policy may
// stack decisions without propagating between them (N1), and backtracking
// from such a state must leave no clause unit with no watch firing. Ledger
// row D-LEVEL-1 in docs/dev/implementation-notes.md.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

// The guard holds on an un-propagated trail: off-fixpoint decisions are legal.
static void test_decide_guard_is_pure_off_fixpoint() {
  Kernel k;
  kernel_reserve_vars(k, 4); // var 4 stays free so Decide always has work
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // 1 -> 2
  kernel_add_binary_clause(k, internalize(-2), internalize(3)); // 2 -> 3
  kernel_assign_unit(k, internalize(1));     // root unit, un-propagated
  assert(k.propagate_head < k.trail.size()); // propagation pending
  assert(decide_guard(k));                   // still applicable
  // decide stacks a decision over the un-drained unit
  assert(decide_apply(k, internalize(4), true) == ApplyResult::Ok);
  assert(k.level == 1 && k.propagate_head < k.trail.size());
  assert(decide_guard(k)); // and again, still off a fixpoint
}

// The escaped-unit hazard: a watch scan treats its clause as satisfied by an
// ahead decision, which a later backtrack unassigns. `trail_tainted` forces
// the full re-propagation that restores the fixpoint invariants.
static void test_off_fixpoint_decide_then_backtrack_no_escape() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, /*redundant=*/false, /*glue=*/0);
  // watched on lits[0]=1 and lits[1]=2; lit 3 is unwatched.

  kernel_assign_unit(k, internalize(-3)); // c = lit 3 false at root
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(k.propagate_head == k.trail.size()); // clean root fixpoint

  // decide ¬1 (lit 1 false) at level 1 — in-fixpoint, so no taint yet
  assert(decide_apply(k, internalize(-1), true) == ApplyResult::Ok);
  assert(!k.trail_tainted);
  // decide 2 off a fixpoint: the ahead assignment, which taints the trail
  assert(decide_apply(k, internalize(2), true) == ApplyResult::Ok);
  assert(k.trail_tainted);

  // the clause looks satisfied, so lit 2 is never derived by it
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(k.propagate_head == k.trail.size());
  assert(k.value(internalize(2)) > 0); // true by decision (not by the clause)

  // now the clause is unit on 2 with a watch that will not re-fire
  kernel_backtrack(k, 1);
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_check_invariants(k); // aborts on an escaped unit — the actual guard
  assert(k.propagate_head == k.trail.size());
  assert(k.value(internalize(2)) > 0); // re-derived as a unit of the clause
}

// `N1-RETIRED-DECIDE`. Retirement takes a variable out of `H`'s ranked
// structures, but the guard still counts it and `apply_instance` still decides
// it, so the enumeration must keep offering it: the API surface may not shrink
// to DefaultPolicy's own action space. Adversarial on purpose — the retired
// variable is the LAST one left unassigned, which is the state the ranked
// pickers cannot answer from their own structures.
static void test_retired_unassigned_var_stays_selectable() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  ScopedHeuristics h(k);
  k.var_status.resize(k.nvars, VarStatus::Active);
  for (Var v = 0; v < k.nvars; v++) {
    k.var_status[v] = VarStatus::Substituted; // as V3/E1 retire one
    heuristics_deactivate_var(k, v);
  }
  assert(decide_guard(k) && "every variable is still unassigned");

  Lit cands[8];
  const uint32_t n = decide_candidates(k, cands, 8);
  assert(n == k.nvars && "a retired unassigned variable is still a candidate");

  // through the registry, i.e. exactly what `tactix_rule_instances` dispatches
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("Decide")];
  uint64_t insts[16];
  const uint32_t m = vt.instances(k, insts, 16);
  assert(m == 2 * k.nvars && "both phases of each are legal instances");
  for (uint32_t i = 0; i < m; i++)
    assert(vt.apply_instance != nullptr && insts[i] != INSTANCE_CANONICAL);

  // and the semantic canonical pick stays total once policy rankings are empty
  assert(decide_apply_canonical(k) == ApplyResult::Ok);
  assert(k.level == 1 && k.unassigned == k.nvars - 1);
}

static void test_canonical_decide_is_policy_independent() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  ScopedHeuristics h(k);
  // VMTF prefers the most recently enqueued variable (v2). The proof rule's
  // canonical instance is nevertheless its stable semantic first literal.
  assert(heuristics_next_decision_var(k) == 2);
  assert(decide_apply_canonical(k) == ApplyResult::Ok);
  assert(k.value(make_lit(0, false)) > 0);
  assert(k.value(make_lit(2, false)) == 0);
}

// DefaultPolicy mirrors Kissat's fixed-variable lifecycle after Propagate.
// The identity remains in Tactix's kernel for N1, but the policy consumer must
// remove each new true-level-zero implication from both ranking structures.
static void test_policy_consumes_root_implications() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  ScopedHeuristics h(k);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_assign_unit(k, internalize(1));
  const uint64_t roots_before = k.stats.root_assignments;
  assert(heuristics_var_active(k, var_of(internalize(2))));
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(k.assigned[var_of(internalize(2))].level == 0);
  assert(k.stats.root_assignments == roots_before + 1);
  heuristics_consume_root_assignments(k, 1);
  assert(!heuristics_var_active(k, var_of(internalize(2))));
  assert(!heuristics_heap_contains(k, var_of(internalize(2))));
}

// An identity fixed before `H` attaches (an input unit assigned during
// formula load) leaves both ranking structures at attach, exactly as
// kissat's flags.c:deactivate_variable removed it when the unit fixed it.
// Its enqueue still consumes an import-order stamp slot, so every later
// variable's stamp matches the reference; a retained entry instead shifts
// all following stamps at the next queue restamp and changes BVE's
// stamp-based relevance ranking.
static void test_attach_deactivates_prefixed_input_unit() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_assign_unit(k, internalize(3)); // input unit, assigned during load
  ScopedHeuristics h(k);
  const Var fixed = var_of(internalize(3));
  assert(!heuristics_var_active(k, fixed));
  assert(!heuristics_heap_contains(k, fixed));
  assert(heuristics_var_active(k, var_of(internalize(1))));
  assert(heuristics_var_active(k, var_of(internalize(4))));
  // The stamp slot of the fixed identity is consumed, not skipped.
  assert(k.h->links[var_of(internalize(4))].stamp == 4);
}

int main() {
  test_decide_guard_is_pure_off_fixpoint();
  test_off_fixpoint_decide_then_backtrack_no_escape();
  test_retired_unassigned_var_stays_selectable();
  test_canonical_decide_is_policy_independent();
  test_policy_consumes_root_implications();
  test_attach_deactivates_prefixed_input_unit();
  puts("test_decide OK");
  return 0;
}
