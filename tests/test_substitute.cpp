// V3 Substitute: SCC detection over the binary implication graph, the round,
// the instance surface, and model reconstruction through ρ. A binary (¬p ∨ q)
// is the implication p → q. See docs/calculus.md §V3.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "occurrence_test_utils.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/substitute/substitute.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

using namespace tactix;

// Congruence can ask for a bridge after root propagation has valued an
// intermediate BIG node. The proved root unit replaces the incoming edge in
// the LRAT propagation chain; filtering every valued node used to lose this
// otherwise legal bridge.
static void test_big_path_chain_crosses_root_true_intermediate() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const uint64_t x_to_root = ++k.next_clause_id;
  kernel_add_binary_clause(k, internalize(-1), internalize(2), x_to_root);
  const uint64_t root_to_y = ++k.next_clause_id;
  kernel_add_binary_clause(k, internalize(-2), internalize(3), root_to_y);
  kernel_assign_unit(k, internalize(2));
  const uint64_t root_id = ++k.next_clause_id;
  k.unit_ids[var_of(internalize(2))] = root_id;

  std::vector<uint64_t> chain;
  assert(big_path_chain(k, internalize(1), internalize(3), chain));
  assert((chain == std::vector<uint64_t>{root_id, root_to_y}));
}

// x1 ≡ x2 via x1 -> x2 -> x1: SCC {+1,+2} plus its complement {-1,-2}
static void test_equivalence_scc() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // x1 -> x2
  kernel_add_binary_clause(k, internalize(-2), internalize(1)); // x2 -> x1
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

// Tactix keeps stable external-index-backed storage, while Kissat assigns its
// compact internal indices in first-occurrence order. SCC representatives must
// use the recorded import order or equivalent formulas retain different
// decision variables after preprocessing.
// Kissat's DFS charges each visited literal's watch cache lines on first
// visit and again on the finishing rescan that folds its reach; the
// single-scan Tarjan pass must charge both so round-one `substitute_ticks`
// equal kissat's and the 10-per-mille second-round test agrees.
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

static void test_representative_uses_import_order() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_record_import(k, 1); // external x2 appears before x1
  kernel_record_import(k, 2);
  kernel_record_import(k, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  kernel_add_binary_clause(k, internalize(-3), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(3));

  const SccResult r = substitute_find_sccs(k);
  assert(r.repr[internalize(1)] == internalize(2));
  assert(r.repr[internalize(-1)] == internalize(-2));
  assert(r.repr[internalize(3)] == internalize(2));
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(kernel_var_retired(k, 0));
  assert(!kernel_var_retired(k, 1));
  assert(kernel_var_retired(k, 2));
  // Kissat's `remove_representative_equivalences` scans its compact internal
  // variables, which are first-import ordered.  Stable external-backed Var
  // numbers would retire x1 before x3 and mutate an equal-score heap
  // differently.
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

// Kissat runs its large-clause substitution pass even when Tarjan found no
// equivalence class.  It therefore commits root simplification left by the
// preceding Sweep: a root-false literal can turn a ternary into a binary.
static void test_no_scc_still_simplifies_root_fixed_clauses() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit clause[3] = {internalize(-1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, clause, 3, /*redundant=*/false, /*glue=*/0);
  kernel_assign_unit(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);

  assert(substitute_find_sccs(k).nontrivial_reps.empty());
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(kernel_binary_live(k, internalize(2), internalize(3)));
  assert(substitute_apply(k) == ApplyResult::NoOp);
}

// Kissat's SCC graph excludes inactive variables.  A retired permanent
// identity may still be mentioned by stale binary material, but must not form
// another substitution class and emit a duplicate retirement event.
static void test_retired_variable_excluded_from_scc() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  k.var_status.assign(k.nvars, VarStatus::Active);
  k.var_status[1] = VarStatus::Substituted;
  k.unassigned_active--;

  const SccResult r = substitute_find_sccs(k);
  assert(r.nontrivial_reps.empty());
  assert(r.repr[internalize(2)] == INVALID_LIT);
  assert(substitute_apply(k) == ApplyResult::NoOp);
  assert(k.retired_events.empty() && k.substituted_events.empty());
}

// x ≡ ¬x: x, a, and ¬x all collapse into one SCC -> UNSAT contradiction
static void test_contradiction_scc() {
  Kernel k;
  kernel_reserve_vars(k, 2); // x = var 1, a = var 2
  kernel_add_binary_clause(k, internalize(-1), internalize(2));  // x  -> a
  kernel_add_binary_clause(k, internalize(-2), internalize(-1)); // a  -> ¬x
  kernel_add_binary_clause(k, internalize(1), internalize(2));   // ¬x -> a
  kernel_add_binary_clause(k, internalize(-2), internalize(1));  // a  -> x
  SccResult r = substitute_find_sccs(k);
  assert(r.contradiction);
  assert(r.contradiction_lit != INVALID_LIT);
  assert(r.repr[r.contradiction_lit] == r.repr[negate(r.contradiction_lit)]);
}

// The canonical round refutes x ≡ ¬x: the rule's Unsat contract.
static void test_apply_unsat() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(-1));
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  assert(substitute_guard(k));
  assert(substitute_apply(k) == ApplyResult::Unsat);
  assert(k.inconsistent);
  assert(k.stats.substitute_rounds == 1);
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

static std::vector<Lit> binary_watch_others(const Kernel &k, Lit lit) {
  std::vector<Lit> result;
  for (const WatchEntry watch : k.watches.entries(lit))
    if (watch.binary())
      result.push_back(watch.other());
  return result;
}

// A consistent x1 ≡ x2 is substituted: the non-rep x2 vanishes everywhere.
static void test_apply_substitutes() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  const Lit c[3] = {internalize(2), internalize(3), internalize(4)};
  kernel_add_large_clause(k, c, 3, /*redundant=*/false, /*glue=*/0);

  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(!k.inconsistent);
  assert(!k.substituted_events.empty());
  assert(k.retired_events == k.substituted_events);
  assert(!lit_in_any_clause(k, internalize(2))); // x2 gone, both polarities
  assert(!lit_in_any_clause(k, internalize(-2)));
  // the large clause was rewritten x2 -> x1: (x1∨x3∨x4) is present
  bool found = false;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *cc = clause_at(k.arena, ref);
    if (!cc->garbage() && cc->size == 3) {
      bool h1 = false, h3 = false, h4 = false;
      for (const Lit m : *cc) {
        h1 |= (m == internalize(1));
        h3 |= (m == internalize(3));
        h4 |= (m == internalize(4));
      }
      found |= (h1 && h3 && h4);
    }
    ref += clause_actual_words(cc);
  }
  assert(found && "large clause rewritten to (x1 v x3 v x4)");
  SccResult r2 = substitute_find_sccs(k); // re-detection: the SCC is gone
  assert(r2.nontrivial_reps.empty());
  // a witness pair was pushed for the eliminated variable
  assert(!k.reconstruction.empty());
}

// Canonical Kissat substitution shrinks a surviving large clause in place
// without changing its glue. This differs deliberately from the connected
// Sweep substitution path, which applies the usual size-1 promotion.
static void test_canonical_substitution_preserves_glue_on_shrink() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  const Lit lits[6] = {internalize(1), internalize(2), internalize(3),
                       internalize(4), internalize(5), internalize(6)};
  kernel_add_large_clause(k, lits, 6, /*redundant=*/true, /*glue=*/5);

  assert(substitute_apply(k) == ApplyResult::Ok);
  const Clause *rewritten = nullptr;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *candidate = clause_at(k.arena, ref);
    ref += clause_actual_words(candidate);
    if (!candidate->garbage() && candidate->redundant() &&
        candidate->size == 5)
      rewritten = candidate;
  }
  assert(rewritten);
  assert(rewritten->size == 5);
  assert(rewritten->glue() == 5);
}

// Canonical Substitute matches Kissat's physical lifecycle: a large source
// which collapses to binary stays readable as lazy garbage until a real arena
// owner (Reduce or Eliminate) collects it.
static void test_canonical_substitution_leaves_collapsed_source_lazy() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  const Lit source[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, source, 3, false, 0);
  const uint64_t arena_generation = k.arena_generation;

  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(k.arena.size() == clause_words(3));
  assert(k.arena_generation == arena_generation);
  assert(clause_at(k.arena, 0)->garbage());
  assert(kernel_binary_live(k, internalize(1), internalize(3)));
  kernel_check_invariants(k);
}

// A first equivalence rewrite can expose another equivalence by shrinking
// large clauses to binaries. Kissat's preprocessing `complete=true` call runs
// a second SCC round and consumes that class before search.
static void test_complete_closes_exposed_scc() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  // x1 ≡ x2 is visible initially.
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  // Rewriting x2 -> x1 turns these into the two implications x1 ≡ x3.
  const Lit a[3] = {internalize(1), internalize(-3), internalize(2)};
  const Lit b[3] = {internalize(-1), internalize(3), internalize(-2)};
  kernel_add_large_clause(k, a, 3, false, 0);
  kernel_add_large_clause(k, b, 3, false, 0);

  assert(substitute_apply_complete(k, 2) == ApplyResult::Ok);
  assert(k.stats.substitute_rounds == 2);
  assert(kernel_var_retired(k, 1));
  assert(kernel_var_retired(k, 2));
  assert(!lit_in_any_clause(k, internalize(2)));
  assert(!lit_in_any_clause(k, internalize(3)));
}

// Kissat's Sweep-connected substitution rewrites a surviving large clause in
// place. Tactix now does the same; the Sweep helper must also update its mixed
// dense stream immediately because later capped environments observe order.
static void test_sweep_equivalence_preserves_large_clause_position() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit before[3] = {internalize(3), internalize(4), internalize(5)};
  const Lit touched[3] = {internalize(2), internalize(4), internalize(6)};
  const Lit after[3] = {internalize(-3), internalize(4), internalize(6)};
  kernel_add_large_clause(k, before, 3, false, 0);
  const Ref touched_ref = kernel_add_large_clause(k, touched, 3, false, 0);
  clause_at(k.arena, touched_ref)->set_vivify(true);
  kernel_add_large_clause(k, after, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  OccIndex occ;
  kernel_dense_connect(k, occ);

  const std::vector<std::pair<Lit, Lit>> equivalences = {
      {internalize(1), internalize(2)}};
  SweepSubstitutionSession session;
  session.begin(k);
  assert(substitute_apply_sweep_equivalences(k, equivalences, session) ==
         ApplyResult::Ok);
  assert(session.changed);

  const auto find_rewritten = [&]() -> const Clause * {
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      const uint32_t words = clause_actual_words(c);
      if (!c->garbage() && c->size == 3) {
        bool has_one = false, has_four = false, has_six = false;
        for (const Lit lit : *c) {
          has_one |= lit == internalize(1);
          has_four |= lit == internalize(4);
          has_six |= lit == internalize(6);
        }
        if (has_one && has_four && has_six)
          return c;
      }
      ref += words;
    }
    return nullptr;
  };
  assert(find_rewritten() && find_rewritten()->vivify());

  // The next environment is built before the session publishes its final
  // mixed stream. Its live x4 occurrences must already retain
  // before/touched/after order.
  std::vector<Lit> live_dense_first;
  for (const Ref ref : large_occurrences(occ, internalize(4))) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage())
      live_dense_first.push_back(c->lits[0]);
  }
  assert(live_dense_first ==
         std::vector<Lit>({internalize(3), internalize(1), internalize(-3)}));
  // In-place replacement leaves no representation debt: the boundary call is
  // kept for the interface, but it owes no arena rebuild.
  (void)session.collect(k);
  assert(find_rewritten() && find_rewritten()->vivify());

  std::vector<Lit> first_literals;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    assert(!c->garbage());
    first_literals.push_back(c->lits[0]);
    ref += clause_actual_words(c);
  }
  assert(first_literals.size() == 3);
  assert(first_literals[0] == internalize(3));
  assert(first_literals[1] == internalize(1));
  assert(first_literals[2] == internalize(-3));
}

// Pinned Kissat's two large-clause substitution paths have different added-
// material wiring. Canonical substitute.c overwrites a surviving image
// without re-arming Factor, while Sweep's connected rewrite explicitly marks
// every surviving irredundant literal. The shared Tactix rewrite primitive
// must preserve that distinction.
static void test_substitution_factor_marks_match_calling_path() {
  {
    Kernel k;
    kernel_reserve_vars(k, 4);
    kernel_add_binary_clause(k, internalize(-1), internalize(2));
    kernel_add_binary_clause(k, internalize(-2), internalize(1));
    const Lit clause[3] = {internalize(2), internalize(3), internalize(4)};
    kernel_add_large_clause(k, clause, 3, false, 0);
    std::fill(k.factor_marks.begin(), k.factor_marks.end(), 0);
    k.factor_literals_marked = 0;

    assert(substitute_apply(k) == ApplyResult::Ok);
    assert(!k.factor_marks[internalize(3)]);
    assert(!k.factor_marks[internalize(4)]);
  }

  {
    Kernel k;
    kernel_reserve_vars(k, 4);
    const Lit clause[3] = {internalize(2), internalize(3), internalize(4)};
    kernel_add_large_clause(k, clause, 3, false, 0);
    kernel_add_binary_clause(k, internalize(-1), internalize(2));
    kernel_add_binary_clause(k, internalize(1), internalize(-2));
    OccIndex occ;
    kernel_dense_connect(k, occ);
    SweepSubstitutionSession session;
    session.begin(k);
    std::fill(k.factor_marks.begin(), k.factor_marks.end(), 0);
    k.factor_literals_marked = 0;

    assert(substitute_apply_sweep_equivalences(
               k, {{internalize(1), internalize(2)}}, session) ==
           ApplyResult::Ok);
    assert(k.factor_marks[internalize(3)]);
    assert(k.factor_marks[internalize(4)]);
  }
}

// In Kissat's dense Sweep list, a large clause that collapses to a binary
// replaces the stable endpoint's occurrence in place but appends the moved
// representative's direction. The resulting large/binary interleaving is
// observed by the very next bounded environment.
static void test_sweep_preserves_cross_kind_dense_order() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit before[3] = {internalize(1), internalize(3), internalize(5)};
  const Lit collapse[3] = {internalize(2), internalize(1), internalize(4)};
  const Lit after[3] = {internalize(1), internalize(-3), internalize(-5)};
  kernel_add_large_clause(k, before, 3, false, 0);
  kernel_add_large_clause(k, collapse, 3, false, 0);
  kernel_add_large_clause(k, after, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  OccIndex occ;
  kernel_dense_connect(k, occ, true);

  SweepSubstitutionSession session;
  session.begin(k);
  const std::vector<std::pair<Lit, Lit>> equivalences = {
      {internalize(1), internalize(2)}};
  assert(substitute_apply_sweep_equivalences(k, equivalences, session) ==
         ApplyResult::Ok);

  const auto &with_bridge = session.occurrences(k, internalize(1));
  std::vector<SweepSubstitutionSession::DenseOccurrence> ordered;
  for (const auto occurrence : with_bridge)
    if (!(occurrence.binary && occurrence.payload == internalize(-2)))
      ordered.push_back(occurrence);
  assert(ordered.size() == 3);
  assert(!ordered[0].binary && !ordered[1].binary);
  assert(ordered[2].binary && ordered[2].payload == internalize(4));
  (void)session.collect(k);
  const auto &published = occ.ordered[internalize(1)];
  assert(published.size() == with_bridge.size());
  for (size_t i = 0; i < published.size(); ++i) {
    assert(static_cast<bool>(published[i].binary) == with_bridge[i].binary);
    assert(published[i].payload == with_bridge[i].payload);
  }
  kernel_check_invariants(k);
}

// The logical dense ledger follows Kissat's cross-kind order, while Tactix's
// physical binary watches can retain a different order after an inline
// rewrite. Synchronization matches the old multiset, appends only the new
// binary, and then publishes that order to physical propagation watches.
static void test_sweep_syncs_new_binaries_by_multiset() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit one = internalize(1);
  kernel_add_binary_clause(k, one, internalize(2));
  kernel_add_binary_clause(k, one, internalize(3));
  OccIndex occ;
  kernel_dense_connect(k, occ, true);
  SweepSubstitutionSession session;
  session.begin(k);
  auto &one_stream = session.occurrences(k, one);
  std::swap(one_stream[0], one_stream[1]);

  kernel_add_binary_clause(k, one, internalize(4));
  session.sync_new_binaries(k, {{one, internalize(4)}});
  const auto &ordered = session.occurrences(k, one);
  assert(ordered.size() == 3);
  assert(ordered[0].payload == internalize(3));
  assert(ordered[1].payload == internalize(2));
  assert(ordered[2].payload == internalize(4));
  std::vector<Lit> physical;
  for (const WatchEntry watch : k.watches.entries(one))
    if (watch.binary())
      physical.push_back(watch.other());
  assert((physical == std::vector<Lit>{internalize(3), internalize(2),
                                     internalize(4)}));
  assert(occ.ordered[one].size() == ordered.size());
  for (size_t i = 0; i < ordered.size(); ++i) {
    assert(occ.ordered[one][i].binary == ordered[i].binary);
    assert(occ.ordered[one][i].payload == ordered[i].payload);
  }

  // A stale mirror can sit under a spectator rather than a class endpoint.
  // Mutation provenance names both directed owners, so reconciliation removes
  // both ledger-only directions without scanning either owner's neighbors.
  const Lit phantom_a = internalize(3), phantom_b = internalize(-4);
  session.occurrences(k, phantom_a).push_back({true, phantom_b});
  session.occurrences(k, phantom_b).push_back({true, phantom_a});
  session.dirty_binary_owners.insert(session.dirty_binary_owners.end(),
                                     {phantom_a, phantom_b});
  session.sync_new_binaries(k, {{one, internalize(2)}});
  for (const auto occurrence : session.occurrences(k, phantom_a))
    assert(!occurrence.binary || occurrence.payload != phantom_b);
  for (const auto occurrence : session.occurrences(k, phantom_b))
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
  OccIndex occ;
  kernel_dense_connect(k, occ, true);
  SweepSubstitutionSession session;
  session.begin(k);
  session.sync_new_binaries(k, {{a, b}});
  const auto id = kernel_binary_id(k, negate(a), b);
  const auto count = k.watches.size(negate(a));
  session.repose_emitted_equivalence(k, a, b);
  session.sync_new_binaries(k, {{a, b}});
  std::vector<Lit> physical;
  for (const WatchEntry watch : k.watches.entries(negate(a)))
    if (watch.binary())
      physical.push_back(watch.other());
  assert((physical == std::vector<Lit>{b, c, d, b}));
  assert(k.watches.size(negate(a)) == count);
  assert(kernel_binary_id(k, negate(a), b) == id);
  session.repose_emitted_equivalence(k, a, internalize(5));
  session.sync_new_binaries(k, {{a, internalize(5)}});
  assert(!kernel_binary_live(k, negate(a), internalize(5)));
  assert(k.watches.size(negate(a)) == count);
  kernel_check_invariants(k);
}

static void assert_ledger_mirrors_kernel(Kernel &,
                                         SweepSubstitutionSession &);

// Reconciliation is mutation-provenance bounded. An identity transaction has
// no retiring connected frontier, so it must neither visit unrelated binary
// owners nor report them dirty and trigger a reconciliation scan.
static void test_sweep_identity_rewrite_has_no_dirty_binary_owners() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  kernel_add_binary_clause(k, internalize(1), internalize(3));
  kernel_add_binary_clause(k, internalize(-2), internalize(4));
  kernel_add_binary_clause(k, internalize(-4), internalize(5));
  OccIndex occ;
  kernel_dense_connect(k, occ);
  SweepSubstitutionSession session;
  session.begin(k);
  assert(session.dense_occurrences.empty());

  session.rewrite_binary_occurrences(k, {}, {}, {});
  assert(session.dense_occurrences.empty());
  assert(session.dirty_binary_owners.empty());
  session.flush_moves(k);
  assert_ledger_mirrors_kernel(k, session);
  kernel_dense_disconnect(k);
}

// The session's contract (substitute.hpp): for every literal the ledger's
// binary sub-multiset is exactly that literal's physical binary watch multiset,
// and its large entries name exactly the live irredundant records `Kernel::
// dense` holds for it. Membership belongs to the kernel; the ledger only adds
// Kissat's dense stream order. A ledger claiming a clause the formula does not
// hold puts a phantom clause into the next environment, and one missing a
// clause hides a real one, so both directions are checked.
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
    const auto sorted = [](std::vector<Lit> v) {
      std::sort(v.begin(), v.end());
      return v;
    };
    const auto sorted_refs = [](std::vector<Ref> v) {
      std::sort(v.begin(), v.end());
      return v;
    };
    assert(sorted(ledger_binaries) == sorted(watch_binaries));
    assert(sorted_refs(ledger_large) == sorted_refs(dense_large));
  }
}

// Kissat retires one literal at a time and walks its single unified watch
// stream, so the binaries and the large clauses that stream releases reach the
// representative interleaved, in the order they were met — not all binaries
// and then all large clauses. Only a source whose large occurrence precedes
// one of its binaries can tell the two rules apart, and a moved binary lands
// behind the larges already indexed for that literal, so the discriminating
// stream is the one a previous substitution built.
static void test_sweep_flushes_one_source_stream_at_a_time() {
  Kernel k;
  kernel_reserve_vars(k, 8);
  const Lit one = internalize(1), two = internalize(2), three = internalize(3);
  // x1's own large clause: untouched by both classes, so it keeps its slot and
  // the moves have to land behind it.
  const Lit resident[3] = {internalize(1), internalize(7), internalize(8)};
  // x2's large clause: it survives as a large clause and introduces x1.
  const Lit connected[3] = {internalize(2), internalize(6), internalize(7)};
  kernel_add_large_clause(k, resident, 3, false, 0);
  kernel_add_large_clause(k, connected, 3, false, 0);
  kernel_add_binary_clause(k, three, internalize(5));
  OccIndex occ;
  kernel_dense_connect(k, occ);

  SweepSubstitutionSession session;
  session.begin(k);
  // First class x2 = x3: x3's binary moves to x2 and is appended behind x2's
  // large clause, which is exactly the stream shape Kissat then walks.
  kernel_add_binary_clause(k, negate(two), three);
  kernel_add_binary_clause(k, two, negate(three));
  assert(substitute_apply_sweep_equivalences(k, {{two, three}}, session) ==
         ApplyResult::Ok);
  assert_ledger_mirrors_kernel(k, session);
  {
    const auto &with_bridge = session.occurrences(k, two);
    std::vector<SweepSubstitutionSession::DenseOccurrence> source;
    for (const auto occurrence : with_bridge)
      if (!(occurrence.binary && var_of(occurrence.payload) == var_of(three)))
        source.push_back(occurrence);
    assert(source.size() == 2);
    assert(!source[0].binary); // the large clause was met first
    assert(source[1].binary && source[1].payload == internalize(5));
  }

  // Second class x1 = x2 retires that whole stream into x1: the large clause
  // must arrive before the binary that sat behind it.
  kernel_add_binary_clause(k, negate(one), two);
  kernel_add_binary_clause(k, one, negate(two));
  assert(substitute_apply_sweep_equivalences(k, {{one, two}}, session) ==
         ApplyResult::Ok);
  assert_ledger_mirrors_kernel(k, session);

  const auto &with_bridges = session.occurrences(k, one);
  std::vector<SweepSubstitutionSession::DenseOccurrence> ordered;
  for (const auto occurrence : with_bridges)
    if (!(occurrence.binary && var_of(occurrence.payload) <= var_of(three)))
      ordered.push_back(occurrence);
  assert(ordered.size() == 3);
  assert(!ordered[0].binary); // x1's own resident clause keeps its slot
  const Clause *const kept = clause_at(k.arena, ordered[0].payload);
  assert(!kept->garbage() && std::find(kept->begin(), kept->end(),
                                       internalize(8)) != kept->end());
  assert(!ordered[1].binary); // then x2's large clause, rewritten to x1
  const Clause *const inherited = clause_at(k.arena, ordered[1].payload);
  assert(!inherited->garbage() && inherited->size == 3);
  assert(std::find(inherited->begin(), inherited->end(), internalize(6)) !=
         inherited->end());
  assert(std::find(inherited->begin(), inherited->end(), one) !=
         inherited->end());
  assert(ordered[2].binary && ordered[2].payload == internalize(5));
  (void)session.collect(k);
  kernel_dense_disconnect(k);
}

// A class winner can lose a later merge of the same emission batch through a
// pair that never names it. Kissat's `substitute_connected_clauses` exempts a
// fresh equivalence binary only from its own pair's substitution; every later
// merge still rewrites both of its endpoints, so the preserved bridge of an
// early pair carries the intermediate winner, not the final representative.
// Pinning one endpoint and jumping the other to the final representative
// leaves such a winner with no preserved equivalence binary at all: the
// mandated Substitute cannot rediscover it, and an active variable whose
// occurrences were all rewritten away answers wrong models.
static void test_sweep_bridges_link_intermediate_representative() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit one = internalize(1), two = internalize(2), three = internalize(3),
            four = internalize(4);
  kernel_add_binary_clause(k, one, internalize(5));
  kernel_add_binary_clause(k, two, internalize(5));
  kernel_add_binary_clause(k, three, internalize(6));
  kernel_add_binary_clause(k, four, internalize(6));
  OccIndex occ;
  kernel_dense_connect(k, occ);

  SweepSubstitutionSession session;
  session.begin(k);
  // Emission order: x3 absorbs x4, then x2 absorbs {x3, x4}, and x1 absorbs
  // the whole class through the pair (x4, x1). x2 wins its own pair and is
  // named by no later one.
  kernel_add_binary_clause(k, negate(three), four);
  kernel_add_binary_clause(k, three, negate(four));
  kernel_add_binary_clause(k, negate(three), two);
  kernel_add_binary_clause(k, three, negate(two));
  kernel_add_binary_clause(k, negate(four), one);
  kernel_add_binary_clause(k, four, negate(one));
  assert(substitute_apply_sweep_equivalences(
             k, {{three, four}, {three, two}, {four, one}}, session) ==
         ApplyResult::Ok);
  assert_ledger_mirrors_kernel(k, session);
  assert(k.retired_events.empty());
  // The suffix images of the three emitted pairs: x4 = x1 and x3 = x2 keep
  // their own binaries, and the first pair's binaries carry x2 = x1.
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
  (void)session.collect(k);
  kernel_dense_disconnect(k);
}

// Two retiring literals can share one representative. Kissat empties them in
// the order `sweep_equivalence` proved them, one complete stream at a time;
// walking the class in import order instead interleaves two sources that the
// reference keeps contiguous and in the opposite order.
static void test_sweep_flushes_sources_in_emission_order() {
  Kernel k;
  kernel_reserve_vars(k, 7);
  const Lit one = internalize(1), two = internalize(2), three = internalize(3);
  kernel_add_binary_clause(k, two, internalize(6));
  kernel_add_binary_clause(k, three, internalize(7));
  OccIndex occ;
  kernel_dense_connect(k, occ);

  SweepSubstitutionSession session;
  session.begin(k);
  // x1 wins both merges by import order, so x3 and x2 both retire into it.
  // The batch proves x1 = x3 first, which is not import order.
  kernel_add_binary_clause(k, negate(one), three);
  kernel_add_binary_clause(k, one, negate(three));
  kernel_add_binary_clause(k, negate(one), two);
  kernel_add_binary_clause(k, one, negate(two));
  assert(substitute_apply_sweep_equivalences(k, {{one, three}, {one, two}},
                                             session) == ApplyResult::Ok);
  assert_ledger_mirrors_kernel(k, session);
  // Clause streams follow pair-emission order. The class remains active and
  // the equivalence bridge remains live until canonical Substitute performs
  // Kissat's first-import-order deactivation scan.
  assert(k.retired_events.empty());

  const auto &with_bridges = session.occurrences(k, one);
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

// A same-batch chain can collapse a clause only when its last equivalent
// literal retires.  Kissat appends that binary at the collapse-causing
// literal's position in the unified stream; assigning it to the first member
// exposes it before the intervening large occurrences and changes the next
// capped Sweep environment.
static void test_sweep_chained_collapse_uses_last_mixed_stream() {
  Kernel k;
  kernel_reserve_vars(k, 10);
  const Lit one = internalize(1), two = internalize(2),
            three = internalize(3), four = internalize(4);
  const Lit resident[3] = {one, internalize(9), internalize(10)};
  const Lit three_large[3] = {three, internalize(7), internalize(8)};
  const Lit four_large[3] = {four, internalize(6), internalize(8)};
  const Lit collapse[3] = {three, four, internalize(5)};
  kernel_add_large_clause(k, resident, 3, false, 0);
  kernel_add_large_clause(k, three_large, 3, false, 0);
  kernel_add_large_clause(k, four_large, 3, false, 0);
  kernel_add_large_clause(k, collapse, 3, false, 0);
  OccIndex occ;
  kernel_dense_connect(k, occ);

  SweepSubstitutionSession session;
  session.begin(k);
  const std::vector<std::pair<Lit, Lit>> equivalences = {
      {three, four}, {two, three}, {one, two}};
  for (const auto &[a, b] : equivalences) {
    kernel_add_binary_clause(k, negate(a), b);
    kernel_add_binary_clause(k, a, negate(b));
  }
  assert(substitute_apply_sweep_equivalences(k, equivalences, session) ==
         ApplyResult::Ok);
  assert_ledger_mirrors_kernel(k, session);

  const auto &ordered = session.occurrences(k, one);
  size_t collapse_at = ordered.size(), four_large_at = ordered.size();
  for (size_t i = 0; i < ordered.size(); i++) {
    const auto occurrence = ordered[i];
    if (occurrence.binary && occurrence.payload == internalize(5))
      collapse_at = i;
    if (!occurrence.binary) {
      const Clause *const clause = clause_at(k.arena, occurrence.payload);
      if (std::find(clause->begin(), clause->end(), internalize(6)) !=
          clause->end())
        four_large_at = i;
    }
  }
  assert(four_large_at < ordered.size());
  assert(collapse_at < ordered.size());
  assert(four_large_at < collapse_at);
  kernel_dense_disconnect(k);
}

// Sweep publishes both implication binaries before asking for the rewrite, so
// the ledger owes them a stream position whatever the classes turn out to be.
// Declining every class is a normal outcome — a unit emitted by the same batch
// can fix an endpoint — and returning before the sync left those binaries live
// in the watch lists but invisible to every later environment.
static void test_sweep_records_binaries_of_a_declined_batch() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit a = internalize(1), b = internalize(2);
  const Lit spectator[3] = {internalize(2), internalize(3), internalize(4)};
  kernel_add_large_clause(k, spectator, 3, false, 0);
  OccIndex occ;
  kernel_dense_connect(k, occ);
  SweepSubstitutionSession session;
  session.begin(k);

  // Sweep's own emission, then the endpoint is fixed by the same batch's unit.
  kernel_add_binary_clause(k, negate(a), b);
  kernel_add_binary_clause(k, a, negate(b));
  kernel_assign_unit(k, a);
  assert(propagate_apply(k) == ApplyResult::Ok);

  assert(substitute_apply_sweep_equivalences(k, {{a, b}}, session) ==
         ApplyResult::NoOp);
  assert_ledger_mirrors_kernel(k, session);
  kernel_dense_disconnect(k);
}

// A class member the rewrite declines — root-fixed or already retired — must be
// declined by the ledger too. Mirroring the requested class instead of the
// applied one moved ledger entries the watch lists never moved, and the next
// batch then reconciled a stream that no longer described the formula.
static void test_sweep_ledger_declines_the_same_members_as_the_rewrite() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit live_a = internalize(1), live_b = internalize(2);
  const Lit fixed_a = internalize(3), fixed_b = internalize(4);
  const Lit carrier[3] = {internalize(2), internalize(4), internalize(5)};
  kernel_add_large_clause(k, carrier, 3, false, 0);
  kernel_add_binary_clause(k, live_b, internalize(6));
  kernel_add_binary_clause(k, fixed_b, internalize(6));
  OccIndex occ;
  kernel_dense_connect(k, occ);
  SweepSubstitutionSession session;
  session.begin(k);

  // Sweep proves and publishes both pairs, then a unit of the same batch fixes
  // the second one's endpoints.
  kernel_add_binary_clause(k, negate(live_a), live_b);
  kernel_add_binary_clause(k, live_a, negate(live_b));
  kernel_add_binary_clause(k, negate(fixed_a), fixed_b);
  kernel_add_binary_clause(k, fixed_a, negate(fixed_b));
  kernel_assign_unit(k, fixed_a); // the second pair is no longer substitutable
  assert(propagate_apply(k) == ApplyResult::Ok);

  assert(substitute_apply_sweep_equivalences(
             k, {{live_a, live_b}, {fixed_a, fixed_b}}, session) ==
         ApplyResult::Ok);
  assert(!kernel_var_retired(k, var_of(live_b)));
  assert(!kernel_var_retired(k, var_of(fixed_b)));
  assert_ledger_mirrors_kernel(k, session);
  (void)session.collect(k);
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(kernel_var_retired(k, var_of(live_b)));
  kernel_dense_disconnect(k);
}

// Kissat's dense Sweep stream excludes redundant large clauses.  Search-time
// Sweep must therefore leave this clause alone; the mandated whole-formula
// Substitute owns its rewrite instead of Sweep manufacturing a connected
// binary early.
static void test_sweep_leaves_redundant_clause_outside_dense_transaction() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit redundant[3] = {internalize(2), internalize(3), internalize(4)};
  kernel_add_large_clause(k, redundant, 3, /*redundant=*/true, /*glue=*/2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  kernel_assign_unit(k, internalize(-3)); // the ternary collapses to a binary
  assert(propagate_apply(k) == ApplyResult::Ok);
  OccIndex occ;
  kernel_dense_connect(k, occ);
  SweepSubstitutionSession session;
  session.begin(k);
  k.stats.conflicts = 1;

  assert(substitute_apply_sweep_equivalences(
             k, {{internalize(1), internalize(2)}}, session) ==
         ApplyResult::NoOp);
  assert(!kernel_binary_live(k, internalize(1), internalize(4)));
  uint32_t live_redundant = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && c->redundant())
      live_redundant++;
    ref += clause_actual_words(c);
  }
  assert(live_redundant == 1);
  assert_ledger_mirrors_kernel(k, session);
  assert(!kernel_var_retired(k, var_of(internalize(2))));
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(kernel_var_retired(k, var_of(internalize(2))));
  assert(kernel_binary_live(k, internalize(1), internalize(4)));
  kernel_dense_disconnect(k);
}

// Only the connected irredundant clause is rewritten.  The redundant clause
// remains outside both the physical dense index and the session ledger.
static void test_sweep_rewrites_only_connected_irredundant_clause() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit redundant[4] = {internalize(2), internalize(3), internalize(4),
                            internalize(5)};
  const Lit irredundant[3] = {internalize(2), internalize(5), internalize(6)};
  kernel_add_large_clause(k, redundant, 4, /*redundant=*/true, /*glue=*/3);
  kernel_add_large_clause(k, irredundant, 3, /*redundant=*/false, /*glue=*/0);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  OccIndex occ;
  kernel_dense_connect(k, occ);
  SweepSubstitutionSession session;
  session.begin(k);
  k.stats.conflicts = 1;

  assert(substitute_apply_sweep_equivalences(
             k, {{internalize(1), internalize(2)}}, session) ==
         ApplyResult::Ok);
  // Only the irredundant replacement exists and is indexed.
  uint32_t replacements = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() &&
        std::find(c->begin(), c->end(), internalize(1)) != c->end())
      replacements++;
    ref += clause_actual_words(c);
  }
  assert(replacements == 1);
  assert(large_occurrences(occ, internalize(1)).size() == 1);
  assert(!clause_at(k.arena, large_occurrences(occ, internalize(1))[0])->redundant());
  assert_ledger_mirrors_kernel(k, session);
  (void)session.collect(k);
  assert(!kernel_var_retired(k, var_of(internalize(2))));
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(kernel_var_retired(k, var_of(internalize(2))));
  replacements = 0;
  bool redundant_image = false;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() &&
        std::find(c->begin(), c->end(), internalize(1)) != c->end()) {
      replacements++;
      redundant_image |= c->redundant();
    }
    ref += clause_actual_words(c);
  }
  assert(replacements == 2 && redundant_image);
  kernel_dense_disconnect(k);
}

// A redundant-only neighbourhood is a no-op for Sweep's connected
// large-clause half. The retained equivalence bridge makes the following
// canonical Substitute productive and gives it sole ownership of the residual
// redundant rewrite and variable retirement.
static void test_sweep_does_not_rewrite_redundant_only_neighbourhood() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit redundant[3] = {internalize(2), internalize(3), internalize(4)};
  kernel_add_large_clause(k, redundant, 3, true, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  OccIndex occ;
  kernel_dense_connect(k, occ);
  SweepSubstitutionSession session;
  session.begin(k);

  assert(substitute_apply_sweep_equivalences(
             k, {{internalize(1), internalize(2)}}, session) ==
         ApplyResult::NoOp);
  assert(!kernel_var_retired(k, var_of(internalize(2))));
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(kernel_var_retired(k, var_of(internalize(2))));
  bool rewritten = false;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && c->redundant())
      rewritten = std::find(c->begin(), c->end(), internalize(1)) != c->end() &&
                  std::find(c->begin(), c->end(), internalize(2)) == c->end();
    ref += clause_actual_words(c);
  }
  assert(rewritten);
  (void)session.collect(k);
}

static void test_canonical_substitution_preserves_large_clause_position() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit before[3] = {internalize(3), internalize(4), internalize(5)};
  const Lit touched[3] = {internalize(2), internalize(4), internalize(6)};
  const Lit after[3] = {internalize(-3), internalize(5), internalize(6)};
  kernel_add_large_clause(k, before, 3, false, 0);
  kernel_add_large_clause(k, touched, 3, false, 0);
  kernel_add_large_clause(k, after, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));

  assert(substitute_apply(k) == ApplyResult::Ok);
  std::vector<Lit> first_literals;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    assert(!c->garbage());
    first_literals.push_back(c->lits[0]);
    ref += clause_actual_words(c);
  }
  assert(first_literals ==
         std::vector<Lit>({internalize(3), internalize(1), internalize(-3)}));
}

// Kissat's ordinary `substitute_clauses` compacts a surviving large clause in
// its existing header. A root-false shortening must therefore retain the
// Vivify-leftover priority bit just as a representative rewrite does.
static void test_canonical_root_shrink_preserves_vivify_priority() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit lits[4] = {internalize(1), internalize(2), internalize(3),
                       internalize(4)};
  const Ref source =
      kernel_add_large_clause(k, lits, 4, /*redundant=*/true, /*glue=*/3);
  clause_at(k.arena, source)->set_vivify(true);
  kernel_assign_unit(k, internalize(-1));
  assert(propagate_apply(k) == ApplyResult::Ok);

  assert(substitute_apply(k) == ApplyResult::Ok);
  const Clause *rewritten = nullptr;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage()) {
      assert(!rewritten);
      rewritten = c;
    }
    ref += clause_actual_words(c);
  }
  assert(rewritten && rewritten->size == 3);
  assert(rewritten->vivify());
  assert(std::find(rewritten->begin(), rewritten->end(), internalize(1)) ==
         rewritten->end());
}

// Connected substitution must not rebuild an unrelated binary watch list.
// Besides avoiding a whole-formula snapshot on very large inputs, retaining
// this order is semantic: later bounded scans consume candidates in watch
// order, so re-appending untouched binaries can change the search trajectory.
static void test_substitution_preserves_unrelated_binary_watch_order() {
  Kernel k;
  kernel_reserve_vars(k, 7);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));

  const Lit pivot = internalize(3);
  kernel_add_binary_clause(k, pivot, internalize(6));
  kernel_add_binary_clause(k, pivot, internalize(-4));
  kernel_add_binary_clause(k, pivot, internalize(7));
  kernel_add_binary_clause(k, pivot, internalize(5));
  const std::vector<Lit> before = binary_watch_others(k, pivot);
  assert(before.size() == 4);

  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(binary_watch_others(k, pivot) == before);
}

// The DRAT/no-proof streaming path keeps clause multiplicity. Two connected
// sources that map to an already-present target must leave three target copies;
// deduplicating the internal image would change later watch-order scans.
static void test_substitution_preserves_binary_multiplicity() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  kernel_add_binary_clause(k, internalize(1), internalize(3));
  kernel_add_binary_clause(k, internalize(2), internalize(3));
  kernel_add_binary_clause(k, internalize(2), internalize(3));

  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(kernel_binary_copies(k, internalize(2), internalize(3)) == 0);
  assert(kernel_binary_copies(k, internalize(1), internalize(3)) == 3);
}

// DSR/DRAT deletions name a literal multiset, so identical physical clauses are
// interchangeable but their count must still match the kernel. The two SCC
// bridge lemmas are proof-only copies of the two source binaries: Substitute
// deletes both source copies during rewriting and must then delete both
// transient copies. A stale proof-only binary can invalidate a later direct-SR
// witness even though it could only help ordinary RUP.
static void test_non_lrat_retires_transient_equivalence_lemmas() {
  const std::string path = "/tmp/tactix-substitute-drat-" +
                           std::to_string(static_cast<long>(getpid())) +
                           ".proof";
  std::remove(path.c_str());
  for (const ProofFormat format : {ProofFormat::DratAscii,
                                   ProofFormat::DsrAscii}) {
    {
      Kernel k;
      kernel_reserve_vars(k, 2);
      kernel_add_binary_clause(k, internalize(-1), internalize(2));
      kernel_add_binary_clause(k, internalize(-2), internalize(1));
      Proof proof;
      assert(proof.open(path.c_str(), format));
      k.proof = &proof;
      assert(substitute_apply(k) == ApplyResult::Ok);
    }
    std::ifstream input(path);
    assert(input.good());
    uint32_t additions = 0, deletions = 0;
    for (std::string line; std::getline(input, line);)
      if (line.rfind("d ", 0) == 0)
        deletions++;
      else if (!line.empty())
        additions++;
    input.close();
    std::remove(path.c_str());
    assert(additions == 2);
    assert(deletions == 4);
  }
}

// A preserved bridge's transformed pair can coincide with an ordinary live
// binary (chained same-batch merges compose bridge endpoints onto arbitrary
// intermediate members). The proof pass rewrites or deletes that ordinary
// copy per image, so the physical pass must consume it too: protecting every
// pair-matching watch entry retains a live kernel clause whose checker copies
// are later exhausted, and the first lemma propagating through the survivor
// fails RUP (goldcrest-and-11, campaign 20260824-0337). Bridge entries are
// protected per directed entry, exactly one unit per bridge image.
static void test_sweep_bridge_target_pair_collision_consumes_ordinary_copy() {
  const std::string path = "/tmp/tactix-substitute-bridge-collision-" +
                           std::to_string(static_cast<long>(getpid())) +
                           ".proof";
  std::remove(path.c_str());
  const auto pair_lines = [&](int ea, int eb) {
    std::ifstream input(path);
    assert(input.good());
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
    OccIndex occ;
    kernel_dense_connect(k, occ);
    SweepSubstitutionSession session;
    session.begin(k);
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::DratAscii));
    k.proof = &proof;
    // Sweep publishes both implication binaries per proved pair. Merge order
    // (2,1), (3,4), (4,2) makes the (3,4) bridge's moved endpoint compose to
    // the intermediate member 2 — (¬3 ∨ 4) is preserved as (¬3 ∨ 2), the
    // ordinary pair above — while 2 itself retires to root 1.
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
               session) == ApplyResult::Ok);
    // The transformed bridge is the only surviving physical copy of (¬3, 2);
    // the colliding ordinary copy collapsed to a tautology and must be gone
    // from both directed watch lists.
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
    assert_ledger_mirrors_kernel(k, session);
    assert(!kernel_var_retired(k, var_of(internalize(2))));
    (void)session.collect(k);
    assert(substitute_apply(k) == ApplyResult::Ok);
    assert(kernel_var_retired(k, var_of(internalize(2))));
    assert(kernel_var_retired(k, var_of(internalize(3))));
    assert(kernel_var_retired(k, var_of(internalize(4))));
    assert(kernel_binary_copies(k, internalize(-3), internalize(2)) == 0);
    assert(kernel_binary_copies(k, internalize(2), internalize(-3)) == 0);
    k.proof = nullptr;
    kernel_dense_disconnect(k);
  }
  // DRAT population balance for the colliding pair: the kernel started with
  // one live copy the checker holds from the input, so deletions may exceed
  // proof additions by exactly that one copy — never more. An extra deletion
  // is the checker-divergence this regression pins.
  const auto [additions, deletions] = pair_lines(-3, 2);
  assert(deletions <= additions + 1);
  assert(additions + 1 - deletions ==
         0 /* no live copy of the pair survives above */);
  std::remove(path.c_str());
}

static void test_substitution_rewrites_stable_watch_in_place() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));

  const Lit stable = internalize(3);
  kernel_add_binary_clause(k, stable, internalize(4));
  kernel_add_binary_clause(k, stable, internalize(1));
  kernel_add_binary_clause(k, stable, internalize(2));
  kernel_add_binary_clause(k, stable, internalize(5));
  assert(substitute_apply(k) == ApplyResult::Ok);
  const std::vector<Lit> expected = {internalize(4), internalize(1),
                                     internalize(1), internalize(5)};
  assert(binary_watch_others(k, stable) == expected);
}

// Kissat moves a retired literal's binary watches to its representative in
// the retiring watch list's order.  The stable partners' literal order must
// not reorder that appended suffix: x6 lists x4 before x2 even though x2 is
// visited before x4 by an undirected lower-endpoint snapshot.
static void test_substitution_appends_moved_watches_in_source_order() {
  Kernel k;
  kernel_reserve_vars(k, 7);
  // x1 == x6, with x1 the import-order representative.
  kernel_add_binary_clause(k, internalize(-1), internalize(6));
  kernel_add_binary_clause(k, internalize(-6), internalize(1));

  const Lit representative = internalize(1);
  const Lit retired = internalize(6);
  kernel_add_binary_clause(k, representative, internalize(7));
  kernel_add_binary_clause(k, retired, internalize(4));
  kernel_add_binary_clause(k, retired, internalize(2));

  assert(substitute_apply(k) == ApplyResult::Ok);
  const std::vector<Lit> watches =
      binary_watch_others(k, representative);
  const auto four = std::find(watches.begin(), watches.end(), internalize(4));
  const auto two = std::find(watches.begin(), watches.end(), internalize(2));
  assert(four != watches.end());
  assert(two != watches.end());
  assert(four < two);
}

// When both endpoints retire, Kissat moves the two directed watches
// independently.  The x8 source list says x7 before x6, so representative x1
// must receive x2 before x3 even though the undirected clauses are first seen
// from x6 and x7 in the opposite order.
static void test_substitution_moves_both_directions_independently() {
  Kernel k;
  kernel_reserve_vars(k, 8);
  kernel_add_binary_clause(k, internalize(-1), internalize(8));
  kernel_add_binary_clause(k, internalize(-8), internalize(1));
  kernel_add_binary_clause(k, internalize(-2), internalize(7));
  kernel_add_binary_clause(k, internalize(-7), internalize(2));
  kernel_add_binary_clause(k, internalize(-3), internalize(6));
  kernel_add_binary_clause(k, internalize(-6), internalize(3));

  kernel_add_binary_clause(k, internalize(8), internalize(7));
  kernel_add_binary_clause(k, internalize(8), internalize(6));

  assert(substitute_apply(k) == ApplyResult::Ok);
  const std::vector<Lit> watches =
      binary_watch_others(k, internalize(1));
  const auto two = std::find(watches.begin(), watches.end(), internalize(2));
  const auto three =
      std::find(watches.begin(), watches.end(), internalize(3));
  assert(two != watches.end());
  assert(three != watches.end());
  assert(two < three);
}

// Model reconstruction through ρ: a model of the reduced formula with x2 at the
// wrong value must extend to satisfy every original clause.
static void test_model_reconstruction() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  const Lit c[2] = {internalize(2), internalize(3)}; // (x2 ∨ x3), a binary
  kernel_add_binary_clause(k, c[0], c[1]);
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
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  kernel_add_binary_clause(k, internalize(2), internalize(1));  // (x2 ∨ x1)
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(1)) > 0 && "x1 forced true by the collapsed unit");
  assert(k.propagate_head == k.trail.size() &&
         "binary-collapse units reach root closure before large rewrites");
}

// A failed SCC root can fix both a non-representative and its representative
// after Tarjan has frozen the map. Pinned Kissat skips proof-equivalence and
// variable-retirement work for that now-inactive member, but still applies the
// complete map in substitute_binaries/substitute_clauses. Conflating those two
// predicates leaves the equivalence pair and an unrewritten connected binary
// in the physical formula.
static void test_failed_root_member_still_rewrites_physical_binaries() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit rep = internalize(1), member = internalize(2);
  const Lit witness = internalize(3), spectator = internalize(4);
  kernel_add_binary_clause(k, negate(member), rep); // member -> rep
  kernel_add_binary_clause(k, negate(rep), member); // rep -> member
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
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  kernel_add_binary_clause(k, internalize(-3), internalize(4)); // x3 -> x4
  kernel_add_binary_clause(k, internalize(-4), internalize(3)); // x4 -> x3
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

// N1: one instance still surfaces the global x ≡ ¬x refutation.
static void test_instance_surfaces_unsat() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(-1));
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  const int rid = tactix_rule_id("Substitute");
  const RuleVTable &vt = tactix_rules()[rid];
  uint64_t insts[8];
  const uint32_t n = vt.instances(k, insts, 8);
  assert(n >= 1);
  assert(vt.apply_instance(k, insts[0]) == ApplyResult::Unsat);
  assert(k.inconsistent);
}

// A round deriving two contradictory root units must refute, not drop one: here
// {1} comes from the rewritten large clause and {¬1} from the binary (¬1∨¬2),
// and neither fires by propagation alone.
static void test_contradictory_units_refute() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1)); // 2 -> 1  (1≡2)
  const Lit c[3] = {internalize(1), internalize(2), internalize(-3)};
  kernel_add_large_clause(k, c, 3, /*redundant=*/false, /*glue=*/0);
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
    const Ref ref = kernel_add_large_clause(k, input.data(), input.size(), false, 0);
    OccIndex occ;
    kernel_dense_connect(k, occ);
    SweepSubstitutionSession session;
    session.begin(k);
    std::vector<std::pair<Lit, Lit>> merges;
    for (const auto &[a, b] : test.merges) {
      const Lit x = internalize(a), y = internalize(b);
      kernel_add_binary_clause(k, negate(x), y);
      kernel_add_binary_clause(k, x, negate(y));
      merges.emplace_back(x, y);
    }
    assert(substitute_apply_sweep_equivalences(k, merges, session) == ApplyResult::Ok);
    const Clause *clause = clause_at(k.arena, ref);
    assert(!clause->garbage());
    assert(std::vector<Lit>(clause->begin(), clause->end()) == expected);
    assert_ledger_mirrors_kernel(k, session);
    kernel_dense_disconnect(k);
  }
}

// A free N1 schedule can keep Eliminate's mixed dense stream attached across a
// canonical Substitute (random_policy.cpp attaches one for the whole walk).
// The in-place stable-watch rewrite and the directed mirror moves bypass the
// kernel add/delete primitives, so the rule republishes the image at its
// boundary; the dense oracle in kernel_check_invariants is the acceptance test
// and the retired literals must hold neither physical nor image entries.
static void test_canonical_substitution_republishes_attached_mixed_stream() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  kernel_add_binary_clause(k, internalize(2), internalize(3));
  kernel_add_binary_clause(k, internalize(-2), internalize(4));
  const Lit c[3] = {internalize(2), internalize(3), internalize(5)};
  const Lit d[3] = {internalize(-2), internalize(4), internalize(5)};
  kernel_add_large_clause(k, c, 3, /*redundant=*/false, /*glue=*/0);
  kernel_add_large_clause(k, d, 3, /*redundant=*/false, /*glue=*/0);
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
  test_big_path_chain_crosses_root_true_intermediate();
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
  test_apply_unsat();
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
  test_non_lrat_retires_transient_equivalence_lemmas();
  test_sweep_bridge_target_pair_collision_consumes_ordinary_copy();
  test_substitution_rewrites_stable_watch_in_place();
  test_substitution_appends_moved_watches_in_source_order();
  test_substitution_moves_both_directions_independently();
  test_model_reconstruction();
  test_size_collapse_unit();
  test_failed_root_member_still_rewrites_physical_binaries();
  test_apply_instance_one_class();
  test_instance_surfaces_unsat();
  test_guard();
  puts("test_substitute OK");
  return 0;
}
