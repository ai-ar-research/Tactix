// Kernel unit tests: literal encoding, watch scheme, propagation chains,
// root-level conflict detection.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "policies/default_policy.hpp"
#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <unistd.h>

using namespace tactix;

static void test_irredundant_clause_never_stores_glue() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit lits[3] = {internalize(1), internalize(2), internalize(3)};
  const Ref irred =
      kernel_add_large_clause(k, lits, 3, /*redundant=*/false, /*glue=*/7);
  const Ref red =
      kernel_add_large_clause(k, lits, 3, /*redundant=*/true, /*glue=*/7);
  assert(!clause_at(k.arena, irred)->redundant());
  assert(clause_at(k.arena, irred)->glue() == 0);
  assert(clause_at(k.arena, red)->redundant());
  assert(clause_at(k.arena, red)->glue() == 7);
}

static void test_active_population_is_live_state() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_materialize_cold_state(k);
  assert(kernel_active_variables(k) == 4);
  k.var_status.assign(k.nvars, VarStatus::Active);
  k.var_status[1] = VarStatus::Eliminated;
  // Historical counters survive compaction and therefore are intentionally
  // not a source of truth for Kissat's live `solver->active` predicate.
  k.stats.bve_eliminated = 100;
  k.stats.substitute_vars = 100;
  assert(kernel_active_variables(k) == 3);
  const Var fresh = kernel_new_extension_vars(k, 1);
  assert(fresh == 4);
  assert(kernel_active_variables(k) == 4 &&
         "Factor's fresh activation must trigger Probe round two");
}

static void test_compact_watch_head_encoding_is_full_width() {
  WatchHead head;
  head.size = UINT32_MAX;
  head.set_layout(UINT32_MAX);
  assert(head.binary_count() == UINT32_MAX);
  assert(head.binaries_first());
  assert(!head.has_large());

  head.set_layout(UINT32_MAX - 2, false);
  assert(head.binary_count() == UINT32_MAX - 2);
  assert(!head.binaries_first());
  assert(head.has_large());

  head.set_layout(UINT32_MAX, false);
  assert(head.binary_count() == UINT32_MAX);
  assert(!head.binaries_first());
  assert(!head.has_large());

  head.size = UINT32_MAX - 1;
  head.set_layout(UINT32_MAX - 3, false);
  assert(head.binary_count() == UINT32_MAX - 3);
  assert(!head.binaries_first());
  assert(head.has_large());

  WatchLists lists;
  lists.resize_lits(1);
  lists.heads[0].size = 3;
  lists.heads[0].set_layout(1, false);
  lists.truncate_large(0, 1);
  assert(lists.heads[0].binary_count() == 1);
  assert(!lists.heads[0].binaries_first());
  assert(!lists.heads[0].has_large());
}

static void test_clause_position_alias_pages_are_exact_and_sparse() {
  ClausePositionAliases aliases;
  assert(aliases.get(7) == 0);
  aliases.set(7, 3);
  aliases.set((uint64_t{1} << 20) + 11, UINT64_C(0xfedcba9876543210));
  assert(aliases.get(7) == 3);
  assert(aliases.get(8) == 0);
  assert(aliases.get(uint64_t{1} << 20) == 0);
  assert(aliases.get((uint64_t{1} << 20) + 11) ==
         UINT64_C(0xfedcba9876543210));
}

static void test_binary_liveness_filter_has_no_false_negatives() {
  BinaryLivenessIndex filter;
  filter.reserve(3);
  const uint64_t first = kernel_binary_key(2, 9);
  const uint64_t second = kernel_binary_key(4, 11);
  filter.add(first);
  filter.add(second);
  assert(filter.maybe(first));
  assert(filter.maybe(second));
  // Deletion deliberately leaves a stale positive: exact watch-list
  // confirmation owns liveness and prevents a stale bit changing semantics.
  filter.remove(first);
  assert(filter.maybe(first));
}

static void test_reserved_trail_stack_contract() {
  ReservedTrail trail;
  trail.reserve(5);
  assert(trail.empty() && trail.capacity() == 5);
  trail.push_back(7);
  trail.push_back(3);
  trail.push_back(9);
  assert(trail.size() == 3 && trail.back() == 9);
  assert((std::vector<Lit>(trail) == std::vector<Lit>{7, 3, 9}));

  trail.resize(2);
  trail.push_back(11);
  trail.pop_back();
  assert((std::vector<Lit>(trail) == std::vector<Lit>{7, 3}));

  std::vector<Lit> compacted;
  compacted.reserve(4);
  compacted.push_back(5);
  compacted.push_back(1);
  trail.swap(compacted);
  assert(trail.capacity() == 4);
  trail.push_back(8);
  assert((std::vector<Lit>(trail) == std::vector<Lit>{5, 1, 8}));
}

static Kernel binary_conflict_kernel() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  kernel_assign_decision(k, internalize(1));
  kernel_assign_decision(k, internalize(2));
  assert(k.propagate_head == 0 && k.trail.size() == 2);
  return k;
}

static void test_conflicting_propagation_consumes_trail_literal() {
  {
    Kernel k = binary_conflict_kernel();
    assert(kernel_propagate(k));
    assert(k.propagate_head == 1 &&
           "search propagation must publish Kissat's post-incremented cursor");
    assert(k.stats.propagations == 1 &&
           "propagations count scanned trail literals, not implications");
    kernel_backtrack(k, 0);
    assert(k.propagate_head == 0 &&
           "backtrack, not the conflict scan, owns the surviving-frame rewind");
  }
  {
    Kernel k = binary_conflict_kernel();
    kernel_probing_begin(k, false);
    assert(kernel_propagate_probing(k));
    assert(k.propagate_head == 1 &&
           "probing propagation must publish Kissat's post-incremented cursor");
    assert(k.stats.propagations == 1);
    kernel_probing_end(k);
  }
  {
    Kernel k = binary_conflict_kernel();
    assert(kernel_propagate_binary(k));
    assert(k.propagate_head == 1 && k.binary_only_head == 0 &&
           "binary propagation must advance while retaining its full-scan debt");
    assert(k.stats.propagations == 1);
  }
}

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

static void test_import_order_survives_compaction_linearly() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 6);
  kernel_record_import(k, 4);
  kernel_record_import(k, 1);
  kernel_record_import(k, 5);
  assert((k.internal_import_rank ==
          std::vector<uint32_t>{UINT32_MAX, 1, UINT32_MAX, UINT32_MAX, 0, 2}));

  std::vector<Var> order;
  kernel_vars_in_import_order(k, order);
  assert((order == std::vector<Var>{4, 1, 5, 0, 2, 3}));

  // Retire the earliest imported identity and one unrecorded identity.  The
  // surviving imported variables keep their first-occurrence order, followed
  // by surviving unrecorded variables in external order, after remapping to
  // compact internal identities.
  k.var_status.assign(k.nvars, VarStatus::Active);
  for (const Var v : {Var{4}, Var{2}}) {
    k.var_status[v] = VarStatus::Eliminated;
    heuristics_deactivate_var(k, v);
    k.unassigned_active--;
  }
  assert(kernel_compact_variables(k));
  assert((k.internal_to_external == std::vector<Var>{0, 1, 3, 5}));
  assert((k.internal_import_rank ==
          std::vector<uint32_t>{UINT32_MAX, 1, UINT32_MAX, 2}));
  kernel_vars_in_import_order(k, order);
  assert((order == std::vector<Var>{1, 3, 0, 2}));
  kernel_check_invariants(k);
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
  // Kissat remaps (rather than resets) the focused queue search cache during
  // compact_queue.  Preserve the same external candidate across renumbering.
  hg.h.search_idx = 4;
  hg.h.search_stamp = hg.h.links[4].stamp;
  // Kissat recomputes compacted target/best high-water marks from the
  // surviving nonzero phase entries.  Retired x2/x4 must not count, while the
  // mapped entries for x1/x3/x5 must retain both their values and population.
  hg.h.target = {1, -1, -1, 1, 0};
  hg.h.best = {0, 1, -1, -1, 1};
  hg.h.target_assigned = 4;
  hg.h.best_assigned = 4;
  // Factor marks are persistent per-polarity variable flags in Kissat.  Two
  // surviving asymmetric marks must follow their external literals through
  // renumbering, while a mark on retired x4 disappears.  The material
  // watermark remains cumulative rather than recounting the compacted flags.
  std::fill(k.factor_marks.begin(), k.factor_marks.end(), 0);
  k.factor_literals_marked = 0;
  kernel_mark_factor_literal(k, internalize(-1));
  kernel_mark_factor_literal(k, internalize(3));
  kernel_mark_factor_literal(k, internalize(-4));
  assert(k.factor_literals_marked == 3);
  assert(kernel_should_compact_variables(k, 10));
  assert(kernel_compact_variables(k));

  assert(k.nvars == 3);
  assert(k.internal_to_external == std::vector<Var>({0, 2, 4}));
  assert(k.external_to_internal ==
         std::vector<Var>({0, INVALID_VAR, 1, INVALID_VAR, 2}));
  assert(kernel_internalize(k, 3) == make_lit(1, false));
  assert(kernel_externalize(k, make_lit(2, true)) == -5);
  assert(kernel_externalize(k, make_lit(hg.h.search_idx, false)) == 5);
  assert((hg.h.target == std::vector<int8_t>{1, -1, 0}));
  assert((hg.h.best == std::vector<int8_t>{0, -1, 1}));
  assert(hg.h.target_assigned == 2);
  assert(hg.h.best_assigned == 2);
  assert(k.factor_marks.size() == 2 * k.nvars);
  assert(k.factor_marks[kernel_internalize(k, -1)]);
  assert(k.factor_marks[kernel_internalize(k, 3)]);
  assert(k.factor_literals_marked == 3);
  uint32_t compacted_factor_marks = 0;
  for (const uint8_t marked : k.factor_marks)
    compacted_factor_marks += marked != 0;
  assert(compacted_factor_marks == 2);
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

static void test_compaction_collapses_root_fixed_admission_and_rewatches() {
  Kernel k;
  ScopedHeuristics hg(k);
  kernel_reserve_vars(k, 11);
  const Lit target[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, target, 3, false, 0);
  kernel_assign_unit(k, internalize(-1));
  kernel_assign_unit(k, internalize(11));
  hg.h.search_idx = 2;
  hg.h.search_stamp = hg.h.links[2].stamp;
  // Kissat's initial sparse collection maps both root-fixed variables onto one
  // dense representative before any Reduce can sample compactlim.
  heuristics_compact_random_order_fixed(k);
  assert(!kernel_should_compact_variables(k, 10));
  assert(kernel_compact_variables(k));
  assert(k.nvars == 11 && k.stats.variable_compactions == 1);
  assert(hg.h.search_idx == 2 && hg.h.search_stamp == hg.h.links[2].stamp);
  const Clause *clause = clause_at(k.arena, 0);
  assert(clause->lits[0] != internalize(1) &&
         "a compacting collection must rewatch the whole arena");
}

static void test_compaction_admission_uses_post_startup_population() {
  Kernel k;
  ScopedHeuristics hg(k);
  kernel_reserve_vars(k, 25);

  // Model a sparse DIMACS header: Kissat imported only the first 20 variables,
  // while Tactix retains five absent external identities for its public model
  // surface.  They have already been consumed by the virtual initial compact
  // population and must never become fresh compactlim debt.
  std::vector<Var> imported;
  for (Var v = 0; v < 20; v++)
    imported.push_back(v);
  heuristics_seed_activation_order(k, imported);
  k.var_status.assign(k.nvars, VarStatus::Active);
  for (Var v = 20; v < 25; v++) {
    k.var_status[v] = VarStatus::Eliminated;
    heuristics_deactivate_var(k, v);
    k.unassigned_active--;
  }

  // Three startup units become one Kissat fixed representative: virtual vars
  // is 18, virtual active is 17, and strict compactlim is 1 > floor(1.8), false.
  for (int external : {1, 2, 3})
    kernel_assign_unit(k, internalize(external));
  heuristics_compact_random_order_fixed(k);
  assert(k.compact_import_order.size() == 18);
  assert(!kernel_should_compact_variables(k, 10));

  // A retirement after that baseline is real new debt.  Two inactive of 18 is
  // strictly above floor(10% * 18), exactly Kissat's admission boundary.
  k.var_status[3] = VarStatus::Eliminated;
  heuristics_deactivate_var(k, 3);
  k.unassigned_active--;
  assert(kernel_should_compact_variables(k, 10));
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
// Tactix appends the replacement and retains that native position in a
// generation-local alias. Compaction must replay the current source order
// and must not leak an independently configured inprocessing order.
static void test_variable_compaction_replays_inherited_arena_order() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 13);
  const Lit shared = internalize(7);
  const Lit source[3] = {shared, internalize(3), internalize(4)};
  const Lit middle[3] = {shared, internalize(5), internalize(6)};
  const Lit later[3] = {shared, internalize(4), internalize(5)};
  const Lit replacement[3] = {shared, internalize(3), internalize(6)};
  const Ref source_ref = kernel_add_large_clause(k, source, 3, false, 0, 101);
  kernel_add_large_clause(k, middle, 3, false, 0, 102);
  kernel_add_large_clause(k, later, 3, false, 0, 103);
  kernel_add_large_clause(k, replacement, 3, false, 0, 104);
  kernel_inherit_clause_position(k, 104, source_ref);
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

// Kissat rewrites a shortened large clause in place. Tactix allocates its
// packed replacement at the arena tail, so production whole-arena collection
// must replay the inherited source position even without an optional policy.
static void test_whole_collection_replays_inherited_creation_order() {
  Kernel k;
  kernel_reserve_vars(k, 7);
  const Lit shared = internalize(1);
  const Lit source[3] = {shared, internalize(2), internalize(3)};
  const Lit middle[3] = {shared, internalize(4), internalize(5)};
  const Lit later[3] = {shared, internalize(6), internalize(7)};
  const Lit replacement[3] = {shared, internalize(2), internalize(7)};
  const Ref source_ref =
      kernel_add_large_clause(k, source, 3, false, 0, 101);
  kernel_add_large_clause(k, middle, 3, false, 0, 102);
  kernel_add_large_clause(k, later, 3, false, 0, 103);
  kernel_add_large_clause(k, replacement, 3, false, 0, 104);
  kernel_inherit_clause_position(k, 104, source_ref);
  kernel_delete_clause(k, clause_at(k.arena, source_ref));

  kernel_collect_garbage(k);
  assert(k.clause_position_alias.get(104) == 0 &&
         "collection must materialize and clear generation-local aliases");

  std::vector<uint64_t> arena_ids;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    arena_ids.push_back(c->id());
    ref += clause_actual_words(c);
  }
  assert((arena_ids == std::vector<uint64_t>{104, 102, 103}));
  std::vector<uint64_t> watched_ids;
  for (const WatchEntry watch : k.watches.entries(shared))
    if (!watch.binary())
      watched_ids.push_back(clause_at(k.arena, watch.ref())->id());
  assert(watched_ids == arena_ids);
  kernel_check_invariants(k);
}

// Kissat's sparse Reduce collection permanently moves surviving irredundant
// tail clauses ahead of surviving redundant ones. A later dense/whole
// collection copies that current arena order; immutable proof IDs must not
// restore the pre-Reduce creation sequence.
static void test_whole_collection_preserves_prior_sparse_partition() {
  Kernel k;
  kernel_reserve_vars(k, 9);
  const Lit shared = internalize(1);
  const Lit i0[3] = {shared, internalize(2), internalize(3)};
  const Lit r1[3] = {shared, internalize(4), internalize(5)};
  const Lit i2[3] = {shared, internalize(6), internalize(7)};
  const Lit r3[3] = {shared, internalize(8), internalize(9)};
  kernel_add_large_clause(k, i0, 3, false, 0, 101);
  const Ref start = kernel_add_large_clause(k, r1, 3, true, 8, 102);
  kernel_add_large_clause(k, i2, 3, false, 0, 103);
  kernel_add_large_clause(k, r3, 3, true, 8, 104);

  const auto arena_ids = [&] {
    std::vector<uint64_t> ids;
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      ids.push_back(c->id());
      ref += clause_actual_words(c);
    }
    return ids;
  };

  kernel_collect_garbage(k, start);
  const std::vector<uint64_t> sparse_order{101, 103, 102, 104};
  assert(arena_ids() == sparse_order);
  kernel_collect_garbage(k);
  assert(arena_ids() == sparse_order &&
         "whole collection must retain the current Kissat arena order");
  kernel_check_invariants(k);
}

// Kissat's compactlim path invokes the same sparse collector with start=0.
// It must partition the entire current arena, not silently become a dense
// whole-arena collection merely because the watermark is numerically zero.
static void test_sparse_collection_from_zero_partitions_whole_arena() {
  Kernel k;
  kernel_reserve_vars(k, 9);
  const Lit shared = internalize(1);
  const Lit i0[3] = {shared, internalize(2), internalize(3)};
  const Lit r1[3] = {shared, internalize(4), internalize(5)};
  const Lit i2[3] = {shared, internalize(6), internalize(7)};
  const Lit r3[3] = {shared, internalize(8), internalize(9)};
  kernel_add_large_clause(k, i0, 3, false, 0, 111);
  kernel_add_large_clause(k, r1, 3, true, 8, 112);
  kernel_add_large_clause(k, i2, 3, false, 0, 113);
  kernel_add_large_clause(k, r3, 3, true, 8, 114);

  kernel_collect_garbage(k, 0, /*sparse_collection=*/true);
  std::vector<uint64_t> arena_ids;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    arena_ids.push_back(c->id());
    assert(c->searched == 2 && "start=0 must rewatch every clause");
    ref += clause_actual_words(c);
  }
  assert((arena_ids == std::vector<uint64_t>{111, 113, 112, 114}));
  kernel_check_invariants(k);
}

// An OTFS-style packed replacement can be appended after Reduce has already
// acquired clauses that follow its source. Kissat shortened the source in
// place, so a sparse collection beginning later retains both its prefix watch
// position and search cursor. Tactix must use the inherited current position,
// not the replacement record's physical tail address.
static void test_sparse_collection_retains_pending_prefix_replacement() {
  Kernel k;
  kernel_reserve_vars(k, 9);
  const Lit shared = internalize(1);
  const Lit source[3] = {shared, internalize(2), internalize(3)};
  const Lit middle[3] = {shared, internalize(4), internalize(5)};
  const Lit first_redundant[3] = {shared, internalize(6), internalize(7)};
  const Lit tail[3] = {shared, internalize(8), internalize(9)};
  const Lit replacement[3] = {shared, internalize(2), internalize(9)};
  const Ref source_ref =
      kernel_add_large_clause(k, source, 3, false, 0, 121);
  kernel_add_large_clause(k, middle, 3, false, 0, 122);
  const Ref start =
      kernel_add_large_clause(k, first_redundant, 3, true, 8, 123);
  kernel_add_large_clause(k, tail, 3, true, 8, 124);
  const Ref replacement_ref =
      kernel_add_large_clause(k, replacement, 3, false, 0, 125);
  kernel_inherit_clause_position(k, 125, source_ref);
  // OTFS exchanges the replacement's entry with the source entry on its
  // surviving watch before retiring the source.
  kernel_swap_large_watch_refs(k, shared, source_ref, replacement_ref,
                               source[1], replacement[1]);
  kernel_delete_clause(k, clause_at(k.arena, source_ref));
  clause_at(k.arena, replacement_ref)->searched = 7;

  struct ReverseOrder final : ArenaOrdering {
    void order(const Kernel &, std::vector<Ref> &refs) override {
      std::reverse(refs.begin(), refs.end());
    }
  } reverse;
  k.arena_ordering = &reverse;
  kernel_collect_garbage(k, start, /*sparse_collection=*/true);
  assert(k.arena_ordering == &reverse);
  assert(k.clause_position_alias.get(125) == 0);

  std::vector<uint64_t> arena_ids;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    arena_ids.push_back(c->id());
    if (c->id() == 125)
      assert(c->searched == 7 &&
             "logical sparse prefix must retain the search cursor");
    ref += clause_actual_words(c);
  }
  assert((arena_ids == std::vector<uint64_t>{125, 122, 123, 124}));

  std::vector<uint64_t> watched_ids;
  for (const WatchEntry watch : k.watches.entries(shared))
    if (!watch.binary())
      watched_ids.push_back(clause_at(k.arena, watch.ref())->id());
  assert((watched_ids == std::vector<uint64_t>{125, 122, 123, 124}));
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
  const Clause *b = clause_at(k.arena, clause_actual_words(a));
  const Clause *c = clause_at(k.arena, clause_actual_words(a) +
                                           clause_actual_words(b));
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
    ref += clause_actual_words(c);
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

// A physically interleaved irredundant clause after the Reduce watermark is
// part of Kissat's swept tail. Root collection must flush it even though its
// redundancy bit would have classified it as part of a logical prefix.
static void test_root_sparse_collection_sweeps_physical_irredundant_tail() {
  Kernel k;
  kernel_reserve_vars(k, 7);
  const Lit root = internalize(1);
  kernel_assign_unit(k, root);
  const Lit prefix[3] = {internalize(2), internalize(3), internalize(4)};
  const Lit victim_lits[3] = {internalize(3), internalize(4),
                              internalize(5)};
  const Lit logical_prefix[3] = {root, internalize(6), internalize(7)};
  kernel_add_large_clause(k, prefix, 3, false, 0, 151);
  const Ref start =
      kernel_add_large_clause(k, victim_lits, 3, true, 7, 152);
  kernel_add_large_clause(k, logical_prefix, 3, false, 0, 153);
  kernel_delete_clause(k, clause_at(k.arena, start));

  assert(kernel_sparse_collect_root(k, start) == RootCollectOutcome::Changed);
  bool retained = false;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    retained |= c->id() == 153;
    ref += clause_actual_words(c);
  }
  assert(!retained);
  kernel_check_invariants(k);
}

// Kissat's sparse collector keeps only the unswept current prefix, then stably
// partitions and rewatches every tail record. An interleaved irredundant
// clause is part of that tail, and the diagnostic arena policy is ignored.
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
  const Ref irred_first_ref =
      kernel_add_large_clause(k, irred_first, 3, false, 0, 203);
  kernel_add_large_clause(k, red_second, 3, true, 8, 204);
  kernel_add_large_clause(k, irred_second, 3, false, 0, 205);

  // Model a search relocation before Reduce.  Both irredundant clauses are in
  // the physical tail, so collection discards this order and rewatches them.
  kernel_unwatch_large(k, shared, irred_first_ref);
  k.watches.push_large(shared, internalize(4), irred_first_ref);

  ReverseArenaOrder reverse;
  k.arena_ordering = &reverse;
  kernel_collect_garbage(k, start);
  assert(k.arena_ordering == &reverse);

  std::vector<uint64_t> arena_ids;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    arena_ids.push_back(c->id());
    ref += clause_actual_words(c);
  }
  assert((arena_ids == std::vector<uint64_t>{201, 203, 205, 202, 204}));

  std::vector<uint64_t> watched_ids;
  for (const WatchEntry watch : k.watches.entries(shared))
    if (!watch.binary())
      watched_ids.push_back(clause_at(k.arena, watch.ref())->id());
  assert((watched_ids == std::vector<uint64_t>{201, 203, 205, 202, 204}));
  kernel_check_invariants(k);
}

// A watch relocation may leave the opposite entry caching the old watched
// literal.  If that irredundant clause lies in the physical Reduce tail,
// Kissat rebuilds it and restores the current watched partner as blocker.
static void test_sparse_collection_rebuilds_tail_irredundant_blocker() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 7);
  const Lit a = internalize(1), b = internalize(2), c = internalize(3);
  const Lit prefix[3] = {internalize(4), internalize(5), internalize(6)};
  const Lit redundant[3] = {internalize(4), internalize(6), internalize(7)};
  const Lit focal[3] = {a, b, c};
  kernel_add_large_clause(k, prefix, 3, false, 0, 161);
  const Ref start = kernel_add_large_clause(k, redundant, 3, true, 7, 162);
  const Ref focal_ref = kernel_add_large_clause(k, focal, 3, false, 0, 163);

  kernel_assign_decision(k, negate(a));
  assert(propagate_apply(k) == ApplyResult::Ok);
  const Clause *posed = clause_at(k.arena, focal_ref);
  assert(posed->lits[0] == b && posed->lits[1] == c);

  const auto blocker_for = [&](Lit watched, uint64_t id) {
    for (const WatchEntry watch : k.watches.entries(watched))
      if (!watch.binary() && clause_at(k.arena, watch.ref())->id() == id)
        return watch.blocking();
    return INVALID_LIT;
  };
  assert(blocker_for(b, 163) == a);

  kernel_collect_garbage(k, start);
  assert(blocker_for(b, 163) == c);
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
    ref += clause_actual_words(c);
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
  kernel_reserve_vars(k, 9);
  kernel_assign_unit(k, internalize(1));
  const Lit shared = internalize(2);
  const Lit prefix[3] = {shared, internalize(3), internalize(4)};
  const Lit red_first[3] = {shared, internalize(4), internalize(5)};
  const Lit source_lits[3] = {shared, internalize(5), internalize(6)};
  const Lit victim_lits[3] = {shared, internalize(6), internalize(7)};
  const Lit middle_lits[3] = {shared, internalize(7), internalize(8)};
  const Lit replacement_lits[3] = {shared, internalize(8), internalize(9)};
  kernel_add_large_clause(k, prefix, 3, false, 0, 401);
  const Ref start = kernel_add_large_clause(k, red_first, 3, true, 7, 402);
  const Ref source =
      kernel_add_large_clause(k, source_lits, 3, false, 0, 403);
  const Ref victim =
      kernel_add_large_clause(k, victim_lits, 3, true, 8, 404);
  kernel_add_large_clause(k, middle_lits, 3, false, 0, 405);
  kernel_add_large_clause(k, replacement_lits, 3, false, 0, 406);
  kernel_inherit_clause_position(k, 406, source);
  kernel_delete_clause(k, clause_at(k.arena, source));
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
    ref += clause_actual_words(c);
  }
  assert((arena_ids == std::vector<uint64_t>{401, 406, 405, 402}));

  std::vector<uint64_t> watched_ids;
  for (const WatchEntry watch : k.watches.entries(shared))
    if (!watch.binary())
      watched_ids.push_back(clause_at(k.arena, watch.ref())->id());
  // The inherited source position restores arena enumeration, and every
  // physical tail clause is rewatched in that resulting order.
  assert((watched_ids == std::vector<uint64_t>{401, 406, 405, 402}));
  kernel_check_invariants(k);
}

// Root sweeping uses the same current-position boundary. A replacement that
// Tactix appended after the watermark but Kissat shortened in the retained
// prefix must not be deleted as root-satisfied during this Reduce.
static void test_root_sparse_collection_retains_pending_prefix_replacement() {
  Kernel k;
  kernel_reserve_vars(k, 9);
  kernel_assign_unit(k, internalize(1));
  const Lit shared = internalize(2);
  const Lit source[3] = {shared, internalize(3), internalize(1)};
  const Lit middle[3] = {shared, internalize(4), internalize(5)};
  const Lit redundant[3] = {shared, internalize(6), internalize(7)};
  const Lit replacement[3] = {shared, internalize(8), internalize(1)};
  const Ref source_ref =
      kernel_add_large_clause(k, source, 3, false, 0, 411);
  kernel_add_large_clause(k, middle, 3, false, 0, 412);
  const Ref start =
      kernel_add_large_clause(k, redundant, 3, true, 8, 413);
  const Ref replacement_ref =
      kernel_add_large_clause(k, replacement, 3, false, 0, 414);
  kernel_inherit_clause_position(k, 414, source_ref);
  kernel_swap_large_watch_refs(k, shared, source_ref, replacement_ref,
                               source[1], replacement[1]);
  kernel_delete_clause(k, clause_at(k.arena, source_ref));

  assert(kernel_sparse_collect_root(k, start) == RootCollectOutcome::NoChange);
  const Clause *front = clause_at(k.arena, 0);
  assert(front->id() == 414 && front->size == 3);
  assert(std::find(front->begin(), front->end(), internalize(1)) !=
             front->end() &&
         "the logical sparse prefix must not be root-flushed");
  kernel_check_invariants(k);
}

// A retired clause must be retired exactly ONCE on the proof channel too. A
// checker retires one clause per `d` line and, under DRAT, selects it by the
// literals, so a second deletion of an already-garbage record silently retires
// the live duplicate the solver keeps — after which the next lemma resolving
// through it is not RUP. Regression for the certificate rejected on
// tierC/asconhashv12 (benchmarks/runs/20260818-1930-BC-inherited-sparse).
static void test_repeated_clause_deletion_emits_one_proof_line() {
  Kernel k;
  ScopedHeuristics _hg(k);
  Proof proof;
  const std::string path = "/tmp/tactix-delete-idempotence-" +
                           std::to_string(static_cast<long>(getpid())) +
                           ".drat";
  assert(proof.open(path.c_str(), ProofFormat::DratAscii));
  k.proof = &proof;
  kernel_reserve_vars(k, 4);
  const Lit lits[3] = {internalize(1), internalize(2), internalize(3)};
  // Two live records with the SAME literals: exactly the state in which a
  // surplus deletion is silent rather than a checker warning.
  const Ref first = kernel_add_large_clause(k, lits, 3, true, 3, 11);
  const Ref duplicate = kernel_add_large_clause(k, lits, 3, true, 3, 12);
  kernel_delete_clause(k, clause_at(k.arena, first));
  kernel_delete_clause(k, clause_at(k.arena, first)); // idempotent re-delete
  assert(clause_at(k.arena, first)->garbage());
  assert(!clause_at(k.arena, duplicate)->garbage());
  proof.flush();
  FILE *f = fopen(path.c_str(), "r");
  assert(f);
  char buf[256];
  int dels = 0;
  while (fgets(buf, sizeof buf, f))
    if (buf[0] == 'd')
      dels++;
  fclose(f);
  remove(path.c_str());
  assert(dels == 1); // the surviving duplicate stays in the checker
  k.proof = nullptr;
}

// `searched` carries a forwarding pointer only for a record the collection
// COPIED. An OTFS reason retired before Backjump drains the register keeps its
// search cursor there, so forwarding it unconditionally lands inside another
// clause's literals; the register's own consumer already skips a garbage
// record, so the collector must drop it.
static void test_garbage_collection_drops_retired_otfs_reason() {
  Kernel k;
  ScopedHeuristics _hg(k);
  kernel_reserve_vars(k, 8);
  const Lit lead[3] = {internalize(1), internalize(2), internalize(3)};
  const Lit retired[3] = {internalize(2), internalize(3), internalize(4)};
  const Lit live[3] = {internalize(4), internalize(5), internalize(6)};
  kernel_add_large_clause(k, lead, 3, false, 0, 21);
  const Ref stale = kernel_add_large_clause(k, retired, 3, true, 4, 22);
  const Ref kept = kernel_add_large_clause(k, live, 3, true, 4, 23);
  k.otfs_retired = {stale, kept};
  // Some other rule retires the register entry before Backjump reaches it.
  kernel_delete_clause(k, clause_at(k.arena, stale));
  const uint32_t cursor = clause_at(k.arena, stale)->searched;
  assert(cursor < clause_words(3)); // a cursor, not a record start

  kernel_collect_garbage(k);

  assert(k.otfs_retired.size() == 1);
  const Ref forwarded = k.otfs_retired[0];
  assert(clause_at(k.arena, forwarded)->id() == 23);
  // Every surviving entry is a real record START, which is what the register's
  // consumer dereferences.
  bool is_record_start = false;
  for (Ref ref = 0; ref < k.arena.size();) {
    is_record_start |= ref == forwarded;
    ref += clause_actual_words(clause_at(k.arena, ref));
  }
  assert(is_record_start);
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
  std::vector<uint32_t> binary_counts;
  std::vector<uint8_t> list_order;
  std::vector<uint32_t> clauses;
};

static BookKeeping bookkeeping(const Kernel &k) {
  BookKeeping s;
  s.lists.resize(2 * k.nvars);
  s.binary_counts.resize(2 * k.nvars);
  s.list_order.resize(2 * k.nvars);
  for (Lit l = 0; l < 2 * k.nvars; l++) {
    const WatchHead &h = k.watches.heads[l];
    s.binary_counts[l] = k.watches.binary_size(l);
    s.list_order[l] = static_cast<uint8_t>(h.has_large()) |
                      static_cast<uint8_t>(h.binaries_first()) << 1;
    const uint32_t *const b = k.watches.arena.data() + h.offset;
    s.lists[l].assign(b, b + h.size);
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    s.clauses.push_back(c->searched);
    for (const Lit x : *c)
      s.clauses.push_back(x);
    ref += clause_actual_words(c);
  }
  return s;
}

static bool same(const BookKeeping &a, const BookKeeping &b) {
  return a.lists == b.lists && a.binary_counts == b.binary_counts &&
         a.list_order == b.list_order &&
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

static void test_phase_rebuild_large_watches_restores_source_order() {
  Kernel k;
  kernel_reserve_vars(k, 7);
  const Lit shared = internalize(-1);
  const Lit a[3] = {shared, internalize(2), internalize(3)};
  const Lit b[3] = {shared, internalize(4), internalize(5)};
  const Lit replacement[3] = {shared, internalize(6), internalize(7)};
  const Ref source = kernel_add_large_clause(k, a, 3, false, 0, 301);
  kernel_add_large_clause(k, b, 3, false, 0, 302);
  kernel_inherit_clause_position(k, 303, source);
  kernel_delete_clause(k, clause_at(k.arena, source));
  kernel_add_large_clause(k, replacement, 3, false, 0, 303);

  kernel_rebuild_large_watches(k, /*inherited_source_order=*/true);

  std::vector<uint64_t> ids;
  for (const WatchEntry watch : k.watches.entries(shared))
    if (!watch.binary())
      ids.push_back(clause_at(k.arena, watch.ref())->id());
  assert((ids == std::vector<uint64_t>{303, 302}));
  kernel_check_invariants(k);
}

static void test_dense_occurrences_keep_source_order_across_collection() {
  Kernel k;
  kernel_reserve_vars(k, 7);
  const Lit shared = internalize(-1);
  const Lit a[3] = {shared, internalize(2), internalize(3)};
  const Lit b[3] = {shared, internalize(4), internalize(5)};
  const Lit replacement[3] = {shared, internalize(6), internalize(7)};
  const Ref source = kernel_add_large_clause(k, a, 3, false, 0, 301);
  kernel_add_large_clause(k, b, 3, false, 0, 302);
  kernel_inherit_clause_position(k, 303, source);
  kernel_delete_clause(k, clause_at(k.arena, source));
  kernel_add_large_clause(k, replacement, 3, false, 0, 303);

  OccIndex occ;
  kernel_dense_connect(k, occ);
  const auto dense_ids = [&] {
    std::vector<uint64_t> ids;
    for (const Ref ref : occ.large[shared])
      ids.push_back(clause_at(k.arena, ref)->id());
    return ids;
  };
  assert((dense_ids() == std::vector<uint64_t>{303, 302}));

  ReverseArenaOrder reverse;
  k.arena_ordering = &reverse;
  kernel_collect_garbage(k);
  assert((dense_ids() == std::vector<uint64_t>{303, 302}) &&
         "dense reconnect must not inherit the physical arena policy");
  kernel_dense_disconnect(k);
  kernel_check_invariants(k);
}

static void test_dense_occurrences_reuse_capacity_and_position_order() {
  Kernel k;
  kernel_reserve_vars(k, 13);
  const Lit shared = internalize(-1);
  const Lit a[3] = {shared, internalize(2), internalize(3)};
  const Lit b[3] = {shared, internalize(4), internalize(5)};
  const Lit c[3] = {shared, internalize(6), internalize(7)};
  const Lit d[3] = {shared, internalize(8), internalize(9)};
  const Lit e[3] = {shared, internalize(10), internalize(11)};
  const Lit f[3] = {shared, internalize(12), internalize(13)};
  const Ref ra = kernel_add_large_clause(k, a, 3, false, 0, 513);
  const Ref rb = kernel_add_large_clause(k, b, 3, false, 0, 514);
  const Ref rc = kernel_add_large_clause(k, c, 3, false, 0, 515);
  kernel_add_large_clause(k, d, 3, false, 0, 516);
  kernel_add_large_clause(k, e, 3, false, 0, 517);
  kernel_add_large_clause(k, f, 3, false, 0, 518);
  kernel_inherit_clause_position(k, 519, rc);
  kernel_inherit_clause_position(k, 520, ra);
  kernel_inherit_clause_position(k, 521, rb);
  kernel_delete_clause(k, clause_at(k.arena, ra));
  kernel_delete_clause(k, clause_at(k.arena, rb));
  kernel_delete_clause(k, clause_at(k.arena, rc));
  kernel_add_large_clause(k, a, 3, false, 0, 519);
  kernel_add_large_clause(k, b, 3, false, 0, 520);
  kernel_add_large_clause(k, c, 3, false, 0, 521);

  OccIndex occ;
  kernel_dense_connect(k, occ);
  std::vector<uint64_t> ids;
  for (const Ref ref : occ.large[shared])
    ids.push_back(clause_at(k.arena, ref)->id());
  const std::vector<uint64_t> expected{520, 521, 519, 516, 517, 518};
  assert(ids == expected);
  const Ref *const allocation = occ.large[shared].data();
  const size_t capacity = occ.large[shared].capacity();

  kernel_dense_disconnect(k);
  kernel_dense_connect(k, occ);
  assert(occ.large[shared].data() == allocation);
  assert(occ.large[shared].capacity() == capacity);
  ids.clear();
  for (const Ref ref : occ.large[shared])
    ids.push_back(clause_at(k.arena, ref)->id());
  assert(ids == expected);
  kernel_dense_disconnect(k);
  kernel_check_invariants(k);
}

static void test_dense_mixed_order_is_consumer_scoped() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit binary[2] = {internalize(1), internalize(2)};
  const Lit large[3] = {internalize(1), internalize(3), internalize(4)};
  kernel_add_binary_clause(k, binary[0], binary[1]);
  kernel_add_large_clause(k, large, 3, false, 0);

  OccIndex occ;
  kernel_dense_connect(k, occ);
  assert(!occ.track_order);
  assert(occ.ordered.empty());
  assert(occ.large[large[0]].size() == 1);
  kernel_dense_disconnect(k);

  kernel_dense_connect(k, occ, true);
  assert(occ.track_order);
  assert(occ.ordered.size() == 2 * k.nvars);
  assert(occ.ordered[binary[0]].size() == 2);
  assert(occ.ordered[binary[0]][0].binary);
  assert(!occ.ordered[binary[0]][1].binary);
  kernel_dense_disconnect(k);

  // Returning to a Probe-shaped connection frees the optional formula-wide
  // headers and payload instead of retaining a cold Eliminate image.
  kernel_dense_connect(k, occ);
  assert(!occ.track_order && occ.ordered.empty());
  assert(occ.large[large[0]].size() == 1);
  kernel_dense_disconnect(k);
  kernel_check_invariants(k);
}

static void test_large_replacement_preserves_redundant_recency() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit original[4] = {internalize(1), internalize(-2), internalize(3),
                           internalize(-4)};
  const Ref old_ref = kernel_add_large_clause(
      k, original, 4, true, 3, ++k.next_clause_id);
  Clause *old = clause_at(k.arena, old_ref);
  old->set_used(Clause::MAX_USED - 2);
  kernel_unwatch_large(k, old->lits[0], old_ref);
  kernel_unwatch_large(k, old->lits[1], old_ref);

  const Lit shortened[3] = {original[0], original[1], original[2]};
  Ref replacement = INVALID_REF;
  const ReplaceOutcome outcome = kernel_replace_clause(
      k, old, ++k.next_clause_id, shortened, 3, true, 2, &replacement);

  assert(outcome == ReplaceOutcome::Large);
  assert(replacement != INVALID_REF);
  const Clause *rewritten = clause_at(k.arena, replacement);
  assert(rewritten->used() == Clause::MAX_USED - 2);
  assert(!rewritten->vivify());
  assert(rewritten->glue() == 2);
  kernel_check_invariants(k);
}

static void test_in_place_large_replacement_preserves_allocation_span() {
  Kernel k;
  kernel_reserve_vars(k, 9);
  const Lit original[6] = {internalize(1), internalize(2), internalize(3),
                           internalize(4), internalize(5), internalize(6)};
  const Ref ref = kernel_add_large_clause(k, original, 6, false, 0,
                                          ++k.next_clause_id);
  const Lit tail[3] = {internalize(7), internalize(8), internalize(9)};
  const Ref tail_ref = kernel_add_large_clause(k, tail, 3, false, 0,
                                               ++k.next_clause_id);
  OccIndex occ;
  kernel_dense_connect(k, occ);

  const Lit first_image[4] = {original[0], original[2], original[4], tail[0]};
  kernel_replace_large_clause_in_place(k, ref, ++k.next_clause_id,
                                       first_image, 4, 0);
  const Clause *c = clause_at(k.arena, ref);
  assert(c->size == 4 && c->shrunken());
  assert(clause_actual_words(c) == clause_words(6));
  assert(ref + clause_actual_words(c) == tail_ref);
  assert(std::find(occ.large[original[1]].begin(),
                   occ.large[original[1]].end(), ref) ==
         occ.large[original[1]].end());
  assert(occ.large[tail[0]].back() == ref);

  const Lit second_image[3] = {first_image[0], first_image[2], first_image[3]};
  kernel_replace_large_clause_in_place(k, ref, ++k.next_clause_id,
                                       second_image, 3, 0);
  c = clause_at(k.arena, ref);
  assert(c->size == 3 && c->shrunken());
  assert(clause_actual_words(c) == clause_words(6));
  assert(ref + clause_actual_words(c) == tail_ref);
  kernel_check_invariants(k);

  kernel_collect_garbage(k);
  c = clause_at(k.arena, 0);
  assert(c->size == 3 && !c->shrunken());
  assert(clause_actual_words(c) == clause_words(3));
  assert(k.arena.size() == 2 * clause_words(3));
  kernel_dense_disconnect(k);
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
  const Lit root_satisfied = internalize(6);
  kernel_add_binary_clause(k, root_satisfied, higher);
  kernel_assign_unit(k, root_satisfied);
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
  assert(k.watches.heads[pivot].binaries_first());
  assert(kernel_binary_copies(k, pivot, higher) == 1);
  assert(kernel_binary_copies(k, root_satisfied, higher) == 0 &&
         "Congruence dense entry drops root-satisfied binaries like Kissat");
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
  // propbeyond.c counts every consumed trail literal: the two decisions and
  // the implication appended while their watches are scanned.
  assert(k.stats.warming_propagations == 3);
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
  assert(watches.binary_size(0) == 1);
  assert(watches.binary_size(1) == 1);
  assert(watches.binary_size(2) == 1);

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
  assert(watches.binary_size(0) == 1);
  assert(watches.binary_size(1) == 2);
  assert(watches.arena.size() == arena_size);
  assert(watches.heads[0].size == watches.heads[0].capacity);
  assert(watches.heads[1].size == watches.heads[1].capacity);
  assert(watches.holes == 0);

  watches.begin_batch_reserve();
  watches.finish_batch_reserve(false);
  const size_t exact_size = watches.arena.size();
  assert(watches.arena.capacity() == exact_size);
  watches.push_binary(0, 1);
  assert(watches.binary_size(0) == 2);
  assert(watches.arena.capacity() > exact_size);
}

static void test_kissat_watch_growth_and_clause_checkpoint_defrag() {
  WatchLists growth;
  growth.resize_lits(2);
  for (uint32_t i = 0; i < 3; i++)
    growth.push_binary(0, 1);
  // Kissat's four-word initial stack spends one word on its sentinel, leaving
  // exactly three tail words before the first relocation.
  assert(growth.arena.size() == 3);
  assert(growth.arena.capacity() == 3);
  assert(growth.holes == 0);
  growth.push_binary(0, 1);
  assert(growth.arena.size() == 9);
  assert(growth.heads[0].offset == 3);
  assert(growth.heads[0].size == 4);
  assert(growth.heads[0].capacity == 6);
  assert(growth.holes == 5); // abandoned prefix 3 + spare suffix 2
  for (uint32_t i = 0; i < growth.size(0); i++)
    assert(growth.arena[growth.heads[0].offset + i] ==
           binary_watch(1).raw);
  growth.push_binary(1, 0);
  assert(growth.heads[1].offset == 9);
  assert(growth.holes == 5);

  WatchLists watches;
  watches.resize_lits(2);
  // inlinevector.h extends a vector at the global stack tail one word at a
  // time without abandoning its prefix or creating usable space. Tactix omits
  // Kissat's stored offset-zero sentinel, but maps its capacity and predicates.
  watches.arena.reserve(300000);
  for (uint32_t i = 0; i < 300000; i++)
    watches.push_binary(0, i & 1u);
  assert(watches.arena.size() == 300000);
  assert(watches.holes == 0);

  // Dropping exactly 75% is not enough (`usable <= limit` in collect.h).
  uint32_t retained = 0;
  watches.compact(0, [&](WatchEntry) { return retained++ < 75000; });
  assert(watches.size(0) == 75000 && watches.holes == 225000);
  watches.defrag_if_needed();
  assert(watches.arena.size() == 300000);

  // More than 75% unusable above 2^18 words packs only at the explicit
  // post-clause checkpoint; propagation-time push/grow never calls it.
  retained = 0;
  watches.compact(0, [&](WatchEntry) { return retained++ < 74999; });
  assert(watches.holes == 225001);
  watches.defrag_if_needed();
  assert(watches.holes == 0);
  assert(watches.arena.size() == 74999);
  assert(watches.size(0) == 74999);
  for (uint32_t i = 0; i < watches.size(0); i++)
    assert(watches.arena[watches.heads[0].offset + i] ==
           binary_watch(i & 1u).raw);
}

int main() {
  test_irredundant_clause_never_stores_glue();
  test_active_population_is_live_state();
  test_compact_watch_head_encoding_is_full_width();
  test_clause_position_alias_pages_are_exact_and_sparse();
  test_binary_liveness_filter_has_no_false_negatives();
  test_reserved_trail_stack_contract();
  test_conflicting_propagation_consumes_trail_literal();
  test_variable_identity_maps();
  test_import_order_survives_compaction_linearly();
  test_variable_compaction();
  test_compaction_collapses_root_fixed_admission_and_rewatches();
  test_compaction_admission_uses_post_startup_population();
  test_variable_compaction_preserves_directed_binary_order();
  test_variable_compaction_replays_inherited_arena_order();
  test_whole_collection_replays_inherited_creation_order();
  test_whole_collection_preserves_prior_sparse_partition();
  test_sparse_collection_from_zero_partitions_whole_arena();
  test_sparse_collection_retains_pending_prefix_replacement();
  test_compaction_defers_retired_jumped_reason();
  test_compaction_drops_unassigned_stale_reason();
  test_lit_encoding();
  test_unit_chain();
  test_root_sparse_collection();
  test_root_sparse_collection_keeps_native_order();
  test_root_sparse_collection_ignores_nonroot_values();
  test_root_sparse_collection_rewrites_nonroot_reason();
  test_sparse_collection_flushes_fixed_prefix_watch();
  test_root_sparse_collection_sweeps_physical_irredundant_tail();
  test_sparse_collection_partitions_mixed_tail();
  test_sparse_collection_rebuilds_tail_irredundant_blocker();
  test_root_sparse_collection_partitions_rewritten_tail();
  test_root_sparse_collection_collects_existing_garbage();
  test_root_sparse_collection_retains_pending_prefix_replacement();
  test_repeated_clause_deletion_emits_one_proof_line();
  test_garbage_collection_drops_retired_otfs_reason();
  test_large_clause_propagation();
  test_root_conflict_is_unsat();
  test_backtrack_restores();
  test_probing_does_not_save_phases();
  test_probing_propagation_restores_or_persists_pose();
  test_delayed_relocation_scratch_is_output_sensitive();
  test_default_solve_sat();
  test_rebuild_large_watches_restores_arena_order();
  test_phase_rebuild_large_watches_restores_source_order();
  test_dense_occurrences_keep_source_order_across_collection();
  test_dense_occurrences_reuse_capacity_and_position_order();
  test_dense_mixed_order_is_consumer_scoped();
  test_large_replacement_preserves_redundant_recency();
  test_in_place_large_replacement_preserves_allocation_span();
  test_rebuild_large_watches_reselects_root_false_pair();
  test_watch_repair_primitive();
  test_canonical_binary_watch_rebuild();
  test_null_stack_dense_flush_retains_root_satisfied_pairs();
  test_propagate_beyond_conflicts_for_warmup();
  test_pending_units_borrow_and_restore_literal_marks();
  test_watch_batch_reserve_preserves_prefixes_and_fills_exactly();
  test_kissat_watch_growth_and_clause_checkpoint_defrag();
  puts("test_kernel OK");
  return 0;
}
