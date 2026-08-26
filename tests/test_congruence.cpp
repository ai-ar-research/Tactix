// V6 Congruence: two gates of the same kind over the same inputs have
// equivalent outputs; congruence_apply adds (¬a∨b),(a∨¬b). docs/calculus.md.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/registry.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <unistd.h>

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

static size_t garbage_clauses(const Kernel &k) {
  size_t result = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *const clause = clause_at(k.arena, ref);
    result += clause->garbage();
    ref += clause_actual_words(clause);
  }
  return result;
}

static void assert_large_watches_are_live(const Kernel &k) {
  for (Lit lit = 0; lit < 2 * k.nvars; lit++)
    for (const WatchEntry watch : k.watches.entries(lit))
      if (!watch.binary()) {
        assert(watch.ref() < k.arena.size());
        assert(!clause_at(k.arena, watch.ref())->garbage());
      }
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

// Reference parity: Kissat and CaDiCaL both admit AND gates well beyond the
// old local arity-64 cutoff.  Pin the first excluded arity so a silent scope
// regression cannot strand equivalent high-fan-in outputs.
static void test_and_arity_above_old_cutoff() {
  constexpr int arity = 65;
  constexpr int first_output = arity + 1;
  constexpr int second_output = arity + 2;
  Kernel k;
  kernel_reserve_vars(k, second_output);
  const auto add_gate = [&](int output) {
    std::vector<Lit> base;
    base.reserve(arity + 1);
    base.push_back(internalize(output));
    for (int input = 1; input <= arity; input++) {
      kernel_add_binary_clause(k, internalize(-output), internalize(input));
      base.push_back(internalize(-input));
    }
    kernel_add_large_clause(k, base.data(), static_cast<uint32_t>(base.size()),
                            /*redundant=*/false, /*glue=*/0);
  };
  add_gate(first_output);
  add_gate(second_output);
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(equivalence_added(k, first_output, second_output));
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
  kernel_reserve_vars(k, 5);
  // Spend the shared Congruence allowance in gate extraction first.  V6a's
  // own structural pass must still reach the unrelated binary unit.
  k.work_budget = 1;
  for (unsigned i = 0; i < 300; i++)
    add3(k, 3, 4, 5);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(1)) > 0 && "the unit is asserted at the root");
  assert(k.stats.congruence_y1_found == 1);
  assert(k.stats.congruence_y1_units == 1);
  assert(k.stats.congruence_y1_ticks > 0);
  assert(k.stats.congruence_budget_stops > 0 &&
         "the shared Congruence budget was already exhausted");
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

// A root-valued input and an independently cancelled pair still require the
// XOR degeneracy ladder. The old value-derived branch tried the conclusion as
// one direct LRAT lemma, found it non-RUP, and silently lost the sound unit.
static void test_xor_value_plus_cancel_uses_ladder() {
  const std::string path = "/tmp/tactix-congruence-valued-xor-" +
                           std::to_string(static_cast<long>(getpid())) +
                           ".lrat";
  Proof proof;
  assert(proof.open(path.c_str(), ProofFormat::Lrat));
  Kernel k;
  kernel_reserve_vars(k, 6); // a=1 out=2 x=3 y=4 p=5 q=6
  proof.bind_variable_map(&k.internal_to_external);
  k.proof = &proof;

  k.unit_ids[var_of(internalize(-1))] = ++k.next_clause_id;
  kernel_assign_unit(k, internalize(-1)); // a=0
  const auto add_and = [&](int out, int lhs, int rhs) {
    kernel_add_binary_clause(k, internalize(-out), internalize(lhs),
                             ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(-out), internalize(rhs),
                             ++k.next_clause_id);
    const Lit base[3] = {internalize(out), internalize(-lhs),
                         internalize(-rhs)};
    kernel_add_large_clause(k, base, 3, false, 0, ++k.next_clause_id);
  };
  add_and(3, 5, 6);
  add_and(4, 5, 6); // closure proves x == y

  // out = a XOR x XOR y: all eight odd-negation clauses over the relation.
  const int vars[4] = {1, 3, 4, 2};
  for (unsigned mask = 0; mask < 16; mask++) {
    if (!(__builtin_popcount(mask) & 1))
      continue;
    Lit clause[4];
    for (unsigned j = 0; j < 4; j++)
      clause[j] = internalize((mask & (1u << j)) ? -vars[j] : vars[j]);
    kernel_add_large_clause(k, clause, 4, false, 0, ++k.next_clause_id);
  }

  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(-2)) > 0);
  assert(k.stats.congruence_units >= 1);
  proof.flush();
  k.proof = nullptr;
  std::remove(path.c_str());
}

// D-CONG-INPUT-UNIT: a root-false XOR output with an independently cancelled
// pair forces the one remaining unvalued input, and that conclusion is not RUP
// directly — the parity clauses propagate nothing while the cancelled pair is
// unassigned — so Canon::InputUnit must run the same cancellation ladder as
// Canon::Unit. The output owns the smallest variable so its gate pops first
// and the InputUnit channel, not the sibling Unit gate, draws the inference.
// Pre-repair, native LRAT declined the sound unit (congruence_unjustified)
// while DRAT emitted it without the justifying rungs.
static void test_xor_input_unit_with_cancel_uses_ladder() {
  const auto run = [](ProofFormat format, const std::string &path) {
    Proof proof;
    assert(proof.open(path.c_str(), format));
    Kernel k;
    kernel_reserve_vars(k, 6); // out=1 a=2 x=3 y=4 p=5 q=6
    proof.bind_variable_map(&k.internal_to_external);
    k.proof = &proof;

    k.unit_ids[var_of(internalize(-1))] = ++k.next_clause_id;
    kernel_assign_unit(k, internalize(-1)); // out=0; input a stays unvalued
    const auto add_and = [&](int out, int lhs, int rhs) {
      kernel_add_binary_clause(k, internalize(-out), internalize(lhs),
                               ++k.next_clause_id);
      kernel_add_binary_clause(k, internalize(-out), internalize(rhs),
                               ++k.next_clause_id);
      const Lit base[3] = {internalize(out), internalize(-lhs),
                           internalize(-rhs)};
      kernel_add_large_clause(k, base, 3, false, 0, ++k.next_clause_id);
    };
    add_and(3, 5, 6);
    add_and(4, 5, 6); // closure proves x == y

    // out = a XOR x XOR y: all eight odd-negation clauses over the relation.
    const int vars[4] = {1, 2, 3, 4};
    for (unsigned mask = 0; mask < 16; mask++) {
      if (!(__builtin_popcount(mask) & 1))
        continue;
      Lit clause[4];
      for (unsigned j = 0; j < 4; j++)
        clause[j] = internalize((mask & (1u << j)) ? -vars[j] : vars[j]);
      kernel_add_large_clause(k, clause, 4, false, 0, ++k.next_clause_id);
    }

    assert(congruence_apply(k) == ApplyResult::Ok);
    assert(k.value(internalize(-2)) > 0 && "the ladder derives -a");
    assert(k.stats.congruence_units >= 1);
    assert(k.stats.congruence_unjustified == 0 &&
           "the sound input unit must not be declined");
    proof.flush();
    k.proof = nullptr;
  };

  const std::string base = "/tmp/tactix-congruence-input-unit-" +
                           std::to_string(static_cast<long>(getpid()));
  const std::string lrat_path = base + ".lrat";
  run(ProofFormat::Lrat, lrat_path);
  std::remove(lrat_path.c_str());

  // DRAT admission: the unit line must be preceded by both top-level ladder
  // rungs over the cancelled representative (x ≡ y, so either variable), or
  // the certificate states an unsupported unit and the checker rejects it.
  const std::string drat_path = base + ".drat";
  run(ProofFormat::DratAscii, drat_path);
  {
    FILE *f = std::fopen(drat_path.c_str(), "r");
    assert(f);
    char buf[256];
    bool rung_pos = false, rung_neg = false, unit_line = false;
    while (std::fgets(buf, sizeof buf, f)) {
      const std::string line(buf);
      if (line == "-2 0\n") {
        unit_line = true;
        break;
      }
      if (line == "3 -2 0\n" || line == "4 -2 0\n")
        rung_pos = true;
      if (line == "-3 -2 0\n" || line == "-4 -2 0\n")
        rung_neg = true;
    }
    std::fclose(f);
    assert(unit_line && "the unit must be in the DRAT proof");
    assert(rung_pos && rung_neg &&
           "both ladder rungs must justify the unit before it is added");
  }
  std::remove(drat_path.c_str());
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

// When closure propagation finds an equivalence, Kissat enters dense mode a
// second time for representative-aware forward subsumption.  That transition
// saves the complete population (including the extracted suffix) and resumes
// it in compact pair order.  Thus a newly extracted lower-ranked partner moves
// before an older lower partner, while partners above the watched literal keep
// their first-resume order.  Preserving the whole suffix or sorting every
// partner would each leak the wrong propagation order into search.
static void test_forward_dense_resume_reorders_complete_binary_population() {
  Kernel k;
  kernel_reserve_vars(k, 10); // a new old c hi-new hi-old x y out1 out2
  const Lit a = internalize(1), c = internalize(4);
  const Lit newer = internalize(2), older = internalize(3);
  const Lit higher_newer = internalize(5), higher_older = internalize(6);
  kernel_add_binary_clause(k, negate(a), c, ++k.next_clause_id);
  kernel_add_binary_clause(k, c, older, ++k.next_clause_id);
  kernel_add_binary_clause(k, c, higher_older, ++k.next_clause_id);
  Lit strengthen[3] = {a, c, newer};
  kernel_add_large_clause(k, strengthen, 3, false, 0, ++k.next_clause_id);
  Lit strengthen_higher[3] = {a, c, higher_newer};
  kernel_add_large_clause(k, strengthen_higher, 3, false, 0,
                          ++k.next_clause_id);
  and_gate(k, 9, 7, 8);
  and_gate(k, 10, 7, 8); // forces a merge and therefore the forward transition

  const uint64_t generation = k.arena_generation;
  assert(congruence_apply(k) == ApplyResult::Ok);
  // Congruence reconnects the live image but leaves physical reclamation to
  // the caller's established phase boundary.  Standalone V6 must therefore
  // remain immediately usable with stable Refs and no watch into garbage.
  assert(k.arena_generation == generation);
  assert(garbage_clauses(k) > 0);
  assert_large_watches_are_live(k);
  assert(kernel_binary_live(k, c, newer));
  const std::vector<Lit> partners = binary_partners(k, c);
  const auto newer_pos = std::find(partners.begin(), partners.end(), newer);
  const auto older_pos = std::find(partners.begin(), partners.end(), older);
  const auto higher_newer_pos =
      std::find(partners.begin(), partners.end(), higher_newer);
  const auto higher_older_pos =
      std::find(partners.begin(), partners.end(), higher_older);
  assert(newer_pos != partners.end() && older_pos != partners.end());
  assert(higher_newer_pos != partners.end() &&
         higher_older_pos != partners.end());
  assert(newer_pos < older_pos);
  assert(higher_older_pos < higher_newer_pos);

  kernel_collect_garbage(k);
  assert(k.arena_generation == generation + 1);
  assert(garbage_clauses(k) == 0);
  assert_large_watches_are_live(k);
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

// Kissat's ternary-to-binary extraction is a mandatory pre-pass and has no
// Congruence effort limit.  The Tactix-only gate-cost compensator must not cut
// that semantic frontier even when its allowance is already one tick.
static void test_binary_extraction_precedes_gate_budget() {
  Kernel k;
  kernel_reserve_vars(k, 10);
  for (unsigned i = 0; i < 300; i++)
    add3(k, 7, 8, 9);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  add3(k, 1, 2, 3);
  k.stop_fuzz = 1000000000u; // poll every step; never globally terminate
  k.work_budget = 1;
  (void)congruence_apply(k);
  assert(kernel_binary_live(k, internalize(2), internalize(3)) &&
         "the complete reference pre-pass must extract its tail candidate");
  assert(k.stats.congruence_budget_stops == 1 &&
         "the gate-cost compensator remains live after the pre-pass");
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
  test_binary_extraction_precedes_gate_budget();
  test_and_congruent();
  test_and_arity_above_old_cutoff();
  test_xor_congruent();
  test_ite_congruent();
  test_no_congruence();
  test_kind_isolation();
  test_binary_resolution_unit();
  test_binary_resolution_no_unit();
  test_binary_equivalence_channel();
  test_binary_resolution_refutes();
  test_xor_cancel_learns_unit();
  test_xor_value_plus_cancel_uses_ladder();
  test_xor_input_unit_with_cancel_uses_ladder();
  test_unit_reschedules_only_affected_gate();
  test_xor_cancel_refutes();
  test_and_clash_learns_unit();
  test_and_idempotent_unary_merge();
  test_contradictory_merge_refutes();
  test_xor_output_sign_even_arity();
  test_xor_output_sign_odd_relation();
  test_xor_output_sign_odd_arity();
  test_extracted_binary_suffix_is_sorted();
  test_forward_dense_resume_reorders_complete_binary_population();
  test_binary_extraction_from_mixed_watch_lists();
  test_round_is_the_instance_and_substitute_owns_the_choice();
  std::printf("test_congruence: all passed\n");
  return 0;
}
