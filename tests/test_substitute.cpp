// V3 Substitute: SCC detection over the binary implication graph, the round,
// the instance surface, and model reconstruction through ρ. A binary (¬p ∨ q)
// is the implication p → q. See docs/calculus.md §V3.
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
  assert(r.repr[internalize(2)] == r.repr[internalize(1)]);
  assert(r.repr[internalize(1)] == internalize(1));           // rep = min = +1
  assert(r.repr[internalize(-2)] == r.repr[internalize(-1)]);
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
  assert(r.repr[internalize(3)] == internalize(3));
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
  assert(r.repr[r.contradiction_lit] == r.repr[negate(r.contradiction_lit)]);
}

// The canonical round refutes x ≡ ¬x: the rule's Unsat contract.
static void test_apply_unsat() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(-1));
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  assert(substitute_guard(k));
  assert(substitute_apply(k) == ApplyResult::Unsat);
  assert(k.inconsistent);
  assert(k.stats.substitute_rounds == 1);
}

static bool lit_in_any_clause(const Kernel &k, Lit l) {
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[l].offset;
  for (uint32_t i = 0; i < k.watches.heads[l].size;) {
    if (Watch{ws[i]}.binary())
      return true;
    i += 2; // large watch: blocking word + ref word
  }
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

// A consistent x1 ≡ x2 is substituted: the non-rep x2 vanishes everywhere.
static void test_apply_substitutes() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  const Lit c[3] = {internalize(2), internalize(3), internalize(4)};
  kernel_add_large_clause(k, c, 3, /*redundant=*/false, /*glue=*/0);

  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(!k.inconsistent);
  assert(!lit_in_any_clause(k, internalize(2))); // x2 gone, both polarities
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
  SccResult r2 = substitute_find_sccs(k); // re-detection: the SCC is gone
  assert(r2.nontrivial_reps.empty());
  // a witness pair was pushed for the eliminated variable
  assert(!k.reconstruction.empty());
}

// Model reconstruction through ρ: a model of the reduced formula with x2 at the
// wrong value must extend to satisfy every original clause.
static void test_model_reconstruction() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  const Lit c[2] = {internalize(2), internalize(3)}; // (x2 ∨ x3), a binary
  kernel_add_binary_clause(k, c[0], c[1]);
  // original clauses (DIMACS), captured before substitution
  const int orig[3][2] = {{-1, 2}, {-2, 1}, {2, 3}};

  assert(substitute_apply(k) == ApplyResult::Ok);

  // reduced formula (x1 ∨ x3): model x1=F, x3=T, with x2=T against its rep
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

// A binary collapsing to a unit: (x2 ∨ x1) under x1 ≡ x2 asserts the unit x1.
static void test_size_collapse_unit() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  kernel_add_binary_clause(k, internalize(2), internalize(1));  // (x2 ∨ x1)
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(1)) > 0 && "x1 forced true by the collapsed unit");
  assert(k.propagate_head < k.trail.size() && "unit asserted un-propagated");
}

// N1: one instance substitutes just that class; a junk payload is a total NoOp.
static void test_apply_instance_one_class() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  kernel_add_binary_clause(k, internalize(-3), internalize(4)); // x3 -> x4
  kernel_add_binary_clause(k, internalize(-4), internalize(3)); // x4 -> x3
  const int rid = tactix_rule_id("Substitute");
  const RuleVTable &vt = tactix_rules()[rid];
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

// N1: one instance still surfaces the global x ≡ ¬x refutation.
static void test_instance_surfaces_unsat() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(-1));
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  const int rid = tactix_rule_id("Substitute");
  const RuleVTable &vt = tactix_rules()[rid];
  uint64_t insts[8];
  const uint32_t n = vt.instances(k, insts, 8);
  assert(n >= 1);
  assert(vt.apply_instance(k, insts[0]) == ApplyResult::Unsat);
  assert(k.inconsistent);
}

// A round deriving two contradictory root units must refute, not drop one: here
// {1} comes from the rewritten large clause and {¬1} from the binary (¬1∨¬2),
// and neither fires by propagation alone.
static void test_contradictory_units_refute() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1)); // 2 -> 1  (1≡2)
  const Lit c[3] = {internalize(1), internalize(2), internalize(-3)};
  kernel_add_large_clause(k, c, 3, /*redundant=*/false, /*glue=*/0);
  kernel_add_binary_clause(k, internalize(-1), internalize(-2)); // (¬1 ∨ ¬2)
  kernel_assign_unit(k, internalize(3)); // 3 true → ¬3 root-false
  assert(propagate_apply(k) == ApplyResult::Ok); // no direct conflict
  assert(substitute_apply(k) == ApplyResult::Unsat);
  assert(k.inconsistent);
}

// Guard: pure quiescent-root precondition, no fixpoint (ledger row D-LEVEL).
static void test_guard() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  assert(substitute_guard(k));           // empty trail = trivial fixpoint
  kernel_assign_unit(k, internalize(1)); // un-propagated root unit
  assert(substitute_guard(k));
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(substitute_guard(k));
  kernel_assign_decision(k, internalize(2));
  assert(!substitute_guard(k)); // level > 0 is still not a root state
}

int main() {
  test_equivalence_scc();
  test_no_scc();
  test_contradiction_scc();
  test_apply_unsat();
  test_contradictory_units_refute();
  test_apply_substitutes();
  test_model_reconstruction();
  test_size_collapse_unit();
  test_apply_instance_one_class();
  test_instance_surfaces_unsat();
  test_guard();
  puts("test_substitute OK");
  return 0;
}
