// V6 Congruence — gate congruence closure. Two gates of the same kind over the
// same inputs have equivalent outputs; congruence_apply adds the equivalence
// binaries (¬a∨b),(a∨¬b). These tests build one gate pair per kind (AND/XOR/ITE)
// and assert the closure fires; plus a no-op when inputs differ.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/congruence/congruence.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>

using namespace tactix;

// definitive live-binary test (watch scan)
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

// Two AND gates over the same inputs → outputs merged.
static void test_and_congruent() {
  Kernel k;
  kernel_reserve_vars(k, 4); // x1=1 x2=2 a=3 b=4
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2);
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(equivalence_added(k, 3, 4)); // a ≡ b
}

// Two XOR gates over the same inputs → outputs merged (exercises the ladder).
static void test_xor_congruent() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  xor_gate(k, 3, 1, 2);
  xor_gate(k, 4, 1, 2);
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(equivalence_added(k, 3, 4));
}

// Two ITE gates over the same (cond,then,else) → outputs merged.
static void test_ite_congruent() {
  Kernel k;
  kernel_reserve_vars(k, 5); // c=1 t=2 e=3 a=4 b=5
  ite_gate(k, 4, 1, 2, 3);
  ite_gate(k, 5, 1, 2, 3);
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(equivalence_added(k, 4, 5));
}

// Different inputs → not congruent → NoOp, no equivalence added.
static void test_no_congruence() {
  Kernel k;
  kernel_reserve_vars(k, 6); // a=AND(1,2)=5, b=AND(3,4)=6
  and_gate(k, 5, 1, 2);
  and_gate(k, 6, 3, 4);
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::NoOp);
  assert(!equivalence_added(k, 5, 6));
}

// Mixed kinds over the same inputs must NOT collide: AND(1,2)=4 and XOR(1,2)=5
// compute different functions, so their outputs must stay independent.
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

// --- the unit channel (D-CONG-UNIT) -----------------------------------------
// A gate that degenerates under the current representatives is an INFERENCE, not
// a dead end. Here the closure must first prove x1 ≡ x2 (two AND gates over the
// same inputs), which makes the XOR gate out = x1 ⊕ x2 cancel completely, so its
// output is the constant 0 and ¬out is a root unit. This is the miter shape in
// miniature: it is why congruence alone refutes an equivalence-checking miter.
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

// Two XOR relations with OPPOSITE parity constants collapse onto the same
// output variable: {3,4,5} with R=0 cancels to ¬5 while {6,7,5} with R=1
// cancels to 5. The two derived units are an opposite-sign pair in one round,
// which PendingUnits resolves to the empty clause — the round must REFUTE
// rather than assign. (An earlier form asserted `out` on the trail instead,
// but a root-assigned output suppresses gate extraction entirely, so that
// construction can no longer reach the unit channel.)
static void test_xor_cancel_refutes() {
  Kernel k;
  kernel_reserve_vars(k, 7); // p=1 q=2 a=3 b=4 out=5 c=6 d=7
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2); // 3 ≡ 4
  and_gate(k, 6, 1, 2);
  and_gate(k, 7, 1, 2); // 6 ≡ 7 (and ≡ 3, transitively)
  xor_gate(k, 5, 3, 4); // 3⊕4⊕5 = 0: cancels to the unit ¬5
  // 6⊕7⊕5 = 1 (the even-negation clause set): the gate on {6,7} outputs ¬5,
  // and once 6 ≡ 7 it cancels to the unit 5 — the opposing pair refutes.
  add3(k, 6, 7, 5);
  add3(k, -6, -7, 5);
  add3(k, -6, 7, -5);
  add3(k, 6, -7, -5);
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::Unsat);
  assert(k.inconsistent);
}

// --- the AND degeneracies + contradictory merge (C3b) -----------------------

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

// c = AND(a,b) where a ≡ b: the key deduplicates to the unary gate AND(x),
// so c merges with the representative — no second gate needed.
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

// a = AND(p,q) and ¬b = AND(p,q) while a ≡ b: the closure derives b ≡ ¬b's
// class — a contradictory merge, and the formula is UNSAT (C3b refutation).
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

// --- the XOR output-literal sign convention (PROGRESS C0) -------------------
// A gate's `output` literal must EVALUATE to f(inputs). For XOR that sign is
// invisible to the closure (it only compares two gates' outputs, and a shared
// mis-signing cancels), so it needs its own test — a unit/merge channel built on
// the degenerate cases reads the polarity directly and a flip is then unsound.
//
// For a relation ⊕vⱼ = R over s variables, the gate whose inputs are the other
// s−1 variables has ⊕_{j≠i} vⱼ = R ⊕ vᵢ, so its output literal is vᵢ when R = 0
// and ¬vᵢ when R = 1 — independent of s.

// assert the extracted XOR gate over `inputs` (external DIMACS, any order) has
// exactly `expected_out` (external DIMACS, signed) as its output literal.
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

// R = 0, s = 3 (odd s — the branch the stale `^ s` term corrupted).
// x1⊕x2⊕x3 = 0, so the gate on {x1,x2} outputs +x3.
static void test_xor_output_sign_even_arity() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  xor_gate(k, 3, 1, 2); // the 4 odd-negation clauses ⇒ v1⊕v2⊕v3 = 0
  assert_xor_gate_output(k, {1, 2}, 3);
  assert_xor_gate_output(k, {2, 3}, 1);
}

// R = 1, s = 3: x1⊕x2⊕x3 = 1 (the even-negation clause set), so the gate on
// {x1,x2} outputs ¬x3.
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

// R = 0, s = 4 (even s — the branch that was accidentally correct; pin it so a
// "fix" cannot flip this one instead).  v1⊕v2⊕v3⊕v4 = 0 ⇒ gate on {1,2,3} → +4.
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

int main() {
  test_and_congruent();
  test_xor_congruent();
  test_ite_congruent();
  test_no_congruence();
  test_kind_isolation();
  test_xor_cancel_learns_unit();
  test_xor_cancel_refutes();
  test_and_clash_learns_unit();
  test_and_idempotent_unary_merge();
  test_contradictory_merge_refutes();
  test_xor_output_sign_even_arity();
  test_xor_output_sign_odd_relation();
  test_xor_output_sign_odd_arity();
  std::printf("test_congruence: all passed\n");
  return 0;
}
