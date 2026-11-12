#undef NDEBUG
#include "kernel/kernel.hpp"
#include "session/query_projection.hpp"
#include "policies/default_policy.hpp"
#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/bce/bce.hpp"
#include "rules/bve/bve.hpp"
#include "rules/backbone/backbone.hpp"
#include "rules/cce/cce.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "session/query_history.hpp"
#include "strategy/rule_parameters.hpp"
#include "strategy/scoped_rule_parameters.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace tactix;
using Cnf = std::vector<std::vector<int>>;

static void load(Kernel &k, unsigned variables, const Cnf &formula) {
  kernel_reserve_vars(k, variables);
  for (const auto &clause : formula) {
    std::vector<Lit> lits;
    for (int literal : clause) lits.push_back(internalize(literal));
    if (lits.size() == 1) kernel_assign_unit(k, lits[0]);
    else if (lits.size() == 2) kernel_add_binary_clause(k, lits[0], lits[1]);
    else kernel_add_large_clause(k, lits.data(), lits.size(), false, 0);
  }
}

static void check_model(Kernel &k, const Cnf &formula) {
  assert(default_solve(k) == 10);
  kernel_extend_model(k);
  for (const auto &clause : formula) {
    bool satisfied = false;
    for (int literal : clause) {
      const Lit external = internalize(literal);
      assert(external < k.model_values.size());
      satisfied |= k.model_values[external] > 0;
    }
    assert(satisfied && "reconstructed model must satisfy original clauses");
  }
}

static void test_limit() {
  const Cnf formula = {{1, 2}, {1, 3}, {-2, 4}, {-3, 4}};
  for (unsigned relaxed : {0u, 2u}) {
    Kernel k;
    load(k, 4, formula);
    strategy::install_rule_parameters(k, strategy::BceParameters{1, false});
    assert(bce_apply(k) == ApplyResult::NoOp);
    const auto ticks = k.stats.bce_ticks, mark = k.bce_touch_mark;
    strategy::install_rule_parameters(k, strategy::BceParameters{1, false});
    assert(k.bce_touch_mark == mark && bce_apply(k) == ApplyResult::NoOp &&
           k.stats.bce_ticks == ticks && "identical profiles retain filtering");
    strategy::install_rule_parameters(k, strategy::BceParameters{relaxed, false});
    assert(bce_apply(k) == ApplyResult::Ok &&
           "BCE must revisit after an occurrence limit is relaxed");
    check_model(k, formula);
  }
}

static void test_flush(bool covered) {
  const Cnf formula = {{1, 2, 3, 4}, {-1}};
  Kernel k;
  load(k, 4, formula);
  auto apply = [&]() { return covered ? cce_apply(k) : bce_apply(k); };
  assert(apply() == ApplyResult::NoOp && apply() == ApplyResult::NoOp);
  assert(kernel_sparse_collect_root(k) == RootCollectOutcome::Changed);
  assert(apply() == ApplyResult::Ok &&
         "BCE/CCE must revisit a root-flushed candidate");
  check_model(k, formula);
}
static void test_flush_bce() { test_flush(false); }
static void test_flush_cce() { test_flush(true); }

static void test_roots() {
  using namespace tactix::session;
  const Cnf formula = {{4, 2}, {2, -4}, {3, -2}, {-3, 1, 4}, {4, -1}};
  for (bool preserve : {false, true}) {
    QueryHistory history;
    for (const auto &clause : formula) {
      for (int lit : clause) assert(history.add(lit) == InputStatus::Ok);
      assert(history.add(0) == InputStatus::Ok);
    }
    assert(history.assume(1) == InputStatus::Ok &&
           history.begin_query() == InputStatus::Ok);
    QueryProjection projection(history, history.active_query()->id);
    Kernel k;
    load(k, 4, formula);
    k.query_projection = preserve ? &projection : nullptr;
    k.bce_preserve_query = preserve;
    assert(bce_apply(k) == ApplyResult::NoOp && bce_apply(k) == ApplyResult::NoOp);
    const Var blocker = var_of(internalize(4));
    const auto touched = k.touched[blocker], removed = k.removed[blocker];
    assert(decide_apply(k, internalize(-2), false) == ApplyResult::Ok);
    assert(propagate_apply(k) == ApplyResult::Conflict);
    assert(analyze_apply(k) == ApplyResult::Ok);
    if (k.pending.valid) assert(backjump_apply(k) == ApplyResult::Ok);
    assert(!k.level && k.value(internalize(2)) > 0);
    assert(propagate_apply(k) == ApplyResult::Ok && k.value(internalize(3)) > 0);
    assert(k.touched[blocker] == touched && k.removed[blocker] == removed);
    assert(bce_apply(k) == ApplyResult::Ok &&
           "BCE must revisit blockers after root facts change");
    assert(!preserve || !projection.snapshot().restore);
    k.query_projection = nullptr;
    check_model(k, formula);
  }
}

static void test_tail() {
  Kernel k;
  load(k, 5, {{1, 2}, {-1, 3}, {-2, -3}, {4, 5}});
  const auto touched = k.touched;
  k.work_budget = 5;
  assert(bce_apply(k) == ApplyResult::NoOp && k.stats.bce_rearmed);
  assert(k.touched == touched &&
         "BCE resumption must not touch unrelated rule caches");
  for (unsigned pass = 0; pass < 2 * k.nvars && !k.stats.bce_deleted; ++pass)
    (void)bce_apply(k);
  assert(k.stats.bce_deleted && "budgeted BCE must reach its tail");
}

static void test_retraction() {
  Kernel k;
  load(k, 3, {{-1, 2, 3}});
  // A temporary root restriction hides this candidate until rollback.
  kernel_assign_unit(k, internalize(1));
  assert(bce_apply(k) == ApplyResult::NoOp && bce_apply(k) == ApplyResult::NoOp);
  kernel_unwind_speculation(k, 0);
  assert(bce_apply(k) == ApplyResult::Ok &&
         "BCE must revisit after root retraction");
  check_model(k, {{-1, 2, 3}});
}

static void test_compaction() {
  Kernel k;
  load(k, 5, {{2, 3}, {-2, 4}, {-3, -4}, {1}, {5}});
  assert(bce_apply(k) == ApplyResult::NoOp);
  const auto ticks = k.stats.bce_ticks;
  assert(kernel_compact_variables(k, true));
  assert(k.fixed_external_count == 1);
  assert(bce_apply(k) == ApplyResult::NoOp && k.stats.bce_ticks == ticks &&
         "compaction must preserve the processed root frontier");
  check_model(k, {{2, 3}, {-2, 4}, {-3, -4}, {1}, {5}});
}

static void test_unchanged() {
  Kernel k;
  load(k, 3, {{1, 2}, {-1, 3}, {-2, -3}});
  assert(bce_apply(k) == ApplyResult::NoOp);
  const auto ticks = k.stats.bce_ticks, skipped = k.stats.bce_mark_skipped;
  assert(bce_apply(k) == ApplyResult::NoOp && k.stats.bce_ticks == ticks &&
         k.stats.bce_mark_skipped > skipped);
  while (cce_apply(k) == ApplyResult::Ok) {}
  const auto tried = k.stats.cce_tried;
  assert(cce_apply(k) == ApplyResult::NoOp && k.stats.cce_tried == tried);
}

static void test_unobserved_backbone_roots() {
  Kernel k;
  load(k, 2, {{-1, 2}, {-1, -2}});
  strategy::install_rule_parameters(k, strategy::BceParameters{1, false});
  assert(bce_apply(k) == ApplyResult::NoOp);
  // Distinct consumers may have completed different domains before probing.
  k.bce_query_touch_mark = k.cce_touch_mark = k.bce_touch_mark;
  const auto full = k.bce_touch_mark, partial = k.bce_query_touch_mark;
  const auto covered = k.cce_touch_mark;
  assert(full && backbone_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(-1)) > 0 && !k.level);
  assert(k.bce_touch_mark == full && k.bce_query_touch_mark == partial &&
         k.cce_touch_mark == covered &&
         "unobserved Backbone roots must not invalidate completed domains");
}

static void test_scoped_profile_progress() {
  Kernel k;
  load(k, 5, {{1, 2}, {-1, 3}, {-2, -3}, {4, 5}});
  k.work_budget = 5;
  for (unsigned pass = 0; pass < 2 * k.nvars && !k.stats.bce_deleted; ++pass) {
    strategy::ScopedRuleParameters<strategy::BceParameters> profile(
        k, strategy::BceParameters{100, false});
    (void)bce_apply(k);
  }
  assert(!k.bce_occ_limit && k.stats.bce_deleted == 1 &&
         "scoped BCE profiles must preserve budgeted tail progress");
}

static void test_large_opposite_roots() {
  for (bool covered : {false, true}) {
    Kernel k;
    const Cnf formula = {{1, 3}, {-1, 2, 4}};
    load(k, 4, formula);
    // As with the existing filter tests, arm no candidates before the event.
    k.bce_touch_mark = k.cce_touch_mark = k.touch_epoch;
    auto apply = [&]() { return covered ? cce_apply(k) : bce_apply(k); };
    assert(apply() == ApplyResult::NoOp);
    kernel_assign_unit(k, internalize(2));
    assert(apply() == ApplyResult::Ok &&
           "large root-satisfied opposites must rearm BCE/CCE");
    assert(!kernel_binary_live(k, internalize(1), internalize(3)));
    Cnf original = formula;
    original.push_back({2});
    check_model(k, original);
  }
}

static void test_redundant_removal() {
  const Cnf formula = {{1, 2, 3}, {2, 3}, {-2, 4}, {-4, 3}, {-3, 2}};
  const std::vector<int> learned = {-1, 4, 5};
  // Independently establish that the added redundant clause is entailed.
  for (unsigned bits = 0; bits < 32; ++bits) {
    auto holds = [&](const std::vector<int> &clause) {
      for (int literal : clause)
        if (bool(bits & (1u << (unsigned(std::abs(literal)) - 1))) ==
            (literal > 0)) return true;
      return false;
    };
    bool model = true;
    for (const auto &clause : formula) model &= holds(clause);
    assert(!model || holds(learned));
  }
  Kernel k;
  load(k, 5, formula);
  const Lit lits[] = {internalize(-1), internalize(4), internalize(5)};
  const Ref ref = kernel_add_large_clause(k, lits, 3, true, 8);
  assert(bce_apply(k) == ApplyResult::NoOp && bce_apply(k) == ApplyResult::NoOp);
  kernel_delete_clause(k, clause_at(k.arena, ref)); // Reduce's retirement primitive
  assert(k.trail.empty());
  assert(bce_apply(k) == ApplyResult::Ok &&
         "BCE must revisit after a redundant opposite is deleted");
  check_model(k, formula);
}

static void test_redundant_rewrite(bool in_place) {
  const Cnf formula = {{1, 2, 3}, {2, 3}, {-2, 4}, {-4, 3}, {-3, 2}};
  Kernel k;
  load(k, in_place ? 6 : 5, formula);
  const Lit learned[] = {internalize(-1), internalize(4), internalize(5),
                          internalize(6)};
  const Ref ref = kernel_add_large_clause(k, learned, in_place ? 4 : 3, true, 8);
  assert(bce_apply(k) == ApplyResult::NoOp && bce_apply(k) == ApplyResult::NoOp);
  const Lit replacement[] = {internalize(4), internalize(5), internalize(6)};
  if (in_place)
    kernel_replace_large_clause_in_place(k, ref, ++k.next_clause_id,
                                         replacement, 3, 2);
  else
    assert(kernel_replace_clause(k, clause_at(k.arena, ref), ++k.next_clause_id,
                                 replacement, 2, true, 8, nullptr) == ReplaceOutcome::Binary);
  // The original formula forces 4, so both the old and new learned clauses
  // are entailed. Other pivots can retire the replacement without touching 1.
  while (bce_apply(k) == ApplyResult::Ok) {}
  assert(clause_at(k.arena, 0)->garbage() &&
         "BCE must revisit after a redundant opposite is rewritten");
  check_model(k, formula);
}
static void test_rewrite_packed() { test_redundant_rewrite(false); }
static void test_rewrite_in_place() { test_redundant_rewrite(true); }

static void test_interrupt_candidate() {
  using namespace tactix::session;
  QueryHistory history;
  for (int literal : {1, 2, 0}) assert(history.add(literal) == InputStatus::Ok);
  assert(history.assume(1) == InputStatus::Ok && history.assume(-2) == InputStatus::Ok &&
         history.begin_query() == InputStatus::Ok);
  QueryProjection projection(history, history.active_query()->id);
  Kernel k;
  load(k, 2, {{1, 2}});
  k.query_projection = &projection;
  k.bce_preserve_query = true;
  // seed12 polls false/true/true: the inner candidate poll fires after tick
  // accounting.
  k.stop_fuzz = 2;
  k.stop_fuzz_state = 12;
  assert(bce_apply(k) == ApplyResult::NoOp && k.stats.bce_ticks &&
         k.stats.bce_interrupted == 1 && !k.stats.bce_deleted);
  k.stop_fuzz = 0;
  assert(bce_apply(k) == ApplyResult::Ok && k.stats.bce_deleted == 1 &&
         "interrupted BCE must rearm its unfinished blocking literal");
  assert(!projection.snapshot().restore);
  k.query_projection = nullptr;
  check_model(k, {{1, 2}});
}

static void test_scoped_refusal() {
  const Cnf formula = {{1, 2}, {1, 3}, {-2, 4}, {-3, 4},
                       {-5, 6}, {-6, 7}, {-7, 5}, {8, 9}};
  Kernel k;
  load(k, 9, formula);
  k.work_budget = 5;
  for (unsigned repeat = 0; repeat < 2 * k.nvars && !k.stats.bce_deleted; ++repeat) {
    strategy::ScopedRuleParameters<strategy::BceParameters> profile(
        k, strategy::BceParameters{1, false});
    (void)bce_apply(k);
  }
  assert(!k.bce_occ_limit && k.stats.bce_deleted == 1 &&
         "refusing scoped BCE must still reach its allowed budget tail");
  assert(kernel_binary_live(k, internalize(1), internalize(2)));
  k.work_budget = 0;
  assert(bce_apply(k) == ApplyResult::Ok && k.stats.bce_deleted > 1 &&
         "unlimited BCE must revisit scoped occurrence refusals");
  check_model(k, formula);
}

static void test_otf_kept_pivot() {
  const Cnf formula = {{1, 2, 3}, {-1, 2, 3}, {1, 4, 5}, {1, 6, 7},
                       {1, 8, 9}, {-1, 10, 11}, {-1, 12, 13}, {-1, 14, 15}};
  Kernel k;
  load(k, 15, formula);
  k.bve_gate_mask = 0;
  k.bve_backward = false;
  k.bve_on_the_fly_strengthening = true;
  k.bve_bound = 0;
  const auto mark = k.touch_epoch;
  const RuleVTable &rule = tactix_rules()[tactix_rule_id("BVE")];
  assert(rule.apply_instance(k, lit_instance(internalize(1))) == ApplyResult::Ok);
  assert(!k.stats.bve_eliminated && !kernel_var_retired(k, var_of(internalize(1))));
  assert(clause_at(k.arena, 0)->garbage());
  assert(k.removed[var_of(internalize(1))] > mark &&
         "BVE parent strengthening must mark its retained pivot removed");
  check_model(k, formula);
}

static void test_cce_budget_retry() {
  Cnf formula = {{5, 6, 7}};
  for (unsigned i = 0; i != 3000; ++i) formula.push_back({-5, 1});
  formula.push_back({-1, -6});
  formula.push_back({-7, 1});
  Kernel k;
  load(k, 7, formula);
  k.work_budget = 1;
  assert(cce_apply(k) == ApplyResult::NoOp);
  assert(!clause_at(k.arena, 0)->garbage());
  k.work_budget = 0;
  assert(cce_apply(k) == ApplyResult::Ok && clause_at(k.arena, 0)->garbage() &&
         "CCE must retry a truncated candidate under a larger budget");
  check_model(k, formula);
}

static void test_analyze_redundant_reason(bool binary) {
  const unsigned pivot = binary ? 3 : 5, last = binary ? 4 : 6;
  Cnf formula;
  const std::vector<int> reason = binary ? std::vector<int>{3, -2, -1}
                                        : std::vector<int>{5, -2, -3, -4, -1};
  formula.push_back(reason); // original copy entails the learned reason
  if (!binary) {
    formula.push_back({-2, 3});
    formula.push_back({-2, 4});
  }
  formula.push_back({-2, int(last)});
  formula.push_back({-int(pivot), -int(last)});
  Kernel k;
  load(k, last, formula);
  std::vector<Lit> learned;
  for (int literal : reason) learned.push_back(internalize(literal));
  const Ref ref = kernel_add_large_clause(k, learned.data(), learned.size(), true, 3);
  // Isolate the rewrite from eager subsumption. The driving conflict is a
  // binary with another variable, so retiring it cannot mask the pivot mark.
  k.eager_subsume = 0;
  for (Ref &entry : k.eager_recent) entry = INVALID_REF;
  kernel_assign_decision(k, internalize(1));
  kernel_assign_decision(k, internalize(2));
  if (!binary) {
    kernel_assign(k, internalize(3), 2, internalize(-2), true);
    kernel_assign(k, internalize(4), 2, internalize(-2), true);
  }
  kernel_assign(k, internalize(pivot), 2, ref, false);
  kernel_assign(k, internalize(last), 2, internalize(-2), true);
  k.conflict_ref = INVALID_REF;
  k.conflict_bin[0] = internalize(-int(pivot));
  k.conflict_bin[1] = internalize(-int(last));
  k.conflict_valid = true;
  std::fill(k.bve_retry.begin(), k.bve_retry.end(), 0);
  const auto before = k.touch_epoch;
  assert(analyze_apply(k) == ApplyResult::Ok && k.stats.otfs_strengthened == 1);
  assert(!k.stats.otfs_subsumed && "a subsumed conflict would mask missing advice");
  assert(k.removed[pivot - 1] > before && !k.bve_retry[pivot - 1] &&
         "redundant OTFS must mark removal without BVE retry");
  if (k.pending.valid) assert(backjump_apply(k) == ApplyResult::Ok);
  if (k.level) kernel_backtrack(k, 0);
  check_model(k, formula);
}
static void test_analyze_binary() { test_analyze_redundant_reason(true); }
static void test_analyze_large() { test_analyze_redundant_reason(false); }

int main(int argc, char **argv) {
  struct Test { const char *name; void (*run)(); };
  const Test tests[] = {{"limit", test_limit}, {"flush-bce", test_flush_bce},
                        {"flush-cce", test_flush_cce},
                        {"roots", test_roots}, {"tail", test_tail},
                        {"unchanged", test_unchanged},
                        {"retraction", test_retraction},
                        {"compaction", test_compaction},
                        {"backbone", test_unobserved_backbone_roots},
                        {"scoped", test_scoped_profile_progress},
                        {"large-roots", test_large_opposite_roots},
                        {"redundant", test_redundant_removal},
                        {"rewrite-packed", test_rewrite_packed},
                        {"rewrite-inplace", test_rewrite_in_place},
                        {"candidate-interrupt", test_interrupt_candidate},
                        {"scoped-refusal", test_scoped_refusal},
                        {"otf-pivot", test_otf_kept_pivot},
                        {"cce-budget", test_cce_budget_retry},
                        {"analyze-binary", test_analyze_binary},
                        {"analyze-large", test_analyze_large}};
  assert(argc <= 2);
  bool ran = false;
  for (const auto &test : tests)
    if (argc == 1 || !std::strcmp(argv[1], test.name)) {
      test.run(); ran = true;
    }
  assert(ran);
  std::puts("root-occurrence cache regressions passed");
}
