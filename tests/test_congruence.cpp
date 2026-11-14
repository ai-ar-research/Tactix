// V6 Congruence: two gates of the same kind over the same inputs have
// equivalent outputs; congruence_apply adds (¬a∨b),(a∨¬b). the rule calculus.
#include "unit_test.hpp"
#include "rule_test_utils.hpp"
#include "temporary_file.hpp"
#include "test_io.hpp"
#include "kernel/kernel.hpp"
#include "frontend/parse.hpp"
#include "policies/default_policy.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/registry.hpp"
#include "rules/propagate/propagate.hpp"

#include <map>
#include <sstream>
#include <utility>
#include <unistd.h>

// the two equivalence binaries between vars ea,eb (external DIMACS) are present
static bool equivalence_added(const Kernel &k, int ea, int eb) {
  return has_binary(k, internalize(-ea), internalize(eb)) &&
         has_binary(k, internalize(ea), internalize(-eb));
}

static uint64_t congruence_pair_payload(int first, int second) {
  Lit a = internalize(first), b = internalize(second);
  if (b < a)
    std::swap(a, b);
  return (static_cast<uint64_t>(a) << 32) | b;
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

TEST(test_forward_includes_only_redundant_physical_prefix) {
  Kernel k;
  kernel_reserve_vars(k, 7);
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2);
  const Ref before = add_external_clause(k, {3, -4, 5}, true, 1, ++k.next_clause_id);
  add3(k, 5, 6, 7); // the last irredundant record divides prefix from suffix
  const Ref after = add_external_clause(k, {3, -4, 6}, true, 1, ++k.next_clause_id);
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(clause_at(k.arena, before)->garbage());
  assert(!clause_at(k.arena, after)->garbage());
  assert_large_watches_are_live(k);
}

// Kissat's dense entry (dense.c flush_large_watches) deletes every binary with
// a root-assigned endpoint; with no gate, only the entry can delete it.
TEST(test_root_satisfied_binary_deleted_at_entry) {
  Kernel k;
  kernel_reserve_vars(k, 6); // fillers 1..4, u=5 v=6
  add_external_clause(k, {1, 2, 3}, false, 0, ++k.next_clause_id);
  add_external_clause(k, {-1, -2, 4}, false, 0, ++k.next_clause_id);
  const Lit u = internalize(5), v = internalize(6);
  kernel_add_binary_clause(k, u, v, ++k.next_clause_id);
  k.unit_ids[var_of(u)] = ++k.next_clause_id;
  kernel_assign_unit(k, u); // (u v v) is root-satisfied at entry
  assert(!kernel_propagate(k));
  assert(kernel_binary_copies(k, u, v) == 1);
  const uint64_t deleted_before = k.stats.binaries_deleted;
  assert(congruence_apply(k) == ApplyResult::NoOp);
  assert(kernel_binary_copies(k, u, v) == 0);
  assert(k.stats.binaries_deleted == deleted_before + 1);
}

// Kissat publishes both representative binaries on every union, with fresh
// proof identities and both Factor polarities re-armed, even over copies.
TEST(test_existing_representative_pair_is_republished) {
  Kernel k;
  kernel_reserve_vars(k, 4); // x1=1 x2=2 a=3 b=4
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2);
  const Lit forward[2] = {internalize(-4), internalize(3)};
  const Lit reverse[2] = {internalize(4), internalize(-3)};
  kernel_add_binary_clause(k, forward[0], forward[1], ++k.next_clause_id);
  kernel_add_binary_clause(k, reverse[0], reverse[1], ++k.next_clause_id);
  const uint64_t old_forward_id = kernel_binary_id(k, forward[0], forward[1]);
  const uint64_t old_reverse_id = kernel_binary_id(k, reverse[0], reverse[1]);
  std::fill(k.factor_marks.begin(), k.factor_marks.end(), 0);
  const uint64_t deleted_before = k.stats.binaries_deleted;

  TemporaryFile file;
  const std::string &path = file.path;
  Proof proof;
  assert(proof.open(path.c_str(), ProofFormat::DsrAscii));
  proof.bind_variable_map(&k.internal_to_external);
  k.proof = &proof;
  assert(congruence_apply(k) == ApplyResult::Ok);
  proof.flush();
  k.proof = nullptr;

  for (const Lit lit : {forward[0], forward[1], reverse[0], reverse[1]})
    assert(k.factor_marks[lit] && "representative publication must re-arm");
  assert(kernel_binary_copies(k, forward[0], forward[1]) == 1);
  assert(kernel_binary_copies(k, reverse[0], reverse[1]) == 1);
  // the first dense entry deletes each later copy as a clause
  // (congruence.c init_extraction -> kissat_enter_dense_mode)
  assert(k.stats.binaries_deleted >= deleted_before + 2);
  assert(kernel_binary_id(k, forward[0], forward[1]) > old_forward_id);
  assert(kernel_binary_id(k, reverse[0], reverse[1]) > old_reverse_id);
  assert(k.stats.congruence_unjustified == 0);
}

// Kissat and CaDiCaL admit high-arity AND gates; pin the first excluded arity.
TEST(test_and_arity_above_old_cutoff) {
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

// Negative controls: different inputs, or the same inputs under different gate
// kinds, compute different functions, so the outputs stay independent.
TEST(test_no_congruence) {
  for (const bool mixed_kinds : {false, true}) {
    Kernel k;
    kernel_reserve_vars(k, 6);
    and_gate(k, 5, 1, 2);
    if (mixed_kinds)
      xor_gate(k, 6, 1, 2);
    else
      and_gate(k, 6, 3, 4);
    assert(congruence_guard(k));
    assert(congruence_apply(k) == ApplyResult::NoOp);
    assert(!equivalence_added(k, 5, 6));
  }
}

// --- the binary-resolution unit channel (V6a) --------------------------------
// (l∨o) and (l∨¬o) resolve on o to {l}, no gate involved. the rule calculus.
TEST(test_binary_resolution_unit) {
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

// Kissat's all_variables order: independent Y1 units follow import order.
TEST(test_binary_resolution_unit_uses_import_order) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_record_import(k, 2); // x3 is created before externally smaller x1.
  kernel_record_import(k, 3);
  kernel_record_import(k, 0);
  kernel_record_import(k, 1);
  kernel_add_binary_clause(k, internalize(3), internalize(4));
  kernel_add_binary_clause(k, internalize(3), internalize(-4));
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(-2));

  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(k.stats.congruence_y1_units == 2);
  assert(k.trail.size() >= 2);
  assert(k.trail[0] == internalize(3));
  assert(k.trail[1] == internalize(1));
}

TEST(test_binary_resolution_no_unit) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(3));
  assert(congruence_guard(k));
  assert(congruence_apply(k) == ApplyResult::NoOp);
  assert(k.value(internalize(1)) == 0);
  assert(k.stats.congruence_y1_found == 0);
}

// Kissat imports binary equivalences before simplifying gates; the merge is
// productive, admitting the eager Substitute follow-up.
TEST(test_binary_equivalence_channel) {
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
TEST(test_binary_resolution_refutes) {
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
// With x1 = x2, out = x1 ^ x2 cancels to 0 and -out is a root unit.
TEST(test_xor_cancel_learns_unit) {
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

// out XOR a XOR x XOR y = 0 over `vars` (all eight odd-negation clauses), with
// x == y provable only by closing two AND gates over p,q and root-false x1.
static void cancelled_xor_fixture(Kernel &k, Proof &proof, const char *path,
                                  const int (&vars)[4]) {
  assert(proof.open(path, ProofFormat::DsrAscii));
  kernel_reserve_vars(k, 6); // x=3 y=4 p=5 q=6
  proof.bind_variable_map(&k.internal_to_external);
  k.proof = &proof;
  k.unit_ids[var_of(internalize(-1))] = ++k.next_clause_id;
  kernel_assign_unit(k, internalize(-1));
  for (const int out : {3, 4}) {
    kernel_add_binary_clause(k, internalize(-out), internalize(5),
                             ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(-out), internalize(6),
                             ++k.next_clause_id);
    add_external_clause(k, {out, -5, -6}, false, 0, ++k.next_clause_id);
  }
  for (unsigned mask = 0; mask < 16; mask++) {
    if (!(__builtin_popcount(mask) & 1))
      continue;
    Lit clause[4];
    for (unsigned j = 0; j < 4; j++)
      clause[j] = internalize((mask & (1u << j)) ? -vars[j] : vars[j]);
    kernel_add_large_clause(k, clause, 4, false, 0, ++k.next_clause_id);
  }
}

// A root-valued input (a = x1 = 0) with an independently cancelled pair uses
// the XOR degeneracy ladder for the output unit -x2.
TEST(test_xor_value_plus_cancel_uses_ladder) {
  TemporaryFile file;
  Proof proof;
  Kernel k;
  cancelled_xor_fixture(k, proof, file.path.c_str(), {1, 3, 4, 2});
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(-2)) > 0);
  assert(k.stats.congruence_units >= 1);
  proof.flush();
  k.proof = nullptr;
}

// D-CONG-INPUT-UNIT: a root-false XOR output (x1) with a cancelled pair forces
// the remaining input a = x2, which needs the same cancellation ladder as
// Canon::Unit; both top-level rungs precede the unit line.
TEST(test_xor_input_unit_with_cancel_uses_ladder) {
  TemporaryFile file;
  {
    Proof proof;
    Kernel k;
    cancelled_xor_fixture(k, proof, file.path.c_str(), {1, 2, 3, 4});
    assert(congruence_apply(k) == ApplyResult::Ok);
    assert(k.value(internalize(-2)) > 0 && "the ladder derives -a");
    assert(k.stats.congruence_units >= 1);
    assert(k.stats.congruence_unjustified == 0 &&
           "the sound input unit must not be declined");
    proof.flush();
    k.proof = nullptr;
  }
  std::istringstream proof(read_file(file.path));
  bool rung_pos = false, rung_neg = false, unit_line = false;
  for (std::string line; std::getline(proof, line);) {
    if (line == "-2 0") {
      unit_line = true;
      break;
    }
    rung_pos |= line == "3 -2 0" || line == "4 -2 0";
    rung_neg |= line == "-3 -2 0" || line == "-4 -2 0";
  }
  assert(unit_line && "the unit must be in the proof");
  assert(rung_pos && rung_neg &&
         "both ladder rungs must justify the unit before it is added");
}

// A late unit revisits only the consumed gates that mention it.
TEST(test_unit_reschedules_only_affected_gate) {
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

// A merge revisits a gate first consumed by Canon::Skip: Kissat's all-RHS
// occurrence index follows earlier input merges and revisits it as unary
// `false = x`.
TEST(test_merge_reschedules_skipped_valued_and) {
  Kernel k;
  // x=1 y=2 b=3 z=4 out=5 p=6 q=7 r=8 s=9
  kernel_reserve_vars(k, 9);
  and_gate(k, 5, 2, -1); // after y==x, forces out=0
  and_gate(k, 5, 2, 4);  // target: later becomes false = x & b
  and_gate(k, 1, 6, 7);
  and_gate(k, 2, 6, 7); // y == x
  and_gate(k, 3, 8, 9);
  and_gate(k, 4, 8, 9); // z == b
  xor_gate(k, 3, 1, 5); // out=0 later makes b == x
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(-5)) > 0);
  assert(k.value(internalize(-1)) > 0 &&
         "the skipped valued AND must be revisited after its inputs merge");
  assert(k.stats.congruence_units >= 1);
}

// Opposite-sign units derived in one round must refute, not assign.
TEST(test_xor_cancel_refutes) {
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
TEST(test_and_clash_learns_unit) {
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
TEST(test_and_idempotent_unary_merge) {
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
TEST(test_contradictory_merge_refutes) {
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
// For XOR(v) = R the gate on the other s-1 variables outputs v_i iff R = 0.

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
TEST(test_xor_output_sign_even_arity) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  xor_gate(k, 3, 1, 2); // the 4 odd-negation clauses ⇒ v1⊕v2⊕v3 = 0
  assert_xor_gate_output(k, {1, 2}, 3);
  assert_xor_gate_output(k, {2, 3}, 1);
}

// R = 1, s = 3: x1⊕x2⊕x3 = 1, so the gate on {x1,x2} outputs ¬x3.
TEST(test_xor_output_sign_odd_relation) {
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
TEST(test_xor_output_sign_odd_arity) {
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


// Kissat skips an XOR candidate that is binary after removing root-false
// literals, leaving that relation to Substitute.
TEST(test_root_false_input_makes_binary_xor_ineligible) {
  Kernel k;
  kernel_reserve_vars(k, 3); // out=1, residual=2, fixed=3
  xor_gate(k, 1, 2, 3);
  kernel_assign_unit(k, internalize(-3));

  std::vector<CongGate> gates;
  congruence_extract_gates(k, gates, /*with_xor_ite=*/true);
  for (const CongGate &gate : gates)
    assert(!(gate.kind == GateKind::Xor &&
             var_of(gate.output) == var_of(internalize(1))) &&
           "an effectively binary XOR must not enter Congruence");

  (void)congruence_apply(k);
  assert(!equivalence_added(k, 1, 2) &&
         "Congruence must not publish the residual unary equivalence");
}

// Kissat sorts only the newly extracted binary segment: the suffix is q,p.
TEST(test_extracted_binary_suffix_is_sorted) {
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

// Kissat's second dense entry for forward subsumption resumes the whole binary
// population in compact pair order.
TEST(test_forward_dense_resume_reorders_complete_binary_population) {
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
  // Standalone V6 leaves reclamation to the caller: stable Refs, no garbage watch.
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

// Binary membership is snapshotted before the ternary scan, even when the
// binaries follow large watches in the lists.
TEST(test_binary_extraction_from_mixed_watch_lists) {
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
  assert(!k.watches.heads[negate(a)].binaries_first());
  assert(!k.watches.heads[negate(c)].binaries_first());

  (void)congruence_apply(k);
  assert(kernel_binary_live(k, b, x));
  assert(kernel_binary_live(k, b, y));
}

// work_budget 0 is unbounded; a small budget cuts the round once with no
// unjustified inference (stop_fuzz disables the strided draw).
TEST(test_budget_stops_round) {
  const auto build = [](Kernel &k) {
    kernel_reserve_vars(k, 6);
    and_gate(k, 3, 1, 2);
    and_gate(k, 4, 1, 2);
    and_gate(k, 5, 1, 2);
    and_gate(k, 6, 1, 2);
  };
  Kernel full;
  build(full);
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
}

// Kissat's ternary-to-binary extraction is not limited by the gate budget.
TEST(test_binary_extraction_precedes_gate_budget) {
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

// The canonical payload and the semantic one-merge surface coexist;
// re-enumeration exposes the next closure layer.
TEST(test_individual_merges_and_canonical_closure_coexist) {
  Kernel k;
  kernel_reserve_vars(k, 6); // x1 x2, two AND outputs, and a second gate pair
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2);
  and_gate(k, 5, 1, 2);
  const RuleVTable &cong = tactix_rules()[tactix_rule_id("Congruence")];
  uint64_t insts[8];
  assert(cong.instances(k, insts, 8) == 1 && insts[0] == INSTANCE_CANONICAL &&
         "the frozen v1 surface retains its canonical closure");
  assert(cong.semantic_instances(k, insts, 8) == 3);
  const uint64_t selected = insts[0];
  const Lit selected_a = static_cast<Lit>(selected >> 32);
  const Lit selected_b = static_cast<Lit>(selected);
  assert(cong.apply_semantic_instance(k, selected) == ApplyResult::Ok);
  assert(kernel_binary_live(k, negate(selected_a), selected_b));
  assert(kernel_binary_live(k, selected_a, negate(selected_b)));

  unsigned unselected_live = 0;
  for (const std::pair<int, int> &pair :
       {std::pair<int, int>{3, 4}, {3, 5}, {4, 5}}) {
    if (congruence_pair_payload(pair.first, pair.second) != selected)
      unselected_live += equivalence_added(k, pair.first, pair.second);
  }
  assert(unselected_live == 0 && "one semantic action must not run the closure");
  assert(cong.semantic_instances(k, insts, 8) == 2);

  // The canonical entry remains the broad round and closes what is left.
  assert(cong.apply_instance(k, INSTANCE_CANONICAL) == ApplyResult::Ok);
  assert(equivalence_added(k, 3, 4) && equivalence_added(k, 3, 5));

  // the gate clauses are untouched: the collapse is V3's to schedule
  assert(has_binary(k, internalize(-3), internalize(1)));

  const RuleVTable &subst = tactix_rules()[tactix_rule_id("Substitute")];
  const uint32_t n = subst.instances(k, insts, 8);
  assert(n >= 1 && "the equivalence class V6 discovered is addressable in V3");
  assert(subst.apply_instance(k, insts[0]) == ApplyResult::Ok);
}

TEST(test_individual_merges_expose_later_closure_layers) {
  Kernel k;
  kernel_reserve_vars(k, 7);
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2);
  and_gate(k, 5, 3, 6);
  and_gate(k, 7, 4, 6);
  const RuleVTable &cong = tactix_rules()[tactix_rule_id("Congruence")];
  uint64_t instances[16];
  uint32_t count = cong.semantic_instances(k, instances, 16);
  const uint64_t first_layer = congruence_pair_payload(3, 4);
  const uint64_t second_layer = congruence_pair_payload(5, 7);
  assert(std::find(instances, instances + count, first_layer) !=
         instances + count);
  assert(std::find(instances, instances + count, second_layer) ==
         instances + count);
  assert(cong.apply_semantic_instance(k, first_layer) == ApplyResult::Ok);

  count = cong.semantic_instances(k, instances, 16);
  assert(std::find(instances, instances + count, second_layer) !=
         instances + count);
  assert(cong.apply_semantic_instance(k, second_layer) == ApplyResult::Ok);
  assert(equivalence_added(k, 3, 4));
  assert(equivalence_added(k, 5, 7));
  kernel_check_invariants(k);
}

// The checked-proof witness below (micro_proofs.sh) covers every positive
// semantic kind; one parent alone must not expose a V6a unit.
TEST(test_individual_binary_resolution_needs_both_parents) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  const RuleVTable &cong = tactix_rules()[tactix_rule_id("Congruence")];
  uint64_t instances[4];
  assert(cong.semantic_instances(k, instances, 4) == 0);
}

static bool forward_root_model(const Kernel &k, unsigned bits) {
  const auto value=[&](Lit l) {
    const bool v=(bits >> var_of(l)) & 1u;
    return (l & 1u) ? !v : v;
  };
  for(Var v=0;v<k.nvars;++v) {
    const Lit l=make_lit(v,false);
    if(k.value(l) && value(l)!=(k.value(l)>0)) return false;
  }
  for(Lit l=0;l<2*k.nvars;++l)
    for(const WatchEntry w:k.watches.entries(l))
      if(w.binary() && !value(l) && !value(w.other())) return false;
  for(Ref ref=0;ref<k.arena.size();) {
    const Clause *c=clause_at(k.arena,ref);ref+=clause_actual_words(c);
    if(c->garbage()) continue;
    bool satisfied=false;for(Lit l:*c) satisfied|=value(l);
    if(!satisfied) return false;
  }
  return true;
}

// Newly published representative binaries can re-arm a previously drained
// root; forward cleanup must propagate them before dropping assigned pairs.
TEST(test_forward_cleanup_preserves_root_closure) {
  for(bool preserve:{false,true}) for(bool dense:{false,true}) {
    Kernel k;kernel_reserve_vars(k,13);
    const std::vector<std::vector<int>> clauses={
      {1,7,12},{1,-7,-12},{-1,-5,12},{-1,5,-12},
      {1,7,13},{1,-7,-13},{-1,-5,13},{-1,5,-13},
      {-12,4},{-4,-12}};
    for(const auto& c:clauses) {
      std::vector<Lit> lits;for(int l:c) lits.push_back(internalize(l));
      const uint64_t id=++k.next_clause_id;
      if(lits.size()==2) kernel_add_binary_clause(k,lits[0],lits[1],id);
      else kernel_add_large_clause(k,lits.data(),lits.size(),false,0,id);
    }
    std::vector<bool> original(1u<<13);unsigned count=0;
    for(unsigned bits=0;bits<original.size();++bits) count+=(original[bits]=forward_root_model(k,bits));
    assert(count);
    k.congruence_parameters.binary_watch_order=preserve
        ? strategy::CongruenceBinaryWatchOrder::PreserveExisting
        : strategy::CongruenceBinaryWatchOrder::Canonical;
    OccIndex occurrences;if(dense) kernel_dense_connect(k,occurrences);
    assert(congruence_guard(k));
    assert(congruence_apply(k)==ApplyResult::Ok);
    assert(k.stats.congruence_merges && k.stats.propagation_rearms);
    assert(kernel_propagation_fixpoint(k));
    assert(k.value(internalize(-12))>0 && k.value(internalize(-13))>0);
    for(unsigned bits=0;bits<original.size();++bits) assert(forward_root_model(k,bits)==original[bits]);
    if(dense) kernel_dense_disconnect(k);
    kernel_check_invariants(k);
  }
}

using Row = std::vector<int>;

static Row sorted_row(Row row) {
  std::sort(row.begin(), row.end());
  return row;
}
static void xor_gate(std::vector<Row> &rows, int o, int a, int b) {
  rows.insert(rows.end(), {{-o, a, b}, {o, -a, b},
                           {o, a, -b}, {-o, -a, -b}});
}
static void and_gate(std::vector<Row> &rows, int o, int a, int b) {
  rows.insert(rows.end(), {{-o, a}, {-o, b}, {o, -a, -b}});
}
static void ite_gate(std::vector<Row> &rows, int o, int c, int t, int e) {
  rows.insert(rows.end(), {{-c, -t, o}, {-c, t, -o},
                           {c, -e, o}, {c, e, -o}});
}
// Temporary Congruence clauses must disappear in every clausal backend.
TEST(test_proof_only_ladders_do_not_survive) {
  const ProofFormat format = ProofFormat::DsrAscii;
  for (unsigned shape = 0; shape < 3; ++shape) {
      std::vector<Row> rows;
      if (shape == 0) {
        xor_gate(rows, 3, 1, 2);
        xor_gate(rows, 4, 1, 2);
      } else if (shape == 1) {
        ite_gate(rows, 4, 1, 2, 3);
        ite_gate(rows, 5, 1, 2, 3);
      } else {
        and_gate(rows, 3, 1, 2);
        and_gate(rows, 4, 1, 2);
        xor_gate(rows, 5, 3, 4);
      }
      TemporaryFile proof_file;
      FILE *input = std::tmpfile(), *output = proof_file.stream;
      assert(input && output);
      std::fprintf(input, "p cnf 5 %zu\n", rows.size());
      std::map<Row, int> population;
      for (const Row &row : rows) {
        ++population[sorted_row(row)];
        for (int literal : row)
          std::fprintf(input, "%d ", literal);
        std::fputs("0\n", input);
      }
      std::rewind(input);
      Kernel k;
      Proof proof;
      assert(proof.open(proof_file.path.c_str(), format));
      proof.bind_variable_map(&k.internal_to_external);
      k.proof = &proof;
      assert(!parse_dimacs(input, k));
      assert(congruence_apply(k) == ApplyResult::Ok);
      assert(k.stats.congruence_merges);
      if (shape == 2)
        assert(k.stats.congruence_units);
      proof.flush();
      std::rewind(output);
      std::string text;
      char buffer[4096];
      while (size_t size = std::fread(buffer, 1, sizeof buffer, output))
        text.append(buffer, size);
      std::istringstream lines(text);
      std::string line;
      unsigned retired = 0;
      while (std::getline(lines, line)) {
        const bool deletion = line.starts_with("d ");
        std::istringstream fields(deletion ? line.substr(2) : line);
        Row row;
        int literal;
        while (fields >> literal && literal)
          row.push_back(literal);
        int &copies = population[sorted_row(row)];
        copies += deletion ? -1 : 1;
        retired += deletion;
        assert(copies >= 0);
      }
      assert(retired); // proves the formerly missing backend path was reached
      for (const auto &[row, copies] : population) {
        if (!copies || row.size() <= 1)
          continue;
        bool live = row.size() == 2 &&
            kernel_binary_live(k, internalize(row[0]), internalize(row[1]));
        for (Ref ref = 0; !live && ref < k.arena.size();) {
          const Clause *clause = clause_at(k.arena, ref);
          if (!clause->garbage()) {
            Row actual;
            for (uint32_t i = 0; i < clause->size; ++i)
              actual.push_back(kernel_externalize(k, clause->lits[i]));
            live = sorted_row(actual) == row;
          }
          ref += clause_actual_words(clause);
        }
        assert(live && "proof-only ladder survived Congruence");
      }
      std::fclose(input);
    }
}

// Binary equivalences must reach dense cleanup even without extracted gates.
TEST(test_binary_only_cleanup) {
  unsigned cases = 0;
  for (bool merge : {false, true})
    for (bool entry_unit : {false, true})
      for (unsigned copies : {1u, 2u})
        for (bool reverse_import : {false, true})
          for (bool public_instance : {false, true})
            {
              Kernel k;
              kernel_reserve_vars(k, 4);
              for (unsigned i = 0; i < 4; i++)
                kernel_record_import(k, reverse_import ? 3 - i : i);
              const Lit u = internalize(1), x = internalize(2);
              const Lit a = internalize(3), b = internalize(4);
              for (unsigned i = 0; i < copies; i++) {
                kernel_add_binary_clause(k, u, x, ++k.next_clause_id);
                kernel_add_binary_clause(k, u, negate(x), ++k.next_clause_id);
                if (merge) {
                  kernel_add_binary_clause(k, negate(a), b, ++k.next_clause_id);
                  kernel_add_binary_clause(k, a, negate(b), ++k.next_clause_id);
                }
              }
              if (entry_unit) {
                k.unit_ids[var_of(u)] = ++k.next_clause_id;
                kernel_assign_unit(k, u);
                assert(!kernel_propagate(k));
              }
              std::vector<CongGate> gates;
              congruence_extract_gates(k, gates, true);
              assert(gates.empty());
              k.level = 1;
              assert(!congruence_guard(k));
              k.level = 0;
              assert(congruence_guard(k));
              const int id = tactix_rule_id("Congruence");
              assert(id >= 0);
              const auto &rule = tactix_rules()[id];
              uint64_t instance = 0;
              assert(rule.instances(k, &instance, 1) == 1);
              {
                TemporaryFile file;
                Proof proof;
                assert(proof.open(file.path.c_str(), ProofFormat::DsrAscii));
                proof.bind_variable_map(&k.internal_to_external);
                k.proof = &proof;
                const ApplyResult result = public_instance
                    ? rule.apply_instance(k, instance) : congruence_apply(k);
                assert(result == (merge || !entry_unit ? ApplyResult::Ok : ApplyResult::NoOp));
                proof.flush();
                k.proof = nullptr;
              }
              assert(!k.inconsistent && !k.stats.congruence_unjustified);
              assert(k.stats.congruence_gates == 0);
              assert(k.stats.congruence_merges == static_cast<unsigned>(merge));
              assert(k.nvars == 4 && k.value(u) > 0);
              // No variables retire: both original models a=b=false/true
              // remain models; no reconstruction witness is needed here.
              assert(!kernel_var_retired(k, var_of(a)) && !kernel_var_retired(k, var_of(b)));
              const unsigned unit_copies = merge || entry_unit ? 0 : 1;
              assert(kernel_binary_copies(k, u, x) == unit_copies &&
                     kernel_binary_copies(k, u, negate(x)) == unit_copies);
              assert(!merge || (kernel_binary_copies(k, negate(a), b) == 1 &&
                                kernel_binary_copies(k, a, negate(b)) == 1));
              cases++;
            }
  assert(cases == 32);
}

static int semantic_proof_witness(const char *input_path,
                                  const char *proof_path,
                                  const char *mode,
                                  const char *wanted_kind) {
  Kernel k;
  Proof proof;
  assert(std::string(mode) == "dsr");
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  k.proof = &proof;
  proof.bind_variable_map(&k.internal_to_external);
  FILE *input = std::fopen(input_path, "r");
  assert(input && !parse_dimacs(input, k));
  std::fclose(input);

  const RuleVTable &cong = tactix_rules()[tactix_rule_id("Congruence")];
  const bool canonical = !std::strncmp(wanted_kind, "canonical-", 10);
  const char *const kind = canonical ? wanted_kind + 10 : wanted_kind;
  std::vector<uint64_t> actions(64);
  const uint32_t count =
      cong.semantic_instances(k, actions.data(), actions.size());
  actions.resize(count);
  uint64_t selected = INSTANCE_CANONICAL;
  for (const uint64_t action : actions) {
    const Lit first = static_cast<Lit>(action >> 32);
    const Lit second = static_cast<Lit>(action);
    const int ef = kernel_externalize(k, first);
    const int es = kernel_externalize(k, second);
    if ((!std::strcmp(kind, "v6a") && ef == 1 && es == 1) ||
        (!std::strcmp(kind, "gate-unit") && ef == -5 && es == -5) ||
        (!std::strcmp(kind, "unary") &&
         ((ef == 3 && es == 5) || (ef == 5 && es == 3))) ||
        (!std::strcmp(kind, "clash") &&
         ((ef == 3 && es == 4) || (ef == 4 && es == 3))) ||
        (!std::strcmp(kind, "literal-clash") &&
         ((ef == 3 && es == -3) || (ef == -3 && es == 3))) ||
        (!std::strcmp(kind, "literal-xor-clash") &&
         ((ef == 5 && es == -5) || (ef == -5 && es == 5))) ||
        (!std::strcmp(kind, "literal-ite-clash") &&
         ((ef == 7 && es == -7) || (ef == -7 && es == 7)))) {
      selected = action;
      break;
    }
  }
  assert(canonical || selected != INSTANCE_CANONICAL);
  const ApplyResult applied = canonical
                                  ? cong.apply_instance(k, INSTANCE_CANONICAL)
                                  : cong.apply_semantic_instance(k, selected);
  const bool refutation = !std::strcmp(kind, "clash") ||
                          std::strstr(kind, "literal-");
  assert(applied == (refutation
                         ? ApplyResult::Unsat
                         : ApplyResult::Ok));
  assert(!k.stats.congruence_unjustified);
  if (!std::strcmp(kind, "v6a"))
    assert(k.stats.congruence_y1_units == 1);
  else if (!std::strcmp(kind, "gate-unit"))
    assert(k.stats.congruence_units == 1);
  else if (!refutation)
    assert(k.stats.congruence_merges == 1);

  int result = 20;
  if (!k.inconsistent) {
    DefaultPolicy policy;
    result = policy.solve(k, nullptr);
  }
  assert(result == 20);
  proof.flush();
  k.proof = nullptr;
  return result;
}

// Routine ctest runs the micro_proofs witnesses without a proof checker: each
// selected semantic instance, and the canonical literal-form refutations,
// applies exactly the requested step and the run still refutes.
TEST(test_semantic_witnesses) {
  const std::pair<const char *, const char *> cases[] = {
      {"v6a", "p cnf 4 6\n1 2 0\n1 -2 0\n3 4 0\n3 -4 0\n-3 4 0\n-3 -4 0\n"},
      {"gate-unit",
       "p cnf 7 10\n-3 4 0\n3 -4 0\n-5 3 4 0\n5 -3 4 0\n5 3 -4 0\n"
       "-5 -3 -4 0\n6 7 0\n6 -7 0\n-6 7 0\n-6 -7 0\n"},
      {"unary", "p cnf 7 9\n-3 4 0\n3 -4 0\n-5 3 0\n-5 4 0\n5 -3 -4 0\n"
                "6 7 0\n6 -7 0\n-6 7 0\n-6 -7 0\n"},
      {"clash", "p cnf 4 8\n-3 1 0\n-3 2 0\n3 -1 -2 0\n-4 1 0\n-4 2 0\n"
                "4 -1 -2 0\n-3 -4 0\n3 4 0\n"},
      {"literal-clash",
       "p cnf 3 6\n-3 1 0\n-3 2 0\n3 -1 -2 0\n3 1 0\n3 2 0\n-3 -1 -2 0\n"},
      {"literal-xor-clash",
       "p cnf 5 12\n-5 1 2 0\n5 -1 2 0\n5 1 -2 0\n-5 -1 -2 0\n5 3 4 0\n"
       "-5 -3 4 0\n-5 3 -4 0\n5 -3 -4 0\n-1 3 0\n1 -3 0\n-2 4 0\n2 -4 0\n"},
      {"literal-ite-clash",
       "p cnf 7 14\n1 -3 7 0\n1 3 -7 0\n-1 -2 7 0\n-1 2 -7 0\n4 -6 -7 0\n"
       "4 6 7 0\n-4 -5 -7 0\n-4 5 7 0\n-1 4 0\n1 -4 0\n-2 5 0\n2 -5 0\n"
       "-3 6 0\n3 -6 0\n"},
  };
  for (const auto &[kind, cnf] : cases)
    for (const bool canonical : {false, true}) {
      if (canonical && std::strcmp(kind, "literal-xor-clash") &&
          std::strcmp(kind, "literal-ite-clash"))
        continue;
      TemporaryFile input, proof;
      std::fputs(cnf, input.stream);
      std::fflush(input.stream);
      const std::string wanted =
          std::string(canonical ? "canonical-" : "") + kind;
      assert(semantic_proof_witness(input.path.c_str(), proof.path.c_str(),
                                    "dsr", wanted.c_str()) == 20);
    }
}

int main(int argc, char **argv) {
  if (argc == 5)
    return semantic_proof_witness(argv[1], argv[2], argv[3], argv[4]);
  assert(argc == 1);
  run_tests();
  std::printf("test_congruence: all passed\n");
  return 0;
}
