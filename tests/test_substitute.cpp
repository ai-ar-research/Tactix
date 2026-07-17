// V3a: SCC detection over the binary implication graph (guard + Tarjan).
// A binary clause (¬p ∨ q) is the implication p → q (and, by contrapositive,
// ¬q → ¬p), so an equivalence cycle among literals shows up as a non-trivial
// SCC together with its complement SCC.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/substitute/substitute.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

// x1 ≡ x2 via x1 -> x2 -> x1: SCC {+1,+2} plus its complement {-1,-2}
static void test_equivalence_scc() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // x1 -> x2
  kernel_add_binary_clause(k, internalize(-2), internalize(1)); // x2 -> x1
  SccResult r = substitute_find_sccs(k);
  assert(!r.contradiction);
  assert(r.nontrivial_reps.size() == 2); // {+1,+2} and {-1,-2}
  assert(r.repr[internalize(2)] == r.repr[internalize(1)]);   // x2 ~ x1
  assert(r.repr[internalize(1)] == internalize(1));           // rep = min = +1
  assert(r.repr[internalize(-2)] == r.repr[internalize(-1)]); // -x2 ~ -x1
}

// a one-way implication x1 -> x2 (no return edge) forms no non-trivial SCC
static void test_no_scc() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // x1 -> x2 only
  SccResult r = substitute_find_sccs(k);
  assert(!r.contradiction);
  assert(r.nontrivial_reps.empty());
  assert(r.repr[internalize(1)] == internalize(1)); // singleton = own rep
  assert(r.repr[internalize(3)] == internalize(3)); // isolated var
}

// x ≡ ¬x: x, a, and ¬x all collapse into one SCC -> UNSAT contradiction
static void test_contradiction_scc() {
  Kernel k;
  kernel_reserve_vars(k, 2); // x = var 1, a = var 2
  kernel_add_binary_clause(k, internalize(-1), internalize(2));  // x  -> a
  kernel_add_binary_clause(k, internalize(-2), internalize(-1)); // a  -> ¬x
  kernel_add_binary_clause(k, internalize(1), internalize(2));   // ¬x -> a
  kernel_add_binary_clause(k, internalize(-2), internalize(1));  // a  -> x
  SccResult r = substitute_find_sccs(k);
  assert(r.contradiction);
  assert(r.contradiction_lit != INVALID_LIT);
  // the witness x and its negation share the SCC representative
  assert(r.repr[r.contradiction_lit] == r.repr[negate(r.contradiction_lit)]);
}

// V3b-1: the canonical round REFUTES an x ≡ ¬x formula (empty clause emitted,
// k.inconsistent set) — the rule's Unsat contract.
static void test_apply_unsat() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));  // x  -> a
  kernel_add_binary_clause(k, internalize(-2), internalize(-1)); // a  -> ¬x
  kernel_add_binary_clause(k, internalize(1), internalize(2));   // ¬x -> a
  kernel_add_binary_clause(k, internalize(-2), internalize(1));  // a  -> x
  assert(substitute_guard(k));
  assert(substitute_apply(k) == ApplyResult::Unsat);
  assert(k.inconsistent);
  assert(k.stats.substitute_rounds == 1);
}

// V3b-1: a consistent equivalence (x ≡ a, no complement collapse) is a NoOp —
// no rewrite until V3b-3 — but it IS enumerated as an instance.
static void test_apply_noop_and_instances() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // x -> a
  kernel_add_binary_clause(k, internalize(-2), internalize(1)); // a -> x
  assert(substitute_apply(k) == ApplyResult::NoOp);
  assert(!k.inconsistent);
  // two non-trivial SCCs ({+x,+a} and {-x,-a}) -> two instances, canonical form
  uint64_t insts[8];
  const int rid = tactix_rule_id("Substitute");
  const RuleVTable &vt = tactix_rules()[rid];
  const uint32_t n = vt.instances(k, insts, 8);
  assert(n == 2);
  Lit r0, r1;
  assert(lit_of_instance(insts[0], &r0) && lit_of_instance(insts[1], &r1));
  assert(insts[0] != INSTANCE_CANONICAL && insts[1] != INSTANCE_CANONICAL);
  // an out-of-range / non-representative payload is a total NoOp (N1)
  assert(vt.apply_instance(k, lit_instance(internalize(99))) ==
         ApplyResult::NoOp);
  // the canonical sentinel delegates to the round
  assert(vt.apply_instance(k, INSTANCE_CANONICAL) == ApplyResult::NoOp);
}

// V3b-1 N1: a single instance still surfaces a GLOBAL x ≡ ¬x refutation,
// whichever SCC representative was requested.
static void test_instance_surfaces_unsat() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));  // x  -> a
  kernel_add_binary_clause(k, internalize(-2), internalize(-1)); // a  -> ¬x
  kernel_add_binary_clause(k, internalize(1), internalize(2));   // ¬x -> a
  kernel_add_binary_clause(k, internalize(-2), internalize(1));  // a  -> x
  const int rid = tactix_rule_id("Substitute");
  const RuleVTable &vt = tactix_rules()[rid];
  uint64_t insts[8];
  const uint32_t n = vt.instances(k, insts, 8);
  assert(n >= 1);
  assert(vt.apply_instance(k, insts[0]) == ApplyResult::Unsat);
  assert(k.inconsistent);
}

// guard: only a quiescent root fixpoint is applicable
static void test_guard() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  assert(substitute_guard(k));           // empty trail = trivial fixpoint
  kernel_assign_unit(k, internalize(1)); // un-propagated unit
  assert(!substitute_guard(k));          // pending propagation
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(substitute_guard(k)); // back at fixpoint
}

int main() {
  test_equivalence_scc();
  test_no_scc();
  test_contradiction_scc();
  test_apply_unsat();
  test_apply_noop_and_instances();
  test_instance_surfaces_unsat();
  test_guard();
  puts("test_substitute OK");
  return 0;
}
