#include "kernel/kernel.hpp"

#include "policies/heuristics.hpp" // the opaque `H`: Kernel's ctor/dtor + calls
#include "proof/proof.hpp" // kernel_delete_binary emits the delta atomically

#include <algorithm>
#include <cstdio>

namespace tactix {

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
  // Touched-epoch substrate: a brand-new variable has never been looked at by
  // any consumer, so it must read as touched against every existing watermark
  // — hence a real touch EVENT each, not a resize fill (kissat
  // activate_variable calls kissat_mark_added_literal for the same reason).
  k.touched.resize(nvars, 0);
  k.removed.resize(nvars, 0);
  // The band array needs no such event: a fresh variable is already touched
  // below, and C2 reads the two arrays as a union.
  k.touched_kept.resize(nvars, 0);
  for (uint32_t v = old; v < nvars; v++)
    kernel_touch_var(k, v);
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

Var kernel_new_extension_vars(Kernel &k, uint32_t count) {
  const Var first = k.nvars;
  if (k.first_extension_var == UINT32_MAX) {
    k.first_extension_var = first; // boundary = first non-original var
    k.original_external_vars =
        static_cast<uint32_t>(k.external_to_internal.size());
  }
  kernel_reserve_vars(k, k.nvars + count); // additive: grows all per-var state
  return first;
}

Ref kernel_add_large_clause(Kernel &k, const Lit *lits, uint32_t size,
                            bool redundant, uint32_t glue, uint64_t id) {
  assert(size >= 3);
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

void kernel_add_binary_clause(Kernel &k, Lit a, Lit b, uint64_t id) {
  k.watches.push_binary(a, b);
  k.watches.push_binary(b, a);
  // Either push lands behind whatever large entries a's / b's list already
  // holds, so the binary-first observation cannot be assumed to survive it.
  k.binaries_watched_first = false;
  // LAST id wins. The map must always name a clause the checker still holds,
  // and kernel_delete_binary can retire this pair's id, so a re-add must
  // install its fresh one — otherwise every later LRAT chain through the pair
  // cites a deleted clause. Among live duplicates either id is equally valid.
  k.binary_ids.insert_or_assign(kernel_binary_key(a, b), id);
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
  assert(k.delayed.empty());     // no scan may hold pointers into this slice
  assert(!k.probing.recording);  // an unlogged edit would survive the rollback
  bool removed = false;
  k.watches.compact(l, [&](WatchEntry w) {
    if (removed || w.binary() || w.ref() != ref)
      return true;
    removed = true; // drop this entry's blocking word + ref word
    return false;
  });
  assert(removed && "unwatch of a clause this literal does not watch");
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
                                  bool update_phases) {
  assert(target < k.level);
  // Snapshot timing, as in kissat's
  // `kissat_backtrack_without_updating_phases` (backtrack.c:72): SAVE phases,
  // gated by `update_phases`, but do NOT snapshot target/best — that belongs at
  // the call sites, since the conflict path steps down one level before
  // snapshotting (backtrack.c:156). Snapshotting here would let the high-water
  // mark include the conflict level's own literals, so later genuinely-longer
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
    if constexpr (HasH) {
      if (update_phases)
        k.h->saved[v] = negated(lit) ? -1 : 1; // phase saving
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
        if (!heuristics_heap_contains(k, v))
          heuristics_heap_push(k, v);
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
  const uint32_t reset = (k.trail_tainted && update_phases) ? 0 : boundary;
  if (k.propagate_head > reset)
    k.propagate_head = reset;
}

void kernel_backtrack(Kernel &k, uint32_t target, bool update_phases) {
  // Dispatch ONCE per backtrack on whether `H` is present. With no
  // `H` (an unguided solver) the <false> instantiation runs the same σ unwind but
  // its inner loop is branch-free w.r.t. `H` (every hook is `if constexpr (HasH)`,
  // compiled out). DefaultPolicy always has `H`, so it takes <true> — bit-exact.
  if (k.h)
    kernel_backtrack_impl<true>(k, target, update_phases);
  else
    kernel_backtrack_impl<false>(k, target, update_phases);
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

PendingUnits::Outcome PendingUnits::record(Kernel &k, Lit u, uint64_t id) {
  const Var v = var_of(u);
  if (by_var[v] == INVALID_LIT) {
    by_var[v] = u;
    k.unit_ids[v] = id;
    units.push_back(u);
    return Outcome::Recorded;
  }
  if (by_var[v] == u)
    return Outcome::Duplicate; // same-sign duplicate: a redundant unit
  kernel_emit_empty_from_pair(k, id, k.unit_ids[v]); // {u} + {¬u} ⊢ ∅
  return Outcome::Refuted;
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
  lg.lists.push_back({l, static_cast<uint32_t>(lg.words.size()), h.size});
  lg.words.insert(lg.words.end(), b, b + h.size);
  // S3: snapshot traffic is real memory work — charge it to the tick
  // currency (same cache-line unit as the scan at :602) so the effort
  // ledger can meter it against the probing techniques' budgets.
  kernel_charge_ticks(k, 1 + kernel_cache_lines(h.size));
}

template <bool RECORD> static inline void flush_delayed(Kernel &k) {
  for (size_t i = 0; i < k.delayed.size(); i += 3) {
    const Lit lit = k.delayed[i];
    const Lit blocking = k.delayed[i + 1];
    const Ref ref = k.delayed[i + 2];
    if constexpr (RECORD)
      probing_snapshot_list(k, lit); // before the push grows/moves the list
    k.watches.push_large(lit, blocking, ref);
  }
  k.delayed.clear();
}

// Returns true iff a conflict was found (recorded in the kernel). `ignore`
// (cadical's ignore pointer) is a clause ref that must not propagate or
// conflict during this scan: a rule probing a candidate's own negated literals
// must not derive anything through the candidate. `RECORD` logs every
// watch-list / clause-header update into k.probing so a probing scope can undo
// them; false for search, where codegen is as if the log did not exist.
template <bool RECORD>
static bool propagate_literal(Kernel &k, Lit lit, Ref ignore) {
  const Lit not_lit = negate(lit);
  assert(k.delayed.empty());
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
        k.conflict_valid = true;
        k.conflict_ref = INVALID_REF;
        k.conflict_bin[0] = not_lit;
        k.conflict_bin[1] = blocking;
        conflict = true;
        break;
      }
      kernel_assign_binary(k, blocking, not_lit);
      ticks++;
      k.stats.propagations++;
    } else {
      const Ref ref = *q++ = *p++;
      if (blocking_value > 0)
        continue;
      if (ref == ignore) // keep the watch, derive nothing through it
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
        k.delayed.push_back(replacement);
        k.delayed.push_back(other);
        k.delayed.push_back(ref);
        ticks++;
      } else if (other_value < 0) { // conflict
        k.conflict_valid = true;
        k.conflict_ref = ref;
        conflict = true;
        break;
      } else { // unit: assign `other` with this clause as reason
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
  // The tick charge stays the WHOLE list even when the scan stops early. It is
  // the effort meter the policy schedules against, so harvesting the saving
  // here would move the budgets and with them the search; that is a separate
  // change (kissat does take it, backbone.c:200-202, charging only `touched`).
  kernel_charge_ticks(k, 1 + kernel_cache_lines(end - p));
  // kissat's `stop_early` (backbone.c:186-193): under the binary-first layout
  // the first large entry ends the binary sub-list, so the whole tail is
  // entries this scan would skip one at a time. Sound because it drops nothing
  // a full scan would have read — `binaries_watched_first` says no binary
  // follows a large — and trajectory-neutral because the meter above is not
  // reduced by it.
  const bool stop_early = k.binaries_watched_first;
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

void kernel_probing_begin(Kernel &k) {
  ProbingLog &lg = k.probing;
  if (lg.recording)
    kernel_probing_end(k); // defensive: scopes never nest
  lg.recording = true;
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
  // Restore only from a fully unwound trail. If anything the speculation
  // derived is still assigned, the relocations it made are load-bearing: a
  // clause whose watch moved off a literal that STAYS false would otherwise
  // have no live watch able to fire when it later goes unit. Keeping the
  // updates in that case is exactly the pre-existing behaviour, so it is sound.
  if (k.trail.size() == lg.base_trail && k.propagate_head == lg.base_head &&
      k.level == lg.base_level) {
    // clause records in reverse: each undoes one 3-cycle
    // (lits[0], lits[1], lits[rpos]) <- (other, replacement, not_lit), and at
    // this point lits[1] still holds that record's `replacement`
    for (size_t i = lg.clauses.size(); i-- > 0;) {
      const ProbingLog::ClauseSnap &s = lg.clauses[i];
      Clause *c = clause_at(k.arena, s.ref);
      c->lits[s.rpos] = c->lits[1];
      c->lits[0] = s.lit0;
      c->lits[1] = s.lit1;
      c->searched = s.searched;
    }
    kernel_charge_ticks(k, lg.clauses.size()); // S3: restore traffic, same currency
    for (const ProbingLog::ListSnap &s : lg.lists) {
      k.watches.ensure_capacity(s.lit, s.size); // a defrag may have clamped it
      std::copy(lg.words.begin() + s.off, lg.words.begin() + s.off + s.size,
                k.watches.arena.begin() + k.watches.heads[s.lit].offset);
      k.watches.heads[s.lit].size = s.size;
      kernel_charge_ticks(k, 1 + kernel_cache_lines(s.size));
    }
  }
  lg.lists.clear();
  lg.words.clear();
  lg.clauses.clear();
}

bool kernel_propagate_probing(Kernel &k, Ref ignore) {
  if (!k.probing.recording)
    return kernel_propagate(k, ignore); // no open scope: nothing to record
  while (k.propagate_head < k.trail.size()) {
    const Lit lit = k.trail[k.propagate_head];
    if (propagate_literal<true>(k, lit, ignore))
      return true; // same "do not advance past the conflicting scan" rule
    k.propagate_head++;
  }
  return false;
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
// the large-watch entries. Watched pairs stay lits[0]/lits[1], so all
// watch invariants survive collection unchanged.
void kernel_collect_garbage(Kernel &k) {
  // No probing scope may be open: this relocates every Ref, so an outstanding
  // snapshot would roll pre-collection watch words (and ClauseSnap refs) back
  // over the compacted arena. Long an implicit assumption of the probing log;
  // stated here because `binaries_watched_first` below now rests on it too —
  // it is set true from the layout this function leaves behind, and a later
  // restore of a snapshot taken before it could otherwise reintroduce a binary
  // behind a large one while the flag still said otherwise.
  assert(!k.probing.recording);
  // The eager-subsumption register holds raw arena Refs and this function
  // relocates every one of them, so it is reset here rather than forwarded —
  // kissat does exactly the same at each of its three collect sites
  // (kissat_reset_last_learned, collect.c:274/514/702). Forwarding instead
  // would buy at most four extra candidates and would need `searched` read off
  // a possibly-garbage clause, the corpus-003 shape.
  for (Ref &r : k.eager_recent)
    r = INVALID_REF;
  Arena fresh;
  fresh.reserve(k.arena.size());
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    if (!c->garbage()) {
      const Ref nref = static_cast<Ref>(fresh.size());
      fresh.insert(fresh.end(), k.arena.begin() + ref,
                   k.arena.begin() + ref + words);
      clause_at(fresh, nref)->searched = 2;
      c->searched = nref; // forwarding pointer in the old copy
    }
    ref += words;
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
  if (k.pending.valid && k.pending.ref != INVALID_REF)
    k.pending.ref = clause_at(k.arena, k.pending.ref)->searched;
  for (Lit lit = 0; lit < 2 * k.nvars; lit++) // strip large entries, keep binaries
    k.watches.compact(lit, [](WatchEntry w) { return w.binary(); });
  k.arena.swap(fresh);
  // GC relocated every Ref: the dense occurrence index (increment 2) must be
  // rebuilt from the compacted arena in lock-step with the watches below (it
  // held offsets into the old arena, now stale). Clearing here also drops the
  // garbage Refs that accumulated lazily during the phase.
  if (k.dense)
    k.dense->large.assign(2 * k.nvars, {});
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    k.watches.push_large(c->lits[0], c->lits[1], ref);
    k.watches.push_large(c->lits[1], c->lits[0], ref);
    if (k.dense && !c->redundant())
      for (const Lit l : *c)
        k.dense->large[l].push_back(ref);
    ref += clause_words(c->size);
  }
  // Every surviving binary was kept in place by the strip above and every large
  // entry was appended after it, so the binary-first layout now holds — the
  // same way kissat gets it out of `kissat_watch_large_clauses`
  // (substitute.c:594-596), as a byproduct rather than as a sort.
  k.binaries_watched_first = true;
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

void kernel_lrat_chain(Kernel &k, uint64_t closing_id) {
  std::sort(k.lrat_entries.begin(), k.lrat_entries.end());
  k.lrat_chain.clear();
  for (const auto &e : k.lrat_entries) {
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
    k.lrat_chain.push_back(e.second);
  }
  k.lrat_chain.push_back(closing_id);
}

void kernel_reason_walk(Kernel &k, Var start, std::vector<uint8_t> &seen,
                        std::vector<std::pair<uint32_t, uint64_t>> &out,
                        std::vector<Var> *touched) {
  std::vector<Var> todo; // explicit stack: trails can be arbitrarily deep
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
  // binary_ids counts every distinct binary ever added (incl. derived
  // ones — they are irredundant-lifecycle here); the log-scale consumers
  // tolerate that over-approximation
  uint64_t n = k.binary_ids.size();
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
  // every live large clause is watched on exactly lits[0] and lits[1]
  // (and, folded into the same walk, the eager-subsumption register holds only
  // real clause STARTS — a stale Ref left there by a collect would index the
  // middle of some other clause and read a bogus header, which is the single
  // way that register can go wrong)
  uint32_t eager_seen = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    for (const Ref r : k.eager_recent)
      if (r == ref) {
        assert(c->redundant() && "eager register entry is not a learned clause");
        eager_seen++;
      }
    if (!c->garbage()) {
      for (int w = 0; w < 2; w++) {
        const Lit lit = c->lits[w];
        bool found = false;
        for (const WatchEntry w : k.watches.entries(lit))
          if (!w.binary() && w.ref() == ref)
            found = true;
        assert(found && "clause not watched on a watched literal");
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
  // binary-first layout: while `binaries_watched_first` holds, no list may put
  // a binary entry after a large one. This is the ONLY thing standing between
  // the binary propagator's early break and silently dropped implications, so
  // it is checked the way kissat checks its own
  // (backbone.c:304-315, check_large_clauses_watched_after_binary_clauses).
  if (k.binaries_watched_first) {
    for (Lit lit = 0; lit < 2 * k.nvars; lit++) {
      bool large = false;
      for (const WatchEntry w : k.watches.entries(lit)) {
        if (w.binary())
          assert(!large && "binary watch after a large one under binary-first");
        else
          large = true;
      }
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
  for (uint32_t i = 0; i < nrest; i++)
    k.reconstruction.push_back(rest[i]);
  k.reconstruction.push_back(witness);
  // segment length word (clause size = witness + rest); Lit is uint32_t so
  // it shares the flat stack — read back as the trailing length prefix.
  k.reconstruction.push_back(static_cast<Lit>(nrest + 1));
}

void kernel_extend_model(Kernel &k) {
  if (k.model_extended)
    return;
  k.model_extended = true;
  // Replay must read a TOTAL assignment: the segment test below reads a
  // value-0 variable as false in BOTH polarities, which voids the
  // tautological-resolvent exclusion behind both-sides witness pushes — a
  // pos- and a neg-side window of one pivot can then both look unsatisfied
  // and the reverse walk double-flips the pivot (docs/calculus.md, extend_ρ
  // total-assignment premise). Retired variables can reach this point
  // unassigned, and they occur in no live clause, so completing every
  // value-0 variable to a definite default keeps the active formula
  // satisfied and restores the premise. False matches tactix_value's
  // reading of 0.
  for (Var v = 0; v < k.nvars; v++) {
    const Lit pos = make_lit(v, false);
    if (k.values[pos] == 0) {
      k.values[pos] = -1;
      k.values[negate(pos)] = 1;
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
      if (k.values[k.reconstruction[j]] > 0) {
        satisfied = true;
        break;
      }
    if (!satisfied) {
      k.values[witness] = 1;
      k.values[negate(witness)] = -1;
    }
    i = begin;
  }
}

} // namespace tactix
