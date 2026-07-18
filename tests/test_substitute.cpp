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

static bool lit_in_any_clause(const Kernel &k, Lit l) {
  // binary occurrences: any binary watch on l
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[l].offset;
  for (uint32_t i = 0; i < k.watches.heads[l].size;) {
    if (Watch{ws[i]}.binary())
      return true;
    i += 2; // large watch: blocking word + ref word
  }
  // large occurrences: any non-garbage arena clause containing l
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage())
      for (const Lit m : *c)
        if (m == l)
          return true;
    ref += clause_words(c->size);
  }
  return false;
}

// V3b-3: a consistent equivalence x1 ≡ x2 is SUBSTITUTED — the non-rep x2
// (rep = min = +1) is rewritten to x1 everywhere, so the round returns Ok, x2
// vanishes from all clauses, and the SCC is gone on re-detection.
static void test_apply_substitutes() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // x1 -> x2
  kernel_add_binary_clause(k, internalize(-2), internalize(1)); // x2 -> x1
  const Lit c[3] = {internalize(2), internalize(3), internalize(4)}; // (x2∨x3∨x4)
  kernel_add_large_clause(k, c, 3, /*redundant=*/false, /*glue=*/0);

  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(!k.inconsistent);
  // x2 (both polarities) is fully eliminated
  assert(!lit_in_any_clause(k, internalize(2)));
  assert(!lit_in_any_clause(k, internalize(-2)));
  // the large clause was rewritten x2 -> x1: (x1∨x3∨x4) is present
  bool found = false;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *cc = clause_at(k.arena, ref);
    if (!cc->garbage() && cc->size == 3) {
      bool h1 = false, h3 = false, h4 = false;
      for (const Lit m : *cc) {
        h1 |= (m == internalize(1));
        h3 |= (m == internalize(3));
        h4 |= (m == internalize(4));
      }
      found |= (h1 && h3 && h4);
    }
    ref += clause_words(cc->size);
  }
  assert(found && "large clause rewritten to (x1 v x3 v x4)");
  // re-detection: the SCC is gone
  SccResult r2 = substitute_find_sccs(k);
  assert(r2.nontrivial_reps.empty());
  // a witness pair was pushed for the eliminated variable
  assert(!k.reconstruction.empty());
}

// V3b-3 (highest-risk): model reconstruction through ρ. Substitute x1 ≡ x2,
// hand a model of the REDUCED formula (with x2 left at the "wrong" value), run
// kernel_extend_model, and assert every ORIGINAL clause is satisfied.
static void test_model_reconstruction() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // x1 -> x2
  kernel_add_binary_clause(k, internalize(-2), internalize(1)); // x2 -> x1
  const Lit c[2] = {internalize(2), internalize(3)}; // (x2 ∨ x3), a binary
  kernel_add_binary_clause(k, c[0], c[1]);
  // original clauses (DIMACS), captured before substitution
  const int orig[3][2] = {{-1, 2}, {-2, 1}, {2, 3}};

  assert(substitute_apply(k) == ApplyResult::Ok);

  // reduced formula is (x1 ∨ x3); pick the model x1=F, x3=T, and leave x2=T
  // (inconsistent with x1 — extend must override it to x1's value).
  auto set = [&](int dimacs, bool val) {
    const Lit l = internalize(dimacs);
    k.values[l] = val ? 1 : -1;
    k.values[negate(l)] = val ? -1 : 1;
  };
  set(1, false);
  set(3, true);
  set(2, true); // the deliberately wrong pre-extend value

  kernel_extend_model(k);
  assert(k.values[internalize(2)] < 0 && "x2 reconstructed to x1's value (F)");
  for (const auto &cl : orig) {
    bool sat = false;
    for (const int e : cl)
      sat |= (k.values[internalize(e)] > 0);
    assert(sat && "every original clause satisfied by the extended model");
  }
}

// V3b-3: a binary that collapses to a unit under substitution. x1 ≡ x2, plus
// the binary (x2 ∨ x1) — after x2 -> x1 it becomes (x1 ∨ x1) = the unit x1,
// asserted un-propagated (so the guard no longer holds afterwards).
static void test_size_collapse_unit() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // x1 -> x2
  kernel_add_binary_clause(k, internalize(-2), internalize(1)); // x2 -> x1
  kernel_add_binary_clause(k, internalize(2), internalize(1));  // (x2 ∨ x1)
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(1)) > 0 && "x1 forced true by the collapsed unit");
  assert(k.propagate_head < k.trail.size() && "unit asserted un-propagated");
}

// V3b-3 N1: a single instance substitutes exactly the requested class and its
// mirror; an out-of-range payload is a total NoOp.
static void test_apply_instance_one_class() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // x1 -> x2
  kernel_add_binary_clause(k, internalize(-2), internalize(1)); // x2 -> x1
  kernel_add_binary_clause(k, internalize(-3), internalize(4)); // x3 -> x4
  kernel_add_binary_clause(k, internalize(-4), internalize(3)); // x4 -> x3
  const int rid = tactix_rule_id("Substitute");
  const RuleVTable &vt = tactix_rules()[rid];
  // out-of-range payload -> total NoOp (N1)
  assert(vt.apply_instance(k, lit_instance(internalize(99))) ==
         ApplyResult::NoOp);
  // substitute just the x1≡x2 class (rep = +1); x2 goes, x3/x4 stay
  assert(vt.apply_instance(k, lit_instance(internalize(1))) == ApplyResult::Ok);
  assert(!lit_in_any_clause(k, internalize(2)));
  assert(lit_in_any_clause(k, internalize(3)) ||
         lit_in_any_clause(k, internalize(4))); // x3≡x4 untouched
  SccResult r = substitute_find_sccs(k);
  assert(r.nontrivial_reps.size() == 2); // only {+3,+4} and {-3,-4} remain
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
  test_apply_substitutes();
  test_model_reconstruction();
  test_size_collapse_unit();
  test_apply_instance_one_class();
  test_instance_surfaces_unsat();
  test_guard();
  puts("test_substitute OK");
  return 0;
}
