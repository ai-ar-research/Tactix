// Probe unit tests: failed-literal unit derivation, the both-polarity
// refutation, the BIG-head candidate schedule, and the instance surface.
// probe_apply leaves a clean root with the derived units un-propagated.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/probe/probe.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>

using namespace tactix;

// Assuming 1 conflicts, so 1 is a failed literal and ¬1 becomes a root unit.
static void test_failed_literal_derives_unit() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  assert(probe_guard(k));
  assert(probe_apply(k) == ApplyResult::Ok);
  assert(k.stats.probe_failed == 1);
  assert(k.stats.probe_rounds == 1);
  assert(k.value(internalize(-1)) > 0); // asserted at root, un-propagated
  assert(k.level == 0 && !k.conflict_valid);
  assert(default_solve(k) == 10);
}

// Both polarities of 1 fail. Probing 1 forces ¬1 and skips the variable;
// propagating that unit reproduces the ¬1 failure and refutes the formula.
static void test_both_polarities_fail_unsat() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  kernel_add_binary_clause(k, internalize(1), internalize(3));
  kernel_add_binary_clause(k, internalize(1), internalize(-3));
  assert(probe_apply(k) == ApplyResult::Ok);
  // every failure lands in one inconsistent batch, refuted on propagation
  assert(k.stats.probe_failed >= 1);
  assert(propagate_apply(k) == ApplyResult::Unsat);
  assert(k.inconsistent);
}

// binaries with no failed literal → NoOp, nothing asserted.
static void test_no_failed_literal_noop() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // 1 → 2 only
  kernel_add_binary_clause(k, internalize(-2), internalize(3)); // 2 → 3
  assert(probe_apply(k) == ApplyResult::NoOp);
  assert(k.stats.probe_failed == 0);
  assert(k.trail.empty());
  assert(default_solve(k) == 10);
}

// A cascade through large clauses: exercises the conflict / reason-walk path.
static void test_failed_literal_through_large_clause() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // 1 → 2
  const Lit c[3] = {internalize(-2), internalize(-3), internalize(-1)};
  kernel_add_large_clause(k, c, 3, false, 0); // (¬2 ∨ ¬3 ∨ ¬1): 1,2 → ¬3
  const Lit d[3] = {internalize(-1), internalize(3), internalize(-2)};
  kernel_add_large_clause(k, d, 3, false, 0); // (¬1 ∨ 3 ∨ ¬2): 1,2 → 3
  assert(probe_apply(k) == ApplyResult::Ok);
  assert(k.stats.probe_failed == 1);
  assert(k.value(internalize(-1)) > 0); // ¬1 derived
  assert(default_solve(k) == 10);
}

// Only a literal whose negation carries a binary watch is a probe candidate,
// so a variable appearing solely in large clauses is never probed.
static void test_candidate_schedule() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // ¬1,2 watch bin
  const Lit c[3] = {internalize(2), internalize(3), internalize(4)};
  kernel_add_large_clause(k, c, 3, false, 0); // vars 3,4 large-only
  std::vector<Lit> cands;
  probe_candidates(k, cands);
  const bool has1 = std::find(cands.begin(), cands.end(), internalize(1)) != cands.end();
  const bool hasn2 = std::find(cands.begin(), cands.end(), internalize(-2)) != cands.end();
  assert(has1 && hasn2);
  for (const Lit p : cands) {
    const Var v = var_of(p);
    assert(v != var_of(internalize(3)) && v != var_of(internalize(4)));
  }
  // determinism: var-index ascending, positive-first
  assert(std::is_sorted(cands.begin(), cands.end()));
}

static void test_instance_surface() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  const std::vector<RuleVTable> &rules = tactix_rules();
  const int id = tactix_rule_id("Probe");
  assert(id >= 0 && rules[id].tier == RuleTier::Inprocess);
  uint64_t insts[8];
  const uint32_t n = rules[id].instances(k, insts, 8);
  assert(n >= 1); // 1 is a BIG-head candidate (¬1 has a binary watch)
  bool has_probe1 = false;
  for (uint32_t i = 0; i < n; i++)
    has_probe1 |= (insts[i] == static_cast<uint64_t>(static_cast<int64_t>(1)));
  assert(has_probe1);
  // applying the single-probe instance for literal 1 derives ¬1
  assert(rules[id].apply_instance(k, static_cast<uint64_t>(1)) == ApplyResult::Ok);
  assert(k.stats.probe_failed == 1);
  assert(k.value(internalize(-1)) > 0);
  // invalid payloads are rejected (total apply_instance, N1 contract)
  Kernel k2;
  kernel_reserve_vars(k2, 2);
  kernel_add_binary_clause(k2, internalize(-1), internalize(2));
  assert(rules[id].apply_instance(k2, static_cast<uint64_t>(999999)) ==
         ApplyResult::NoOp);
  assert(rules[id].apply_instance(k2, static_cast<uint64_t>(0)) ==
         ApplyResult::NoOp);
}

int main() {
  test_failed_literal_derives_unit();
  test_both_polarities_fail_unsat();
  test_no_failed_literal_noop();
  test_failed_literal_through_large_clause();
  test_candidate_schedule();
  test_instance_surface();
  puts("test_probe OK");
  return 0;
}
