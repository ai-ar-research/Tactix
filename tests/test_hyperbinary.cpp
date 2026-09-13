// HyperBinaryRes unit tests: the resolvent and its dominator, the two cases
// cadical declines, the subsumption deletion, the instance surface, and
// entailment of the emitted binary.
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

// 3 and 4 are dominated by 2, so the resolvent must be the tight (¬2 ∨ 5).
static void test_dominator_resolvent() {
  Kernel k;
  k.jump_reasons = false; // isolate CaDiCaL's direct-reason HBR mechanism
  kernel_reserve_vars(k, 5);
  bin(k, -1, 2);
  bin(k, -2, 3);
  bin(k, -2, 4);
  add3(k, -3, -4, 5);
  assert(hyperbinary_guard(k));
  assert(hyperbinary_apply(k) == ApplyResult::Ok);
  assert(k.stats.hbr_binaries == 1);
  assert(has_bin(k, -2, 5)); // the tight one, not the trivial one
  assert(!has_bin(k, -1, 5));
  assert(default_solve(k) == 10);
}

// A dominator equal to the probe root gains nothing, so it is declined.
static void test_root_dominator_declined() {
  Kernel k;
  k.jump_reasons = false;
  kernel_reserve_vars(k, 4);
  bin(k, -1, 2);
  bin(k, -1, 3);
  add3(k, -2, -3, 4);
  assert(hyperbinary_apply(k) == ApplyResult::NoOp);
  assert(k.stats.hbr_binaries == 0);
  assert(k.stats.hbr_probes > 0); // it ran and declined; it did not skip
}

// cadical's case (A): a clause already binary in effect gains no resolvent.
static void test_root_fixed_rest_declined() {
  Kernel k;
  k.jump_reasons = false;
  kernel_reserve_vars(k, 5);
  bin(k, -1, 2);
  bin(k, -2, 3);
  kernel_assign_unit(k, internalize(-4)); // 4 root-false
  add3(k, -3, 4, 5);
  assert(propagate_apply(k) != ApplyResult::Unsat);
  hyperbinary_apply(k);
  // a non-root literal still remains here, so the resolvent is emitted; the
  // decline applies only when none remains, as in k2 below
  Kernel k2;
  k2.jump_reasons = false;
  kernel_reserve_vars(k2, 4);
  bin(k2, -1, 2);
  kernel_assign_unit(k2, internalize(-3));
  add3(k2, 3, 3, 4); // tail entirely root-false once 3 is fixed
  assert(propagate_apply(k2) != ApplyResult::Unsat);
  hyperbinary_apply(k2);
  assert(k2.stats.hbr_binaries == 0);
}

// With ¬dom in the clause the resolvent subsumes its reason, which is then
// deleted (cadical `stats.hbrsubs`).
static void test_subsumption_deletes_reason() {
  Kernel k;
  k.jump_reasons = false;
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
  k.jump_reasons = false;
  kernel_reserve_vars(k, 4);
  bin(k, -1, 2);
  add3(k, 3, 4, -4);
  assert(hyperbinary_apply(k) == ApplyResult::NoOp);
  assert(k.stats.hbr_binaries == 0);
}

// The emitted binary must be entailed: adding its negation is UNSAT. Same
// statement as the RUP replay, checked independently of it.
static void test_resolvent_is_implied() {
  Kernel k;
  k.jump_reasons = false;
  kernel_reserve_vars(k, 5);
  bin(k, -1, 2);
  bin(k, -2, 3);
  bin(k, -2, 4);
  add3(k, -3, -4, 5);
  kernel_assign_unit(k, internalize(2));
  kernel_assign_unit(k, internalize(-5));
  assert(default_solve(k) == 20); // UNSAT ⇒ (¬2 ∨ 5) is entailed
}

// N1: a per-instance payload from the registry must really do the work — a
// surface only the rule's own round can drive is not an instance surface.
static void test_instance_surface() {
  Kernel k;
  k.jump_reasons = false;
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

// The semantic surface names individual resolvents.  Applying a later payload
// first must not shift the earlier payload to a different consequence.
static void test_individual_semantic_resolvents_keep_identity() {
  Kernel k;
  k.jump_reasons = false;
  kernel_reserve_vars(k, 8);
  bin(k, -1, 2);
  bin(k, -2, 3);
  bin(k, -2, 4);
  add3(k, -3, -4, 5);
  bin(k, -2, 6);
  bin(k, -2, 7);
  add3(k, -6, -7, 8);
  const RuleVTable &rule =
      tactix_rules()[tactix_rule_id("HyperBinaryRes")];
  assert(rule.semantic_instances && rule.apply_semantic_instance);
  uint64_t choices[64], selected[2];
  const uint32_t count = rule.semantic_instances(k, choices, 64);
  uint32_t selected_count = 0;
  for (uint32_t i = 0; i < count && selected_count < 2; ++i)
    if (static_cast<Lit>(choices[i] >> 32) == internalize(2))
      selected[selected_count++] = choices[i];
  assert(selected_count == 2);

  assert(apply_rule_semantic_instance(rule, k, selected[1]) == ApplyResult::Ok);
  assert(k.stats.hbr_binaries == 1);
  assert(apply_rule_semantic_instance(rule, k, selected[0]) == ApplyResult::Ok);
  assert(k.stats.hbr_binaries == 2);
  assert(has_bin(k, -2, 5));
  assert(has_bin(k, -2, 8));
}

// The calculus permits any dominator with the required binary paths. The
// canonical probe deliberately chooses the closest (2), but N1 must also make
// the weaker, legal dominator 1 directly reachable.
static void test_nonclosest_dominator_is_semantically_reachable() {
  Kernel k;
  k.jump_reasons = false;
  kernel_reserve_vars(k, 5);
  bin(k, -1, 2);
  bin(k, -2, 3);
  bin(k, -2, 4);
  add3(k, -3, -4, 5);
  const RuleVTable &rule =
      tactix_rules()[tactix_rule_id("HyperBinaryRes")];
  uint64_t choices[64];
  const uint32_t count = rule.semantic_instances(k, choices, 64);
  uint64_t selected = INSTANCE_CANONICAL;
  for (uint32_t i = 0; i < count; ++i)
    if (static_cast<Lit>(choices[i] >> 32) == internalize(1)) {
      selected = choices[i];
      break;
    }
  assert(selected != INSTANCE_CANONICAL);
  assert(rule.apply_semantic_instance(k, selected) == ApplyResult::Ok);
  assert(has_bin(k, -1, 5));
  assert(!has_bin(k, -2, 5));
}

static void test_semantic_resolvent_can_collapse_to_unit() {
  Kernel k;
  k.jump_reasons = false;
  kernel_reserve_vars(k, 4);
  bin(k, -1, 3);
  bin(k, -1, 4);
  add3(k, -1, -3, -4);
  const RuleVTable &rule =
      tactix_rules()[tactix_rule_id("HyperBinaryRes")];
  uint64_t choices[64];
  const uint32_t count = rule.semantic_instances(k, choices, 64);
  uint64_t selected = INSTANCE_CANONICAL;
  for (uint32_t i = 0; i < count; ++i)
    if (static_cast<Lit>(choices[i] >> 32) == internalize(1)) {
      selected = choices[i];
      break;
    }
  assert(selected != INSTANCE_CANONICAL);
  assert(rule.apply_semantic_instance(k, selected) == ApplyResult::Ok);
  assert(k.value(internalize(-1)) > 0);
  assert(k.stats.hbr_units == 1 && k.stats.hbr_binaries == 0);
}

static void test_invalid_semantic_payload_is_noop() {
  Kernel k;
  k.jump_reasons = false;
  kernel_reserve_vars(k, 2);
  const RuleVTable &rule =
      tactix_rules()[tactix_rule_id("HyperBinaryRes")];
  assert(apply_rule_semantic_instance(
             rule, k,
             hyperbinary_semantic_instance(make_lit(1, false), 17)) ==
         ApplyResult::NoOp);
  assert(k.stats.hbr_binaries == 0);
}

static void test_semantic_enumeration_observes_published_stop() {
  Kernel k;
  kernel_reserve_vars(k, 500000);
  bin(k, 1, -500000);
  const RuleVTable &rule =
      tactix_rules()[tactix_rule_id("HyperBinaryRes")];
  volatile sig_atomic_t stop = 1;
  KernelStopPublisher published(k, &stop);
  uint64_t choice = 0;
  assert(rule.semantic_instances(k, &choice, 1) == 0);
}

// `apply_instance` is total: an off-schedule literal is a NoOp, not a trap.
static void test_apply_off_schedule_is_total() {
  Kernel k;
  k.jump_reasons = false;
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
  test_individual_semantic_resolvents_keep_identity();
  test_nonclosest_dominator_is_semantically_reachable();
  test_semantic_resolvent_can_collapse_to_unit();
  test_invalid_semantic_payload_is_noop();
  test_semantic_enumeration_observes_published_stop();
  test_apply_off_schedule_is_total();
  printf("test_hyperbinary: all passed\n");
  return 0;
}
