#include "kernel/propagation_region.hpp"
#include "kernel/kernel.hpp"

#include "diagnostics/rule_journal.hpp"
#include "diagnostics/event_trace.hpp"

#include "policies/heuristics.hpp" // the opaque `H`: Kernel's ctor/dtor + calls
#include "proof/proof.hpp" // kernel_delete_binary emits the delta atomically
#include "rules/semantic_extension/affine_matrix_search.hpp"

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
  // Each producer owns its within-search best; calls are not ranked.
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

// `H` is non-owning; kernel_backtrack_impl<true> needs the complete type.

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
  // Before startup preprocessing one common stamp equals per-variable stamps.
  assert(!k.subsume_touch_mark && !k.strengthen_touch_mark &&
         !k.root_occurrence_head &&
         !k.bce_touch_mark && !k.bce_query_touch_mark &&
         k.bce_query_cube.empty() && !k.ternary_touch_mark &&
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

template <bool Reconnect>
static void reserve_variables(Kernel &k, uint32_t nvars, Var restored = INVALID_VAR) {
  if (nvars <= k.nvars)
    return;
  const uint32_t old = k.nvars;
  kernel_affine_note_namespace_change(k);
  k.nvars = nvars;
  // Amortized growth: extensions append one variable at a time.
  const auto reserve_amortized = [](auto &values, size_t need) {
    if (values.capacity() >= need)
      return;
    const size_t current = values.capacity();
    const size_t grown = current + std::max<size_t>(current / 2, 1);
    values.reserve(std::max(need, grown));
  };
  reserve_amortized(k.internal_to_external, nvars);
  if constexpr (Reconnect) {
    assert(nvars == old + 1 && restored < k.external_to_internal.size());
    assert(k.external_to_internal[restored] == INVALID_VAR);
    k.internal_to_external.push_back(restored);
    k.external_to_internal[restored] = old;
  } else
    for (Var v = old; v < nvars; v++) {
      const Var external = static_cast<Var>(k.external_to_internal.size());
      k.internal_to_external.push_back(external);
      k.external_to_internal.push_back(v);
    }
  k.external_import_rank.resize(k.external_to_internal.size(), UINT32_MAX);
  k.internal_import_rank.resize(nvars, UINT32_MAX);
  if constexpr (Reconnect)
    k.internal_import_rank[old] = k.external_import_rank[restored];
  // --- σ-state + derived-index growth (kernel-owned) ---
  k.values.resize(2 * nvars, 0);
  k.assigned.resize(nvars);
  // Read only under `assigned[v].jumped`; grown to keep assign branch-free.
  k.jumped_over.resize(nvars, 0);
  k.watches.resize_lits(2 * nvars);
  k.unit_ids.resize(nvars, 0);
  if (!k.dsr_root_values.empty())
    k.dsr_root_values.resize(nvars, 0);
  // A new variable starts BVE-retry-worthy (kissat activate_variable).
  if (!k.cold_state_deferred)
    k.bve_retry.resize(nvars, 1);
  k.factor_marks.resize(2 * nvars, 0);
  if (!k.cold_state_deferred)
    k.backbone_priority.resize(nvars, 0);
  // A real touch event per new variable (kissat activate_variable,
  // kissat_mark_added_literal).
  if (!k.cold_state_deferred) {
    k.touched.resize(nvars, 0);
    k.removed.resize(nvars, 0);
    k.subsume_material.resize(nvars, 0);
  } else {
    // No consumer can clear flags before cold-state materialization.
    assert(UINT64_MAX - k.subsume_material_count >= nvars - old);
    k.subsume_material_count += nvars - old;
  }
  // The band array needs no such event: a fresh variable is already touched
  // below, and C2 reads the two arrays as a union.
  if (!k.cold_state_deferred)
    k.touched_kept.resize(nvars, 0);
  for (uint32_t v = old; v < nvars; v++) {
    // Kissat activation sets eliminate and subsume: the initial BVE retry.
    kernel_mark_bve_retry(k, v);
    kernel_touch_var(k, v);
    kernel_mark_subsume_material(k, v);
    // Variable activation marks the stripped (positive) literal in Kissat;
    // actual clause polarities are marked below by clause addition.
    kernel_mark_factor_literal(k, make_lit(v, false), "activate");
  }
  // A new variable is Active; grown only once a variable has retired.
  if (!k.var_status.empty())
    k.var_status.resize(nvars, VarStatus::Active);
  if (!k.cold_state_deferred) {
    k.seen.resize(nvars, 0);
    k.eager_marks.resize(2 * nvars, 0); // all-zero between uses; see the field
    k.marked_levels.resize(nvars + 1, 0);
    k.probing.stamp.resize(2 * nvars, 0);
  }
  // Nvars may grow while dense is connected; new literals have no large clause.
  if (k.dense) {
    if (k.dense->track_order)
      k.dense->ordered.resize_lits(2 * nvars);
    else
      k.dense->large.resize_lits(2 * nvars);
  }
  k.unassigned += nvars - old;
  k.unassigned_active += nvars - old; // fresh variables are Active (R-a)
  // Fixed-capacity trail/frames: the inlined assign never reallocates.
  reserve_amortized(k.trail, nvars);
  reserve_amortized(k.frames, static_cast<size_t>(nvars) + 1);
  // Search-guidance growth, only when `H` is attached.
  if (k.h)
    heuristics_reserve_vars(k, old, nvars);
}

void kernel_reserve_vars(Kernel &k, uint32_t nvars) {
  reserve_variables<false>(k, nvars);
}

void kernel_reconnect_external_variable(Kernel &k, Var external) {
  reserve_variables<true>(k, k.nvars + 1, external);
}

void kernel_record_import(Kernel &k, Var v) {
  assert(v < k.nvars);
  assert(k.internal_import_rank.size() == k.nvars);
  // Already ranked: internal and external ranks agree by invariant.
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

// kernel_import_var_order and kernel_import_lit_order are inline in kernel.hpp.

uint64_t kernel_compact_import_lit_order(const Kernel &k, Lit lit) {
  const Var v = var_of(lit);
  if (v < k.compact_import_position.size()) {
    const Var position = k.compact_import_position[v];
    if (position != INVALID_VAR && position < k.compact_import_order.size() &&
        k.compact_import_order[position] == v)
      return 2ull * position + negated(lit);
    // Clients may replace the order directly before rebuilding the inverse.
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
  // external-to-compact map; compaction leaves holes but never reorders it.
  for (const Var external : k.import_rank_to_external) {
    assert(external < k.external_import_rank.size());
    assert(k.external_import_rank[external] < k.next_import_rank);
    if (external >= k.external_to_internal.size())
      continue;
    const Var internal = k.external_to_internal[external];
    if (internal != INVALID_VAR)
      order.push_back(internal);
  }
  // Direct kernel clients: unrecorded live variables follow recorded imports
  // in stable external order, as in `kernel_import_var_order`.
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
  // Kissat's `solver->import` order is by external identity; holes skipped.
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
  // After H's first attachment the kernel keeps the compact population image
  // current while H is absent, so a later attachment finds new variables.
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

// Propagation is trail-driven, so a clause added over an already-scanned false
// literal is never revisited. Rewinding the cursor to the earliest such
// literal re-arms the scan; it is a no-op unless a clause landed behind it.
static void kernel_rearm_propagation(Kernel &k, const Lit *lits,
                                     uint32_t size) {
  // Root additions outside a recorded conflict only (Backjump resets anyway).
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
  // Kissat assigns glue only to redundant clauses.
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
  // Dense mode publishes occurrences (Kissat new_large_clause `watching`).
  if (k.large_watches_connected) {
    k.watches.push_large(lits[0], lits[1], ref);
    k.watches.push_large(lits[1], lits[0], ref);
  }
  // Irredundant clauses feed the dense index and added-material consumers.
  // Kissat's `mark_added_literal` does not set `flags.eliminate`.
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
      // Kissat updates a scheduled variable but does not reinsert a popped one.
      kernel_notify_bve_score(k, var_of(lits[i]), false);
    }
  // The redundant half is a CaDiCaL diagnostic arm, outside Kissat defaults.
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

Ref kernel_add_detached_large_reason(Kernel &k, const Lit *lits,
                                     uint32_t size, uint64_t id) {
  assert(size >= 3 && id);
  const Ref ref = static_cast<Ref>(k.arena.size());
  k.arena.resize(k.arena.size() + clause_words(size));
  Clause *c = clause_at(k.arena, ref);
  c->size = size;
  c->flags = Clause::GARBAGE | Clause::REDUNDANT | Clause::REASON |
             Clause::DETACHED_REASON | Clause::USED_MASK;
  c->set_glue(0);
  c->searched = 2;
  c->set_id(id);
  for (uint32_t i = 0; i < size; ++i)
    c->lits[i] = lits[i];
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
  // Id 0 means no proof identity, so there is no key to retain.
  if (!replacement_id)
    return;
  const uint64_t position = kernel_clause_position(k, source_ref);
  assert(position < UINT64_MAX);
  k.clause_position_alias.set(replacement_id, position + 1);
}

static void note_binary_add(Kernel &k, Lit a, Lit b, uint64_t id,
                            bool new_pair, bool mark_added_material) {
  // Last id wins: a re-add installs its fresh id.
  const uint64_t key = kernel_binary_key(a, b);
  k.binary_ids.insert_or_assign_known(key, id, new_pair);
  if (k.binary_liveness)
    k.binary_liveness->add(key);
  // Added material for subsumption and Factor; Kissat's mark_added_literal
  // leaves `flags.eliminate` alone.
  if (mark_added_material) {
    kernel_notify_bve_score(k, var_of(a), false);
    kernel_notify_bve_score(k, var_of(b), false);
    // Both variables are new material (kissat new_binary_clause, clause.c:71).
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
  // The caller rewrote (stable -> moved); add only the opposite direction.
  assert(!k.bve_occurrence_count_image);
  kernel_rearm_binary_clause(k, stable, moved);
  k.watches.push_binary(moved, stable);
  // The missing physical direction of a published clause adds no material
  // (Kissat moves the watch without mark_added_literal).
  note_binary_add(k, stable, moved, id, false, false);
}

void kernel_publish_binary_clause(Kernel &k, Lit a, Lit b, uint64_t id,
                                  bool new_pair,
                                  BinaryPopulationEffect population_effect) {
  assert(!k.bve_occurrence_count_image);
  kernel_rearm_binary_clause(k, a, b);
  // Publishes only the rebuilt physical watch pair and its identity; Kissat's
  // substitute_binaries and Sweep move watches without mark_added_literal.
  note_binary_add(k, a, b, id, new_pair, false);
  if (population_effect == BinaryPopulationEffect::LogicalAdd) {
    const Lit clause[2] = {a, b};
    kernel_observe_solver_clause(k, SolverPopulationEventKind::Add,
                                 SolverClauseKind::Binary, id, clause, 2);
  }
}

// Retires every live root assignment the pair (a, b) antecedes before it is
// deleted; false refuses deletion for a live non-root antecedent. Under reason
// jumping the first antecedent is (x ∨ jumped_over[x]); the endpoint-local
// test covers the whole chain.
static bool retire_binary_root_reasons(Kernel &k, Lit a, Lit b) {
  for (const Lit x : {a, b}) {
    Assigned &ax = k.assigned[var_of(x)];
    const Lit other = (x == a) ? b : a;
    if (k.values[x] <= 0 || !ax.binary ||
        (ax.jumped ? k.jumped_over[var_of(x)] != other : ax.reason != other))
      continue;
    if (ax.level != 0)
      return false; // live non-root reason: refuse (never at a root)
    kernel_materialize_dsr_root_unit(k, x);
    ax.reason = UNIT_REASON;
  }
  return true;
}

// Removes one physical copy from both mirror lists (and the dense traversal),
// preserving survivor order, and retires its two BVE occurrence counts.
static void remove_one_binary_watch_pair(Kernel &k, Lit a, Lit b) {
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
}

static void record_binary_copy_deletion(Kernel &k, Lit a, Lit b,
                                        const char *touch_reason,
                                        uint32_t deleted_copies = 1) {
  k.stats.binaries_deleted += deleted_copies;
  kernel_mark_bve_retry(k, var_of(a), touch_reason);
  kernel_mark_bve_retry(k, var_of(b), touch_reason);
  kernel_notify_bve_score(k, var_of(a), true);
  kernel_notify_bve_score(k, var_of(b), true);
  kernel_removed_var(k, var_of(a));
  kernel_removed_var(k, var_of(b));
}

bool kernel_delete_binary(Kernel &k, Lit a, Lit b) {
  // (1) Absent: no-op; duplicated: unnameable (kernel_binary_copies).
  if (kernel_binary_copies(k, a, b, 2) != 1)
    return false; // absent (idempotent) or duplicated (unnameable)
  // (2) Retire a live root reason before deleting the clause it names.
  if (!retire_binary_root_reasons(k, a, b))
    return false;
  // (3) Remove the mirror watch entries by in-place compaction.
  remove_one_binary_watch_pair(k, a, b);
  if (k.binary_liveness)
    k.binary_liveness->remove(kernel_binary_key(a, b));
  // (4) proof delete (chain-free; deletions do not consume an id).
  if (k.proof) {
    const Lit lits2[2] = {a, b};
    k.proof->del(k.next_clause_id, kernel_binary_id(k, a, b), lits2, 2);
  }
  // (5) binary_ids keeps the key (size-monotone) and the retired id, which is
  // unreachable until a re-add overwrites it.
  // BVE retry (kissat clause.c kissat_delete_binary -> mark_removed)
  record_binary_copy_deletion(k, a, b, "delete-binary");
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
  // An identical clause remains, so reasons and the retained proof id stay
  // valid; remove one slot per mirror, preserving survivor order (Kissat).
  remove_one_binary_watch_pair(k, a, b);
  // DSR and streaming PBP track every physical copy.
  if (k.proof && !k.proof->startup_pb_written()) {
    const Lit clause[2] = {a, b};
    k.proof->del(k.next_clause_id, 0, clause, 2);
  }
  record_binary_copy_deletion(k, a, b, "delete-binary-copy");
  const Lit clause[2] = {a, b};
  kernel_observe_solver_clause(k, SolverPopulationEventKind::Delete,
                               SolverClauseKind::Binary,
                               kernel_binary_id(k, a, b), clause, 2);
  return true;
}

bool kernel_delete_binary_one_copy_bve_scan(Kernel &k, Lit pivot, Lit other) {
  // Reproduces eliminate.c::kissat_eliminate_binary's score notification:
  // `pivot` was already popped; `other` is updated and may be reinserted.
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
  // forward.c compacts each directed watch vector in place keeping the first
  // occurrence; one-at-a-time deletion would keep the last.
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
  // One deletion per retired copy, named by literals (Kissat).
  if (k.proof) {
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
  const Lit clause[2] = {a, b};
  kernel_observe_solver_clause(k, SolverPopulationEventKind::Delete,
                               SolverClauseKind::Binary,
                               kernel_binary_id(k, a, b), clause, 2,
                               removed_copies);
  return removed_copies;
}

// Remove every physical copy of (a,b), retiring each by literal multiset.
template <bool CheckerVisible>
static bool delete_binary_all_copies(Kernel &k, Lit a, Lit b,
                                     uint32_t copies) {
  if (copies == 0)
    return false; // absent (idempotent)
  // Retire a live root reason before removing the clauses that name it.
  if (!retire_binary_root_reasons(k, a, b))
    return false;
  // Factor's unique pairs in binary-only slices are removed by moving the
  // suffix (Kissat factor.c:eagerly_remove_watch); other lists compact stably.
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
  if constexpr (CheckerVisible) if (k.proof) {
    const Lit lits2[2] = {a, b};
    for (uint32_t i = 0; i < copies; i++)
      k.proof->del(k.next_clause_id, kernel_binary_id(k, a, b), lits2, 2);
  }
  record_binary_copy_deletion(k, a, b, "delete-all-binary", copies);
  const Lit clause[2] = {a, b};
  kernel_observe_solver_clause(k, SolverPopulationEventKind::Delete,
                               SolverClauseKind::Binary,
                               kernel_binary_id(k, a, b), clause, 2, copies);
  return true;
}

bool kernel_delete_binary_all_copies(Kernel &k, Lit a, Lit b) {
  return delete_binary_all_copies<true>(k, a, b, kernel_binary_copies(k, a, b));
}

bool kernel_discard_query_binary_copies(Kernel &k, Lit a, Lit b,
                                        const QueryPopulationRetirement &) {
  assert(k.trail.empty() && !k.level && !k.pending.valid && !k.conflict_valid);
  return delete_binary_all_copies<false>(k,a,b,kernel_binary_copies(k,a,b));
}

bool kernel_delete_binary_all_copies_known(Kernel &k, Lit a, Lit b,
                                           uint32_t copies) {
  assert(kernel_binary_copies(k, a, b) == copies);
  return delete_binary_all_copies<true>(k, a, b, copies);
}

// --- watch repair ----------------------------------------------------------
// Contract in kernel.hpp. From kissat watch.h (kissat_unwatch_blocking /
// kissat_watch_blocking); the walk is entry-aware since a large watch occupies
// two words.
void kernel_unwatch_large(Kernel &k, Lit l, Ref ref) {
  assert(!k.delayed_used); // no scan may hold pointers into this slice
  // An unlogged edit would be overwritten by a pose rollback.
  assert(!kernel_probing_rollback_active(k));
  bool removed = false;
  k.watches.compact(l, [&](WatchEntry w) {
    if (removed || w.binary() || w.ref() != ref)
      return true;
    removed = true; // drop this entry's blocking word + ref word
    return false;
  });
  // Sparse collection may have flushed this entry from a root-fixed literal's
  // list; only a permanently assigned slot may be absent.
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
  // `c` survives: unwatch and push_large touch only the watch arena.
  c->lits[slot] = promoted;
  c->lits[from] = displaced;
  // The other watch's blocking literal and `c->searched` stay valid hints.
  k.watches.push_large(promoted, c->lits[slot ^ 1], ref);
}

// Contract in kernel.hpp, including why this stops at the fixed variable.
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

// The one per-literal unassign: value, both unassigned counters and the
// policy's ranking structures move together.
template <bool HasH> static void kernel_unassign_literal(Kernel &k, Lit lit) {
  const Var v = var_of(lit);
  Assigned &assignment = k.assigned[v];
  if (assignment.affine && k.affine_matrix_search_session)
    k.affine_matrix_search_session->discard_lazy_reason(v);
  if (!assignment.affine && !assignment.binary &&
      assignment.reason != DECISION_REASON &&
      assignment.reason != UNIT_REASON) {
    Clause *reason = clause_at(k.arena, assignment.reason);
    // A detached algebraic reason belongs to exactly one implied literal, so
    // releasing it here lets the next arena collection reclaim it.
    if (reason->detached_reason() && reason->reason())
      reason->flags &= ~Clause::REASON;
  }
  k.values[lit] = 0;
  k.values[negate(lit)] = 0;
  assignment.affine = false;
  k.unassigned++;
  if (!kernel_var_retired(k, v))
    k.unassigned_active++;
  if constexpr (HasH) {
    // A retired variable must not re-enter either ranking structure, or
    // deactivation degenerates into demotion.
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

// `HasH` selects at compile time whether backtrack maintains search guidance,
// so the null-`H` instantiation carries no per-literal `if (k.h)`.
template <bool HasH>
static void kernel_backtrack_impl(Kernel &k, uint32_t target,
                                  bool search_backtrack) {
  assert(target < k.level);
  // Saved phases are refreshed by kernel_assign (kissat inlineassign.h);
  // target/best snapshots stay at the call sites (kissat backtrack.c:156).
  // Compacting backtrack (implementation-notes D-LEVEL-2): keep every literal
  // whose true level <= target, unassigning only those above it.
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
  // Re-propagate from the boundary (as CaDiCaL), or from 0 after an
  // off-fixpoint Decide tainted the trail (D-LEVEL-1). A probing restore
  // always lands on the boundary.
  const uint32_t reset = (k.trail_tainted && search_backtrack) ? 0 : boundary;
  if (k.propagate_head > reset)
    k.propagate_head = reset;
}

void kernel_backtrack(Kernel &k, uint32_t target, bool search_backtrack) {
  // Dispatch once per backtrack on whether `H` is present.
  if (k.h)
    kernel_backtrack_impl<true>(k, target, search_backtrack);
  else
    kernel_backtrack_impl<false>(k, target, search_backtrack);
}

template <bool HasH>
static void kernel_unwind_speculation_impl(Kernel &k, uint32_t position) {
  assert(position <= k.trail.size());
  // Every level above root must lie inside the discarded suffix.
  assert(!k.level || k.frames[1] >= position);
  if (k.trail.size() > position) kernel_affine_note_trail_retraction(k);
  const bool observed_roots_retracted = position < k.root_occurrence_head ||
                                       (!position && k.fixed_external_count);
  while (k.trail.size() > position) {
    const Lit lit = k.trail.back();
    k.trail.pop_back();
    kernel_unassign_literal<HasH>(k, lit);
    // A unit id caches the retracted polarity; a later root replay may derive
    // the opposite one. The old proof line stays live.
    k.unit_ids[var_of(lit)] = 0;
  }
  k.touch_fixed_head = std::min(k.touch_fixed_head, position);
  k.root_occurrence_head = std::min(k.root_occurrence_head, position);
  if (observed_roots_retracted) {
    k.bce_touch_mark = k.bce_query_touch_mark = k.cce_touch_mark = 0;
    k.bce_rejected_occ_limit = 0;
    k.cce_rejected_budget = UINT64_MAX;
  }
  k.frames.resize(1);
  k.level = 0;
  // The cursor may only move back, as in backtrack.
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
void kernel_emit_empty(Kernel &k) {
  if (k.proof)
    k.proof->add_empty(0);
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
}

PendingUnits::Outcome PendingUnits::record(Kernel &k, Lit u) {
  assert(owner == &k);
  if (!k.eager_marks[u] && !k.eager_marks[negate(u)]) {
    k.eager_marks[u] = 1;
    units.push_back(u);
    return Outcome::Recorded;
  }
  if (k.eager_marks[u])
    return Outcome::Duplicate; // same-sign duplicate: a redundant unit
  kernel_emit_empty(k);
  return Outcome::Refuted;
}

// Membership lookup that continues after the previous match, wrapping over
// the prefix: order-preserving rewrites stay linear, and every query still
// searches the complete image. Changes no marks, pose or shared scratch.
static auto lit_membership(const Lit *begin, const Lit *end) {
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
}

ReplaceOutcome kernel_replace_clause(Kernel &k, Clause *c, uint64_t id,
                                     const Lit *lits, uint32_t size,
                                     bool redundant, uint32_t glue,
                                     Ref *large_ref,
                                     bool mark_added_material) {
  // `id` is the replacement's freshly allocated id (`next_clause_id`).
  assert(id == k.next_clause_id);
  // A large redundant rewrite models Kissat's in-place shrink: save recency
  // before the arena-mutating add so the replacement cannot rejuvenate it.
  // Caller-specific metadata is restored by the caller.
  const uint32_t inherited_used = c->used();
  const uint64_t predecessor_id = c->id();
  kernel_observe_replacement(k, SolverReplacementPhase::Begin,
                             SolverReplacementOutcome::Pending,
                             predecessor_id, id, c->lits, c->size, lits,
                             size);
  const bool semantic_irredundant = !c->redundant();
  const Ref source_ref = static_cast<Ref>(
      reinterpret_cast<const uint32_t *>(c) - k.arena.data());
  // One semantic rewrite: Kissat arms elimination only for removed literals.
  {
    auto in_new = lit_membership(lits, lits + size);
    for (const Lit old : *c)
      if (!in_new(old)) {
        if (semantic_irredundant)
          kernel_mark_bve_retry(k, var_of(old));
        kernel_removed_var(k, var_of(old));
        if (semantic_irredundant)
          kernel_notify_bve_score(k, var_of(old), true);
      }
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

  // Order-preserving membership cursors (lit_membership above).
  auto in_new = lit_membership(lits, lits + size);

  // Update the dense membership while the old image is readable: retained
  // literals keep their occurrence position; new ones are appended.
  if (k.dense && !redundant) {
    auto in_old = lit_membership(c->lits, c->lits + old_size);
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

  // A live in-place record cannot retain stale watch entries; kissat's
  // on-the-fly strengthening keeps the surviving watch's entry.
  if (k.large_watches_connected) {
    if (!retain || old_first != retained_watch)
      kernel_unwatch_large(k, old_first, ref);
    if (!retain || old_second != retained_watch)
      kernel_unwatch_large(k, old_second, ref);
  }
  if (!retain)
    for (uint32_t i = 0; i < old_size; ++i) {
      const Lit old = c->lits[i];
      if (in_new(old))
        continue;
      if (!redundant)
        kernel_mark_bve_retry(k, var_of(old));
      kernel_removed_var(k, var_of(old));
      if (!redundant)
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
      // strengthen.c: the kept entry changes its blocker; the new watch appends.
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
      // A higher-level reason is not root-satisfied; root reasons retire below.
#ifndef NDEBUG
      if (reason_clause)
        for (const Lit assigned_lit : k.trail) {
          const Var v = var_of(assigned_lit);
          const Assigned &a = k.assigned[v];
          if (!a.affine && !a.binary && a.reason == original_ref) {
            assert(reason_var == INVALID_VAR);
            reason_var = v;
          }
        }
      assert(reason_var == INVALID_VAR || k.assigned[reason_var].level == 0);
#endif
      // Root flushing still informs BCE/CCE, independent of BVE bookkeeping.
      if (!mark_satisfied_removal)
        for (Lit member : *c) kernel_removed_var(k, var_of(member));
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

  // Allocate scratch only for a clause that really shrinks.
  std::vector<Lit> kept;
  kept.reserve(c->size);
  for (const Lit l : *c)
    if (kernel_root_value(k, l) >= 0)
      kept.push_back(l);

  // Locate a large reason only after the satisfied early return (O(trail)).
  if (reason_clause)
    // A reason has exactly one satisfied literal, the one it forced
    // (kissat collect.c:get_forced).
    for (const Lit lit : *c) {
      if (kernel_root_value(k, lit) > 0 || k.values[lit] <= 0)
        continue;
      const Var v = var_of(lit);
      const Assigned &a = k.assigned[v];
      if (!a.affine && !a.binary && a.reason == original_ref)
        reason_var = v;
      break;
    }

  const bool redundant = c->redundant();
  // kissat's root flush (collect.c) keeps glue; Vivify's schedule-time
  // simplification clamps to size-1 (vivify.c) via `clamp_glue`.
  const uint32_t glue =
      clamp_glue ? kernel_shrunken_glue(k, c->glue(),
                                        static_cast<uint32_t>(kept.size()))
                 : c->glue();
  // Stripping false roots can turn an ineligible source into an open victim.
  if (!redundant)
    for (Lit literal : kept) kernel_removed_var(k, var_of(literal));
  // Not needed at a root fixpoint, but an off-fixpoint caller must unwind
  // before the replacement becomes a permanent unit.
  if (kept.size() == 1 && k.level)
    kernel_backtrack(k, 0, /*search_backtrack=*/false);
  const uint64_t id = ++k.next_clause_id;
  if (k.proof) // the writer tracks the replacement's id for `del id`
    k.proof->add_rup(id, kept.data(), static_cast<uint32_t>(kept.size()));
  k.stats.flushed++;
  if (changed)
    (*changed)++;
  if (kept.size() >= 3) {
    // Kissat shortens a surviving large clause inside its arena record,
    // preserving reference/occurrence order and header state (`vivify`,
    // `used`, `reason`).
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
    k.unit_ids[var_of(unit)] = id;
    if (value < 0) {
      kernel_emit_empty(k);
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

  // Kissat flushes watched binaries satisfied by any root literal before
  // sweeping the arena (collect.c:flush_watched_clauses_by_literal). Snapshot
  // unique pairs first because deletion compacts both watch lists.
  std::vector<std::pair<Lit, Lit>> satisfied_binaries;
  // The variable component of each stable import rank; the sign is the low
  // literal bit, giving the exact kernel_import_lit_order comparison.
  std::vector<uint64_t> var_rank(static_cast<size_t>(k.nvars));
  for (Var v = 0; v < k.nvars; v++)
    var_rank[v] = kernel_import_var_order(k, v);
  // Visit only root-true endpoint lists. With both endpoints root true the
  // lower import endpoint owns the visit, so each physical copy is recorded
  // exactly once in the owner's orientation.
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
  // Every copy was met above and each pair is deleted whole.
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

  // Reduce collection starts at Kissat's reducible boundary. Classify each
  // original record by its logical position: the prefix stays untouched and
  // every tail record is root-flushed.
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
  // Reduce's marked victims need the sweep and rewatch even without root
  // simplification, so `changed` alone is not the admission predicate.
  if ((changed || has_garbage) && outcome != RootCollectOutcome::Refuted) {
    kernel_collect_garbage(k, start, /*sparse_collection=*/true);
  }
  return outcome;
}

// --- propagation engine ------------------------------------------------
// From kissat/src/proplit.h: blocking literals, inline binaries, in-place
// compaction, `searched` resumption and delayed relocation (a push can move
// the flat arena, so relocations queue in kernel.delayed). The kernel owns it
// because probing rules need propagation as a subroutine (plan §5.3).
// Probing log (ProbingLog in kernel.hpp): snapshot `l`'s watch list the first
// time this scope writes it; until then it is unchanged since probing began.
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
  // Snapshot traffic is charged in ticks against the probing budgets.
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

// Returns true iff a conflict was found. `ignore` (cadical's ignore pointer)
// is a clause that must not propagate or conflict during this scan. `RECORD`
// logs watch/header updates into k.probing for rollback. `IGNORE` mirrors
// Kissat's `PROBING_PROPAGATION` (proplit.h:151,169).
static inline bool region_allows(const Kernel &k, Lit l,
                                 const PropagationRegion &region) {
  const Var v = var_of(l);
  return (v < region.stamps.size() && region.stamps[v] == region.epoch) ||
         kernel_root_value(k, l);
}

template <bool RECORD, bool BEYOND = false, bool IGNORE = true,
          bool LOCAL = false>
static bool propagate_literal(Kernel &k, Lit lit, Ref ignore,
                              uint64_t &total_ticks,
                              const PropagationRegion *region = nullptr) {
  const Lit not_lit = negate(lit);
  if constexpr (!RECORD)
    event_trace_watch_scan(k, lit);
  assert(!k.delayed_used);
  // A copy-forward scan that rewinds or rewrites entries, so it does not use
  // WatchLists::entries/compact. Hoisted data pointers (kissat proplit.h):
  // the arrays never move during a scan.
  const int8_t *const values = k.values.data();
  uint32_t *const arena = k.arena.data();
  uint32_t *const begin = k.watches.begin(not_lit);
  uint32_t *q = begin;
  const uint32_t *p = q;
  const uint32_t *const end = k.watches.end(not_lit);
  // proplit.h hoists the propagated literal's assignment once per scan.
  const Assigned &binary_source = k.assigned[var_of(lit)];
  // Reused scratch, grown only when a relocation is delayed (as Kissat's
  // stack is output-sensitive).
  bool conflict = false;
  // Search ticks exactly as kissat's proplit.h: 1 + cache lines of the list,
  // then +1 per binary assignment (:92), large-clause dereference (:98),
  // watch relocation (:148) and large-clause assignment (:174).
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
      if constexpr (LOCAL)
        if (!region_allows(k, blocking, *region)) continue;
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
      if constexpr (LOCAL) {
        bool selected = true;
        for (Lit x : *c)
          if (!region_allows(k, x, *region)) { selected = false; break; }
        if (!selected) continue;
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
        // Kissat tests the ignored candidate only after any legal relocation.
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
  // No snapshot needed: q != begin + size only after a drop, which snapshots.
  const uint64_t words_after = q - begin;
  k.watches.truncate_large(not_lit, static_cast<uint32_t>(words_after));
  flush_delayed<RECORD>(k);
  // Cost rows are emitted for the probing scan too.
  event_trace_watch_cost(k, lit, words_before, words_after, base_ticks,
                         binary_assignments, large_dereferences, relocations,
                         large_assignments, ticks);
  total_ticks += ticks;
  return conflict;
}

// Full propagation takes over the region binary-only propagation credited to
// the shared cursor (Kernel::binary_only_head): pull the cursor back so the
// skipped large watches are inspected exactly once here.
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
    // Kissat post-increments its cursor before scanning and publishes it even
    // on conflict (propsearch.c:37-40); Backtrack owns any rewind.
    const Lit lit = k.trail[propagate++];
#if defined(TACTIX_ENABLE_EVENT_TRACE) || defined(TACTIX_ENABLE_RULE_JOURNAL)
    // Diagnostic callbacks observe the consumed cursor.
    k.propagate_head = propagate;
#endif
    // Kissat counts scanned trail literals, including the conflicting one,
    // not implications (propsearch.c:14-23). Only the probing caller passes
    // `ignore`.
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
  // Select the candidate-aware scan once per call; publish the cursor on
  // both exits as propsearch.c does.
  const size_t assigned_before = k.trail.size();
  const uint64_t roots_before = k.stats.root_assignments;
  const bool conflict = ignore == INVALID_REF
                            ? propagate_trail<false>(k, INVALID_REF)
                            : propagate_trail<true>(k, ignore);
  // Kissat writes every root-level assignment to the proof (assign.c); this
  // is the one home for that. Literals are stamped at their true level, so
  // the scan runs whenever this call counted a root assignment. A root
  // conflict normally ends the run, so streaming PBP stages these units and
  // drops them if the empty clause is next.
  if (k.proof && !k.proof->startup_pb_written()) {
    const bool stage = conflict && k.level == 0;
    if (stage)
      k.proof->begin_deferred_root_units();
    if (__builtin_expect(k.stats.root_assignments != roots_before, 0))
      kernel_emit_root_units(k, assigned_before);
    if (stage)
      k.proof->end_deferred_root_units();
  }
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

// Binary-only propagation of `lit` (kissat backbone.c backbone_propagate,
// proprobe.c): large watches are skipped, never mutated. A binary conflict is
// recorded exactly like the full engine.
static bool propagate_literal_binary(Kernel &k, Lit lit,
                                     bool use_current_level,
                                     uint64_t &total_ticks) {
  const Lit not_lit = negate(lit);
  const int8_t *const values = k.values.data();
  const uint32_t *const begin = k.watches.begin(not_lit);
  const uint32_t *p = begin;
  const uint32_t *const end = k.watches.end(not_lit);
  // kissat `stop_early` (backbone.c:186-193): under binary-first layout the
  // first large entry ends the binary sub-list.
  const bool stop_early = k.watches.heads[not_lit].binaries_first();
  // Residual pricing (Stats::binprop_*) on scans the flag did not cover.
  bool large_seen = false, ordered = true;
  uint64_t large_words = 0;
  // The word cursor is load-bearing: Kissat returns on a conflicting binary
  // before charging the scan, and an early break charges the first large word.
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
  // Accounted over the prefix actually scanned.
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
// Contract in kernel.hpp. Divergence: one engine plus an undo log instead of
// kissat_probing_propagate's second proplit.h instantiation; propagation is
// bit-identical to kernel_propagate.

void kernel_probing_begin(Kernel &k, bool rollback_pose) {
  // Production probing starts at a root fixpoint; low-level tests also probe
  // at decision levels, where the caller owns the unfinished scan.
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
  // A speculative propagation can force a literal at level 0 that the unwind
  // leaves assigned; it bypasses `propagate_service`, so emit its unit here.
  const size_t assigned_before = k.trail.size();
  const uint64_t roots_before = k.stats.root_assignments;
  const bool named_root_units = k.proof && !k.proof->startup_pb_written();
  bool conflict = false;
  kernel_settle_binary_only_debt(k); // this scan is a full one
  if (!k.probing.recording) {
    conflict = kernel_propagate(k, ignore); // no open scope: nothing to record
  } else {
    const uint32_t saved_propagate = k.propagate_head;
    uint64_t ticks = 0;
    while (k.propagate_head < k.trail.size()) {
      // proprobe.c uses the same post-increment cursor contract.
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
  // Only a level-0 assignment made by this call can need a unit line.
  if (named_root_units && k.stats.root_assignments != roots_before)
    kernel_emit_root_units(k, assigned_before);
  return conflict;
}

// Selected-premise probing entry point; region membership is tested only here.
bool kernel_propagate_region(Kernel &k, const PropagationRegion &region) {
  assert(k.probing.recording && !k.probing.rollback_pose && k.level);
  const uint32_t saved = k.propagate_head;
  uint32_t cursor = saved;
  uint64_t ticks = 0;
  bool conflict = false;
  while (cursor < k.trail.size()) {
    const Lit l = k.trail[cursor++];
    if (propagate_literal<false, false, true, true>(
            k, l, INVALID_REF, ticks, &region)) { conflict = true; break; }
  }
  k.stats.propagations += cursor - saved;
  kernel_charge_ticks(k, ticks);
  // A subset scan cannot discharge global propagation debt, even on success.
  assert(k.propagate_head == saved);
  return conflict;
}

// Binary-only propagation to fixpoint or first conflict (V2 Backbone).
bool kernel_propagate_binary(Kernel &k, bool use_current_level) {
  // Not `propagate_service`, so it carries two obligations: record the debt
  // for binary-only cursor advance (Kernel::binary_only_head), and emit a unit
  // line for a level-0 literal the caller's unwind may leave assigned.
  if (k.propagate_head < k.binary_only_head)
    k.binary_only_head = k.propagate_head;
  const size_t assigned_before = k.trail.size();
  const uint64_t roots_before = k.stats.root_assignments;
  const bool named_root_units = k.proof && !k.proof->startup_pb_written();
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
  // Only a level-0 assignment made by this call can need a unit line.
  if (named_root_units && k.stats.root_assignments != roots_before)
    kernel_emit_root_units(k, assigned_before);
  return conflict;
}

// Garbage collection after kissat/src/collect.c, simplified: copy live
// clauses into a fresh arena in order, leave a forwarding pointer, fix reasons
// and rebuild large watches with the best current pair first. With a nonzero
// Reduce watermark, match Kissat's physical sparse split: keep watches before
// `first_reducible` and rebuild every surviving tail record.
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

// Stable LSD radix ordering by logical clause position, the order Kissat gets
// from shortening in place; Tactix records replacement source positions.
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
  // Positions are 32-bit arena Refs: four radix passes.
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
    // Release the previous Eliminate phase's optional image payload.
    occ.ordered.release();
  else
    occ.large.release();

  // One arena walk collects irredundant references and counts occurrences,
  // so each slice is laid out at its exact final length. The order list is
  // kernel-owned and keeps its allocation.
  std::vector<Ref> &order = k.dense_order_scratch;
  order.clear();
  if (occ.track_order)
    occ.ordered.begin_exact_layout(nlits);
  else
    occ.large.begin_exact_layout(nlits);
  for (const auto [ref, c] : arena_records(k.arena))
    if (!c->garbage() && !c->redundant()) {
      order.push_back(ref);
      for (const Lit lit : *c) {
        if (occ.track_order)
          occ.ordered.count(lit);
        else
          occ.large.count(lit);
      }
    }
  // Eliminate's BVE image keeps the binary vectors and appends every
  // irredundant large occurrence.
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
  // Kissat's dense lists follow irredundant arena order (logical positions).
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
  // Construct first so the final sample follows the old arena's destruction.
  struct MemoryExit {
    const Kernel &kernel;
    ~MemoryExit() {
      event_trace_memory_checkpoint(kernel, "Collect", "exit");
    }
  } memory_exit{k};
  event_trace_memory_checkpoint(k, "Collect", "entry");
#endif
  assert(!k.bve_occurrence_count_image);
  // No probing scope may be open: this relocates every Ref, so a snapshot
  // would roll pre-collection words back over the compacted arena.
  assert(!k.probing.recording);
  // Collection is physically dense only while large watches are disconnected:
  // connected sparse watches are authoritative even with a live occurrence
  // cache, and disconnected watches are never silently reconnected.
  const bool dense_collection = !k.large_watches_connected;
  const Ref old_reducible = k.first_reducible;
  Ref packed_reducible = INVALID_REF;
  // Genuine Kissat collections reset the recent-learned register
  // (collect.c:274/514/702); an implementation-only packing pass forwards its
  // live entries instead. Garbage entries are invalidated below.
  if (!preserve_eager_recent) {
    event_trace_detail(k, "eager_reset_collect",
                       static_cast<int>(sparse_collection));
    for (Ref &r : k.eager_recent)
      r = INVALID_REF;
  }
  // A sparse watermark must fall on a record boundary; prefix garbage may stay
  // marked until Reduce, and its live watch entries are forwarded below.
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
  // Mark aliased, already-dead sources so the sparse prefix cannot copy them;
  // sentinels die with the old arena (implementation-notes §5).
  if (sparse_collection && !k.clause_position_alias.empty())
    for (const auto [ref, c] : arena_records(k.arena))
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
  const auto retain_record = [&k, sparse_collection,
                              sparse_cut](Ref ref, const Clause *c) {
    if (!c->garbage() || (c->detached_reason() && c->reason()))
      return true;
    // Only the sparse sweep has a prefix to preserve.
    if (!sparse_collection)
      return false;
    const uint64_t position = kernel_clause_position(k, ref);
    assert(position <= UINT32_MAX && position < k.arena.size());
    const Clause *const source =
        clause_at(k.arena, static_cast<Ref>(position));
    return position < sparse_cut && source->searched != INVALID_REF;
  };
  // One walk yields the retained-word total and the survivor list.
  size_t retained_words = 0;
  std::vector<Ref> &order = k.collect_order_scratch;
  order.clear();
  for (const auto [ref, c] : arena_records(k.arena))
    if (retain_record(ref, c)) {
      // Collection packs the semantic image; sparse prefix garbage remains
      // part of the physical image, as before Kissat's sweep watermark.
      retained_words += clause_words(c->size);
      order.push_back(ref);
    }
  assert(retained_words <= UINT32_MAX);
  Arena fresh;
  // Event-trace memory sample, charging the new arena's reservation.
  const auto checkpoint = [&](const char *stage) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_memory_checkpoint(k, "Collect", stage,
                                  fresh.capacity() * sizeof(Arena::value_type));
#else
    (void)stage;
#endif
  };
  checkpoint("before-reserve");
  // Reserve exactly the live words: the compacted image.
  fresh.reserve(retained_words);
  checkpoint("before-sort");
  uint32_t packed_watch_start = 0;
  // Kissat's sparse collector keeps the prefix, sweeps the tail in order,
  // then stably moves surviving redundant clauses behind irredundant ones
  // (collect.c:move_redundant_clauses_to_the_end).
  if (sparse_collection) {
    // Reduce is not an arena-policy checkpoint: keep Kissat's sparse order.
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
    // A packed replacement returns to its source's current position,
    // emulating Kissat's in-place replacement.
    kernel_sort_refs_by_logical_clause_position(k, order);
  }
  checkpoint("after-sort");
#ifndef NDEBUG
  std::vector<uint8_t> seen(k.arena.size());
  for (const Ref ref : order) {
    assert(ref < k.arena.size() && !seen[ref]);
    assert(retain_record(ref, clause_at(k.arena, ref)));
    seen[ref] = 1;
  }
  for (const auto [ref, c] : arena_records(k.arena))
    assert(seen[ref] || !retain_record(ref, c));
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
    // Genuine collection refreshes Kissat's first-reducible register; a
    // packing pass forwards the old cut, or the first surviving tail record.
    if (!c->garbage() &&
        (c->redundant() ||
         (preserve_eager_recent && old_reducible != INVALID_REF &&
          kernel_clause_position(k, ref) >= old_reducible)))
      packed_reducible = std::min(packed_reducible, nref);
    fresh.insert(fresh.end(), k.arena.begin() + ref,
                 k.arena.begin() + ref + words);
    // Only Kissat's logical prefix before `watch_start` avoids rewatching and
    // keeps its replacement cursor; whole-arena collection resets every clause.
    Clause *const packed = clause_at(fresh, nref);
    packed->flags &= ~Clause::SHRUNKEN;
    // Dense collection is a byte-semantic copy (literal order and cursor kept,
    // Kissat); sparse collection rewatches and resets the cursor outside the
    // retained prefix.
    packed->searched =
        (dense_collection || retained_sparse_watch) ? c->searched : 2;
    c->searched = nref;
  }
  checkpoint("after-copy");
  for (const Lit lit : k.trail) {
    Assigned &a = k.assigned[var_of(lit)];
    if (a.affine) {
      assert(a.reason == AFFINE_REASON && a.level);
    } else if (!a.binary && a.reason != DECISION_REASON &&
               a.reason != UNIT_REASON) {
      const Clause *c = clause_at(k.arena, a.reason);
      if (c->garbage() &&
          !(c->detached_reason() && c->reason())) {
        // Only a root-fixed literal's reason can be garbage here (inprocessing
        // deletes root-satisfied clauses at a quiescent root; Reduce protects
        // reasons). It is a root unit and needs no reason; forwarding a garbage
        // clause would read a bogus `searched`.
        assert(a.level == 0);
        a.reason = UNIT_REASON;
      } else {
        a.reason = c->searched;
      }
    }
  }
  // `searched` holds a forwarding pointer only for a copied record.
  if (k.pending.valid && k.pending.ref != INVALID_REF) {
    const Clause *c = clause_at(k.arena, k.pending.ref);
    // The Analyze->Backjump handoff is reason-locked by every collector caller.
    assert(!c->garbage() && "pending learned clause retired before Backjump");
    k.pending.ref = c->searched;
  }
  if (k.conflict_valid && k.conflict_ref != INVALID_REF) {
    const Clause *c = clause_at(k.arena, k.conflict_ref);
    // Conflict-state Reduce marks this clause as a reason, so it is forwarded;
    // a binary conflict has INVALID_REF.
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
    // An OTFS reason retired before Backjump is dropped, as
    // `analyze_retire_otfs_reasons` would skip it.
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
    // Kissat tests fixedness and collection mode once per list
    // (collect.c:flush_watched_clauses_by_literal). Stripping a root-fixed
    // watch is valid only after root collection; an unresolved search
    // conflict keeps both watches until it is discharged.
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
    // The compaction is stable: entries keep position and cached blocker.
  }
  k.arena.swap(fresh);
  checkpoint("after-swap");
  // The packed arena embodies the selected order; old aliases are void.
  k.clause_position_alias.clear();
  k.first_reducible = packed_reducible;
  // Rebuild the dense occurrence index from the compacted arena, keeping
  // per-literal allocation capacity.
  for (const auto [ref, c] : arena_records(k.arena))
    if (!c->garbage() && !dense_collection &&
        (!sparse_collection || ref >= packed_watch_start)) {
      // collect.c:rewatch_clauses selects the best pair before appending.
      sort_clause_watches_like_kissat(k, c);
      k.watches.push_large(c->lits[0], c->lits[1], ref);
      k.watches.push_large(c->lits[1], c->lits[0], ref);
    }
  // Dense collection rebuilds only the full-occurrence image, in compacted
  // order; sparse watches wait for sparse resume.
  if (k.dense)
    rebuild_dense_large_occurrences(k, *k.dense);
  checkpoint("after-reindex");
  // Surviving binaries stayed in place and large entries were appended, so
  // the binary-first layout holds (as kissat_watch_large_clauses,
  // substitute.c:594-596).
  k.arena_generation++; // every Ref taken before this call is now stale
}

// Re-enter sparse search after an inprocessing phase in arena order: Kissat's
// dense simplifiers flush large watches and `kissat_watch_large_clauses`
// rebuilds them on resume.
void kernel_rebuild_large_watches(Kernel &k, bool flush_eliminated) {
  for (Lit lit = 0; lit < 2 * k.nvars; lit++)
    k.watches.compact(lit, [](WatchEntry w) { return w.binary(); });
  std::vector<Ref> order;
  for (const auto [ref, c] : arena_records(k.arena)) {
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
  }
  // Logical position equals the current Kissat arena position.
  kernel_sort_refs_by_logical_clause_position(k, order);
  for (const Ref ref : order) {
    Clause *c = clause_at(k.arena, ref);
      // `kissat_watch_large_clauses` first runs `kissat_sort_literals` under
      // the root assignment; the selected pair is search-visible.
      sort_clause_watches_like_kissat(k, c);
      c->searched = 2;
      k.watches.push_large(c->lits[0], c->lits[1], ref);
      k.watches.push_large(c->lits[1], c->lits[0], ref);
  }
  k.large_watches_connected = true;
}

// Dense resume builds the packed arena once from exact list sizes, as
// Kissat's dense->sparse transition. The begin step sizes both mirrors of each
// pair in `binaries`; the finish step fills the live large clauses after
// Kissat's watch-pair selection. Each literal's binaries precede its large
// watches.
static void begin_packed_watches(
    Kernel &k, const std::vector<std::pair<Lit, Lit>> &binaries) {
  std::vector<uint32_t>().swap(k.watches.arena);
  k.watches.holes = 0;
  for (WatchHead &head : k.watches.heads)
    head = WatchHead{};
  for (const auto &[a, b] : binaries) {
    k.watches.heads[a].add_binary_word();
    k.watches.heads[b].add_binary_word();
  }
}

// `large_order` lists the live large clauses (null: all, in arena order).
// `extraction_headroom` selects Congruence's canonical rebuild: one spare word
// per `suffix` endpoint, bounded relocation slack, and a holes recount.
static void finish_packed_watches(
    Kernel &k, const std::vector<std::pair<Lit, Lit>> &binaries,
    const std::vector<Ref> *large_order, bool extraction_headroom,
    const std::vector<std::pair<Lit, Lit>> *suffix = nullptr) {
  const auto for_each_large = [&](auto visit) {
    if (large_order) {
      for (const Ref ref : *large_order)
        visit(ref);
      return;
    }
    for (const auto [ref, c] : arena_records(k.arena))
      if (!c->garbage())
        visit(ref);
  };
  if (suffix)
    for (const auto &[a, b] : *suffix) {
      k.watches.heads[a].capacity++;
      k.watches.heads[b].capacity++;
    }
  for_each_large([&](Ref ref) {
    Clause *c = clause_at(k.arena, ref);
    sort_clause_watches_like_kissat(k, c);
    c->searched = 2;
    k.watches.heads[c->lits[0]].add_large_words();
    k.watches.heads[c->lits[1]].add_large_words();
  });
  bool keep_extraction_suffix = false;
  if (extraction_headroom) {
    uint64_t planned_words = 0;
    for (const WatchHead &head : k.watches.heads)
      planned_words += static_cast<uint64_t>(head.size) + head.capacity;
    keep_extraction_suffix = planned_words <= UINT32_MAX;
  }
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
  if (extraction_headroom) {
    // Congruence adds representative binaries next; reserve bounded headroom
    // so the first list relocation does not copy the whole arena.
    const uint64_t relocation_slack = std::min<uint64_t>(
        {total_words / 16, uint64_t{1} << 24, UINT32_MAX - total_words});
    k.watches.arena.reserve(
        static_cast<size_t>(total_words + relocation_slack));
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
  for_each_large([&](Ref ref) {
    const Clause *c = clause_at(k.arena, ref);
    const Lit first = c->lits[0], second = c->lits[1];
    push_word(first, blocking_watch(second).raw);
    push_word(first, ref);
    push_word(second, blocking_watch(first).raw);
    push_word(second, ref);
    assert(k.watches.heads[first].has_large());
    assert(k.watches.heads[second].has_large());
  });
  if (extraction_headroom) {
    k.watches.holes = 0;
    for (const WatchHead &head : k.watches.heads)
      k.watches.holes += head.capacity - head.size;
  }
}

void kernel_resume_walk_watches(Kernel &k) {
  // Walk's dense entry follows `dense.c:flush_large_watches`, which saves a
  // pair from its lower compact endpoint; Tactix uses import order as the
  // compact-literal key.
  std::vector<Var> ordered_variables;
  kernel_vars_in_import_order(k, ordered_variables);
  std::vector<std::pair<Lit, Lit>> satisfied;
  std::vector<uint64_t> lit_rank(2 * static_cast<size_t>(k.nvars));
  for (Lit l = 0; l < static_cast<Lit>(2 * k.nvars); l++)
    lit_rank[l] = kernel_import_lit_order(k, l);
  // Visits each binary from its lower-ranked endpoint, watching literals in
  // import order.
  const auto for_each_binary = [&](auto visit) {
    for (const Var variable : ordered_variables)
      for (uint32_t sign = 0; sign < 2; sign++) {
        const Lit lit = make_lit(variable, sign != 0);
        for (const WatchEntry watch : k.watches.entries(lit))
          if (watch.binary() && lit_rank[lit] < lit_rank[watch.other()])
            visit(lit, watch.other());
      }
  };
  for_each_binary([&](Lit lit, Lit other) {
    if (kernel_root_value(k, lit) > 0 || kernel_root_value(k, other) > 0)
      satisfied.emplace_back(lit, other);
  });
  // The scan met every copy, so the count travels with the pair; deletions
  // run in sorted encounter order.
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
  // Save surviving pairs in that encounter order; Walk does not sort
  // equal-first runs.
  std::vector<std::pair<Lit, Lit>> binaries;
  for_each_binary(
      [&](Lit lit, Lit other) { binaries.emplace_back(lit, other); });

  begin_packed_watches(k, binaries);
  std::vector<Ref> large_order;
  for (const auto [ref, c] : arena_records(k.arena))
    if (!c->garbage())
      large_order.push_back(ref);
  // Reconstruct the current Kissat arena order before sizing the image.
  kernel_sort_refs_by_logical_clause_position(k, large_order);
  finish_packed_watches(k, binaries, &large_order,
                        /*extraction_headroom=*/false);
}

void kernel_canonicalize_binary_watches(Kernel &k,
                                        bool count_congruence_rebuild,
                                        const std::vector<std::pair<Lit, Lit>> *
                                            binary_suffix,
                                        std::vector<std::pair<Lit, Lit>> *
                                            binary_population) {
  // Kissat Congruence (`dense.c:flush_large_watches`, `extract_binaries`):
  // keep the first copy of each binary, save it at its smaller literal, sort
  // each equal-first run by the second literal. Tactix maps compact indices to
  // import order. The binary count sums each head's binary-prefix length.
  std::vector<std::pair<Lit, Lit>> binaries;
  std::vector<uint8_t> seen;
  std::vector<Var> ordered_variables;
  // Cache each literal's import rank once for the scan and the run sorts.
  std::vector<uint64_t> lit_rank(2 * static_cast<size_t>(k.nvars));
  for (Lit l = 0; l < static_cast<Lit>(2 * k.nvars); l++)
    lit_rank[l] = kernel_import_lit_order(k, l);
  if (binary_population) {
    // Consume Congruence's saved root-open pairs (first-literal import order).
    binaries.swap(*binary_population);
  } else {
    // Reserve both mirror entries of every retained binary.
    binaries.reserve(k.watches.binary_clauses());
    // Per-list deduplication: a byte per literal, cleared in a second pass.
    seen.assign(2 * k.nvars, 0);
    kernel_vars_in_import_order(k, ordered_variables);
    for (const Var variable : ordered_variables)
      for (uint32_t sign = 0; sign < 2; sign++) {
        const Lit lit = make_lit(variable, sign != 0);
        for (const WatchEntry w : k.watches.entries(lit)) {
          if (!w.binary())
            continue;
          const Lit other = w.other();
          // Sparse resume re-appends saved pairs; duplicates go, values don't.
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
  // Kissat's extract_binaries sorts each equal-first run by the other
  // literal: comparison sort below 32, stable byte-radix otherwise.
  std::vector<std::pair<Lit, Lit>> radix_tmp;
  for (size_t begin = 0; begin < binaries.size();) {
    size_t end = begin + 1;
    while (end < binaries.size() &&
           binaries[end].first == binaries[begin].first)
      end++;
    const size_t size = end - begin;
    // Detect an already-canonical run before sorting; this also supplies the
    // radix bounds.
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
  // Build the packed image plus the bounded extraction suffix; each literal's
  // partners come smaller first, then larger, in import order.
  std::vector<uint8_t>().swap(seen);
  std::vector<Var>().swap(ordered_variables);
  begin_packed_watches(k, binaries);
  finish_packed_watches(k, binaries, /*large_order=*/nullptr,
                        /*extraction_headroom=*/true, binary_suffix);
  // Congruence's extraction ends at Kissat's dense-to-sparse resume, so
  // publish the connection explicitly.
  k.large_watches_connected = true;
  if (count_congruence_rebuild)
    k.stats.congruence_binary_rebuilds++;
  // A complete representation boundary: rebuild a retained Eliminate
  // occurrence image from the canonical result.
  if (k.dense)
    rebuild_dense_large_occurrences(k, *k.dense);
}

static void kernel_compact_dense_entry_watches(
    Kernel &k, bool keep_large,
    std::vector<std::pair<Lit, Lit>> *root_assigned = nullptr,
    std::vector<std::pair<Lit, Lit>> *binary_population = nullptr) {
  // kissat dense.c:flush_large_watches / watch.c:kissat_flush_large_watches:
  // in literal order the first watch of a binary stays and later copies are
  // deleted as clauses (`kissat_delete_binary` from the smaller literal).
  // Values are not inspected: root-satisfied binaries stay (null-stack
  // `watch.c`). One pass per list marks partners and drops large entries;
  // consecutive epochs carry both mark values.
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
        // Congruence's save-stack entry collects root-valued pairs here in
        // scan order; its caller deletes them after deduplication.
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
  // Tactix's phase-local occurrence image; rules keep their own sparse-watch
  // flush/resume boundaries.
  occ.track_order = track_order;
  rebuild_dense_large_occurrences(k, occ);
  k.dense = &occ;
}

void kernel_dense_disconnect(Kernel &k) {
  assert(!k.bve_occurrence_count_image);
  // Move the phase's arena readouts out of the caller-owned index.
  k.stats.dense_slice_moves += std::exchange(k.dense->large.relocations, 0) +
                               k.dense->ordered.take_relocations();
  k.stats.dense_defrags += std::exchange(k.dense->large.defrags, 0) +
                           k.dense->ordered.take_defrags();
  k.dense = nullptr;
}

bool kernel_should_compact_variables(const Kernel &k, uint32_t percent) {
  if (k.nvars == 0)
    return false;

  // Kissat measures `vars - active` after compacting never-activated and
  // root-fixed variables; `compact_import_order` is that population.
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

  // collect.c's percentage product truncated to unsigned, then
  // `inactive > limit`; integer division reproduces the floor.
  const uint64_t limit = static_cast<uint64_t>(percent) * vars / 100;
  return inactive > limit;
}

bool kernel_compact_variables(Kernel &k, bool fold_fixed) {
  if (k.conflict_valid || k.pending.valid || k.dense || k.probing.recording ||
      k.delayed_used)
    return false;

  // A compact affine handle names its implied variable, which compaction
  // would not remap; inapplicable until every live handle is materialized.
  for (const Lit lit : k.trail) {
    const Assigned &a = k.assigned[var_of(lit)];
    if (a.affine)
      return false;
  }

  const uint32_t old_nvars = k.nvars;
  std::vector<uint8_t> archive;
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
    // True-level roots may follow decisions on the trail; keep them hot until
    // backtracking brings them into the root prefix. Representation-only:
    // fixed literals in an explicit caller's clauses are kept.
    for (const auto [ref, c] : arena_records(k.arena))
      if (!c->garbage())
        for (Lit l : *c)
          archive[var_of(l)] = 0;
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
  // A direct compaction also rebuilds the arena/watch pose with only
  // root-fixed identities; Reduce counts one representative (Kissat).
  const bool root_fixed = kernel_root_fixed(k);
  if (new_nvars == old_nvars && !root_fixed)
    return false;

  // Compaction only changes representation, so it refuses a half-retired
  // variable, whose clauses would need a proof delta.
  for (const auto [ref, c] : arena_records(k.arena))
    if (!c->garbage())
      for (const Lit l : *c)
        if (old_to_new[var_of(l)] == INVALID_VAR)
          return false;
  for (Lit l = 0; l < 2 * old_nvars; l++)
    for (const WatchEntry w : k.watches.entries(l))
      if (w.binary() && (old_to_new[var_of(l)] == INVALID_VAR ||
                         old_to_new[var_of(w.other())] == INVALID_VAR))
        return false;

  // A live non-root reason is representation state too (a jumped binary names
  // `reason` and `jumped_over`): defer while either endpoint is retired.
  for (Var old = 0; old < old_nvars; old++) {
    if (old_to_new[old] == INVALID_VAR ||
        k.values[make_lit(old, false)] == 0)
      continue;
    const Assigned &a = k.assigned[old];
    // A kept fixed assignment's stale binary antecedent becomes its unit.
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

  // Remove garbage first (forwarding assigned reasons). Kissat's sparse
  // collector walks the current arena while remapping variables, so replay
  // the generation-local alias order.
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

  for (const auto [ref, c] : arena_records(k.arena))
    for (Lit &l : *c)
      l = map_lit(l);

  WatchLists watches;
  watches.resize_lits(2 * new_nvars);
  // Kissat moves each literal's binary watch prefix onto the new literal
  // (collect.c:flush_watched_clauses_by_literal), preserving directed order,
  // then rewatches the large tail.
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
  for (const auto [ref, c] : arena_records(k.arena)) {
    watches.reserve_batch_word(c->lits[0]);
    watches.reserve_batch_word(c->lits[0]);
    watches.reserve_batch_word(c->lits[1]);
    watches.reserve_batch_word(c->lits[1]);
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
  for (const auto [ref, c] : arena_records(k.arena)) {
    watches.push_large(c->lits[0], c->lits[1], ref);
    watches.push_large(c->lits[1], c->lits[0], ref);
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
    // Kissat's per-polarity Factor flag survives renumbering with the
    // literal; the marked-literal statistic is cumulative.
    factor_marks[npos] = k.factor_marks[opos];
    factor_marks[negate(npos)] = k.factor_marks[negate(opos)];
    // Backtracking leaves Assigned metadata behind; it is inert while
    // unassigned and must not be remapped (jumped_over may be INVALID_LIT).
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
  // The binary-only debt is a trail position (UINT32_MAX: none); remap it.
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
  const uint32_t old_occurrence_head = k.root_occurrence_head;
  // The input activation seed names internal identities before H attaches,
  // so remap it even without H.
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
  k.root_occurrence_head = trail_prefix[old_occurrence_head];
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
  event_trace_detail(k, "eager_reset_compact", 0);
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


// Both references garbage every redundant clause with an eliminated variable
// (cadical mark_redundant_clauses_with_eliminated_variables_as_garbage,
// elim.cpp:736-756; kissat flushes its watches). The dense index is
// irredundant-only, so Tactix does it explicitly; deleting redundant clauses
// is always sound.
void kernel_flush_retired_redundant(Kernel &k) {
  if (k.var_status.empty())
    return;
  for (const auto [ref, c] : arena_records(k.arena))
    if (!c->garbage() && c->redundant())
      for (const Lit l : *c)
        if (kernel_var_retired(k, var_of(l))) {
          kernel_delete_clause(k, c);
          k.stats.flushed_retired_redundant++;
          break;
        }
}

// Per-stream table: binds root-true `lit` and returns its slot variable.
static Var bind_dsr_root_table(Kernel &k, Lit lit) {
  const Var v = var_of(lit);
  assert(v < k.nvars);
  assert(k.values[lit] > 0);
  assert(k.assigned[v].level == 0);
  if (!k.proof->stream_identity() || k.dsr_root_proof!=k.proof ||
      k.dsr_root_stream_identity!=k.proof->stream_identity()) {
    k.dsr_root_values.assign(k.nvars,0);
    k.dsr_root_proof=k.proof;
    k.dsr_root_stream_identity=k.proof->stream_identity();
  }
  if (k.dsr_root_values.empty())
    k.dsr_root_values.resize(k.nvars, 0);
  return v;
}

bool kernel_root_unit_table_stream(const Kernel &k) {
  return k.proof && !k.proof->startup_pb_written() && k.proof->stream_identity();
}

void kernel_note_dsr_root_unit(Kernel &k, Lit lit) {
  if (!kernel_root_unit_table_stream(k))
    return;
  const Var v = bind_dsr_root_table(k, lit);
  k.dsr_root_values[v] = negated(lit) ? -1 : 1;
}

void kernel_materialize_dsr_root_unit(Kernel &k, Lit lit) {
  if (!k.proof || k.proof->startup_pb_written())
    return;
  const Var v = bind_dsr_root_table(k, lit);
  const int8_t polarity = negated(lit) ? -1 : 1;
  if (k.dsr_root_values[v] == polarity)
    return;
  const uint64_t id = ++k.next_clause_id;
  k.proof->add_derived_unit(id, lit);
  k.dsr_root_values[v] = polarity;
  kernel_observe_solver_clause(k,
                               SolverPopulationEventKind::UnitMaterialized,
                               SolverClauseKind::Unit, id, &lit, 1);
}

// DSR and streaming PBP: every root-level propagation-derived literal becomes
// an explicit unit clause, available after its reason is deleted. This does
// not populate `unit_ids`, which is solver-semantic state.
void kernel_emit_root_units(Kernel &k, size_t from) {
  for (size_t i = from; i < k.trail.size(); i++) {
    const Lit lit = k.trail[i];
    const Var v = var_of(lit);
    const Assigned &a = k.assigned[v];
    assert(!a.affine && "native affine reason reached root-unit publication");
    if (a.level != 0 || a.reason == UNIT_REASON ||
        a.reason == DECISION_REASON)
      continue; // asserted units got their ids at the source
    if (k.unit_ids[v])
      continue; // an asserted/named unit is already present in the proof
    kernel_materialize_dsr_root_unit(k, lit);
  }
}

#ifndef NDEBUG
// Oracle scans for the maintained counts (drift moves inprocessing deadlines).
void kernel_check_binirr_counts(const Kernel &k) {
  uint64_t binaries = 0;
  for (Lit lit = 0; lit < static_cast<Lit>(2 * k.nvars); lit++)
    for (const WatchEntry w : k.watches.entries(lit))
      if (w.binary() && lit < w.other())
        binaries++;
  assert(binaries == k.watches.binary_clauses());
  uint64_t large = 0;
  for (const auto [ref, c] : arena_records(k.arena))
    if (!c->garbage() && !c->redundant())
      large++;
  assert(large == k.live_irredundant_large);
  (void)binaries;
  (void)large;
}
#endif

uint64_t kernel_irredundant_clauses(const Kernel &k) {
  // Kissat's BINIRR_CLAUSES is a live-clause statistic; `binary_ids` keeps
  // deleted pairs, so read `clauses_binary + clauses_irredundant` as kissat.
#ifndef NDEBUG
  kernel_check_binirr_counts(k);
#endif
  return k.watches.binary_clauses() + k.live_irredundant_large;
}

#ifndef NDEBUG
void kernel_check_invariants(const Kernel &k) {
  heuristics_check(k); // `H` sized to nvars (catches a missed Factor growth)
  // The proof never holds a root unit the kernel lacks, while the kernel is
  // consistent (a closing speculative round may leave retracted literals).
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
    if (a.affine) {
      assert(!a.binary && a.reason == AFFINE_REASON && a.level);
    } else if (!a.binary && a.reason != DECISION_REASON &&
               a.reason != UNIT_REASON) {
      assert(a.reason < k.arena.size());
      const Clause *reason = clause_at(k.arena, a.reason);
      assert((!reason->garbage() ||
              (reason->detached_reason() && reason->reason() &&
               reason->redundant())) &&
             "live assignment names an unprotected garbage reason");
    }
    // a literal sits at or after the start of its level's frame; true-level
    // assignment leaves the trail non-level-sorted, so no upper bound.
    assert(a.level == 0 || i >= k.frames[a.level]);
  }
  // every live large clause is watched on exactly lits[0] and lits[1], and the
  // eager-subsumption register holds only real clause starts. Redundancy is
  // not a register invariant: consumers re-test it (cadical analyze.cpp
  // eager_subsume_recent; kissat learn.c:163).
  uint32_t eager_seen = 0;
  for (const auto [ref, c] : arena_records(k.arena)) {
    for (const Ref r : k.eager_recent)
      if (r == ref)
        eager_seen++; // a live clause START; see the count check below
    if (!c->garbage() && k.large_watches_connected) {
      // ...except on a root-assigned watched literal: the sparse collect
      // drops a fixed literal's large entries (kissat collect.c, `if
      // (!lit_fixed)`) while the unswept prefix clause stays live. Pinned by
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

    // Eliminate's connected image may keep garbage large refs but must not
    // invent or lose binary copies, checked here so fuzzing attributes the
    // first bad transition.
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
  // binary watch symmetry: every (l -> other) has its mirror (other -> l)
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

  // propagation completeness: fully propagated + no conflict in flight means
  // no clause is falsified; an outstanding binary-only debt is no fixpoint.
  if (k.propagate_head == k.trail.size() &&
      k.binary_only_head >= k.propagate_head && !k.conflict_valid &&
      !k.pending.valid && !k.inconsistent) {
    for (Lit lit = 0; lit < 2 * k.nvars; lit++) {
      for (const WatchEntry w : k.watches.entries(lit)) {
        if (!w.binary())
          continue;
        // at a fixpoint, a binary watched by a false literal has its other
        // literal true (else a missed conflict or unit)
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
    // Dense elimination disconnects every large watch, so Propagate there
    // reaches a binary fixpoint only.
    if (k.large_watches_connected) {
      for (const auto [ref, c] : arena_records(k.arena))
        if (!c->garbage()) {
          int nonfalse = 0;
          Lit last = INVALID_LIT;
          for (const Lit l : *c)
            if (k.values[l] >= 0) {
              nonfalse++;
              last = l;
            }
          // fixpoint: no live clause may be falsified or unit
          assert(nonfalse > 0 && "falsified large clause escaped propagation");
          assert((nonfalse != 1 || k.values[last] > 0) &&
                 "unit large clause escaped propagation");
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
  k.projection_changed = true;
  if (k.query_projection)
    k.query_projection->reconstruction_literal(kernel_external_lit(k, witness));
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  rule_journal_note_reconstruction(k, witness, rest, nrest);
#endif
  for (uint32_t i = 0; i < nrest; i++)
    k.reconstruction.push_back(kernel_external_lit(k, rest[i]));
  k.reconstruction.push_back(kernel_external_lit(k, witness));
  // segment length word (clause size = witness + rest), read back last
  k.reconstruction.push_back(static_cast<Lit>(nrest + 1));
}

void kernel_push_cube_witness(Kernel &k, const Lit *clause, uint32_t nclause,
                              const Lit *witness, uint32_t nwitness) {
  assert(nclause && nwitness && clause && witness);
  k.projection_changed = true;
  if (k.query_projection)
    for (uint32_t i = 0; i < nwitness; ++i)
      k.query_projection->reconstruction_literal(witness[i]);
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
  // Single-witness segments end in a positive length; zero is a cube tag.
  k.reconstruction.push_back(0);
}

template <bool Seed>
static void extend_model(Kernel &k, bool materialize_internal,
                         std::span<const int32_t> seeds = {}) {
  if (k.model_extended)
    return;
  k.model_extended = true;
  // Replay needs a total external assignment (calculus.md, extend_ρ
  // premise): complete every value-0 variable to false, as tactix_value
  // reads 0; retired variables occur in no live clause.
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
  if constexpr (Seed) {
    for (const int32_t external : seeds) {
      const Lit literal = internalize(external);
      const Var ev = var_of(literal);
      assert(ev < k.external_to_internal.size());
      if (ev < k.fixed_external_values.size() && k.fixed_external_values[ev])
        continue;
      const Var iv = k.external_to_internal[ev];
      if (iv != INVALID_VAR &&
          (!kernel_var_retired(k, iv) || k.values[make_lit(iv, false)]))
        continue;
      k.model_values[literal] = 1;
      k.model_values[negate(literal)] = -1;
    }
  }
  // Replay rho in reverse: a false segment clause forces its witness (or
  // every literal of a tagged cube); identities are external.
  const auto satisfied = [&](size_t begin, size_t end) {
    for (size_t j = begin; j < end; j++)
      if (k.model_values[k.reconstruction[j]] > 0)
        return true;
    return false;
  };
  const auto make_true = [&](Lit literal) {
    k.model_values[literal] = 1;
    k.model_values[negate(literal)] = -1;
  };
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
      if (!satisfied(clause_begin, witness_begin))
        for (size_t j = witness_begin; j < i; j++)
          make_true(k.reconstruction[j]);
      i = clause_begin;
      continue;
    }
    assert(i >= len);
    const size_t begin = i - len;                // clause lits are [begin, i)
    const Lit witness = k.reconstruction[i - 1]; // pushed last before len
    if (!satisfied(begin, i))
      make_true(witness);
    i = begin;
  }
  if (materialize_internal) {
    // Keep the kernel debug view for live internal variables; model_values is
    // the public contract.
    for (Var iv = 0; iv < k.nvars; iv++) {
      const Var ev = k.internal_to_external[iv];
      const Lit ipos = make_lit(iv, false), epos = make_lit(ev, false);
      k.values[ipos] = k.model_values[epos];
      k.values[negate(ipos)] = k.model_values[negate(epos)];
    }
  }
}

void kernel_extend_model(Kernel &k, bool materialize_internal) {
  extend_model<false>(k, materialize_internal);
}

void kernel_extend_model_seeded(Kernel &k, std::span<const int32_t> seeds,
                                 bool materialize_internal) {
  extend_model<true>(k, materialize_internal, seeds);
}

} // namespace tactix
