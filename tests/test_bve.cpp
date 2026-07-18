// E1a BVE: the rule skeleton, the round-local occurrence index, and candidate
// scheduling (no elimination yet — apply is a NoOp, like V3a shipped
// detection-only). These tests pin the index contents and the schedule order.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/bve/bve.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

// guard: pure quiescent-root precondition (level 0, not refuted). No fixpoint
// requirement (D-LEVEL purified rev 43).
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
  // +1 in the large clause only; ¬1 in the binary only
  assert(occ.large[internalize(1)].size() == 1);
  assert(occ.binary[internalize(1)].empty());
  assert(occ.count(internalize(1)) == 1);
  assert(occ.binary[internalize(-1)].size() == 1 &&
         occ.binary[internalize(-1)][0] == internalize(2));
  assert(occ.count(internalize(-1)) == 1);
  // +2 in both the large clause and the binary
  assert(occ.large[internalize(2)].size() == 1);
  assert(occ.binary[internalize(2)].size() == 1 &&
         occ.binary[internalize(2)][0] == internalize(-1));
  assert(occ.count(internalize(2)) == 2);
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
  puts("test_bve OK");
  return 0;
}
