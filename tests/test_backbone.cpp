// V2 Backbone unit tests: binary-implication failed-literal detection, the
// binary-only restriction (a failure reachable only through large clauses is
// MISSED by Backbone but found by full-propagation Probe — the V1/V2
// distinction), a no-failure NoOp, and the instance surface.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/backbone/backbone.hpp"
#include "rules/probe/probe.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

// (¬1 ∨ 2) and (¬1 ∨ ¬2): assuming 1 binary-propagates 2 and ¬2 → conflict,
// so 1 is a binary failed literal and ¬1 is derived as a root unit.
static void test_binary_failed_literal() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  assert(backbone_guard(k));
  assert(backbone_apply(k) == ApplyResult::Ok);
  assert(k.stats.backbone_failed == 1);
  assert(k.stats.backbone_rounds == 1);
  assert(k.value(internalize(-1)) > 0); // ¬1 asserted at root, un-propagated
  assert(k.level == 0 && !k.conflict_valid);
  assert(default_solve(k) == 10);
}

// The binary-only restriction (the whole point of V2 vs V1): 1 fails, but only
// through LARGE clauses — assume 1 binary-propagates 2, then the size-3
// clauses (¬1∨¬2∨3) and (¬1∨¬2∨¬3) force 3 and ¬3. Backbone's binary-only
// propagation stops after 2 and finds NOTHING; V1 Probe's full propagation
// finds 1 failed. Same formula, opposite verdict — the distinction is real.
static void test_binary_only_misses_large_clause_failure() {
  const Lit a[3] = {internalize(-1), internalize(-2), internalize(3)};
  const Lit b[3] = {internalize(-1), internalize(-2), internalize(-3)};
  {
    Kernel k;
    kernel_reserve_vars(k, 3);
    kernel_add_binary_clause(k, internalize(-1), internalize(2)); // 1 → 2
    kernel_add_large_clause(k, a, 3, false, 0);
    kernel_add_large_clause(k, b, 3, false, 0);
    assert(backbone_apply(k) == ApplyResult::NoOp); // binary-only sees nothing
    assert(k.stats.backbone_failed == 0);
    assert(k.stats.backbone_probed >= 1); // it DID try (1 and ¬2 are BIG heads)
    assert(k.trail.empty());
  }
  { // identical formula, full-propagation Probe DOES find 1 failed
    Kernel k;
    kernel_reserve_vars(k, 3);
    kernel_add_binary_clause(k, internalize(-1), internalize(2));
    kernel_add_large_clause(k, a, 3, false, 0);
    kernel_add_large_clause(k, b, 3, false, 0);
    assert(probe_apply(k) == ApplyResult::Ok);
    assert(k.stats.probe_failed >= 1);
    assert(k.value(internalize(-1)) > 0);
  }
}

// binaries with no failed literal → NoOp.
static void test_no_failed_literal_noop() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // 1 → 2
  kernel_add_binary_clause(k, internalize(-2), internalize(3)); // 2 → 3
  assert(backbone_apply(k) == ApplyResult::NoOp);
  assert(k.stats.backbone_failed == 0);
  assert(k.trail.empty());
  assert(default_solve(k) == 10);
}

// both polarities fail via binaries → UNSAT through M1's cascade of the batch.
static void test_both_polarities_fail_unsat() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  kernel_add_binary_clause(k, internalize(1), internalize(3));
  kernel_add_binary_clause(k, internalize(1), internalize(-3));
  assert(backbone_apply(k) == ApplyResult::Ok);
  assert(k.stats.backbone_failed >= 1);
  assert(propagate_apply(k) == ApplyResult::Unsat);
  assert(k.inconsistent);
}

static void test_instance_surface() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  const std::vector<RuleVTable> &rules = tactix_rules();
  const int id = tactix_rule_id("Backbone");
  assert(id >= 0 && rules[id].tier == RuleTier::Inprocess);
  uint64_t insts[8];
  const uint32_t n = rules[id].instances(k, insts, 8);
  assert(n >= 1);
  assert(rules[id].apply_instance(k, static_cast<uint64_t>(1)) ==
         ApplyResult::Ok); // probing literal 1 derives ¬1
  assert(k.stats.backbone_failed == 1);
  assert(k.value(internalize(-1)) > 0);
  // invalid payloads are rejected (total apply_instance, N1 contract) — on a
  // fresh quiescent kernel, since the derivation above left k non-quiescent
  Kernel k2;
  kernel_reserve_vars(k2, 2);
  kernel_add_binary_clause(k2, internalize(-1), internalize(2));
  assert(rules[id].apply_instance(k2, static_cast<uint64_t>(999999)) ==
         ApplyResult::NoOp);
  assert(rules[id].apply_instance(k2, static_cast<uint64_t>(0)) ==
         ApplyResult::NoOp);
}

int main() {
  test_binary_failed_literal();
  test_binary_only_misses_large_clause_failure();
  test_no_failed_literal_noop();
  test_both_polarities_fail_unsat();
  test_instance_surface();
  puts("test_backbone OK");
  return 0;
}
