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

// N1 instance surface: candidate variables are enumerated; apply / apply_instance
// are total NoOps (no elimination in E1a).
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
  // E1a: everything is a NoOp
  assert(vt.apply(k) == ApplyResult::NoOp);
  assert(vt.apply_instance(k, INSTANCE_CANONICAL) == ApplyResult::NoOp);
  assert(vt.apply_instance(k, lit_instance(internalize(99))) ==
         ApplyResult::NoOp); // out of range
}

int main() {
  test_guard();
  test_occurrences();
  test_occurrences_skip_redundant();
  test_schedule_order();
  test_schedule_excludes();
  test_instance_surface();
  puts("test_bve OK");
  return 0;
}
