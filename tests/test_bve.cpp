// E1a BVE: the rule skeleton, the round-local occurrence index, and candidate
// scheduling (no elimination yet — apply is a NoOp, like V3a shipped
// detection-only). These tests pin the index contents and the schedule order.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/bve/bve.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>

using namespace tactix;

// guard: pure quiescent-root precondition (level 0, not refuted). No fixpoint
// requirement (D-LEVEL purified).
static void test_guard() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  assert(bve_guard(k)); // empty trail at level 0
  kernel_assign_decision(k, internalize(1));
  assert(!bve_guard(k)); // level > 0 is not a root state
  kernel_backtrack(k, 0);
  assert(bve_guard(k));
  k.inconsistent = true;
  assert(!bve_guard(k)); // refuted
}

// occurrence index: both polarities, large + binaries, active literals only.
static void test_occurrences() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, /*redundant=*/false, /*glue=*/0); // (1∨2∨3)
  kernel_add_binary_clause(k, internalize(-1), internalize(2));       // (¬1∨2)

  BveOccurrences occ;
  bve_build_occurrences(k, occ);
  // count-level contract (BveOccurrences is flat counts):
  // +1 large only; ¬1 binary only; +2 in both; +3 large only
  assert(occ.count(internalize(1)) == 1);
  assert(occ.count(internalize(-1)) == 1);
  assert(occ.count(internalize(2)) == 2);
  assert(occ.count(internalize(-2)) == 0);
  assert(occ.count(internalize(3)) == 1);
}

// redundant (learned) large clauses are NOT in the elimination set.
static void test_occurrences_skip_redundant() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, /*redundant=*/true, /*glue=*/2); // learned
  BveOccurrences occ;
  bve_build_occurrences(k, occ);
  assert(occ.count(internalize(1)) == 0); // redundant clause not counted
}

// schedule: fewest-resolutions-first (ascending occ(x)·occ(¬x)); a pure-ish var
// (one polarity absent → score 0) ranks first.
static void test_schedule_order() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);                  // (1∨2∨3)
  kernel_add_binary_clause(k, internalize(-1), internalize(-2)); // (¬1∨¬2)
  // scores: var3 = occ(+3)·occ(¬3) = 1·0 = 0; var1 = 1·1 = 1; var2 = 1·1 = 1
  std::vector<Var> cand;
  bve_schedule(k, cand);
  assert(cand.size() == 3);
  assert(cand[0] == var_of(internalize(3))); // score 0 ranks first
  // the two score-1 vars follow in stable (variable) order: D1 then D2
  assert(cand[1] == var_of(internalize(1)));
  assert(cand[2] == var_of(internalize(2)));
}

// assigned variables and variables in no elimination clause are not candidates.
static void test_schedule_excludes() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0); // (1∨2∨3); var4 unused
  kernel_assign_unit(k, internalize(-3));     // 3 assigned (false)
  std::vector<Var> cand;
  bve_schedule(k, cand);
  // var3 assigned and var4 in no clause → both excluded; var1, var2 remain
  assert(cand.size() == 2);
  for (const Var v : cand) {
    assert(v != var_of(internalize(3))); // assigned
    assert(v != var_of(internalize(4))); // in no elimination clause
  }
}

// N1 instance surface: candidate variables are enumerated; an out-of-range
// payload is a total NoOp.
static void test_instance_surface() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  const int rid = tactix_rule_id("BVE");
  assert(rid >= 0);
  const RuleVTable &vt = tactix_rules()[rid];
  assert(vt.guard(k) && vt.instances && vt.apply_instance);
  uint64_t insts[8];
  const uint32_t n = vt.instances(k, insts, 8);
  assert(n >= 1); // at least one candidate
  Lit r;
  assert(lit_of_instance(insts[0], &r)); // decodable var payload
  assert(insts[0] != INSTANCE_CANONICAL);
  assert(vt.apply_instance(k, lit_instance(internalize(99))) ==
         ApplyResult::NoOp); // out of range
}

static bool has_binary(const Kernel &k, Lit a, Lit b) {
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[a].offset;
  for (uint32_t i = 0; i < k.watches.heads[a].size;) {
    if (Watch{ws[i]}.binary()) {
      if (Watch{ws[i]}.other() == b)
        return true;
      i += 1;
    } else {
      i += 2;
    }
  }
  return false;
}

// E1b: eliminate var 1 from (1∨2),(¬1∨3) → the resolvent (2∨3); var 1 vanishes
// and reconstruction witnesses are pushed.
static void test_eliminate_basic() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));  // (1∨2)
  kernel_add_binary_clause(k, internalize(-1), internalize(3)); // (¬1∨3)
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) == ApplyResult::Ok);
  assert(!has_binary(k, internalize(1), internalize(2)));  // originals gone
  assert(!has_binary(k, internalize(-1), internalize(3)));
  assert(has_binary(k, internalize(2), internalize(3)));   // resolvent present
  assert(!k.reconstruction.empty());                       // witnesses pushed
}

// E1b (highest-risk): model reconstruction through ρ. Eliminate 1 from
// (1∨2),(¬1∨3); the reduced formula is (2∨3). Hand a model (2=F, 3=T) with 1 at
// the WRONG value, extend, and assert both ORIGINAL clauses are satisfied.
static void test_eliminate_reconstruction() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));  // (1∨2)
  kernel_add_binary_clause(k, internalize(-1), internalize(3)); // (¬1∨3)
  const int orig[2][2] = {{1, 2}, {-1, 3}};
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) == ApplyResult::Ok);
  auto set = [&](int dimacs, bool val) {
    const Lit l = internalize(dimacs);
    k.values[l] = val ? 1 : -1;
    k.values[negate(l)] = val ? -1 : 1;
  };
  set(2, false);
  set(3, true);
  set(1, false); // deliberately wrong — extend must recover 1 = true
  kernel_extend_model(k);
  assert(k.values[internalize(1)] > 0 && "1 reconstructed to true");
  for (const auto &cl : orig) {
    bool sat = false;
    for (const int e : cl)
      sat |= (k.values[internalize(e)] > 0);
    assert(sat && "every original clause satisfied by the extended model");
  }
}

// E1b: a variable whose resolvent count exceeds occ⁺+occ⁻+bound is NOT
// eliminated (bound respected). 6 (x∨aᵢ) × 6 (¬x∨bⱼ) → 36 resolvents > 6+6+16.
static void test_bound_skip() {
  Kernel k;
  kernel_reserve_vars(k, 13); // x=1, a=2..7, b=8..13
  for (int i = 2; i <= 7; i++)
    kernel_add_binary_clause(k, internalize(1), internalize(i)); // (1∨aᵢ)
  for (int j = 8; j <= 13; j++)
    kernel_add_binary_clause(k, internalize(-1), internalize(j)); // (¬1∨bⱼ)
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::NoOp);                               // 36 > 28: kept
  assert(has_binary(k, internalize(1), internalize(2)));   // var 1 still present
}

// E1c: an AND/OR gate x↔OR(a₁..a₆) lets BVE resolve only gate×antecedent. With
// 4 antecedents per side the full cross product (E1b) exceeds the bound, but the
// gate reduction drops the 16 antecedent×antecedent resolvents, so BVE fires.
// Also assert a gate×antecedent resolvent is present and an antecedent×antecedent
// one is absent (the reduction's defining behavior).
static void test_gate_and_or_fires() {
  Kernel k;
  kernel_reserve_vars(k, 15); // x=1, a=2..7, p=8..11, q=12..15
  Lit base[7] = {internalize(-1), internalize(2), internalize(3), internalize(4),
                 internalize(5),  internalize(6), internalize(7)};
  kernel_add_large_clause(k, base, 7, false, 0); // (¬x∨a₁∨…∨a₆)
  for (int i = 2; i <= 7; i++)
    kernel_add_binary_clause(k, internalize(1), internalize(-i)); // (x∨¬aᵢ)
  for (int p = 8; p <= 11; p++)
    kernel_add_binary_clause(k, internalize(1), internalize(p)); // (x∨pₖ) antecedent
  for (int q = 12; q <= 15; q++)
    kernel_add_binary_clause(k, internalize(-1), internalize(q)); // (¬x∨qₗ) antecedent
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) == ApplyResult::Ok);
  assert(!has_binary(k, internalize(1), internalize(8)));    // var 1 eliminated
  assert(has_binary(k, internalize(-2), internalize(12)));   // gate×ant (¬a₁∨q₁)
  assert(!has_binary(k, internalize(8), internalize(12)));   // ant×ant (p₁∨q₁) skipped
}

// E1c: an equivalence gate x↔y eliminates x as a substitution — antecedents get
// x replaced by ±y, the antecedent×antecedent resolvent is skipped, and the
// model still reconstructs (x = y).
static void test_gate_equivalence_substitution() {
  Kernel k;
  kernel_reserve_vars(k, 4); // x=1, y=2, ant vars 3,4
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // (¬x∨y)  gate
  kernel_add_binary_clause(k, internalize(1), internalize(-2)); // (x∨¬y)  gate
  kernel_add_binary_clause(k, internalize(1), internalize(3));  // (x∨3)   antecedent
  kernel_add_binary_clause(k, internalize(-1), internalize(4)); // (¬x∨4)  antecedent
  const int orig[4][2] = {{-1, 2}, {1, -2}, {1, 3}, {-1, 4}};
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) == ApplyResult::Ok);
  assert(has_binary(k, internalize(-2), internalize(4)));    // (¬y∨4): x→y in ant⁻
  assert(has_binary(k, internalize(2), internalize(3)));     // (y∨3):  x→y in ant⁺
  assert(!has_binary(k, internalize(3), internalize(4)));    // (3∨4) ant×ant skipped
  auto set = [&](int dimacs, bool val) {
    const Lit l = internalize(dimacs);
    k.values[l] = val ? 1 : -1;
    k.values[negate(l)] = val ? -1 : 1;
  };
  set(2, true);  // y = T
  set(4, true);  // needed by (¬y∨4)
  set(3, false);
  set(1, false); // wrong — extend must recover x = y = true
  kernel_extend_model(k);
  assert(k.values[internalize(1)] > 0 && "x reconstructed to y = true");
  for (const auto &cl : orig) {
    bool sat = false;
    for (const int e : cl)
      sat |= (k.values[internalize(e)] > 0);
    assert(sat && "every original clause satisfied by the extended model");
  }
}

// SP-BVE-GATE-1: an ITE gate x ↔ (c ? ¬e : ¬t), encoded by the four ternaries
//   (x∨c∨t) (x∨¬c∨e) (¬x∨c∨¬t) (¬x∨¬c∨¬e),
// plus 6 antecedent binaries per side. Full cross product 8·8 − 4 tautologies =
// 60 > base+bound = 16+16, so E1b alone declines; the gate charges only the 24
// gate×antecedent pairs and BVE fires. The mask arm proves the gate is what did
// it, and the counters are the reachability readout.
static void test_gate_ite_fires() {
  auto build = [](Kernel &k) {
    kernel_reserve_vars(k, 16); // x=1, c=2, t=3, e=4, ant p=5..10, q=11..16
    const int gate[4][3] = {
        {1, 2, 3}, {1, -2, 4}, {-1, 2, -3}, {-1, -2, -4}};
    for (const auto &g : gate) {
      Lit ls[3] = {internalize(g[0]), internalize(g[1]), internalize(g[2])};
      kernel_add_large_clause(k, ls, 3, false, 0);
    }
    for (int p = 5; p <= 10; p++)
      kernel_add_binary_clause(k, internalize(1), internalize(p));
    for (int q = 11; q <= 16; q++)
      kernel_add_binary_clause(k, internalize(-1), internalize(q));
  };
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  const uint64_t x = lit_instance(internalize(1));
  { // ITE off → full cross product exceeds the bound. R10's general definition
    // extractor has to come off with it: the sub-solver refutes this shape's
    // reduced occurrence lists too (that is the point of it), so leaving it on
    // would make the arm a NoOp-that-never-was.
    Kernel k;
    k.bve_gate_mask = ~(BVE_GATE_ITE | BVE_GATE_DEFINITION);
    build(k);
    assert(vt.apply_instance(k, x) == ApplyResult::NoOp);
    assert(k.stats.bve_gates_ite == 0);
  }
  { // ITE on → the gate reduction fires
    Kernel k;
    k.bve_gate_mask = BVE_GATE_ITE;
    build(k);
    assert(vt.apply_instance(k, x) == ApplyResult::Ok);
    assert(k.stats.bve_gates_ite == 1);
    // occ⁺·occ⁻ = 64 charged pairs become 24 → 40 saved
    assert(k.stats.bve_gate_pairs_saved == 40);
    assert(!has_binary(k, internalize(5), internalize(11))); // ant×ant skipped
  }
}

// SP-BVE-GATE-1: an arity-2 XOR gate x ↔ a⊕b (all four parity clauses) with 6
// antecedent binaries per side — the same arithmetic as the ITE case. The
// ITE detector must DECLINE this shape (its two branch literals share a
// variable), so the "XOR off" arm is a real negative and not a silent AND/OR
// hit. Also pins model reconstruction through an XOR gate.
static void test_gate_xor_fires() {
  auto build = [](Kernel &k) {
    kernel_reserve_vars(k, 15); // x=1, a=2, b=3, ant p=4..9, q=10..15
    const int gate[4][3] = {
        {1, -2, 3}, {1, 2, -3}, {-1, 2, 3}, {-1, -2, -3}};
    for (const auto &g : gate) {
      Lit ls[3] = {internalize(g[0]), internalize(g[1]), internalize(g[2])};
      kernel_add_large_clause(k, ls, 3, false, 0);
    }
    for (int p = 4; p <= 9; p++)
      kernel_add_binary_clause(k, internalize(1), internalize(p));
    for (int q = 10; q <= 15; q++)
      kernel_add_binary_clause(k, internalize(-1), internalize(q));
  };
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  const uint64_t x = lit_instance(internalize(1));
  { // XOR off (ITE and the rest on, R10's definition extractor off for the same
    // reason as the ITE arm) → declined
    Kernel k;
    k.bve_gate_mask = ~(BVE_GATE_XOR | BVE_GATE_DEFINITION);
    build(k);
    assert(vt.apply_instance(k, x) == ApplyResult::NoOp);
    assert(k.stats.bve_gates_ite == 0 && "ITE must decline an XOR shape");
  }
  { // XOR on → the gate reduction fires, and the model still reconstructs
    Kernel k;
    k.bve_gate_mask = BVE_GATE_XOR;
    build(k);
    assert(vt.apply_instance(k, x) == ApplyResult::Ok);
    assert(k.stats.bve_gates_xor == 1);
    assert(k.stats.bve_gate_pairs_saved == 40);
    auto set = [&](int dimacs, bool val) {
      const Lit l = internalize(dimacs);
      k.values[l] = val ? 1 : -1;
      k.values[negate(l)] = val ? -1 : 1;
    };
    set(2, true);   // a = T
    set(3, false);  // b = F  ⇒ the gate forces x = a⊕b = T
    for (int p = 4; p <= 9; p++)
      set(p, false); // every (x∨p) antecedent needs x
    for (int q = 10; q <= 15; q++)
      set(q, true);  // every (¬x∨q) antecedent already satisfied
    set(1, false);   // deliberately wrong
    kernel_extend_model(k);
    assert(k.values[internalize(1)] > 0 && "x reconstructed to a⊕b = true");
  }
}

// a LIVE large clause whose literal set is exactly `want` (order-insensitive)
static bool has_large(const Kernel &k, std::vector<int> want) {
  std::sort(want.begin(), want.end());
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage()) {
      std::vector<int> got;
      for (const Lit l : *c)
        got.push_back(externalize(l));
      std::sort(got.begin(), got.end());
      if (got == want)
        return true;
    }
    ref += clause_words(c->size);
  }
  return false;
}

// R10 · a GENERAL definition, i.e. one no syntactic matcher sees. The pivot x:
//   gate⁺ (x∨a) (x∨b)      gate⁻ (¬x∨¬a∨c) (¬x∨¬b∨¬c)
// reduces to {a, b, ¬a∨c, ¬b∨¬c}, which is UNSAT — condition (★) — so those
// four clauses are a definition of x. Every syntactic detector declines it:
// EQUIV needs a binary on BOTH sides, AND/OR needs the base clause's inputs as
// binaries on the other side (¬c has none), and ITE/XOR need ternaries on the
// POSITIVE side, which has only binaries.
//
// It is also the shape whose gate×gate resolvents are NOT tautologies —
// (x∨a)⊗(¬x∨¬b∨¬c) = (a∨¬b∨¬c) — which is exactly why the definition type sets
// `resolve_gate`. That resolvent's presence is asserted below; without it the
// elimination would be unsound, and no other gate type in this file can test
// the code path that emits it.
//
// Four antecedent binaries per side put the full cross product (36 pairs, 34
// non-tautological) past base+bound = 12+16 = 28, so E1 declines with the
// definition type masked off; the gate charges 20 pairs (18 resolvents) and it
// fires.
static void test_gate_definition_fires() {
  auto build = [](Kernel &k) {
    kernel_reserve_vars(k, 12); // x=1, a=2, b=3, c=4, ant p=5..8, q=9..12
    kernel_add_binary_clause(k, internalize(1), internalize(2)); // (x∨a)
    kernel_add_binary_clause(k, internalize(1), internalize(3)); // (x∨b)
    const int g1[3] = {-1, -2, 4}, g2[3] = {-1, -3, -4};
    for (const int *g : {g1, g2}) {
      Lit ls[3] = {internalize(g[0]), internalize(g[1]), internalize(g[2])};
      kernel_add_large_clause(k, ls, 3, false, 0);
    }
    for (int p = 5; p <= 8; p++)
      kernel_add_binary_clause(k, internalize(1), internalize(p));
    for (int q = 9; q <= 12; q++)
      kernel_add_binary_clause(k, internalize(-1), internalize(q));
  };
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  const uint64_t x = lit_instance(internalize(1));
  { // every SYNTACTIC type on, definitions off → nothing matches, bound refuses
    Kernel k;
    k.bve_gate_mask = ~BVE_GATE_DEFINITION;
    build(k);
    assert(vt.apply_instance(k, x) == ApplyResult::NoOp);
    assert(k.stats.bve_gates_equiv == 0 && k.stats.bve_gates_and_or == 0 &&
           k.stats.bve_gates_ite == 0 && k.stats.bve_gates_xor == 0 &&
           "no syntactic matcher may claim this shape");
    assert(k.stats.bve_definitions_checked == 0); // the mask really gates it
  }
  { // definitions on (and NOTHING else) → the sub-solver extracts the gate
    Kernel k;
    k.bve_gate_mask = BVE_GATE_DEFINITION;
    build(k);
    assert(vt.apply_instance(k, x) == ApplyResult::Ok);
    assert(k.stats.bve_definitions_checked == 1);
    assert(k.stats.bve_gates_definition == 1);
    assert(k.stats.bve_definition_budget == 0); // the tick ceiling did not bind
    // 36 pairs, 16 of them antecedent×antecedent and skipped
    assert(k.stats.bve_gate_pairs_saved == 16);
    assert(!has_binary(k, internalize(1), internalize(5))); // x eliminated
    assert(has_binary(k, internalize(2), internalize(9)));  // gate×ant (a∨q₁)
    assert(!has_binary(k, internalize(5), internalize(9))); // ant×ant skipped
    // the resolve_gate witness: a gate×gate resolvent that is NOT a tautology
    assert(has_large(k, {2, -3, -4}) && "gate x gate resolvent (a∨¬b∨¬c)");
  }
}

// R10 · MODEL RECONSTRUCTION through a definition-derived elimination, the
// obligation E1's witness push carries (rule-dev DoD #4). Same exhaustive shape
// as test_cce's: random formulas seeded with definitional gadgets, BVE run with
// ONLY the definition type enabled so every gate came from the sub-solver, then
// EVERY model of the surviving formula extended through ρ and checked against
// the ORIGINAL clauses. The interesting models are the ones that genuinely
// needed repairing; `repaired` counts them, so a green run over zero of them
// cannot pass for evidence.
static void test_definition_reconstruction_exhaustive() {
  const uint32_t nvars = 8;
  uint64_t rng = 0x243f6a8885a308d3ull;
  const auto next = [&rng]() {
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return rng;
  };
  uint64_t gates = 0, eliminated = 0, models = 0, repaired = 0, trials = 0;
  for (int trial = 0; trial < 400; trial++) {
    std::vector<std::vector<int>> f;
    // a definitional gadget on a random pivot, in the shape above: the reduced
    // lists {a, b, ¬a∨c, ¬b∨¬c} are UNSAT, so x has a definition whenever the
    // four clauses survive. Random clauses around it supply the antecedents.
    std::vector<int> v(nvars);
    for (uint32_t i = 0; i < nvars; i++)
      v[i] = (int)i + 1;
    for (uint32_t i = nvars; i > 1; i--)
      std::swap(v[i - 1], v[next() % i]);
    const int x = v[0], a = v[1], b = v[2], c = v[3];
    f.push_back({x, a});
    f.push_back({x, b});
    f.push_back({-x, -a, c});
    f.push_back({-x, -b, -c});
    const int nclauses = 4 + (int)(next() % 10);
    for (int i = 0; i < nclauses; i++) {
      std::vector<int> cl;
      const size_t want = 2 + (size_t)(next() % 2);
      while (cl.size() < want) {
        const int w = 1 + (int)(next() % nvars);
        bool dup = false;
        for (const int e : cl)
          dup |= (e == w || e == -w);
        if (!dup)
          cl.push_back((next() & 1) ? w : -w);
      }
      f.push_back(cl);
    }
    Kernel k;
    k.bve_gate_mask = BVE_GATE_DEFINITION; // every gate comes from the sub-solver
    kernel_reserve_vars(k, nvars);
    for (const auto &cl : f) {
      std::vector<Lit> ls;
      for (const int e : cl)
        ls.push_back(internalize(e));
      if (ls.size() == 2)
        kernel_add_binary_clause(k, ls[0], ls[1]);
      else
        kernel_add_large_clause(k, ls.data(), (uint32_t)ls.size(), false, 0);
    }
    if (k.inconsistent)
      continue;
    const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
    const uint64_t before = k.stats.bve_gates_definition;
    for (uint32_t w = 0; w < nvars; w++)
      if (vt.apply_instance(k, lit_instance(internalize((int)w + 1))) ==
          ApplyResult::Unsat)
        break;
    if (k.inconsistent)
      continue;
    gates += k.stats.bve_gates_definition - before;
    eliminated += k.stats.bve_eliminated;
    if (!k.stats.bve_eliminated)
      continue;
    trials++;

    // what survives: root units, live binaries, live large clauses
    std::vector<std::vector<int>> surviving;
    for (uint32_t i = 0; i < k.trail.size(); i++)
      surviving.push_back({externalize(k.trail[i])});
    for (Lit l = 0; l < 2 * k.nvars; l++)
      for (const WatchEntry ws : k.watches.entries(l))
        if (ws.binary() && ws.other() > l)
          surviving.push_back({externalize(l), externalize(ws.other())});
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *cl = clause_at(k.arena, ref);
      if (!cl->garbage()) {
        std::vector<int> lits;
        for (const Lit l : *cl)
          lits.push_back(externalize(l));
        surviving.push_back(lits);
      }
      ref += clause_words(cl->size);
    }

    const auto sat = [&k](const std::vector<int> &cl) {
      for (const int e : cl)
        if (k.values[internalize(e)] > 0)
          return true;
      return false;
    };
    const std::vector<int8_t> saved(k.values.begin(), k.values.end());
    for (uint32_t bits = 0; bits < (1u << nvars); bits++) {
      for (uint32_t w = 0; w < nvars; w++) {
        const Lit p = internalize((int)w + 1);
        const int8_t val = (bits >> w) & 1 ? 1 : -1;
        k.values[p] = val;
        k.values[negate(p)] = (int8_t)-val;
      }
      bool ok = true;
      for (const auto &cl : surviving)
        ok &= sat(cl);
      if (!ok)
        continue;
      models++;
      bool needed = false;
      for (const auto &cl : f)
        needed |= !sat(cl);
      repaired += needed;
      k.model_extended = false;
      kernel_extend_model(k);
      for (const auto &cl : f)
        if (!sat(cl)) {
          std::printf("trial %d: reconstruction left a clause unsatisfied:",
                      trial);
          for (const int e : cl)
            std::printf(" %d", e);
          std::printf("\n");
          assert(false);
        }
    }
    std::copy(saved.begin(), saved.end(), k.values.begin());
  }
  std::printf("  definition reconstruction: %llu trials, %llu gates, "
              "%llu eliminated, %llu models (%llu needed repair)\n",
              (unsigned long long)trials, (unsigned long long)gates,
              (unsigned long long)eliminated, (unsigned long long)models,
              (unsigned long long)repaired);
  // the leg must have gone through the path it claims to cover
  assert(gates > 0 && "no definition was ever extracted");
  assert(repaired > 0 && "no model ever needed rho: the check was vacuous");
}

// E1d: gate-type selection via Kernel::bve_gate_mask. The AND/OR-gate variable
// of test_gate_and_or_fires eliminates only through the gate reduction (its full
// cross product exceeds the bound). Disabling BVE_GATE_AND_OR must therefore turn
// the same elimination into a NoOp; re-enabling it restores the Ok. Proves the
// mask actually gates detection (and that a disabled type is a clean fallback).
static void test_gate_mask() {
  auto build_and_or = [](Kernel &k) {
    kernel_reserve_vars(k, 15); // x=1, a=2..7, p=8..11, q=12..15
    Lit base[7] = {internalize(-1), internalize(2), internalize(3),
                   internalize(4),  internalize(5), internalize(6),
                   internalize(7)};
    kernel_add_large_clause(k, base, 7, false, 0);
    for (int i = 2; i <= 7; i++)
      kernel_add_binary_clause(k, internalize(1), internalize(-i));
    for (int p = 8; p <= 11; p++)
      kernel_add_binary_clause(k, internalize(1), internalize(p));
    for (int q = 12; q <= 15; q++)
      kernel_add_binary_clause(k, internalize(-1), internalize(q));
  };
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  const uint64_t x = lit_instance(internalize(1));
  { // AND/OR disabled → full cross product exceeds the bound → NoOp
    Kernel k;
    k.bve_gate_mask = BVE_GATE_EQUIV; // AND/OR off
    build_and_or(k);
    assert(vt.apply_instance(k, x) == ApplyResult::NoOp);
  }
  { // AND/OR enabled → the gate reduction fires
    Kernel k;
    k.bve_gate_mask = BVE_GATE_AND_OR; // AND/OR on (equivalence off)
    build_and_or(k);
    assert(vt.apply_instance(k, x) == ApplyResult::Ok);
  }
}

// RETRY FILTER (Kernel::bve_retry, kissat flags->eliminate): a variable BVE
// already attempted against an unchanged neighbourhood leaves the schedule, and
// any change to that neighbourhood puts it back. N1 is untouched: the payload
// surface narrows, the API does not — apply_instance still accepts the filtered
// variable.
static void test_retry_filter() {
  Kernel k;
  kernel_reserve_vars(k, 14); // the bound-skip formula: x=1, a=2..7, b=8..13
  for (int i = 2; i <= 7; i++)
    kernel_add_binary_clause(k, internalize(1), internalize(i));
  for (int j = 8; j <= 13; j++)
    kernel_add_binary_clause(k, internalize(-1), internalize(j));
  const Var x = var_of(internalize(1));
  // the rule's ADVICE ("worth re-attempting?"), which the POLICY consults. The
  // instance surface deliberately does NOT narrow with it — see `offered`.
  auto worth_retry = [&](Var v) {
    kernel_bve_retry_sync_fixed(k);
    return k.bve_retry[v] != 0;
  };
  // N1: whatever the advice says, the enumeration still OFFERS the variable, so
  // RandomPolicy and a learned policy keep the full action space.
  auto offered = [&](Var v) {
    std::vector<Var> cand;
    bve_schedule(k, cand);
    for (const Var c : cand)
      if (c == v)
        return true;
    return false;
  };
  assert(worth_retry(x)); // fresh variable: retry-worthy
  assert(offered(x));
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::NoOp);   // 36 resolvents > 6+6+16: bound exceeded
  assert(!worth_retry(x));     // attempted, nothing changed: not worth a retry
  assert(offered(x));          // ...but STILL offered by instances() (N1)
  // ...but the rule itself is still applicable to it via the API (N1): the same
  // NoOp, computed, not assumed.
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::NoOp);
  assert(!worth_retry(x));
  // a NEW IRREDUNDANT clause on x is new material: retry. (var 14 is
  // otherwise unused, so this adds an occurrence without forming a gate.)
  // DEVIATION: kissat marks on REMOVAL only. That variant was measured here
  // and is negative; see kernel.cpp kernel_add_large_clause.
  kernel_add_binary_clause(k, internalize(1), internalize(14));
  assert(worth_retry(x));
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::NoOp);
  assert(!worth_retry(x));
  // deleting one of x's clauses is new material too
  assert(kernel_delete_binary(k, internalize(1), internalize(14)));
  assert(worth_retry(x));
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::NoOp);
  assert(!worth_retry(x));
  // a REDUNDANT clause on x is not: BVE never resolves it, and marking on
  // learned-clause traffic would defeat the filter
  const Lit r[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, r, 3, /*redundant=*/true, /*glue=*/3);
  assert(!worth_retry(x));
  // fixing a NEIGHBOUR changes what BVE computes for x (its clauses become
  // delete-only / lose the falsified literal), so x must be retried
  kernel_assign_unit(k, internalize(-2));
  assert(worth_retry(x));
}

int main() {
  test_guard();
  test_occurrences();
  test_occurrences_skip_redundant();
  test_schedule_order();
  test_schedule_excludes();
  test_instance_surface();
  test_eliminate_basic();
  test_eliminate_reconstruction();
  test_bound_skip();
  test_gate_and_or_fires();
  test_gate_equivalence_substitution();
  test_gate_ite_fires();
  test_gate_xor_fires();
  test_gate_definition_fires();
  test_definition_reconstruction_exhaustive();
  test_gate_mask();
  test_retry_filter();
  puts("test_bve OK");
  return 0;
}
