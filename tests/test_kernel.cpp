// Kernel unit tests: literal encoding, watch scheme, propagation chains,
// root-level conflict detection.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "policies/default_policy.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>
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
  kernel_backtrack(k, 0);
  assert(k.level == 0 && k.unassigned == 3);
  assert(k.values[internalize(1)] == 0);
  assert(k.h->saved[0] == 1);  // phase of var 1 saved positive
  assert(k.h->saved[1] == -1); // phase of var 2 saved negative
}

// A probing episode derives exactly what kernel_propagate derives and hands
// back watch lists, literal order and `searched` byte-identical.
struct BookKeeping {
  std::vector<std::vector<uint32_t>> lists;
  std::vector<uint32_t> clauses;
};

static BookKeeping bookkeeping(const Kernel &k) {
  BookKeeping s;
  s.lists.resize(2 * k.nvars);
  for (Lit l = 0; l < 2 * k.nvars; l++) {
    const WatchHead &h = k.watches.heads[l];
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
  return a.lists == b.lists && a.clauses == b.clauses;
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
  assert(!kernel_propagate_probing(b));
  assert(b.trail == derived); // same implied literals, same trail order
  kernel_backtrack(b, 0, /*update_phases=*/false);
  kernel_probing_end(b);
  assert(same(bookkeeping(b), b0));

  // not a one-shot: a second episode repeats, so the stamp epoch rolls over
  kernel_probing_begin(b);
  kernel_assign_decision(b, internalize(-1));
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

int main() {
  test_variable_identity_maps();
  test_lit_encoding();
  test_unit_chain();
  test_large_clause_propagation();
  test_root_conflict_is_unsat();
  test_backtrack_restores();
  test_probing_propagation_restores();
  test_default_solve_sat();
  test_watch_repair_primitive();
  puts("test_kernel OK");
  return 0;
}
