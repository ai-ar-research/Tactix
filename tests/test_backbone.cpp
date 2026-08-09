// Backbone unit tests: binary-implication failed-literal detection, the
// binary-only restriction that distinguishes it from Probe, and the
// instance surface.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/backbone/backbone.hpp"
#include "rules/probe/probe.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

// Binary propagation alone conflicts, so ¬1 is derived as a root unit.
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

// The binary-only restriction: 1 fails only through the size-3 clauses, so
// Backbone finds nothing where Probe finds 1 failed — opposite by design.
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
    assert(k.stats.backbone_probed >= 1); // it tried: 1 and ¬2 are BIG heads
    assert(k.trail.empty());
  }
  { // identical formula: full-propagation Probe does find 1 failed
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

static void test_binary_meter_charges_scanned_prefix() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  const Lit large[3] = {internalize(-1), internalize(3), internalize(4)};
  for (unsigned i = 0; i < 40; i++)
    kernel_add_large_clause(k, large, 3, false, 0);
  kernel_collect_garbage(k);
  assert(k.binaries_watched_first);

  const uint64_t before = k.stats.ticks;
  kernel_assign_decision(k, internalize(1));
  assert(!kernel_propagate_binary(k));
  assert(k.value(internalize(2)) > 0);
  assert(k.stats.binprop_words == 1);
  assert(k.stats.ticks - before == 3); // one-word prefix, then an empty scan
}

// both polarities fail via binaries: UNSAT once the derived batch propagates
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
  // invalid payloads are rejected (N1: apply_instance is total); a fresh
  // kernel, since the derivation above left k non-quiescent
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
  test_binary_meter_charges_scanned_prefix();
  test_both_polarities_fail_unsat();
  test_instance_surface();
  puts("test_backbone OK");
  return 0;
}
