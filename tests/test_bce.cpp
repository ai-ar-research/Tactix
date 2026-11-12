// BCE (rule X1). Deleting a blocked clause preserves satisfiability only, so
// every original clause must still hold after `kernel_extend_model`.
#include "unit_test.hpp"
#include "kernel/kernel.hpp"
#include "rule_test_utils.hpp"
#include "clause_load_support.hpp"
#include "session/query_projection.hpp"
#include "policies/default_policy.hpp"
#include "rules/bce/bce.hpp"
#include "rules/propagate/propagate.hpp"

#include <csignal>

static void binaries(Kernel &k, uint32_t vars,
                     std::initializer_list<std::pair<int, int>> clauses) {
  kernel_reserve_vars(k, vars);
  for (const auto &[a, b] : clauses) add_external_clause(k, {a, b});
}

// (1 v 2) and (-1 v -2): each clause is blocked on either of its literals.
static void blocked_pair(Kernel &k) { binaries(k, 2, {{1, 2}, {-1, -2}}); }

static void begin_query(session::QueryHistory &history,
                        std::initializer_list<int> clauses, int assumption) {
  for (int literal : clauses)
    assert(history.add(literal) == session::InputStatus::Ok);
  assert(history.assume(assumption) == session::InputStatus::Ok &&
         history.begin_query() == session::InputStatus::Ok);
}


// The only resolvent on 1 is (2 ∨ ¬2), so (1 ∨ 2) is blocked on 1.
TEST(test_blocked_binary) {
  Kernel k;
  blocked_pair(k);
  assert(bce_guard(k));
  assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
  assert(k.stats.bce_deleted == 1);
}

// The resolvent (2 ∨ 3) is not tautological, so nothing is blocked on 1 —
// without a working tautology test this case would delete.
TEST(test_not_blocked) {
  Kernel k;
  binaries(k, 3, {{1, 2}, {-1, 3}});
  assert(bce_try(k, internalize(1)) == ApplyResult::NoOp);
  assert(k.stats.bce_deleted == 0);
}

// A pure literal blocks vacuously: BCE subsumes that rule (deviation 4).
TEST(test_pure_literal) {
  Kernel k;
  binaries(k, 3, {{1, 2}, {1, 3}});
  assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
  assert(k.stats.bce_deleted == 2);
}

// Redundant clauses are not candidates (deviation 1): Reduce drops them free.
TEST(test_redundant_skipped) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {1, 2, 3}, true, 3);
  assert(bce_try(k, internalize(1)) == ApplyResult::NoOp);
  assert(k.stats.bce_deleted == 0);
}

// The reconstruction obligation, end to end through a solve.
TEST(test_model_reconstruction) {
  Kernel k;
  blocked_pair(k);
  assert(bce_apply(k) == ApplyResult::Ok);
  assert(k.stats.bce_deleted >= 1);
  assert_extended_model(k, {{1, 2}, {-1, -2}});
}

TEST(test_explicit_instance_bypasses_canonical_occurrence_limit) {
  Kernel k;
  // Only the dense blocking literal 1 (over the occurrence limit) can block.
  binaries(k, 4, {{1, 2}, {1, 3}, {-2, 4}, {-3, 4}});
  k.bce_occ_limit = 1;
  assert(bce_apply(k) == ApplyResult::NoOp);
  assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
  assert(k.stats.bce_deleted == 2);
}

TEST(test_semantic_instance_deletes_one_clause) {
  Kernel k;
  binaries(k, 3, {{1, 2}, {1, 3}});
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

TEST(test_semantic_negative_control) {
  Kernel k;
  binaries(k, 3, {{1, 2}, {-1, 3}});
  const RuleVTable &rule = tactix_rules()[tactix_rule_id("BCE")];
  uint64_t choices[8];
  assert(rule.semantic_instances(k, choices, 8) == 4);
  assert(rule.apply_semantic_instance(k, choices[0]) == ApplyResult::NoOp);
  assert(k.stats.bce_deleted == 0);
  assert(kernel_binary_live(k, internalize(1), internalize(2)));
}

// Reconstruction regression: root propagation assigns 39, 9, 33, so the two
// ternaries carry an assigned literal while (19 ∨ 4) does not.
TEST(test_regression_fuzz_2204) {
  const Cnf orig = {{19, 4}, {33, -9}, {-34, -33, -4}, {34, -33, 4}, {39, 9}, {-39}};
  Kernel k;
  load_cnf(k, 39, orig);
  bce_apply(k);
  assert_extended_model(k, orig);
}

// N1: the touched-epoch filter is policy-only. With the watermark advanced the
// round declines, but `instances()` and `apply_instance` stay complete.
TEST(test_touch_filter_is_policy_only) {
  Kernel k;
  blocked_pair(k);
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
  binaries(k2, 3, {{1, 2}, {-1, 3}});
  k2.bce_touch_mark = k2.touch_epoch;
  assert(!kernel_changed_since(k2, var_of(internalize(1)),
                               k2.bce_touch_mark));
  kernel_delete_binary(k2, internalize(-1), internalize(3));
  assert(kernel_changed_since(k2, var_of(internalize(1)),
                              k2.bce_touch_mark) &&
         "removing an opposite occurrence is exactly what blocks a clause");
}

// Both interrupt sites, each entry point driven separately.
TEST(test_interrupt) {
  volatile sig_atomic_t stop = 1;
  // (1) the round loop: cut at the first literal, so nothing is deleted
  {
    Kernel k;
    blocked_pair(k);
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
    blocked_pair(k);
    KernelStopPublisher pub(k, &stop);
    assert(bce_try(k, internalize(1)) == ApplyResult::NoOp);
    assert(k.stats.bce_deleted == 0);
    assert(k.stats.bce_interrupted == 1);
  }
  {
    Kernel k;
    blocked_pair(k);
    assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
    assert(k.stats.bce_deleted == 1 && k.stats.bce_interrupted == 0);
  }
}

TEST(test_query_domain_keeps_complete_round_watermark) {
  using namespace tactix::session;
  QueryHistory history;
  begin_query(history, {1, 2, 0, -1, 3, 0}, 1);
  QueryProjection projection(history, history.active_query()->id);
  Kernel k;
  binaries(k, 3, {{1, 2}, {-1, 3}, {-2, -3}});
  k.bce_preserve_query = true;
  // Establish a real complete-round watermark on a formula with no blocker.
  assert(bce_apply(k) == ApplyResult::NoOp);
  const uint64_t watermark = k.bce_touch_mark;
  assert(watermark == k.touch_epoch && !k.stats.bce_deleted);
  const uint64_t skipped = k.stats.bce_mark_skipped;
  assert(bce_apply(k) == ApplyResult::NoOp &&
         k.stats.bce_mark_skipped == skipped + 6);
  // Removing an opposite occurrence creates pure blockers on 2 and 3,
  // while variable 1 remains unchanged and can still be skipped.
  assert(kernel_delete_binary(k, internalize(-2), internalize(-3)));
  k.query_projection = &projection;
  assert(bce_apply(k) == ApplyResult::Ok);
  assert(k.stats.bce_deleted == 2 && k.bce_touch_mark == watermark &&
         k.stats.bce_mark_skipped > skipped + 6 && !projection.snapshot().restore);
  k.query_projection = nullptr;
  assert(default_solve(k) == 10);
  kernel_extend_model(k);
  // Reconstruction may leave a witness false when its clause is already true.
  const auto &model = k.model_values;
  assert(model.size() >= 6);
  assert(model[internalize(1)] > 0 || model[internalize(2)] > 0);
  assert(model[internalize(-1)] > 0 || model[internalize(3)] > 0);
}

TEST(test_budgeted_query_bce_reaches_unchanged_prefix_tail) {
  using namespace tactix::session;
  QueryHistory history;
  begin_query(history, {1, 2, 0, -1, 3, 0, -2, -3, 0, 4, 5, 0}, 1);
  QueryProjection projection(history, history.active_query()->id);
  Kernel k;
  kernel_reserve_vars(k, 5);
  for (const auto &[a, b] : {std::pair{1, 2}, {-1, 3}, {-2, -3}, {4, 5}})
    kernel_add_binary_clause(k, internalize(a), internalize(b));
  k.query_projection = &projection;
  k.bce_preserve_query = true;
  k.work_budget = 5; // Two unsuccessful literal checks; not the whole pass.
  for (unsigned pass = 0; pass <= 2 * k.nvars && !k.stats.bce_deleted; ++pass) {
    (void)bce_apply(k);
    assert(!projection.snapshot().restore && !k.bce_touch_mark);
  }
  assert(k.stats.bce_deleted == 1 &&
         "budgeted query batches must reach the pure-literal tail");
  k.query_projection = nullptr;
}

TEST(test_query_bce_rekeys_its_completed_domain) {
  using namespace tactix::session;
  Kernel k;
  kernel_reserve_vars(k, 3);
  for (const auto &[a, b] : {std::pair{1, 2}, {-1, 3}, {-2, -3}})
    kernel_add_binary_clause(k, internalize(a), internalize(b));
  k.bce_preserve_query = true;
  for (int assumption : {1, -1}) {
    QueryHistory history;
    begin_query(history, {1, 2, 0, -1, 3, 0, -2, -3, 0}, assumption);
    QueryProjection projection(history, history.active_query()->id);
    k.query_projection = &projection;
    const uint64_t ticks = k.stats.bce_ticks;
    assert(bce_apply(k) == ApplyResult::NoOp && k.stats.bce_ticks > ticks);
    const uint64_t quiescent = k.stats.bce_ticks;
    assert(bce_apply(k) == ApplyResult::NoOp && k.stats.bce_ticks == quiescent);
    assert(!k.bce_touch_mark && !projection.snapshot().restore);
    k.query_projection = nullptr;
  }
}

TEST(test_explicit_query_bce_still_requests_repair) {
  using namespace tactix::session;
  unsigned semantic_repairs = 0;
  for (unsigned choice = 0; choice <= 4; ++choice) {
    QueryHistory history;
    begin_query(history, {1, 2, 0, -1, -2, 0}, 1);
    QueryProjection projection(history, history.active_query()->id);
    Kernel k;
    blocked_pair(k);
    k.query_projection = &projection;
    k.bce_preserve_query = true;
    if (!choice) {
      assert(bce_try(k, internalize(-1)) == ApplyResult::Ok);
      assert(projection.snapshot().restore);
    } else {
      const RuleVTable &rule = tactix_rules()[tactix_rule_id("BCE")];
      uint64_t instances[8];
      assert(rule.semantic_instances(k, instances, 8) == 4);
      assert(rule.apply_semantic_instance(k, instances[choice - 1]) == ApplyResult::Ok);
      semantic_repairs += projection.snapshot().restore;
    }
    assert(k.stats.bce_deleted == 1);
    k.query_projection = nullptr;
  }
  assert(semantic_repairs > 0); // semantic requests keep the full legal surface
}
