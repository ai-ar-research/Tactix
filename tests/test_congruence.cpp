// V6 Congruence: two gates of the same kind over the same inputs have
// equivalent outputs; congruence_apply adds (¬a∨b),(a∨¬b). docs/dev/calculus.md.
#undef NDEBUG
#include "temporary_file.hpp"
#include "kernel/kernel.hpp"
#include "frontend/parse.hpp"
#include "policies/default_policy.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/registry.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
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

static uint64_t congruence_pair_payload(int first, int second) {
  Lit a = internalize(first), b = internalize(second);
  if (b < a)
    std::swap(a, b);
  return (static_cast<uint64_t>(a) << 32) | b;
}

static uint64_t congruence_unit_payload(int unit) {
  const Lit literal = internalize(unit);
  return (static_cast<uint64_t>(literal) << 32) | literal;
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

static void test_forward_includes_only_redundant_physical_prefix() {
  Kernel k;
  kernel_reserve_vars(k, 7);
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2);
  const Lit before_lits[] = {internalize(3), internalize(-4), internalize(5)};
  const Ref before = kernel_add_large_clause(k, before_lits, 3, true, 1,
                                              ++k.next_clause_id);
  add3(k, 5, 6, 7); // the last irredundant record divides prefix from suffix
  const Lit after_lits[] = {internalize(3), internalize(-4), internalize(6)};
  const Ref after = kernel_add_large_clause(k, after_lits, 3, true, 1,
                                             ++k.next_clause_id);
  assert(congruence_apply(k) == ApplyResult::Ok);
  assert(clause_at(k.arena, before)->garbage());
  assert(!clause_at(k.arena, after)->garbage());
  assert_large_watches_are_live(k);
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

// Kissat's Congruence dense entry takes the save-stack path of
// `dense.c:flush_large_watches`, which deletes every binary with a
// root-assigned endpoint as a clause before the binaries are extracted; the
// rules that follow (Factor's occurrence counts in particular) never see it.
// The formula has no gate, so no merge and no forward-subsumption pass can
// retire the binary later: only the entry itself may delete it.
static void test_root_satisfied_binary_deleted_at_entry() {
  Kernel k;
  kernel_reserve_vars(k, 6); // fillers 1..4, u=5 v=6
  const Lit c1[3] = {internalize(1), internalize(2), internalize(3)};
  const Lit c2[3] = {internalize(-1), internalize(-2), internalize(4)};
  kernel_add_large_clause(k, c1, 3, false, 0, ++k.next_clause_id);
  kernel_add_large_clause(k, c2, 3, false, 0, ++k.next_clause_id);
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

// Pinned Kissat publishes both representative binaries on every new union,
// even when the formula already contains identical copies.  Dense forward
// entry later keeps one physical copy, but the publication must first create
// fresh proof identities and re-arm both Factor polarities.  The old liveness
// shortcut skipped that complete lifecycle and lost Rank-27 candidates.
static void test_existing_representative_pair_is_republished() {
  for (const ProofFormat format : {ProofFormat::DratAscii, ProofFormat::Lrat}) {
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

    const std::string path = "/tmp/tactix-congruence-republish-" +
                             std::to_string(static_cast<long>(getpid())) +
                             (format == ProofFormat::Lrat ? ".lrat" : ".drat");
    Proof proof;
    assert(proof.open(path.c_str(), format));
    proof.bind_variable_map(&k.internal_to_external);
    k.proof = &proof;
    assert(congruence_apply(k) == ApplyResult::Ok);
    proof.flush();
    k.proof = nullptr;
    std::remove(path.c_str());

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
// (l∨o) and (l∨¬o) resolve on o to {l}, no gate involved. docs/dev/calculus.md.
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

// Kissat's `all_variables` follows compact internal creation order. Stable
// Tactix variable IDs can disagree with that order after parsing/compaction,
// so two independent Y1 units must be discovered in permanent import order,
// not raw variable-number order.
static void test_binary_resolution_unit_uses_import_order() {
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

// A representative merge must also revisit a gate that was not an ordinary
// hash key on its first pop.  With out=0, `out = a & b` is initially only the
// residual clause (¬x∨¬b), so Canon::Skip consumes it.  Two earlier input
// merges have already moved its raw y/z occurrences to x/b. A unit-producing
// sibling then fixes out=0, and a value-rescheduled XOR proves x==b. Kissat's
// all-RHS occurrence index follows both earlier moves and revisits the skipped
// target as unary `false = x`; indexing only Canon::Ok keys strands it.
static void test_merge_reschedules_skipped_valued_and() {
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

// Kissat indexes XOR candidates after removing root-false literals and skips
// an effectively binary clause.  Keeping the original ternary shapes would
// fabricate a unary XOR gate and publish out == residual during Congruence;
// the reference leaves that residual relation to Substitute.
static void test_root_false_input_makes_binary_xor_ineligible() {
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
  assert(!k.watches.heads[negate(a)].binaries_first());
  assert(!k.watches.heads[negate(c)].binaries_first());

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

// The frozen v1 payload retains the canonical closure, while the additive
// semantic surface exposes one currently legal congruent-output inference.
// Re-enumeration after one merge observes the persistent binary equivalence
// partition and makes the next closure layer available without hiding a batch.
static void test_individual_merges_and_canonical_closure_coexist() {
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
    const Lit a = internalize(pair.first), b = internalize(pair.second);
    const uint64_t payload =
        (static_cast<uint64_t>(std::min(a, b)) << 32) | std::max(a, b);
    if (payload != selected)
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

static void test_individual_merges_expose_later_closure_layers() {
  Kernel k;
  kernel_reserve_vars(k, 7);
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2);
  and_gate(k, 5, 3, 6);
  and_gate(k, 7, 4, 6);
  const RuleVTable &cong = tactix_rules()[tactix_rule_id("Congruence")];
  uint64_t instances[16];
  uint32_t count = cong.semantic_instances(k, instances, 16);
  const auto payload = [](int first, int second) {
    const Lit a = internalize(first), b = internalize(second);
    return (static_cast<uint64_t>(std::min(a, b)) << 32) | std::max(a, b);
  };
  const uint64_t first_layer = payload(3, 4);
  const uint64_t second_layer = payload(5, 7);
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

static void test_individual_binary_resolution_unit() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  const RuleVTable &cong = tactix_rules()[tactix_rule_id("Congruence")];
  uint64_t instances[4];
  assert(cong.semantic_instances(k, instances, 4) == 1);
  assert(instances[0] == congruence_unit_payload(1));
  assert(cong.apply_semantic_instance(k, instances[0]) == ApplyResult::Ok);
  assert(k.value(internalize(1)) > 0);
  assert(k.stats.congruence_y1_units == 1);
  assert(k.stats.congruence_units == 0);
  assert(cong.semantic_instances(k, instances, 4) == 0);

  Kernel negative;
  kernel_reserve_vars(negative, 3);
  kernel_add_binary_clause(negative, internalize(1), internalize(2));
  assert(cong.semantic_instances(negative, instances, 4) == 0 &&
         "one parent must not expose a V6a unit");
}

static void test_individual_unary_gate_equivalence() {
  Kernel k;
  kernel_reserve_vars(k, 5); // p q, equivalent a/b, unary result c
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2);
  and_gate(k, 5, 3, 4);
  // State the enabling equality without running Congruence's closure.
  kernel_add_binary_clause(k, internalize(-3), internalize(4));
  kernel_add_binary_clause(k, internalize(3), internalize(-4));
  const RuleVTable &cong = tactix_rules()[tactix_rule_id("Congruence")];
  uint64_t instances[8];
  const uint32_t count = cong.semantic_instances(k, instances, 8);
  const uint64_t wanted = congruence_pair_payload(3, 5);
  assert(std::find(instances, instances + count, wanted) !=
         instances + count);
  assert(cong.apply_semantic_instance(k, wanted) == ApplyResult::Ok);
  assert(equivalence_added(k, 3, 5));
  assert(!k.inconsistent);
  kernel_check_invariants(k);
}

static void test_individual_gate_degeneracy_unit() {
  Kernel k;
  kernel_reserve_vars(k, 5); // p q, equivalent a/b, XOR result out
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2);
  xor_gate(k, 5, 3, 4);
  kernel_add_binary_clause(k, internalize(-3), internalize(4));
  kernel_add_binary_clause(k, internalize(3), internalize(-4));
  const RuleVTable &cong = tactix_rules()[tactix_rule_id("Congruence")];
  uint64_t instances[8];
  const uint32_t count = cong.semantic_instances(k, instances, 8);
  const uint64_t wanted = congruence_unit_payload(-5);
  assert(std::find(instances, instances + count, wanted) !=
         instances + count);
  assert(cong.apply_semantic_instance(k, wanted) == ApplyResult::Ok);
  assert(k.value(internalize(-5)) > 0);
  assert(k.stats.congruence_units == 1);
  assert(k.stats.congruence_y1_units == 0);
  kernel_check_invariants(k);
}

static void test_individual_complementary_class_refutation() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  and_gate(k, 3, 1, 2);
  and_gate(k, 4, 1, 2);
  // State a ≡ ¬b. The two congruent AND outputs then prove a ≡ b, so the
  // selected semantic transition is itself a refutation.
  kernel_add_binary_clause(k, internalize(-3), internalize(-4));
  kernel_add_binary_clause(k, internalize(3), internalize(4));
  const RuleVTable &cong = tactix_rules()[tactix_rule_id("Congruence")];
  uint64_t instances[8];
  const uint32_t count = cong.semantic_instances(k, instances, 8);
  const uint64_t wanted = congruence_pair_payload(3, 4);
  assert(std::find(instances, instances + count, wanted) !=
         instances + count);
  assert(cong.apply_semantic_instance(k, wanted) == ApplyResult::Unsat);
  assert(k.inconsistent);
  assert(k.stats.congruence_merges == 1);
}

static void test_individual_literal_form_and_refutation() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  and_gate(k, 3, 1, 2);
  // Define the opposite polarity of the same variable as the same function.
  kernel_add_binary_clause(k, internalize(3), internalize(1));
  kernel_add_binary_clause(k, internalize(3), internalize(2));
  add3(k, -3, -1, -2);
  const RuleVTable &cong = tactix_rules()[tactix_rule_id("Congruence")];
  uint64_t instances[8];
  const uint32_t count = cong.semantic_instances(k, instances, 8);
  const uint64_t wanted = congruence_pair_payload(3, -3);
  assert(std::find(instances, instances + count, wanted) !=
         instances + count);
  assert(cong.apply_semantic_instance(k, wanted) == ApplyResult::Unsat);
  assert(k.inconsistent);
}

static void literal_form_xor_fixture(Kernel &k) {
  kernel_reserve_vars(k, 5);
  xor_gate(k, 5, 1, 2);
  xor_gate(k, -5, 3, 4);
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_add_binary_clause(k, internalize(1), internalize(-3));
  kernel_add_binary_clause(k, internalize(-2), internalize(4));
  kernel_add_binary_clause(k, internalize(2), internalize(-4));
}

static void test_individual_literal_form_xor_refutation() {
  Kernel k;
  literal_form_xor_fixture(k);
  const RuleVTable &cong = tactix_rules()[tactix_rule_id("Congruence")];
  uint64_t instances[16];
  const uint32_t count = cong.semantic_instances(k, instances, 16);
  const uint64_t wanted = congruence_pair_payload(5, -5);
  assert(std::find(instances, instances + count, wanted) !=
         instances + count);
  assert(cong.apply_semantic_instance(k, wanted) == ApplyResult::Unsat);
  assert(k.inconsistent && !k.stats.congruence_unjustified);
}

static void literal_form_ite_fixture(Kernel &k) {
  kernel_reserve_vars(k, 7);
  ite_gate(k, 7, 1, 2, 3);
  ite_gate(k, -7, 4, 5, 6);
  for (const std::pair<int, int> &pair :
       {std::pair<int, int>{1, 4}, {2, 5}, {3, 6}}) {
    kernel_add_binary_clause(k, internalize(-pair.first),
                             internalize(pair.second));
    kernel_add_binary_clause(k, internalize(pair.first),
                             internalize(-pair.second));
  }
}

static void test_individual_literal_form_ite_refutation() {
  Kernel k;
  literal_form_ite_fixture(k);
  const RuleVTable &cong = tactix_rules()[tactix_rule_id("Congruence")];
  uint64_t instances[32];
  const uint32_t count = cong.semantic_instances(k, instances, 32);
  const uint64_t wanted = congruence_pair_payload(7, -7);
  assert(std::find(instances, instances + count, wanted) !=
         instances + count);
  assert(cong.apply_semantic_instance(k, wanted) == ApplyResult::Unsat);
  assert(k.inconsistent && !k.stats.congruence_unjustified);
}

static void test_canonical_literal_form_xor_ite_refutations() {
  Kernel xor_kernel;
  literal_form_xor_fixture(xor_kernel);
  assert(congruence_apply(xor_kernel) == ApplyResult::Unsat);
  assert(xor_kernel.inconsistent &&
         !xor_kernel.stats.congruence_unjustified);

  Kernel ite_kernel;
  literal_form_ite_fixture(ite_kernel);
  assert(congruence_apply(ite_kernel) == ApplyResult::Unsat);
  assert(ite_kernel.inconsistent &&
         !ite_kernel.stats.congruence_unjustified);
}

// A root unit already supplied by the negated lemma is not a unit hint:
// under ~(1 v -2), literal 2 is an assumption, not a propagation. The
// remaining two clauses still give a complete ordered LRAT refutation.
static void test_lrat_replay_omits_root_unit_in_initial_cube() {
  TemporaryFile proof_file;
  FILE *input = std::tmpfile(), *sink = proof_file.stream;
  assert(input && sink);
  // Put the unit last: input normalization must not strip -2 from the
  // earlier ternary, or its root reason never enters the replay walk.
  std::fputs("p cnf 3 3\n-2 1 3 0\n1 -3 0\n2 0\n", input);
  std::rewind(input);
  Kernel k;
  Proof proof;
  assert(proof.open(proof_file.path.c_str(),
                    ProofFormat::Lrat));
  proof.bind_variable_map(&k.internal_to_external);
  k.proof = &proof;
  assert(!parse_dimacs(input, k));
  const Lit root = kernel_internalize(k, 2);
  const uint64_t root_id = k.unit_ids[var_of(root)];
  assert(root_id);
  const Lit lemma[] = {kernel_internalize(k, 1), kernel_internalize(k, -2)};
  std::vector<uint64_t> chain;
  assert(congruence_replay_rup(k, lemma, 2, chain));
  assert(std::find(chain.begin(), chain.end(), root_id) == chain.end());
  // Under the complete cube {-1, 2}, clause 1 propagates 3 and clause 2
  // closes the refutation. The order is part of the LRAT hint contract.
  assert((chain == std::vector<uint64_t>{1, 2}));
  k.proof = nullptr;
  std::fclose(input);
}

static int semantic_proof_witness(const char *input_path,
                                  const char *proof_path,
                                  const char *mode,
                                  const char *wanted_kind) {
  Kernel k;
  Proof proof;
  const ProofFormat format = std::string(mode) == "lrat"
                                 ? ProofFormat::Lrat
                                 : ProofFormat::DratAscii;
  assert(proof.open(proof_path, format));
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

int main(int argc, char **argv) {
  if (argc == 5)
    return semantic_proof_witness(argv[1], argv[2], argv[3], argv[4]);
  assert(argc == 1);
  test_budget_stops_round();
  test_binary_extraction_precedes_gate_budget();
  test_and_congruent();
  test_existing_representative_pair_is_republished();
  test_root_satisfied_binary_deleted_at_entry();
  test_and_arity_above_old_cutoff();
  test_xor_congruent();
  test_ite_congruent();
  test_no_congruence();
  test_kind_isolation();
  test_binary_resolution_unit();
  test_binary_resolution_unit_uses_import_order();
  test_binary_resolution_no_unit();
  test_binary_equivalence_channel();
  test_binary_resolution_refutes();
  test_xor_cancel_learns_unit();
  test_xor_value_plus_cancel_uses_ladder();
  test_xor_input_unit_with_cancel_uses_ladder();
  test_unit_reschedules_only_affected_gate();
  test_merge_reschedules_skipped_valued_and();
  test_xor_cancel_refutes();
  test_and_clash_learns_unit();
  test_and_idempotent_unary_merge();
  test_contradictory_merge_refutes();
  test_xor_output_sign_even_arity();
  test_xor_output_sign_odd_relation();
  test_xor_output_sign_odd_arity();
  test_root_false_input_makes_binary_xor_ineligible();
  test_extracted_binary_suffix_is_sorted();
  test_forward_dense_resume_reorders_complete_binary_population();
  test_forward_includes_only_redundant_physical_prefix();
  test_binary_extraction_from_mixed_watch_lists();
  test_individual_merges_and_canonical_closure_coexist();
  test_individual_merges_expose_later_closure_layers();
  test_individual_binary_resolution_unit();
  test_individual_unary_gate_equivalence();
  test_individual_gate_degeneracy_unit();
  test_individual_complementary_class_refutation();
  test_individual_literal_form_and_refutation();
  test_individual_literal_form_xor_refutation();
  test_individual_literal_form_ite_refutation();
  test_canonical_literal_form_xor_ite_refutations();
  test_lrat_replay_omits_root_unit_in_initial_cube();
  std::printf("test_congruence: all passed\n");
  return 0;
}
