#include "kernel/kernel.hpp"

#include "diagnostics/rule_journal.hpp"
#include "policies/event_trace.hpp"

#include "policies/heuristics.hpp" // the opaque `H`: Kernel's ctor/dtor + calls
#include "proof/proof.hpp" // kernel_delete_binary emits the delta atomically

#include <algorithm>
#include <cstdio>
#include <numeric>
#include <type_traits>
#include <utility>

namespace tactix {

void kernel_update_target_phases(Kernel &k) {
  if (k.h)
    heuristics_update_target_phases(k);
}

// The kernel does NOT own `H`: it is a non-owning `Heuristics *`
// (kernel.hpp), attached by whoever owns the storage (the session / a policy).
// So Kernel needs no ctor/dtor for it. heuristics.hpp is still included above —
// kernel_backtrack_impl<true> reads `k.h->…`, which needs the complete type.

void kernel_reserve_vars(Kernel &k, uint32_t nvars) {
  if (nvars <= k.nvars)
    return;
  const uint32_t old = k.nvars;
  k.nvars = nvars;
  k.internal_to_external.reserve(nvars);
  for (Var v = old; v < nvars; v++) {
    const Var external = static_cast<Var>(k.external_to_internal.size());
    k.internal_to_external.push_back(external);
    k.external_to_internal.push_back(v);
  }
  k.external_import_rank.resize(k.external_to_internal.size(), UINT32_MAX);
  // --- σ-state + derived-index growth (kernel-owned) ---
  k.values.resize(2 * nvars, 0);
  k.assigned.resize(nvars);
  // Only meaningful under `assigned[v].jumped`, so the initial value is never
  // read; grown unconditionally to keep the assign hot path branch-free.
  k.jumped_over.resize(nvars, 0);
  k.watches.resize_lits(2 * nvars);
  k.unit_ids.resize(nvars, 0);
  // BVE retry filter: a brand-new variable has never been attempted, so it
  // starts retry-worthy (kissat activate_variable marks it removed+added). A
  // derived index, not `H` (docs/implementation-notes.md §6) — kernel-side.
  k.bve_retry.resize(nvars, 1);
  k.factor_marks.resize(2 * nvars, 0);
  // Touched-epoch substrate: a brand-new variable has never been looked at by
  // any consumer, so it must read as touched against every existing watermark
  // — hence a real touch EVENT each, not a resize fill (kissat
  // activate_variable calls kissat_mark_added_literal for the same reason).
  k.touched.resize(nvars, 0);
  k.removed.resize(nvars, 0);
  // The band array needs no such event: a fresh variable is already touched
  // below, and C2 reads the two arrays as a union.
  k.touched_kept.resize(nvars, 0);
  for (uint32_t v = old; v < nvars; v++) {
    kernel_touch_var(k, v);
    // Variable activation marks the stripped (positive) literal in Kissat;
    // actual clause polarities are marked below by clause addition.
    k.factor_marks[make_lit(v, false)] = 1;
  }
  // Retirement status: a brand-new variable is Active (design R-a). Only grown
  // once something has retired a variable, so the common case stays empty.
  if (!k.var_status.empty())
    k.var_status.resize(nvars, VarStatus::Active);
  // Paired with bve_retry (init is irrelevant — the retry bit starts set, so the
  // first attempt always recomputes it before any read).
  k.bve_min_bound.resize(nvars, 0);
  k.seen.resize(nvars, 0);
  k.eager_marks.resize(2 * nvars, 0); // all-zero between uses; see the field
  k.marked_levels.resize(nvars + 1, 0);
  k.probing.stamp.resize(2 * nvars, 0);
  // The dense occurrence index is per-literal kernel state (null outside a
  // simplification phase). A var-adding rule (E2 Factor / BVA) can grow nvars
  // WHILE dense is connected — the shared index is built once per phase and
  // rules run under it (§5.3). Grow it here so no later consumer indexes a new
  // literal out of bounds. New (extension) literals occur in no large clause
  // yet, so empty entries are correct; add() maintains them from there.
  if (k.dense)
    k.dense->large.resize(2 * nvars);
  k.unassigned += nvars - old;
  k.unassigned_active += nvars - old; // fresh variables are Active (R-a)
  // fixed-capacity trail/frames: the inlined assign hot path must never
  // reallocate, and its push_back capacity branch becomes always-false
  k.trail.reserve(nvars);
  k.frames.reserve(nvars + 1);
  // --- search-guidance (`H`) growth: the extracted seam ---
  // Conditional on `H` being present (H5): an unguided solver (`k.h == nullptr`)
  // has no per-var `H` state to grow. DefaultPolicy always has `H` → unchanged.
  if (k.h)
    heuristics_reserve_vars(k, old, nvars);
}

void kernel_record_import(Kernel &k, Var v) {
  assert(v < k.nvars);
  const Var external = k.internal_to_external[v];
  if (k.external_import_rank.size() <= external)
    k.external_import_rank.resize(external + 1, UINT32_MAX);
  uint32_t &rank = k.external_import_rank[external];
  if (rank == UINT32_MAX)
    rank = k.next_import_rank++;
}

uint64_t kernel_import_var_order(const Kernel &k, Var v) {
  assert(v < k.nvars);
  const Var external = k.internal_to_external[v];
  if (external < k.external_import_rank.size() &&
      k.external_import_rank[external] != UINT32_MAX)
    return k.external_import_rank[external];
  // Direct kernel clients need no input finalization. Put their unrecorded
  // variables after recorded imports in stable external order.
  return static_cast<uint64_t>(k.next_import_rank) + external;
}

uint64_t kernel_import_lit_order(const Kernel &k, Lit lit) {
  return 2 * kernel_import_var_order(k, var_of(lit)) + negated(lit);
}

void kernel_vars_in_import_order(const Kernel &k, std::vector<Var> &order) {
  order.resize(k.nvars);
  // Before variable compaction, finish_input gives every live variable one
  // dense import rank.  Invert that permutation in linear time: comparison
  // sorting tens of millions of stable identities only reconstructs the same
  // order Kissat already has in its compact internal indices.
  if (k.next_import_rank == k.nvars) {
    for (Var v = 0; v < k.nvars; v++) {
      const uint64_t rank = kernel_import_var_order(k, v);
      assert(rank < k.nvars);
      order[rank] = v;
    }
    return;
  }
  // Compaction can leave holes in the permanent external import ranks.
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&](Var a, Var b) {
    return kernel_import_var_order(k, a) < kernel_import_var_order(k, b);
  });
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

Var kernel_new_extension_vars(Kernel &k, uint32_t count) {
  const Var first = k.nvars;
  if (k.first_extension_var == UINT32_MAX) {
    k.first_extension_var = first; // boundary = first non-original var
    k.original_external_vars =
        static_cast<uint32_t>(k.external_to_internal.size());
  }
  kernel_reserve_vars(k, k.nvars + count); // additive: grows all per-var state
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

Ref kernel_add_large_clause(Kernel &k, const Lit *lits, uint32_t size,
                            bool redundant, uint32_t glue, uint64_t id) {
  assert(size >= 3);
  kernel_rearm_propagation(k, lits, size);
  const Ref ref = static_cast<Ref>(k.arena.size());
  k.arena.resize(k.arena.size() + clause_words(size));
  Clause *c = clause_at(k.arena, ref);
  c->size = size;
  // redundant clauses start fully "used" (kissat learn.c: used = MAX_USED)
  c->flags = redundant ? (Clause::REDUNDANT | Clause::USED_MASK) : 0;
  c->set_glue(glue);
  c->searched = 2;
  c->set_id(id);
  for (uint32_t i = 0; i < size; i++)
    c->lits[i] = lits[i];
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  rule_journal_note_large_add(k, lits, size, id, redundant, glue);
#endif
  k.watches.push_large(lits[0], lits[1], ref);
  k.watches.push_large(lits[1], lits[0], ref);
  // Irredundant clauses only, for all three consumers: the dense index (Refs
  // are arena offsets, stable across the resize above), the BVE retry filter,
  // and the touched-epoch substrate. Redundant clauses are excluded on purpose
  // — BVE does not resolve them and learned-clause traffic would mark
  // everything (kissat marks the same population, clause.c:98). Deviation:
  // kissat marks on REMOVAL only, which is measured negative here.
  if (!redundant)
    for (uint32_t i = 0; i < size; i++) {
      kernel_mark_bve_retry(k, var_of(lits[i]));
      kernel_touch_var(k, var_of(lits[i]));
      k.factor_marks[lits[i]] = 1;
      if (k.dense)
        k.dense->large[lits[i]].push_back(ref);
    }
  // The redundant half of the touch substrate (see Kernel::touch_redundant).
  // Counted both ways even when stamping is off, because a single count cannot
  // show the predicate tracks the live tier. The stamp goes to `touched_kept`,
  // never to `touched` — the split keeps the other consumers' filters intact.
  if (redundant) {
    const bool dyn = kernel_likely_kept(k, true, glue, size);
    k.stats.touch_redundant_dynamic += dyn;
    k.stats.touch_redundant_static6 += (glue <= 6); // reduce.hpp TIER2_STATIC
    if (dyn && k.touch_redundant)
      for (uint32_t i = 0; i < size; i++)
        kernel_touch_kept_var(k, var_of(lits[i]));
  }
  return ref;
}

uint64_t kernel_clause_order(const Kernel &k, uint64_t id) {
  const auto found = k.clause_order_alias.find(id);
  return found == k.clause_order_alias.end() ? id : found->second;
}

void kernel_inherit_clause_order(Kernel &k, uint64_t replacement_id,
                                 uint64_t source_id) {
  // Id 0 is "no proof identity", the documented default of the clause-add API
  // (kernel.hpp) and the state every kernel built without a proof is in. The
  // alias map is keyed by clause id, so there is nothing to key and nothing to
  // inherit; the order consumers already fall back to the arena Ref for an
  // id-less clause (vivify.cpp VivifyArenaOrder::order).
  if (!replacement_id || !source_id)
    return;
  k.clause_order_alias.insert_or_assign(replacement_id,
                                        kernel_clause_order(k, source_id));
}

static void note_binary_add(Kernel &k, Lit a, Lit b, uint64_t id,
                            bool new_pair) {
  // LAST id wins. The map must always name a clause the checker still holds,
  // and kernel_delete_binary can retire this pair's id, so a re-add must
  // install its fresh one — otherwise every later LRAT chain through the pair
  // cites a deleted clause. Among live duplicates either id is equally valid.
  k.binary_ids.insert_or_assign_known(kernel_binary_key(a, b), id, new_pair);
  if (k.binary_liveness)
    k.binary_liveness->add(kernel_binary_key(a, b));
  // BVE retry filter: binaries have an irredundant lifecycle in this kernel,
  // so every added pair is new elimination material for both variables
  // (removal-only marking measured and rejected — see kernel_add_large_clause).
  kernel_mark_bve_retry(k, var_of(a));
  kernel_mark_bve_retry(k, var_of(b));
  // Touched-epoch substrate: a binary is the strongest subsumer/partner there
  // is, so both variables are new material for every consumer (kissat
  // new_binary_clause marks both unconditionally, clause.c:71-72 — including
  // learned binaries, which is where this over-touches on purpose).
  kernel_touch_var(k, var_of(a));
  kernel_touch_var(k, var_of(b));
  k.factor_marks[a] = 1;
  k.factor_marks[b] = 1;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  rule_journal_note_binary_add(k, a, b, id);
#endif
}

void kernel_add_binary_clause(Kernel &k, Lit a, Lit b, uint64_t id) {
  const bool new_pair = !kernel_binary_live(k, a, b);
  event_trace_binary_add(k, a, b);
  kernel_rearm_propagation(k, a, b);
  k.watches.push_binary(a, b);
  k.watches.push_binary(b, a);
  note_binary_add(k, a, b, id, new_pair);
}

void kernel_add_binary_mirror(Kernel &k, Lit stable, Lit moved, uint64_t id) {
  // The stable list already contains (stable -> moved), rewritten in place by
  // the caller. Adding the opposite direction restores the mirrored-watch
  // invariant without appending and immediately discarding a stable duplicate.
  kernel_rearm_propagation(k, stable, moved);
  k.watches.push_binary(moved, stable);
  note_binary_add(k, stable, moved, id, false);
}

void kernel_publish_binary_clause(Kernel &k, Lit a, Lit b, uint64_t id,
                                  bool new_pair) {
  kernel_rearm_propagation(k, a, b);
  note_binary_add(k, a, b, id, new_pair);
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
      ax.reason = UNIT_REASON;  // root literal is a unit, already has its id
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
  }
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
  kernel_mark_bve_retry(k, var_of(a));
  kernel_mark_bve_retry(k, var_of(b));
  kernel_removed_var(k, var_of(a)); // BCE's side of the touched-epoch substrate
  kernel_removed_var(k, var_of(b));
  return true;
}

// DRAT-only variant for E2 Factor: remove EVERY copy of the pair (a,b). A
// factoring replaces its whole grid with the aux-var definition, so a
// duplicated grid binary must go entirely, where plain kernel_delete_binary
// refuses copies != 1 to protect a surviving copy's later LRAT chain. Here no
// copy survives, and Factor no-ops under native LRAT (D-FACTOR-LRAT), so that
// ambiguity never reaches a checker. Only Factor's exact grid pairs are
// removed. Emits one DRAT `del` per copy, and returns true iff any was removed.
bool kernel_delete_binary_all_copies(Kernel &k, Lit a, Lit b) {
  const uint32_t copies = kernel_binary_copies(k, a, b);
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
      ax.reason = UNIT_REASON;
    }
  }
  // remove ALL matching mirror watch entries by in-place compaction
  for (const Lit x : {a, b}) {
    const Lit other = (x == a) ? b : a;
    k.watches.compact(x, [&](WatchEntry w) { // drop every matching copy
      return !(w.binary() && w.other() == other);
    });
  }
  if (k.binary_liveness)
    k.binary_liveness->remove(kernel_binary_key(a, b));
  if (k.proof) {
    const Lit lits2[2] = {a, b};
    for (uint32_t i = 0; i < copies; i++)
      k.proof->del(k.next_clause_id, kernel_binary_id(k, a, b), lits2, 2);
  }
  k.stats.binaries_deleted += copies;
  kernel_mark_bve_retry(k, var_of(a)); // BVE retry filter (see above)
  kernel_mark_bve_retry(k, var_of(b));
  kernel_removed_var(k, var_of(a));
  kernel_removed_var(k, var_of(b));
  return true;
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
  assert(removed && "unwatch of a clause this literal does not watch");
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

// See kernel.hpp for the contract. Two occurrence sources, mirroring how BVE
// itself reads the formula: binaries from the watch lists, irredundant large
// clauses from the shared dense index when a simplification phase has attached
// one, else one arena scan. Redundant clauses are irrelevant — BVE never
// resolves them, so a fixed literal inside one changes nothing it computes.
void kernel_bve_retry_sync_fixed(Kernel &k) {
  const uint32_t fixed = kernel_root_fixed(k);
  if (k.bve_retry_fixed_head >= fixed)
    return;
  const uint32_t from = k.bve_retry_fixed_head;
  k.bve_retry_fixed_head = fixed;
  for (uint32_t i = from; i < fixed; i++) {
    const Var v = var_of(k.trail[i]);
    if (k.assigned[v].level != 0)
      continue; // an out-of-order literal above the root frame is not fixed yet
    kernel_mark_bve_retry(k, v);
    for (const Lit l : {make_lit(v, false), make_lit(v, true)}) {
      // binaries: the partner's neighbourhood changed
      for (const WatchEntry w : k.watches.entries(l))
        if (w.binary())
          kernel_mark_bve_retry(k, var_of(w.other()));
      if (k.dense)
        for (const Ref ref : k.dense->large[l]) {
          const Clause *c = clause_at(k.arena, ref);
          if (c->garbage())
            continue;
          for (const Lit m : *c)
            kernel_mark_bve_retry(k, var_of(m));
        }
    }
  }
  if (k.dense)
    return;
  // no phase index: one arena scan, marking every irredundant large clause that
  // contains one of the newly fixed variables.
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    if (!c->garbage() && !c->redundant()) {
      bool touched = false;
      for (const Lit m : *c) {
        const Var mv = var_of(m);
        if (k.values[m] != 0 && k.assigned[mv].level == 0 &&
            k.assigned[mv].trail >= from) {
          touched = true;
          break;
        }
      }
      if (touched)
        for (const Lit m : *c)
          kernel_mark_bve_retry(k, var_of(m));
    }
    ref += words;
  }
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
  // Compacting backtrack (docs/implementation-notes.md, D-LEVEL-2): true-level
  // assignment leaves the trail unsorted by level, so it cannot be truncated by
  // position. Scan the suffix above the target frame and KEEP every literal
  // whose true level <= target, unassigning only those above it. Inert while
  // chrono is off, where nothing is kept and this is plain truncation.
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
      if (k.retire_enabled && kernel_var_retired(k, v)) {
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
                                     Ref *large_ref) {
  // `id` is the REPLACEMENT's id, and every caller allocates it immediately
  // before calling in — so it equals `next_clause_id`, which is what the shared
  // primitive prefixes its LRAT deletion line with. That was an unstated,
  // uncheckable invariant while this site passed `id` by hand; assert it.
  assert(id == k.next_clause_id);
  kernel_inherit_clause_order(k, id, c->id());
  kernel_delete_clause(k, c); // before any arena-mutating add below
  if (size == 0) { // under-derived root only (corpus 002): the ADD just
    k.inconsistent = true; // emitted IS the refutation
    return ReplaceOutcome::Refuted;
  }
  if (size == 1)
    return ReplaceOutcome::Unit; // caller owns unit semantics
  if (size == 2) {
    kernel_add_binary_clause(k, lits[0], lits[1], id);
    return ReplaceOutcome::Binary;
  }
  const Ref r = kernel_add_large_clause(k, lits, size, redundant, glue, id);
  if (large_ref)
    *large_ref = r;
  return ReplaceOutcome::Large;
}

RootFlushOutcome kernel_flush_root_clause(Kernel &k, Ref *ref,
                                          uint64_t *changed) {
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
      if (kernel_shadow_retire(k, c->id()))
        k.stats.c1_shadow_deleted++;
      kernel_delete_clause(k, c);
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
    for (const Lit assigned_lit : k.trail) {
      const Var v = var_of(assigned_lit);
      const Assigned &a = k.assigned[v];
      if (!a.binary && a.reason == original_ref) {
        assert(reason_var == INVALID_VAR);
        reason_var = v;
      }
    }

  if (kernel_shadow_retire(k, c->id()))
    k.stats.c1_shadow_rewritten++;
  const uint64_t original_id = c->id();
  const bool redundant = c->redundant();
  const uint32_t glue = kernel_shrunken_glue(
      k, c->glue(), static_cast<uint32_t>(kept.size()));
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
  switch (kernel_replace_clause(k, c, id, kept.data(),
                                static_cast<uint32_t>(kept.size()), redundant,
                                redundant ? glue : 0, ref)) {
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
    if (reason_var != INVALID_VAR) {
      Assigned &a = k.assigned[reason_var];
      a.reason = *ref;
      clause_at(k.arena, *ref)->flags |= Clause::REASON;
    }
    if (k.pending.valid && k.pending.ref == original_ref) {
      k.pending.ref = *ref;
      clause_at(k.arena, *ref)->flags |= Clause::REASON;
    }
    return RootFlushOutcome::Rewritten;
  }
  assert(false);
  return RootFlushOutcome::Refuted;
}

RootCollectOutcome kernel_sparse_collect_root(Kernel &k, Ref start) {
  k.stats.sparse_collections++;
  if (!kernel_has_root_assignment(k))
    return RootCollectOutcome::NoChange;
  // Rewrites allocate a new variable-width record, but Kissat's collector
  // emits that record at the old clause's position. Preserve the same relative
  // order explicitly; otherwise tens of thousands of shortened clauses move
  // to the arena tail and change every capped downstream environment.
  struct RootArenaOrder final : ArenaOrdering {
    std::vector<Ref> desired;
    void order(const Kernel &k, std::vector<Ref> &refs) override {
      if (refs.size() != desired.size())
        return; // defensive: never drop an unaccounted live clause
#ifndef NDEBUG
      std::vector<uint8_t> seen(k.arena.size());
      for (const Ref ref : refs)
        seen[ref] = 1;
      for (const Ref ref : desired)
        if (ref >= seen.size() || !seen[ref])
          return;
        else
          seen[ref] = 0;
#else
      (void)k;
#endif
      refs = desired;
    }
  } root_order;
  uint64_t changed = 0;
  bool has_garbage = false;
  RootCollectOutcome outcome = RootCollectOutcome::NoChange;

  // A Reduce collection starts at the first redundant clause.  The live
  // irredundant prefix is copied byte-for-byte and is not root-flushed, just
  // as in Kissat's `sparse_sweep_garbage_clauses(start)`.
  for (Ref ref = 0; ref < start;) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage())
      root_order.desired.push_back(ref);
    else
      has_garbage = true;
    ref += clause_words(c->size);
  }
  const size_t prefix_clauses = root_order.desired.size();

  // Kissat's sparse collector flushes watched binaries before sweeping the
  // large-clause arena (collect.c:flush_watched_clauses_by_literal).  In
  // particular it drops binaries satisfied by any root literal.  Keeping
  // those pairs made initial collection create 216 shortened binaries on
  // FmlaEquivChain without retiring the 216 already-satisfied pairs, shifting
  // every later Factor score even though the root assignment was identical.
  // Snapshot unique pairs first because deletion compacts both watch lists.
  std::vector<std::pair<Lit, Lit>> satisfied_binaries;
  for (Lit lit = 0; lit < static_cast<Lit>(2 * k.nvars); lit++)
    for (const WatchEntry watch : k.watches.entries(lit)) {
      if (!watch.binary())
        continue;
      const Lit other = watch.other();
      if (kernel_import_lit_order(k, lit) >=
          kernel_import_lit_order(k, other))
        continue;
      if (kernel_root_value(k, lit) > 0 ||
          kernel_root_value(k, other) > 0)
        satisfied_binaries.emplace_back(lit, other);
    }
  std::sort(satisfied_binaries.begin(), satisfied_binaries.end());
  satisfied_binaries.erase(
      std::unique(satisfied_binaries.begin(), satisfied_binaries.end()),
      satisfied_binaries.end());
  for (const auto &[a, b] : satisfied_binaries) {
    const uint32_t copies = kernel_binary_copies(k, a, b);
    if (!copies)
      continue;
    const bool deleted = copies == 1 ? kernel_delete_binary(k, a, b)
                                     : kernel_delete_binary_all_copies(k, a, b);
    assert(deleted);
    (void)deleted;
    changed += copies;
    outcome = RootCollectOutcome::Changed;
  }

  const Ref original_end = static_cast<Ref>(k.arena.size());
  for (Ref ref = start; ref < original_end;) {
    const uint32_t words = clause_words(clause_at(k.arena, ref)->size);
    if (!clause_at(k.arena, ref)->garbage()) {
      Ref current = ref;
      const RootFlushOutcome flushed =
          kernel_flush_root_clause(k, &current, &changed);
      if (flushed == RootFlushOutcome::Refuted) {
        outcome = RootCollectOutcome::Refuted;
        break;
      }
      if (flushed == RootFlushOutcome::Unit)
        outcome = RootCollectOutcome::Unit;
      else if (flushed != RootFlushOutcome::None &&
               outcome == RootCollectOutcome::NoChange)
        outcome = RootCollectOutcome::Changed;
      if (flushed == RootFlushOutcome::None ||
          flushed == RootFlushOutcome::Rewritten)
        root_order.desired.push_back(current);
    } else
      has_garbage = true;
    ref += words;
  }
  k.stats.sparse_collected += changed;
  // Reduce marks its selected victims before entering the root-aware sparse
  // collector.  Those records still require the same sweep and rewatch even
  // when no root-fixed literal happens to shorten another clause in this
  // collection.  `changed` only describes root simplification, so it cannot
  // by itself be the collection admission predicate.
  if ((changed || has_garbage) && outcome != RootCollectOutcome::Refuted) {
    // Root shortening appends packed replacement records, while Kissat writes
    // them at the swept source position.  `desired` restores those positions.
    // Replacements inherited before this collection can already be physically
    // appended, so replay every swept record's persistent source-position key
    // before composing the collector's stable move of tail irredundant clauses
    // ahead of tail redundant clauses.  The unswept prefix remains untouched.
    const auto tail = root_order.desired.begin() + prefix_clauses;
    std::vector<std::pair<uint64_t, Ref>> source_order;
    source_order.reserve(root_order.desired.end() - tail);
    for (auto it = tail; it != root_order.desired.end(); ++it) {
      const Clause *c = clause_at(k.arena, *it);
      source_order.emplace_back(
          c->id() ? kernel_clause_order(k, c->id()) : *it, *it);
    }
    std::stable_sort(source_order.begin(), source_order.end(),
                     [](const auto &a, const auto &b) {
                       return a.first < b.first;
                     });
    for (size_t i = 0; i < source_order.size(); ++i)
      tail[i] = source_order[i].second;
    std::stable_partition(tail, root_order.desired.end(), [&k](Ref ref) {
      return !clause_at(k.arena, ref)->redundant();
    });
    ArenaOrdering *const saved = k.arena_ordering;
    k.arena_ordering = &root_order;
    kernel_collect_garbage(k, start, /*apply_sparse_order=*/true);
    k.arena_ordering = saved;
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
                      h.has_large, h.binaries_first});
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
template <bool RECORD, bool BEYOND = false>
static bool propagate_literal(Kernel &k, Lit lit, Ref ignore) {
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
  uint64_t ticks = 1 + kernel_cache_lines(end - p);

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
      kernel_assign_binary(k, blocking, not_lit);
      ticks++;
      k.stats.propagations++;
    } else {
      const Ref ref = *q++ = *p++;
      if (blocking_value > 0)
        continue;
      ticks++;
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
      } else if (other_value < 0) { // conflict
        // Kissat tests the ignored candidate only after performing any legal
        // watch relocation above.  Skipping at dereference time preserves a
        // different clause pose and can change a later antecedent choice.
        if (ref == ignore)
          continue;
        conflict = true;
        if constexpr (!BEYOND) {
          k.conflict_valid = true;
          k.conflict_ref = ref;
          break;
        }
      } else { // unit: assign `other` with this clause as reason
        if (ref == ignore)
          continue;
        kernel_assign_ref(k, other, ref);
        ticks++;
        k.stats.propagations++;
      }
    }
  }
  while (p != end)
    *q++ = *p++;
  // no snapshot needed here: q != begin + size only if an entry was dropped
  // above, and every drop site snapshots first
  k.watches.truncate(not_lit, static_cast<uint32_t>(q - begin));
  flush_delayed<RECORD>(k);
  kernel_charge_ticks(k, ticks);
  return conflict;
}

bool kernel_propagate(Kernel &k, Ref ignore) {
  while (k.propagate_head < k.trail.size()) {
    const Lit lit = k.trail[k.propagate_head];
    if (propagate_literal<false>(k, lit, ignore)) {
      // do NOT advance past a literal whose scan broke on the conflict:
      // if a later backjump keeps this literal, the unscanned tail of its
      // watch list must be revisited — otherwise its implications are
      // lost forever
      return true;
    }
    k.propagate_head++;
  }
  return false;
}

void kernel_propagate_beyond_conflicts(Kernel &k) {
  assert(k.warming);
  const uint64_t before = k.stats.propagations;
  while (k.propagate_head < k.trail.size()) {
    const Lit lit = k.trail[k.propagate_head++];
    if (propagate_literal<false, true>(k, lit, INVALID_REF))
      k.stats.warming_conflicts++;
  }
  k.stats.warming_propagations += k.stats.propagations - before;
}

// binary-only propagation of `lit` (kissat backbone.c backbone_propagate /
// proprobe.c binary path): follow ONLY binary implications — large watches
// are skipped, never replaced or compacted (binary watches are permanent, so
// there is no watch mutation and no delayed relocation). Far cheaper than the
// full engine; the units it derives are a subset of full propagation's. A
// binary conflict (not_lit, other) is recorded exactly like the full engine.
static bool propagate_literal_binary(Kernel &k, Lit lit) {
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
  const bool stop_early = k.watches.heads[not_lit].binaries_first;
  // Residual pricing (Stats::binprop_*): on the scans the flag did NOT cover,
  // `ordered` records whether the list was in fact binary-first anyway (what a
  // per-LIST order bit would add over the reference's single global flag) and
  // `large_words` the words those scans still spend stepping over large
  // entries.
  bool large_seen = false, ordered = true;
  uint64_t large_words = 0;
  bool conflict = false;
  // Not WatchLists::entries: `binprop_words`/`binprop_skipped` below price the
  // word cursor where the scan stopped, and the conflict break leaves it past
  // the conflicting binary. An entry iterator stops before the entry it breaks
  // on, so the stride is load-bearing here beyond iteration.
  while (p != end) {
    const Watch w{*p};
    if (!w.binary()) { // large watch occupies two slots; skipped here
      if (stop_early)
        break;
      large_seen = true;
      large_words += 2;
      p += 2;
      continue;
    }
    ordered = ordered && !large_seen;
    p += 1;
    const Lit other = w.other();
    const int8_t other_value = values[other];
    if (other_value > 0)
      continue;
    if (other_value < 0) { // conflict on the binary (not_lit, other)
      k.conflict_valid = true;
      k.conflict_ref = INVALID_REF;
      k.conflict_bin[0] = not_lit;
      k.conflict_bin[1] = other;
      conflict = true;
      break;
    }
    kernel_assign_binary(k, other, not_lit);
    k.stats.propagations++;
  }
  kernel_charge_ticks(k, 1 + kernel_cache_lines(p - begin));
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
  return conflict;
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
      h.size = s.size;
      h.has_large = s.has_large;
      h.binaries_first = s.binaries_first;
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
  const bool lrat = k.proof && k.proof->lrat();
  bool conflict = false;
  if (!k.probing.recording) {
    conflict = kernel_propagate(k, ignore); // no open scope: nothing to record
  } else {
    while (k.propagate_head < k.trail.size()) {
      const Lit lit = k.trail[k.propagate_head];
      const bool found = k.probing.rollback_pose
                             ? propagate_literal<true, false>(k, lit, ignore)
                             : propagate_literal<false, false>(k, lit, ignore);
      if (found) {
        conflict = true; // same "do not advance past the conflicting scan" rule
        break;
      }
      k.propagate_head++;
    }
  }
  if (lrat)
    kernel_emit_root_units(k, assigned_before);
  return conflict;
}

// binary-only propagation to fixpoint or first conflict (V2 Backbone). Same
// contract as kernel_propagate but through the binary-implication graph only.
bool kernel_propagate_binary(Kernel &k) {
  while (k.propagate_head < k.trail.size()) {
    const Lit lit = k.trail[k.propagate_head];
    if (propagate_literal_binary(k, lit))
      return true;
    k.propagate_head++;
  }
  return false;
}

// Garbage collection after kissat/src/collect.c (MIT), simplified: copy
// live clauses into a fresh arena in order, leave a forwarding pointer in
// the old header (`searched` is dead there), then fix reasons and rebuild
// the large-watch entries. Rebuilt clauses select their best current watched
// pair into lits[0]/lits[1]. With a nonzero watermark, match Kissat's sparse
// collector: retain the existing watches for the live, unmoved prefix and only
// rebuild watches for the relocated tail.
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

void kernel_collect_garbage(Kernel &k, Ref watch_start,
                            bool apply_sparse_order) {
  // No probing scope may be open: this relocates every Ref, so an outstanding
  // snapshot would roll pre-collection watch words (and ClauseSnap refs) back
  // over the compacted arena. Long an implicit assumption of the probing log;
  // stated here because the per-list order bits rest on it too: restoring an
  // older snapshot after collection could otherwise invalidate them.
  assert(!k.probing.recording);
  // The eager-subsumption register holds raw arena Refs and this function
  // relocates every one of them, so it is reset here rather than forwarded —
  // kissat does exactly the same at each of its three collect sites
  // (kissat_reset_last_learned, collect.c:274/514/702). Forwarding instead
  // would buy at most four extra candidates and would need `searched` read off
  // a possibly-garbage clause, the corpus-003 shape.
  for (Ref &r : k.eager_recent)
    r = INVALID_REF;
  // A sparse watermark must fall exactly on an arena-record boundary.  Prefix
  // garbage is legal in Tactix because clauses retired by an earlier rule can
  // remain marked until Reduce; its live watch entries are forwarded below.
  if (watch_start > k.arena.size())
    watch_start = 0;
  for (Ref ref = 0; ref < watch_start;) {
    const Clause *c = clause_at(k.arena, ref);
    ref += clause_words(c->size);
    if (ref > watch_start) {
      watch_start = 0;
      break;
    }
  }
  size_t live_words = 0, live_clauses = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    if (!c->garbage()) {
      live_words += words;
      live_clauses++;
    }
    ref += words;
  }
  assert(live_words <= UINT32_MAX);
  std::vector<Ref> order;
  order.reserve(live_clauses);
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    if (!c->garbage())
      order.push_back(ref);
    ref += words;
  }
  Arena fresh;
  // Appending rewrites can leave a large garbage suffix. Reserving the old
  // arena size here needlessly kept that suffix alive in both arenas during
  // collection; the live-word prepass is the exact Kissat-style compacted
  // image and also prevents geometric growth while copying it.
  fresh.reserve(live_words);
  // Arena-order policies belong to whole-arena/inprocessing collection.
  // Kissat's sparse collector preserves the prefix, sweeps the tail in its
  // existing order, then stably moves surviving redundant clauses behind all
  // surviving irredundant clauses in that tail (collect.c:
  // move_redundant_clauses_to_the_end).  This partition is observable when
  // the tail is rewatched, and is independent of the optional policy order.
  if (watch_start && !apply_sparse_order) {
    const auto tail = std::lower_bound(order.begin(), order.end(), watch_start);
    std::stable_partition(tail, order.end(), [&k](Ref ref) {
      return !clause_at(k.arena, ref)->redundant();
    });
  } else if (k.arena_ordering)
    k.arena_ordering->order(k, order);
#ifndef NDEBUG
  std::vector<uint8_t> seen(k.arena.size());
  for (const Ref ref : order) {
    assert(ref < k.arena.size() && !seen[ref]);
    assert(!clause_at(k.arena, ref)->garbage());
    seen[ref] = 1;
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    assert(c->garbage() || seen[ref]);
    ref += clause_words(c->size);
  }
#endif
  // In Kissat the live prefix occupies exactly `watch_start` words.  Prefix
  // garbage is a Tactix representation difference, so compute where the tail
  // begins after that garbage is omitted from the fresh arena.
  Ref rewatch_start = 0;
  if (watch_start)
    for (const Ref ref : order) {
      if (ref >= watch_start)
        break;
      rewatch_start += clause_words(clause_at(k.arena, ref)->size);
    }
  for (const Ref ref : order) {
    Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    const Ref nref = static_cast<Ref>(fresh.size());
    fresh.insert(fresh.end(), k.arena.begin() + ref,
                 k.arena.begin() + ref + words);
    // Kissat's sparse sweep starts at `watch_start`: the irredundant prefix
    // is neither copied nor rewatched there, so its replacement-search cursor
    // remains search-visible state.  Tactix copies the whole arena into
    // `fresh`, so the same prefix cursor must survive while the relocated tail
    // reconnected below resets.  Whole-arena collection has
    // `watch_start == 0` and still resets every clause.
    clause_at(fresh, nref)->searched =
        ref < watch_start ? c->searched : 2;
    c->searched = nref;
  }
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
  for (Lit lit = 0; lit < 2 * k.nvars; lit++) {
    k.watches.compact(lit, [&k, watch_start, lit](WatchEntry w) {
      return w.binary() ||
             (kernel_root_value(k, lit) == 0 && w.ref() < watch_start &&
              !clause_at(k.arena, w.ref())->garbage());
    });
    k.watches.map_large_refs(lit, [&k](Ref ref) {
      return clause_at(k.arena, ref)->searched;
    });
  }
  k.arena.swap(fresh);
  // GC relocated every Ref: the dense occurrence index (increment 2) must be
  // rebuilt from the compacted arena in lock-step with the watches below (it
  // held offsets into the old arena, now stale). Clearing here also drops the
  // garbage Refs that accumulated lazily during the phase.
  if (k.dense)
    k.dense->large.assign(2 * k.nvars, {});
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    if (ref >= rewatch_start) {
      // collect.c:rewatch_clauses selects the best current watched pair before
      // appending the relocated clause to its two watch lists.
      sort_clause_watches_like_kissat(k, c);
      k.watches.push_large(c->lits[0], c->lits[1], ref);
      k.watches.push_large(c->lits[1], c->lits[0], ref);
    }
    if (k.dense && !c->redundant())
      for (const Lit l : *c)
        k.dense->large[l].push_back(ref);
    ref += clause_words(c->size);
  }
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
void kernel_rebuild_large_watches(Kernel &k) {
  for (Lit lit = 0; lit < 2 * k.nvars; lit++)
    k.watches.compact(lit, [](WatchEntry w) { return w.binary(); });
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    if (!c->garbage()) {
      // `kissat_watch_large_clauses` does not merely reconnect the existing
      // pair: it first runs `kissat_sort_literals` under the current root
      // assignment.  In particular, root-false literals must not remain in
      // front of unassigned literals after Probe/Eliminate.  The selected
      // pair is search-visible state and can control which of several valid
      // antecedents propagation records.
      sort_clause_watches_like_kissat(k, c);
      c->searched = 2;
      k.watches.push_large(c->lits[0], c->lits[1], ref);
      k.watches.push_large(c->lits[1], c->lits[0], ref);
    }
    ref += words;
  }
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
  for (const Var variable : ordered_variables)
    for (uint32_t sign = 0; sign < 2; sign++) {
      const Lit lit = make_lit(variable, sign != 0);
      for (const WatchEntry watch : k.watches.entries(lit)) {
        if (!watch.binary())
          continue;
        const Lit other = watch.other();
        if (kernel_import_lit_order(k, lit) >=
            kernel_import_lit_order(k, other))
          continue;
        if (kernel_root_value(k, lit) > 0 ||
            kernel_root_value(k, other) > 0)
          satisfied.emplace_back(lit, other);
      }
    }
  for (const auto &[a, b] : satisfied) {
    const uint32_t copies = kernel_binary_copies(k, a, b);
    if (copies == 1)
      kernel_delete_binary(k, a, b);
    else if (copies > 1)
      kernel_delete_binary_all_copies(k, a, b);
  }
  // Save each surviving pair in that same compact-literal encounter order.
  // Unlike Congruence, Walk does not sort equal-first runs; the current watch
  // order of a pair whose lower endpoint is shared is retained verbatim.
  std::vector<std::pair<Lit, Lit>> binaries;
  for (const Var variable : ordered_variables)
    for (uint32_t sign = 0; sign < 2; sign++) {
      const Lit lit = make_lit(variable, sign != 0);
      for (const WatchEntry watch : k.watches.entries(lit))
        if (watch.binary() && kernel_import_lit_order(k, lit) <
                                  kernel_import_lit_order(k, watch.other()))
          binaries.emplace_back(lit, watch.other());
    }

  std::vector<uint32_t>().swap(k.watches.arena);
  k.watches.holes = 0;
  for (WatchHead &head : k.watches.heads)
    head = WatchHead{};
  for (const auto &[a, b] : binaries) {
    k.watches.heads[a].size++;
    k.watches.heads[b].size++;
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    if (!c->garbage()) {
      sort_clause_watches_like_kissat(k, c);
      c->searched = 2;
      k.watches.heads[c->lits[0]].size += 2;
      k.watches.heads[c->lits[1]].size += 2;
    }
    ref += words;
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
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    if (!c->garbage()) {
      const Lit first = c->lits[0], second = c->lits[1];
      push_word(first, blocking_watch(second).raw);
      push_word(first, ref);
      push_word(second, blocking_watch(first).raw);
      push_word(second, ref);
      k.watches.heads[first].has_large = true;
      k.watches.heads[second].has_large = true;
    }
    ref += words;
  }
}

void kernel_canonicalize_binary_watches(Kernel &k,
                                        bool count_congruence_rebuild,
                                        const std::vector<std::pair<Lit, Lit>> *
                                            binary_suffix) {
  // Kissat Congruence enters dense mode with a non-null binary save stack.
  // `dense.c:flush_large_watches` walks literals globally, keeps the first
  // copy of each binary, and saves a pair only at its smaller literal. Then
  // `extract_binaries` sorts every equal-first run by its second literal before
  // sparse resume recreates both mirrors. Thus the surviving original pairs
  // are globally ordered by (min internal literal, max internal literal), not
  // by their first occurrence in a watch list. Tactix models compacted
  // internal indices with import order; sorting by the corresponding pair key
  // preserves that reference-visible order for capped Sweep environments.
  size_t binary_watch_entries = 0;
  for (Lit lit = 0; lit < 2 * k.nvars; lit++)
    for (const WatchEntry w : k.watches.entries(lit))
      binary_watch_entries += w.binary();

  std::vector<std::pair<Lit, Lit>> binaries;
  // Every retained binary has two mirrored watch entries.  Reserve their
  // upper bound before the import-order scan so a large dense instance never
  // pays the transient memory of geometric vector growth.
  binaries.reserve((binary_watch_entries + 1) / 2);

  // Deduplication is local to one watch list.  A byte per literal plus a
  // second pass that clears the touched bits is substantially smaller than a
  // 32-bit global epoch table and preserves the same first-copy semantics.
  std::vector<uint8_t> seen(2 * k.nvars, 0);
  std::vector<Var> ordered_variables;
  kernel_vars_in_import_order(k, ordered_variables);
  for (const Var variable : ordered_variables)
    for (uint32_t sign = 0; sign < 2; sign++) {
      const Lit lit = make_lit(variable, sign != 0);
      for (const WatchEntry w : k.watches.entries(lit)) {
        if (!w.binary())
          continue;
        const Lit other = w.other();
        if (kernel_import_lit_order(k, lit) >=
                kernel_import_lit_order(k, other) ||
            seen[other])
          continue;
        seen[other] = 1;
        binaries.emplace_back(lit, other);
      }
      for (const WatchEntry w : k.watches.entries(lit))
        if (w.binary() && kernel_import_lit_order(k, lit) <
                              kernel_import_lit_order(k, w.other()))
          seen[w.other()] = 0;
    }
  // The outer scan already appends groups in first-literal import order.
  // Kissat's extract_binaries sorts only each equal-first run by the other
  // literal (insertion sort below 32, radix otherwise), not the complete
  // binary population.  Sorting each run independently preserves that exact
  // lexicographic result without an O(m log m) global sort of 83M pairs.
  for (size_t begin = 0; begin < binaries.size();) {
    size_t end = begin + 1;
    while (end < binaries.size() &&
           binaries[end].first == binaries[begin].first)
      end++;
    std::sort(binaries.begin() + begin, binaries.begin() + end,
              [&](const auto &a, const auto &b) {
                return kernel_import_lit_order(k, a.second) <
                       kernel_import_lit_order(k, b.second);
              });
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
    k.watches.heads[binary.first].size++;
    k.watches.heads[binary.second].size++;
  }
  if (binary_suffix)
    for (const auto &[a, b] : *binary_suffix) {
      k.watches.heads[a].capacity++;
      k.watches.heads[b].capacity++;
    }
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    if (!c->garbage()) {
      sort_clause_watches_like_kissat(k, c);
      c->searched = 2;
      k.watches.heads[c->lits[0]].size += 2;
      k.watches.heads[c->lits[1]].size += 2;
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
    const uint32_t words = clause_words(c->size);
    if (!c->garbage()) {
      const Lit first = c->lits[0], second = c->lits[1];
      push_word(first, blocking_watch(second).raw);
      push_word(first, ref);
      push_word(second, blocking_watch(first).raw);
      push_word(second, ref);
      k.watches.heads[first].has_large = true;
      k.watches.heads[second].has_large = true;
    }
    ref += words;
  }
  if (count_congruence_rebuild)
    k.stats.congruence_binary_rebuilds++;
}

void kernel_deduplicate_binary_watches(Kernel &k) {
  // Kissat's null-stack dense entry (`watch.c:kissat_flush_large_watches`)
  // retains the first watch for each partner in each literal list, including
  // root-satisfied binaries.  The latter is load-bearing for Sweep and Factor:
  // unlike `dense.c:flush_large_watches` with a non-null binary-save stack,
  // the null-stack path never inspects values.  Congruence performs its own
  // value-aware collection immediately after this operation. Proof clauses
  // need not be deleted for duplicate watches: one internal pair can represent
  // any still-live identical proof clause, and `binary_ids` names the latest.
  std::vector<uint32_t> seen(2 * k.nvars, 0);
  uint32_t epoch = 0;
  for (Lit lit = 0; lit < 2 * k.nvars; lit++) {
    if (++epoch == 0) {
      std::fill(seen.begin(), seen.end(), 0);
      epoch = 1;
    }
    k.watches.compact(lit, [&](WatchEntry watch) {
      if (!watch.binary())
        return true;
      const Lit other = watch.other();
      if (seen[other] == epoch)
        return false;
      seen[other] = epoch;
      return true;
    });
  }

}

void kernel_dense_connect(Kernel &k, OccIndex &occ) {
  assert(k.level == 0);
  occ.large.assign(2 * k.nvars, {});
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    if (!c->garbage() && !c->redundant())
      for (const Lit l : *c)
        occ.large[l].push_back(ref);
    ref += words;
  }
  k.dense = &occ;
}

void kernel_dense_disconnect(Kernel &k) { k.dense = nullptr; }

bool kernel_should_compact_variables(const Kernel &k, uint32_t percent) {
  if (k.var_status.empty() || k.nvars == 0)
    return false;
  uint64_t inactive = 0;
  for (Var v = 0; v < k.nvars; ++v) {
    const bool fixed = k.values[make_lit(v, false)] != 0 &&
                       k.assigned[v].level == 0;
    inactive += k.var_status[v] != VarStatus::Active || fixed;
  }
  return 100 * inactive > static_cast<uint64_t>(percent) * k.nvars;
}

bool kernel_compact_variables(Kernel &k) {
  if (k.var_status.empty() || k.conflict_valid || k.pending.valid || k.dense ||
      k.probing.recording || k.delayed_used)
    return false;

  const uint32_t old_nvars = k.nvars;
  std::vector<Var> old_to_new(old_nvars, INVALID_VAR);
  uint32_t new_nvars = 0;
  for (Var old = 0; old < old_nvars; old++)
    if (k.var_status[old] == VarStatus::Active)
      old_to_new[old] = new_nvars++;
  // Tactix keeps distinct public identities for root-fixed variables, while
  // Kissat maps them onto one representative.  They still count as inactive
  // for `compactlim`, and a due compacting collection must rebuild the whole
  // arena/watch pose even when there is no retired identity to renumber.
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
    ref += clause_words(c->size);
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
  // inherited clause-order key is the permanent representation of that native
  // position.  Replay that logical arena order here instead of either physical
  // append order or the solve-scoped inprocessing policy.
  struct CompactionArenaOrder final : ArenaOrdering {
    void order(const Kernel &kernel, std::vector<Ref> &refs) override {
      std::stable_sort(refs.begin(), refs.end(), [&](Ref a, Ref b) {
        const Clause *ca = clause_at(kernel.arena, a);
        const Clause *cb = clause_at(kernel.arena, b);
        const uint64_t ao =
            ca->id() ? kernel_clause_order(kernel, ca->id()) : a;
        const uint64_t bo =
            cb->id() ? kernel_clause_order(kernel, cb->id()) : b;
        return ao < bo;
      });
    }
  } compaction_order;
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
    ref += clause_words(c->size);
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
    ref += clause_words(c->size);
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
    ref += clause_words(c->size);
  }
  std::vector<int8_t> values(2 * new_nvars, 0);
  std::vector<Assigned> assigned(new_nvars);
  std::vector<Lit> jumped_over(new_nvars, INVALID_LIT);
  std::vector<uint64_t> unit_ids(new_nvars, 0);
  for (Var old = 0; old < old_nvars; old++) {
    const Var now = old_to_new[old];
    if (now == INVALID_VAR)
      continue;
    const Lit opos = make_lit(old, false), npos = make_lit(now, false);
    values[npos] = k.values[opos];
    values[negate(npos)] = k.values[negate(opos)];
    // Backtracking clears the value/trail but deliberately leaves Assigned
    // metadata behind.  It is inert while the variable is unassigned and must
    // not become representation state again: stale jumped=true commonly pairs
    // with jumped_over=INVALID_LIT.  Goldcrest used to remap that sentinel as
    // a variable index here.  Fresh vectors already hold the canonical empty
    // record, invalid jumped endpoint, and zero unit id for this case.
    if (values[npos] != 0) {
      assigned[now] = k.assigned[old];
      if (assigned[now].binary && assigned[now].reason < 2 * old_nvars)
        assigned[now].reason = map_lit(assigned[now].reason);
      if (assigned[now].jumped)
        jumped_over[now] = map_lit(k.jumped_over[old]);
      unit_ids[now] = k.unit_ids[old];
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
  compact_by_var(k.bve_min_bound, uint16_t{0});
  compact_by_var(k.touched, uint64_t{0});
  compact_by_var(k.removed, uint64_t{0});
  compact_by_var(k.touched_kept, uint64_t{0});

  std::vector<Var> internal_to_external(new_nvars);
  for (Var old = 0; old < old_nvars; old++) {
    const Var now = old_to_new[old];
    const Var external = k.internal_to_external[old];
    k.external_to_internal[external] =
        now == INVALID_VAR ? INVALID_VAR : now;
    if (now != INVALID_VAR)
      internal_to_external[now] = external;
  }

  const uint32_t old_bve_head = k.bve_retry_fixed_head;
  const uint32_t old_touch_head = k.touch_fixed_head;
  heuristics_compact_vars(k, old_to_new, new_nvars);

  k.nvars = new_nvars;
  k.internal_to_external.swap(internal_to_external);
  k.values.swap(values);
  k.assigned.swap(assigned);
  k.jumped_over.swap(jumped_over);
  k.unit_ids.swap(unit_ids);
  k.trail.swap(trail);
  k.frames.swap(frames);
  k.propagate_head = propagate_head;
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

  k.bve_retry_fixed_head = trail_prefix[old_bve_head];
  k.touch_fixed_head = trail_prefix[old_touch_head];
  k.seen.assign(new_nvars, 0);
  k.eager_marks.assign(2 * new_nvars, 0);
  k.marked_levels.assign(new_nvars + 1, 0);
  k.probing.stamp.assign(2 * new_nvars, 0);
  k.var_status.clear();
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
  for (Ref &ref : k.eager_recent)
    ref = INVALID_REF;
  k.model_values.clear();
  k.model_extended = false;
  if (k.proof)
    k.proof->bind_variable_map(&k.internal_to_external);
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
    const uint32_t words = clause_words(c->size);
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
    // later antecedent. `unit_ids[v] == 0` means "this literal has no unit
    // clause in the proof yet", which any direct citation site must have
    // ruled out before getting here (the reason-walk sites cannot hit it:
    // they only read unit_ids under `reason == UNIT_REASON`). Cheap
    // permanent guard, same family as Proof::add_rup's no-tautology and
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

// LRAT: every root-level propagation-derived literal becomes an explicit unit
// clause in the proof (CaDiCaL's scheme) so later chains can cite it. Emitting
// in trail order is what makes each chain nameable: a reason's other literals
// are all root-false and were assigned earlier, so their own ids already exist.
void kernel_emit_root_units(Kernel &k, size_t from) {
  for (size_t i = from; i < k.trail.size(); i++) {
    const Lit lit = k.trail[i];
    const Var v = var_of(lit);
    const Assigned &a = k.assigned[v];
    if (a.level != 0 || a.reason == UNIT_REASON ||
        a.reason == DECISION_REASON)
      continue; // asserted units got their ids at the source
    if (k.unit_ids[v])
      continue; // already named (a repeated drain of the same root literal)
    k.lrat_entries.clear();
    uint64_t reason_id;
    if (a.binary) {
      // Reason jumping needs the resolved-away literal above level 0, so it
      // cannot reach the root and the reason still names a real clause.
      assert(!a.jumped);
      reason_id = kernel_binary_id(k, lit, a.reason);
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
    if (a.reason == UNIT_REASON) {
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

uint64_t kernel_irredundant_clauses(const Kernel &k) {
  // Kissat's BINIRR_CLAUSES is a live-clause statistic.  `binary_ids` is a
  // proof-identity archive and deliberately retains deleted pairs, so using
  // its size here moves every scaled inprocessing deadline after a phase that
  // adds and later removes binaries.  Count one watch direction per live
  // binary instead.
  uint64_t n = 0;
  for (Lit lit = 0; lit < static_cast<Lit>(2 * k.nvars); lit++)
    for (const WatchEntry w : k.watches.entries(lit))
      if (w.binary() && lit < w.other())
        n++;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && !c->redundant())
      n++;
    ref += clause_words(c->size);
  }
  return n;
}

#ifndef NDEBUG
void kernel_check_invariants(const Kernel &k) {
  heuristics_check(k); // `H` sized to nvars (catches a missed Factor growth)
  assert(k.internal_to_external.size() == k.nvars);
  for (Var internal = 0; internal < k.nvars; internal++) {
    const Var external = k.internal_to_external[internal];
    assert(external < k.external_to_internal.size());
    assert(k.external_to_internal[external] == internal);
  }
  for (Var external = 0; external < k.external_to_internal.size(); external++) {
    const Var internal = k.external_to_internal[external];
    assert(internal == INVALID_VAR ||
           (internal < k.nvars &&
            k.internal_to_external[internal] == external));
  }
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
  // Native LRAT: a root-fixed literal must already carry its unit line's id.
  // 0 is the format's line TERMINATOR, so a later chain citing the literal is
  // truncated rather than wrong, and the defect surfaces only as a checker
  // rejection thousands of lemmas downstream (corpus 011). Checked at every
  // rule boundary so the abort names the rule that FIXED the literal instead
  // of the one that later cites it.
  if (k.proof && k.proof->lrat())
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
    if (!c->garbage()) {
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
    ref += clause_words(c->size);
  }
  {
    uint32_t eager_live = 0;
    for (const Ref r : k.eager_recent)
      if (r != INVALID_REF)
        eager_live++;
    assert(eager_seen == eager_live && "stale eager-subsumption register ref");
  }
  // A set per-list order bit permits the binary propagator's early break.
  for (Lit lit = 0; lit < 2 * k.nvars; lit++) {
    if (k.watches.heads[lit].binaries_first) {
      bool large = false;
      for (const WatchEntry w : k.watches.entries(lit)) {
        if (w.binary())
          assert(!large && "binary watch after a large one under binary-first");
        else
          large = true;
      }
    }
    for (const WatchEntry w : k.watches.entries(lit)) {
      if (w.binary())
        continue;
      const Clause *c = clause_at(k.arena, w.ref());
      bool blocker_is_member = false;
      for (const Lit member : *c)
        blocker_is_member |= member == w.blocking();
      assert(blocker_is_member && "large-watch blocker outside its clause");
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
  // means no clause is falsified
  if (k.propagate_head == k.trail.size() && !k.conflict_valid &&
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
      ref += clause_words(c->size);
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

void kernel_extend_model(Kernel &k) {
  if (k.model_extended)
    return;
  k.model_extended = true;
  // Replay must read a TOTAL external assignment: the segment test below
  // reads a value-0 variable as false in BOTH polarities, which voids the
  // tautological-resolvent exclusion behind both-sides witness pushes — a
  // pos- and a neg-side window of one pivot can then both look unsatisfied
  // and the reverse walk double-flips the pivot (docs/calculus.md, extend_ρ
  // total-assignment premise). Retired variables can reach this point
  // unassigned, and they occur in no live clause, so completing every
  // value-0 variable to a definite default keeps the active formula
  // satisfied and restores the premise. False matches tactix_value's
  // reading of 0.
  k.model_values.resize(2 * k.external_to_internal.size());
  for (Var ev = 0; ev < k.external_to_internal.size(); ev++) {
    const Lit pos = make_lit(ev, false);
    k.model_values[pos] = -1;
    k.model_values[negate(pos)] = 1;
  }
  for (Var iv = 0; iv < k.nvars; iv++) {
    const Var ev = k.internal_to_external[iv];
    const Lit ipos = make_lit(iv, false), epos = make_lit(ev, false);
    if (k.values[ipos] != 0) {
      k.model_values[epos] = k.values[ipos];
      k.model_values[negate(epos)] = k.values[negate(ipos)];
    }
  }
  // Replay rho in reverse (Jarvisalo-Heule-Biere): for each witness segment
  // whose clause is unsatisfied under the current assignment, force its
  // witness literal true. Because a variable-eliminating rule removed all of
  // the witness variable's clauses, its search-time value is meaningless;
  // this re-derives it. Untouched variables keep their trail value.
  size_t i = k.reconstruction.size();
  while (i > 0) {
    const uint32_t len = static_cast<uint32_t>(k.reconstruction[--i]);
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
  // Preserve the long-standing kernel test/debug view for variables that are
  // still live internally.  Compacted-away variables remain available only
  // through model_values, which is the public/output contract.
  for (Var iv = 0; iv < k.nvars; iv++) {
    const Var ev = k.internal_to_external[iv];
    const Lit ipos = make_lit(iv, false), epos = make_lit(ev, false);
    k.values[ipos] = k.model_values[epos];
    k.values[negate(ipos)] = k.model_values[negate(epos)];
  }
}

} // namespace tactix
