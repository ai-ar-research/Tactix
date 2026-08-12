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
  assert(decide->instances(k, instances, 4) == 1);
  Lit external;
  assert(lit_of_instance(instances[0], &external));
  assert(std::abs(externalize(external)) == 5);
  kernel_check_invariants(k);

  kernel_extend_model(k);
  assert(k.model_values[internalize(2)] > 0);
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
  kernel_add_large_clause(k, satisfied, 3, false, 0);
  kernel_add_large_clause(k, shortened, 4, false, 0);
  assert(propagate_apply(k) == ApplyResult::Ok);

  assert(kernel_sparse_collect_root(k) == RootCollectOutcome::Changed);
  assert(k.stats.sparse_collections == 1);
  assert(k.stats.sparse_collected == 2 && k.stats.flushed == 2);
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

static void test_probing_propagation_restores() {
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

  // probing engine: same derivation, bookkeeping handed back untouched
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

  // not a one-shot: a second episode repeats, so the stamp epoch rolls over
  kernel_probing_begin(b);
  kernel_assign_decision(b, internalize(-1));
  assert(b.h->saved[0] == 0);
  assert(!kernel_propagate_probing(b));
  assert(b.trail == derived);
  kernel_backtrack(b, 0, /*update_phases=*/false);
  kernel_probing_end(b);
  assert(same(bookkeeping(b), b0));

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
  k.warming = true;
  kernel_propagate_beyond_conflicts(k);
  k.warming = false;

  assert(k.values[internalize(3)] > 0);
  assert(k.h->saved[2] == 1);
  assert(k.stats.warming_conflicts > 0);
  assert(k.stats.warming_propagations == 1);
  assert(k.stats.decisions == decisions);
  assert(!k.conflict_valid && !k.inconsistent);
  kernel_backtrack(k, 0);
  kernel_check_invariants(k);
}

int main() {
  test_variable_identity_maps();
  test_variable_compaction();
  test_compaction_defers_retired_jumped_reason();
  test_compaction_drops_unassigned_stale_reason();
  test_lit_encoding();
  test_unit_chain();
  test_root_sparse_collection();
  test_root_sparse_collection_keeps_native_order();
  test_large_clause_propagation();
  test_root_conflict_is_unsat();
  test_backtrack_restores();
  test_probing_does_not_save_phases();
  test_probing_propagation_restores();
  test_default_solve_sat();
  test_rebuild_large_watches_restores_arena_order();
  test_rebuild_large_watches_reselects_root_false_pair();
  test_watch_repair_primitive();
  test_canonical_binary_watch_rebuild();
  test_propagate_beyond_conflicts_for_warmup();
  puts("test_kernel OK");
  return 0;
}
