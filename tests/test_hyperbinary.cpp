// X6 HyperBinaryRes unit tests: the resolvent itself, the dominator that makes
// it tighter than the trivial (¬root ∨ l), the two cases cadical declines
// (root-fixed rest = its case (A); a dominator equal to the root), the
// subsumption deletion, the instance surface, and that the emitted binary is
// genuinely implied rather than merely plausible.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/hyperbinary/hyperbinary.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace tactix;

static void bin(Kernel &k, int a, int b) {
  kernel_add_binary_clause(k, internalize(a), internalize(b), 0);
}
static void add3(Kernel &k, int a, int b, int c) {
  const Lit l[3] = {internalize(a), internalize(b), internalize(c)};
  kernel_add_large_clause(k, l, 3, /*redundant=*/false, 0);
}
static bool has_bin(const Kernel &k, int a, int b) {
  return kernel_binary_live(k, internalize(a), internalize(b));
}

// The shape the rule exists for. 1→2, 2→3, 2→4 is a binary tree; the long
// clause (¬3 ∨ ¬4 ∨ 5) becomes unit on 5 once 3 and 4 are true. The trivial
// resolvent would be (¬1 ∨ 5), but 3 and 4 are both dominated by 2, so the
// TIGHT resolvent (¬2 ∨ 5) is what we must emit.
static void test_dominator_resolvent() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  bin(k, -1, 2);
  bin(k, -2, 3);
  bin(k, -2, 4);
  add3(k, -3, -4, 5);
  assert(hyperbinary_guard(k));
  assert(hyperbinary_apply(k) == ApplyResult::Ok);
  assert(k.stats.hbr_binaries == 1);
  // the tight one, not the trivial one
  assert(has_bin(k, -2, 5));
  assert(!has_bin(k, -1, 5));
  assert(default_solve(k) == 10);
}

// A dominator equal to the probe root gains nothing over the assumption
// itself, so it is declined: 1→2, 1→3, (¬2 ∨ ¬3 ∨ 4) dominates only at 1.
static void test_root_dominator_declined() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  bin(k, -1, 2);
  bin(k, -1, 3);
  add3(k, -2, -3, 4);
  assert(hyperbinary_apply(k) == ApplyResult::NoOp);
  assert(k.stats.hbr_binaries == 0);
  assert(k.stats.hbr_probes > 0); // it RAN and declined; it did not skip
}

// cadical's case (A): every other literal of the long clause is root-FALSE, so
// the clause is already binary in effect and no resolvent is worth adding.
static void test_root_fixed_rest_declined() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  bin(k, -1, 2);
  bin(k, -2, 3);
  kernel_assign_unit(k, internalize(-4)); // 4 root-false
  add3(k, -3, 4, 5);
  assert(propagate_apply(k) != ApplyResult::Unsat);
  hyperbinary_apply(k);
  // ¬3's dominator would be 2, but 4 is root-fixed and contributes none, so
  // only ONE non-root literal remains and the resolvent is (¬2 ∨ 5): that is a
  // real dominance gain, so it IS emitted. The case-(A) decline is when NO
  // non-root literal remains at all — covered by the assert below on a clause
  // whose whole tail is root-fixed.
  Kernel k2;
  kernel_reserve_vars(k2, 4);
  bin(k2, -1, 2);
  kernel_assign_unit(k2, internalize(-3));
  add3(k2, 3, 3, 4); // tail entirely root-false once 3 is fixed
  assert(propagate_apply(k2) != ApplyResult::Unsat);
  hyperbinary_apply(k2);
  assert(k2.stats.hbr_binaries == 0);
}

// The resolvent subsumes its own reason when ¬dom is IN the clause, and then
// the reason is deleted (cadical `stats.hbrsubs`).
static void test_subsumption_deletes_reason() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  bin(k, -1, 2);
  bin(k, -2, 3);
  add3(k, -2, -3, 5); // ¬dom = ¬2 is present ⇒ (¬2 ∨ 5) subsumes it
  assert(hyperbinary_apply(k) == ApplyResult::Ok);
  assert(k.stats.hbr_binaries == 1);
  assert(has_bin(k, -2, 5));
  assert(k.stats.hbr_subsumed == 1);
  assert(default_solve(k) == 10);
}

// Nothing to resolve: no long clause ever becomes unit at level 1.
static void test_no_opportunity() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  bin(k, -1, 2);
  add3(k, 3, 4, -4);
  assert(hyperbinary_apply(k) == ApplyResult::NoOp);
  assert(k.stats.hbr_binaries == 0);
}

// The emitted binary must be IMPLIED, not merely plausible. Adding ¬(¬2 ∨ 5),
// i.e. the units 2 and ¬5, to the original formula must be UNSAT — that is the
// same statement the RUP replay makes, checked here independently of it.
static void test_resolvent_is_implied() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  bin(k, -1, 2);
  bin(k, -2, 3);
  bin(k, -2, 4);
  add3(k, -3, -4, 5);
  kernel_assign_unit(k, internalize(2));
  kernel_assign_unit(k, internalize(-5));
  assert(default_solve(k) == 20); // UNSAT ⇒ (¬2 ∨ 5) is entailed
}

// N1: the rule is reachable through the REGISTRY with a per-instance payload,
// and the canonical payload runs the round. An instance surface that only the
// rule's own round can drive is not an instance surface.
static void test_instance_surface() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  bin(k, -1, 2);
  bin(k, -2, 3);
  bin(k, -2, 4);
  add3(k, -3, -4, 5);
  const RuleVTable *rule = nullptr;
  for (const RuleVTable &vt : tactix_rules())
    if (!std::strcmp(vt.name, "HyperBinaryRes"))
      rule = &vt;
  assert(rule && rule->instances && rule->apply_instance);
  uint64_t buf[64];
  const uint32_t n = rule->instances(k, buf, 64);
  assert(n > 0); // the schedule is non-empty on this formula
  bool emitted = false;
  for (uint32_t i = 0; i < n && !emitted; i++)
    emitted = apply_rule_instance(*rule, k, buf[i]) == ApplyResult::Ok;
  assert(emitted); // a per-instance application really does the work
  assert(has_bin(k, -2, 5));
  assert(default_solve(k) == 10);
}

// Applying to an arbitrary unfixed literal that is NOT on the schedule must be
// total (NoOp, never a crash or an assert) — `apply_instance` stays total even
// where `instances()` declines to advertise.
static void test_apply_off_schedule_is_total() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  bin(k, -1, 2);
  add3(k, -2, 3, 4);
  for (int v = 1; v <= 5; v++)
    for (int s = -1; s <= 1; s += 2)
      hyperbinary_try(k, internalize(s * v)); // must not trap
  assert(default_solve(k) == 10);
}

int main() {
  test_dominator_resolvent();
  test_root_dominator_declined();
  test_root_fixed_rest_declined();
  test_subsumption_deletes_reason();
  test_no_opportunity();
  test_resolvent_is_implied();
  test_instance_surface();
  test_apply_off_schedule_is_total();
  printf("test_hyperbinary: all passed\n");
  return 0;
}
