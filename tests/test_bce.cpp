// BCE (rule X1). Deleting a blocked clause preserves satisfiability only, so
// every original clause must still hold after `kernel_extend_model`.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/bce/bce.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <csignal>
#include <cstdio>
#include <vector>

using namespace tactix;

// The only resolvent on 1 is (2 ∨ ¬2), so (1 ∨ 2) is blocked on 1.
static void test_blocked_binary() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  assert(bce_guard(k));
  assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
  assert(k.stats.bce_deleted == 1);
}

// The resolvent (2 ∨ 3) is not tautological, so nothing is blocked on 1 —
// without a working tautology test this case would delete.
static void test_not_blocked() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  assert(bce_try(k, internalize(1)) == ApplyResult::NoOp);
  assert(k.stats.bce_deleted == 0);
}

// A pure literal blocks vacuously: BCE subsumes that rule (deviation 4).
static void test_pure_literal() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2)); // 1 is pure
  kernel_add_binary_clause(k, internalize(1), internalize(3));
  assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
  assert(k.stats.bce_deleted == 2);
}

// Redundant clauses are not candidates (deviation 1): Reduce drops them free.
static void test_redundant_skipped() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, /*redundant=*/true, 3);
  assert(bce_try(k, internalize(1)) == ApplyResult::NoOp);
  assert(k.stats.bce_deleted == 0);
}

// The reconstruction obligation, end to end through a solve.
static void test_model_reconstruction() {
  const std::vector<std::vector<int>> orig = {{1, 2}, {-1, -2}};
  Kernel k;
  kernel_reserve_vars(k, 2);
  for (const auto &cl : orig)
    kernel_add_binary_clause(k, internalize(cl[0]), internalize(cl[1]));
  assert(bce_apply(k) == ApplyResult::Ok);
  assert(k.stats.bce_deleted >= 1);
  assert(default_solve(k) == 10);
  kernel_extend_model(k);
  for (const auto &cl : orig) {
    bool sat = false;
    for (const int e : cl)
      sat |= (k.values[internalize(e)] > 0);
    assert(sat && "every original clause satisfied by the extended model");
  }
}

// Instance surface (N1): literals are the payloads.
static void test_instances() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  std::vector<Lit> lits;
  bce_literals(k, lits);
  assert(lits.size() == 4); // both polarities of both variables
  assert(bce_try(k, internalize(2)) == ApplyResult::Ok); // symmetric here
}

static void test_explicit_instance_bypasses_canonical_occurrence_limit() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(3));
  // Keep 2 and 3 from being independently pure blocked literals; 4 itself
  // exceeds the same occurrence ceiling. Thus only the deliberately dense
  // blocking literal 1 could eliminate anything in this fixture.
  kernel_add_binary_clause(k, internalize(-2), internalize(4));
  kernel_add_binary_clause(k, internalize(-3), internalize(4));
  k.bce_occ_limit = 1;
  assert(bce_apply(k) == ApplyResult::NoOp);
  assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
  assert(k.stats.bce_deleted == 2);
}

static void test_semantic_instance_deletes_one_clause() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(3));
  const RuleVTable &rule = tactix_rules()[tactix_rule_id("BCE")];
  assert(rule.semantic_instances && rule.apply_semantic_instance);
  uint64_t choices[8];
  assert(rule.semantic_instances(k, choices, 8) == 4);
  assert(rule.apply_semantic_instance(k, choices[0]) == ApplyResult::Ok);
  assert(k.stats.bce_deleted == 1 &&
         "one semantic BCE instance deleted a blocking-literal batch");
  assert(kernel_binary_live(k, internalize(1), internalize(3)) &&
         "the unselected blocked clause was deleted");
}

static void test_semantic_negative_control() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  const RuleVTable &rule = tactix_rules()[tactix_rule_id("BCE")];
  uint64_t choices[8];
  assert(rule.semantic_instances(k, choices, 8) == 4);
  assert(rule.apply_semantic_instance(k, choices[0]) == ApplyResult::NoOp);
  assert(k.stats.bce_deleted == 0);
  assert(kernel_binary_live(k, internalize(1), internalize(2)));
}

// Reconstruction regression: root propagation assigns 39, 9, 33, so the two
// ternaries carry an assigned literal while (19 ∨ 4) does not.
static void test_regression_fuzz_2204() {
  const std::vector<std::vector<int>> orig = {
      {19, 4}, {33, -9}, {-34, -33, -4}, {34, -33, 4}, {39, 9}, {-39}};
  Kernel k;
  kernel_reserve_vars(k, 39);
  for (const auto &cl : orig) {
    std::vector<Lit> l;
    for (const int e : cl)
      l.push_back(internalize(e));
    if (l.size() == 1)
      kernel_assign_unit(k, l[0]);
    else if (l.size() == 2)
      kernel_add_binary_clause(k, l[0], l[1]);
    else
      kernel_add_large_clause(k, l.data(), (uint32_t)l.size(), false, 0);
  }
  bce_apply(k);
  assert(default_solve(k) == 10);
  kernel_extend_model(k);
  for (const auto &cl : orig) {
    bool sat = false;
    for (const int e : cl)
      sat |= (k.values[internalize(e)] > 0);
    if (!sat) {
      std::printf("UNSATISFIED after reconstruction:");
      for (const int e : cl)
        std::printf(" %d", e);
      std::printf("\n");
    }
    assert(sat && "every original clause satisfied by the extended model");
  }
}

// N1: the touched-epoch filter is policy-only. With the watermark advanced the
// round declines, but `instances()` and `apply_instance` stay complete.
static void test_touch_filter_is_policy_only() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  k.bce_touch_mark = k.touch_epoch;
  const int id = tactix_rule_id("BCE");
  assert(id >= 0);
  const RuleVTable &r = tactix_rules()[id];
  assert(r.apply_instance(k, INSTANCE_CANONICAL) == ApplyResult::NoOp);
  assert(k.stats.bce_mark_skipped == 4);
  uint64_t insts[8];
  assert(r.instances(k, insts, 8) == 4); // not narrowed by the mark
  assert(r.guard(k));
  assert(r.apply_instance(k, insts[0]) == ApplyResult::Ok);

  // the deletion side re-arms BCE: a removal re-offers the literal
  Kernel k2;
  kernel_reserve_vars(k2, 3);
  kernel_add_binary_clause(k2, internalize(1), internalize(2));
  kernel_add_binary_clause(k2, internalize(-1), internalize(3));
  k2.bce_touch_mark = k2.touch_epoch;
  assert(!kernel_changed_since(k2, var_of(internalize(1)),
                               k2.bce_touch_mark));
  kernel_delete_binary(k2, internalize(-1), internalize(3));
  assert(kernel_changed_since(k2, var_of(internalize(1)),
                              k2.bce_touch_mark) &&
         "removing an opposite occurrence is exactly what blocks a clause");
}

// Both interrupt sites, each entry point driven separately.
static void test_interrupt() {
  volatile sig_atomic_t stop = 1;
  // (1) the round loop: cut at the first literal, so nothing is deleted
  {
    Kernel k;
    kernel_reserve_vars(k, 2);
    kernel_add_binary_clause(k, internalize(1), internalize(2));
    kernel_add_binary_clause(k, internalize(-1), internalize(-2));
    KernelStopPublisher pub(k, &stop);
    assert(bce_apply(k) == ApplyResult::NoOp);
    assert(k.stats.bce_deleted == 0);
    assert(k.stats.bce_interrupted == 1); // the round poll, once
    assert(k.stats.rule_interrupted == 1);
  }
  // (2) the per-literal scan, through the single-instance entry; the
  // uninterrupted run below is the control
  {
    Kernel k;
    kernel_reserve_vars(k, 2);
    kernel_add_binary_clause(k, internalize(1), internalize(2));
    kernel_add_binary_clause(k, internalize(-1), internalize(-2));
    KernelStopPublisher pub(k, &stop);
    assert(bce_try(k, internalize(1)) == ApplyResult::NoOp);
    assert(k.stats.bce_deleted == 0);
    assert(k.stats.bce_interrupted == 1);
  }
  {
    Kernel k;
    kernel_reserve_vars(k, 2);
    kernel_add_binary_clause(k, internalize(1), internalize(2));
    kernel_add_binary_clause(k, internalize(-1), internalize(-2));
    assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
    assert(k.stats.bce_deleted == 1 && k.stats.bce_interrupted == 0);
  }
}

int main() {
  test_interrupt();
  test_regression_fuzz_2204();
  test_touch_filter_is_policy_only();
  test_blocked_binary();
  test_not_blocked();
  test_pure_literal();
  test_redundant_skipped();
  test_model_reconstruction();
  test_instances();
  test_explicit_instance_bypasses_canonical_occurrence_limit();
  test_semantic_instance_deletes_one_clause();
  test_semantic_negative_control();
  std::printf("test_bce: all passed\n");
  return 0;
}
