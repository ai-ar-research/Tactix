// V6 Congruence: two gates of the same kind over the same inputs have
// equivalent outputs; congruence_apply adds (¬a∨b),(a∨¬b). docs/calculus.md.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/registry.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>

using namespace tactix;

static bool has_binary(const Kernel &k, Lit a, Lit b) {
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[a].offset;
  for (uint32_t i = 0; i < k.watches.heads[a].size;) {
    const Watch w{ws[i]};
    if (w.binary()) {
      if (w.other() == b)
        return true;
      i += 1;
    } else {
      i += 2;
    }
  }
  return false;
}

// the two equivalence binaries between vars ea,eb (external DIMACS) are present
static bool equivalence_added(const Kernel &k, int ea, int eb) {
  return has_binary(k, internalize(-ea), internalize(eb)) &&
         has_binary(k, internalize(ea), internalize(-eb));
}

static std::vector<Lit> binary_partners(const Kernel &k, Lit lit) {
  std::vector<Lit> result;
  for (const WatchEntry watch : k.watches.entries(lit))
    if (watch.binary())
      result.push_back(watch.other());
  return result;
}

static void add3(Kernel &k, int a, int b, int c) {
  Lit lits[3] = {internalize(a), internalize(b), internalize(c)};
  kernel_add_large_clause(k, lits, 3, /*redundant=*/false, /*glue=*/0);
}

// out = AND(x1,x2): binaries (¬out∨x1),(¬out∨x2) + base (out∨¬x1∨¬x2)
static void and_gate(Kernel &k, int out, int x1, int x2) {
  kernel_add_binary_clause(k, internalize(-out), internalize(x1));
  kernel_add_binary_clause(k, internalize(-out), internalize(x2));
  add3(k, out, -x1, -x2);
}

// out = XOR(x1,x2): the 4 odd-parity clauses over {out,x1,x2}
static void xor_gate(Kernel &k, int out, int x1, int x2) {
  add3(k, -out, x1, x2);
  add3(k, out, -x1, x2);
  add3(k, out, x1, -x2);
  add3(k, -out, -x1, -x2);
}

// out = c ? t : e : the 4 defining ITE ternaries
static void ite_gate(Kernel &k, int out, int c, int t, int e) {
  add3(k, c, -e, out);
  add3(k, c, e, -out);
  add3(k, -c, -t, out);
  add3(k, -c, t, -out);
}

static void test_and_congruent() {
  Kernel k;
  kernel_reserve_vars(k, 4); // x1=1 x2=2 a=3 b=4
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2);
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(equivalence_added(k, 3, 4)); // a ≡ b
}

// XOR merging exercises the parity ladder.
static void test_xor_congruent() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  xor_gate(k, 3, 1, 2);
  xor_gate(k, 4, 1, 2);
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(equivalence_added(k, 3, 4));
}

static void test_ite_congruent() {
  Kernel k;
  kernel_reserve_vars(k, 5); // c=1 t=2 e=3 a=4 b=5
  ite_gate(k, 4, 1, 2, 3);
  ite_gate(k, 5, 1, 2, 3);
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(equivalence_added(k, 4, 5));
}

static void test_no_congruence() {
  Kernel k;
  kernel_reserve_vars(k, 6); // a=AND(1,2)=5, b=AND(3,4)=6
  and_gate(k, 5, 1, 2);
  and_gate(k, 6, 3, 4);
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::NoOp);
  assert(!equivalence_added(k, 5, 6));
}

// Mixed kinds compute different functions, so their outputs stay independent.
static void test_kind_isolation() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  and_gate(k, 4, 1, 2);
  xor_gate(k, 5, 1, 2);
  assert(congruence_guard(k));
  const ApplyResult r = congruence_apply(k);
  assert(r == ApplyResult::NoOp); // no same-kind pair
  assert(!equivalence_added(k, 4, 5));
}

// --- the binary-resolution unit channel (V6a) --------------------------------
// (l∨o) and (l∨¬o) resolve on o to {l}, no gate involved. docs/calculus.md.
static void test_binary_resolution_unit() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(1)) > 0 && "the unit is asserted at the root");
  assert(k.stats.congruence_y1_found == 1);
  assert(k.stats.congruence_y1_units == 1);
  assert(k.stats.congruence_units == 0 && "not the degenerate-gate channel");
  assert(k.stats.congruence_gates == 0 && "no gate was extracted");
}

static void test_binary_resolution_no_unit() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(3));
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::NoOp);
  assert(k.value(internalize(1)) == 0);
  assert(k.stats.congruence_y1_found == 0);
}

// Kissat imports paired binary equivalences into the congruence closure before
// simplifying gates. The merge is productive even though both binaries were
// already present: that return value admits preprocessing's eager Substitute
// follow-up.
static void test_binary_equivalence_channel() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  xor_gate(k, 3, 1, 2);
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(k.stats.congruence_binary_equivalences == 1);
  assert(k.stats.congruence_merges >= 1);
  assert(k.value(internalize(3)) < 0 &&
         "the imported equivalence must simplify the same-round XOR gate");
}

// Both polarities in one sweep refute via PendingUnits, not a false unit.
static void test_binary_resolution_refutes() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_add_binary_clause(k, internalize(-1), internalize(-3));
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::Unsat);
}

// --- the unit channel (D-CONG-UNIT) -----------------------------------------
// A gate degenerating under the current representatives is an inference: with
// x1 ≡ x2, out = x1 ⊕ x2 cancels to 0 and ¬out is a root unit.
static void test_xor_cancel_learns_unit() {
  Kernel k;
  kernel_reserve_vars(k, 5); // a=1 b=2 x1=3 x2=4 out=5
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2);
  xor_gate(k, 5, 3, 4); // out = x1 ⊕ x2, which collapses to 0 once x1 ≡ x2
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(equivalence_added(k, 3, 4));          // the enabling merge
  assert(k.value(internalize(5)) < 0);         // ¬out asserted at the root
  assert(k.stats.congruence_units >= 1);
}

// A late unit must revisit an already-consumed gate that mentions it, without
// reopening every unrelated gate.  AND gates are extracted before XOR gates:
// the dependent gate is therefore indexed first, then the XOR cancellation
// fixes its input and the value-occurrence schedule derives its output unit.
static void test_unit_reschedules_only_affected_gate() {
  Kernel k;
  kernel_reserve_vars(k, 7); // p q a b x c r
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2); // a == b
  and_gate(k, 6, 5, 7); // c = x & r, consumed before the XOR unit
  xor_gate(k, 5, 3, 4); // x = a xor b = 0
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(5)) < 0);
  assert(k.value(internalize(6)) < 0 &&
         "the affected earlier gate must be revisited after x becomes false");
}

// Opposite-sign units derived in one round must refute, not assign.
static void test_xor_cancel_refutes() {
  Kernel k;
  kernel_reserve_vars(k, 7); // p=1 q=2 a=3 b=4 out=5 c=6 d=7
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2); // 3 ≡ 4
  and_gate(k, 6, 1, 2);
  and_gate(k, 7, 1, 2); // 6 ≡ 7 (and ≡ 3, transitively)
  xor_gate(k, 5, 3, 4); // 3⊕4⊕5 = 0: cancels to the unit ¬5
  // 6⊕7⊕5 = 1: the gate on {6,7} outputs ¬5 and cancels to the unit 5.
  add3(k, 6, 7, 5);
  add3(k, -6, -7, 5);
  add3(k, -6, 7, -5);
  add3(k, 6, -7, -5);
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::Unsat);
  assert(k.inconsistent);
}

// --- the AND degeneracies and the contradictory merge -----------------------

static void add4(Kernel &k, int a, int b, int c, int d) {
  Lit lits[4] = {internalize(a), internalize(b), internalize(c),
                 internalize(d)};
  kernel_add_large_clause(k, lits, 4, /*redundant=*/false, /*glue=*/0);
}

// c = AND(a,¬b,r) where the closure first proves a ≡ b: the canonical inputs
// clash (x with ¬x), the gate's value is constant 0, and ¬c is a root unit.
static void test_and_clash_learns_unit() {
  Kernel k;
  kernel_reserve_vars(k, 6); // p=1 q=2 a=3 b=4 r=5 c=6
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2);
  // c = AND(a, ¬b, r): binaries (¬c∨a),(¬c∨¬b),(¬c∨r) + base (c∨¬a∨b∨¬r)
  kernel_add_binary_clause(k, internalize(-6), internalize(3));
  kernel_add_binary_clause(k, internalize(-6), internalize(-4));
  kernel_add_binary_clause(k, internalize(-6), internalize(5));
  add4(k, 6, -3, 4, -5);
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(equivalence_added(k, 3, 4));  // the enabling merge
  assert(k.value(internalize(6)) < 0); // ¬c asserted at the root
  assert(k.stats.congruence_units >= 1);
}

// c = AND(a,b) with a ≡ b: the key deduplicates to AND(x), so c merges with
// the representative with no second gate.
static void test_and_idempotent_unary_merge() {
  Kernel k;
  kernel_reserve_vars(k, 5); // p=1 q=2 a=3 b=4 c=5
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2);
  and_gate(k, 5, 3, 4); // collapses to AND(rep) once 3 ≡ 4
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(equivalence_added(k, 3, 4)); // the enabling merge
  assert(equivalence_added(k, 5, 3)); // the unary merge c ≡ a
}

// a = AND(p,q) and ¬b = AND(p,q) with a ≡ b: b joins ¬b's class, and that
// contradictory merge means UNSAT.
static void test_contradictory_merge_refutes() {
  Kernel k;
  kernel_reserve_vars(k, 4); // p=1 q=2 a=3 b=4
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2);
  // ¬b = AND(p,q): binaries (b∨p),(b∨q) + base (¬b∨¬p∨¬q)
  kernel_add_binary_clause(k, internalize(4), internalize(1));
  kernel_add_binary_clause(k, internalize(4), internalize(2));
  add3(k, -4, -1, -2);
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::Unsat);
  assert(k.inconsistent);
}

// --- the XOR output-literal sign convention ---------------------------------
// A gate's `output` must evaluate to f(inputs): invisible to the closure, but
// the degenerate-case unit/merge channel reads the polarity directly. For
// ⊕vⱼ = R the gate on the other s−1 variables outputs vᵢ iff R = 0.

// The XOR gate over `inputs` has `expected_out` as its output (both DIMACS).
static void assert_xor_gate_output(const Kernel &k, std::vector<int> inputs,
                                   int expected_out) {
  std::vector<CongGate> gates;
  congruence_extract_gates(k, gates, /*with_xor_ite=*/true);
  std::vector<Lit> want;
  for (const int e : inputs)
    want.push_back(internalize(e));
  std::sort(want.begin(), want.end());
  bool found = false;
  for (const CongGate &g : gates) {
    if (g.kind != GateKind::Xor || g.inputs != want)
      continue;
    found = true;
    assert(g.output == internalize(expected_out) &&
           "XOR gate output literal has the wrong sign");
  }
  assert(found && "expected XOR gate was not extracted");
}

// R = 0, s = 3: x1⊕x2⊕x3 = 0, so the gate on {x1,x2} outputs +x3.
static void test_xor_output_sign_even_arity() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  xor_gate(k, 3, 1, 2); // the 4 odd-negation clauses ⇒ v1⊕v2⊕v3 = 0
  assert_xor_gate_output(k, {1, 2}, 3);
  assert_xor_gate_output(k, {2, 3}, 1);
}

// R = 1, s = 3: x1⊕x2⊕x3 = 1, so the gate on {x1,x2} outputs ¬x3.
static void test_xor_output_sign_odd_relation() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add3(k, 1, 2, 3);
  add3(k, -1, -2, 3);
  add3(k, -1, 2, -3);
  add3(k, 1, -2, -3);
  assert_xor_gate_output(k, {1, 2}, -3);
  assert_xor_gate_output(k, {2, 3}, -1);
}

// R = 0, s = 4: v1⊕v2⊕v3⊕v4 = 0 ⇒ the gate on {1,2,3} outputs +4.
static void test_xor_output_sign_odd_arity() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const int v[4] = {1, 2, 3, 4};
  for (int m = 0; m < 16; m++) { // every odd-negation clause over the 4 vars
    int neg = 0;
    for (int i = 0; i < 4; i++)
      neg += (m >> i) & 1;
    if (!(neg & 1))
      continue;
    Lit ls[4];
    for (int i = 0; i < 4; i++)
      ls[i] = internalize(((m >> i) & 1) ? -v[i] : v[i]);
    kernel_add_large_clause(k, ls, 4, /*redundant=*/false, /*glue=*/0);
  }
  assert_xor_gate_output(k, {1, 2, 3}, 4);
  assert_xor_gate_output(k, {2, 3, 4}, 1);
}

// Congruence emits extracted binaries in arena order for the proof, then
// Kissat sorts only that newly extracted segment before sparse reconnection.
// Give q an earlier compact/import rank while placing p's ternary first: the
// old prefix stays fixed and the new suffix must be q,p rather than p,q.
static void test_extracted_binary_suffix_is_sorted() {
  Kernel k;
  kernel_reserve_vars(k, 5); // a=1 b=2 c=3 p=4 q=5
  for (const Var v : {0u, 1u, 2u, 4u, 3u})
    kernel_record_import(k, v);
  const Lit a = internalize(1), b = internalize(2), c = internalize(3);
  const Lit p = internalize(4), q = internalize(5);
  kernel_add_binary_clause(k, negate(a), c, ++k.next_clause_id);
  kernel_add_binary_clause(k, negate(b), c, ++k.next_clause_id);
  Lit first[3] = {a, c, p};
  Lit second[3] = {b, c, q};
  kernel_add_large_clause(k, first, 3, false, 0, ++k.next_clause_id);
  kernel_add_large_clause(k, second, 3, false, 0, ++k.next_clause_id);

  (void)congruence_apply(k);
  const std::vector<Lit> partners = binary_partners(k, c);
  assert(partners.size() >= 4);
  assert(partners[partners.size() - 2] == q);
  assert(partners[partners.size() - 1] == p);
}

// Input binaries need not precede large watches in a sparse list.  The
// candidate pass snapshots live binary membership before scanning ternaries,
// rather than repeatedly walking a high-occurrence mixed list.  Keep the
// adversarial ordering explicit here: both antecedent binaries are appended
// only after the ternaries installed large watches on their pivot literals.
static void test_binary_extraction_from_mixed_watch_lists() {
  Kernel k;
  kernel_reserve_vars(k, 7);
  const Lit a = internalize(1), b = internalize(2), c = internalize(3);
  const Lit x = internalize(4), y = internalize(5);
  Lit pad_a[3] = {negate(a), internalize(6), internalize(7)};
  Lit pad_c[3] = {negate(c), internalize(-6), internalize(-7)};
  Lit first[3] = {a, b, x};
  Lit second[3] = {c, b, y};
  kernel_add_large_clause(k, pad_a, 3, false, 0, ++k.next_clause_id);
  kernel_add_large_clause(k, pad_c, 3, false, 0, ++k.next_clause_id);
  kernel_add_large_clause(k, first, 3, false, 0, ++k.next_clause_id);
  kernel_add_large_clause(k, second, 3, false, 0, ++k.next_clause_id);
  kernel_add_binary_clause(k, negate(a), b, ++k.next_clause_id);
  kernel_add_binary_clause(k, negate(c), b, ++k.next_clause_id);
  assert(!k.watches.heads[negate(a)].binaries_first);
  assert(!k.watches.heads[negate(c)].binaries_first);

  (void)congruence_apply(k);
  assert(kernel_binary_live(k, b, x));
  assert(kernel_binary_live(k, b, y));
}

// The mid-round budget: `work_budget` 0 leaves the rule unbounded, and a small
// budget cuts the round — counted once in `congruence_budget_stops` — with no
// unjustified inference. `stop_fuzz` keeps the strided termination draw from
// firing, so only the budget path is pinned.
static void test_budget_stops_round() {
  const auto build = [](Kernel &k) {
    kernel_reserve_vars(k, 6);
    and_gate(k, 3, 1, 2);
    and_gate(k, 4, 1, 2);
    and_gate(k, 5, 1, 2);
    and_gate(k, 6, 1, 2);
  };
  Kernel full;
  build(full);
  assert(full.work_budget == 0 && "unbounded is the default for every caller");
  assert(congruence_apply(full) == ApplyResult::Ok);
  assert(full.stats.congruence_budget_stops == 0);
  assert(full.stats.congruence_ticks > 0 && "the meter must be charged");
  const uint64_t full_gates = full.stats.congruence_gates;

  Kernel cut;
  build(cut);
  cut.stop_fuzz = 1000000000u; // check every step; never terminate
  cut.work_budget = 1;
  congruence_apply(cut);
  assert(cut.stats.congruence_budget_stops == 1 && "the cut is counted once");
  assert(cut.stats.rule_interrupted == 0 && "a budget cut is not a terminate");
  assert(cut.stats.congruence_gates < full_gates && "the round did less work");
  assert(cut.stats.congruence_unjustified == 0);
  assert(cut.work_budget == 1 && "the rule must not clobber the handoff");

  // And the budget is round-scoped: a later unbudgeted apply is unaffected.
  Kernel again;
  build(again);
  assert(congruence_apply(again) == ApplyResult::Ok);
  assert(again.stats.congruence_budget_stops == 0);
}

// `N1-CONG-INSTANCES` (disposition: docs/calculus.md §6 V6). V6's effect is the
// closure, so a single merge is a prefix of one application rather than an
// application, and the canonical round is the whole legal instance space. The
// finer choice — which discovered equivalence to collapse into the formula —
// belongs to V3, which offers one instance per representative over exactly the
// binaries V6 adds. This test holds that division in place.
static void test_round_is_the_instance_and_substitute_owns_the_choice() {
  Kernel k;
  kernel_reserve_vars(k, 6); // x1 x2, two AND outputs, and a second gate pair
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2);
  and_gate(k, 5, 1, 2);
  const RuleVTable &cong = tactix_rules()[tactix_rule_id("Congruence")];
  uint64_t insts[8];
  assert(cong.instances(k, insts, 8) == 1 && insts[0] == INSTANCE_CANONICAL &&
         "V6's instance is the closure; see the disposition above");
  assert(cong.apply_instance(k, INSTANCE_CANONICAL) == ApplyResult::Ok);
  assert(equivalence_added(k, 3, 4) && equivalence_added(k, 3, 5));

  // the gate clauses are untouched: the collapse is V3's to schedule
  assert(has_binary(k, internalize(-3), internalize(1)));

  const RuleVTable &subst = tactix_rules()[tactix_rule_id("Substitute")];
  const uint32_t n = subst.instances(k, insts, 8);
  assert(n >= 1 && "the equivalence class V6 discovered is addressable in V3");
  assert(subst.apply_instance(k, insts[0]) == ApplyResult::Ok);
}

int main() {
  test_budget_stops_round();
  test_and_congruent();
  test_xor_congruent();
  test_ite_congruent();
  test_no_congruence();
  test_kind_isolation();
  test_binary_resolution_unit();
  test_binary_resolution_no_unit();
  test_binary_equivalence_channel();
  test_binary_resolution_refutes();
  test_xor_cancel_learns_unit();
  test_unit_reschedules_only_affected_gate();
  test_xor_cancel_refutes();
  test_and_clash_learns_unit();
  test_and_idempotent_unary_merge();
  test_contradictory_merge_refutes();
  test_xor_output_sign_even_arity();
  test_xor_output_sign_odd_relation();
  test_xor_output_sign_odd_arity();
  test_extracted_binary_suffix_is_sorted();
  test_binary_extraction_from_mixed_watch_lists();
  test_round_is_the_instance_and_substitute_owns_the_choice();
  std::printf("test_congruence: all passed\n");
  return 0;
}
