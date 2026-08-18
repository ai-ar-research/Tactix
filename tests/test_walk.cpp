// Walk unit tests (plan §5.2 policy op). The walker repairs phases the
// defaults get wrong, is `H`-only, respects the trail, and is deterministic.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "policies/ops/walk/walk.hpp"
#include "rules/registry.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace tactix;

// Pairwise-conflicting variables plus one covering clause: a model sets
// exactly one variable true, so the all-positive default phase is worst-case.
static void build_conflict_cover(Kernel &k, int n) {
  kernel_reserve_vars(k, n);
  for (int i = 1; i <= n; i++)
    for (int j = i + 1; j <= n; j++)
      kernel_add_binary_clause(k, internalize(-i), internalize(-j));
  std::vector<Lit> cover;
  for (int i = 1; i <= n; i++)
    cover.push_back(internalize(i));
  kernel_add_large_clause(k, cover.data(), (uint32_t)cover.size(), false, 0);
}

// the walk finds the model and writes it into the saved phases
static void test_finds_model() {
  Kernel k;
  ScopedHeuristics _h(k);
  build_conflict_cover(k, 8);
  for (Var v = 0; v < k.nvars; v++) {
    assert(k.h->saved[v] == 0); // no assignment has established a saved phase
    assert(!negated(heuristics_decide_lit(k, v))); // initial phase is positive
  }

  assert(walk_guard(k));
  assert(walk_apply(k) == ApplyResult::Ok); // improved
  assert(k.stats.walks == 1 && k.stats.walk_improved == 1);
  assert(k.stats.walk_flips > 0);

  int trues = 0;
  for (Var v = 0; v < k.nvars; v++)
    trues += (k.h->saved[v] > 0);
  assert(trues == 1); // exactly one: every binary satisfied, cover satisfied
  for (Var v = 0; v < k.nvars; v++)
    assert(k.h->target[v] == k.h->saved[v]); // the rephase tail ran
  assert(k.h->target_assigned == 0);
  assert(k.stats.rephased == 1);
}

// `H`-only: no clause, trail, level or proof state may move
static void test_kernel_untouched() {
  Kernel k;
  ScopedHeuristics _h(k);
  build_conflict_cover(k, 6);
  const size_t arena = k.arena.size();
  const size_t trail = k.trail.size();
  const uint32_t level = k.level;
  const uint64_t confl = k.stats.conflicts, props = k.stats.propagations;
  walk_apply(k);
  assert(k.arena.size() == arena);
  assert(k.trail.size() == trail);
  assert(k.level == level);
  assert(k.stats.conflicts == confl && k.stats.propagations == props);
  assert(!k.inconsistent);
}

// trail-assigned literals are fixed: the walk may not flip them
static void test_respects_trail() {
  Kernel k;
  ScopedHeuristics _h(k);
  build_conflict_cover(k, 6);
  kernel_assign_decision(k, internalize(3)); // x3 true at level 1
  assert(k.level == 1);
  walk_apply(k);
  assert(k.h->saved[2] == 1); // var 3 (0-based 2) keeps its assigned phase
  int trues = 0;
  for (Var v = 0; v < k.nvars; v++)
    trues += (k.h->saved[v] > 0);
  assert(trues == 1); // the other five must go false around it
}

// the walker is a pure function of kernel state: same formula, same result
static void test_deterministic() {
  int8_t first[10] = {0};
  for (int run = 0; run < 2; run++) {
    Kernel k;
    ScopedHeuristics _h(k);
    build_conflict_cover(k, 10);
    walk_apply(k);
    if (!run)
      for (Var v = 0; v < k.nvars; v++)
        first[v] = k.h->saved[v];
    else
      for (Var v = 0; v < k.nvars; v++)
        assert(k.h->saved[v] == first[v]);
  }
}

// a starved budget terminates, and nothing to improve reports NoOp, not Ok
static void test_budget_and_noop() {
  Kernel k;
  ScopedHeuristics _h(k);
  build_conflict_cover(k, 8);
  assert(walk_apply_steps(k, 1) != ApplyResult::Conflict); // terminates
  Kernel k2;
  ScopedHeuristics _h2(k2);
  kernel_reserve_vars(k2, 3);
  kernel_add_binary_clause(k2, internalize(1), internalize(2));
  assert(walk_apply(k2) == ApplyResult::NoOp); // default phase satisfies it
  assert(k2.stats.walk_improved == 0);
}

// Walk's non-null dense transition saves each binary from the lower compact
// literal and replays that stack on sparse resume. Stable kernel identities can
// disagree with compact/import order after retirement, so the shared endpoint
// below must receive var 1 before var 0 even though the clauses were inserted
// in the opposite order. No witness literals or formula profile are involved.
static void test_resume_uses_compact_literal_order() {
  Kernel k;
  ScopedHeuristics _h(k);
  kernel_reserve_vars(k, 3);
  kernel_record_import(k, 1);
  kernel_record_import(k, 0);
  kernel_record_import(k, 2);
  const Lit shared = internalize(3);
  kernel_add_binary_clause(k, internalize(1), shared);
  kernel_add_binary_clause(k, internalize(2), shared);

  const auto partners = [&] {
    std::vector<Lit> result;
    for (const WatchEntry watch : k.watches.entries(shared))
      if (watch.binary())
        result.push_back(watch.other());
    return result;
  };
  assert((partners() == std::vector<Lit>{internalize(1), internalize(2)}));
  assert(walk_apply_steps(k, 0) != ApplyResult::Conflict);
  assert((partners() == std::vector<Lit>{internalize(2), internalize(1)}));
}

// registered as a policy op with a working instance pair
static void test_registry() {
  const int id = tactix_rule_id("Walk");
  assert(id >= 0);
  const RuleVTable &vt = tactix_rules()[id];
  assert(vt.kind == ActionKind::PolicyOp);
  Kernel k;
  ScopedHeuristics _h(k);
  build_conflict_cover(k, 8);
  assert(vt.guard(k));
  uint64_t insts[4];
  assert(vt.instances(k, insts, 4) == 1);
  assert(insts[0] == INSTANCE_CANONICAL);
  assert(vt.apply_instance(k, INSTANCE_CANONICAL) == ApplyResult::Ok);
  assert(vt.apply_instance(k, 50000) != ApplyResult::Conflict); // any payload
}

// a kernel without `H` has no phases to improve, so the guard must reject
static void test_no_heuristics_guard() {
  Kernel k;
  build_conflict_cover(k, 4);
  assert(k.h == nullptr);
  assert(!walk_guard(k));
}

int main() {
  test_finds_model();
  test_kernel_untouched();
  test_respects_trail();
  test_deterministic();
  test_budget_and_noop();
  test_resume_uses_compact_literal_order();
  test_registry();
  test_no_heuristics_guard();
  printf("test_walk: all passed\n");
  return 0;
}
