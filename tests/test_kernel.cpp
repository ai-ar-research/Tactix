// Kernel unit tests: literal encoding, watch scheme, propagation chains,
// root-level conflict detection.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "policies/default_policy.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace tactix;

static void test_variable_identity_maps() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  assert(k.internal_to_external == std::vector<Var>({0, 1, 2}));
  assert(k.external_to_internal == std::vector<Var>({0, 1, 2}));
  assert(kernel_externalize(k, make_lit(1, true)) == -2);
  assert(kernel_internalize(k, -2) == make_lit(1, true));
  assert(kernel_internalize(k, 4) == INVALID_LIT);
  assert(kernel_num_output_vars(k) == 3);

  const Var first = kernel_new_extension_vars(k, 2);
  assert(first == 3);
  assert(k.original_external_vars == 3);
  assert(kernel_num_output_vars(k) == 3);
  assert(kernel_externalize(k, make_lit(4, false)) == 5);
  assert(kernel_internalize(k, 5) == make_lit(4, false));
}

static void test_variable_compaction() {
  Kernel k;
  ScopedHeuristics hg(k);
  kernel_reserve_vars(k, 5);
  const Lit binary[2] = {internalize(1), internalize(5)};
  kernel_add_binary_clause(k, binary[0], binary[1], 7);
  const Lit large[3] = {internalize(1), internalize(3), internalize(5)};
  kernel_add_large_clause(k, large, 3, false, 0, 8);
  kernel_assign_unit(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(3));
  assert(propagate_apply(k) == ApplyResult::Ok);

  // x2 and x4 have completed retirement: neither occurs in the live formula.
  // A reconstruction witness keeps x2's permanent model identity meaningful.
  kernel_push_witness(k, internalize(2), nullptr, 0);
  const uint64_t stable_instance = lit_instance(k, internalize(5));
  k.sweep_tail = {1, 4}; // permanent external identities, including retired x2
  k.var_status.assign(k.nvars, VarStatus::Active);
  for (const Var v : {Var{1}, Var{3}}) {
    k.var_status[v] = VarStatus::Eliminated;
    heuristics_deactivate_var(k, v);
    k.unassigned_active--;
  }
  assert(kernel_should_compact_variables(k, 10));
  assert(kernel_compact_variables(k));

  assert(k.nvars == 3);
  assert(k.internal_to_external == std::vector<Var>({0, 2, 4}));
  assert(k.external_to_internal ==
         std::vector<Var>({0, INVALID_VAR, 1, INVALID_VAR, 2}));
  assert(kernel_internalize(k, 3) == make_lit(1, false));
  assert(kernel_externalize(k, make_lit(2, true)) == -5);
  assert(lit_instance(k, make_lit(2, false)) == stable_instance);
  assert(kernel_binary_live(k, make_lit(0, false), make_lit(2, false)));
  const Clause *c = clause_at(k.arena, 0);
  // Whole-arena collection reselects Kissat's best watched pair: the only
  // unassigned literal first, then the latest satisfied one.
  assert(c->lits[0] == make_lit(2, false));
  assert(c->lits[1] == make_lit(1, false));
  assert(c->lits[2] == make_lit(0, false));
  assert(k.stats.variable_compactions == 1);
  assert(k.stats.compacted_variables == 2);
  assert(k.sweep_tail == std::vector<Var>({1, 4}));
  assert(k.level == 1 && k.frames.size() == 2);
  assert(k.trail.size() == 2 && k.propagate_head == 2);
  assert(k.value(kernel_internalize(k, 1)) > 0);
  assert(k.value(kernel_internalize(k, 3)) > 0);

  const RuleVTable *decide = nullptr;
  for (const RuleVTable &rule : tactix_rules())
    if (!std::strcmp(rule.name, "Decide"))
      decide = &rule;
  assert(decide && decide->instances && decide->apply_instance);
  uint64_t instances[4];
  // one unassigned variable left, offered in both phases (M2's instance
  // surface is any unassigned LITERAL — docs/calculus.md §3 M2)
  assert(decide->instances(k, instances, 4) == 2);
  Lit external;
  assert(lit_of_instance(instances[0], &external));
  assert(std::abs(externalize(external)) == 5);
  Lit opposite;
  assert(lit_of_instance(instances[1], &opposite));
  assert(opposite == negate(external));
  kernel_check_invariants(k);

  kernel_extend_model(k);
  assert(k.model_values[internalize(2)] > 0);
}

// Kissat moves each surviving literal's watch vector wholesale during variable
// compaction.  The directed binary prefix must therefore retain its own
// encounter order; rebuilding it from numerically enumerated undirected pairs
// can reverse a shared endpoint even though the formula is unchanged.
static void test_variable_compaction_preserves_directed_binary_order() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 5);
  const Lit shared = internalize(5);
  kernel_add_binary_clause(k, internalize(3), shared);
  kernel_add_binary_clause(k, internalize(1), shared);

  const auto external_partners = [&] {
    std::vector<int> result;
    const Lit current_shared = kernel_internalize(k, 5);
    for (const WatchEntry watch : k.watches.entries(current_shared))
      if (watch.binary())
        result.push_back(kernel_externalize(k, watch.other()));
    return result;
  };
  assert((external_partners() == std::vector<int>{3, 1}));

  // Retire an unrelated variable so all binary endpoints remain live while
  // their internal identities move.
  k.var_status.assign(k.nvars, VarStatus::Active);
  k.var_status[1] = VarStatus::Eliminated;
  heuristics_deactivate_var(k, 1);
  k.unassigned_active--;
  assert(kernel_compact_variables(k));
  assert((external_partners() == std::vector<int>{3, 1}));
  kernel_check_invariants(k);
}

// Variable compaction is not an arena-policy checkpoint.  Kissat keeps a
// replacement at its source clause's position during compacting collection;
// Tactix appends the replacement and retains that native position in the
// inherited clause-order key.  Compaction must replay the logical source order
// and must not leak an independently configured inprocessing order.
static void test_variable_compaction_replays_inherited_arena_order() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 7);
  const Lit shared = internalize(7);
  const Lit source[3] = {shared, internalize(3), internalize(4)};
  const Lit middle[3] = {shared, internalize(5), internalize(6)};
  const Lit later[3] = {shared, internalize(4), internalize(5)};
  const Lit replacement[3] = {shared, internalize(3), internalize(6)};
  const Ref source_ref = kernel_add_large_clause(k, source, 3, false, 0, 101);
  kernel_add_large_clause(k, middle, 3, false, 0, 102);
  kernel_add_large_clause(k, later, 3, false, 0, 103);
  kernel_add_large_clause(k, replacement, 3, false, 0, 104);
  kernel_inherit_clause_order(k, 104, 101);
  kernel_delete_clause(k, clause_at(k.arena, source_ref));

  const auto watched_ids = [&] {
    std::vector<uint64_t> ids;
    const Lit current_shared = kernel_internalize(k, 7);
    for (const WatchEntry watch : k.watches.entries(current_shared))
      if (!watch.binary() && !clause_at(k.arena, watch.ref())->garbage())
        ids.push_back(clause_at(k.arena, watch.ref())->id());
    return ids;
  };
  assert((watched_ids() == std::vector<uint64_t>{102, 103, 104}));

  struct ReverseOrder final : ArenaOrdering {
    void order(const Kernel &, std::vector<Ref> &refs) override {
      std::reverse(refs.begin(), refs.end());
    }
  } reverse;
  k.arena_ordering = &reverse;

  // Retire an unrelated variable.  Physical append order is 102,103,104 and
  // the posed policy would reverse it to 104,103,102.  The distinct result is
  // 104,102,103 because replacement 104 inherits source position 101.
  k.var_status.assign(k.nvars, VarStatus::Active);
  k.var_status[1] = VarStatus::Eliminated;
  heuristics_deactivate_var(k, 1);
  k.unassigned_active--;
  assert(kernel_compact_variables(k));
  assert(k.arena_ordering == &reverse);
  assert((watched_ids() == std::vector<uint64_t>{104, 102, 103}));
  kernel_check_invariants(k);
}

static void test_compaction_defers_retired_jumped_reason() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 3);
  const Lit p = internalize(1), q = internalize(2), r = internalize(3);
  kernel_assign_decision(k, p);
  kernel_assign(k, q, k.level, DECISION_REASON, false);
  kernel_assign(k, r, k.level, p, true, true);
  k.jumped_over[var_of(r)] = q;

  // The retired q no longer occurs in the formula, but r's virtual binary
  // reason still needs it. Dropping q would make the reason unrepresentable;
  // the goldcrest crash indexed old_to_new[INVALID_VAR] while remapping it.
  k.var_status.assign(k.nvars, VarStatus::Active);
  k.var_status[var_of(q)] = VarStatus::Eliminated;
  heuristics_deactivate_var(k, var_of(q));
  assert(!kernel_compact_variables(k));
  assert(k.nvars == 3 && k.jumped_over[var_of(r)] == q);
}

static void test_compaction_drops_unassigned_stale_reason() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 3);

  // Backtracking intentionally leaves Assigned metadata behind. Model the
  // goldcrest state exactly: active r is unassigned, but its stale record says
  // a jumped binary reason exists while jumped_over is already INVALID_LIT.
  const Var retired = 1, stale = 2;
  k.assigned[stale] = {7, 11, INVALID_LIT, true, true};
  k.jumped_over[stale] = INVALID_LIT;
  k.var_status.assign(k.nvars, VarStatus::Active);
  k.var_status[retired] = VarStatus::Eliminated;
  heuristics_deactivate_var(k, retired);
  k.unassigned_active--;

  assert(kernel_compact_variables(k));
  assert(k.nvars == 2);
  const Assigned &a = k.assigned[1];
  assert(!a.binary && !a.jumped && a.level == 0 && a.trail == 0 &&
         a.reason == 0);
  assert(k.jumped_over[1] == INVALID_LIT);
  kernel_check_invariants(k);
}

static void test_lit_encoding() {
  assert(internalize(1) == make_lit(0, false));
  assert(internalize(-1) == make_lit(0, true));
  assert(externalize(internalize(7)) == 7);
  assert(externalize(internalize(-42)) == -42);
  assert(negate(internalize(3)) == internalize(-3));
  assert(var_of(internalize(-5)) == 4);
}

// ScopedHeuristics attaches k.h: the session, not the kernel, owns it.
static void test_unit_chain() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 4);
  kernel_assign_unit(k, internalize(1));
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(3));
  kernel_add_binary_clause(k, internalize(-3), internalize(4));
  assert(propagate_apply(k) == ApplyResult::Ok);
  for (int e = 1; e <= 4; e++)
    assert(k.values[internalize(e)] > 0);
  assert(k.trail.size() == 4);
  assert(k.unassigned == 0);
}

static void test_root_sparse_collection() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_assign_unit(k, internalize(1));
  const Lit satisfied[3] = {internalize(1), internalize(2),
                            internalize(3)};
  const Lit shortened[4] = {internalize(-1), internalize(2),
                            internalize(3), internalize(4)};
  kernel_add_binary_clause(k, internalize(1), internalize(4));
  kernel_add_large_clause(k, satisfied, 3, false, 0);
  kernel_add_large_clause(k, shortened, 4, false, 0);
  assert(propagate_apply(k) == ApplyResult::Ok);

  assert(kernel_sparse_collect_root(k) == RootCollectOutcome::Changed);
  assert(k.stats.sparse_collections == 1);
  assert(k.stats.sparse_collected == 3 && k.stats.flushed == 2);
  assert(kernel_binary_copies(k, internalize(1), internalize(4)) == 0);
  assert(k.arena.size() == clause_words(3));
  const Clause *remaining = clause_at(k.arena, 0);
  assert(remaining->size == 3 && !remaining->garbage());
  for (const Lit l : *remaining)
    assert(var_of(l) != 0);
  kernel_check_invariants(k);
}

struct ReverseArenaOrder final : ArenaOrdering {
  void order(const Kernel &, std::vector<Ref> &refs) override {
    std::reverse(refs.begin(), refs.end());
  }
};

static void test_root_sparse_collection_keeps_native_order() {
  Kernel k;
  kernel_reserve_vars(k, 7);
  ReverseArenaOrder reverse;
  k.arena_ordering = &reverse;
  kernel_assign_unit(k, internalize(1));
  const Lit first[3] = {internalize(2), internalize(3), internalize(4)};
  const Lit shortened[4] = {internalize(-1), internalize(3), internalize(4),
                            internalize(5)};
  const Lit garbage[3] = {internalize(1), internalize(3), internalize(4)};
  const Lit second[3] = {internalize(5), internalize(6), internalize(7)};
  kernel_add_large_clause(k, first, 3, false, 0, 11);
  kernel_add_large_clause(k, shortened, 4, false, 0, 12);
  kernel_add_large_clause(k, garbage, 3, false, 0, 14);
  kernel_add_large_clause(k, second, 3, false, 0, 13);
  assert(kernel_sparse_collect_root(k) == RootCollectOutcome::Changed);
  const Clause *a = clause_at(k.arena, 0);
  const Clause *b = clause_at(k.arena, clause_words(a->size));
  const Clause *c = clause_at(k.arena, clause_words(a->size) +
                                           clause_words(b->size));
  assert(a->id() == 11 && b->id() == 1 && c->id() == 13);
  kernel_check_invariants(k);
}

static void test_root_sparse_collection_ignores_nonroot_values() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 7);
  kernel_assign_unit(k, internalize(1));

  const Lit root_satisfied[3] = {internalize(1), internalize(6),
                                 internalize(7)};
  kernel_add_large_clause(k, root_satisfied, 3, true, 7, 101);
  const Lit search_satisfied[3] = {internalize(2), internalize(4),
                                   internalize(5)};
  kernel_add_large_clause(k, search_satisfied, 3, true, 7, 102);

  kernel_assign_decision(k, internalize(2));
  assert(k.level == 1 && kernel_root_value(k, internalize(2)) == 0);
  assert(kernel_sparse_collect_root(k) == RootCollectOutcome::Changed);

  bool saw_root = false, saw_search = false;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    saw_root |= c->id() == 101;
    saw_search |= c->id() == 102;
    ref += clause_words(c->size);
  }
  assert(!saw_root);
  assert(saw_search && "a decision-level true literal is not a root fact");
  assert(k.level == 1 && k.values[internalize(2)] > 0);
  kernel_check_invariants(k);
}

static void test_root_sparse_collection_rewrites_nonroot_reason() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 3);
  kernel_assign_unit(k, internalize(1));
  const Lit reason_lits[3] = {internalize(-1), internalize(-2),
                              internalize(3)};
  const Ref reason =
      kernel_add_large_clause(k, reason_lits, 3, true, 3, 103);
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(2));
  assert(propagate_apply(k) == ApplyResult::Ok);
  Assigned &a = k.assigned[var_of(internalize(3))];
  assert(a.level == 1 && !a.binary && a.reason == reason);
  clause_at(k.arena, reason)->flags |= Clause::REASON;

  assert(kernel_sparse_collect_root(k) == RootCollectOutcome::Changed);
  assert(k.values[internalize(3)] > 0);
  assert(a.level == 1 && a.binary && !a.jumped);
  assert(a.reason == internalize(-2));
  assert(kernel_binary_live(k, internalize(-2), internalize(3)));
  kernel_check_invariants(k);
}

static void test_sparse_collection_flushes_fixed_prefix_watch() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit prefix[3] = {internalize(-1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, prefix, 3, false, 0, 104);
  const Ref start = static_cast<Ref>(k.arena.size());
  const Lit victim_lits[3] = {internalize(4), internalize(5), internalize(6)};
  const Ref victim =
      kernel_add_large_clause(k, victim_lits, 3, true, 7, 105);
  kernel_assign_unit(k, internalize(1));
  kernel_delete_clause(k, clause_at(k.arena, victim));

  kernel_collect_garbage(k, start);
  bool fixed_watch = false, live_watch = false;
  for (const WatchEntry w : k.watches.entries(internalize(-1)))
    fixed_watch |= !w.binary();
  for (const WatchEntry w : k.watches.entries(internalize(2)))
    live_watch |= !w.binary();
  assert(!fixed_watch && "Kissat flushes prefix watches of fixed literals");
  assert(live_watch);
  kernel_check_invariants(k);
}

// Kissat's sparse collector keeps the unswept prefix, then stably partitions
// a mixed swept tail into irredundant and redundant clauses before rewatching
// it.  The partition is a collection invariant, not an arena policy choice.
static void test_sparse_collection_partitions_mixed_tail() {
  Kernel k;
  kernel_reserve_vars(k, 7);
  const Lit shared = internalize(1);
  const Lit prefix[3] = {shared, internalize(2), internalize(3)};
  const Lit red_first[3] = {shared, internalize(3), internalize(4)};
  const Lit irred_first[3] = {shared, internalize(4), internalize(5)};
  const Lit red_second[3] = {shared, internalize(5), internalize(6)};
  const Lit irred_second[3] = {shared, internalize(6), internalize(7)};
  kernel_add_large_clause(k, prefix, 3, false, 0, 201);
  const Ref start = kernel_add_large_clause(k, red_first, 3, true, 7, 202);
  kernel_add_large_clause(k, irred_first, 3, false, 0, 203);
  kernel_add_large_clause(k, red_second, 3, true, 8, 204);
  kernel_add_large_clause(k, irred_second, 3, false, 0, 205);

  ReverseArenaOrder reverse;
  k.arena_ordering = &reverse;
  kernel_collect_garbage(k, start);
  assert(k.arena_ordering == &reverse);

  std::vector<uint64_t> arena_ids;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    arena_ids.push_back(c->id());
    ref += clause_words(c->size);
  }
  assert((arena_ids == std::vector<uint64_t>{201, 203, 205, 202, 204}));

  std::vector<uint64_t> watched_ids;
  for (const WatchEntry watch : k.watches.entries(shared))
    if (!watch.binary())
      watched_ids.push_back(clause_at(k.arena, watch.ref())->id());
  assert((watched_ids == arena_ids));
  kernel_check_invariants(k);
}

// The same partition must compose with root shortening: replacements retain
// their source position within their redundancy class, then the two stable
// classes form Kissat's rewatched sparse tail.
static void test_root_sparse_collection_partitions_rewritten_tail() {
  Kernel k;
  kernel_reserve_vars(k, 8);
  kernel_assign_unit(k, internalize(1));
  const Lit shared = internalize(2);
  const Lit prefix[3] = {shared, internalize(3), internalize(4)};
  const Lit red_rewritten[4] = {internalize(-1), shared, internalize(4),
                                internalize(5)};
  const Lit irred_first[3] = {shared, internalize(5), internalize(6)};
  const Lit red_second[3] = {shared, internalize(6), internalize(7)};
  const Lit irred_second[3] = {shared, internalize(7), internalize(8)};
  kernel_add_large_clause(k, prefix, 3, false, 0, 301);
  const Ref start =
      kernel_add_large_clause(k, red_rewritten, 4, true, 7, 302);
  kernel_add_large_clause(k, irred_first, 3, false, 0, 303);
  kernel_add_large_clause(k, red_second, 3, true, 8, 304);
  kernel_add_large_clause(k, irred_second, 3, false, 0, 305);

  ReverseArenaOrder reverse;
  k.arena_ordering = &reverse;
  assert(kernel_sparse_collect_root(k, start) == RootCollectOutcome::Changed);
  assert(k.arena_ordering == &reverse);

  std::vector<uint64_t> arena_ids;
  std::vector<bool> redundant;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    arena_ids.push_back(c->id());
    redundant.push_back(c->redundant());
    ref += clause_words(c->size);
  }
  assert((arena_ids == std::vector<uint64_t>{301, 303, 305, 1, 304}));
  assert((redundant == std::vector<bool>{false, false, false, true, true}));

  std::vector<uint64_t> watched_ids;
  for (const WatchEntry watch : k.watches.entries(shared))
    if (!watch.binary())
      watched_ids.push_back(clause_at(k.arena, watch.ref())->id());
  assert((watched_ids == arena_ids));
  kernel_check_invariants(k);
}

// Reduce has already marked its victims when it enters the root-aware sparse
// collector.  Pre-existing garbage alone must therefore trigger the sweep and
// rewatch; it must not depend on an unrelated root shortening in the same pass.
static void test_root_sparse_collection_collects_existing_garbage() {
  Kernel k;
  kernel_reserve_vars(k, 8);
  kernel_assign_unit(k, internalize(1));
  const Lit shared = internalize(2);
  const Lit prefix[3] = {shared, internalize(3), internalize(4)};
  const Lit red_first[3] = {shared, internalize(4), internalize(5)};
  const Lit irred_first[3] = {shared, internalize(5), internalize(6)};
  const Lit victim_lits[3] = {shared, internalize(6), internalize(7)};
  const Lit irred_second[3] = {shared, internalize(7), internalize(8)};
  kernel_add_large_clause(k, prefix, 3, false, 0, 401);
  const Ref start = kernel_add_large_clause(k, red_first, 3, true, 7, 402);
  kernel_add_large_clause(k, irred_first, 3, false, 0, 403);
  const Ref victim =
      kernel_add_large_clause(k, victim_lits, 3, true, 8, 404);
  kernel_add_large_clause(k, irred_second, 3, false, 0, 405);
  kernel_delete_clause(k, clause_at(k.arena, victim));
  const uint64_t generation = k.arena_generation;

  // The root assignment is deliberately absent from every live clause, so
  // collecting the already-marked victim is the only reason to compact.
  assert(kernel_sparse_collect_root(k, start) == RootCollectOutcome::NoChange);
  assert(k.arena_generation == generation + 1);

  std::vector<uint64_t> arena_ids;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    arena_ids.push_back(c->id());
    ref += clause_words(c->size);
  }
  assert((arena_ids == std::vector<uint64_t>{401, 403, 405, 402}));

  std::vector<uint64_t> watched_ids;
  for (const WatchEntry watch : k.watches.entries(shared))
    if (!watch.binary())
      watched_ids.push_back(clause_at(k.arena, watch.ref())->id());
  assert((watched_ids == arena_ids));
  kernel_check_invariants(k);
}

static void test_large_clause_propagation() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 3);
  const Lit lits[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, lits, 3, false, 0);
  kernel_assign_unit(k, internalize(-1));
  kernel_assign_unit(k, internalize(-2));
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(k.values[internalize(3)] > 0);
}

static void test_root_conflict_is_unsat() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 2);
  kernel_assign_unit(k, internalize(1));
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  assert(propagate_apply(k) == ApplyResult::Unsat);
  assert(k.inconsistent);
}

static void test_backtrack_restores() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 3);
  kernel_assign_decision(k, internalize(1));
  kernel_assign_decision(k, internalize(-2));
  assert(k.level == 2 && k.unassigned == 1);
  // Kissat saves at assignment, not while removing the trail.
  assert(k.h->saved[0] == 1);
  assert(k.h->saved[1] == -1);
  kernel_backtrack(k, 0);
  assert(k.level == 0 && k.unassigned == 3);
  assert(k.values[internalize(1)] == 0);
  assert(k.h->saved[0] == 1);  // phase of var 1 saved positive
  assert(k.h->saved[1] == -1); // phase of var 2 saved negative
}

static void test_probing_does_not_save_phases() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 2);
  k.h->saved[0] = 1;
  k.phase_saving_suppressed = true;
  kernel_assign_decision(k, internalize(-1));
  assert(k.h->saved[0] == 1);
  k.phase_saving_suppressed = false;
  kernel_backtrack(k, 0, /*search_backtrack=*/false);
}

// A probing episode derives exactly what kernel_propagate derives and hands
// back watch lists, literal order and `searched` byte-identical.
struct BookKeeping {
  std::vector<std::vector<uint32_t>> lists;
  std::vector<uint8_t> list_order;
  std::vector<uint32_t> clauses;
};

static BookKeeping bookkeeping(const Kernel &k) {
  BookKeeping s;
  s.lists.resize(2 * k.nvars);
  s.list_order.resize(2 * k.nvars);
  for (Lit l = 0; l < 2 * k.nvars; l++) {
    const WatchHead &h = k.watches.heads[l];
    s.list_order[l] = static_cast<uint8_t>(h.has_large) |
                      static_cast<uint8_t>(h.binaries_first) << 1;
    const uint32_t *const b = k.watches.arena.data() + h.offset;
    s.lists[l].assign(b, b + h.size);
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    s.clauses.push_back(c->searched);
    for (const Lit x : *c)
      s.clauses.push_back(x);
    ref += clause_words(c->size);
  }
  return s;
}

static bool same(const BookKeeping &a, const BookKeeping &b) {
  return a.lists == b.lists && a.list_order == b.list_order &&
         a.clauses == b.clauses;
}

// Every watched literal has a replacement further along, so scans relocate.
static void build_probing_formula(Kernel &k) {
  kernel_reserve_vars(k, 12);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  static const int cls[][4] = {{1, 3, 4, 5},   {1, 6, 7, 8},  {1, 9, 10, 11},
                               {-2, 3, 6, 9},  {-2, 4, 7, 10}, {1, 2, 5, 12}};
  for (const auto &c : cls) {
    Lit l[4];
    for (int i = 0; i < 4; i++)
      l[i] = internalize(c[i]);
    kernel_add_large_clause(k, l, 4, /*redundant=*/false, /*glue=*/3,
                            ++k.next_clause_id);
  }
}

static void test_probing_propagation_restores_or_persists_pose() {
  // control: the plain engine churns it, so the identity below is not vacuous
  Kernel a;
  ScopedHeuristics _hg_a(a);
  build_probing_formula(a);
  const BookKeeping a0 = bookkeeping(a);
  kernel_assign_decision(a, internalize(-1)); // probe assumption
  assert(!kernel_propagate(a));
  const std::vector<Lit> derived = a.trail;
  kernel_backtrack(a, 0, /*update_phases=*/false);
  assert(!same(bookkeeping(a), a0));

  // The default isolated scope restores bookkeeping.
  Kernel b;
  ScopedHeuristics _hg_b(b);
  build_probing_formula(b);
  const BookKeeping b0 = bookkeeping(b);
  assert(same(b0, a0));
  kernel_probing_begin(b);
  kernel_assign_decision(b, internalize(-1));
  assert(b.h->saved[0] == 0); // probing assignments never become saved phases
  assert(!kernel_propagate_probing(b));
  assert(b.trail == derived); // same implied literals, same trail order
  kernel_backtrack(b, 0, /*update_phases=*/false);
  kernel_probing_end(b);
  assert(same(bookkeeping(b), b0));

  // A second episode repeats exactly.
  kernel_probing_begin(b);
  kernel_assign_decision(b, internalize(-1));
  assert(b.h->saved[0] == 0);
  assert(!kernel_propagate_probing(b));
  assert(b.trail == derived);
  kernel_backtrack(b, 0, /*update_phases=*/false);
  kernel_probing_end(b);
  assert(same(bookkeeping(b), b0));

  // Vivify selects Kissat's persistent-pose scope explicitly.
  Kernel p;
  ScopedHeuristics _hg_p(p);
  build_probing_formula(p);
  kernel_probing_begin(p, /*rollback_pose=*/false);
  kernel_assign_decision(p, internalize(-1));
  assert(!kernel_propagate_probing(p));
  kernel_backtrack(p, 0, /*update_phases=*/false);
  const BookKeeping p1 = bookkeeping(p);
  kernel_probing_end(p);
  assert(same(bookkeeping(p), p1));
  assert(!same(bookkeeping(p), b0));

  // a scope closed on a trail still assigned keeps the updates: they are
  // load-bearing for those literals
  Kernel c;
  ScopedHeuristics _hg_c(c);
  build_probing_formula(c);
  kernel_probing_begin(c);
  kernel_assign_decision(c, internalize(-1));
  assert(!kernel_propagate_probing(c));
  const BookKeeping c1 = bookkeeping(c);
  kernel_probing_end(c); // still at level 1 with the probe assigned
  assert(same(bookkeeping(c), c1));
}

static void test_delayed_relocation_scratch_is_output_sensitive() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 3);
  const Lit clause[3] = {internalize(-1), internalize(2), internalize(3)};
  for (unsigned i = 0; i < 64; i++)
    kernel_add_large_clause(k, clause, 3, false, 0, i + 1);

  // Propagating x1 scans 64 large watches, but every blocking x2 is true.
  // Kissat delays no relocation here, so the scratch must stay empty instead
  // of reserving three words for every possible relocation.
  kernel_assign_unit(k, internalize(2));
  kernel_assign_unit(k, internalize(1));
  assert(!kernel_propagate(k));
  assert(k.delayed_used == 0);
  assert(k.delayed.empty());
}

static void test_default_solve_sat() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  assert(default_solve(k) == 10);
  assert(k.values[internalize(2)] > 0);
  assert(k.values[internalize(1)] > 0);
}

// `kernel_unwatch_large` is the inverse of `push_large`, and
// `kernel_promote_watched_lit` is the unwatch/swap/watch triple built on it.
static bool watched_by(const Kernel &k, Lit l, Ref ref) {
  const WatchHead &h = k.watches.heads[l];
  const uint32_t *ws = k.watches.arena.data() + h.offset;
  for (uint32_t i = 0; i < h.size;) {
    if (Watch{ws[i]}.binary()) {
      i += 1;
    } else {
      if (ws[i + 1] == ref)
        return true;
      i += 2;
    }
  }
  return false;
}

static std::vector<Ref> watched_large_refs(const Kernel &k, Lit l) {
  std::vector<Ref> refs;
  for (const WatchEntry entry : k.watches.entries(l))
    if (!entry.binary())
      refs.push_back(entry.ref());
  return refs;
}

static void test_rebuild_large_watches_restores_arena_order() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit a[3] = {internalize(-1), internalize(2), internalize(3)};
  const Lit b[3] = {internalize(-1), internalize(4), internalize(5)};
  const Ref ra = kernel_add_large_clause(k, a, 3, false, 0);
  const Ref rb = kernel_add_large_clause(k, b, 3, false, 0);
  kernel_unwatch_large(k, internalize(-1), ra);
  k.watches.push_large(internalize(-1), internalize(2), ra);
  assert(watched_large_refs(k, internalize(-1)) ==
         std::vector<Ref>({rb, ra}));
  clause_at(k.arena, ra)->searched = 3;

  kernel_rebuild_large_watches(k);

  assert(watched_large_refs(k, internalize(-1)) ==
         std::vector<Ref>({ra, rb}));
  assert(clause_at(k.arena, ra)->searched == 2);
  kernel_check_invariants(k);
}

static void test_rebuild_large_watches_reselects_root_false_pair() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit root = internalize(1);
  kernel_assign_unit(k, root);
  kernel_assign_unit(k, internalize(2));
  assert(propagate_apply(k) == ApplyResult::Ok);

  // The clause starts with two literals false at root.  Kissat's sparse
  // reconnect moves the two unassigned literals to the watched pair.
  const Lit lits[4] = {negate(root), internalize(-2), internalize(3),
                       internalize(4)};
  const Ref ref = kernel_add_large_clause(k, lits, 4, false, 0);
  Clause *c = clause_at(k.arena, ref);
  assert(c->lits[0] == negate(root));
  assert(c->lits[1] == internalize(-2));

  kernel_rebuild_large_watches(k);

  c = clause_at(k.arena, ref);
  assert(c->lits[0] == internalize(3));
  assert(c->lits[1] == internalize(4));
  assert(watched_by(k, internalize(3), ref));
  assert(watched_by(k, internalize(4), ref));
  assert(!watched_by(k, negate(root), ref));
  kernel_check_invariants(k);
}

static void test_watch_repair_primitive() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  // -1's list holds neighbours on both sides of the removed entry, and a
  // binary too: a large entry is two words and a binary one, so the walk
  // must step entry-wise, not word-wise
  const Lit c1[4] = {internalize(-1), internalize(-2), internalize(-3),
                     internalize(-4)};
  const Lit c2[3] = {internalize(-1), internalize(5), internalize(6)};
  const Lit c3[3] = {internalize(-1), internalize(-5), internalize(-6)};
  const Ref r1 = kernel_add_large_clause(k, c1, 4, false, 0);
  const Ref r2 = kernel_add_large_clause(k, c2, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  const Ref r3 = kernel_add_large_clause(k, c3, 3, false, 0);
  const uint32_t before = k.watches.size(internalize(-1));

  kernel_unwatch_large(k, internalize(-1), r2);
  assert(!watched_by(k, internalize(-1), r2));
  assert(watched_by(k, internalize(-1), r1)); // neighbours survive, both sides
  assert(watched_by(k, internalize(-1), r3));
  assert(k.watches.size(internalize(-1)) == before - 2); // exactly one entry
  assert(watched_by(k, internalize(5), r2)); // r2's other watch is untouched
  k.watches.push_large(internalize(-1), internalize(6), r2); // put it back
  assert(watched_by(k, internalize(-1), r2));

  // promoting position 3 into slot 0 swaps the two and moves the watch
  kernel_promote_watched_lit(k, r1, 0, 3);
  const Clause *c = clause_at(k.arena, r1);
  assert(c->lits[0] == internalize(-4));
  assert(c->lits[3] == internalize(-1));
  assert(c->lits[1] == internalize(-2)); // the other watch is unmoved
  assert(watched_by(k, internalize(-4), r1));
  assert(!watched_by(k, internalize(-1), r1));
  assert(watched_by(k, internalize(-2), r1));
  kernel_check_invariants(k); // every live clause watched on lits[0]/lits[1]

  // the repaired clause still propagates through its new watch pair
  kernel_assign_decision(k, internalize(4));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(2));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(3));
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(k.values[internalize(-1)] > 0); // c1 propagated it
  kernel_check_invariants(k);
}

static void test_canonical_binary_watch_rebuild() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit pivot = internalize(3);
  const Lit higher = internalize(-5);
  const Lit lower2 = internalize(2);
  const Lit lower0 = internalize(1);
  const Lit lower1 = internalize(-1);
  kernel_add_binary_clause(k, pivot, higher);
  kernel_add_binary_clause(k, pivot, lower2);
  kernel_add_binary_clause(k, pivot, lower0);
  kernel_add_binary_clause(k, pivot, lower1);
  const Lit large[3] = {pivot, internalize(4), internalize(6)};
  kernel_add_large_clause(k, large, 3, false, 0);

  std::vector<Lit> before;
  for (const WatchEntry w : k.watches.entries(pivot))
    if (w.binary())
      before.push_back(w.other());
  assert(before == std::vector<Lit>({higher, lower2, lower0, lower1}));

  kernel_canonicalize_binary_watches(k);
  std::vector<Lit> after;
  bool saw_large = false;
  for (const WatchEntry w : k.watches.entries(pivot)) {
    if (w.binary()) {
      assert(!saw_large);
      after.push_back(w.other());
    } else {
      saw_large = true;
    }
  }
  assert(after == std::vector<Lit>({lower0, lower1, lower2, higher}));
  assert(saw_large);
  assert(k.watches.heads[pivot].binaries_first);
  assert(kernel_binary_copies(k, pivot, higher) == 1);
  assert(kernel_binary_copies(k, pivot, internalize(6)) == 0);
  assert(k.stats.congruence_binary_rebuilds == 1);
  kernel_check_invariants(k);

  // Congruence's saved binary stack is pair-sorted after extraction. Model a
  // tumbled import order in which the pivot was imported second: pairs whose
  // other endpoint was imported later must still be sorted by that endpoint,
  // rather than retaining the shuffled order of the pivot's watch list.
  Kernel tumbled;
  kernel_reserve_vars(tumbled, 6);
  for (const Var v : {0u, 1u, 2u, 4u, 3u, 5u})
    kernel_record_import(tumbled, v);
  const Lit p = make_lit(1, false);
  for (const Var v : {5u, 3u, 0u, 4u, 2u})
    kernel_add_binary_clause(tumbled, p, make_lit(v, true));
  kernel_canonicalize_binary_watches(tumbled);
  std::vector<Lit> pair_sorted;
  for (const WatchEntry w : tumbled.watches.entries(p))
    if (w.binary())
      pair_sorted.push_back(w.other());
  assert(pair_sorted ==
         std::vector<Lit>({make_lit(0, true), make_lit(2, true),
                           make_lit(4, true), make_lit(3, true),
                           make_lit(5, true)}));
  kernel_check_invariants(tumbled);
}

static void test_null_stack_dense_flush_retains_root_satisfied_pairs() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 4);
  const Lit root = internalize(-1);
  const Lit satisfied = internalize(2);
  const Lit live_a = internalize(3), live_b = internalize(-4);
  kernel_add_binary_clause(k, root, satisfied, ++k.next_clause_id);
  kernel_add_binary_clause(k, live_a, live_b, ++k.next_clause_id);
  kernel_assign_unit(k, root);

  kernel_deduplicate_binary_watches(k);

  assert(kernel_binary_copies(k, root, satisfied) == 1);
  assert(kernel_binary_copies(k, live_a, live_b) == 1);
  assert(k.stats.binaries_deleted == 0);
  kernel_check_invariants(k);
}

static void test_propagate_beyond_conflicts_for_warmup() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  kernel_add_binary_clause(k, internalize(-2), internalize(3));

  // The first clause conflicts under x1=x2=true. Warm-up propagation must
  // still scan x2's remaining watches and derive x3, without installing a
  // search conflict or changing the ordinary decision counter.
  kernel_assign_decision(k, internalize(1));
  kernel_assign_decision(k, internalize(2));
  const uint64_t decisions = k.stats.decisions;
  const uint64_t ticks_before = k.stats.ticks;
  const uint64_t search_ticks_before = k.stats.search_ticks;
  k.warming = true;
  kernel_propagate_beyond_conflicts(k);
  k.warming = false;

  assert(k.values[internalize(3)] > 0);
  assert(k.h->saved[2] == 1);
  assert(k.stats.warming_conflicts > 0);
  assert(k.stats.warming_propagations == 1);
  // Kissat propbeyond.c charges warm-up work to total ticks only.  In
  // particular it must not shorten the stable-mode search-tick interval.
  assert(k.stats.ticks > ticks_before);
  assert(k.stats.search_ticks == search_ticks_before);
  kernel_charge_ticks(k, 7);
  assert(k.stats.search_ticks == search_ticks_before + 7);
  assert(k.stats.decisions == decisions);
  assert(!k.conflict_valid && !k.inconsistent);
  kernel_backtrack(k, 0);
  kernel_check_invariants(k);
}

static void test_pending_units_borrow_and_restore_literal_marks() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit a = internalize(1), b = internalize(-2);
  {
    PendingUnits pending;
    pending.init(k);
    assert(pending.ids.empty()); // proof-less/DRAT batches stay sparse
    assert(pending.record(k, a, 11) == PendingUnits::Outcome::Recorded);
    assert(pending.record(k, a, 12) == PendingUnits::Outcome::Duplicate);
    assert(pending.record(k, b, 13) == PendingUnits::Outcome::Recorded);
    assert(k.eager_marks[a] && k.eager_marks[b]);
    pending.init(k); // resetting a batch restores every borrowed byte
    assert(!k.eager_marks[a] && !k.eager_marks[b]);
  }
  assert(std::all_of(k.eager_marks.begin(), k.eager_marks.end(),
                     [](uint8_t mark) { return !mark; }));

  PendingUnits opposite;
  opposite.init(k);
  assert(opposite.record(k, a, 21) == PendingUnits::Outcome::Recorded);
  assert(opposite.record(k, negate(a), 22) ==
         PendingUnits::Outcome::Refuted);
  assert(k.inconsistent);
}

static void test_watch_batch_reserve_preserves_prefixes_and_fills_exactly() {
  WatchLists watches;
  watches.resize_lits(4);
  watches.push_binary(0, 1);
  watches.push_large(0, 2, 17);
  watches.push_binary(1, 0);
  watches.push_binary(2, 3);

  const std::vector<uint32_t> prefix0(watches.begin(0), watches.end(0));
  const std::vector<uint32_t> prefix1(watches.begin(1), watches.end(1));
  const uint32_t size0 = watches.size(0), size1 = watches.size(1);
  watches.begin_batch_reserve();
  watches.reserve_batch_word(0);
  watches.reserve_batch_word(0);
  watches.reserve_batch_word(1);
  watches.finish_batch_reserve();

  assert(std::vector<uint32_t>(watches.begin(0), watches.end(0)) == prefix0);
  assert(std::vector<uint32_t>(watches.begin(1), watches.end(1)) == prefix1);
  assert(watches.heads[0].capacity == size0 + 2);
  assert(watches.heads[1].capacity == size1 + 1);
  const size_t arena_size = watches.arena.size();
  watches.push_large(0, 3, 23);
  watches.push_binary(1, 2);
  assert(watches.arena.size() == arena_size);
  assert(watches.heads[0].size == watches.heads[0].capacity);
  assert(watches.heads[1].size == watches.heads[1].capacity);
  assert(watches.holes == 0);

  watches.begin_batch_reserve();
  watches.finish_batch_reserve(false);
  const size_t exact_size = watches.arena.size();
  assert(watches.arena.capacity() == exact_size);
  watches.push_binary(0, 1);
  assert(watches.arena.capacity() >= exact_size + (exact_size + 2) / 3);
}

int main() {
  test_variable_identity_maps();
  test_variable_compaction();
  test_variable_compaction_preserves_directed_binary_order();
  test_variable_compaction_replays_inherited_arena_order();
  test_compaction_defers_retired_jumped_reason();
  test_compaction_drops_unassigned_stale_reason();
  test_lit_encoding();
  test_unit_chain();
  test_root_sparse_collection();
  test_root_sparse_collection_keeps_native_order();
  test_root_sparse_collection_ignores_nonroot_values();
  test_root_sparse_collection_rewrites_nonroot_reason();
  test_sparse_collection_flushes_fixed_prefix_watch();
  test_sparse_collection_partitions_mixed_tail();
  test_root_sparse_collection_partitions_rewritten_tail();
  test_root_sparse_collection_collects_existing_garbage();
  test_large_clause_propagation();
  test_root_conflict_is_unsat();
  test_backtrack_restores();
  test_probing_does_not_save_phases();
  test_probing_propagation_restores_or_persists_pose();
  test_delayed_relocation_scratch_is_output_sensitive();
  test_default_solve_sat();
  test_rebuild_large_watches_restores_arena_order();
  test_rebuild_large_watches_reselects_root_false_pair();
  test_watch_repair_primitive();
  test_canonical_binary_watch_rebuild();
  test_null_stack_dense_flush_retains_root_satisfied_pairs();
  test_propagate_beyond_conflicts_for_warmup();
  test_pending_units_borrow_and_restore_literal_marks();
  test_watch_batch_reserve_preserves_prefixes_and_fills_exactly();
  puts("test_kernel OK");
  return 0;
}
