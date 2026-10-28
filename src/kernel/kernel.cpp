#include "kernel/kernel.hpp"

#include "diagnostics/rule_journal.hpp"
#include "diagnostics/event_trace.hpp"

#include "policies/heuristics.hpp" // the opaque `H`: Kernel's ctor/dtor + calls
#include "proof/proof.hpp" // kernel_delete_binary emits the delta atomically

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdio>
#include <numeric>
#include <type_traits>
#include <utility>

namespace tactix {

void kernel_update_target_phases(Kernel &k) {
  if (k.h)
    heuristics_update_target_phases(k);
}

namespace {
template<class Assignment, class Polarity>
void publish_local_search_hint(
    Kernel &k, strategy::LocalSearchKind kind,
    const Assignment &assignment, Polarity polarity, uint64_t objective,
    bool objective_exact, strategy::LocalSearchEffort effort) {
  if (k.local_search_publication == strategy::LOCAL_SEARCH_GENERATION_MAX)
    return;
  auto &hint = k.local_search_hints[static_cast<size_t>(kind)];
  // Each producer owns its within-search best. Scores from different calls
  // need not describe the same decoded objective, even on an unchanged CNF.
  // Publish the new candidate without making an ungrounded cross-call ranking.
  if (hint.available())
    ++k.stats.local_search_hints_replaced;
  hint.external_phases.assign(k.external_to_internal.size(), 0);
  for (Var internal = 0; internal < k.nvars; ++internal) {
    const Var external = k.internal_to_external[internal];
    if (external >= hint.external_phases.size())
      hint.external_phases.resize(static_cast<size_t>(external) + 1, 0);
    hint.external_phases[external] = polarity(assignment[internal]);
  }
  hint.known_phases = static_cast<uint64_t>(std::count_if(
      hint.external_phases.begin(), hint.external_phases.end(),
      [](int8_t phase) { return phase != 0; }));
  hint.objective = objective;
  hint.objective_exact = objective_exact;
  ++k.stats.local_search_hints_offered;
  k.stats.local_search_hint_phases_offered += hint.known_phases;
  hint.id = (++k.local_search_publication << 8) |
            static_cast<uint64_t>(kind);
  hint.formula_touch_epoch = k.touch_epoch;
  hint.formula_root_assignments = k.stats.root_assignments;
  hint.external_variables = k.external_to_internal.size();
  hint.effort = effort;
}
} // namespace

void kernel_offer_local_search_hint(
    Kernel &k, strategy::LocalSearchKind kind,
    const std::vector<uint8_t> &assignment, uint64_t objective,
    bool objective_exact, strategy::LocalSearchEffort effort) {
  if (assignment.empty() || assignment.size() != k.nvars ||
      !strategy::valid_local_search_kind(kind) ||
      std::any_of(assignment.begin(), assignment.end(),
                  [](uint8_t value) { return value > 1; }))
    return;
  publish_local_search_hint(k, kind, assignment,
      [](uint8_t value) { return int8_t(value ? 1 : -1); },
      objective, objective_exact, effort);
}

void kernel_offer_local_search_partial_hint(
    Kernel &k, strategy::LocalSearchKind kind,
    const std::vector<int8_t> &assignment, uint64_t objective,
    bool objective_exact, strategy::LocalSearchEffort effort) {
  if (assignment.size() != k.nvars ||
      !strategy::valid_local_search_kind(kind) ||
      std::any_of(assignment.begin(), assignment.end(),
                  [](int8_t value) { return value < -1 || value > 1; }) ||
      std::none_of(assignment.begin(), assignment.end(),
                   [](int8_t value) { return value != 0; }))
    return;
  publish_local_search_hint(k, kind, assignment,
      [](int8_t value) { return value; }, objective, objective_exact, effort);
}

const strategy::LocalSearchHint *kernel_local_search_hint(const Kernel &k,
                                                         uint64_t candidate) {
  if (!strategy::valid_local_search_candidate(candidate))
    return nullptr;
  const auto &hint = k.local_search_hints[static_cast<size_t>(
      strategy::local_search_candidate_kind(candidate))];
  return hint.available() && hint.id == candidate ? &hint : nullptr;
}

// The kernel does NOT own `H`: it is a non-owning `Heuristics *`
// (kernel.hpp), attached by whoever owns the storage (the session / a policy).
// So Kernel needs no ctor/dtor for it. heuristics.hpp is still included above —
// kernel_backtrack_impl<true> reads `k.h->…`, which needs the complete type.

void kernel_defer_cold_state(Kernel &k) {
  assert(k.nvars == 0);
  assert(k.bve_retry.empty() && k.backbone_priority.empty());
  assert(k.touched.empty() && k.removed.empty() && k.touched_kept.empty());
  assert(k.subsume_material.empty() && !k.subsume_material_count);
  assert(k.seen.empty() && k.eager_marks.empty());
  assert(k.marked_levels.empty() && k.probing.stamp.empty());
  k.cold_state_deferred = true;
}

void kernel_materialize_cold_state(Kernel &k) {
  if (!k.cold_state_deferred)
    return;
  // No consumer watermark can advance before startup preprocessing. Every
  // variable has at least its activation touch, so one common current stamp is
  // predicate-equivalent to the distinct historical stamps for the first
  // round; later events and watermarks are recorded normally. Parsing and
  // early Lucky do not remove clauses or create kept-band touches.
  assert(!k.subsume_touch_mark && !k.strengthen_touch_mark &&
         !k.bce_touch_mark && !k.ternary_touch_mark &&
         !k.instantiate_touch_mark && !k.cce_touch_mark);
  k.bve_retry.assign(k.nvars, 1);
  k.backbone_priority.assign(k.nvars, 0);
  k.touched.assign(k.nvars, k.touch_epoch);
  k.subsume_material.assign(k.nvars, 1);
  k.removed.assign(k.nvars, 0);
  k.touched_kept.assign(k.nvars, 0);
  k.seen.assign(k.nvars, 0);
  k.eager_marks.assign(2 * static_cast<size_t>(k.nvars), 0);
  k.marked_levels.assign(static_cast<size_t>(k.nvars) + 1, 0);
  k.probing.stamp.assign(2 * static_cast<size_t>(k.nvars), 0);
  k.cold_state_deferred = false;
}

void kernel_reserve_vars(Kernel &k, uint32_t nvars) {
  if (nvars <= k.nvars)
    return;
  const uint32_t old = k.nvars;
  kernel_affine_note_namespace_change(k);
  k.nvars = nvars;
  // Parsing normally grows once to the declared header. Variable-adding rules
  // instead append one extension at a time; exact reserve(nvars) on every call
  // copies the whole identity/trail backing store quadratically. Preserve the
  // fixed-capacity hot-path invariant with amortized geometric growth.
  const auto reserve_amortized = [](auto &values, size_t need) {
    if (values.capacity() >= need)
      return;
    const size_t current = values.capacity();
    const size_t grown = current + std::max<size_t>(current / 2, 1);
    values.reserve(std::max(need, grown));
  };
  reserve_amortized(k.internal_to_external, nvars);
  for (Var v = old; v < nvars; v++) {
    const Var external = static_cast<Var>(k.external_to_internal.size());
    k.internal_to_external.push_back(external);
    k.external_to_internal.push_back(v);
  }
  k.external_import_rank.resize(k.external_to_internal.size(), UINT32_MAX);
  k.internal_import_rank.resize(nvars, UINT32_MAX);
  // --- σ-state + derived-index growth (kernel-owned) ---
  k.values.resize(2 * nvars, 0);
  k.assigned.resize(nvars);
  // Only meaningful under `assigned[v].jumped`, so the initial value is never
  // read; grown unconditionally to keep the assign hot path branch-free.
  k.jumped_over.resize(nvars, 0);
  k.watches.resize_lits(2 * nvars);
  k.unit_ids.resize(nvars, 0);
  if (!k.dsr_root_values.empty())
    k.dsr_root_values.resize(nvars, 0);
  // BVE retry filter: a brand-new variable has never been attempted, so it
  // starts retry-worthy (kissat activate_variable marks it removed+added). A
  // derived index, not `H` (docs/dev/implementation-notes.md §6) — kernel-side.
  if (!k.cold_state_deferred)
    k.bve_retry.resize(nvars, 1);
  k.factor_marks.resize(2 * nvars, 0);
  if (!k.cold_state_deferred)
    k.backbone_priority.resize(nvars, 0);
  // Touched-epoch substrate: a brand-new variable has never been looked at by
  // any consumer, so it must read as touched against every existing watermark
  // — hence a real touch EVENT each, not a resize fill (kissat
  // activate_variable calls kissat_mark_added_literal for the same reason).
  if (!k.cold_state_deferred) {
    k.touched.resize(nvars, 0);
    k.removed.resize(nvars, 0);
    k.subsume_material.resize(nvars, 0);
  } else {
    // No consumer can clear flags before cold-state materialization. Parsing
    // additions therefore cannot flip any of these new activation flags again.
    assert(UINT64_MAX - k.subsume_material_count >= nvars - old);
    k.subsume_material_count += nvars - old;
  }
  // The band array needs no such event: a fresh variable is already touched
  // below, and C2 reads the two arrays as a union.
  if (!k.cold_state_deferred)
    k.touched_kept.resize(nvars, 0);
  for (uint32_t v = old; v < nvars; v++) {
    // Kissat activates a variable by setting both its eliminate and subsume
    // flags.  Clause addition below only owns the latter; this is the unique
    // initial BVE-retry event for a newly imported/extended variable.
    kernel_mark_bve_retry(k, v);
    kernel_touch_var(k, v);
    kernel_mark_subsume_material(k, v);
    // Variable activation marks the stripped (positive) literal in Kissat;
    // actual clause polarities are marked below by clause addition.
    kernel_mark_factor_literal(k, make_lit(v, false), "activate");
  }
  // Retirement status: a brand-new variable is Active (design R-a). Only grown
  // once something has retired a variable, so the common case stays empty.
  if (!k.var_status.empty())
    k.var_status.resize(nvars, VarStatus::Active);
  if (!k.cold_state_deferred) {
    k.seen.resize(nvars, 0);
    k.eager_marks.resize(2 * nvars, 0); // all-zero between uses; see the field
    k.marked_levels.resize(nvars + 1, 0);
    k.probing.stamp.resize(2 * nvars, 0);
  }
  // The dense occurrence index is per-literal kernel state (null outside a
  // simplification phase). A var-adding rule (E2 Factor / BVA) can grow nvars
  // WHILE dense is connected — the shared index is built once per phase and
  // rules run under it (§5.3). Grow it here so no later consumer indexes a new
  // literal out of bounds. New (extension) literals occur in no large clause
  // yet, so empty entries are correct; add() maintains them from there.
  if (k.dense) {
    if (k.dense->track_order)
      k.dense->ordered.resize_lits(2 * nvars);
    else
      k.dense->large.resize_lits(2 * nvars);
  }
  k.unassigned += nvars - old;
  k.unassigned_active += nvars - old; // fresh variables are Active (R-a)
  // fixed-capacity trail/frames: the inlined assign hot path must never
  // reallocate, and its push_back capacity branch becomes always-false
  reserve_amortized(k.trail, nvars);
  reserve_amortized(k.frames, static_cast<size_t>(nvars) + 1);
  // --- search-guidance (`H`) growth: the extracted seam ---
  // Conditional on `H` being present (H5): an unguided solver (`k.h == nullptr`)
  // has no per-var `H` state to grow. DefaultPolicy always has `H` → unchanged.
  if (k.h)
    heuristics_reserve_vars(k, old, nvars);
}

void kernel_record_import(Kernel &k, Var v) {
  assert(v < k.nvars);
  assert(k.internal_import_rank.size() == k.nvars);
  // Already ranked: the internal and external ranks agree by invariant
  // (kernel_check_invariants), so there is nothing to record.
  if (k.internal_import_rank[v] != UINT32_MAX)
    return;
  const Var external = k.internal_to_external[v];
  if (k.external_import_rank.size() <= external)
    k.external_import_rank.resize(external + 1, UINT32_MAX);
  uint32_t &rank = k.external_import_rank[external];
  if (rank == UINT32_MAX) {
    rank = k.next_import_rank++;
    assert(rank == k.import_rank_to_external.size());
    k.import_rank_to_external.push_back(external);
  }
  assert(k.internal_import_rank.size() == k.nvars);
  k.internal_import_rank[v] = rank;
}

// kernel_import_var_order and kernel_import_lit_order are defined inline in
// kernel.hpp (hot cross-TU comparator calls; report 3.85).

uint64_t kernel_compact_import_lit_order(const Kernel &k, Lit lit) {
  const Var v = var_of(lit);
  if (v < k.compact_import_position.size()) {
    const Var position = k.compact_import_position[v];
    if (position != INVALID_VAR && position < k.compact_import_order.size() &&
        k.compact_import_order[position] == v)
      return 2ull * position + negated(lit);
    // Diagnostic and external-policy clients may replace the order directly;
    // keep that representation observable even before they rebuild the inverse.
    const auto it = std::find(k.compact_import_order.begin(),
                              k.compact_import_order.end(), v);
    if (it != k.compact_import_order.end())
      return 2ull * static_cast<uint64_t>(it - k.compact_import_order.begin()) +
             negated(lit);
  }
  return kernel_import_lit_order(k, lit);
}

void kernel_vars_in_import_order(const Kernel &k, std::vector<Var> &order) {
  order.clear();
  order.reserve(k.nvars);
  assert(k.next_import_rank == k.import_rank_to_external.size());
  // Resolve the permanent first-occurrence permutation through the current
  // external-to-compact map.  Compaction retires identities but never changes
  // this order, so skipping its holes is equivalent to sorting live variables
  // by `kernel_import_var_order` without rebuilding the same permutation.
  for (const Var external : k.import_rank_to_external) {
    assert(external < k.external_import_rank.size());
    assert(k.external_import_rank[external] < k.next_import_rank);
    if (external >= k.external_to_internal.size())
      continue;
    const Var internal = k.external_to_internal[external];
    if (internal != INVALID_VAR)
      order.push_back(internal);
  }
  // Direct kernel clients need no input finalization.  Their unrecorded live
  // variables follow recorded imports in stable external order, exactly as in
  // `kernel_import_var_order`.
  for (Var external = 0; external < k.external_to_internal.size(); external++) {
    const Var internal = k.external_to_internal[external];
    if (internal == INVALID_VAR)
      continue;
    if (external >= k.external_import_rank.size() ||
        k.external_import_rank[external] == UINT32_MAX)
      order.push_back(internal);
  }
  assert(order.size() == k.nvars);
}

void kernel_vars_in_external_order(const Kernel &k, std::vector<Var> &order) {
  order.clear();
  order.reserve(k.nvars);
  // Kissat's `solver->import` is indexed by external variable identity, not
  // by the order in which tumble assigned internal identities.  Retired
  // external variables remain as holes after compaction and are skipped.
  for (const Var internal : k.external_to_internal)
    if (internal != INVALID_VAR)
      order.push_back(internal);
  assert(order.size() == k.nvars);
}

bool kernel_reserve_proof_variables(Kernel &k, uint32_t count, Var &first) {
  const uint64_t begin = k.external_to_internal.size();
  if (begin + count > static_cast<uint64_t>(INT32_MAX))
    return false;
  first = static_cast<Var>(begin);
  if (!count)
    return true;
  // Freeze the original-model boundary before adding namespace-only holes.
  if (k.original_external_vars == UINT32_MAX)
    k.original_external_vars = static_cast<uint32_t>(begin);
  kernel_affine_note_namespace_change(k);
  k.external_to_internal.resize(static_cast<size_t>(begin + count), INVALID_VAR);
  k.external_import_rank.resize(k.external_to_internal.size(), UINT32_MAX);
  return true;
}

Var kernel_new_extension_vars(Kernel &k, uint32_t count) {
  const Var first = k.nvars;
  if (k.first_extension_var == UINT32_MAX) {
    k.first_extension_var = first; // boundary = first non-original var
    if (k.original_external_vars == UINT32_MAX)
      k.original_external_vars =
          static_cast<uint32_t>(k.external_to_internal.size());
  }
  kernel_reserve_vars(k, k.nvars + count); // additive: grows all per-var state
  // A first late H attachment reconstructs extensions from their stable
  // external identities. After H has already attached once, however, its
  // activation seed is consumed and the kernel retains the compact population
  // image through detachment. Keep that image current while H is absent so a
  // later attachment neither loses these new decision variables nor needs to
  // reintroduce retired input identities to find them.
  if (!k.h && !k.compact_import_order.empty()) {
    k.compact_import_position.resize(k.nvars, INVALID_VAR);
    for (Var v = first; v < first + count; ++v) {
      assert(k.compact_import_position[v] == INVALID_VAR);
      k.compact_import_position[v] =
          static_cast<Var>(k.compact_import_order.size());
      k.compact_import_order.push_back(v);
    }
  }
  for (Var v = first; v < first + count; v++)
    kernel_record_import(k, v);
  return first;
}

// Propagation is trail-driven: it only ever visits a clause through the watch
// list of a literal it is currently scanning off the trail. A clause added over
// a literal that is ALREADY false and already scanned is therefore never
// looked at again, and if it is unit the next drain still reports a fixpoint —
// the corpus-004 stale-scan family, reached here by V5 Sweep, whose emission
// keeps units unpropagated while its RUP replay fixes literals underneath the
// pair it is about to add. Rewinding to the earliest such literal re-arms the
// scan; it is a no-op unless a clause really was added behind the cursor.
static void kernel_rearm_propagation(Kernel &k, const Lit *lits,
                                     uint32_t size) {
  // Root additions only, and only outside a recorded conflict. ConflictAnalysis
  // is the one caller that adds above the root, and it adds a clause whose
  // literals are ALL false by construction — re-arming there would rewind the
  // cursor over the whole trail on every conflict, where the Backjump that
  // must follow already resets it to the level boundary and rescans.
  if (k.level != 0 || k.conflict_valid)
    return;
  for (uint32_t i = 0; i < size; i++) {
    const Lit l = lits[i];
    if (k.values[l] >= 0)
      continue;
    const uint32_t pos = k.assigned[var_of(l)].trail;
    if (pos < k.propagate_head) {
      k.propagate_head = pos;
      k.stats.propagation_rearms++;
    }
  }
}

static void kernel_rearm_propagation(Kernel &k, Lit a, Lit b) {
  const Lit pair[2] = {a, b};
  kernel_rearm_propagation(k, pair, 2);
}

void kernel_rearm_binary_clause(Kernel &k, Lit a, Lit b) {
  kernel_rearm_propagation(k, a, b);
}

void kernel_rearm_theory_clause(Kernel &k, const Lit *lits, uint32_t size) {
  assert(k.level > 0 && !k.conflict_valid && size >= 2);
  uint32_t earliest = k.propagate_head;
  [[maybe_unused]] bool found = false;
  for (uint32_t i = 0; i < size; ++i) {
    const Lit literal = lits[i];
    if (k.values[literal] >= 0) continue;
    const uint32_t position = k.assigned[var_of(literal)].trail;
    earliest = std::min(earliest, position);
    found = true;
  }
  assert(found);
  if (earliest < k.propagate_head) {
    k.propagate_head = earliest;
    k.stats.propagation_rearms++;
  }
}

Ref kernel_add_large_clause(Kernel &k, const Lit *lits, uint32_t size,
                            bool redundant, uint32_t glue, uint64_t id,
                            bool mark_added_material) {
  assert(size >= 3);
  kernel_rearm_propagation(k, lits, size);
  const Ref ref = static_cast<Ref>(k.arena.size());
  k.arena.resize(k.arena.size() + clause_words(size));
  Clause *c = clause_at(k.arena, ref);
  c->size = size;
  // redundant clauses start fully "used" (kissat learn.c: used = MAX_USED)
  c->flags = redundant ? (Clause::REDUNDANT | Clause::USED_MASK) : 0;
  if (!redundant)
    k.live_irredundant_large++;
  // Kissat's clause header assigns glue only to redundant clauses.  Keep that
  // representation invariant at the arena boundary: proof rules such as BVE
  // may price an irredundant resolvent by its size, but that price is not LBD
  // metadata and must never leak into later conflict/reason handling.
  c->set_glue(redundant ? glue : 0);
  c->searched = 2;
  c->set_id(id);
  if (redundant) {
    const uint64_t position = kernel_clause_position(k, ref);
    assert(position <= UINT32_MAX);
    k.first_reducible = std::min(k.first_reducible, static_cast<Ref>(position));
  }
  for (uint32_t i = 0; i < size; i++)
    c->lits[i] = lits[i];
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  rule_journal_note_large_add(k, lits, size, id, redundant, glue);
#endif
  // Physical dense mode publishes occurrences below, not sparse watches.
  // Kissat's new_large_clause selects those representations by `watching`;
  // in-place replacement already obeys this same connection flag.
  // Reconnection publishes the final live clauses once, in their owned order.
  if (k.large_watches_connected) {
    k.watches.push_large(lits[0], lits[1], ref);
    k.watches.push_large(lits[1], lits[0], ref);
  }
  // Irredundant clauses feed the dense index and the added-material consumers
  // (forward subsumption and Factor).  Pinned Kissat's `mark_added_literal`
  // does NOT set `flags.eliminate`: BVE retry is armed by variable activation
  // and semantic removal only, never by clause creation.
  if (!redundant)
    for (uint32_t i = 0; i < size; i++) {
      if (mark_added_material) {
        kernel_touch_var(k, var_of(lits[i]));
        kernel_mark_subsume_material(k, var_of(lits[i]));
        kernel_mark_factor_literal(k, lits[i], "add_large");
      }
      if (k.dense) {
        kernel_dense_add_large_occurrence(k, lits[i], ref);
        kernel_bve_occurrence_add(k, lits[i]);
      }
      // Kissat updates an already-scheduled variable after adding a
      // resolvent, but does not reinsert a candidate popped earlier.
      kernel_notify_bve_score(k, var_of(lits[i]), false);
    }
  // The redundant half is an explicit CaDiCaL diagnostic arm.  Keep both its
  // telemetry and stamps out of Kissat-default learned-clause creation.
  if (redundant && k.touch_redundant) {
    const bool dyn = kernel_likely_kept(k, true, glue, size);
    k.stats.touch_redundant_dynamic += dyn;
    k.stats.touch_redundant_static6 += (glue <= 6); // reduce.hpp TIER2_STATIC
    if (dyn)
      for (uint32_t i = 0; i < size; i++)
        kernel_touch_kept_var(k, var_of(lits[i]));
  }
  kernel_observe_solver_clause(k, SolverPopulationEventKind::Add,
                               SolverClauseKind::Large, id, c->lits, c->size,
                               1, redundant);
  return ref;
}

uint64_t kernel_clause_position(const Kernel &k, Ref ref) {
  if (k.clause_position_alias.empty())
    return ref; // no in-place rewrite since the last collection
  const Clause *c = clause_at(k.arena, ref);
  const uint64_t alias = c->id() ? k.clause_position_alias.get(c->id()) : 0;
  return alias ? alias - 1 : ref;
}

void kernel_inherit_clause_position(Kernel &k, uint64_t replacement_id,
                                    Ref source_ref) {
  // Id 0 is "no proof identity", the documented default of the clause-add API
  // (kernel.hpp), so there is no key under which to retain the position.
  if (!replacement_id)
    return;
  const uint64_t position = kernel_clause_position(k, source_ref);
  assert(position < UINT64_MAX);
  k.clause_position_alias.set(replacement_id, position + 1);
}

static void note_binary_add(Kernel &k, Lit a, Lit b, uint64_t id,
                            bool new_pair, bool mark_added_material) {
  // LAST id wins. The map must always name a clause the checker still holds,
  // and kernel_delete_binary can retire this pair's id, so a re-add must
  // install its fresh one — otherwise every later LRAT chain through the pair
  // cites a deleted clause. Among live duplicates either id is equally valid.
  const uint64_t key = kernel_binary_key(a, b);
  k.binary_ids.insert_or_assign_known(key, id, new_pair);
  if (k.binary_liveness)
    k.binary_liveness->add(key);
  // Like large-clause creation, a new binary is added material for forward
  // subsumption and Factor, but not for Kissat's elimination flag.
  if (mark_added_material) {
    kernel_notify_bve_score(k, var_of(a), false);
    kernel_notify_bve_score(k, var_of(b), false);
    // Touched-epoch substrate: a binary is the strongest subsumer/partner
    // there is, so both variables are new material for every consumer
    // (kissat new_binary_clause marks both unconditionally, clause.c:71-72).
    kernel_touch_var(k, var_of(a));
    kernel_touch_var(k, var_of(b));
    kernel_mark_subsume_material(k, var_of(a));
    kernel_mark_subsume_material(k, var_of(b));
    kernel_mark_factor_literal(k, a, "add_binary");
    kernel_mark_factor_literal(k, b, "add_binary");
  }
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  rule_journal_note_binary_add(k, a, b, id);
#endif
  event_trace_binary_lifecycle(k, new_pair ? "add-new" : "add-copy", a, b,
                               id, kernel_binary_copies(k, a, b));
}

static void add_binary_clause(Kernel &k, Lit a, Lit b, uint64_t id,
                              bool new_pair, bool mark_added_material) {
  event_trace_binary_add(k, a, b);
  kernel_rearm_binary_clause(k, a, b);
  k.watches.push_binary(a, b);
  k.watches.push_binary(b, a);
  if (k.dense && k.dense->track_order) {
    k.dense->ordered.push(a, OccIndex::Entry::from_binary(b));
    k.dense->ordered.push(b, OccIndex::Entry::from_binary(a));
  }
  kernel_bve_occurrence_add(k, a);
  kernel_bve_occurrence_add(k, b);
  note_binary_add(k, a, b, id, new_pair, mark_added_material);
  const Lit clause[2] = {a, b};
  kernel_observe_solver_clause(k, SolverPopulationEventKind::Add,
                               SolverClauseKind::Binary, id, clause, 2);
}

void kernel_add_binary_clause(Kernel &k, Lit a, Lit b, uint64_t id,
                              bool mark_added_material) {
  add_binary_clause(k, a, b, id, !kernel_binary_live(k, a, b),
                    mark_added_material);
}

void kernel_add_new_binary_clause(Kernel &k, Lit a, Lit b, uint64_t id) {
  assert(!kernel_binary_live(k, a, b));
  add_binary_clause(k, a, b, id, true, true);
}

void kernel_add_binary_mirror(Kernel &k, Lit stable, Lit moved, uint64_t id) {
  // The stable list already contains (stable -> moved), rewritten in place by
  // the caller. Adding the opposite direction restores the mirrored-watch
  // invariant without appending and immediately discarding a stable duplicate.
  assert(!k.bve_occurrence_count_image);
  kernel_rearm_binary_clause(k, stable, moved);
  k.watches.push_binary(moved, stable);
  // This is the missing physical direction of an already-published clause,
  // not new semantic material.  Kissat's in-place/delayed substitution moves
  // the watch without calling mark_added_literal.
  note_binary_add(k, stable, moved, id, false, false);
}

void kernel_publish_binary_clause(Kernel &k, Lit a, Lit b, uint64_t id,
                                  bool new_pair,
                                  BinaryPopulationEffect population_effect) {
  assert(!k.bve_occurrence_count_image);
  kernel_rearm_binary_clause(k, a, b);
  // Binary substitution has already published the logical/proof image.  This
  // call only publishes the rebuilt physical watch pair and its identity.
  // Pinned Kissat's substitute_binaries and Sweep connected substitution move
  // those watches in place/delayed without mark_added_literal, even when the
  // representative pair did not previously exist.
  note_binary_add(k, a, b, id, new_pair, false);
  if (population_effect == BinaryPopulationEffect::LogicalAdd) {
    const Lit clause[2] = {a, b};
    kernel_observe_solver_clause(k, SolverPopulationEventKind::Add,
                                 SolverClauseKind::Binary, id, clause, 2);
  }
}

// Does the live binary pair (x, other) ANTECEDE x's current assignment? Under
// reason jumping the recorded reason is a virtual resolvent, so the real first
// antecedent is (x ∨ jumped_over[x]) rather than (x ∨ reason); the second
// antecedent, ¬jumped_over[x]'s own reason, is a pair whose own endpoint
// answers this same question, so an endpoint-local test still covers the whole
// chain.
static bool binary_antecedes(const Kernel &k, Lit x, Lit other) {
  const Assigned &ax = k.assigned[var_of(x)];
  if (!ax.binary)
    return false;
  return ax.jumped ? k.jumped_over[var_of(x)] == other : ax.reason == other;
}

bool kernel_delete_binary(Kernel &k, Lit a, Lit b) {
  // (1) liveness probe (kernel_binary_copies, cap 2): an absent binary is a
  // no-op, and a DUPLICATED pair cannot be deleted by name — see that
  // predicate's contract in kernel.hpp for why either answer refuses.
  if (kernel_binary_copies(k, a, b, 2) != 1)
    return false; // absent (idempotent) or duplicated (unnameable)
  // (2) retire a live ROOT reason BEFORE deleting the clause that names it, so
  // no trail reason ever cites a just-deleted binary (the corpus-003 analog for
  // binary reasons; GC cannot do this — its reason-fix loop skips binaries).
  for (const Lit x : {a, b}) {
    Assigned &ax = k.assigned[var_of(x)];
    const Lit other = (x == a) ? b : a;
    if (k.values[x] > 0 && binary_antecedes(k, x, other)) {
      if (ax.level != 0)
        return false;          // live non-root reason: refuse (never at a root)
      kernel_materialize_dsr_root_unit(k, x);
      ax.reason = UNIT_REASON;
    }
  }
  // (3) remove the mirror watch entries by in-place compaction (write cursor),
  // preserving large watches and the binary mirror-symmetry invariant.
  for (const Lit x : {a, b}) {
    const Lit other = (x == a) ? b : a;
    bool removed = false;
    k.watches.compact(x, [&](WatchEntry w) {
      if (removed || !w.binary() || w.other() != other)
        return true;
      removed = true; // drop exactly this slot
      return false;
    });
    assert(removed);
    if (k.dense && k.dense->track_order) {
      const auto ordered = k.dense->ordered[x];
      const auto at = std::find_if(ordered.begin(), ordered.end(),
                                   [other](const OccIndex::Entry &entry) {
                                     return entry.binary &&
                                            entry.payload == other;
                                   });
      assert(at != ordered.end());
      k.dense->ordered.erase_at(x, static_cast<size_t>(at - ordered.begin()));
    }
  }
  kernel_bve_occurrence_remove(k, a);
  kernel_bve_occurrence_remove(k, b);
  if (k.binary_liveness)
    k.binary_liveness->remove(kernel_binary_key(a, b));
  // (4) proof delete (chain-free; deletions do not consume an id).
  if (k.proof) {
    const Lit lits2[2] = {a, b};
    k.proof->del(k.next_clause_id, kernel_binary_id(k, a, b), lits2, 2);
  }
  // (5) binary_ids keeps the KEY (the map stays size-monotone — the
  // inprocessing trigger reads .size()), and keeps the now-retired id too: no
  // live reason cites it (step 2 saw to that) and no caller looks up a pair
  // that is not in the watch lists, so the stale value is unreachable until a
  // re-add overwrites it (kernel_add_binary_clause: last id wins).
  k.stats.binaries_deleted++;
  // BVE retry filter (kissat clause.c kissat_delete_binary -> mark_removed)
  kernel_mark_bve_retry(k, var_of(a), "delete-binary");
  kernel_mark_bve_retry(k, var_of(b), "delete-binary");
  kernel_notify_bve_score(k, var_of(a), true);
  kernel_notify_bve_score(k, var_of(b), true);
  kernel_removed_var(k, var_of(a)); // BCE's side of the touched-epoch substrate
  kernel_removed_var(k, var_of(b));
  event_trace_binary_lifecycle(k, "delete", a, b,
                               kernel_binary_id(k, a, b), 0);
  const Lit clause[2] = {a, b};
  kernel_observe_solver_clause(k, SolverPopulationEventKind::Delete,
                               SolverClauseKind::Binary,
                               kernel_binary_id(k, a, b), clause, 2);
  return true;
}

bool kernel_delete_binary_one_copy(Kernel &k, Lit a, Lit b) {
  const uint32_t copies = kernel_binary_copies(k, a, b);
  if (copies <= 1)
    return copies && kernel_delete_binary(k, a, b);
  // An identical clause remains, so every assignment reason and the pair's
  // retained proof id remain valid. Remove one matching slot from each mirror
  // list, preserving the relative order of every survivor as Kissat's source
  // compaction does.
  for (const Lit x : {a, b}) {
    const Lit other = x == a ? b : a;
    bool removed = false;
    k.watches.compact(x, [&](WatchEntry w) {
      if (removed || !w.binary() || w.other() != other)
        return true;
      removed = true;
      return false;
    });
    assert(removed);
    if (k.dense && k.dense->track_order) {
      const auto ordered = k.dense->ordered[x];
      const auto at = std::find_if(ordered.begin(), ordered.end(),
                                   [other](const OccIndex::Entry &entry) {
                                     return entry.binary &&
                                            entry.payload == other;
                                   });
      assert(at != ordered.end());
      k.dense->ordered.erase_at(x, static_cast<size_t>(at - ordered.begin()));
    }
  }
  kernel_bve_occurrence_remove(k, a);
  kernel_bve_occurrence_remove(k, b);
  // DSR and streaming PBP must track every physical copy: a stale checker
  // clause imposes an extra substitution obligation after the last live copy
  // disappears. Both formats can retire one literal-named copy while leaving
  // the surviving reason. DRAT/LRAT retain their optional-deletion behavior;
  // without per-copy LRAT ids, deleting the current id could invalidate a
  // later reason.
  if (k.proof && (k.proof->dsr() || k.proof->streaming_pbp())) {
    const Lit clause[2] = {a, b};
    k.proof->del(k.next_clause_id, 0, clause, 2);
  }
  k.stats.binaries_deleted++;
  kernel_mark_bve_retry(k, var_of(a), "delete-binary-copy");
  kernel_mark_bve_retry(k, var_of(b), "delete-binary-copy");
  kernel_notify_bve_score(k, var_of(a), true);
  kernel_notify_bve_score(k, var_of(b), true);
  kernel_removed_var(k, var_of(a));
  kernel_removed_var(k, var_of(b));
  event_trace_binary_lifecycle(k, "delete-copy", a, b,
                               kernel_binary_id(k, a, b), copies - 1);
  const Lit clause[2] = {a, b};
  kernel_observe_solver_clause(k, SolverPopulationEventKind::Delete,
                               SolverClauseKind::Binary,
                               kernel_binary_id(k, a, b), clause, 2);
  return true;
}

bool kernel_delete_binary_one_copy_bve_scan(Kernel &k, Lit pivot, Lit other) {
  // Reuse the proof-, reason-, multiplicity-, and representation-safe kernel
  // transition, but reproduce eliminate.c::kissat_eliminate_binary's score
  // notification: `pivot` is the stack currently being compacted and has
  // already been popped from the schedule, while `other` is updated and may
  // be reinserted.  Suppressing the hook does not suppress retry/touch state.
  const BveScoreUpdateHook hook = k.bve_score_update_hook;
  void *const state = k.bve_score_update_state;
  k.bve_score_update_hook = nullptr;
  const bool removed = kernel_delete_binary_one_copy(k, pivot, other);
  k.bve_score_update_hook = hook;
  k.bve_score_update_state = state;
  if (removed)
    kernel_notify_bve_score(k, var_of(other), true);
  return removed;
}

uint32_t kernel_remove_duplicate_binary_copies(Kernel &k, Lit a, Lit b) {
  const uint32_t copies = kernel_binary_copies(k, a, b);
  if (copies <= 1)
    return 0;
  const uint32_t removed_copies = copies - 1;
  // Pinned forward.c compacts each directed watch vector in place and retains
  // its first occurrence.  Do the same on both mirrors; deleting one copy at
  // a time through the general primitive would retain the last occurrence and
  // change later propagation/occurrence order.
  for (const Lit x : {a, b}) {
    const Lit other = x == a ? b : a;
    bool kept = false;
    uint32_t removed = 0;
    k.watches.compact(x, [&](WatchEntry w) {
      if (!w.binary() || w.other() != other)
        return true;
      if (!kept) {
        kept = true;
        return true;
      }
      removed++;
      return false;
    });
    assert(kept && removed == removed_copies);
    if (k.dense && k.dense->track_order) {
      bool retained = false;
      uint32_t retired = 0;
      k.dense->ordered.compact(x, [&](OccIndex::Entry entry) {
        if (entry.binary && entry.payload == other) {
          if (!retained) {
            retained = true;
          } else {
            retired++;
            return false;
          }
        }
        return true;
      });
      assert(retained && retired == removed_copies);
    }
  }
  kernel_bve_occurrence_remove(k, a, removed_copies);
  kernel_bve_occurrence_remove(k, b, removed_copies);
  // DRAT names clauses by literals and can mirror Kissat's one deletion per
  // retired copy. Native LRAT keeps only the newest id for an identical pair;
  // deletions are optional, so retain those checker clauses and keep that id
  // available to later reasons.
  if (k.proof && !k.proof->lrat()) {
    const Lit lits2[2] = {a, b};
    for (uint32_t i = 0; i < removed_copies; ++i)
      k.proof->del(k.next_clause_id, 0, lits2, 2);
  }
  k.stats.binaries_deleted += removed_copies;
  for (uint32_t i = 0; i < removed_copies; ++i) {
    kernel_mark_bve_retry(k, var_of(a), "deduplicate-binary");
    kernel_mark_bve_retry(k, var_of(b), "deduplicate-binary");
    kernel_removed_var(k, var_of(a));
    kernel_removed_var(k, var_of(b));
  }
  kernel_notify_bve_score(k, var_of(a), true);
  kernel_notify_bve_score(k, var_of(b), true);
  event_trace_binary_lifecycle(k, "deduplicate", a, b,
                               kernel_binary_id(k, a, b), 1);
  const Lit clause[2] = {a, b};
  kernel_observe_solver_clause(k, SolverPopulationEventKind::Delete,
                               SolverClauseKind::Binary,
                               kernel_binary_id(k, a, b), clause, 2,
                               removed_copies);
  return removed_copies;
}

// Remove EVERY physical copy of the pair (a,b).  DRAT can retire each copy by
// literal multiset.  Native LRAT stores only the latest id for a duplicated
// pair, so deleting that id repeatedly would be invalid; when multiple copies
// exist, retain their proof clauses in the checker (deletions are optional)
// while removing the complete internal watch multiset.
static bool delete_binary_all_copies(Kernel &k, Lit a, Lit b,
                                     uint32_t copies) {
  if (copies == 0)
    return false; // absent (idempotent)
  // retire a live ROOT reason before removing the clause(s) that name it (a no-op
  // for Factor, whose grid literals are both unassigned, but kept for generality)
  for (const Lit x : {a, b}) {
    Assigned &ax = k.assigned[var_of(x)];
    const Lit other = (x == a) ? b : a;
    if (k.values[x] > 0 && binary_antecedes(k, x, other)) {
      if (ax.level != 0)
        return false; // live non-root reason: refuse (never at a root)
      kernel_materialize_dsr_root_unit(k, x);
      ax.reason = UNIT_REASON;
    }
  }
  // Remove every matching mirror.  Factor's null-stack dense entry makes each
  // original pair unique and leaves binary-only slices.  Kissat removes that
  // exact case by finding the word and moving only its suffix
  // (`factor.c:eagerly_remove_watch`); use the same representation operation.
  // Duplicated or mixed sparse lists retain the general stable compaction.
  for (const Lit x : {a, b}) {
    const Lit other = (x == a) ? b : a;
    WatchHead &head = k.watches.heads[x];
    if (copies == 1 && head.size == head.binary_count()) {
      uint32_t *const begin = k.watches.begin(x);
      uint32_t *const end = k.watches.end(x);
      uint32_t *word = std::find(begin, end, binary_watch(other).raw);
      assert(word != end);
      if (word != end - 1)
        std::memmove(word, word + 1,
                     static_cast<size_t>(end - word - 1) * sizeof(*word));
      k.watches.resize_live(x, head.size - 1);
      head.set_layout(head.size, true);
    } else {
      k.watches.compact(x, [&](WatchEntry w) { // drop every matching copy
        return !(w.binary() && w.other() == other);
      });
    }
    if (k.dense && k.dense->track_order) {
      uint32_t retired = 0;
      k.dense->ordered.compact(x, [&](OccIndex::Entry entry) {
        if (entry.binary && entry.payload == other) {
          retired++;
          return false;
        }
        return true;
      });
      assert(retired == copies);
    }
  }
  kernel_bve_occurrence_remove(k, a, copies);
  kernel_bve_occurrence_remove(k, b, copies);
  if (k.binary_liveness)
    k.binary_liveness->remove(kernel_binary_key(a, b));
  if (k.proof && (!k.proof->lrat() || copies == 1)) {
    const Lit lits2[2] = {a, b};
    for (uint32_t i = 0; i < copies; i++)
      k.proof->del(k.next_clause_id, kernel_binary_id(k, a, b), lits2, 2);
  }
  k.stats.binaries_deleted += copies;
  kernel_mark_bve_retry(k, var_of(a), "delete-all-binary");
  kernel_mark_bve_retry(k, var_of(b), "delete-all-binary");
  kernel_notify_bve_score(k, var_of(a), true);
  kernel_notify_bve_score(k, var_of(b), true);
  kernel_removed_var(k, var_of(a));
  kernel_removed_var(k, var_of(b));
  event_trace_binary_lifecycle(k, "delete-all", a, b,
                               kernel_binary_id(k, a, b), 0);
  const Lit clause[2] = {a, b};
  kernel_observe_solver_clause(k, SolverPopulationEventKind::Delete,
                               SolverClauseKind::Binary,
                               kernel_binary_id(k, a, b), clause, 2, copies);
  return true;
}

bool kernel_delete_binary_all_copies(Kernel &k, Lit a, Lit b) {
  return delete_binary_all_copies(k, a, b, kernel_binary_copies(k, a, b));
}

bool kernel_delete_binary_all_copies_known(Kernel &k, Lit a, Lit b,
                                           uint32_t copies) {
  assert(kernel_binary_copies(k, a, b) == copies);
  return delete_binary_all_copies(k, a, b, copies);
}

// --- watch repair ----------------------------------------------------------
// Contract and preconditions in kernel.hpp. Transliterated from kissat watch.h
// (kissat_unwatch_blocking / kissat_watch_blocking, MIT) onto the flat watch
// arena: the per-literal vector removal becomes the write-cursor compaction
// kernel_delete_binary already uses, and the walk must be entry-aware rather
// than word-aware because a large watch occupies two consecutive words.
void kernel_unwatch_large(Kernel &k, Lit l, Ref ref) {
  assert(!k.delayed_used); // no scan may hold pointers into this slice
  // An unlogged edit would be overwritten by a pose rollback. Vivify's rewrite
  // path unwatches the replaced candidate while its own scope is still open —
  // the large-replacement case deliberately retains the assumption prefix — and
  // that scope is opened with `rollback_pose == false`, so nothing is restored
  // over the removal.
  assert(!kernel_probing_rollback_active(k));
  bool removed = false;
  k.watches.compact(l, [&](WatchEntry w) {
    if (removed || w.binary() || w.ref() != ref)
      return true;
    removed = true; // drop this entry's blocking word + ref word
    return false;
  });
  // Sparse collection may already have flushed this entry from a root-fixed
  // literal's list while retaining the unswept clause. Match the ordinary
  // watch invariant: only a permanently assigned slot may be absent. Still
  // remove an entry when present, including one attached after the flush.
  assert((removed || kernel_root_value(k, l) != 0) &&
         "unwatch of a clause this literal does not watch");
}

void kernel_swap_large_watch_refs(Kernel &k, Lit l, Ref first, Ref second,
                                  Lit blocking_for_first,
                                  Lit blocking_for_second) {
  assert(first != second);
  uint32_t *p = k.watches.begin(l);
  uint32_t *const end = k.watches.end(l);
  bool found_first = false, found_second = false;
  while (p != end) {
    if (Watch{*p}.binary()) {
      p++;
      continue;
    }
    if (p[1] == first) {
      assert(!found_first);
      p[0] = blocking_watch(blocking_for_second).raw;
      p[1] = second;
      found_first = true;
    } else if (p[1] == second) {
      assert(!found_second);
      p[0] = blocking_watch(blocking_for_first).raw;
      p[1] = first;
      found_second = true;
    }
    p += 2;
  }
  assert(found_first && found_second);
  (void)found_first;
  (void)found_second;
}

void kernel_promote_watched_lit(Kernel &k, Ref ref, uint32_t slot,
                                uint32_t from) {
  assert(slot < 2 && from >= 2);
  Clause *c = clause_at(k.arena, ref);
  assert(!c->garbage());
  assert(from < c->size);
  const Lit displaced = c->lits[slot];
  const Lit promoted = c->lits[from];
  kernel_unwatch_large(k, displaced, ref);
  // `c` survives both calls: unwatch and push_large touch the WATCH arena,
  // never the clause arena, so no clause is relocated here.
  c->lits[slot] = promoted;
  c->lits[from] = displaced;
  // The blocking literal of the OTHER watch's entry still names `displaced`,
  // which is still a member of the clause — blocking literals are hints, valid
  // for any member (kissat leaves the same staleness behind for the same
  // reason). `c->searched` stays in [2, size) and is likewise only a hint.
  k.watches.push_large(promoted, c->lits[slot ^ 1], ref);
}

// See kernel.hpp for the contract, and for why this stops at the fixed
// variable instead of walking its occurrences.
void kernel_touch_sync_fixed(Kernel &k) {
  const uint32_t fixed = kernel_root_fixed(k);
  if (k.touch_fixed_head >= fixed)
    return;
  const uint32_t from = k.touch_fixed_head;
  k.touch_fixed_head = fixed;
  for (uint32_t i = from; i < fixed; i++) {
    const Var v = var_of(k.trail[i]);
    if (k.assigned[v].level != 0)
      continue; // an out-of-order literal above the root frame is not fixed yet
    kernel_touch_var(k, v);
  }
}

// kernel_assign* live header-inline in kernel.hpp (fastassign.h pattern)

// The one per-literal unassign. Every path that takes a literal off the trail
// goes through here: value reset, both unassigned counters, and the policy's
// ranking structures must move together, and a caller that re-implements the
// first two silently desynchronizes the third.
template <bool HasH> static void kernel_unassign_literal(Kernel &k, Lit lit) {
  const Var v = var_of(lit);
  k.values[lit] = 0;
  k.values[negate(lit)] = 0;
  k.unassigned++;
  if (!kernel_var_retired(k, v))
    k.unassigned_active++;
  if constexpr (HasH) {
    // Ranking-structure maintenance. Invariant: a RETIRED variable must not
    // re-enter either structure here, or deactivation degenerates into
    // demotion — the heap re-push puts it straight back, and in focused mode
    // the hint update points `search_idx` at a dequeued variable whose
    // `values` are 0, so the queue returns it immediately. kissat never hits
    // this, because its deactivated variables are never on the trail.
    if (kernel_var_retired(k, v)) {
      // leave both structures alone; it stays out for good
    } else if (k.h->stable) {
      if (!heuristics_heap_contains(k, v)) {
        event_trace_detail(k, "heap_reinsert",
                           kernel_externalize(k, make_lit(v, false)));
        heuristics_heap_push(k, v);
      }
    } else if (k.h->links[v].stamp > k.h->search_stamp) {
      k.h->search_idx = v;
      k.h->search_stamp = k.h->links[v].stamp;
    }
  }
}

// The `H`-hook seam. `HasH` selects at compile time whether this backtrack
// maintains search guidance — the four `H` touches (target/best snapshot, phase
// save, heap re-insert, VMTF search-cache) each sit under `if constexpr
// (HasH)`, so the null-`H` instantiation's inner loop carries no per-literal
// `if (k.h)`. Only <true> is instantiated today.
template <bool HasH>
static void kernel_backtrack_impl(Kernel &k, uint32_t target,
                                  bool search_backtrack) {
  assert(target < k.level);
  // Saved phases are refreshed by kernel_assign, matching kissat's
  // inlineassign.h.  Target/best snapshots still belong at the call sites:
  // the conflict path steps down one level before snapshotting
  // (kissat backtrack.c:156). Snapshotting here would let the high-water mark
  // include the conflict level's own literals, so later genuinely-longer
  // trails would fail to update it and the target phases would go stale.
  // Compacting backtrack (docs/dev/implementation-notes.md, D-LEVEL-2): true-level
  // assignment leaves the trail unsorted by level, so it cannot be truncated by
  // position. Scan the suffix above the target frame and KEEP every literal
  // whose true level <= target, unassigning only those above it. Inert while
  // chrono is off, where nothing is kept and this is plain truncation.
  kernel_affine_note_trail_retraction(k);
  const uint32_t boundary = k.frames[target + 1];
  const uint32_t end = static_cast<uint32_t>(k.trail.size());
  uint32_t j = boundary; // write cursor for kept (out-of-order) literals
  for (uint32_t i = boundary; i < end; i++) {
    const Lit lit = k.trail[i];
    const Var v = var_of(lit);
    if (k.assigned[v].level <= target) { // keep: still implied at <= target
      k.trail[j] = lit;
      k.assigned[v].trail = j;
      j++;
      continue;
    }
    kernel_unassign_literal<HasH>(k, lit);
  }
  k.trail.resize(j);
  k.frames.resize(target + 1);
  k.level = target;
  // Re-propagate from the right point: normally the boundary, as cadical does,
  // since the kept out-of-order literals get rescanned and the prefix below the
  // boundary was propagated at a fixpoint. But if an off-fixpoint Decide
  // tainted the trail (D-LEVEL-1) a below-boundary scan may be stale — it
  // treated a clause as satisfied by an ahead literal this backtrack just
  // unassigned — so the whole search trail must be rescanned: reset to 0, never
  // forward. Only a real search backtrack does this. A probing restore unwinds
  // its own assumptions off a clean root fixpoint and must land back on the
  // boundary, so it keeps the boundary reset regardless of taint.
  const uint32_t reset = (k.trail_tainted && search_backtrack) ? 0 : boundary;
  if (k.propagate_head > reset)
    k.propagate_head = reset;
}

void kernel_backtrack(Kernel &k, uint32_t target, bool search_backtrack) {
  // Dispatch ONCE per backtrack on whether `H` is present. With no
  // `H` (an unguided solver) the <false> instantiation runs the same σ unwind but
  // its inner loop is branch-free w.r.t. `H` (every hook is `if constexpr (HasH)`,
  // compiled out). DefaultPolicy always has `H`, so it takes <true> — bit-exact.
  if (k.h)
    kernel_backtrack_impl<true>(k, target, search_backtrack);
  else
    kernel_backtrack_impl<false>(k, target, search_backtrack);
}

template <bool HasH>
static void kernel_unwind_speculation_impl(Kernel &k, uint32_t position) {
  assert(position <= k.trail.size());
  // Every level above the root must live entirely inside the discarded
  // suffix, or landing at level zero would strand one of its literals.
  assert(!k.level || k.frames[1] >= position);
  if (k.trail.size() > position) kernel_affine_note_trail_retraction(k);
  while (k.trail.size() > position) {
    const Lit lit = k.trail.back();
    k.trail.pop_back();
    kernel_unassign_literal<HasH>(k, lit);
    // A unit id caches the polarity of the assignment being retracted. A
    // later root replay can derive the opposite polarity before refuting F;
    // retaining this id would suppress that derivation and cite the wrong
    // unit in the empty-clause chain. The old proof line itself stays live.
    k.unit_ids[var_of(lit)] = 0;
  }
  k.frames.resize(1);
  k.level = 0;
  // Same monotonicity rule as a backtrack: the cursor may only move back.
  // Moving it forward would declare literals propagated that never were.
  if (k.propagate_head > position)
    k.propagate_head = position;
  k.conflict_valid = false;
  k.conflict_ref = INVALID_REF;
}

void kernel_unwind_speculation(Kernel &k, uint32_t position) {
  if (k.h)
    kernel_unwind_speculation_impl<true>(k, position);
  else
    kernel_unwind_speculation_impl<false>(k, position);
}

// --- shared proof idioms (decls in kernel.hpp) ------------------------------
void kernel_emit_empty_from_pair(Kernel &k, uint64_t id_a, uint64_t id_b) {
  if (k.proof) {
    if (k.proof->lrat()) {
      const uint64_t ch[2] = {id_a, id_b};
      k.proof->add_empty(++k.next_clause_id, ch, 2);
    } else {
      k.proof->add_empty(0, nullptr, 0);
    }
  }
  k.inconsistent = true;
}

PendingUnits::~PendingUnits() {
  if (owner)
    for (const Lit unit : units)
      owner->eager_marks[unit] = 0;
}

void PendingUnits::init(Kernel &k) {
  if (owner)
    for (const Lit unit : units)
      owner->eager_marks[unit] = 0;
  units.clear();
  owner = &k;
#ifndef NDEBUG
  for (const uint8_t mark : k.eager_marks)
    assert(!mark && "PendingUnits requires the shared marks to be clear");
#endif
  if (k.proof && k.proof->lrat())
    ids.assign(k.nvars, 0);
  else {
    ids.clear();
    ids.shrink_to_fit();
  }
}

PendingUnits::Outcome PendingUnits::record(Kernel &k, Lit u, uint64_t id) {
  assert(owner == &k);
  const Var v = var_of(u);
  if (!k.eager_marks[u] && !k.eager_marks[negate(u)]) {
    k.eager_marks[u] = 1;
    if (!ids.empty())
      ids[v] = id;
    units.push_back(u);
    return Outcome::Recorded;
  }
  if (k.eager_marks[u])
    return Outcome::Duplicate; // same-sign duplicate: a redundant unit
  kernel_emit_empty_from_pair(k, id, ids.empty() ? 0 : ids[v]);
  return Outcome::Refuted;
}

void PendingUnits::commit(Kernel &k, Lit u) const {
  const Var v = var_of(u);
  if (!ids.empty() && ids[v])
    k.unit_ids[v] = ids[v];
}

void PendingUnits::refute(Kernel &k, Lit u) const {
  kernel_emit_empty_from_pair(
      k, ids.empty() ? 0 : ids[var_of(u)], k.unit_ids[var_of(u)]);
}

ReplaceOutcome kernel_replace_clause(Kernel &k, Clause *c, uint64_t id,
                                     const Lit *lits, uint32_t size,
                                     bool redundant, uint32_t glue,
                                     Ref *large_ref,
                                     bool mark_added_material) {
  // `id` is the REPLACEMENT's id, and every caller allocates it immediately
  // before calling in — so it equals `next_clause_id`, which is what the shared
  // primitive prefixes its LRAT deletion line with. That was an unstated,
  // uncheckable invariant while this site passed `id` by hand; assert it.
  assert(id == k.next_clause_id);
  // Large redundant-clause rewrites model Kissat's in-place shrink operations
  // even though Tactix appends a packed replacement record.  Recency belongs
  // to that logical clause: save it before the arena-mutating add so the
  // representation cannot rejuvenate an aged clause.  Other metadata is
  // caller-specific (in particular Sweep's `vivify` bit) and is restored by
  // the source-backed caller that owns that in-place rewrite.
  const uint32_t inherited_used = c->used();
  const uint64_t predecessor_id = c->id();
  kernel_observe_replacement(k, SolverReplacementPhase::Begin,
                             SolverReplacementOutcome::Pending,
                             predecessor_id, id, c->lits, c->size, lits,
                             size);
  const bool semantic_irredundant = !c->redundant();
  const Ref source_ref = static_cast<Ref>(
      reinterpret_cast<const uint32_t *>(c) - k.arena.data());
  // This is one semantic rewrite, not a deletion of every old literal followed
  // by an unrelated addition.  Kissat arms elimination only for literals that
  // actually disappear from the clause.
  if (semantic_irredundant)
    for (const Lit old : *c)
      if (std::find(lits, lits + size, old) == lits + size) {
        kernel_mark_bve_retry(k, var_of(old));
        kernel_removed_var(k, var_of(old));
        kernel_notify_bve_score(k, var_of(old), true);
      }
  kernel_delete_clause(k, c, false); // before any arena-mutating add below
  if (size == 0) { // under-derived root only (corpus 002): the ADD just
    k.inconsistent = true; // emitted IS the refutation
    kernel_observe_replacement(k, SolverReplacementPhase::End,
                               SolverReplacementOutcome::Refuted,
                               predecessor_id, id, nullptr, 0, nullptr, 0);
    return ReplaceOutcome::Refuted;
  }
  if (size == 1) {
    kernel_observe_replacement(k, SolverReplacementPhase::End,
                               SolverReplacementOutcome::Unit,
                               predecessor_id, id, nullptr, 0, lits, size);
    return ReplaceOutcome::Unit; // caller owns unit semantics
  }
  if (size == 2) {
    kernel_add_binary_clause(k, lits[0], lits[1], id, mark_added_material);
    kernel_observe_replacement(k, SolverReplacementPhase::End,
                               SolverReplacementOutcome::Binary,
                               predecessor_id, id, nullptr, 0, lits, size);
    return ReplaceOutcome::Binary;
  }
  kernel_inherit_clause_position(k, id, source_ref);
  const Ref r = kernel_add_large_clause(k, lits, size, redundant, glue, id,
                                        mark_added_material);
  if (redundant)
    clause_at(k.arena, r)->set_used(inherited_used);
  if (large_ref)
    *large_ref = r;
  const Clause *const replacement = clause_at(k.arena, r);
  kernel_observe_replacement(k, SolverReplacementPhase::End,
                             SolverReplacementOutcome::Large,
                             predecessor_id, id, nullptr, 0,
                             replacement->lits, replacement->size);
  return ReplaceOutcome::Large;
}

void kernel_replace_large_clause_in_place(Kernel &k, Ref ref, uint64_t id,
                                          const Lit *lits, uint32_t size,
                                          uint32_t glue,
                                          const char *source_tag,
                                          bool mark_added_material,
                                          Lit retained_watch) {
  Clause *c = clause_at(k.arena, ref);
  assert(!c->garbage());
  assert(id == k.next_clause_id);
  assert(size >= 3 && size <= c->size);
  const bool retain = retained_watch != INVALID_LIT;
  assert(!retain || (lits[0] == retained_watch &&
                     (c->lits[0] == retained_watch ||
                      c->lits[1] == retained_watch)));
  assert(!k.delayed_used);
  assert(!kernel_probing_rollback_active(k));

  const uint32_t old_size = c->size;
  const uint64_t predecessor_id = c->id();
  kernel_observe_replacement(k, SolverReplacementPhase::Begin,
                             SolverReplacementOutcome::Pending,
                             predecessor_id, id, c->lits, c->size, lits,
                             size);
  const uint32_t old_flags = c->flags;
  const uint64_t old_position_alias =
      c->id() ? k.clause_position_alias.get(c->id()) : 0;
  const Lit old_first = c->lits[0], old_second = c->lits[1];
  const bool redundant = c->redundant();

  // Rewrite images usually retain the source order (root stripping and
  // connected substitution). Restarting each membership lookup at the first
  // literal turns those unchanged runs into quadratic scans. Continue after
  // the previous match, wrapping over the prefix if necessary: every query
  // still searches the complete image, including arbitrary reorderings.
  // No marks, clause pose, or shared scratch are changed by this cursor.
  const auto membership = [](const Lit *begin, const Lit *end) {
    return [begin, end, next = begin](Lit lit) mutable {
      const Lit *found = std::find(next, end, lit);
      if (found == end) {
        found = std::find(begin, next, lit);
        if (found == next)
          return false;
      }
      next = found + 1;
      return true;
    };
  };
  auto in_new = membership(lits, lits + size);

  // Update the optional dense membership while the old semantic image is
  // still readable.  Retained literals keep the same Ref and therefore their
  // exact occurrence position; only a genuinely new literal is appended.
  if (k.dense && !redundant) {
    auto in_old = membership(c->lits, c->lits + old_size);
    for (uint32_t i = 0; i < old_size; i++) {
      const Lit lit = c->lits[i];
      if (in_new(lit))
        continue;
      if (k.dense_bulk_cleared && lit < k.dense_bulk_cleared->size() &&
          (*k.dense_bulk_cleared)[lit]) {
        kernel_bve_occurrence_remove(k, lit);
        continue;
      }
      kernel_dense_remove_large_occurrence(k, lit, ref);
      kernel_bve_occurrence_remove(k, lit);
    }
    for (uint32_t i = 0; i < size; i++)
      if (!in_old(lits[i])) {
        kernel_dense_add_large_occurrence(k, lits[i], ref);
        kernel_bve_occurrence_add(k, lits[i]);
      }
  }

  // A live in-place record cannot retain stale watch entries.  Removing them
  // before the semantic overwrite also avoids depending on cached blockers.
  // kissat's on-the-fly strengthening keeps the surviving watch's entry.
  if (k.large_watches_connected) {
    if (!retain || old_first != retained_watch)
      kernel_unwatch_large(k, old_first, ref);
    if (!retain || old_second != retained_watch)
      kernel_unwatch_large(k, old_second, ref);
  }
  if (!redundant && !retain)
    for (uint32_t i = 0; i < old_size; ++i) {
      const Lit old = c->lits[i];
      if (in_new(old))
        continue;
      kernel_mark_bve_retry(k, var_of(old));
      kernel_removed_var(k, var_of(old));
      kernel_notify_bve_score(k, var_of(old), true);
    }
  kernel_delete_clause(k, c, false);
  if (old_position_alias)
    k.clause_position_alias.set(id, old_position_alias);

  kernel_rearm_propagation(k, lits, size);
  std::copy(lits, lits + size, c->lits);
  if (size < old_size && !(old_flags & Clause::SHRUNKEN))
    c->lits[old_size - 1] = INVALID_LIT;
  c->size = size;
  c->flags = old_flags & ~Clause::GARBAGE;
  if (!(old_flags & Clause::REDUNDANT))
    k.live_irredundant_large++; // live again, with its previous status
  if (size < old_size)
    c->flags |= Clause::SHRUNKEN;
  c->set_glue(redundant ? glue : 0);
  c->searched = 2;
  c->set_id(id);

#ifdef TACTIX_ENABLE_RULE_JOURNAL
  rule_journal_note_large_add(k, lits, size, id, redundant,
                              redundant ? glue : 0);
#endif
  if (k.large_watches_connected) {
    if (retain) {
      // strengthen.c: the retained entry only changes its blocking literal;
      // the new second watch is appended to its list.
      uint32_t *p = k.watches.begin(lits[0]);
      uint32_t *const end = k.watches.end(lits[0]);
      bool updated = false;
      while (p != end) {
        if (Watch{*p}.binary()) {
          p++;
          continue;
        }
        if (p[1] == ref) {
          p[0] = blocking_watch(lits[1]).raw;
          updated = true;
          break;
        }
        p += 2;
      }
      assert(updated && "retained watch entry must exist");
      (void)updated;
      k.watches.push_large(lits[1], lits[0], ref);
    } else {
      k.watches.push_large(lits[0], lits[1], ref);
      k.watches.push_large(lits[1], lits[0], ref);
    }
  }

  if (!redundant && mark_added_material)
    for (uint32_t i = 0; i < size; i++) {
      kernel_touch_var(k, var_of(lits[i]));
      kernel_mark_subsume_material(k, var_of(lits[i]));
      kernel_mark_factor_literal(k, lits[i], source_tag);
      kernel_notify_bve_score(k, var_of(lits[i]), false);
    }
  else if (k.touch_redundant) {
    const bool dyn = kernel_likely_kept(k, true, glue, size);
    k.stats.touch_redundant_dynamic += dyn;
    k.stats.touch_redundant_static6 += (glue <= 6);
    if (dyn)
      for (uint32_t i = 0; i < size; i++)
        kernel_touch_kept_var(k, var_of(lits[i]));
  }
  kernel_observe_solver_clause(k, SolverPopulationEventKind::Add,
                               SolverClauseKind::Large, id, c->lits, c->size,
                               1, redundant);
  kernel_observe_replacement(k, SolverReplacementPhase::End,
                             SolverReplacementOutcome::Large,
                             predecessor_id, id, nullptr, 0, c->lits,
                             c->size);
}

RootFlushOutcome kernel_flush_root_clause(Kernel &k, Ref *ref,
                                          uint64_t *changed,
                                          bool mark_binary_added_material,
                                          bool clamp_glue,
                                          bool mark_satisfied_removal) {
  const Ref original_ref = *ref;
  Clause *c = clause_at(k.arena, *ref);
  assert(!c->garbage());
  const bool reason_clause = c->reason();
  Var reason_var = INVALID_VAR;
  bool has_root_false = false;
  for (const Lit l : *c) {
    const int8_t root_value = kernel_root_value(k, l);
    if (root_value > 0) {
      // A higher-level reason cannot also be satisfied by a root literal.
      // Root reasons are deliberately retired to UNIT_REASON by GC below.
#ifndef NDEBUG
      if (reason_clause)
        for (const Lit assigned_lit : k.trail) {
          const Var v = var_of(assigned_lit);
          const Assigned &a = k.assigned[v];
          if (!a.binary && a.reason == original_ref) {
            assert(reason_var == INVALID_VAR);
            reason_var = v;
          }
        }
      assert(reason_var == INVALID_VAR || k.assigned[reason_var].level == 0);
#endif
      kernel_delete_clause(k, c, mark_satisfied_removal);
      k.stats.flushed++;
      if (changed)
        (*changed)++;
      return RootFlushOutcome::Deleted;
    }
    has_root_false |= root_value < 0;
  }
  if (!has_root_false)
    return RootFlushOutcome::None;

  // Allocate rewrite/proof scratch only for a clause that really shrinks.
  // Root collection visits every live clause, while most are unchanged; the
  // former eager vectors made that common case pay two heap allocations and a
  // full literal copy merely to discover `None`.
  std::vector<Lit> kept;
  std::vector<uint64_t> chain;
  kept.reserve(c->size);
  if (k.proof && k.proof->lrat())
    chain.reserve(c->size + 1);
  for (const Lit l : *c) {
    if (kernel_root_value(k, l) < 0) {
      if (k.proof && k.proof->lrat()) {
        const uint64_t unit_id = k.unit_ids[var_of(l)];
        assert(unit_id);
        chain.push_back(unit_id);
      }
    } else {
      kept.push_back(l);
    }
  }

  // Locating a large reason is O(trail).  Most reason clauses encountered by
  // root collection are already satisfied and return above, so doing this
  // before scanning the clause made the collector quadratic in the root trail
  // for no semantic benefit.  A rewritten unsatisfied reason still needs the
  // exact owner so its arena reference can be repaired below.
  if (reason_clause)
    // A reason clause has exactly one satisfied literal, the one it forced, so
    // kissat reads the owner off the clause (`collect.c:get_forced`) instead of
    // searching the trail for the variable that names this reference.
    for (const Lit lit : *c) {
      if (kernel_root_value(k, lit) > 0 || k.values[lit] <= 0)
        continue;
      const Var v = var_of(lit);
      const Assigned &a = k.assigned[v];
      if (!a.binary && a.reason == original_ref)
        reason_var = v;
      break;
    }

  const uint64_t original_id = c->id();
  const bool redundant = c->redundant();
  // kissat's generic root flush (collect.c) drops root-false literals without
  // touching glue; Vivify's schedule-time root simplification clamps to
  // size-1 (vivify.c) and selects that semantics via `clamp_glue`.
  const uint32_t glue =
      clamp_glue ? kernel_shrunken_glue(k, c->glue(),
                                        static_cast<uint32_t>(kept.size()))
                 : c->glue();
#ifdef TACTIX_ENABLE_EVENT_TRACE
  if (kept.size() == 2) {
    std::string payload = "source_id=" + std::to_string(original_id) +
                          "|source=";
    bool first = true;
    for (const Lit lit : *c) {
      if (!first)
        payload.push_back(',');
      payload += std::to_string(kernel_externalize(k, lit));
      first = false;
    }
    event_trace_binary_lifecycle(
        k, "root-flush-binary-source", kept[0], kept[1], original_id,
        kernel_binary_copies(k, kept[0], kept[1]), payload.c_str());
  }
#endif
  // At a root propagation fixpoint this cannot be needed above level zero: a
  // clause with one non-root-fixed literal would already have made that
  // literal root-fixed.  Keep the primitive sound for an off-fixpoint caller,
  // though: unwind before turning the replacement into a permanent unit.
  if (kept.size() == 1 && k.level)
    kernel_backtrack(k, 0, /*search_backtrack=*/false);
  const uint64_t id = ++k.next_clause_id;
  if (k.proof) {
    if (k.proof->lrat()) {
      chain.push_back(original_id);
      k.proof->add_rup(id, kept.data(), static_cast<uint32_t>(kept.size()),
                       chain.data(), static_cast<uint32_t>(chain.size()));
    } else {
      k.proof->add_rup(0, kept.data(), static_cast<uint32_t>(kept.size()),
                       nullptr, 0);
    }
  }
  k.stats.flushed++;
  if (changed)
    (*changed)++;
  if (kept.size() >= 3) {
    // Kissat's root simplifiers shorten a surviving large clause inside its
    // existing arena record.  Besides preserving reference/occurrence order,
    // this retains header state such as `vivify`, `used`, and `reason`.
    // Appending a packed replacement here silently cleared Vivify's leftover
    // priority between doses and could postpone the clause past the next
    // cumulative effort boundary despite an identical semantic formula.
    kernel_replace_large_clause_in_place(
        k, original_ref, id, kept.data(), static_cast<uint32_t>(kept.size()),
        redundant ? glue : 0, "root_flush",
        /*mark_added_material=*/false);
    *ref = original_ref;
    return RootFlushOutcome::Rewritten;
  }
  switch (kernel_replace_clause(k, c, id, kept.data(),
                                static_cast<uint32_t>(kept.size()), redundant,
                                redundant ? glue : 0, ref,
                                /*mark_added_material=*/
                                    kept.size() == 2 &&
                                        mark_binary_added_material)) {
  case ReplaceOutcome::Refuted:
    return RootFlushOutcome::Refuted;
  case ReplaceOutcome::Unit: {
    const Lit unit = kept[0];
    const int8_t value = k.value(unit);
    const uint64_t opposite_id = k.unit_ids[var_of(unit)];
    k.unit_ids[var_of(unit)] = id;
    if (value < 0) {
      kernel_emit_empty_from_pair(k, id, opposite_id);
      return RootFlushOutcome::Refuted;
    }
    if (!value)
      kernel_assign_unit(k, unit);
    return RootFlushOutcome::Unit;
  }
  case ReplaceOutcome::Binary:
    if (reason_var != INVALID_VAR) {
      Assigned &a = k.assigned[reason_var];
      const Lit forced = k.trail[a.trail];
      assert(kept.size() == 2 &&
             (kept[0] == forced || kept[1] == forced));
      a.reason = kept[0] == forced ? kept[1] : kept[0];
      a.binary = true;
      a.jumped = false;
    }
    assert(!k.pending.valid || k.pending.ref != original_ref);
    return RootFlushOutcome::Binary;
  case ReplaceOutcome::Large:
    assert(false);
    return RootFlushOutcome::Refuted;
  }
  assert(false);
  return RootFlushOutcome::Refuted;
}

RootCollectOutcome kernel_sparse_collect_root(Kernel &k, Ref start) {
  k.stats.sparse_collections++;
  if (!kernel_has_root_assignment(k))
    return RootCollectOutcome::NoChange;
  uint64_t changed = 0;
  bool has_garbage = false;
  RootCollectOutcome outcome = RootCollectOutcome::NoChange;
  const uint64_t sparse_cut = start ? kernel_clause_position(k, start) : 0;

  // Kissat's sparse collector flushes watched binaries before sweeping the
  // large-clause arena (collect.c:flush_watched_clauses_by_literal).  In
  // particular it drops binaries satisfied by any root literal.  Keeping
  // those pairs made initial collection create 216 shortened binaries on
  // FmlaEquivChain without retiring the 216 already-satisfied pairs, shifting
  // every later Factor score even though the root assignment was identical.
  // Snapshot unique pairs first because deletion compacts both watch lists.
  std::vector<std::pair<Lit, Lit>> satisfied_binaries;
  // The mirror-owner test needs the stable import rank for every binary watch.
  // Store its variable component once instead of materializing two 64-bit
  // literal ranks per variable: the sign is already the low literal bit.  On
  // binary-heavy formulas this keeps the random rank probes in half the cache
  // footprint while preserving the exact kernel_import_lit_order comparison.
  std::vector<uint64_t> var_rank(static_cast<size_t>(k.nvars));
  for (Var v = 0; v < k.nvars; v++)
    var_rank[v] = kernel_import_var_order(k, v);
  // A satisfied binary has at least one root-true endpoint. Visit only those
  // endpoint lists instead of decoding both mirrors of the complete BIG. If
  // both endpoints are root true, the stable lower import endpoint owns the
  // visit; otherwise orient the recorded pair to that same owner afterward.
  // Thus every physical copy is recorded exactly once in the same form as the
  // former complete-image scan.
  for (const Lit lit : k.trail) {
    if (kernel_root_value(k, lit) <= 0)
      continue;
    for (const WatchEntry watch : k.watches.entries(lit)) {
      if (!watch.binary())
        continue;
      const Lit other = watch.other();
      const uint64_t lit_order = 2 * var_rank[var_of(lit)] + negated(lit);
      const uint64_t other_order =
          2 * var_rank[var_of(other)] + negated(other);
      if (kernel_root_value(k, other) > 0 && lit_order >= other_order)
        continue;
      if (lit_order < other_order)
        satisfied_binaries.emplace_back(lit, other);
      else
        satisfied_binaries.emplace_back(other, lit);
    }
  }
  std::sort(satisfied_binaries.begin(), satisfied_binaries.end());
  // The scan above already met every copy of every pair, and each pair is
  // deleted whole, so recounting through the watch list would rescan a
  // root-satisfied literal's still-intact prefix once per pair it owns.
  for (size_t at = 0; at < satisfied_binaries.size();) {
    const auto [a, b] = satisfied_binaries[at];
    size_t end = at;
    while (end < satisfied_binaries.size() && satisfied_binaries[end].first == a &&
           satisfied_binaries[end].second == b)
      end++;
    const uint32_t copies = static_cast<uint32_t>(end - at);
    at = end;
    const bool deleted = kernel_delete_binary_all_copies_known(k, a, b, copies);
    assert(deleted);
    (void)deleted;
    changed += copies;
    outcome = RootCollectOutcome::Changed;
  }

  // A Reduce collection starts at the historical reducible boundary in Kissat's
  // current arena. Tactix can have an appended packed replacement whose
  // generation-local position lies before that cut. Classify every original
  // record by that position: the logical prefix remains byte-for-byte
  // untouched, while every logical-tail record is root-flushed.
  const Ref original_end = static_cast<Ref>(k.arena.size());
  for (Ref ref = 0; ref < original_end;) {
    Clause *const source = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(source);
    if (source->garbage()) {
      has_garbage = true;
    } else if (kernel_clause_position(k, ref) >= sparse_cut) {
      Ref current = ref;
      const RootFlushOutcome flushed =
          kernel_flush_root_clause(k, &current, &changed,
                                   /*mark_binary_added_material=*/false,
                                   /*clamp_glue=*/false,
                                   /*mark_satisfied_removal=*/false);
      if (flushed == RootFlushOutcome::Refuted) {
        outcome = RootCollectOutcome::Refuted;
        break;
      }
      if (flushed == RootFlushOutcome::Unit)
        outcome = RootCollectOutcome::Unit;
      else if (flushed != RootFlushOutcome::None &&
               outcome == RootCollectOutcome::NoChange)
        outcome = RootCollectOutcome::Changed;
    }
    ref += words;
  }
  k.stats.sparse_collected += changed;
  // Reduce marks its selected victims before entering the root-aware sparse
  // collector.  Those records still require the same sweep and rewatch even
  // when no root-fixed literal happens to shorten another clause in this
  // collection.  `changed` only describes root simplification, so it cannot
  // by itself be the collection admission predicate.
  if ((changed || has_garbage) && outcome != RootCollectOutcome::Refuted) {
    kernel_collect_garbage(k, start, /*sparse_collection=*/true);
  }
  return outcome;
}

// --- propagation engine ------------------------------------------------
// Transliterated from kissat/src/proplit.h (MIT, Biere et al.): blocking
// literals, binary clauses inlined in watch lists, in-place compaction,
// `searched` scan resumption, and delayed watch relocation — with the flat
// watch arena, a push during a scan can move the arena, so relocations queue in
// kernel.delayed. It lives in the kernel rather than in the Propagate rule
// because the probing rules need propagation as a subroutine (plan §5.3).
// Probing bookkeeping log (see ProbingLog in kernel.hpp): snapshot `l`'s watch
// list the FIRST time this scope touches it, i.e. just before the scan's first
// real write or a delayed relocation's first push. Up to that point the list
// still holds exactly what it held at kernel_probing_begin, since a scan copies
// `*q++ = *p++` with q == p until its first drop.
static void probing_snapshot_list(Kernel &k, Lit l) {
  ProbingLog &lg = k.probing;
  if (lg.stamp[l] == lg.epoch)
    return;
  lg.stamp[l] = lg.epoch;
  const WatchHead &h = k.watches.heads[l];
  const uint32_t *const b = k.watches.arena.data() + h.offset;
  lg.lists.push_back({l, static_cast<uint32_t>(lg.words.size()), h.size,
                      k.watches.binary_size(l), h.has_large(),
                      h.binaries_first()});
  lg.words.insert(lg.words.end(), b, b + h.size);
  // S3: snapshot traffic is real memory work — charge it to the tick
  // currency (same cache-line unit as the scan at :602) so the effort
  // ledger can meter it against the probing techniques' budgets.
  kernel_charge_ticks(k, 1 + kernel_cache_lines(h.size));
}

template <bool RECORD> static inline void flush_delayed(Kernel &k) {
  for (size_t i = 0; i < k.delayed_used; i += 3) {
    const Lit lit = k.delayed[i];
    const Lit blocking = k.delayed[i + 1];
    const Ref ref = k.delayed[i + 2];
    if constexpr (RECORD)
      probing_snapshot_list(k, lit); // before the push grows/moves the list
    k.watches.push_large(lit, blocking, ref);
  }
  k.delayed_used = 0;
}

// Returns true iff a conflict was found (recorded in the kernel). `ignore`
// (cadical's ignore pointer) is a clause ref that must not propagate or
// conflict during this scan: a rule probing a candidate's own negated literals
// must not derive anything through the candidate. `RECORD` logs every
// watch-list / clause-header update into k.probing when an observer explicitly
// requests rollback; false for search and for the canonical probing engine.
// Kissat's probing propagator retains relocation and `searched` churn, so the
// production probing path deliberately uses the false instantiation too.
// `IGNORE` mirrors Kissat's `PROBING_PROPAGATION`: `propsearch.c` compiles
// without it and carries no candidate test, `proprobe.c` compiles with it
// (`proplit.h:151,169`).
template <bool RECORD, bool BEYOND = false, bool IGNORE = true>
static bool propagate_literal(Kernel &k, Lit lit, Ref ignore,
                              uint64_t &total_ticks) {
  const Lit not_lit = negate(lit);
  if constexpr (!RECORD)
    event_trace_watch_scan(k, lit);
  assert(!k.delayed_used);
  // Not WatchLists::entries/compact: this scan COPIES each entry forward and
  // then rewinds or rewrites it (dropped garbage, a new blocking literal, a
  // delayed relocation), which is neither a read-only walk nor a keep/drop
  // filter. The stride lives here on purpose.
  // hoisted data pointers (kissat proplit.h idiom): the inlined assigns store
  // through k, so without locals the compiler must re-derive the vector data
  // pointers after every assignment. The underlying arrays never move during a
  // scan — no clause allocation, and `values` is sized at reserve time.
  const int8_t *const values = k.values.data();
  uint32_t *const arena = k.arena.data();
  uint32_t *const begin = k.watches.begin(not_lit);
  uint32_t *q = begin;
  const uint32_t *p = q;
  const uint32_t *const end = k.watches.end(not_lit);
  // proplit.h hoists the propagated literal's assignment once per watch scan
  // and passes its level/reason metadata to every binary implication.  The
  // source variable cannot be reassigned while this scan is active.
  const Assigned &binary_source = k.assigned[var_of(lit)];
  // Retain the largest backing store across scans and write through a raw
  // cursor.  Grow it only when a relocation is actually delayed: Kissat's
  // stack has that output-sensitive behavior, while reserving one triple for
  // every large entry in the input watch list can allocate hundreds of MiB
  // even when every blocker is already true and nothing moves.
  bool conflict = false;
  // search ticks, kissat's proplit.h charge exactly: 1 + the cache lines of
  // the watch list scanned, then +1 per binary assignment (:92), per
  // large-clause dereference (:98), per watch relocation (:148) and per
  // large-clause assignment (:174). A deterministic memory-traffic proxy, and
  // the currency `search_ticks` is denominated in — which is the reference
  // every effort budget takes its per-mille of.
  const uint64_t words_before = end - p;
  const uint64_t base_ticks = 1 + kernel_cache_lines(words_before);
  uint64_t binary_assignments = 0, large_dereferences = 0;
  uint64_t relocations = 0, large_assignments = 0;
  uint64_t ticks = base_ticks;

  while (p != end) {
    const Watch head{*q++ = *p++};
    const Lit blocking = head.blocking();
    const int8_t blocking_value = values[blocking];
    if (head.binary()) {
      if (blocking_value > 0)
        continue;
      if (blocking_value < 0) { // conflict: clause (not_lit, blocking)
        conflict = true;
        if constexpr (!BEYOND) {
          k.conflict_valid = true;
          k.conflict_ref = INVALID_REF;
          k.conflict_bin[0] = not_lit;
          k.conflict_bin[1] = blocking;
          break;
        }
        continue;
      }
      kernel_assign_binary_from(k, blocking, not_lit, binary_source);
      binary_assignments++;
      ticks++;
    } else {
      const Ref ref = *q++ = *p++;
      if (blocking_value > 0)
        continue;
      ticks++;
      large_dereferences++;
      Clause *c = clause_at(arena, ref);
      if (c->garbage()) {
        if constexpr (RECORD)
          probing_snapshot_list(k, not_lit); // dropping an entry
        q -= 2;
        continue;
      }
      Lit *lits = c->lits;
      const Lit other = lits[0] ^ lits[1] ^ not_lit;
      const int8_t other_value = values[other];
      if (other_value > 0) { // satisfied: remember as blocking literal
        Watch w = blocking_watch(other);
        if constexpr (RECORD)
          probing_snapshot_list(k, not_lit); // rewriting a blocking literal
        q[-2] = w.raw;
        continue;
      }
      // scan for a non-false replacement, resuming at c->searched
      const Lit *const end_lits = lits + c->size;
      Lit *const searched = lits + c->searched;
      Lit *r;
      Lit replacement = INVALID_LIT;
      int8_t replacement_value = -1;
      for (r = searched; r != end_lits; r++) {
        replacement = *r;
        replacement_value = values[replacement];
        if (replacement_value >= 0)
          break;
      }
      if (replacement_value < 0) {
        for (r = lits + 2; r != searched; r++) {
          replacement = *r;
          replacement_value = values[replacement];
          if (replacement_value >= 0)
            break;
        }
      }
      if (replacement_value >= 0) { // move the watch (delayed: see above)
        if constexpr (RECORD) {
          probing_snapshot_list(k, not_lit); // dropping an entry
          k.probing.clauses.push_back({ref, static_cast<uint32_t>(r - lits),
                                       lits[0], lits[1], c->searched});
        }
        c->searched = static_cast<uint32_t>(r - lits);
        lits[0] = other;
        lits[1] = replacement;
        *r = not_lit;
        if constexpr (!RECORD)
          event_trace_watch_move(k, lit, replacement, other, c);
        q -= 2;
        if (k.delayed_used + 3 > k.delayed.size()) {
          const size_t grown =
              std::max(k.delayed_used + 3,
                       k.delayed.size() ? 2 * k.delayed.size() : size_t{3});
          k.delayed.resize(grown);
        }
        k.delayed[k.delayed_used++] = replacement;
        k.delayed[k.delayed_used++] = other;
        k.delayed[k.delayed_used++] = ref;
        ticks++;
        relocations++;
      } else if (other_value < 0) { // conflict
        // Kissat tests the ignored candidate only after performing any legal
        // watch relocation above.  Skipping at dereference time preserves a
        // different clause pose and can change a later antecedent choice.
        if constexpr (IGNORE)
          if (ref == ignore)
            continue;
        conflict = true;
        if constexpr (!BEYOND) {
          k.conflict_valid = true;
          k.conflict_ref = ref;
          break;
        }
      } else { // unit: assign `other` with this clause as reason
        if constexpr (IGNORE)
          if (ref == ignore)
            continue;
        kernel_assign_ref(k, other, ref);
        large_assignments++;
        ticks++;
      }
    }
  }
  while (p != end)
    *q++ = *p++;
  // no snapshot needed here: q != begin + size only if an entry was dropped
  // above, and every drop site snapshots first
  const uint64_t words_after = q - begin;
  k.watches.truncate_large(not_lit, static_cast<uint32_t>(words_after));
  flush_delayed<RECORD>(k);
  if constexpr (!RECORD)
    event_trace_watch_scan_post(k, lit);
  // Cost rows are emitted for the recording (probing) scan too: Vivify's
  // budget is denominated in these ticks and a per-literal comparison with
  // kissat's proplit.h needs the probing propagations as well.
  event_trace_watch_cost(k, lit, words_before, words_after, base_ticks,
                         binary_assignments, large_dereferences, relocations,
                         large_assignments, ticks);
  total_ticks += ticks;
  return conflict;
}

// Full propagation takes over whatever region binary-only propagation credited
// to the shared cursor (Kernel::binary_only_head): pull the cursor back so the
// large watches that scan skipped are inspected exactly once here. A literal
// below the returned cursor has had every clause it watches examined again,
// which is precisely what the cursor is allowed to mean.
static void kernel_settle_binary_only_debt(Kernel &k) {
  if (k.binary_only_head < k.propagate_head)
    k.propagate_head = k.binary_only_head;
  k.binary_only_head = UINT32_MAX;
}


template <bool IGNORE>
static bool propagate_trail(Kernel &k, Ref ignore) {
  kernel_settle_binary_only_debt(k);
  const uint32_t saved_propagate = k.propagate_head;
  uint32_t propagate = saved_propagate;
  uint64_t ticks = 0;
  bool conflict = false;
  while (propagate < k.trail.size()) {
    // Pinned Kissat post-increments its local trail cursor before scanning the
    // literal and publishes that cursor even when the scan finds a conflict
    // (propsearch.c:37-40). Backtrack, not Propagate, owns any required rewind
    // to the surviving frame boundary.
    const Lit lit = k.trail[propagate++];
#if defined(TACTIX_ENABLE_EVENT_TRACE) || defined(TACTIX_ENABLE_RULE_JOURNAL)
    // Diagnostic callbacks observe the same consumed cursor as before. The
    // ordinary scan only appends assignments and never reads or rewinds it.
    k.propagate_head = propagate;
#endif
    // Kissat's propagation statistic counts trail literals whose watch lists
    // are scanned, including the literal that exposes a conflict.  It does not
    // count implications at assignment time (propsearch.c:14-23).
    // The candidate test exists only for the probing caller, which is the one
    // site that passes a reference here; search takes the instantiation
    // without it, exactly as `propsearch.c` is compiled without
    // `PROBING_PROPAGATION`.
    const bool scan_conflict =
        propagate_literal<false, false, IGNORE>(k, lit, ignore, ticks);
    if (scan_conflict) {
      conflict = true;
      break;
    }
  }
  k.propagate_head = propagate;
  k.stats.propagations += propagate - saved_propagate;
#ifdef TACTIX_ENABLE_EVENT_TRACE
  if (!k.inprocessing && !k.warming)
    k.diagnostic_search_propagation_ticks += ticks;
#endif
  kernel_charge_ticks(k, ticks);
  return conflict;
}

bool kernel_propagate(Kernel &k, Ref ignore) {
  // Select the candidate-aware scan once per call, not once per trail literal.
  // The local cursor follows the growing used trail during assignments; publish
  // it on both fixpoint and conflict exits as native propsearch.c does.
  const size_t assigned_before = k.trail.size();
  const bool conflict = ignore == INVALID_REF
                            ? propagate_trail<false>(k, INVALID_REF)
                            : propagate_trail<true>(k, ignore);
  // Kissat writes every root-level assignment to the proof as it is made
  // (assign.c).  This is the one home for that publication: a caller which
  // propagates directly (probing, vivification, lucky phases, instantiation)
  // can force a literal at level 0, and a root literal left implicit is one a
  // backward checker knows only as an implied unit.  It cannot cite such a unit
  // for a witnessed addition whose candidate the literal satisfies, so the
  // elaborated LSR loses that hint group.  Not gated on `k.level`: a forced
  // literal is stamped at its true level, and `kernel_emit_root_units` applies
  // the per-literal test.
  if (k.proof &&
      (k.proof->lrat() || k.proof->dsr() || k.proof->streaming_pbp()))
    kernel_emit_root_units(k, assigned_before);
  return conflict;
}

void kernel_propagate_beyond_conflicts(Kernel &k) {
  assert(k.warming);
  kernel_settle_binary_only_debt(k); // full scans, conflicts included
  const uint64_t before = k.stats.propagations;
  const uint32_t saved_propagate = k.propagate_head;
  uint64_t ticks = 0;
  while (k.propagate_head < k.trail.size()) {
    const Lit lit = k.trail[k.propagate_head++];
    if (propagate_literal<false, true>(k, lit, INVALID_REF, ticks))
      k.stats.warming_conflicts++;
  }
  k.stats.propagations += k.propagate_head - saved_propagate;
  kernel_charge_ticks(k, ticks);
  k.stats.warming_propagations += k.stats.propagations - before;
}

// binary-only propagation of `lit` (kissat backbone.c backbone_propagate /
// proprobe.c binary path): follow ONLY binary implications — large watches
// are skipped, never replaced or compacted (binary watches are permanent, so
// there is no watch mutation and no delayed relocation). Far cheaper than the
// full engine; the units it derives are a subset of full propagation's. A
// binary conflict (not_lit, other) is recorded exactly like the full engine.
static bool propagate_literal_binary(Kernel &k, Lit lit,
                                     bool use_current_level,
                                     uint64_t &total_ticks) {
  const Lit not_lit = negate(lit);
  const int8_t *const values = k.values.data();
  const uint32_t *const begin = k.watches.begin(not_lit);
  const uint32_t *p = begin;
  const uint32_t *const end = k.watches.end(not_lit);
  // kissat's `stop_early` (backbone.c:186-193): under the binary-first layout
  // the first large entry ends the binary sub-list, so the whole tail is
  // entries this scan would skip one at a time. Sound because it drops nothing
  // a full scan would have read — this list's bit says no binary follows a
  // large entry.
  const bool stop_early = k.watches.heads[not_lit].binaries_first();
  // Residual pricing (Stats::binprop_*): on the scans the flag did NOT cover,
  // `ordered` records whether the list was in fact binary-first anyway (what a
  // per-LIST order bit would add over the reference's single global flag) and
  // `large_words` the words those scans still spend stepping over large
  // entries.
  bool large_seen = false, ordered = true;
  uint64_t large_words = 0;
  // Not WatchLists::entries: `binprop_words`/`binprop_skipped` below price the
  // word cursor where a successful scan stopped. Kissat returns immediately
  // on a conflicting binary, before charging that literal's scan at all.
  // Kissat also fetches the first large watch with
  // `*p++` before its binary-type test, so an early break charges that first
  // word even though it does not inspect the large clause reference.  An entry
  // iterator stops before the entry it breaks on, so the stride is load-bearing
  // here beyond iteration.
  while (p != end) {
    const Watch w{*p++};
    if (!w.binary()) { // large watch occupies two slots; skipped here
      if (stop_early)
        break;
      large_seen = true;
      large_words += 2;
      p += 1;
      continue;
    }
    ordered = ordered && !large_seen;
    const Lit other = w.other();
    const int8_t other_value = values[other];
    if (other_value > 0)
      continue;
    if (other_value < 0) { // conflict on the binary (not_lit, other)
      k.conflict_valid = true;
      k.conflict_ref = INVALID_REF;
      k.conflict_bin[0] = not_lit;
      k.conflict_bin[1] = other;
      return true;
    }
    if (use_current_level)
      kernel_assign(k, other, k.level, not_lit, true);
    else
      kernel_assign_binary(k, other, not_lit);
  }
  total_ticks += 1 + kernel_cache_lines(p - begin);
#ifdef TACTIX_ENABLE_EVENT_TRACE
  // Per-literal binary-propagation scan journal for backbone tick-parity
  // diagnosis; dormant unless TACTIX_BINSCAN_TRACE names a path and the
  // conflict gate matches.
  if (const char *bs_path = std::getenv("TACTIX_BINSCAN_TRACE")) {
    const char *bs_conflict = std::getenv("SAT_BINSCAN_CONFLICT");
    if (*bs_path &&
        (!bs_conflict || !*bs_conflict ||
         std::strtoull(bs_conflict, nullptr, 10) == k.stats.conflicts)) {
      static std::FILE *bs_out = nullptr;
      static uint64_t bs_ordinal = 0;
      if (!bs_out) {
        bs_out = std::fopen(bs_path, "w");
        if (bs_out) {
          std::setvbuf(bs_out, nullptr, _IOLBF, 0);
          std::fputs("# binscan\t1\n", bs_out);
          std::fputs("# solver\tordinal\tlit\twords\tlistwords\tstop_early\n",
                     bs_out);
        }
      }
      if (bs_out)
        std::fprintf(bs_out, "tactix\t%llu\t%d\t%llu\t%llu\t%d\n",
                     (unsigned long long)bs_ordinal++,
                     kernel_externalize(k, not_lit),
                     (unsigned long long)(p - begin),
                     (unsigned long long)(end - begin),
                     stop_early ? 1 : 0);
    }
  }
#endif
  // Accounted over the prefix actually scanned. `ordered` and `large` describe
  // only the scans the flag did not cover, so they stay the residual measure
  // once the early break is doing its work.
  k.stats.binprop_scans++;
  k.stats.binprop_words += static_cast<uint64_t>(p - begin);
  if (stop_early) {
    k.stats.binprop_flagged++;
    k.stats.binprop_skipped += static_cast<uint64_t>(end - p);
  } else {
    k.stats.binprop_ordered += ordered;
    k.stats.binprop_large += large_words;
  }
  return false;
}

// --- probing propagation (kissat proprobe.c) --------------------------------
// See kernel.hpp for the contract. Deliberate divergence from the reference:
// kissat_probing_propagate is a second instantiation of the same proplit.h
// scan, so kissat's probing does relocate watches and rewrite `searched`, and
// its insulation from search comes from elsewhere. One engine plus an undo log
// is the smaller change and is exact — the propagation is bit-identical to
// kernel_propagate, so the derived literals, their trail order and the recorded
// conflict are the same.

void kernel_probing_begin(Kernel &k, bool rollback_pose) {
  // Production probing rules enter at the root.  Starting an assumption while
  // a root literal is still waiting to be propagated can mask one of its
  // watched clauses; after the assumption is retracted the cursor would then
  // claim a false fixpoint.  Low-level tests also exercise this primitive at
  // decision levels, where the caller deliberately owns the unfinished scan.
  assert(k.level != 0 || kernel_propagation_fixpoint(k));
  ProbingLog &lg = k.probing;
  if (lg.recording)
    kernel_probing_end(k); // defensive: scopes never nest
  lg.recording = true;
  lg.rollback_pose = rollback_pose;
  lg.base_trail = static_cast<uint32_t>(k.trail.size());
  lg.base_head = k.propagate_head;
  lg.base_level = k.level;
  if (++lg.epoch == 0) { // wrap: retire every stamp, then restart at 1
    std::fill(lg.stamp.begin(), lg.stamp.end(), 0u);
    lg.epoch = 1;
  }
}

void kernel_probing_end(Kernel &k) {
  ProbingLog &lg = k.probing;
  if (!lg.recording)
    return;
  lg.recording = false;
  if (lg.rollback_pose && k.trail.size() == lg.base_trail &&
      k.propagate_head == lg.base_head && k.level == lg.base_level) {
    for (size_t i = lg.clauses.size(); i-- > 0;) {
      const ProbingLog::ClauseSnap &s = lg.clauses[i];
      Clause *c = clause_at(k.arena, s.ref);
      c->lits[s.rpos] = c->lits[1];
      c->lits[0] = s.lit0;
      c->lits[1] = s.lit1;
      c->searched = s.searched;
    }
    kernel_charge_ticks(k, lg.clauses.size());
    for (const ProbingLog::ListSnap &s : lg.lists) {
      k.watches.ensure_capacity(s.lit, s.size);
      std::copy(lg.words.begin() + s.off, lg.words.begin() + s.off + s.size,
                k.watches.arena.begin() + k.watches.heads[s.lit].offset);
      WatchHead &h = k.watches.heads[s.lit];
      k.watches.resize_live(s.lit, s.size);
      h.set_layout(s.binaries, s.binaries_first);
      assert(h.has_large() == s.has_large);
      kernel_charge_ticks(k, 1 + kernel_cache_lines(s.size));
    }
  }
  lg.lists.clear();
  lg.words.clear();
  lg.clauses.clear();
}

bool kernel_propagate_probing(Kernel &k, Ref ignore) {
  // A speculative propagation can force a literal at level 0 (D-LEVEL-2 stamps
  // a forced literal at its TRUE level), and the unwind leaves it assigned. It
  // never passes through `propagate_service`, so without this it keeps
  // `unit_ids[v] == 0` — LRAT's line terminator — and truncates the next chain
  // that cites it. Reachable whenever the root is not drained at entry, which
  // V5 Sweep does deliberately: it asserts each environment's units without
  // propagating and then replays the next equivalence.
  const size_t assigned_before = k.trail.size();
  const bool named_root_units =
      k.proof && (k.proof->lrat() || k.proof->dsr() ||
                  k.proof->streaming_pbp());
  bool conflict = false;
  kernel_settle_binary_only_debt(k); // this scan is a full one
  if (!k.probing.recording) {
    conflict = kernel_propagate(k, ignore); // no open scope: nothing to record
  } else {
    const uint32_t saved_propagate = k.propagate_head;
    uint64_t ticks = 0;
    while (k.propagate_head < k.trail.size()) {
      // proprobe.c uses the same post-increment-before-scan cursor contract as
      // search propagation.
      const Lit lit = k.trail[k.propagate_head++];
      const bool found = k.probing.rollback_pose
                             ? propagate_literal<true, false>(k, lit, ignore,
                                                              ticks)
                             : propagate_literal<false, false>(k, lit, ignore,
                                                               ticks);
      if (found) {
        conflict = true;
        break;
      }
    }
    k.stats.propagations += k.propagate_head - saved_propagate;
    kernel_charge_ticks(k, ticks);
  }
  if (named_root_units)
    kernel_emit_root_units(k, assigned_before);
  return conflict;
}

// binary-only propagation to fixpoint or first conflict (V2 Backbone). Same
// contract as kernel_propagate but through the binary-implication graph only.
bool kernel_propagate_binary(Kernel &k, bool use_current_level) {
  // Two obligations this speculative propagator carries because it is not
  // `propagate_service`, and neither of them is the caller's to remember:
  //
  // 1. It advances the SHARED full-propagation cursor while inspecting only
  //    binary watches, so record the debt for the next full propagation
  //    (Kernel::binary_only_head). Without it, a covered literal left on the
  //    trail keeps its large watches unscanned behind a claimed fixpoint.
  // 2. Exactly as in `kernel_propagate_probing`, it can force a literal at
  //    level zero and the caller's unwind may leave that literal assigned. It
  //    never passes through `propagate_service`, so without its own unit line
  //    the literal keeps `unit_ids[v] == 0` — LRAT's line terminator — and
  //    truncates the next chain that cites it.
  if (k.propagate_head < k.binary_only_head)
    k.binary_only_head = k.propagate_head;
  const size_t assigned_before = k.trail.size();
  const bool named_root_units =
      k.proof && (k.proof->lrat() || k.proof->dsr() ||
                  k.proof->streaming_pbp());
  bool conflict = false;
  const uint32_t saved_propagate = k.propagate_head;
  uint64_t ticks = 0;
  while (k.propagate_head < k.trail.size()) {
    // backbone.c also publishes its post-incremented local cursor on conflict.
    const Lit lit = k.trail[k.propagate_head++];
    if (propagate_literal_binary(k, lit, use_current_level, ticks)) {
      conflict = true;
      break;
    }
  }
  k.stats.propagations += k.propagate_head - saved_propagate;
  kernel_charge_ticks(k, ticks);
  if (named_root_units)
    kernel_emit_root_units(k, assigned_before);
  return conflict;
}

// Garbage collection after kissat/src/collect.c (MIT), simplified: copy
// live clauses into a fresh arena in order, leave a forwarding pointer in
// the old header (`searched` is dead there), then fix reasons and rebuild
// the large-watch entries. Rebuilt clauses select their best current watched
// pair into lits[0]/lits[1]. With a nonzero Reduce watermark, match Kissat's
// physical sparse split exactly: retain watches only for records before
// `first_reducible`, then rebuild every surviving record in the physical tail.
// An arena policy can place irredundant clauses in that tail, but Kissat still
// flushes and rewatches them; treating redundancy as the boundary preserves a
// different blocker order and replacement cursor.
static int8_t move_best_watch_to_front(Kernel &k, Clause *c, uint32_t start,
                                       bool satisfied_is_enough) {
  Lit a = c->lits[start];
  int8_t u = k.values[a];
  if (!u || (u > 0 && satisfied_is_enough))
    return u;

  uint32_t pos = 0;
  Lit best = a;
  uint32_t best_level =
      u ? k.assigned[var_of(a)].level : UINT32_MAX;
  for (uint32_t i = start + 1; i < c->size; ++i) {
    const Lit b = c->lits[i];
    const int8_t v = k.values[b];
    if (!v || (v > 0 && satisfied_is_enough)) {
      best = b;
      pos = i;
      u = v;
      break;
    }
    const uint32_t level = k.assigned[var_of(b)].level;
    const bool better =
        (u < 0 && v > 0) ? true
        : (u > 0 && v < 0) ? false
        : (u < 0)           ? best_level < level
                            : best_level > level;
    if (better) {
      best = b;
      pos = i;
      u = v;
      best_level = level;
    }
  }
  if (pos) {
    c->lits[start] = best;
    c->lits[pos] = a;
  }
  return u;
}

static void sort_clause_watches_like_kissat(Kernel &k, Clause *c) {
  const int8_t first = move_best_watch_to_front(k, c, 0, false);
  if (c->size > 2)
    move_best_watch_to_front(k, c, 1, first >= 0);
}

// Stable LSD radix ordering over each clause's current logical position.
// Kissat obtains this order for free because its arena shortens clauses in
// place. Tactix appends variable-width replacements and records their source
// position until the next collection materializes it. Unlike immutable proof
// IDs, these positions naturally retain every earlier sparse-collection
// partition. Comparison sorting the multi-million clause image made dense
// entry and sparse resume dominate useful rule work.
void kernel_sort_refs_by_logical_clause_position(const Kernel &k,
                                                 std::vector<Ref> &refs) {
  if (refs.size() < 2 || k.clause_position_alias.empty())
    return; // allocation order already is the logical order

  bool ordered = true;
  uint64_t previous = 0;
  for (size_t i = 0; i < refs.size(); ++i) {
    const uint64_t key = kernel_clause_position(k, refs[i]);
    if (i && key < previous)
      ordered = false;
    previous = key;
  }
  if (ordered)
    return;

  std::vector<uint64_t> keys;
  keys.reserve(refs.size());
  for (const Ref ref : refs)
    keys.push_back(kernel_clause_position(k, ref));
  std::vector<Ref> ref_scratch(refs.size());
  std::vector<uint64_t> key_scratch(keys.size());
  // Positions are arena `Ref`s, hence exactly 32 bits. Four passes are enough;
  // the former proof-ID key needed eight.
  for (unsigned shift = 0; shift < 32; shift += 8) {
    std::array<size_t, 256> offsets{};
    for (const uint64_t key : keys)
      offsets[(key >> shift) & 0xffu]++;
    size_t sum = 0;
    for (size_t &offset : offsets) {
      const size_t count = offset;
      offset = sum;
      sum += count;
    }
    for (size_t i = 0; i < refs.size(); ++i) {
      const size_t bucket = (keys[i] >> shift) & 0xffu;
      const size_t dst = offsets[bucket]++;
      ref_scratch[dst] = refs[i];
      key_scratch[dst] = keys[i];
    }
    refs.swap(ref_scratch);
    keys.swap(key_scratch);
  }
}

static void rebuild_dense_large_occurrences(Kernel &k, OccIndex &occ) {
  const size_t nlits = 2 * static_cast<size_t>(k.nvars);
  if (!occ.track_order)
    // The previous Eliminate phase may have populated this optional image.
    // Probe has no consumer for it; release its payload rather than retain
    // several GiB of cold capacity across phases.
    occ.ordered.release();
  else
    occ.large.release();

  // One arena walk both collects the irredundant references and counts each
  // literal's occurrences, so every slice below is laid out at its exact
  // final length and the fill relocates nothing.  The reference list is
  // kernel-owned and keeps its allocation across rebuilds; growing a fresh one
  // per rebuild put its geometric reallocation in the instruction profile
  // (benchmarks/reports/2026-09-02-unit-cost-and-fidelity-stack.md 3.47).
  std::vector<Ref> &order = k.dense_order_scratch;
  order.clear();
  if (occ.track_order)
    occ.ordered.begin_exact_layout(nlits);
  else
    occ.large.begin_exact_layout(nlits);
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && !c->redundant()) {
      order.push_back(ref);
      for (const Lit lit : *c) {
        if (occ.track_order)
          occ.ordered.count(lit);
        else
          occ.large.count(lit);
      }
    }
    ref += clause_actual_words(c);
  }
  // Eliminate's optional BVE image retains the current binary vectors and
  // appends every full irredundant large occurrence. Large sparse watches are
  // not part of this image; later clause creation appends in connected order.
  if (occ.track_order)
    for (Lit lit = 0; lit < nlits; ++lit)
      occ.ordered.count(lit, k.watches.binary_size(lit));
  if (occ.track_order) {
    occ.ordered.finish_exact_layout();
    for (Lit lit = 0; lit < nlits; ++lit)
      for (const WatchEntry watch : k.watches.entries(lit))
        if (watch.binary())
          occ.ordered.push(lit, OccIndex::Entry::from_binary(watch.other()));
  } else
    occ.large.finish_exact_layout();
  // Kissat's dense lists follow its current irredundant arena order. Tactix's
  // generation-local source positions retain that order across append-only
  // replacement records until collection materializes it.
  kernel_sort_refs_by_logical_clause_position(k, order);
  for (const Ref ref : order)
    for (const Lit lit : *clause_at(k.arena, ref))
      occ.push_large(lit, ref);
}

void kernel_rebuild_dense_occurrences(Kernel &k) {
  assert(k.dense);
  rebuild_dense_large_occurrences(k, *k.dense);
}

void kernel_dense_add_large_occurrence(Kernel &k, Lit lit, Ref ref) {
  assert(k.dense);
  k.dense->push_large(lit, ref);
}

void kernel_dense_remove_large_occurrence(Kernel &k, Lit lit, Ref ref) {
  assert(k.dense);
  if (k.dense->track_order) {
    const bool removed = k.dense->ordered.erase_large_once(lit, ref);
    assert(removed);
    (void)removed;
  } else {
    const auto occ = k.dense->large[lit];
    const auto at = std::find(occ.begin(), occ.end(), ref);
    assert(at != occ.end());
    k.dense->large.erase_at(lit, static_cast<size_t>(at - occ.begin()));
  }
}

void kernel_promote_large_irredundant(Kernel &k, Ref ref) {
  Clause *const c = clause_at(k.arena, ref);
  assert(!c->garbage() && c->redundant());
  c->set_redundant(false);
  c->set_glue(0);
  k.live_irredundant_large++;
  kernel_observe_solver_clause(k, SolverPopulationEventKind::MetadataUpdate,
                               SolverClauseKind::Large, c->id(), c->lits,
                               c->size, 1, false);
  if (!k.dense)
    return;
  for (const Lit lit : *c) {
    kernel_dense_add_large_occurrence(k, lit, ref);
    kernel_bve_occurrence_add(k, lit);
  }
}

void kernel_defrag_vectors(Kernel &k) {
  k.watches.defrag_if_needed();
  if (!k.dense)
    return;
  if (k.dense->track_order)
    k.dense->ordered.defrag_if_needed();
  else
    k.dense->large.defrag_if_needed();
}

void kernel_collect_garbage(Kernel &k, Ref watch_start,
                            bool sparse_collection,
                            bool preserve_eager_recent) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
  // Construct first so the final sample follows destruction of the old arena
  // and every collector-local vector, without changing their lifetimes.
  struct MemoryExit {
    const Kernel &kernel;
    ~MemoryExit() {
      event_trace_memory_checkpoint(kernel, "Collect", "exit");
    }
  } memory_exit{k};
  event_trace_memory_checkpoint(k, "Collect", "entry");
#endif
  assert(!k.bve_occurrence_count_image);
  // No probing scope may be open: this relocates every Ref, so an outstanding
  // snapshot would roll pre-collection watch words (and ClauseSnap refs) back
  // over the compacted arena. Long an implicit assumption of the probing log;
  // stated here because the per-list order bits rest on it too: restoring an
  // older snapshot after collection could otherwise invalidate them.
  assert(!k.probing.recording);
  // The occurrence index can outlive a rule-local sparse resume.  It is only
  // a physical dense collection while large watches are disconnected.  Once
  // sparse watches are connected they are the authoritative representation,
  // so collection must take the ordinary rewatch path even if the phase-local
  // occurrence cache remains live.  Treating the cache alone as dense
  // preserves stale clause pose and creates a transition Kissat never has.
  // The converse is independent too: an explicit caller can disconnect large
  // watches without installing an occurrence cache. Do not silently reconnect
  // them while leaving the connection flag false.
  const bool dense_collection = !k.large_watches_connected;
  const Ref old_reducible = k.first_reducible;
  Ref packed_reducible = INVALID_REF;
  // Genuine Kissat collections reset the raw-reference recent-learned
  // register (collect.c:274/514/702).  Some Tactix rules need an additional
  // packing pass solely because replacements are append-only here; if the
  // corresponding Kissat operation does not collect, preserve its register
  // semantics by forwarding live entries through that implementation-only
  // pass.  Garbage entries must never read `searched` as a forwarding pointer
  // (the corpus-003 shape), so they are invalidated below.
  if (!preserve_eager_recent) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_detail(k, "eager_reset_collect",
                       static_cast<int>(sparse_collection));
#endif
    for (Ref &r : k.eager_recent)
      r = INVALID_REF;
  }
  // A sparse watermark must fall exactly on an arena-record boundary.  Prefix
  // garbage is legal in Tactix because clauses retired by an earlier rule can
  // remain marked until Reduce; its live watch entries are forwarded below.
  if (watch_start > k.arena.size())
    watch_start = 0;
  for (Ref ref = 0; ref < watch_start;) {
    const Clause *c = clause_at(k.arena, ref);
    ref += clause_actual_words(c);
    if (ref > watch_start) {
      watch_start = 0;
      break;
    }
  }
  sparse_collection |= watch_start != 0;
  const uint64_t sparse_cut =
      watch_start ? kernel_clause_position(k, watch_start) : 0;
  // Mark aliased, already-dead sources in their searched word so the sparse
  // prefix cannot copy them; live replacements receive separate forwarding.
  // With no aliases, every clause owns its position and this scan is inert.
  // Sentinels die with the old arena, preserving proof ids and live cursors
  // (docs/dev/implementation-notes.md §5).
  if (sparse_collection && !k.clause_position_alias.empty())
    for (Ref ref = 0; ref < k.arena.size();) {
      Clause *const c = clause_at(k.arena, ref);
      if (!c->garbage()) {
        const uint64_t position = kernel_clause_position(k, ref);
        if (position != ref) {
          assert(position <= UINT32_MAX && position < k.arena.size());
          Clause *const source =
              clause_at(k.arena, static_cast<Ref>(position));
          assert(source->garbage());
          source->searched = INVALID_REF;
        }
      }
      ref += clause_actual_words(c);
    }
  const auto retain_record = [&k, sparse_collection,
                              sparse_cut](Ref ref, const Clause *c) {
    if (!c->garbage())
      return true;
    // A whole-arena collection retires every garbage record, so the position
    // lookup below (a hash probe plus a second clause dereference) would only
    // be discarded. Only the sparse sweep has a prefix to preserve.
    if (!sparse_collection)
      return false;
    const uint64_t position = kernel_clause_position(k, ref);
    assert(position <= UINT32_MAX && position < k.arena.size());
    const Clause *const source =
        clause_at(k.arena, static_cast<Ref>(position));
    return position < sparse_cut && source->searched != INVALID_REF;
  };
  // One walk, not two: the retained-word total and the survivor list come out
  // of the same pass, so the predicate above runs once per record. The order
  // vector is kernel-owned and keeps its allocation across collections.
  size_t retained_words = 0;
  std::vector<Ref> &order = k.collect_order_scratch;
  order.clear();
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(c);
    if (retain_record(ref, c)) {
      // Collection packs the semantic image and retires any stable-allocation
      // tail left by an in-place shrink. Sparse prefix garbage remains part of
      // the physical image, just as it does before Kissat's sweep watermark.
      retained_words += clause_words(c->size);
      order.push_back(ref);
    }
    ref += words;
  }
  assert(retained_words <= UINT32_MAX);
#ifdef TACTIX_ENABLE_EVENT_TRACE
  event_trace_memory_checkpoint(k, "Collect", "before-reserve");
#endif
  Arena fresh;
  // Appending rewrites can leave a large garbage suffix. Reserving the old
  // arena size here needlessly kept that suffix alive in both arenas during
  // collection; the live-word prepass is the exact Kissat-style compacted
  // image and also prevents geometric growth while copying it.
  fresh.reserve(retained_words);
#ifdef TACTIX_ENABLE_EVENT_TRACE
  event_trace_memory_checkpoint(k, "Collect", "before-sort",
                                fresh.capacity() * sizeof(Arena::value_type));
#endif
  uint32_t packed_watch_start = 0;
  // Arena-order policies belong to whole-arena/inprocessing collection.
  // Kissat's sparse collector preserves the prefix, sweeps the tail in its
  // existing order, then stably moves surviving redundant clauses behind all
  // surviving irredundant clauses in that tail (collect.c:
  // move_redundant_clauses_to_the_end).  This partition is observable when
  // the tail is rewatched, and is independent of the optional policy order.
  if (sparse_collection) {
    // Reduce is not an arena-policy checkpoint. Even the explicit CaDiCaL
    // diagnostic arm must leave Kissat's sparse order alone.
    kernel_sort_refs_by_logical_clause_position(k, order);
    const auto tail = std::lower_bound(
        order.begin(), order.end(), sparse_cut,
        [&k](Ref ref, uint64_t cut) {
          return kernel_clause_position(k, ref) < cut;
        });
    std::stable_partition(tail, order.end(), [&k](Ref ref) {
      return !clause_at(k.arena, ref)->redundant();
    });
  } else if (k.arena_ordering) {
    k.arena_ordering->order(k, order);
  } else {
    // Production emulates Kissat's in-place clause replacements: a packed
    // replacement returns to its source's current position instead of
    // retaining Tactix's append-only allocation position. The explicit
    // CaDiCaL diagnostic arm above remains the only alternative whole-arena
    // permutation. Sparse collection deliberately stays on the separate
    // current-position path above.
    kernel_sort_refs_by_logical_clause_position(k, order);
  }
#ifdef TACTIX_ENABLE_EVENT_TRACE
  event_trace_memory_checkpoint(k, "Collect", "after-sort",
                                fresh.capacity() * sizeof(Arena::value_type));
#endif
#ifndef NDEBUG
  std::vector<uint8_t> seen(k.arena.size());
  for (const Ref ref : order) {
    assert(ref < k.arena.size() && !seen[ref]);
    assert(retain_record(ref, clause_at(k.arena, ref)));
    seen[ref] = 1;
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    assert(seen[ref] || !retain_record(ref, c));
    ref += clause_actual_words(c);
  }
#endif
  for (const Ref ref : order) {
    Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    const bool retained_sparse_watch =
        sparse_collection && kernel_clause_position(k, ref) < sparse_cut;
    if (retained_sparse_watch) {
      assert(packed_watch_start <= UINT32_MAX - words);
      packed_watch_start += words;
    }
    const Ref nref = static_cast<Ref>(fresh.size());
    // Genuine collection refreshes Kissat's first-reducible register. An
    // implementation-only packing pass instead forwards the old logical cut,
    // even if its original record was promoted or retired. The first surviving
    // tail record is the new boundary when the original record disappeared.
    if (c->redundant() ||
        (preserve_eager_recent && old_reducible != INVALID_REF &&
         kernel_clause_position(k, ref) >= old_reducible))
      packed_reducible = std::min(packed_reducible, nref);
    fresh.insert(fresh.end(), k.arena.begin() + ref,
                 k.arena.begin() + ref + words);
    // Kissat's sparse sweep starts at `watch_start` in its current arena: only
    // that logical prefix avoids rewatching and keeps its replacement-search
    // cursor. A packed Tactix replacement can physically live at the tail.
    // Whole-arena collection has `watch_start == 0` and resets every clause.
    Clause *const packed = clause_at(fresh, nref);
    packed->flags &= ~Clause::SHRUNKEN;
    // Dense elimination collection in Kissat is a byte-semantic arena copy:
    // it preserves both literal order and the replacement-search cursor, and
    // reconnects full occurrences only at the beginning of the next forward
    // round. Sparse/ordinary collection instead rewatches here and resets the
    // cursor, except for the retained sparse prefix.
    packed->searched =
        (dense_collection || retained_sparse_watch) ? c->searched : 2;
    c->searched = nref;
  }
#ifdef TACTIX_ENABLE_EVENT_TRACE
  event_trace_memory_checkpoint(k, "Collect", "after-copy",
                                fresh.capacity() * sizeof(Arena::value_type));
#endif
  for (const Lit lit : k.trail) {
    Assigned &a = k.assigned[var_of(lit)];
    if (!a.binary && a.reason != DECISION_REASON && a.reason != UNIT_REASON) {
      const Clause *c = clause_at(k.arena, a.reason);
      if (c->garbage()) {
        // The reason clause is being deleted. That is only possible for a
        // ROOT-fixed literal: inprocessing rules delete root-satisfied
        // clauses (and a root reason IS root-satisfied) only at a quiescent
        // root, so no higher-level reason is ever garbage; Reduce protects
        // reasons. A root-fixed literal stays assigned and needs no reason —
        // it is a root unit (emit_root_units already gave it an LRAT unit id
        // when it was propagated). Forwarding a garbage clause would read a
        // bogus `searched` and leave a dangling reason (corpus 003).
        assert(a.level == 0);
        a.reason = UNIT_REASON;
      } else {
        a.reason = c->searched;
      }
    }
  }
  // Same forwarding contract as the trail reasons above: `searched` holds a
  // forwarding pointer only for a record this collection COPIED. A garbage
  // record keeps its search cursor there (a small literal index), so reading it
  // as a Ref lands inside some unrelated clause's literals.
  if (k.pending.valid && k.pending.ref != INVALID_REF) {
    const Clause *c = clause_at(k.arena, k.pending.ref);
    // The Analyze->Backjump handoff is reason-locked by every collector caller,
    // so it must still be live here; there is no meaningful recovery from a
    // pending clause that was retired mid-handoff.
    assert(!c->garbage() && "pending learned clause retired before Backjump");
    k.pending.ref = c->searched;
  }
  if (k.conflict_valid && k.conflict_ref != INVALID_REF) {
    const Clause *c = clause_at(k.arena, k.conflict_ref);
    // Conflict-state Reduce marks this clause as a reason before collection,
    // so its proof antecedent remains live and receives an ordinary forwarding
    // pointer.  A binary conflict has INVALID_REF and needs no forwarding.
    assert(!c->garbage() && "conflict clause retired before analysis");
    k.conflict_ref = c->searched;
  }
  if (preserve_eager_recent)
    for (Ref &ref : k.eager_recent) {
      if (ref == INVALID_REF)
        continue;
      const Clause *const c = clause_at(k.arena, ref);
      ref = c->garbage() ? INVALID_REF : c->searched;
    }
  {
    // An OTFS reason can be retired by another rule before Backjump drains the
    // register; `analyze_retire_otfs_reasons` already skips a garbage record,
    // so dropping it here is exactly what a correctly forwarded entry would
    // have done — and it is the only way to avoid forwarding a search cursor.
    size_t kept = 0;
    for (const Ref ref : k.otfs_retired) {
      const Clause *c = clause_at(k.arena, ref);
      if (!c->garbage())
        k.otfs_retired[kept++] = c->searched;
    }
    k.otfs_retired.resize(kept);
  }
  const auto forward_large_ref = [&k](Ref ref) {
    return clause_at(k.arena, ref)->searched;
  };
  for (Lit lit = 0; lit < 2 * k.nvars; lit++) {
    // Kissat tests the literal's fixedness and the collection mode once per
    // LIST (`collect.c:flush_watched_clauses_by_literal`), not once per entry.
    // Both are loop-invariant here, and when neither holds every large entry
    // is dropped, so the position lookup -- a hash probe plus two further
    // clause dereferences -- has nothing to decide.
    // Stripping a root-fixed watch is valid only after root collection has
    // simplified every clause it can affect. An unresolved search conflict
    // deliberately skips that root transition in Reduce: its live prefix
    // clauses must keep both watches until the conflict is discharged and
    // root propagation can rescan them. Otherwise a later Restart can expose
    // a unit clause with no watch on either falsified literal.
    if (sparse_collection &&
        (k.conflict_valid || kernel_root_value(k, lit) == 0))
      k.watches.compact_map_large(lit, [&k, sparse_cut](WatchEntry w) {
        if (w.binary())
          return true;
        const uint64_t position = kernel_clause_position(k, w.ref());
        assert(position <= UINT32_MAX && position < k.arena.size());
        if (position >= sparse_cut)
          return false;
        if (!clause_at(k.arena, w.ref())->garbage())
          return true;
        return clause_at(k.arena, static_cast<Ref>(position))->searched !=
               INVALID_REF;
      }, forward_large_ref);
    else
      k.watches.compact_map_large(
          lit, [](WatchEntry w) { return w.binary(); }, forward_large_ref);
    // The compaction is stable: every current-prefix entry keeps its exact
    // position and cached blocker. Sorting these survivors would erase
    // search-visible watch relocation.
  }
  k.arena.swap(fresh);
#ifdef TACTIX_ENABLE_EVENT_TRACE
  event_trace_memory_checkpoint(k, "Collect", "after-swap",
                                fresh.capacity() * sizeof(Arena::value_type));
#endif
  // Every replacement alias named a position in the old arena. The packed
  // arena now embodies the selected order directly; retaining aliases would
  // make a later sparse partition snap clauses back to a superseded
  // generation. All rebuilds below therefore consume the new physical arena.
  k.clause_position_alias.clear();
  k.first_reducible = packed_reducible;
  // GC relocated every Ref: the dense occurrence index (increment 2) is
  // rebuilt from the compacted arena in lock-step with the watches below (it
  // held offsets into the old arena, now stale).  The rebuild clears logical
  // sizes while retaining Kissat-shaped per-literal allocation capacity.
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    if (!dense_collection &&
        (!sparse_collection || ref >= packed_watch_start)) {
      // collect.c:rewatch_clauses selects the best current watched pair before
      // appending the relocated clause to its two watch lists.
      sort_clause_watches_like_kissat(k, c);
      k.watches.push_large(c->lits[0], c->lits[1], ref);
      k.watches.push_large(c->lits[1], c->lits[0], ref);
    }
    ref += clause_actual_words(c);
  }
  // Dense collection preserves clauses' own pose but relocates every Ref.
  // Rebuild the separate full-occurrence image in compacted arena order; do
  // not install sparse two-watches or sort literals until sparse resume.
  if (k.dense)
    rebuild_dense_large_occurrences(k, *k.dense);
#ifdef TACTIX_ENABLE_EVENT_TRACE
  event_trace_memory_checkpoint(k, "Collect", "after-reindex",
                                fresh.capacity() * sizeof(Arena::value_type));
#endif
  // Every surviving binary was kept in place by the strip above and every large
  // entry was appended after it, so the binary-first layout now holds — the
  // same way kissat gets it out of `kissat_watch_large_clauses`
  // (substitute.c:594-596), as a byproduct rather than as a sort.
  k.arena_generation++; // every Ref taken before this call is now stale
}

// Re-enter sparse search after an inprocessing phase in arena order.  Kissat's
// dense-mode simplifiers flush large watches and `kissat_watch_large_clauses`
// rebuilds them on resume; retaining the pre-phase lists instead leaks Lucky
// and simplifier scan order into the following search trajectory.
void kernel_rebuild_large_watches(Kernel &k, bool flush_eliminated) {
  for (Lit lit = 0; lit < 2 * k.nvars; lit++)
    k.watches.compact(lit, [](WatchEntry w) { return w.binary(); });
  std::vector<Ref> order;
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(c);
    if (flush_eliminated && !c->garbage()) {
      bool collect = false;
      for (const Lit lit : *c)
        if (kernel_root_value(k, lit) > 0 ||
            kernel_var_retired(k, var_of(lit))) {
          collect = true;
          break;
        }
      if (collect)
        kernel_delete_clause(k, c);
    }
    if (!c->garbage())
      order.push_back(ref);
    ref += words;
  }
  // Kissat traverses its current physical arena. Tactix implements the same
  // in-place rewrite semantics with append-only replacement records plus a
  // generation-local source-position alias. Thus the logical position is the
  // current Kissat arena position at every reconnect, not only after selected
  // phases or after a Vivify band has already changed the database. Using raw
  // allocation order here reverses otherwise identical clauses and makes
  // propagation select a different unit/reason.
  kernel_sort_refs_by_logical_clause_position(k, order);
  for (const Ref ref : order) {
    Clause *c = clause_at(k.arena, ref);
      // `kissat_watch_large_clauses` does not merely reconnect the existing
      // pair: it first runs `kissat_sort_literals` under the current root
      // assignment. In particular, root-false literals must not remain in
      // front of unassigned literals after Probe/Eliminate. The selected pair
      // is search-visible state and can control which valid antecedent records.
      sort_clause_watches_like_kissat(k, c);
      c->searched = 2;
      k.watches.push_large(c->lits[0], c->lits[1], ref);
      k.watches.push_large(c->lits[1], c->lits[0], ref);
  }
  k.large_watches_connected = true;
}

void kernel_resume_walk_watches(Kernel &k) {
  // Walk's non-null dense entry deletes root-satisfied binaries before saving
  // and restoring the root-open population.  `dense.c:flush_large_watches`
  // scans Kissat's compact literals globally and saves a pair only from its
  // lower compact endpoint.  Sparse resume then replays that stack without a
  // sort.  Tactix's stable internal identities can contain holes and therefore
  // must use permanent import order as the compact-literal key; numeric kernel
  // order preserves the wrong mirror pose after Walk.
  std::vector<Var> ordered_variables;
  kernel_vars_in_import_order(k, ordered_variables);
  std::vector<std::pair<Lit, Lit>> satisfied;
  std::vector<uint64_t> lit_rank(2 * static_cast<size_t>(k.nvars));
  for (Lit l = 0; l < static_cast<Lit>(2 * k.nvars); l++)
    lit_rank[l] = kernel_import_lit_order(k, l);
  for (const Var variable : ordered_variables)
    for (uint32_t sign = 0; sign < 2; sign++) {
      const Lit lit = make_lit(variable, sign != 0);
      for (const WatchEntry watch : k.watches.entries(lit)) {
        if (!watch.binary())
          continue;
        const Lit other = watch.other();
        if (lit_rank[lit] >= lit_rank[other])
          continue;
        if (kernel_root_value(k, lit) > 0 ||
            kernel_root_value(k, other) > 0)
          satisfied.emplace_back(lit, other);
      }
    }
  // Same reason as the sparse collector above: the scan met every copy, so
  // the count travels with the pair rather than costing another prefix walk.
  // The encounter order the deletions run in is the one the surviving-pair
  // pass below mirrors, so group by sorted index and restore it.
  {
    std::vector<uint32_t> order(satisfied.size());
    for (uint32_t i = 0; i < order.size(); i++)
      order[i] = i;
    std::sort(order.begin(), order.end(), [&](uint32_t x, uint32_t y) {
      return satisfied[x] != satisfied[y] ? satisfied[x] < satisfied[y] : x < y;
    });
    std::vector<std::pair<uint32_t, uint32_t>> groups; // first index, copies
    for (size_t at = 0; at < order.size();) {
      size_t end = at;
      while (end < order.size() && satisfied[order[end]] == satisfied[order[at]])
        end++;
      groups.emplace_back(order[at], static_cast<uint32_t>(end - at));
      at = end;
    }
    std::sort(groups.begin(), groups.end());
    for (const auto &[first, copies] : groups)
      kernel_delete_binary_all_copies_known(k, satisfied[first].first,
                                            satisfied[first].second, copies);
  }
  // Save each surviving pair in that same compact-literal encounter order.
  // Unlike Congruence, Walk does not sort equal-first runs; the current watch
  // order of a pair whose lower endpoint is shared is retained verbatim.
  std::vector<std::pair<Lit, Lit>> binaries;
  for (const Var variable : ordered_variables)
    for (uint32_t sign = 0; sign < 2; sign++) {
      const Lit lit = make_lit(variable, sign != 0);
      for (const WatchEntry watch : k.watches.entries(lit))
        if (watch.binary() && lit_rank[lit] < lit_rank[watch.other()])
          binaries.emplace_back(lit, watch.other());
    }

  std::vector<uint32_t>().swap(k.watches.arena);
  k.watches.holes = 0;
  for (WatchHead &head : k.watches.heads)
    head = WatchHead{};
  for (const auto &[a, b] : binaries) {
    k.watches.heads[a].add_binary_word();
    k.watches.heads[b].add_binary_word();
  }
  std::vector<Ref> large_order;
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(c);
    if (!c->garbage())
      large_order.push_back(ref);
    ref += words;
  }
  // Kissat resumes by scanning its physical arena.  Tactix's physical arena
  // can differ after append-only replacements, while the generation-local
  // alias is the exact current source-arena position. Reconstruct that order before
  // sizing and filling the exact watch image.
  kernel_sort_refs_by_logical_clause_position(k, large_order);
  for (const Ref ref : large_order) {
    Clause *c = clause_at(k.arena, ref);
    sort_clause_watches_like_kissat(k, c);
    c->searched = 2;
    k.watches.heads[c->lits[0]].add_large_words();
    k.watches.heads[c->lits[1]].add_large_words();
  }
  uint64_t total_words = 0;
  for (WatchHead &head : k.watches.heads) {
    assert(total_words + head.size <= UINT32_MAX);
    head.offset = static_cast<uint32_t>(total_words);
    head.capacity = head.size;
    total_words += head.size;
    head.size = 0;
  }
  k.watches.arena.resize(static_cast<size_t>(total_words));
  const auto push_word = [&](Lit lit, uint32_t word) {
    WatchHead &head = k.watches.heads[lit];
    assert(head.size < head.capacity);
    k.watches.arena[head.offset + head.size++] = word;
  };
  for (const auto &[a, b] : binaries) {
    push_word(a, binary_watch(b).raw);
    push_word(b, binary_watch(a).raw);
  }
  for (const Ref ref : large_order) {
    Clause *c = clause_at(k.arena, ref);
    const Lit first = c->lits[0], second = c->lits[1];
    push_word(first, blocking_watch(second).raw);
    push_word(first, ref);
    push_word(second, blocking_watch(first).raw);
    push_word(second, ref);
    assert(k.watches.heads[first].has_large());
    assert(k.watches.heads[second].has_large());
  }
}

void kernel_canonicalize_binary_watches(Kernel &k,
                                        bool count_congruence_rebuild,
                                        const std::vector<std::pair<Lit, Lit>> *
                                            binary_suffix,
                                        std::vector<std::pair<Lit, Lit>> *
                                            binary_population) {
  // Kissat Congruence enters dense mode with a non-null binary save stack.
  // `dense.c:flush_large_watches` walks literals globally, keeps the first
  // copy of each binary, and saves a pair only at its smaller literal. Then
  // `extract_binaries` sorts every equal-first run by its second literal before
  // sparse resume recreates both mirrors. Surviving pairs use (min,max) order,
  // not first occurrence. Tactix maps compact indices to import order, keeping
  // that reference-visible order for capped Sweep environments.
  // Every head maintains its exact binary-prefix length.  Summing those
  // lengths is the same count as decoding the complete watch image, and is
  // also the representation-level statistic Kissat uses.  Avoid a redundant
  // pass over both mirrors immediately before the collection pass below.
  std::vector<std::pair<Lit, Lit>> binaries;
  std::vector<uint8_t> seen;
  std::vector<Var> ordered_variables;
  // kissat compares raw compacted literals here; Tactix's stable identities
  // need their import rank, which is two lookups and a branch per call.  Read
  // it once per literal so the scan and the per-run sort below index an array
  // instead of calling into the map for every entry and every comparison.
  std::vector<uint64_t> lit_rank(2 * static_cast<size_t>(k.nvars));
  for (Lit l = 0; l < static_cast<Lit>(2 * k.nvars); l++)
    lit_rank[l] = kernel_import_lit_order(k, l);
  if (binary_population) {
    // Congruence's dense entry has already saved every unique root-open pair
    // in first-literal import order. Consume that owner-style save stack
    // instead of decoding both sparse-watch mirrors again.
    binaries.swap(*binary_population);
  } else {
    // Every retained binary has two mirrored watch entries. Reserve their
    // upper bound so a large dense instance avoids geometric vector growth.
    binaries.reserve(k.watches.binary_clauses());
    // Deduplication is local to one watch list. A byte per literal plus a
    // second pass that clears touched bits preserves first-copy semantics.
    seen.assign(2 * k.nvars, 0);
    kernel_vars_in_import_order(k, ordered_variables);
    for (const Var variable : ordered_variables)
      for (uint32_t sign = 0; sign < 2; sign++) {
        const Lit lit = make_lit(variable, sign != 0);
        for (const WatchEntry w : k.watches.entries(lit)) {
          if (!w.binary())
            continue;
          const Lit other = w.other();
          // Sparse resume re-appends the entry-time saved pairs without
          // consulting values. Duplicates go; later root values do not.
          if (lit_rank[lit] >= lit_rank[other] || seen[other])
            continue;
          seen[other] = 1;
          binaries.emplace_back(lit, other);
        }
        for (const WatchEntry w : k.watches.entries(lit))
          if (w.binary() && lit_rank[lit] < lit_rank[w.other()])
            seen[w.other()] = 0;
      }
  }
  // The outer scan already appends groups in first-literal import order.
  // Kissat's extract_binaries sorts only each equal-first run by the other
  // literal: comparison sort below 32, stable byte-radix otherwise.  Mirror
  // that owner algorithm and reuse one temporary buffer across large runs;
  // std::sort on a high-degree literal turns this linear dense transition into
  // O(m log m) work.
  std::vector<std::pair<Lit, Lit>> radix_tmp;
  for (size_t begin = 0; begin < binaries.size();) {
    size_t end = begin + 1;
    while (end < binaries.size() &&
           binaries[end].first == binaries[begin].first)
      end++;
    const size_t size = end - begin;
    // Sparse resume publishes each run in canonical partner order.  On a
    // later dense round most runs therefore remain canonical unless an
    // intervening rule appended a new binary to that literal.  Detect that
    // exact fixed point before invoking either owner sorting path.  This also
    // supplies the radix bounds in the only case that still needs them.
    bool ordered = true;
    uint64_t previous = 0, lower = UINT64_MAX, upper = 0;
    for (size_t i = begin; i < end; i++) {
      const uint64_t rank = lit_rank[binaries[i].second];
      ordered &= previous <= rank;
      previous = rank;
      lower &= rank;
      upper |= rank;
    }
    if (ordered) {
      begin = end;
      continue;
    }
    if (size < 32) {
      std::sort(binaries.begin() + begin, binaries.begin() + end,
                [&lit_rank](const auto &a, const auto &b) {
                  return lit_rank[a.second] < lit_rank[b.second];
                });
    } else {
      radix_tmp.resize(size);
      auto *const original = binaries.data() + begin;
      auto *source = original;
      auto *target = radix_tmp.data();
      for (unsigned shift = 0; shift < 64; shift += 8) {
        if (((lower >> shift) & 255u) == ((upper >> shift) & 255u))
          continue;
        std::array<size_t, 256> count{};
        bool digit_sorted = true;
        unsigned previous = 0;
        for (size_t i = 0; i < size; i++) {
          const unsigned digit =
              static_cast<unsigned>((lit_rank[source[i].second] >> shift) &
                                    255u);
          digit_sorted &= previous <= digit;
          previous = digit;
          count[digit]++;
        }
        if (digit_sorted)
          continue;
        size_t position = 0;
        for (size_t &bucket : count) {
          const size_t entries = bucket;
          bucket = position;
          position += entries;
        }
        for (size_t i = 0; i < size; i++) {
          const unsigned digit =
              static_cast<unsigned>((lit_rank[source[i].second] >> shift) &
                                    255u);
          target[count[digit]++] = source[i];
        }
        source = target;
        target = source == original ? radix_tmp.data() : original;
      }
      if (source != original)
        std::copy(source, source + size, original);
    }
    begin = end;
  }
  // Dense resume knows the complete entry size of every watch list.  Build the
  // packed arena once (plus the bounded extraction suffix below), as Kissat's
  // dense->sparse transition does, instead of
  // feeding tens of millions of entries through capacity doubling, slice
  // relocation, and repeated whole-arena defragmentation.  Iterating the
  // globally ordered pair vector writes each literal's binary partners in the
  // same order as the former push loop: smaller partners first, then larger
  // partners, both in import order.
  std::vector<uint8_t>().swap(seen);
  std::vector<Var>().swap(ordered_variables);
  std::vector<uint32_t>().swap(k.watches.arena);
  k.watches.holes = 0;
  for (WatchHead &head : k.watches.heads)
    head = WatchHead{};
  for (const auto &binary : binaries) {
    k.watches.heads[binary.first].add_binary_word();
    k.watches.heads[binary.second].add_binary_word();
  }
  if (binary_suffix)
    for (const auto &[a, b] : *binary_suffix) {
      k.watches.heads[a].capacity++;
      k.watches.heads[b].capacity++;
    }
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(c);
    if (!c->garbage()) {
      sort_clause_watches_like_kissat(k, c);
      c->searched = 2;
      k.watches.heads[c->lits[0]].add_large_words();
      k.watches.heads[c->lits[1]].add_large_words();
    }
    ref += words;
  }
  uint64_t planned_words = 0;
  for (const WatchHead &head : k.watches.heads)
    planned_words += static_cast<uint64_t>(head.size) + head.capacity;
  const bool keep_extraction_suffix = planned_words <= UINT32_MAX;
  uint64_t total_words = 0;
  for (WatchHead &head : k.watches.heads) {
    const uint64_t capacity = static_cast<uint64_t>(head.size) +
                              (keep_extraction_suffix ? head.capacity : 0);
    assert(total_words + capacity <= UINT32_MAX);
    head.offset = static_cast<uint32_t>(total_words);
    head.capacity = static_cast<uint32_t>(capacity);
    total_words += capacity;
    head.size = 0;
  }
  // The canonical layout is exact per list, but Congruence immediately adds a
  // sparse set of representative binaries.  With an exactly-sized global
  // vector, the first list relocation asks std::vector to geometrically copy
  // the entire (often GiB-scale) arena.  Reserve bounded relocation headroom
  // while the replacement arena is still empty, preserving every slice and
  // entry order without paying that transient duplicate.
  const uint64_t relocation_slack = std::min<uint64_t>(
      {total_words / 16, uint64_t{1} << 24, UINT32_MAX - total_words});
  k.watches.arena.reserve(
      static_cast<size_t>(total_words + relocation_slack));
  k.watches.arena.resize(static_cast<size_t>(total_words));
  const auto push_word = [&](Lit lit, uint32_t word) {
    WatchHead &head = k.watches.heads[lit];
    assert(head.size < head.capacity);
    k.watches.arena[head.offset + head.size++] = word;
  };
  for (const auto &binary : binaries) {
    push_word(binary.first, binary_watch(binary.second).raw);
    push_word(binary.second, binary_watch(binary.first).raw);
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(c);
    if (!c->garbage()) {
      const Lit first = c->lits[0], second = c->lits[1];
      push_word(first, blocking_watch(second).raw);
      push_word(first, ref);
      push_word(second, blocking_watch(first).raw);
      push_word(second, ref);
      assert(k.watches.heads[first].has_large());
      assert(k.watches.heads[second].has_large());
    }
    ref += words;
  }
  k.watches.holes = 0;
  for (const WatchHead &head : k.watches.heads)
    k.watches.holes += head.capacity - head.size;
  // Congruence's extraction ends at Kissat's dense-to-sparse resume: the
  // packed image above contains both mirrors of every binary and both sparse
  // watches of every live large clause.  Dense phase entry now clears this
  // flag together with those large watches, so publish the completed resume
  // explicitly before later Probe rules decide whether they must flush again.
  k.large_watches_connected = true;
  if (count_congruence_rebuild)
    k.stats.congruence_binary_rebuilds++;
  // This operation is a complete dense-to-sparse representation boundary, not
  // a sequence of ordinary per-clause edits.  A free N1 schedule can retain an
  // Eliminate occurrence image across it, so rebuild that image from the same
  // canonical physical result instead of leaving pre-extraction binary copies
  // in its ordered stream.
  if (k.dense)
    rebuild_dense_large_occurrences(k, *k.dense);
}

static void kernel_compact_dense_entry_watches(
    Kernel &k, bool keep_large,
    std::vector<std::pair<Lit, Lit>> *root_assigned = nullptr,
    std::vector<std::pair<Lit, Lit>> *binary_population = nullptr) {
  // kissat dense.c:flush_large_watches / watch.c:kissat_flush_large_watches:
  // scanning the literals in index order, the first watch of a binary pair
  // stays and every later copy is deleted as a clause (`kissat_delete_binary`
  // from the smaller literal: proof, statistics, marks, scores).  Dropping
  // only the watch entries would leave the copy count and the position of a
  // later re-added copy behind (docs/dev/scheduling-parity-ledger.md).  Values are never
  // inspected here: like the null-stack `watch.c` path, root-satisfied
  // binaries stay, which Sweep and Factor rely on; the value-aware
  // `dense.c` path is modelled by each rule's own root collection.
  // One pass per list, marking partners and dropping large entries with the
  // same cursor, as `kissat_flush_large_watches` does; consecutive epochs in
  // one array carry both of its mark values.
  std::vector<uint32_t> seen(2 * k.nvars, 0);
  uint32_t epoch = 0;
  std::vector<Lit> duplicates; // partners repeated in this list, once each
  std::vector<Var> ordered_variables;
  if (binary_population)
    kernel_vars_in_import_order(k, ordered_variables);
  const auto compact = [&](Lit lit) {
    if (epoch > UINT32_MAX - 2) {
      std::fill(seen.begin(), seen.end(), 0);
      epoch = 0;
    }
    epoch += 2;
    const uint32_t queued = epoch + 1;
    duplicates.clear();
    const auto mark = [&](WatchEntry watch) {
      if (!watch.binary())
        return keep_large;
      const Lit other = watch.other();
      const uint32_t stamp = seen[other];
      if (stamp != epoch && stamp != queued) {
        seen[other] = epoch;
        // Congruence's save-stack dense entry removes root-valued binaries in
        // this same scan.  Its caller delays the actual clause deletions until
        // deduplication has completed, preserving Tactix's proof lifecycle;
        // collect the unique pairs in the exact order of the former follow-up
        // scan so that no second walk over the complete image is needed.
        if ((root_assigned || binary_population) &&
            kernel_import_lit_order(k, lit) <
                kernel_import_lit_order(k, other)) {
          if ((k.value(lit) || k.value(other)) && root_assigned) {
            assert(k.value(lit) > 0 || k.value(other) > 0);
            root_assigned->emplace_back(lit, other);
          } else if (binary_population) {
            binary_population->emplace_back(lit, other);
          }
        }
      } else if (lit < other && stamp == epoch) {
        seen[other] = queued;
        duplicates.push_back(other);
      }
      return true; // the copies leave through the deletion below, as in kissat
    };
    if (keep_large)
      for (const WatchEntry watch : k.watches.entries(lit))
        (void)mark(watch);
    else
      k.watches.compact(lit, mark);
    for (const Lit other : duplicates)
      kernel_remove_duplicate_binary_copies(k, lit, other);
  };
  if (binary_population) {
    for (const Var variable : ordered_variables)
      for (uint32_t sign = 0; sign < 2; sign++)
        compact(make_lit(variable, sign != 0));
  } else {
    for (Lit lit = 0; lit < 2 * k.nvars; lit++)
      compact(lit);
  }
}

void kernel_flush_large_watches(Kernel &k) {
  assert(k.large_watches_connected);
  kernel_compact_dense_entry_watches(k, /*keep_large=*/false);
  k.large_watches_connected = false;
}

void kernel_drop_large_watches(Kernel &k) {
  assert(k.large_watches_connected);
  for (Lit lit = 0; lit < 2 * k.nvars; lit++)
    k.watches.compact(lit, [](WatchEntry w) { return w.binary(); });
  k.large_watches_connected = false;
}

void kernel_deduplicate_binary_watches(
    Kernel &k, std::vector<std::pair<Lit, Lit>> *root_assigned,
    std::vector<std::pair<Lit, Lit>> *binary_population) {
  if (root_assigned)
    root_assigned->clear();
  if (binary_population)
    binary_population->clear();
  if (binary_population)
    binary_population->reserve(k.watches.binary_clauses());
  kernel_compact_dense_entry_watches(k, /*keep_large=*/true, root_assigned,
                                     binary_population);
}

void kernel_dense_connect(Kernel &k, OccIndex &occ, bool track_order) {
  assert(!k.bve_occurrence_count_image);
  assert(k.level == 0);
  // This constructs Tactix's separate phase-local occurrence image. It is not
  // itself a Kissat physical dense-mode transition: individual rules retain
  // ownership of their sparse-watch flush/resume boundaries.
  occ.track_order = track_order;
  rebuild_dense_large_occurrences(k, occ);
  k.dense = &occ;
}

void kernel_dense_disconnect(Kernel &k) {
  assert(!k.bve_occurrence_count_image);
  // Drain the phase's arena readouts; the index itself is caller-owned and
  // outlives this call, so the counters are moved rather than read in place.
  k.stats.dense_slice_moves += k.dense->large.relocations +
                               k.dense->ordered.take_relocations();
  k.dense->large.relocations = 0;
  k.stats.dense_defrags += k.dense->large.defrags +
                         k.dense->ordered.take_defrags();
  k.dense->large.defrags = 0;
  k.dense = nullptr;
}

bool kernel_should_compact_variables(const Kernel &k, uint32_t percent) {
  if (k.nvars == 0)
    return false;

  // Kissat measures `solver->vars - solver->active` after its initial sparse
  // collection has physically removed never-activated variables and compacted
  // every root-fixed variable onto one representative.  Tactix deliberately
  // keeps stable kernel identities for the public rule/proof/model surface;
  // `compact_import_order` is the corresponding dense Kissat population.
  // Count admission on that representation, or already-consumed startup
  // garbage would spuriously trigger another whole-arena collection at the
  // first Reduce.
  const bool virtual_population = !k.compact_import_order.empty();
  const uint64_t vars =
      virtual_population ? k.compact_import_order.size() : k.nvars;
  uint64_t inactive = 0;
  const auto count_inactive = [&](Var v) {
    const bool retired =
        v < k.var_status.size() && k.var_status[v] != VarStatus::Active;
    const bool root_fixed = k.values[make_lit(v, false)] != 0 &&
                            k.assigned[v].level == 0;
    inactive += retired || root_fixed;
  };
  if (virtual_population)
    for (const Var v : k.compact_import_order)
      count_inactive(v);
  else
    for (Var v = 0; v < k.nvars; ++v)
      count_inactive(v);

  // collect.c stores the floating-point percentage product in an unsigned,
  // then applies a strict comparison (`inactive > limit`).  Integer division
  // reproduces that floor exactly without introducing a second threshold.
  const uint64_t limit = static_cast<uint64_t>(percent) * vars / 100;
  return inactive > limit;
}

bool kernel_compact_variables(Kernel &k, bool fold_fixed) {
  if (k.conflict_valid || k.pending.valid || k.dense || k.probing.recording ||
      k.delayed_used)
    return false;

  const uint32_t old_nvars = k.nvars;
  std::vector<uint8_t> archive;
  if (fold_fixed) {
    // LRAT roots must already have been named by their inference owner. Do not
    // manufacture proof units or change a rule's preparation from compaction.
    if (k.proof && k.proof->lrat())
      for (Var v = 0; v < old_nvars; v++)
        if (kernel_root_value(k, make_lit(v, false)) && !k.unit_ids[v]) {
          fold_fixed = false;
          break;
        }
  }
  if (fold_fixed) {
    archive.assign(old_nvars, 0);
    Var representative = INVALID_VAR;
    for (Var v : k.compact_import_order)
      if (!kernel_var_retired(k, v) && kernel_root_value(k, make_lit(v, false))) {
        representative = v;
        break;
      }
    if (representative == INVALID_VAR)
      for (Var v = 0; v < old_nvars; v++)
        if (!kernel_var_retired(k, v) && kernel_root_value(k, make_lit(v, false))) {
          representative = v;
          break;
        }
    const uint32_t root_prefix = kernel_root_fixed(k);
    for (Var v = 0; v < old_nvars; v++)
      archive[v] = v != representative && !kernel_var_retired(k, v) &&
                   kernel_root_value(k, make_lit(v, false)) &&
                   k.assigned[v].trail < root_prefix;
    // True-level roots may follow decisions on the physical trail. Keep that
    // suffix hot until backtracking brings it into the root prefix: archiving
    // it now would change prefix-based public/material counts without an
    // inference. This restricts storage folding, never a public proof rule.
    // Representation-only: fixed literals can remain in an explicit caller's
    // unsimplified clauses. Keep those identities, never flush their clauses
    // or narrow the public rule surface to make folding convenient.
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      if (!c->garbage())
        for (Lit l : *c)
          archive[var_of(l)] = 0;
      ref += clause_actual_words(c);
    }
    for (Lit l = 0; l < 2 * old_nvars; l++)
      for (WatchEntry w : k.watches.entries(l))
        if (w.binary()) {
          archive[var_of(l)] = 0;
          archive[var_of(w.other())] = 0;
        }
    for (Var v = 0; v < old_nvars; v++) {
      if (!k.values[make_lit(v, false)] || !k.assigned[v].level ||
          kernel_var_retired(k, v))
        continue;
      const Assigned &a = k.assigned[v];
      if (a.binary && a.reason < 2 * old_nvars)
        archive[var_of(a.reason)] = 0;
      if (a.jumped && k.jumped_over[v] < 2 * old_nvars)
        archive[var_of(k.jumped_over[v])] = 0;
    }
  }
  const uint32_t archived = static_cast<uint32_t>(
      std::count(archive.begin(), archive.end(), uint8_t{1}));
  std::vector<Var> old_to_new(old_nvars, INVALID_VAR);
  uint32_t new_nvars = 0;
  for (Var old = 0; old < old_nvars; old++)
    if ((k.var_status.empty() || k.var_status[old] == VarStatus::Active) &&
        (!archived || !archive[old]))
      old_to_new[old] = new_nvars++;
  // A direct compaction can still be useful when only root-fixed identities
  // remain: it rebuilds the whole arena/watch pose even when there is no
  // retired identity to renumber.  Reduce counts the one representative in
  // `compact_import_order`, just as Kissat's compacted `vars - active` does.
  const bool root_fixed = kernel_root_fixed(k);
  if (new_nvars == old_nvars && !root_fixed)
    return false;

  // Retirement owns the formula-changing work.  Compaction is only a change
  // of representation, so it refuses a half-retired variable instead of
  // silently deleting or shortening a clause without a proof delta.
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage())
      for (const Lit l : *c)
        if (old_to_new[var_of(l)] == INVALID_VAR)
          return false;
    ref += clause_actual_words(c);
  }
  for (Lit l = 0; l < 2 * old_nvars; l++)
    for (const WatchEntry w : k.watches.entries(l))
      if (w.binary() && (old_to_new[var_of(l)] == INVALID_VAR ||
                         old_to_new[var_of(w.other())] == INVALID_VAR))
        return false;

  // Non-root assignments can outlive retirement until the next backtrack.
  // Their reason graph is representation state too: in particular, a jumped
  // binary reason names both its virtual endpoint in `reason` and the real
  // antecedent in `jumped_over`. Defer compaction while either endpoint is a
  // retired variable; dropping it would make the live assignment's reason
  // unrepresentable (and used to index old_to_new[INVALID_VAR] on goldcrest).
  for (Var old = 0; old < old_nvars; old++) {
    if (old_to_new[old] == INVALID_VAR ||
        k.values[make_lit(old, false)] == 0)
      continue;
    const Assigned &a = k.assigned[old];
    // A kept fixed assignment may have a stale binary antecedent in the cold
    // set. Its already-proved unit becomes the canonical root reason below.
    if (archived && a.level == 0)
      continue;
    if (a.binary && a.reason < 2 * old_nvars &&
        old_to_new[var_of(a.reason)] == INVALID_VAR)
      return false;
    if (a.jumped) {
      const Lit over = k.jumped_over[old];
      if (over >= 2 * old_nvars ||
          old_to_new[var_of(over)] == INVALID_VAR)
        return false;
    }
  }

  // Remove garbage first so the rebuilt arena and watch lists contain exactly
  // the live formula.  This preserves clause refs in assigned reasons through
  // kernel_collect_garbage's forwarding pass.  Variable compaction is not an
  // optional policy-ordering checkpoint in Kissat: its sparse collector walks
  // the current arena while remapping variables.  Tactix can append a
  // replacement that Kissat emitted at the source clause's position; the
  // generation-local alias represents that current native position until the
  // next collection. Replay it here instead of either physical append order
  // or the solve-scoped inprocessing policy.
  struct CompactionArenaOrder final : ArenaOrdering {
    void order(const Kernel &kernel, std::vector<Ref> &refs) override {
      kernel_sort_refs_by_logical_clause_position(kernel, refs);
    }
  } compaction_order;
  kernel_affine_note_namespace_change(k);
  kernel_affine_note_trail_retraction(k);
  ArenaOrdering *const saved_arena_ordering = k.arena_ordering;
  k.arena_ordering = &compaction_order;
  kernel_collect_garbage(k);
  k.arena_ordering = saved_arena_ordering;

  struct Binary {
    Lit a, b;
    uint64_t id;
  };
  std::vector<Binary> binaries;
  for (Lit a = 0; a < 2 * old_nvars; a++)
    for (const WatchEntry w : k.watches.entries(a))
      if (w.binary() && a < w.other())
        binaries.push_back({a, w.other(), kernel_binary_id(k, a, w.other())});

  const auto map_lit = [&](Lit old) {
    const Var now = old_to_new[var_of(old)];
    assert(now != INVALID_VAR);
    return make_lit(now, negated(old));
  };

  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    for (Lit &l : *c)
      l = map_lit(l);
    ref += clause_actual_words(c);
  }

  WatchLists watches;
  watches.resize_lits(2 * new_nvars);
  // Kissat compacts a surviving literal by moving its binary watch prefix onto
  // the new literal (collect.c:flush_watched_clauses_by_literal), then rewatches
  // the collected large-clause tail.  Preserve each directed binary encounter
  // order; rebuilding both directions from undirected pairs would instead use
  // an unrelated stable/external identity order.
  for (Lit old_lit = 0; old_lit < 2 * old_nvars; old_lit++) {
    const Var now = old_to_new[var_of(old_lit)];
    if (now == INVALID_VAR)
      continue;
    const Lit lit = make_lit(now, negated(old_lit));
    for (const WatchEntry watch : k.watches.entries(old_lit))
      if (watch.binary() &&
          old_to_new[var_of(watch.other())] != INVALID_VAR)
        watches.reserve_batch_word(lit);
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    watches.reserve_batch_word(c->lits[0]);
    watches.reserve_batch_word(c->lits[0]);
    watches.reserve_batch_word(c->lits[1]);
    watches.reserve_batch_word(c->lits[1]);
    ref += clause_actual_words(c);
  }
  watches.finish_batch_reserve(false);
  for (Lit old_lit = 0; old_lit < 2 * old_nvars; old_lit++) {
    const Var now = old_to_new[var_of(old_lit)];
    if (now == INVALID_VAR)
      continue;
    const Lit lit = make_lit(now, negated(old_lit));
    for (const WatchEntry watch : k.watches.entries(old_lit)) {
      if (!watch.binary())
        continue;
      const Var other_now = old_to_new[var_of(watch.other())];
      if (other_now == INVALID_VAR)
        continue;
      watches.push_binary(
          lit, make_lit(other_now, negated(watch.other())));
    }
  }
  BinaryIdMap binary_ids;
  if (!k.binary_ids.tracks_keys())
    binary_ids.discard_storage();
  else if (!k.binary_ids.tracks_values())
    binary_ids.discard_values();
  binary_ids.reserve(binaries.size());
  for (const Binary &b : binaries) {
    const Lit a = map_lit(b.a), other = map_lit(b.b);
    binary_ids.insert_or_assign_known(kernel_binary_key(a, other), b.id, true);
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    watches.push_large(c->lits[0], c->lits[1], ref);
    watches.push_large(c->lits[1], c->lits[0], ref);
    ref += clause_actual_words(c);
  }
  std::vector<int8_t> values(2 * new_nvars, 0);
  std::vector<uint8_t> factor_marks(2 * new_nvars, 0);
  std::vector<Assigned> assigned(new_nvars);
  std::vector<Lit> jumped_over(new_nvars, INVALID_LIT);
  std::vector<uint64_t> unit_ids(new_nvars, 0);
  std::vector<int8_t> dsr_root_values;
  if (!k.dsr_root_values.empty())
    dsr_root_values.resize(new_nvars, 0);
  for (Var old = 0; old < old_nvars; old++) {
    const Var now = old_to_new[old];
    if (now == INVALID_VAR)
      continue;
    const Lit opos = make_lit(old, false), npos = make_lit(now, false);
    values[npos] = k.values[opos];
    values[negate(npos)] = k.values[negate(opos)];
    // Kissat's per-polarity Factor flag is part of the compacted variable
    // flags.  It survives renumbering with the literal; clearing or leaving
    // this vector in the old numbering changes the next incremental Factor
    // candidate heap after a collection.  The separate marked-literal
    // statistic is cumulative and deliberately is not reduced when retired
    // literals disappear here.
    factor_marks[npos] = k.factor_marks[opos];
    factor_marks[negate(npos)] = k.factor_marks[negate(opos)];
    // Backtracking clears the value/trail but deliberately leaves Assigned
    // metadata behind.  It is inert while the variable is unassigned and must
    // not become representation state again: stale jumped=true commonly pairs
    // with jumped_over=INVALID_LIT.  Goldcrest used to remap that sentinel as
    // a variable index here.  Fresh vectors already hold the canonical empty
    // record, invalid jumped endpoint, and zero unit id for this case.
    if (values[npos] != 0) {
      assigned[now] = k.assigned[old];
      if (archived && assigned[now].level == 0) {
        assigned[now].reason = UNIT_REASON;
        assigned[now].binary = false;
        assigned[now].jumped = false;
      }
      if (assigned[now].binary && assigned[now].reason < 2 * old_nvars)
        assigned[now].reason = map_lit(assigned[now].reason);
      if (assigned[now].jumped)
        jumped_over[now] = map_lit(k.jumped_over[old]);
      unit_ids[now] = k.unit_ids[old];
      if (!dsr_root_values.empty())
        dsr_root_values[now] = k.dsr_root_values[old];
    }
  }

  std::vector<uint32_t> trail_prefix(k.trail.size() + 1, 0);
  std::vector<Lit> trail;
  trail.reserve(new_nvars);
  for (size_t i = 0; i < k.trail.size(); i++) {
    const Lit old = k.trail[i];
    const Var now = old_to_new[var_of(old)];
    if (now != INVALID_VAR) {
      const Lit lit = make_lit(now, negated(old));
      assigned[now].trail = static_cast<uint32_t>(trail.size());
      trail.push_back(lit);
    }
    trail_prefix[i + 1] = static_cast<uint32_t>(trail.size());
  }
  std::vector<uint32_t> frames = k.frames;
  for (uint32_t &frame : frames)
    frame = trail_prefix[frame];
  const uint32_t propagate_head = trail_prefix[k.propagate_head];
  // The binary-only debt is a trail POSITION like the cursor, so it moves with
  // the same map; UINT32_MAX is the no-debt sentinel, not a position.
  const uint32_t binary_only_head =
      k.binary_only_head <= k.trail.size() ? trail_prefix[k.binary_only_head]
                                           : UINT32_MAX;

  auto compact_by_var = [&](auto &values_by_var, const auto &fallback) {
    using Vector = std::decay_t<decltype(values_by_var)>;
    Vector fresh(new_nvars, fallback);
    for (Var old = 0; old < old_nvars; old++) {
      const Var now = old_to_new[old];
      if (now != INVALID_VAR)
        fresh[now] = values_by_var[old];
    }
    values_by_var.swap(fresh);
  };
  compact_by_var(k.bve_retry, uint8_t{0});
  compact_by_var(k.backbone_priority, uint8_t{0});
  compact_by_var(k.touched, uint64_t{0});
  compact_by_var(k.subsume_material, uint8_t{0});
  compact_by_var(k.removed, uint64_t{0});
  compact_by_var(k.touched_kept, uint64_t{0});
  compact_by_var(k.internal_import_rank, uint32_t{UINT32_MAX});

  std::vector<Var> internal_to_external(new_nvars);
  if (archived) {
    k.fixed_external_values.resize(k.external_to_internal.size(), 0);
    k.fixed_external_units.resize(k.external_to_internal.size(), 0);
    for (Var old = 0; old < old_nvars; old++) {
      if (!archive[old])
        continue;
      const Var external = k.internal_to_external[old];
      assert(!k.fixed_external_values[external]);
      k.fixed_external_values[external] = k.values[make_lit(old, false)];
      k.fixed_external_units[external] = k.unit_ids[old];
    }
    k.fixed_external_count += archived;
  }
  for (Var old = 0; old < old_nvars; old++) {
    const Var now = old_to_new[old];
    const Var external = k.internal_to_external[old];
    k.external_to_internal[external] =
        now == INVALID_VAR ? INVALID_VAR : now;
    if (now != INVALID_VAR)
      internal_to_external[now] = external;
  }

  const uint32_t old_touch_head = k.touch_fixed_head;
  // The input activation seed can outlive parsing until a policy attaches H.
  // It names current internal identities, so remap it even when no H exists
  // yet. Otherwise a pre-attach extension/compaction makes the later seed
  // omit live variables (or name an identity that was retired).
  auto remap_pending_order = [&](std::vector<Var> &order) {
    size_t kept = 0;
    for (const Var old : order) {
      assert(old < old_nvars);
      const Var now = old_to_new[old];
      if (now != INVALID_VAR)
        order[kept++] = now;
    }
    order.resize(kept);
  };
  remap_pending_order(k.input_activation_order);
  if (k.h) {
    heuristics_compact_vars(k, old_to_new, new_nvars);
  } else {
    remap_pending_order(k.compact_import_order);
    k.compact_import_position.assign(new_nvars, INVALID_VAR);
    for (Var position = 0; position < k.compact_import_order.size(); ++position) {
      const Var v = k.compact_import_order[position];
      assert(k.compact_import_position[v] == INVALID_VAR);
      k.compact_import_position[v] = position;
    }
  }

  k.nvars = new_nvars;
  k.internal_to_external.swap(internal_to_external);
  k.values.swap(values);
  k.factor_marks.swap(factor_marks);
  k.assigned.swap(assigned);
  k.jumped_over.swap(jumped_over);
  k.unit_ids.swap(unit_ids);
  k.dsr_root_values.swap(dsr_root_values);
  k.trail.swap(trail);
  k.frames.swap(frames);
  k.propagate_head = propagate_head;
  k.binary_only_head = binary_only_head;
  k.watches = std::move(watches);
  k.binary_ids.swap(binary_ids);
  k.unassigned = 0;
  for (Var v = 0; v < new_nvars; v++)
    k.unassigned += k.values[make_lit(v, false)] == 0;
  k.unassigned_active = k.unassigned;
  k.first_extension_var = UINT32_MAX;
  if (k.original_external_vars != UINT32_MAX) {
    k.first_extension_var = new_nvars;
    for (Var v = 0; v < new_nvars; v++)
      if (k.internal_to_external[v] >= k.original_external_vars) {
        k.first_extension_var = v;
        break;
      }
  }

  k.touch_fixed_head = trail_prefix[old_touch_head];
  k.seen.assign(new_nvars, 0);
  k.eager_marks.assign(2 * new_nvars, 0);
  k.marked_levels.assign(new_nvars + 1, 0);
  k.probing.stamp.assign(2 * new_nvars, 0);
  if (archived) {
    // assign/clear alone retain the old root-sized backing allocations.
    k.seen.shrink_to_fit();
    k.eager_marks.shrink_to_fit();
    k.marked_levels.shrink_to_fit();
    k.probing.stamp.shrink_to_fit();
  }
  k.var_status.clear();
  if (archived)
    k.var_status.shrink_to_fit();
  k.retired_events.clear();
  k.substituted_events.clear();
  k.fresh_var_events.clear();
  k.analyzed.clear();
  k.bump_order.clear();
  k.shrinkable.clear();
  k.reason_bumped.clear();
  k.minimize_scratch.clear();
  k.learned_scratch.clear();
  k.otfs_scratch.clear();
  k.otfs_retired.clear();
  k.radix_scratch.clear();
  k.jump_pending.clear();
  k.lrat_entries.clear();
  k.lrat_chain.clear();
#ifdef TACTIX_ENABLE_EVENT_TRACE
  event_trace_detail(k, "eager_reset_compact", 0);
#endif
  for (Ref &ref : k.eager_recent)
    ref = INVALID_REF;
  k.model_values.clear();
  k.model_extended = false;
  if (k.proof) {
    k.proof->bind_variable_map(&k.internal_to_external);
    k.proof->bind_external_variable_map(&k.external_to_internal);
  }
  k.stats.variable_compactions++;
  k.stats.compacted_variables += old_nvars - new_nvars;
  kernel_check_invariants(k);
  return true;
}


// Both references garbage every redundant clause that mentions an eliminated
// variable (cadical mark_redundant_clauses_with_eliminated_variables_as_garbage,
// elim.cpp:736-756; kissat deactivates the var and flushes its watches), so a
// retired variable stops propagating. Tactix must do it explicitly: the dense
// index is irredundant-only, so elimination never even sees these clauses —
// the wrong-SAT exhibit's two leftover HTR ternaries kept an eliminated
// variable live. Deletion of redundant clauses is unconditionally sound.
void kernel_flush_retired_redundant(Kernel &k) {
  if (k.var_status.empty())
    return;
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(c);
    if (!c->garbage() && c->redundant())
      for (const Lit l : *c)
        if (kernel_var_retired(k, var_of(l))) {
          kernel_delete_clause(k, c);
          k.stats.flushed_retired_redundant++;
          break;
        }
    ref += words;
  }
}

void kernel_build_lrat_chain(
    std::vector<std::pair<uint32_t, uint64_t>> &entries,
    uint64_t closing_id, std::vector<uint64_t> &chain) {
  std::sort(entries.begin(), entries.end());
  chain.clear();
  chain.reserve(entries.size() + (closing_id != 0));
  for (const auto &e : entries) {
    // 0 is LRAT's line terminator, so a zero antecedent does not produce a
    // wrong chain — it produces a TRUNCATED one, silently dropping every
    // later antecedent. `unit_ids[v] == 0` means no proof unit exists yet, so
    // citation sites must exclude it; root-level reason walks assert this when
    // collapsing to the named unit. Cheap permanent guard, same family as
    // Proof::add_rup's no-tautology and
    // no-empty-chain asserts: those two are what make a whole class of
    // certificate defects visible to the debug fuzz legs, since the permissive
    // format hides them — a congruence bug emitting `<id> <lits> 0 0` sat
    // behind a verified DRAT proof on every tier run and every fuzz leg until
    // an LRAT checker walked the chain and rejected the hint-less line.
    assert(e.second != 0 && "LRAT chain cites a literal with no unit id");
    chain.push_back(e.second);
  }
  if (closing_id)
    chain.push_back(closing_id);
}

void kernel_lrat_chain(Kernel &k, uint64_t closing_id) {
  kernel_build_lrat_chain(k.lrat_entries, closing_id, k.lrat_chain);
}

// The proof-only table belongs to one DSR stream; a new stream starts empty.
static void bind_dsr_root_table(Kernel &k) {
  if (!k.proof->stream_identity() || k.dsr_root_proof!=k.proof ||
      k.dsr_root_stream_identity!=k.proof->stream_identity()) {
    k.dsr_root_values.assign(k.nvars,0);
    k.dsr_root_proof=k.proof;
    k.dsr_root_stream_identity=k.proof->stream_identity();
  }
  if (k.dsr_root_values.empty())
    k.dsr_root_values.resize(k.nvars, 0);
}

bool kernel_root_unit_table_stream(const Kernel &k) {
  return k.proof && !k.proof->lrat() &&
         (!k.proof->pb() || k.proof->streaming_pbp()) &&
         k.proof->stream_identity();
}

void kernel_note_dsr_root_unit(Kernel &k, Lit lit) {
  if (!kernel_root_unit_table_stream(k))
    return;
  const Var v = var_of(lit);
  assert(v < k.nvars);
  assert(k.values[lit] > 0);
  assert(k.assigned[v].level == 0);
  bind_dsr_root_table(k);
  k.dsr_root_values[v] = negated(lit) ? -1 : 1;
}

void kernel_materialize_dsr_root_unit(Kernel &k, Lit lit) {
  if (!k.proof || !(k.proof->dsr() || k.proof->streaming_pbp()))
    return;
  const Var v = var_of(lit);
  assert(v < k.nvars);
  assert(k.values[lit] > 0);
  assert(k.assigned[v].level == 0);
  bind_dsr_root_table(k);
  const int8_t polarity = negated(lit) ? -1 : 1;
  if (k.dsr_root_values[v] == polarity)
    return;
  const uint64_t id = ++k.next_clause_id;
  k.proof->add_derived_unit(id, lit, nullptr, 0);
  k.dsr_root_values[v] = polarity;
  kernel_observe_solver_clause(k,
                               SolverPopulationEventKind::UnitMaterialized,
                               SolverClauseKind::Unit, id, &lit, 1);
}

// LRAT, DSR, and streaming PBP: every root-level propagation-derived literal
// becomes an
// explicit unit clause so it remains available after its reason is deleted.
// Emitting in trail order makes each LRAT chain nameable; native DSR lets its
// checker reconstruct the same RUP chain and therefore avoids the reason walk;
// streaming PBP has the same RUP property. DSR/PBP publication deliberately
// does not populate `unit_ids`: that vector is solver-semantic state (several
// rules distinguish asserted from implied root units), so using it only to
// remember a proof line changes the default search.
void kernel_emit_root_units(Kernel &k, size_t from) {
  for (size_t i = from; i < k.trail.size(); i++) {
    const Lit lit = k.trail[i];
    const Var v = var_of(lit);
    const Assigned &a = k.assigned[v];
    if (a.level != 0 || a.reason == UNIT_REASON ||
        a.reason == DECISION_REASON)
      continue; // asserted units got their ids at the source
    if (k.unit_ids[v])
      continue; // an asserted/named unit is already present in the proof
    if (k.proof->dsr() || k.proof->streaming_pbp()) {
      kernel_materialize_dsr_root_unit(k, lit);
      continue;
    }
    k.lrat_entries.clear();
    uint64_t reason_id;
    if (a.binary) {
      // Reason jumping needs the resolved-away literal above level 0, so it
      // cannot reach the root and the reason still names a real clause.
      assert(!a.jumped);
      reason_id = kernel_binary_id(k, lit, a.reason);
      // Same terminator hazard as a zero antecedent, one step earlier: a
      // closing id of 0 is DROPPED by `kernel_build_lrat_chain`, so the line
      // goes out with a short chain the checker cannot replay. The pair is
      // unnamed only when it is not a live clause of this formula — a reason
      // that survived its clause, which is a state defect, not a proof one.
      if (!reason_id) {
        fprintf(stderr,
                "INVARIANT: root literal %d has a binary reason (%d %d) with "
                "no proof id; watched %u forward / %u mirrored\n",
                externalize(lit), externalize(lit), externalize(a.reason),
                kernel_binary_copies(k, lit, a.reason, 1),
                kernel_binary_copies(k, a.reason, lit, 1));
        assert(false && "root unit's binary reason has no clause id");
      }
      const Var ov = var_of(a.reason);
      k.lrat_entries.emplace_back(k.assigned[ov].trail, k.unit_ids[ov]);
    } else {
      const Clause *c = clause_at(k.arena, a.reason);
      reason_id = c->id();
      for (const Lit l : *c)
        if (l != lit) {
          const Var ov = var_of(l);
          k.lrat_entries.emplace_back(k.assigned[ov].trail,
                                      k.unit_ids[ov]);
        }
    }
    kernel_lrat_chain(k, reason_id);
    const uint64_t id = ++k.next_clause_id;
    k.unit_ids[v] = id;
    k.proof->add_derived_unit(id, lit, k.lrat_chain.data(),
                              static_cast<uint32_t>(k.lrat_chain.size()));
  }
}

void kernel_reason_walk(Kernel &k, Var start, std::vector<uint8_t> &seen,
                        std::vector<std::pair<uint32_t, uint64_t>> &out,
                        std::vector<Var> *touched) {
  // Explicit solver-owned stack: trails can be arbitrarily deep, and native
  // LRAT can invoke this walk many times for one rule application.
  std::vector<Var> &todo = k.reason_walk_stack;
  todo.clear();
  todo.push_back(start);
  while (!todo.empty()) {
    const Var v = todo.back();
    todo.pop_back();
    if (seen[v])
      continue;
    seen[v] = 1;
    if (touched)
      touched->push_back(v);
    const Assigned &a = k.assigned[v];
    if (a.reason == DECISION_REASON)
      continue; // probe assumption: the negation of the emitted clause
    if (a.reason == UNIT_REASON || a.level == 0) {
      // A root-fixed literal is a named unit clause in the proof by the time
      // any rule walks reasons (every propagation wrapper runs
      // kernel_emit_root_units before returning; asserted units get their ids
      // at the source), so cite that unit and stop — the same scheme as
      // analyze's chain_record_root. Re-walking the retained clause reason
      // instead cites a clause a rule may have deleted since (a root-satisfied
      // candidate is garbage the moment a unit satisfies it), which the
      // checker rejects as a deleted hint.
      assert(k.unit_ids[v] &&
             "root-fixed literal walked before its unit id exists");
      out.emplace_back(a.trail, k.unit_ids[v]);
      continue;
    }
    if (a.binary) {
      const Lit assigned_lit = make_lit(v, k.values[make_lit(v, true)] > 0);
      // A jumped reason is a VIRTUAL resolvent: cite the real first antecedent
      // (assigned_lit ∨ jumped_over) and continue through the literal it
      // resolved away, whose own entry supplies the second. Walking to
      // `a.reason` directly instead would skip that clause and leave the chain
      // unpropagatable.
      const Lit next = a.jumped ? k.jumped_over[v] : a.reason;
      out.emplace_back(a.trail, kernel_binary_id(k, assigned_lit, next));
      todo.push_back(var_of(next));
      continue;
    }
    const Clause *reason = clause_at(k.arena, a.reason);
    out.emplace_back(a.trail, reason->id());
    for (const Lit m : *reason)
      if (var_of(m) != v)
        todo.push_back(var_of(m));
  }
}

uint64_t kernel_conflict_reason_walk(
    Kernel &k, std::vector<uint8_t> &seen,
    std::vector<std::pair<uint32_t, uint64_t>> &entries,
    std::vector<Var> *touched) {
  if (k.conflict_ref == INVALID_REF) { // binary conflict
    const uint64_t last =
        kernel_binary_id(k, k.conflict_bin[0], k.conflict_bin[1]);
    kernel_reason_walk(k, var_of(k.conflict_bin[0]), seen, entries, touched);
    kernel_reason_walk(k, var_of(k.conflict_bin[1]), seen, entries, touched);
    return last;
  }
  const Clause *conf = clause_at(k.arena, k.conflict_ref);
  const uint64_t last = conf->id();
  for (const Lit m : *conf)
    kernel_reason_walk(k, var_of(m), seen, entries, touched);
  return last;
}

#ifndef NDEBUG
// The two per-entry scans the maintained quantities replace, kept as their
// oracle: a drift here would silently move every scaled inprocessing deadline.
void kernel_check_binirr_counts(const Kernel &k) {
  uint64_t binaries = 0;
  for (Lit lit = 0; lit < static_cast<Lit>(2 * k.nvars); lit++)
    for (const WatchEntry w : k.watches.entries(lit))
      if (w.binary() && lit < w.other())
        binaries++;
  assert(binaries == k.watches.binary_clauses());
  uint64_t large = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && !c->redundant())
      large++;
    ref += clause_actual_words(c);
  }
  assert(large == k.live_irredundant_large);
  (void)binaries;
  (void)large;
}
#endif

uint64_t kernel_irredundant_clauses(const Kernel &k) {
  // Kissat's BINIRR_CLAUSES is a live-clause statistic.  `binary_ids` is a
  // proof-identity archive and deliberately retains deleted pairs, so using
  // its size here moves every scaled inprocessing deadline after a phase that
  // adds and later removes binaries.  Both halves are read the way kissat
  // reads `clauses_binary + clauses_irredundant`, without walking the formula.
#ifndef NDEBUG
  kernel_check_binirr_counts(k);
#endif
  return k.watches.binary_clauses() + k.live_irredundant_large;
}

#ifndef NDEBUG
void kernel_check_invariants(const Kernel &k) {
  heuristics_check(k); // `H` sized to nvars (catches a missed Factor growth)
  // The native-DSR proof never holds a root unit the kernel lacks: a DRAT/DSR
  // checker keeps every unit it derives, so a unit published for the proof
  // alone would let the checker's formula drift ahead of the solver's.  The
  // statement is about a kernel that keeps deriving: once it is inconsistent
  // the refutation is complete, and a speculative round that closed it (the
  // Backbone replay) legitimately leaves retracted literals unassigned.
  for (Var v = 0; !k.inconsistent && v < k.dsr_root_values.size(); v++)
    if (const int8_t polarity = k.dsr_root_values[v]) {
      const Lit unit = make_lit(v, polarity < 0);
      assert(k.values[unit] > 0 && k.assigned[v].level == 0);
    }
  assert(k.internal_to_external.size() == k.nvars);
  assert(k.internal_import_rank.size() == k.nvars);
  assert(k.factor_marks.size() == 2 * k.nvars);
  assert(k.cold_state_deferred ? k.subsume_material.empty()
                               : k.subsume_material.size() == k.nvars);
  for (Var internal = 0; internal < k.nvars; internal++) {
    const Var external = k.internal_to_external[internal];
    assert(external < k.external_to_internal.size());
    assert(k.external_to_internal[external] == internal);
    const uint32_t external_rank =
        external < k.external_import_rank.size()
            ? k.external_import_rank[external]
            : UINT32_MAX;
    assert(k.internal_import_rank[internal] == external_rank);
  }
  for (Var external = 0; external < k.external_to_internal.size(); external++) {
    const Var internal = k.external_to_internal[external];
    assert(internal == INVALID_VAR ||
           (internal < k.nvars &&
           k.internal_to_external[internal] == external));
  }
  assert(k.fixed_external_values.size() == k.fixed_external_units.size());
  assert(k.fixed_external_values.size() <= k.external_to_internal.size());
  uint32_t fixed_external_count = 0;
  for (Var external = 0; external < k.fixed_external_values.size(); external++) {
    const int8_t value = k.fixed_external_values[external];
    assert(value >= -1 && value <= 1);
    if (value) {
      assert(k.external_to_internal[external] == INVALID_VAR);
      assert(!k.proof || !k.proof->lrat() || k.fixed_external_units[external]);
      fixed_external_count++;
    }
  }
  assert(fixed_external_count == k.fixed_external_count);
  // values <-> trail <-> unassigned coherence
  uint32_t assigned_count = 0;
  for (Var v = 0; v < k.nvars; v++) {
    const int8_t pos = k.values[make_lit(v, false)];
    const int8_t neg = k.values[make_lit(v, true)];
    assert(pos == -neg);
    if (pos != 0)
      assigned_count++;
  }
  assert(assigned_count + k.unassigned == k.nvars);
  {
    uint32_t active = 0; // R-c2: the counter must equal a recount
    for (Var v = 0; v < k.nvars; v++)
      if (k.values[make_lit(v, false)] == 0 && !kernel_var_retired(k, v))
        active++;
    assert(active == k.unassigned_active);
  }
  assert(assigned_count == k.trail.size());
  assert(k.propagate_head <= k.trail.size());
  assert(k.frames.size() == k.level + 1);
  for (size_t i = 0; i < k.trail.size(); i++) {
    const Lit lit = k.trail[i];
    assert(k.values[lit] > 0);
    const Assigned &a = k.assigned[var_of(lit)];
    assert(a.trail == i);
    assert(a.level <= k.level);
    // a literal sits at or after the start of its level's frame. (The old
    // upper bound `i < frames[a.level+1]` — trail level-sortedness — is gone
    // since decision (89ee0fb): true-level assignment + the compacting backtrack leave the
    // trail non-level-sorted. The value-based escaped-unit/conflict
    // completeness checks below are what actually catch propagation bugs.)
    assert(a.level == 0 || i >= k.frames[a.level]);
  }
  // Native LRAT: every root-fixed literal in a live derivation needs its unit
  // line id. 0 is the format's line terminator, so a later chain citing it is
  // truncated. Once an input replay has completed the refutation, however,
  // its proof is closed and the proofless kernel may retain a different root
  // trail. No later chain can cite those literals (as for DSR above).
  if (k.proof && k.proof->lrat() && !k.inconsistent)
    for (const Lit lit : k.trail) {
      const Var v = var_of(lit);
      if (k.assigned[v].level != 0 || k.unit_ids[v] != 0)
        continue;
      fprintf(stderr,
              "INVARIANT: root literal %d has no LRAT unit id; trailpos %u "
              "reason %s\n",
              externalize(lit), k.assigned[v].trail,
              k.assigned[v].reason == UNIT_REASON      ? "unit"
              : k.assigned[v].reason == DECISION_REASON ? "decision"
              : k.assigned[v].binary                    ? "binary"
                                                        : "large");
      assert(false && "root literal without an LRAT unit id");
    }
  // every live large clause is watched on exactly lits[0] and lits[1]
  // (and, folded into the same walk, the eager-subsumption register holds only
  // real clause STARTS — a stale Ref left there by a collect would index the
  // middle of some other clause and read a bogus header, which is the single
  // way that register can go wrong). Redundancy is NOT an invariant of the
  // register: C3 Vivify can promote a registered clause to irredundant, and
  // both consumers re-test it (analyze.cpp eager_subsume_recent; kissat
  // learn.c:163 `if (!c->redundant) continue`).
  uint32_t eager_seen = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    for (const Ref r : k.eager_recent)
      if (r == ref)
        eager_seen++; // a live clause START; see the count check below
    if (!c->garbage() && k.large_watches_connected) {
      // ...except on a ROOT-ASSIGNED watched literal. The sparse root collect
      // sweeps only from its watermark while its watch flush drops every large
      // entry of a fixed literal (kissat collect.c
      // flush_watched_clauses_by_literal, `if (!lit_fixed)`), so a clause in
      // the unswept prefix stays live and singly watched until a whole-arena
      // collect reclaims it. That entry can never fire again: the literal keeps
      // its root value for the rest of the solve. Pinned by
      // tests/test_kernel.cpp::test_sparse_collection_flushes_fixed_prefix_watch.
      for (int w = 0; w < 2; w++) {
        const Lit lit = c->lits[w];
        bool found = false;
        for (const WatchEntry w : k.watches.entries(lit))
          if (!w.binary() && w.ref() == ref)
            found = true;
        assert((found || kernel_root_value(k, lit) != 0) &&
               "clause not watched on a watched literal");
      }
    }
    ref += clause_actual_words(c);
  }
  {
    uint32_t eager_live = 0;
    for (const Ref r : k.eager_recent)
      if (r != INVALID_REF)
        eager_live++;
    assert(eager_seen == eager_live && "stale eager-subsumption register ref");
  }
  // A set per-list order bit permits the binary propagator's early break.
  std::vector<uint32_t> physical_binary, ordered_binary;
  std::vector<Ref> physical_large, ordered_large;
  for (Lit lit = 0; lit < 2 * k.nvars; lit++) {
    uint32_t binaries = 0;
    if (k.watches.heads[lit].binaries_first()) {
      bool large = false;
      for (const WatchEntry w : k.watches.entries(lit)) {
        if (w.binary()) {
          binaries++;
          assert(!large && "binary watch after a large one under binary-first");
        } else
          large = true;
      }
    } else {
      for (const WatchEntry w : k.watches.entries(lit))
        binaries += w.binary();
    }
    assert(binaries == k.watches.binary_size(lit) &&
           "stale binary watch population");
    for (const WatchEntry w : k.watches.entries(lit)) {
      if (w.binary())
        continue;
      const Clause *c = clause_at(k.arena, w.ref());
      bool blocker_is_member = false;
      for (const Lit member : *c)
        blocker_is_member |= member == w.blocking();
      assert(blocker_is_member && "large-watch blocker outside its clause");
    }
    if (!k.large_watches_connected)
      assert(!k.watches.heads[lit].has_large() &&
             "large watch retained while globally disconnected");

    // Eliminate's connected image is allowed to retain garbage large refs, but
    // it is not allowed to invent or lose physical binary copies, nor may its
    // mixed stream and cached large population disagree. Checking only at the
    // deletion sites reports the later consumer (usually BVE) instead of the
    // bulk rewrite that corrupted the image; keep this in the random-policy
    // boundary oracle so fuzzing attributes the first bad transition.
    if (k.dense && k.dense->track_order) {
      physical_binary.clear();
      ordered_binary.clear();
      k.dense->copy_large(lit, physical_large);
      ordered_large.clear();
      for (const WatchEntry w : k.watches.entries(lit))
        if (w.binary())
          physical_binary.push_back(w.other());
      for (const OccIndex::Entry entry : k.dense->ordered[lit]) {
        if (entry.binary)
          ordered_binary.push_back(entry.payload);
        else
          ordered_large.push_back(entry.payload);
      }
      std::sort(physical_binary.begin(), physical_binary.end());
      std::sort(ordered_binary.begin(), ordered_binary.end());
      assert(physical_binary == ordered_binary &&
             "dense ordered stream lost a physical binary copy");
      assert(physical_large == ordered_large &&
             "dense ordered stream disagrees with its filtered range");
      assert(ordered_large.size() == k.dense->large_size(lit) &&
             "dense ordered stream lost its cached large population");
    }
  }
  // binary watch symmetry: every binary entry (l -> other) must have its
  // mirror (other -> l); losing one side silently kills the clause
  for (Lit lit = 0; lit < 2 * k.nvars; lit++) {
    for (const WatchEntry w : k.watches.entries(lit)) {
      if (!w.binary())
        continue;
      const Lit o = w.other();
      bool mirror = false;
      for (const WatchEntry m : k.watches.entries(o))
        if (m.binary() && m.other() == lit)
          mirror = true;
      if (!mirror) {
        fprintf(stderr, "INVARIANT: binary watch (%d %d) lost its mirror\n",
                externalize(lit), externalize(o));
        assert(false && "asymmetric binary watch");
      }
    }
  }

  // propagation completeness: fully propagated + no conflict in flight
  // means no clause is falsified.  An outstanding binary-only debt is NOT a
  // fixpoint however far the cursor stands: the next full propagation pulls
  // the cursor back over that region and scans the large watches the binary
  // propagator skipped.
  if (k.propagate_head == k.trail.size() &&
      k.binary_only_head >= k.propagate_head && !k.conflict_valid &&
      !k.pending.valid && !k.inconsistent) {
    for (Lit lit = 0; lit < 2 * k.nvars; lit++) {
      for (const WatchEntry w : k.watches.entries(lit)) {
        if (!w.binary())
          continue;
        // at a propagation fixpoint, a binary watched by a FALSE literal
        // must have its other literal TRUE: other-false is a missed
        // conflict, other-unassigned is a missed unit (both mean
        // propagation escaped — e.g. a decision taken on an un-propagated
        // trail, corpus 004)
        const Lit o = w.other();
        if (k.values[lit] < 0 && k.values[o] <= 0) {
          fprintf(stderr,
                  "INVARIANT: %s binary (%d %d); "
                  "levels %u %u trailpos %u %u head %u size %zu level %u\n",
                  k.values[o] < 0 ? "falsified" : "unit", externalize(lit),
                  externalize(o), k.assigned[var_of(lit)].level,
                  k.assigned[var_of(o)].level, k.assigned[var_of(lit)].trail,
                  k.assigned[var_of(o)].trail, k.propagate_head,
                  k.trail.size(), k.level);
          assert(false && "binary clause escaped propagation");
        }
      }
    }
    // Dense elimination deliberately disconnects every large watch.  A public
    // Propagate inside that phase therefore reaches a binary fixpoint only;
    // the phase exit reconnects the large clauses and explicitly replays the
    // saved root suffix.  Inspecting the bare arena here would mistake that
    // documented intermediate representation for escaped propagation.
    if (k.large_watches_connected) {
      for (Ref ref = 0; ref < k.arena.size();) {
        const Clause *c = clause_at(k.arena, ref);
        if (!c->garbage()) {
          int nonfalse = 0;
          Lit last = INVALID_LIT;
          for (const Lit l : *c)
            if (k.values[l] >= 0) {
              nonfalse++;
              last = l;
            }
          // fixpoint: no live clause may be falsified (0 non-false literals) or
          // unit (exactly one non-false literal that is still UNASSIGNED — it
          // should have been propagated); both are escaped propagation
          assert(nonfalse > 0 && "falsified large clause escaped propagation");
          assert((nonfalse != 1 || k.values[last] > 0) &&
                 "unit large clause escaped propagation");
        }
        ref += clause_actual_words(c);
      }
    }
  }
}
#else
void kernel_check_invariants(const Kernel &) {}
#endif

// --- reconstruction (rho) --------------------------------------------------
void kernel_push_witness(Kernel &k, Lit witness, const Lit *rest,
                         uint32_t nrest) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  rule_journal_note_reconstruction(k, witness, rest, nrest);
#endif
  for (uint32_t i = 0; i < nrest; i++)
    k.reconstruction.push_back(kernel_external_lit(k, rest[i]));
  k.reconstruction.push_back(kernel_external_lit(k, witness));
  // segment length word (clause size = witness + rest); Lit is uint32_t so
  // it shares the flat stack — read back as the trailing length prefix.
  k.reconstruction.push_back(static_cast<Lit>(nrest + 1));
}

void kernel_push_cube_witness(Kernel &k, const Lit *clause, uint32_t nclause,
                              const Lit *witness, uint32_t nwitness) {
  assert(nclause && nwitness && clause && witness);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  rule_journal_note_cube_reconstruction(k, clause, nclause, witness, nwitness);
#endif
  for (uint32_t i = 0; i < nclause; i++)
    k.reconstruction.push_back(kernel_external_lit(k, clause[i]));
  for (uint32_t i = 0; i < nwitness; i++) {
    assert(var_of(witness[i]) < k.external_to_internal.size());
    k.reconstruction.push_back(witness[i]); // already a stable external literal
  }
  k.reconstruction.push_back(static_cast<Lit>(nclause));
  k.reconstruction.push_back(static_cast<Lit>(nwitness));
  // Existing single-witness segments always end in a positive clause length.
  // Zero is therefore a tag, not an external literal or an empty clause.
  k.reconstruction.push_back(0);
}

void kernel_extend_model(Kernel &k, bool materialize_internal) {
  if (k.model_extended)
    return;
  k.model_extended = true;
  // Replay must read a TOTAL external assignment: the segment test below
  // reads a value-0 variable as false in BOTH polarities, which voids the
  // tautological-resolvent exclusion behind both-sides witness pushes — a
  // pos- and a neg-side window of one pivot can then both look unsatisfied
  // and the reverse walk double-flips the pivot (docs/dev/calculus.md, extend_ρ
  // total-assignment premise). Retired variables can reach this point
  // unassigned, and they occur in no live clause, so completing every
  // value-0 variable to a definite default keeps the active formula
  // satisfied and restores the premise. False matches tactix_value's
  // reading of 0.
  k.model_values.resize(2 * k.external_to_internal.size());
  for (Var ev = 0; ev < k.external_to_internal.size(); ev++) {
    const Lit pos = make_lit(ev, false);
    const int8_t fixed = ev < k.fixed_external_values.size()
                             ? k.fixed_external_values[ev] : 0;
    k.model_values[pos] = fixed ? fixed : -1;
    k.model_values[negate(pos)] = -k.model_values[pos];
  }
  for (Var iv = 0; iv < k.nvars; iv++) {
    const Var ev = k.internal_to_external[iv];
    const Lit ipos = make_lit(iv, false), epos = make_lit(ev, false);
    if (k.values[ipos] != 0) {
      k.model_values[epos] = k.values[ipos];
      k.model_values[negate(epos)] = k.values[negate(ipos)];
    }
  }
  // Replay rho in reverse. A single-witness segment forces its witness when
  // its clause is false; a tagged cube segment sets every witness literal in
  // that case. Both segment kinds use permanent external literal identities.
  size_t i = k.reconstruction.size();
  while (i > 0) {
    const uint32_t len = static_cast<uint32_t>(k.reconstruction[--i]);
    if (len == 0) {
      assert(i >= 2);
      const uint32_t nwitness = static_cast<uint32_t>(k.reconstruction[--i]);
      const uint32_t nclause = static_cast<uint32_t>(k.reconstruction[--i]);
      assert(nwitness && nclause &&
             i >= static_cast<size_t>(nwitness) + nclause);
      const size_t witness_begin = i - nwitness;
      const size_t clause_begin = witness_begin - nclause;
      bool satisfied = false;
      for (size_t j = clause_begin; j < witness_begin; j++)
        if (k.model_values[k.reconstruction[j]] > 0) {
          satisfied = true;
          break;
        }
      if (!satisfied)
        for (size_t j = witness_begin; j < i; j++) {
          const Lit literal = k.reconstruction[j];
          k.model_values[literal] = 1;
          k.model_values[negate(literal)] = -1;
        }
      i = clause_begin;
      continue;
    }
    assert(i >= len);
    const size_t begin = i - len;                // clause lits are [begin, i)
    const Lit witness = k.reconstruction[i - 1]; // pushed last before len
    bool satisfied = false;
    for (size_t j = begin; j < i; j++)
      if (k.model_values[k.reconstruction[j]] > 0) {
        satisfied = true;
        break;
      }
    if (!satisfied) {
      k.model_values[witness] = 1;
      k.model_values[negate(witness)] = -1;
    }
    i = begin;
  }
  if (materialize_internal) {
    // Preserve the long-standing terminal kernel test/debug view for variables
    // that are still live internally. Compacted-away variables remain
    // available only through model_values, which is the public/output
    // contract. Resumable online callers leave the semantic state untouched.
    for (Var iv = 0; iv < k.nvars; iv++) {
      const Var ev = k.internal_to_external[iv];
      const Lit ipos = make_lit(iv, false), epos = make_lit(ev, false);
      k.values[ipos] = k.model_values[epos];
      k.values[negate(ipos)] = k.model_values[negate(epos)];
    }
  }
}

} // namespace tactix
