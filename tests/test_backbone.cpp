// Backbone unit tests: binary-implication failed-literal detection, the
// binary-only restriction that distinguishes it from Probe, and the
// instance surface.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rules/backbone/backbone.hpp"
#include "rules/probe/probe.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>
#include <string>

#include <unistd.h>

using namespace tactix;

// Binary propagation alone conflicts, so ¬1 is derived as a root unit.
static void test_binary_failed_literal() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  assert(backbone_guard(k));
  assert(backbone_apply(k) == ApplyResult::Ok);
  assert(k.stats.backbone_computations == 1);
  assert(k.stats.backbone_failed == 1);
  assert(k.stats.backbone_rounds >= 1);
  assert(k.value(internalize(-1)) > 0); // ¬1 asserted at root, un-propagated
  assert(k.level == 0 && !k.conflict_valid);
  assert(default_solve(k) == 10);
}

static void test_canonical_computation_counter_advances() {
  Kernel k;
  kernel_reserve_vars(k, 1);
  assert(backbone_apply(k) == ApplyResult::NoOp);
  assert(backbone_apply(k) == ApplyResult::NoOp);
  assert(k.stats.backbone_computations == 2);
}

// The conflict-causing probe need not be the strongest failed literal.  Here
// 1 implies 2 and 2 forks to both 3 and ¬3.  Binary-UIP analysis learns ¬2;
// replaying it at root then propagates ¬1.  Independent probes learn only ¬1
// from the same first conflict and miss the reference algorithm's UIP unit.
static void test_chained_binary_uip_learns_internal_predecessor() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(3));
  kernel_add_binary_clause(k, internalize(-2), internalize(-3));
  assert(backbone_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(-2)) > 0);
  assert(k.value(internalize(-1)) > 0);
  assert(k.stats.backbone_failed == 1);
  assert(k.level == 0 && !k.conflict_valid);
}

static void test_chained_binary_uip_lrat_order() {
  const std::string path = "/tmp/tactix-backbone-" +
                           std::to_string(static_cast<long>(getpid())) +
                           ".lrat";
  {
    Kernel k;
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::Lrat));
    k.proof = &proof;
    kernel_reserve_vars(k, 3);
    kernel_add_binary_clause(k, internalize(-1), internalize(2), 1);
    kernel_add_binary_clause(k, internalize(-2), internalize(3), 2);
    kernel_add_binary_clause(k, internalize(-2), internalize(-3), 3);
    k.next_clause_id = 3;
    assert(backbone_apply(k) == ApplyResult::Ok);
    proof.flush();
    k.proof = nullptr;
  }
  FILE *file = fopen(path.c_str(), "rb");
  assert(file);
  char bytes[64] = {};
  const size_t size = fread(bytes, 1, sizeof bytes, file);
  fclose(file);
  remove(path.c_str());
  assert(std::string(bytes, size) == "4 -2 0 2 3 0\n");
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
  assert(k.watches.heads[internalize(-1)].binaries_first);

  const uint64_t before = k.stats.ticks;
  kernel_assign_decision(k, internalize(1));
  assert(!kernel_propagate_binary(k));
  assert(k.value(internalize(2)) > 0);
  assert(k.stats.binprop_words == 1);
  assert(k.stats.ticks - before == 3); // one-word prefix, then an empty scan
}

static void test_binary_order_is_per_list() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit large[3] = {internalize(-1), internalize(3), internalize(4)};
  kernel_add_large_clause(k, large, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));

  const Lit affected = internalize(-1);
  const Lit unaffected = internalize(-5);
  assert(!k.watches.heads[affected].binaries_first);
  assert(k.watches.heads[unaffected].binaries_first);

  kernel_assign_decision(k, internalize(1));
  assert(!kernel_propagate_binary(k));
  assert(k.value(internalize(2)) > 0); // binary behind the large was scanned
}

// both polarities fail via binaries: UNSAT once the derived batch propagates
static void test_both_polarities_fail_unsat() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  kernel_add_binary_clause(k, internalize(1), internalize(3));
  kernel_add_binary_clause(k, internalize(1), internalize(-3));
  assert(backbone_apply(k) == ApplyResult::Unsat);
  assert(k.stats.backbone_failed >= 1);
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
  test_canonical_computation_counter_advances();
  test_chained_binary_uip_learns_internal_predecessor();
  test_chained_binary_uip_lrat_order();
  test_binary_only_misses_large_clause_failure();
  test_no_failed_literal_noop();
  test_binary_meter_charges_scanned_prefix();
  test_binary_order_is_per_list();
  test_both_polarities_fail_unsat();
  test_instance_surface();
  puts("test_backbone OK");
  return 0;
}
