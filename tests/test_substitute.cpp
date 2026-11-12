// V3 Substitute: SCC detection over the binary implication graph, the round,
// the instance surface, and model reconstruction through ρ. A binary (¬p ∨ q)
// is the implication p → q. See the rule calculus §V3.
#undef NDEBUG
#include "rule_test_utils.hpp"
#include "kernel/kernel.hpp"
#include "occurrence_test_utils.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/substitute/substitute.hpp"
#include "temporary_file.hpp"
#include "test_io.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>


using namespace tactix;

// x ≡ y as the implication binaries (¬x ∨ y), (¬y ∨ x).
static void equivalence(Kernel &k, int x, int y) {
  kernel_add_binary_clause(k, internalize(-x), internalize(y));
  kernel_add_binary_clause(k, internalize(-y), internalize(x));
}

// The bridge binaries (¬a ∨ b), (a ∨ ¬b) Sweep publishes for a proved a ≡ b.
static void bridge(Kernel &k, Lit a, Lit b) {
  kernel_add_binary_clause(k, negate(a), b);
  kernel_add_binary_clause(k, a, negate(b));
}

// The live clause whose literal set is exactly `external`, if any.
static const Clause *live_clause(const Kernel &k,
                                 std::initializer_list<int> external) {
  std::vector<Lit> want = lits_of(external);
  std::sort(want.begin(), want.end());
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    ref += clause_actual_words(c);
    std::vector<Lit> got(c->begin(), c->end());
    std::sort(got.begin(), got.end());
    if (!c->garbage() && got == want)
      return c;
  }
  return nullptr;
}

// The number of live arena clauses satisfying `predicate`.
template <class Predicate>
static uint32_t count_live(const Kernel &k, Predicate predicate) {
  uint32_t count = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    ref += clause_actual_words(c);
    count += !c->garbage() && predicate(*c);
  }
  return count;
}

static bool contains(const Clause &c, int external) {
  return std::find(c.begin(), c.end(), internalize(external)) != c.end();
}

// The first literal of every arena clause, none of which is garbage.
static std::vector<Lit> first_literals(const Kernel &k) {
  std::vector<Lit> result;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    assert(!c->garbage());
    result.push_back(c->lits[0]);
    ref += clause_actual_words(c);
  }
  return result;
}

// A connected dense index with a begun Sweep substitution session.
struct DenseSession {
  OccIndex occ;
  SweepSubstitutionSession session;
  explicit DenseSession(Kernel &k, bool track_order = false) {
    kernel_dense_connect(k, occ, track_order);
    session.begin(k);
  }
};

// x1 ≡ x2 via x1 -> x2 -> x1: SCC {+1,+2} plus its complement {-1,-2}
static void test_equivalence_scc() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  equivalence(k, 1, 2);
  SccResult r = substitute_find_sccs(k);
  assert(!r.contradiction);
  assert(r.nontrivial_reps.size() == 2); // {+1,+2} and {-1,-2}
  assert(r.repr[internalize(2)] == r.repr[internalize(1)]);
  assert(r.repr[internalize(1)] == internalize(1));           // rep = min = +1
  assert(r.repr[internalize(-2)] == r.repr[internalize(-1)]);

  const SccResult compact = substitute_find_sccs(k, false);
  assert(compact.nontrivial_count == 2);
  assert(compact.nontrivial_reps.empty());
  assert(compact.repr == r.repr);
}

// Kissat's DFS charges each visited literal's watch cache lines on entry and
// on the finishing rescan, so round-one `substitute_ticks` equal Kissat's.
static void test_scc_charges_two_scans_per_literal() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // x1 -> x2
  kernel_add_binary_clause(k, internalize(-2), internalize(3)); // x2 -> x3
  kernel_add_binary_clause(k, internalize(-3), internalize(1)); // x3 -> x1
  assert(k.stats.substitute_ticks == 0);
  uint64_t one_scan = 0;
  for (Var v = 0; v < k.nvars; v++)
    for (uint32_t sign = 0; sign < 2; sign++) {
      const Lit l = make_lit(v, sign != 0);
      one_scan += 1 + kernel_cache_lines(k.watches.size(negate(l)));
    }
  const SccResult r = substitute_find_sccs(k);
  assert(r.nontrivial_reps.size() == 2);
  assert(k.stats.substitute_ticks == 2 * one_scan &&
         "every visited literal is charged on entry and on finish");
}

static void test_instance_enumeration_does_not_charge_work() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(3));
  kernel_add_binary_clause(k, internalize(-3), internalize(1));
  const RuleVTable &rule = tactix_rules()[tactix_rule_id("Substitute")];
  uint64_t instances[8];
  assert(rule.instances(k, instances, 8) > 0);
  assert(k.stats.substitute_ticks == 0 &&
         "observing Substitute instances must not consume rule work");
}

// SCC representatives follow Kissat's first-occurrence (import) order.
static void test_representative_uses_import_order() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_record_import(k, 1); // external x2 appears before x1
  kernel_record_import(k, 2);
  kernel_record_import(k, 0);
  equivalence(k, 1, 2);
  equivalence(k, 3, 2);

  const SccResult r = substitute_find_sccs(k);
  assert(r.repr[internalize(1)] == internalize(2));
  assert(r.repr[internalize(-1)] == internalize(-2));
  assert(r.repr[internalize(3)] == internalize(2));
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(kernel_var_retired(k, 0));
  assert(!kernel_var_retired(k, 1));
  assert(kernel_var_retired(k, 2));
  // Kissat's `remove_representative_equivalences` retires in import order.
  assert(k.retired_events == std::vector<Var>({2, 0}));
}

// a one-way implication x1 -> x2 (no return edge) forms no non-trivial SCC
static void test_no_scc() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // x1 -> x2 only
  SccResult r = substitute_find_sccs(k);
  assert(!r.contradiction);
  assert(r.nontrivial_reps.empty());
  assert(r.repr[internalize(1)] == internalize(1)); // singleton = own rep
  assert(r.repr[internalize(3)] == internalize(3));
}

// Kissat's large-clause pass runs even without an equivalence class, so it
// commits root simplification (a ternary with a root-false literal shrinks).
static void test_no_scc_still_simplifies_root_fixed_clauses() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {-1, 2, 3});
  kernel_assign_unit(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);

  assert(substitute_find_sccs(k).nontrivial_reps.empty());
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(kernel_binary_live(k, internalize(2), internalize(3)));
  assert(substitute_apply(k) == ApplyResult::NoOp);
}

// Kissat's SCC graph excludes inactive variables, even ones stale binaries
// still mention.
static void test_retired_variable_excluded_from_scc() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  equivalence(k, 1, 2);
  k.var_status.assign(k.nvars, VarStatus::Active);
  k.var_status[1] = VarStatus::Substituted;
  k.unassigned_active--;

  const SccResult r = substitute_find_sccs(k);
  assert(r.nontrivial_reps.empty());
  assert(r.repr[internalize(2)] == INVALID_LIT);
  assert(substitute_apply(k) == ApplyResult::NoOp);
  assert(k.retired_events.empty() && k.substituted_events.empty());
}

// x ≡ ¬x (x -> a -> ¬x -> a -> x) is one contradictory SCC; the canonical
// round and one N1 instance both refute it.
static void x_equivalent_not_x(Kernel &k) {
  kernel_reserve_vars(k, 2); // x = var 1, a = var 2
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(-1));
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
}

static void test_contradiction_scc() {
  Kernel k;
  x_equivalent_not_x(k);
  SccResult r = substitute_find_sccs(k);
  assert(r.contradiction);
  assert(r.contradiction_lit != INVALID_LIT);
  assert(r.repr[r.contradiction_lit] == r.repr[negate(r.contradiction_lit)]);

  Kernel round;
  x_equivalent_not_x(round);
  assert(substitute_guard(round));
  assert(substitute_apply(round) == ApplyResult::Unsat);
  assert(round.inconsistent);
  assert(round.stats.substitute_rounds == 1);

  Kernel instance;
  x_equivalent_not_x(instance);
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("Substitute")];
  uint64_t insts[8];
  assert(vt.instances(instance, insts, 8) >= 1);
  assert(vt.apply_instance(instance, insts[0]) == ApplyResult::Unsat);
  assert(instance.inconsistent);
}

static bool lit_in_any_clause(const Kernel &k, Lit l) {
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[l].offset;
  for (uint32_t i = 0; i < k.watches.heads[l].size;) {
    if (Watch{ws[i]}.binary())
      return true;
    i += 2; // large watch: blocking word + ref word
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage())
      for (const Lit m : *c)
        if (m == l)
          return true;
    ref += clause_actual_words(c);
  }
  return false;
}

// A consistent x1 ≡ x2 is substituted: the non-rep x2 vanishes everywhere.
static void test_apply_substitutes() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  equivalence(k, 1, 2);
  add_external_clause(k, {2, 3, 4});

  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(!k.inconsistent);
  assert(!k.substituted_events.empty());
  assert(k.retired_events == k.substituted_events);
  assert(!lit_in_any_clause(k, internalize(2))); // x2 gone, both polarities
  assert(!lit_in_any_clause(k, internalize(-2)));
  assert(live_clause(k, {1, 3, 4}) && "large clause rewritten to (x1 v x3 v x4)");
  SccResult r2 = substitute_find_sccs(k); // re-detection: the SCC is gone
  assert(r2.nontrivial_reps.empty());
  // a witness pair was pushed for the eliminated variable
  assert(!k.reconstruction.empty());
}

// Canonical Kissat substitution shrinks a large clause in place keeping its
// glue (connected Sweep substitution applies the size-1 promotion).
static void test_canonical_substitution_preserves_glue_on_shrink() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  equivalence(k, 1, 2);
  add_external_clause(k, {1, 2, 3, 4, 5, 6}, true, 5);

  assert(substitute_apply(k) == ApplyResult::Ok);
  const Clause *rewritten = live_clause(k, {1, 3, 4, 5, 6});
  assert(rewritten && rewritten->redundant() && rewritten->glue() == 5);
}

// As in Kissat, a large source collapsed to a binary stays lazy garbage until
// an arena owner (Reduce or Eliminate) collects it.
static void test_canonical_substitution_leaves_collapsed_source_lazy() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  equivalence(k, 1, 2);
  add_external_clause(k, {1, 2, 3});
  const uint64_t arena_generation = k.arena_generation;

  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(k.arena.size() == clause_words(3));
  assert(k.arena_generation == arena_generation);
  assert(clause_at(k.arena, 0)->garbage());
  assert(kernel_binary_live(k, internalize(1), internalize(3)));
  kernel_check_invariants(k);
}

// Kissat's `complete=true` preprocessing call runs a second SCC round for an
// equivalence the first rewrite exposes.
static void test_complete_closes_exposed_scc() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  // x1 ≡ x2 is visible initially.
  equivalence(k, 1, 2);
  // Rewriting x2 -> x1 turns these into the two implications x1 ≡ x3.
  add_external_clause(k, {1, -3, 2});
  add_external_clause(k, {-1, 3, -2});

  assert(substitute_apply_complete(k, 2) == ApplyResult::Ok);
  assert(k.stats.substitute_rounds == 2);
  assert(kernel_var_retired(k, 1));
  assert(kernel_var_retired(k, 2));
  assert(!lit_in_any_clause(k, internalize(2)));
  assert(!lit_in_any_clause(k, internalize(3)));
}

// Kissat's Sweep-connected substitution rewrites a large clause in place and
// updates the mixed dense stream at once (later capped environments see it).
static void test_sweep_equivalence_preserves_large_clause_position() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  add_external_clause(k, {3, 4, 5});
  const Ref touched_ref = add_external_clause(k, {2, 4, 6});
  clause_at(k.arena, touched_ref)->set_vivify(true);
  add_external_clause(k, {-3, 4, 6});
  equivalence(k, 1, 2);
  DenseSession dense(k);
  assert(substitute_apply_sweep_equivalences(
             k, {{internalize(1), internalize(2)}}, dense.session) ==
         ApplyResult::Ok);
  assert(dense.session.changed);
  assert(live_clause(k, {1, 4, 6}) && live_clause(k, {1, 4, 6})->vivify());

  // Before the session publishes, live x4 occurrences keep their order.
  std::vector<Lit> live_dense_first;
  for (const Ref ref : large_occurrences(dense.occ, internalize(4))) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage())
      live_dense_first.push_back(c->lits[0]);
  }
  assert(live_dense_first ==
         std::vector<Lit>({internalize(3), internalize(1), internalize(-3)}));
  // In-place replacement owes no arena rebuild at the session boundary.
  (void)dense.session.collect(k);
  assert(live_clause(k, {1, 4, 6}) && live_clause(k, {1, 4, 6})->vivify());
  assert(first_literals(k) ==
         std::vector<Lit>({internalize(3), internalize(1), internalize(-3)}));
}

// Kissat's canonical substitute.c rewrite does not re-arm Factor; Sweep's
// connected rewrite marks every surviving irredundant literal.
static void test_substitution_factor_marks_match_calling_path() {
  {
    Kernel k;
    kernel_reserve_vars(k, 4);
    equivalence(k, 1, 2);
    add_external_clause(k, {2, 3, 4});
    std::fill(k.factor_marks.begin(), k.factor_marks.end(), 0);
    k.factor_literals_marked = 0;

    assert(substitute_apply(k) == ApplyResult::Ok);
    assert(!k.factor_marks[internalize(3)]);
    assert(!k.factor_marks[internalize(4)]);
  }

  {
    Kernel k;
    kernel_reserve_vars(k, 4);
    add_external_clause(k, {2, 3, 4});
    bridge(k, internalize(1), internalize(2));
    DenseSession dense(k);
    std::fill(k.factor_marks.begin(), k.factor_marks.end(), 0);
    k.factor_literals_marked = 0;

    assert(substitute_apply_sweep_equivalences(
               k, {{internalize(1), internalize(2)}}, dense.session) ==
           ApplyResult::Ok);
    assert(k.factor_marks[internalize(3)]);
    assert(k.factor_marks[internalize(4)]);
  }
}

// In Kissat's dense Sweep list a large clause collapsing to a binary replaces
// the stable endpoint's occurrence in place and appends the moved direction.
static void test_sweep_preserves_cross_kind_dense_order() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  add_external_clause(k, {1, 3, 5});
  add_external_clause(k, {2, 1, 4});
  add_external_clause(k, {1, -3, -5});
  equivalence(k, 1, 2);
  DenseSession dense(k, true);
  assert(substitute_apply_sweep_equivalences(
             k, {{internalize(1), internalize(2)}}, dense.session) ==
         ApplyResult::Ok);

  const auto &with_bridge = dense.session.occurrences(k, internalize(1));
  std::vector<SweepSubstitutionSession::DenseOccurrence> ordered;
  for (const auto occurrence : with_bridge)
    if (!(occurrence.binary && occurrence.payload == internalize(-2)))
      ordered.push_back(occurrence);
  assert(ordered.size() == 3);
  assert(!ordered[0].binary && !ordered[1].binary);
  assert(ordered[2].binary && ordered[2].payload == internalize(4));
  (void)dense.session.collect(k);
  const auto &published = dense.occ.ordered[internalize(1)];
  assert(published.size() == with_bridge.size());
  for (size_t i = 0; i < published.size(); ++i) {
    assert(static_cast<bool>(published[i].binary) == with_bridge[i].binary);
    assert(published[i].payload == with_bridge[i].payload);
  }
  kernel_check_invariants(k);
}

// Binary sync matches the old multiset, appends only the new binary, and
// publishes the ledger's Kissat order to the physical watches.
static void test_sweep_syncs_new_binaries_by_multiset() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit one = internalize(1);
  kernel_add_binary_clause(k, one, internalize(2));
  kernel_add_binary_clause(k, one, internalize(3));
  DenseSession dense(k, true);
  auto &one_stream = dense.session.occurrences(k, one);
  std::swap(one_stream[0], one_stream[1]);

  kernel_add_binary_clause(k, one, internalize(4));
  dense.session.sync_new_binaries(k, {{one, internalize(4)}});
  const auto &ordered = dense.session.occurrences(k, one);
  assert(ordered.size() == 3);
  assert(ordered[0].payload == internalize(3));
  assert(ordered[1].payload == internalize(2));
  assert(ordered[2].payload == internalize(4));
  const std::vector<Lit> physical = binary_partners(k, one);
  assert((physical == std::vector<Lit>{internalize(3), internalize(2),
                                     internalize(4)}));
  assert(dense.occ.ordered[one].size() == ordered.size());
  for (size_t i = 0; i < ordered.size(); ++i) {
    assert(dense.occ.ordered[one][i].binary == ordered[i].binary);
    assert(dense.occ.ordered[one][i].payload == ordered[i].payload);
  }

  // Reconciliation removes a stale spectator mirror named by both directed
  // owners.
  const Lit phantom_a = internalize(3), phantom_b = internalize(-4);
  dense.session.occurrences(k, phantom_a).push_back({true, phantom_b});
  dense.session.occurrences(k, phantom_b).push_back({true, phantom_a});
  dense.session.dirty_binary_owners.insert(dense.session.dirty_binary_owners.end(),
                                     {phantom_a, phantom_b});
  dense.session.sync_new_binaries(k, {{one, internalize(2)}});
  for (const auto occurrence : dense.session.occurrences(k, phantom_a))
    assert(!occurrence.binary || occurrence.payload != phantom_b);
  for (const auto occurrence : dense.session.occurrences(k, phantom_b))
    assert(!occurrence.binary || occurrence.payload != phantom_a);
  kernel_check_invariants(k);
}

static void test_sweep_reposes_only_last_bridge_copy() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit a = internalize(1), b = internalize(2);
  const Lit c = internalize(3), d = internalize(4);
  kernel_add_binary_clause(k, negate(a), b);
  kernel_add_binary_clause(k, negate(a), c);
  kernel_add_binary_clause(k, negate(a), b);
  kernel_add_binary_clause(k, negate(a), d);
  kernel_add_binary_clause(k, a, negate(b));
  DenseSession dense(k, true);
  dense.session.sync_new_binaries(k, {{a, b}});
  const auto id = kernel_binary_id(k, negate(a), b);
  const auto count = k.watches.size(negate(a));
  dense.session.repose_emitted_equivalence(k, a, b);
  dense.session.sync_new_binaries(k, {{a, b}});
  const std::vector<Lit> physical = binary_partners(k, negate(a));
  assert((physical == std::vector<Lit>{b, c, d, b}));
  assert(k.watches.size(negate(a)) == count);
  assert(kernel_binary_id(k, negate(a), b) == id);
  dense.session.repose_emitted_equivalence(k, a, internalize(5));
  dense.session.sync_new_binaries(k, {{a, internalize(5)}});
  assert(!kernel_binary_live(k, negate(a), internalize(5)));
  assert(k.watches.size(negate(a)) == count);
  kernel_check_invariants(k);
}

static void assert_ledger_mirrors_kernel(Kernel &,
                                         SweepSubstitutionSession &);

// An identity transaction reports no dirty binary owners.
static void test_sweep_identity_rewrite_has_no_dirty_binary_owners() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  kernel_add_binary_clause(k, internalize(1), internalize(3));
  kernel_add_binary_clause(k, internalize(-2), internalize(4));
  kernel_add_binary_clause(k, internalize(-4), internalize(5));
  DenseSession dense(k);
  assert(dense.session.dense_occurrences.empty());

  dense.session.rewrite_binary_occurrences(k, {}, {}, {});
  assert(dense.session.dense_occurrences.empty());
  assert(dense.session.dirty_binary_owners.empty());
  dense.session.flush_moves(k);
  assert_ledger_mirrors_kernel(k, dense.session);
  kernel_dense_disconnect(k);
}

// The session contract (substitute.hpp): per literal, the ledger's binaries
// are exactly the physical binary watch multiset and its large entries exactly
// the live irredundant records of `Kernel::dense`, checked both ways.
static void assert_ledger_mirrors_kernel(Kernel &k,
  SweepSubstitutionSession &s) {
  assert(k.dense);
  assert(s.dense_occurrences.size() <= 2 * k.nvars);
  for (Lit lit = 0; lit < 2 * k.nvars; lit++) {
    std::vector<Lit> ledger_binaries, watch_binaries;
    std::vector<Ref> ledger_large, dense_large;
    for (const auto occurrence : s.occurrences(k, lit)) {
      if (occurrence.binary)
        ledger_binaries.push_back(static_cast<Lit>(occurrence.payload));
      else if (!clause_at(k.arena, occurrence.payload)->garbage())
        ledger_large.push_back(occurrence.payload);
    }
    for (const WatchEntry watch : k.watches.entries(lit))
      if (watch.binary())
        watch_binaries.push_back(watch.other());
    for (const Ref ref : large_occurrences(*k.dense, lit)) {
      const Clause *c = clause_at(k.arena, ref);
      if (!c->garbage() && !c->redundant())
        dense_large.push_back(ref);
    }
    const auto sorted = [](auto v) {
      std::sort(v.begin(), v.end());
      return v;
    };
    assert(sorted(ledger_binaries) == sorted(watch_binaries));
    assert(sorted(ledger_large) == sorted(dense_large));
  }
}

// Kissat retires one literal at a time through its unified watch stream, so
// a source's binaries and large clauses reach the representative interleaved
// in stream order; a previous substitution builds the discriminating stream.
static void test_sweep_flushes_one_source_stream_at_a_time() {
  Kernel k;
  kernel_reserve_vars(k, 8);
  const Lit one = internalize(1), two = internalize(2), three = internalize(3);
  add_external_clause(k, {1, 7, 8}); // x1's own: keeps its slot
  add_external_clause(k, {2, 6, 7}); // x2's: survives, rewritten to x1
  kernel_add_binary_clause(k, three, internalize(5));
  DenseSession dense(k);
  // x2 = x3: x3's binary is appended behind x2's large clause.
  bridge(k, two, three);
  assert(substitute_apply_sweep_equivalences(k, {{two, three}}, dense.session) ==
         ApplyResult::Ok);
  assert_ledger_mirrors_kernel(k, dense.session);
  {
    const auto &with_bridge = dense.session.occurrences(k, two);
    std::vector<SweepSubstitutionSession::DenseOccurrence> source;
    for (const auto occurrence : with_bridge)
      if (!(occurrence.binary && var_of(occurrence.payload) == var_of(three)))
        source.push_back(occurrence);
    assert(source.size() == 2);
    assert(!source[0].binary); // the large clause was met first
    assert(source[1].binary && source[1].payload == internalize(5));
  }

  // x1 = x2 retires that stream: the large clause precedes the binary.
  bridge(k, one, two);
  assert(substitute_apply_sweep_equivalences(k, {{one, two}}, dense.session) ==
         ApplyResult::Ok);
  assert_ledger_mirrors_kernel(k, dense.session);

  const auto &with_bridges = dense.session.occurrences(k, one);
  std::vector<SweepSubstitutionSession::DenseOccurrence> ordered;
  for (const auto occurrence : with_bridges)
    if (!(occurrence.binary && var_of(occurrence.payload) <= var_of(three)))
      ordered.push_back(occurrence);
  assert(ordered.size() == 3);
  assert(!ordered[0].binary); // x1's own resident clause keeps its slot
  const Clause *const kept = clause_at(k.arena, ordered[0].payload);
  assert(!kept->garbage() && contains(*kept, 8));
  assert(!ordered[1].binary); // then x2's large clause, rewritten to x1
  const Clause *const inherited = clause_at(k.arena, ordered[1].payload);
  assert(!inherited->garbage() && inherited->size == 3);
  assert(contains(*inherited, 6) && contains(*inherited, 1));
  assert(ordered[2].binary && ordered[2].payload == internalize(5));
  (void)dense.session.collect(k);
  kernel_dense_disconnect(k);
}

// Kissat's `substitute_connected_clauses` exempts a fresh bridge only from its
// own pair, so an early pair's bridge carries the intermediate winner; without
// it Substitute cannot rediscover a winner that a later merge absorbs.
static void test_sweep_bridges_link_intermediate_representative() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit one = internalize(1), two = internalize(2), three = internalize(3),
            four = internalize(4);
  kernel_add_binary_clause(k, one, internalize(5));
  kernel_add_binary_clause(k, two, internalize(5));
  kernel_add_binary_clause(k, three, internalize(6));
  kernel_add_binary_clause(k, four, internalize(6));
  DenseSession dense(k);
  // x3 absorbs x4, x2 absorbs {x3, x4}, x1 absorbs the class via (x4, x1).
  bridge(k, three, four);
  bridge(k, three, two);
  bridge(k, four, one);
  assert(substitute_apply_sweep_equivalences(
             k, {{three, four}, {three, two}, {four, one}}, dense.session) ==
         ApplyResult::Ok);
  assert_ledger_mirrors_kernel(k, dense.session);
  assert(k.retired_events.empty());
  // x4 = x1 and x3 = x2 keep their binaries; the first pair's carry x2 = x1.
  assert(kernel_binary_live(k, negate(four), one));
  assert(kernel_binary_live(k, four, negate(one)));
  assert(kernel_binary_live(k, negate(three), two));
  assert(kernel_binary_live(k, three, negate(two)));
  assert(kernel_binary_live(k, negate(two), one));
  assert(kernel_binary_live(k, two, negate(one)));
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(!kernel_var_retired(k, var_of(one)));
  assert(kernel_var_retired(k, var_of(two)));
  assert(kernel_var_retired(k, var_of(three)));
  assert(kernel_var_retired(k, var_of(four)));
  (void)dense.session.collect(k);
  kernel_dense_disconnect(k);
}

// Kissat empties two sources of one representative in `sweep_equivalence`
// proof order, one complete stream at a time.
static void test_sweep_flushes_sources_in_emission_order() {
  Kernel k;
  kernel_reserve_vars(k, 7);
  const Lit one = internalize(1), two = internalize(2), three = internalize(3);
  kernel_add_binary_clause(k, two, internalize(6));
  kernel_add_binary_clause(k, three, internalize(7));
  DenseSession dense(k);
  // x1 wins both merges; the batch proves x1 = x3 first.
  bridge(k, one, three);
  bridge(k, one, two);
  assert(substitute_apply_sweep_equivalences(k, {{one, three}, {one, two}},
                                             dense.session) == ApplyResult::Ok);
  assert_ledger_mirrors_kernel(k, dense.session);
  // Clause streams follow pair-emission order. The class remains active and
  // the equivalence bridge remains live until canonical Substitute performs
  // Kissat's first-import-order deactivation scan.
  assert(k.retired_events.empty());

  const auto &with_bridges = dense.session.occurrences(k, one);
  std::vector<SweepSubstitutionSession::DenseOccurrence> ordered;
  for (const auto occurrence : with_bridges)
    if (!(occurrence.binary && var_of(occurrence.payload) <= var_of(three)))
      ordered.push_back(occurrence);
  assert(ordered.size() == 2);
  assert(ordered[0].binary && ordered[0].payload == internalize(7)); // from x3
  assert(ordered[1].binary && ordered[1].payload == internalize(6)); // from x2
  std::vector<Lit> physical;
  for (const WatchEntry watch : k.watches.entries(one))
    if (watch.binary() && var_of(watch.other()) > var_of(three))
      physical.push_back(watch.other());
  assert(physical == std::vector<Lit>({internalize(7), internalize(6)}));
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(k.retired_events ==
         std::vector<Var>({var_of(two), var_of(three)}));
  kernel_dense_disconnect(k);
}

// A same-batch chain collapses a clause when its last equivalent literal
// retires; Kissat appends the binary at that literal's stream position.
static void test_sweep_chained_collapse_uses_last_mixed_stream() {
  Kernel k;
  kernel_reserve_vars(k, 10);
  const Lit one = internalize(1), two = internalize(2),
            three = internalize(3), four = internalize(4);
  add_external_clause(k, {1, 9, 10});
  add_external_clause(k, {3, 7, 8});
  add_external_clause(k, {4, 6, 8});
  add_external_clause(k, {3, 4, 5}); // collapses when x4 retires
  DenseSession dense(k);
  const std::vector<std::pair<Lit, Lit>> equivalences = {
      {three, four}, {two, three}, {one, two}};
  for (const auto &[a, b] : equivalences)
    bridge(k, a, b);
  assert(substitute_apply_sweep_equivalences(k, equivalences, dense.session) ==
         ApplyResult::Ok);
  assert_ledger_mirrors_kernel(k, dense.session);

  const auto &ordered = dense.session.occurrences(k, one);
  size_t collapse_at = ordered.size(), four_large_at = ordered.size();
  for (size_t i = 0; i < ordered.size(); i++) {
    const auto occurrence = ordered[i];
    if (occurrence.binary && occurrence.payload == internalize(5))
      collapse_at = i;
    if (!occurrence.binary &&
        contains(*clause_at(k.arena, occurrence.payload), 6))
      four_large_at = i;
  }
  assert(four_large_at < ordered.size());
  assert(collapse_at < ordered.size());
  assert(four_large_at < collapse_at);
  kernel_dense_disconnect(k);
}

// The ledger records Sweep's published bridges even when every class is
// declined (a unit of the same batch fixes an endpoint).
static void test_sweep_records_binaries_of_a_declined_batch() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit a = internalize(1), b = internalize(2);
  add_external_clause(k, {2, 3, 4});
  DenseSession dense(k);

  // Sweep's own emission, then the endpoint is fixed by the same batch's unit.
  bridge(k, a, b);
  kernel_assign_unit(k, a);
  assert(propagate_apply(k) == ApplyResult::Ok);

  assert(substitute_apply_sweep_equivalences(k, {{a, b}}, dense.session) ==
         ApplyResult::NoOp);
  assert_ledger_mirrors_kernel(k, dense.session);
  kernel_dense_disconnect(k);
}

// The ledger declines exactly the class members the rewrite declines.
static void test_sweep_ledger_declines_the_same_members_as_the_rewrite() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit live_a = internalize(1), live_b = internalize(2);
  const Lit fixed_a = internalize(3), fixed_b = internalize(4);
  add_external_clause(k, {2, 4, 5});
  kernel_add_binary_clause(k, live_b, internalize(6));
  kernel_add_binary_clause(k, fixed_b, internalize(6));
  DenseSession dense(k);

  bridge(k, live_a, live_b);
  bridge(k, fixed_a, fixed_b);
  kernel_assign_unit(k, fixed_a); // the second pair is no longer substitutable
  assert(propagate_apply(k) == ApplyResult::Ok);

  assert(substitute_apply_sweep_equivalences(
             k, {{live_a, live_b}, {fixed_a, fixed_b}}, dense.session) ==
         ApplyResult::Ok);
  assert(!kernel_var_retired(k, var_of(live_b)));
  assert(!kernel_var_retired(k, var_of(fixed_b)));
  assert_ledger_mirrors_kernel(k, dense.session);
  (void)dense.session.collect(k);
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(kernel_var_retired(k, var_of(live_b)));
  kernel_dense_disconnect(k);
}

// Kissat's dense Sweep stream excludes redundant large clauses; Substitute
// owns their rewrite.
static void test_sweep_leaves_redundant_clause_outside_dense_transaction() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  add_external_clause(k, {2, 3, 4}, true, 2);
  equivalence(k, 1, 2);
  kernel_assign_unit(k, internalize(-3)); // the ternary collapses to a binary
  assert(propagate_apply(k) == ApplyResult::Ok);
  DenseSession dense(k);
  k.stats.conflicts = 1;

  assert(substitute_apply_sweep_equivalences(
             k, {{internalize(1), internalize(2)}}, dense.session) ==
         ApplyResult::NoOp);
  assert(!kernel_binary_live(k, internalize(1), internalize(4)));
  assert(count_live(k, [](const Clause &c) { return c.redundant(); }) == 1);
  assert_ledger_mirrors_kernel(k, dense.session);
  assert(!kernel_var_retired(k, var_of(internalize(2))));
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(kernel_var_retired(k, var_of(internalize(2))));
  assert(kernel_binary_live(k, internalize(1), internalize(4)));
  kernel_dense_disconnect(k);
}

// Only the connected irredundant clause is rewritten and indexed.
static void test_sweep_rewrites_only_connected_irredundant_clause() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  add_external_clause(k, {2, 3, 4, 5}, true, 3);
  add_external_clause(k, {2, 5, 6});
  equivalence(k, 1, 2);
  DenseSession dense(k);
  k.stats.conflicts = 1;

  assert(substitute_apply_sweep_equivalences(
             k, {{internalize(1), internalize(2)}}, dense.session) ==
         ApplyResult::Ok);
  const auto with_one = [](const Clause &c) { return contains(c, 1); };
  assert(count_live(k, with_one) == 1);
  assert(large_occurrences(dense.occ, internalize(1)).size() == 1);
  assert(!clause_at(k.arena, large_occurrences(dense.occ, internalize(1))[0])->redundant());
  assert_ledger_mirrors_kernel(k, dense.session);
  (void)dense.session.collect(k);
  assert(!kernel_var_retired(k, var_of(internalize(2))));
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(kernel_var_retired(k, var_of(internalize(2))));
  assert(count_live(k, with_one) == 2);
  assert(count_live(k, [](const Clause &c) {
           return c.redundant() && contains(c, 1);
         }) >= 1);
  kernel_dense_disconnect(k);
}

// A redundant-only neighbourhood is a Sweep no-op; the retained bridge lets
// Substitute own the redundant rewrite and the retirement.
static void test_sweep_does_not_rewrite_redundant_only_neighbourhood() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  add_external_clause(k, {2, 3, 4}, true, 2);
  equivalence(k, 1, 2);
  DenseSession dense(k);

  assert(substitute_apply_sweep_equivalences(
             k, {{internalize(1), internalize(2)}}, dense.session) ==
         ApplyResult::NoOp);
  assert(!kernel_var_retired(k, var_of(internalize(2))));
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(kernel_var_retired(k, var_of(internalize(2))));
  assert(count_live(k, [](const Clause &c) { return c.redundant(); }) == 1);
  assert(live_clause(k, {1, 3, 4}) && live_clause(k, {1, 3, 4})->redundant());
  (void)dense.session.collect(k);
}

static void test_canonical_substitution_preserves_large_clause_position() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  add_external_clause(k, {3, 4, 5});
  add_external_clause(k, {2, 4, 6});
  add_external_clause(k, {-3, 5, 6});
  equivalence(k, 1, 2);

  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(first_literals(k) ==
         std::vector<Lit>({internalize(3), internalize(1), internalize(-3)}));
}

// Kissat's `substitute_clauses` compacts in the existing header, so a
// root-false shortening keeps the Vivify-leftover priority bit.
static void test_canonical_root_shrink_preserves_vivify_priority() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Ref source = add_external_clause(k, {1, 2, 3, 4}, true, 3);
  clause_at(k.arena, source)->set_vivify(true);
  kernel_assign_unit(k, internalize(-1));
  assert(propagate_apply(k) == ApplyResult::Ok);

  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(count_live(k, [](const Clause &) { return true; }) == 1);
  const Clause *rewritten = live_clause(k, {2, 3, 4});
  assert(rewritten && rewritten->vivify());
}

// Substitution leaves an unrelated binary watch list in order (later bounded
// scans consume candidates in watch order).
static void test_substitution_preserves_unrelated_binary_watch_order() {
  Kernel k;
  kernel_reserve_vars(k, 7);
  equivalence(k, 1, 2);

  const Lit pivot = internalize(3);
  kernel_add_binary_clause(k, pivot, internalize(6));
  kernel_add_binary_clause(k, pivot, internalize(-4));
  kernel_add_binary_clause(k, pivot, internalize(7));
  kernel_add_binary_clause(k, pivot, internalize(5));
  const std::vector<Lit> before = binary_partners(k, pivot);
  assert(before.size() == 4);

  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(binary_partners(k, pivot) == before);
}

// The streaming path keeps multiplicity: two sources mapping onto a present
// target leave three copies.
static void test_substitution_preserves_binary_multiplicity() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  equivalence(k, 1, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(3));
  kernel_add_binary_clause(k, internalize(2), internalize(3));
  kernel_add_binary_clause(k, internalize(2), internalize(3));

  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(kernel_binary_copies(k, internalize(2), internalize(3)) == 0);
  assert(kernel_binary_copies(k, internalize(1), internalize(3)) == 3);
}

// Substitute deletes both source binaries and both proof-only SCC bridge
// copies; a stale proof-only binary can invalidate a later direct-SR witness.
static void test_retires_transient_equivalence_lemmas() {
  TemporaryFile file;
  {
    Kernel k;
    kernel_reserve_vars(k, 2);
    equivalence(k, 1, 2);
    Proof proof;
    assert(proof.open(file.path.c_str(), ProofFormat::DsrAscii));
    k.proof = &proof;
    assert(substitute_apply(k) == ApplyResult::Ok);
  }
  std::istringstream input(read_file(file.path));
  uint32_t additions = 0, deletions = 0;
  for (std::string line; std::getline(input, line);)
    if (line.rfind("d ", 0) == 0)
      deletions++;
    else if (!line.empty())
      additions++;
  assert(additions == 2);
  assert(deletions == 4);
}

// A transformed bridge pair that coincides with an ordinary live binary
// consumes that copy in the physical pass as the proof pass does; otherwise a
// survivor outlives its checker copies and a later lemma fails RUP
// (goldcrest-and-11, campaign 20260824-0337).
static void test_sweep_bridge_target_pair_collision_consumes_ordinary_copy() {
  TemporaryFile file;
  const auto pair_lines = [&](int ea, int eb) {
    std::istringstream input(read_file(file.path));
    uint32_t additions = 0, deletions = 0;
    for (std::string line; std::getline(input, line);) {
      if (line.empty())
        continue;
      std::istringstream tokens(line);
      std::string first;
      tokens >> first;
      const bool deletion = first == "d";
      std::vector<int> lits;
      int value = 0;
      if (!deletion)
        lits.push_back(std::stoi(first));
      while (tokens >> value && value != 0)
        lits.push_back(value);
      if (lits.size() != 2)
        continue;
      std::sort(lits.begin(), lits.end());
      if (lits[0] != std::min(ea, eb) || lits[1] != std::max(ea, eb))
        continue;
      (deletion ? deletions : additions)++;
    }
    return std::make_pair(additions, deletions);
  };
  {
    Kernel k;
    kernel_reserve_vars(k, 4);
    // Ordinary binary equal to the transformed bridge target below.
    kernel_add_binary_clause(k, internalize(-3), internalize(2));
    DenseSession dense(k);
    Proof proof;
    assert(proof.open(file.path.c_str(), ProofFormat::DsrAscii));
    k.proof = &proof;
    // Merges (2,1), (3,4), (4,2) preserve (¬3 ∨ 4) as (¬3 ∨ 2), the ordinary
    // pair above, while 2 retires to root 1.
    kernel_add_binary_clause(k, internalize(-2), internalize(1),
                             ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(2), internalize(-1),
                             ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(-3), internalize(4),
                             ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(3), internalize(-4),
                             ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(-4), internalize(2),
                             ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(4), internalize(-2),
                             ++k.next_clause_id);
    assert(substitute_apply_sweep_equivalences(
               k,
               {{internalize(2), internalize(1)},
                {internalize(3), internalize(4)},
                {internalize(4), internalize(2)}},
               dense.session) == ApplyResult::Ok);
    // The transformed bridge is the only physical copy of (¬3, 2).
    assert(kernel_binary_copies(k, internalize(-3), internalize(2)) == 1);
    assert(kernel_binary_copies(k, internalize(2), internalize(-3)) == 1);
    assert(kernel_binary_copies(k, internalize(-3), internalize(4)) == 0);
    assert(kernel_binary_copies(k, internalize(3), internalize(-2)) == 1);
    assert(kernel_binary_copies(k, internalize(3), internalize(-4)) == 0);
    // Untouched bridges stay in place for canonical Substitute.
    assert(kernel_binary_copies(k, internalize(-2), internalize(1)) == 1);
    assert(kernel_binary_copies(k, internalize(2), internalize(-1)) == 1);
    assert(kernel_binary_copies(k, internalize(-4), internalize(2)) == 1);
    assert(kernel_binary_copies(k, internalize(4), internalize(-2)) == 1);
    assert_ledger_mirrors_kernel(k, dense.session);
    assert(!kernel_var_retired(k, var_of(internalize(2))));
    (void)dense.session.collect(k);
    assert(substitute_apply(k) == ApplyResult::Ok);
    assert(kernel_var_retired(k, var_of(internalize(2))));
    assert(kernel_var_retired(k, var_of(internalize(3))));
    assert(kernel_var_retired(k, var_of(internalize(4))));
    assert(kernel_binary_copies(k, internalize(-3), internalize(2)) == 0);
    assert(kernel_binary_copies(k, internalize(2), internalize(-3)) == 0);
    k.proof = nullptr;
    kernel_dense_disconnect(k);
  }
  // The checker holds one input copy, so deletions exceed additions by exactly
  // one and no live copy survives.
  const auto [additions, deletions] = pair_lines(-3, 2);
  assert(deletions <= additions + 1);
  assert(additions + 1 - deletions == 0);
}

static void test_substitution_rewrites_stable_watch_in_place() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  equivalence(k, 1, 2);

  const Lit stable = internalize(3);
  kernel_add_binary_clause(k, stable, internalize(4));
  kernel_add_binary_clause(k, stable, internalize(1));
  kernel_add_binary_clause(k, stable, internalize(2));
  kernel_add_binary_clause(k, stable, internalize(5));
  assert(substitute_apply(k) == ApplyResult::Ok);
  const std::vector<Lit> expected = {internalize(4), internalize(1),
                                     internalize(1), internalize(5)};
  assert(binary_partners(k, stable) == expected);
}

// Kissat moves a retired literal's binary watches to its representative in
// the retiring watch list's order (x6 lists x4 before x2).
static void test_substitution_appends_moved_watches_in_source_order() {
  Kernel k;
  kernel_reserve_vars(k, 7);
  // x1 == x6, with x1 the import-order representative.
  equivalence(k, 1, 6);

  const Lit representative = internalize(1);
  const Lit retired = internalize(6);
  kernel_add_binary_clause(k, representative, internalize(7));
  kernel_add_binary_clause(k, retired, internalize(4));
  kernel_add_binary_clause(k, retired, internalize(2));

  assert(substitute_apply(k) == ApplyResult::Ok);
  const std::vector<Lit> watches = binary_partners(k, representative);
  const auto four = std::find(watches.begin(), watches.end(), internalize(4));
  const auto two = std::find(watches.begin(), watches.end(), internalize(2));
  assert(four != watches.end());
  assert(two != watches.end());
  assert(four < two);
}

// Kissat moves the two directed watches independently when both endpoints
// retire: x8 lists x7 before x6, so x1 receives x2 before x3.
static void test_substitution_moves_both_directions_independently() {
  Kernel k;
  kernel_reserve_vars(k, 8);
  equivalence(k, 1, 8);
  equivalence(k, 2, 7);
  equivalence(k, 3, 6);

  kernel_add_binary_clause(k, internalize(8), internalize(7));
  kernel_add_binary_clause(k, internalize(8), internalize(6));

  assert(substitute_apply(k) == ApplyResult::Ok);
  const std::vector<Lit> watches = binary_partners(k, internalize(1));
  const auto two = std::find(watches.begin(), watches.end(), internalize(2));
  const auto three = std::find(watches.begin(), watches.end(), internalize(3));
  assert(two != watches.end());
  assert(three != watches.end());
  assert(two < three);
}

// Model reconstruction through ρ: a model of the reduced formula with x2 at the
// wrong value must extend to satisfy every original clause.
static void test_model_reconstruction() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  equivalence(k, 1, 2);
  kernel_add_binary_clause(k, internalize(2), internalize(3));
  // original clauses (DIMACS), captured before substitution
  const int orig[3][2] = {{-1, 2}, {-2, 1}, {2, 3}};

  assert(substitute_apply(k) == ApplyResult::Ok);

  // reduced formula (x1 ∨ x3): model x1=F, x3=T, with x2=T against its rep
  auto set = [&](int dimacs, bool val) {
    const Lit l = internalize(dimacs);
    k.values[l] = val ? 1 : -1;
    k.values[negate(l)] = val ? -1 : 1;
  };
  set(1, false);
  set(3, true);
  set(2, true); // the deliberately wrong pre-extend value

  kernel_extend_model(k);
  assert(k.values[internalize(2)] < 0 && "x2 reconstructed to x1's value (F)");
  for (const auto &cl : orig) {
    bool sat = false;
    for (const int e : cl)
      sat |= (k.values[internalize(e)] > 0);
    assert(sat && "every original clause satisfied by the extended model");
  }
}

// A binary collapsing to a unit: (x2 ∨ x1) under x1 ≡ x2 asserts the unit x1.
static void test_size_collapse_unit() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  equivalence(k, 1, 2);
  kernel_add_binary_clause(k, internalize(2), internalize(1));  // (x2 ∨ x1)
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(1)) > 0 && "x1 forced true by the collapsed unit");
  assert(k.propagate_head == k.trail.size() &&
         "binary-collapse units reach root closure before large rewrites");
}

// A failed SCC root can fix a member after Tarjan froze the map: Kissat skips
// its retirement but still applies the complete map in substitute_binaries.
static void test_failed_root_member_still_rewrites_physical_binaries() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit rep = internalize(1), member = internalize(2);
  const Lit witness = internalize(3), spectator = internalize(4);
  equivalence(k, 2, 1);
  // Assuming -rep forces both witness signs, so SCC detection derives rep.
  kernel_add_binary_clause(k, rep, witness);
  kernel_add_binary_clause(k, rep, negate(witness));
  kernel_add_binary_clause(k, member, spectator);

  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(k.value(rep) > 0 && k.value(member) > 0);
  assert(!kernel_var_retired(k, var_of(member)));
  assert(!kernel_binary_live(k, member, spectator));
  assert(kernel_binary_live(k, rep, spectator));
  assert(!kernel_binary_live(k, negate(member), rep));
  assert(!kernel_binary_live(k, negate(rep), member));
}

// N1: one instance substitutes just that class; a junk payload is a total NoOp.
static void test_apply_instance_one_class() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  equivalence(k, 1, 2);
  equivalence(k, 3, 4);
  const int rid = tactix_rule_id("Substitute");
  const RuleVTable &vt = tactix_rules()[rid];
  assert(vt.apply_instance(k, lit_instance(internalize(99))) ==
         ApplyResult::NoOp);
  // substitute just the x1≡x2 class (rep = +1); x2 goes, x3/x4 stay
  assert(vt.apply_instance(k, lit_instance(internalize(1))) == ApplyResult::Ok);
  assert(!lit_in_any_clause(k, internalize(2)));
  assert(lit_in_any_clause(k, internalize(3)) ||
         lit_in_any_clause(k, internalize(4))); // x3≡x4 untouched
  SccResult r = substitute_find_sccs(k);
  assert(r.nontrivial_reps.size() == 2); // only {+3,+4} and {-3,-4} remain
}

// A round deriving two contradictory root units must refute, not drop one: here
// {1} comes from the rewritten large clause and {¬1} from the binary (¬1∨¬2),
// and neither fires by propagation alone.
static void test_contradictory_units_refute() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  equivalence(k, 1, 2);
  add_external_clause(k, {1, 2, -3});
  kernel_add_binary_clause(k, internalize(-1), internalize(-2)); // (¬1 ∨ ¬2)
  kernel_assign_unit(k, internalize(3)); // 3 true → ¬3 root-false
  assert(propagate_apply(k) == ApplyResult::Ok); // no direct conflict
  assert(substitute_apply(k) == ApplyResult::Unsat);
  assert(k.inconsistent);
}

// Guard: pure quiescent-root precondition, no fixpoint (ledger row D-LEVEL).
static void test_guard() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  assert(substitute_guard(k));           // empty trail = trivial fixpoint
  kernel_assign_unit(k, internalize(1)); // un-propagated root unit
  assert(substitute_guard(k));
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(substitute_guard(k));
  kernel_assign_decision(k, internalize(2));
  assert(!substitute_guard(k)); // level > 0 is still not a root state
}

// Connected Sweep rewrites keep the retiring occurrence, unlike canonical
// first-occurrence deduplication. Merge-forest order covers batched chains.
static void test_sweep_duplicate_literal_pose() {
  struct Case {
    std::vector<int> input, expected;
    std::vector<std::pair<int, int>> merges;
  };
  const Case cases[] = {
      {{1, 4, 2, 5}, {4, 1, 5}, {{1, 2}}},
      {{2, 4, 1, 5}, {1, 4, 5}, {{1, 2}}},
      {{4, 2, 5, 6, 3}, {4, 5, 6, 1}, {{1, 2}, {1, 3}}},
      {{4, 3, 5, 6, 2}, {4, 1, 5, 6}, {{1, 2}, {1, 3}}},
      {{2, 5, 3, 6, 4}, {5, 6, 1}, {{3, 4}, {2, 3}, {1, 2}}},
  };
  for (const auto &test : cases) {
    Kernel k;
    kernel_reserve_vars(k, 6);
    std::vector<Lit> input, expected;
    for (int lit : test.input)
      input.push_back(internalize(lit));
    for (int lit : test.expected)
      expected.push_back(internalize(lit));
    const Ref ref =
        kernel_add_large_clause(k, input.data(), input.size(), false, 0);
    DenseSession dense(k);
    std::vector<std::pair<Lit, Lit>> merges;
    for (const auto &[a, b] : test.merges) {
      const Lit x = internalize(a), y = internalize(b);
      bridge(k, x, y);
      merges.emplace_back(x, y);
    }
    assert(substitute_apply_sweep_equivalences(k, merges, dense.session) == ApplyResult::Ok);
    const Clause *clause = clause_at(k.arena, ref);
    assert(!clause->garbage());
    assert(std::vector<Lit>(clause->begin(), clause->end()) == expected);
    assert_ledger_mirrors_kernel(k, dense.session);
    kernel_dense_disconnect(k);
  }
}

// Canonical Substitute republishes an attached mixed dense stream (an N1
// schedule can keep Eliminate's attached): the dense oracle in
// kernel_check_invariants accepts it and retired literals hold no entries.
static void test_canonical_substitution_republishes_attached_mixed_stream() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  equivalence(k, 1, 2);
  kernel_add_binary_clause(k, internalize(2), internalize(3));
  kernel_add_binary_clause(k, internalize(-2), internalize(4));
  add_external_clause(k, {2, 3, 5});
  add_external_clause(k, {-2, 4, 5});
  OccIndex occ;
  kernel_dense_connect(k, occ, /*track_order=*/true);
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(!k.inconsistent);
  for (const Lit retired : {internalize(2), internalize(-2)}) {
    assert(k.watches.binary_size(retired) == 0);
    assert(occ.ordered[retired].size() == 0);
    assert(large_occurrences(occ, retired).size() == 0);
  }
  kernel_check_invariants(k);
  kernel_dense_disconnect(k);
}

int main() {
  test_canonical_substitution_republishes_attached_mixed_stream();
  test_sweep_duplicate_literal_pose();
  test_equivalence_scc();
  test_scc_charges_two_scans_per_literal();
  test_instance_enumeration_does_not_charge_work();
  test_representative_uses_import_order();
  test_no_scc();
  test_no_scc_still_simplifies_root_fixed_clauses();
  test_retired_variable_excluded_from_scc();
  test_contradiction_scc();
  test_contradictory_units_refute();
  test_apply_substitutes();
  test_canonical_substitution_preserves_glue_on_shrink();
  test_canonical_substitution_leaves_collapsed_source_lazy();
  test_complete_closes_exposed_scc();
  test_sweep_equivalence_preserves_large_clause_position();
  test_substitution_factor_marks_match_calling_path();
  test_sweep_preserves_cross_kind_dense_order();
  test_sweep_flushes_one_source_stream_at_a_time();
  test_sweep_bridges_link_intermediate_representative();
  test_sweep_flushes_sources_in_emission_order();
  test_sweep_chained_collapse_uses_last_mixed_stream();
  test_sweep_syncs_new_binaries_by_multiset();
  test_sweep_reposes_only_last_bridge_copy();
  test_sweep_identity_rewrite_has_no_dirty_binary_owners();
  test_sweep_records_binaries_of_a_declined_batch();
  test_sweep_ledger_declines_the_same_members_as_the_rewrite();
  test_sweep_leaves_redundant_clause_outside_dense_transaction();
  test_sweep_rewrites_only_connected_irredundant_clause();
  test_sweep_does_not_rewrite_redundant_only_neighbourhood();
  test_canonical_substitution_preserves_large_clause_position();
  test_canonical_root_shrink_preserves_vivify_priority();
  test_substitution_preserves_unrelated_binary_watch_order();
  test_substitution_preserves_binary_multiplicity();
  test_retires_transient_equivalence_lemmas();
  test_sweep_bridge_target_pair_collision_consumes_ordinary_copy();
  test_substitution_rewrites_stable_watch_in_place();
  test_substitution_appends_moved_watches_in_source_order();
  test_substitution_moves_both_directions_independently();
  test_model_reconstruction();
  test_size_collapse_unit();
  test_failed_root_member_still_rewrites_physical_binaries();
  test_apply_instance_one_class();
  test_guard();
  puts("test_substitute OK");
  return 0;
}
