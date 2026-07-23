#include "kernel/kernel.hpp"

#include "policies/heuristics.hpp" // the opaque `H`: Kernel's ctor/dtor + calls
#include "proof/proof.hpp" // kernel_delete_binary emits the delta atomically

#include <algorithm>
#include <cstdio>

namespace tactix {

// The kernel does NOT own `H` (Track H · H7): it is a non-owning `Heuristics *`
// (kernel.hpp), attached by whoever owns the storage (the session / a policy).
// So Kernel needs no ctor/dtor for it. heuristics.hpp is still included above —
// kernel_backtrack_impl<true> reads `k.h->…`, which needs the complete type.

void kernel_reserve_vars(Kernel &k, uint32_t nvars) {
  if (nvars <= k.nvars)
    return;
  const uint32_t old = k.nvars;
  k.nvars = nvars;
  // --- σ-state + derived-index growth (kernel-owned) ---
  k.values.resize(2 * nvars, 0);
  k.assigned.resize(nvars);
  k.watches.resize_lits(2 * nvars);
  k.unit_ids.resize(nvars, 0);
  // BVE retry filter: a brand-new variable has never been attempted, so it
  // starts retry-worthy (kissat activate_variable marks it removed+added).
  // A DERIVED INDEX, not `H` (implementation-notes §6) — stays kernel-side.
  k.bve_retry.resize(nvars, 1);
  k.seen.resize(nvars, 0);
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
  // fixed-capacity trail/frames: the inlined assign hot path must never
  // reallocate, and its push_back capacity branch becomes always-false
  k.trail.reserve(nvars);
  k.frames.reserve(nvars + 1);
  // --- search-guidance (`H`) growth (Track H · H2: extracted seam) ---
  // Conditional on `H` being present (H5): an unguided solver (`k.h == nullptr`)
  // has no per-var `H` state to grow. DefaultPolicy always has `H` → unchanged.
  if (k.h)
    heuristics_reserve_vars(k, old, nvars);
}

Var kernel_new_extension_vars(Kernel &k, uint32_t count) {
  const Var first = k.nvars;
  if (k.first_extension_var == UINT32_MAX)
    k.first_extension_var = first; // boundary = first non-original var
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
  // IRREDUNDANT clauses only, for both consumers below:
  //  - dense phase (increment 2): a new irredundant large clause joins the
  //    shared occurrence index. Refs are arena offsets — stable across the
  //    resize above, so existing entries need no fix-up; only GC (which
  //    rewrites offsets) does.
  //  - BVE retry filter: this clause is new material for every variable in it.
  //    Redundant clauses are excluded on purpose — BVE does not resolve them,
  //    and learned-clause traffic would mark everything (see Kernel::bve_retry).
  if (!redundant)
    for (uint32_t i = 0; i < size; i++) {
      kernel_mark_bve_retry(k, var_of(lits[i]));
      if (k.dense)
        k.dense->large[lits[i]].push_back(ref);
    }
  return ref;
}

void kernel_add_binary_clause(Kernel &k, Lit a, Lit b, uint64_t id) {
  k.watches.push_binary(a, b);
  k.watches.push_binary(b, a);
  // LAST id wins (rev 33). The map must always name a clause the checker still
  // holds, and since kernel_delete_binary can retire this pair's id, a re-add
  // MUST install its fresh one — Analyze/Strengthen/Vivify can all re-derive a
  // binary V4 deleted, and keeping the retired id would make every later LRAT
  // chain through it cite a deleted clause. Among live DUPLICATES either id is
  // equally valid (both clauses are live), which is why first-vs-last was a
  // free choice before deletion existed and is not one now.
  k.binary_ids.insert_or_assign(kernel_binary_key(a, b), id);
  // BVE retry filter: binaries have an irredundant lifecycle in this kernel, so
  // every added pair is new elimination material for both of its variables.
  kernel_mark_bve_retry(k, var_of(a));
  kernel_mark_bve_retry(k, var_of(b));
}

bool kernel_delete_binary(Kernel &k, Lit a, Lit b) {
  // (1) liveness probe: the watch lists are the only liveness signal (a
  // deleted pair's id lingers in binary_ids until the pair is re-added), so an
  // absent binary is a no-op. COUNT the copies: binary_ids names a literal
  // PAIR, not a copy, so a duplicated pair cannot be deleted by name — the
  // `del` would retire the one id the surviving copy is still cited by, and
  // every later LRAT chain through it would name a deleted clause. Refuse.
  uint32_t copies = 0;
  for (const uint32_t *p = k.watches.begin(a), *e = k.watches.end(a); p != e;) {
    const Watch w{*p};
    if (!w.binary()) {
      p += 2; // large watch: blocking word + ref word
    } else {
      if (w.other() == b && ++copies == 2)
        break;
      p += 1;
    }
  }
  if (copies != 1)
    return false; // absent (idempotent) or duplicated (unnameable)
  // (2) retire a live ROOT reason BEFORE deleting the clause that names it, so
  // no trail reason ever cites a just-deleted binary (the corpus-003 analog for
  // binary reasons; GC cannot do this — its reason-fix loop skips binaries).
  for (const Lit x : {a, b}) {
    Assigned &ax = k.assigned[var_of(x)];
    const Lit other = (x == a) ? b : a;
    if (k.values[x] > 0 && ax.binary && ax.reason == other) {
      if (ax.level != 0)
        return false;          // live non-root reason: refuse (never at a root)
      ax.reason = UNIT_REASON;  // root literal is a unit, already has its id
    }
  }
  // (3) remove the mirror watch entries by in-place compaction (write cursor),
  // preserving large watches and the binary mirror-symmetry invariant.
  for (const Lit x : {a, b}) {
    const Lit other = (x == a) ? b : a;
    uint32_t *const begin = k.watches.begin(x);
    uint32_t *q = begin;
    const uint32_t *p = begin;
    const uint32_t *const end = k.watches.end(x);
    bool removed = false;
    while (p != end) {
      const Watch w{*p};
      if (!w.binary()) {
        *q++ = *p++; // large: blocking word...
        *q++ = *p++; // ...+ ref word
      } else if (!removed && w.other() == other) {
        removed = true; // drop exactly this slot
        p += 1;
      } else {
        *q++ = *p++;
      }
    }
    assert(removed);
    k.watches.truncate(x, static_cast<uint32_t>(q - begin));
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
  return true;
}

// DRAT-only variant for E2 Factor: remove EVERY copy of the pair (a,b), not just
// a single nameable one. A factoring replaces its whole grid with the aux-var
// definition, so a duplicated grid binary must be fully removed — but the plain
// kernel_delete_binary refuses copies!=1 because its retired id would break a
// later LRAT chain through a SURVIVING copy. Here no copy survives, so that
// hazard is gone; and Factor no-ops under native LRAT (D-FACTOR-LRAT), so the
// one-id-per-pair LRAT ambiguity never reaches a checker. Non-grid duplicates
// elsewhere are untouched (only Factor's exact grid pairs are removed), so the
// duplicate-binary search bias the base solver relies on is preserved. Emits one
// DRAT `del` per copy so the checker's clause set stays exact. Returns true iff
// at least one copy was removed.
bool kernel_delete_binary_all_copies(Kernel &k, Lit a, Lit b) {
  uint32_t copies = 0;
  for (const uint32_t *p = k.watches.begin(a), *e = k.watches.end(a); p != e;) {
    const Watch w{*p};
    if (!w.binary())
      p += 2;
    else {
      if (w.other() == b)
        copies++;
      p += 1;
    }
  }
  if (copies == 0)
    return false; // absent (idempotent)
  // retire a live ROOT reason before removing the clause(s) that name it (a no-op
  // for Factor, whose grid literals are both unassigned, but kept for generality)
  for (const Lit x : {a, b}) {
    Assigned &ax = k.assigned[var_of(x)];
    const Lit other = (x == a) ? b : a;
    if (k.values[x] > 0 && ax.binary && ax.reason == other) {
      if (ax.level != 0)
        return false; // live non-root reason: refuse (never at a root)
      ax.reason = UNIT_REASON;
    }
  }
  // remove ALL matching mirror watch entries by in-place compaction
  for (const Lit x : {a, b}) {
    const Lit other = (x == a) ? b : a;
    uint32_t *const begin = k.watches.begin(x);
    uint32_t *q = begin;
    const uint32_t *p = begin;
    const uint32_t *const end = k.watches.end(x);
    while (p != end) {
      const Watch w{*p};
      if (!w.binary()) {
        *q++ = *p++;
        *q++ = *p++;
      } else if (w.other() == other) {
        p += 1; // drop every matching copy
      } else {
        *q++ = *p++;
      }
    }
    k.watches.truncate(x, static_cast<uint32_t>(q - begin));
  }
  if (k.proof) {
    const Lit lits2[2] = {a, b};
    for (uint32_t i = 0; i < copies; i++)
      k.proof->del(k.next_clause_id, kernel_binary_id(k, a, b), lits2, 2);
  }
  k.stats.binaries_deleted += copies;
  kernel_mark_bve_retry(k, var_of(a)); // BVE retry filter (see above)
  kernel_mark_bve_retry(k, var_of(b));
  return true;
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
      for (const uint32_t *p = k.watches.begin(l), *e = k.watches.end(l);
           p != e;) {
        const Watch w{*p};
        if (w.binary()) {
          kernel_mark_bve_retry(k, var_of(w.other()));
          p += 1;
        } else {
          p += 2;
        }
      }
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

// kernel_assign* live header-inline in kernel.hpp (fastassign.h pattern)

// H2 (Track H): the H-hook seam. `HasH` selects at COMPILE TIME whether this
// backtrack maintains search guidance — the four `H` touches (target/best
// snapshot, phase save, heap re-insert, VMTF search-cache) are each under
// `if constexpr (HasH)`, so the null-`H` instantiation's inner loop is branch-
// free w.r.t. `H` (no per-literal `if (k.h)`). Only <true> is instantiated
// today (below), so codegen is byte-identical to the pre-H2 function; H4/H5 add
// the <false> (empty-`H`) instantiation and the once-per-backtrack dispatch.
template <bool HasH>
static void kernel_backtrack_impl(Kernel &k, uint32_t target,
                                  bool update_phases) {
  assert(target < k.level);
  if constexpr (HasH) {
    if (update_phases)
      heuristics_update_target_phases(k); // before unassigning (backtrack.c)
  }
  // COMPACTING backtrack (rev 38, D-LEVEL-2): with true-level assignment the
  // trail is no longer level-sorted, so we cannot truncate by position. Scan
  // the suffix above the target frame and KEEP (compact down, re-propagate)
  // every literal whose true level <= target; unassign only those above it.
  // Transliterated from cadical backtrack.cpp / kissat backtrack.c. Under a
  // level-sorted trail the suffix is exactly the level>target literals, so
  // nothing is kept and this reduces to the old truncation. That WAS
  // DefaultPolicy's situation and it made this change bit-exact at the time;
  // since chrono was enabled by default (2026-07-22) DefaultPolicy's trail is
  // no longer level-sorted, so the keep-and-compact path is live in the default
  // configuration.
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
    if constexpr (HasH) {
      // ranking-structure maintenance (loop-invariant k.h->stable; hoisted by the
      // optimizer, so identical codegen to the pre-H2 `const bool stable`)
      if (k.h->stable) {
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
  // re-propagate from the right point. Normally the boundary (cadical sets
  // propagated = the boundary): the kept out-of-order literals get rescanned,
  // and the level-≤target prefix below the boundary was propagated at a
  // fixpoint so its scans are still valid. But if an off-fixpoint Decide has
  // tainted the trail (D-LEVEL-1), a below-boundary scan may be STALE — it
  // treated a clause as satisfied by an ahead literal this backtrack just
  // unassigned — so the whole SEARCH trail must be rescanned against the
  // post-shrink assignment: reset to 0 (never forward). Only a real search
  // backtrack (update_phases=true — restart/backjump) does this. A probing
  // restore (update_phases=false — Lucky/Probe/Backbone/Vivify) unwinds only
  // its own assumptions off a clean root fixpoint and MUST land back on
  // head==root_fixed (the boundary), so it keeps the boundary reset regardless
  // of taint; the root it returns to was already re-propagated clean by the
  // search backtrack that reached level 0. DefaultPolicy never taints, so this
  // is the boundary reset for it (bit-exact).
  const uint32_t reset = (k.trail_tainted && update_phases) ? 0 : boundary;
  if (k.propagate_head > reset)
    k.propagate_head = reset;
}

void kernel_backtrack(Kernel &k, uint32_t target, bool update_phases) {
  // Track H · H5: dispatch ONCE per backtrack on whether `H` is present. With no
  // `H` (an unguided solver) the <false> instantiation runs the same σ unwind but
  // its inner loop is branch-free w.r.t. `H` (every hook is `if constexpr (HasH)`,
  // compiled out). DefaultPolicy always has `H`, so it takes <true> — bit-exact.
  if (k.h)
    kernel_backtrack_impl<true>(k, target, update_phases);
  else
    kernel_backtrack_impl<false>(k, target, update_phases);
}

// --- propagation engine ------------------------------------------------
// Transliterated from kissat/src/proplit.h (MIT, Biere et al.): blocking
// literals, binary clauses inlined in watch lists, in-place watch-list
// compaction, `searched` scan resumption, and DELAYED watch relocation —
// with the flat watch arena, pushing to another literal's list during a
// scan can move the arena, so relocations queue in kernel.delayed and
// flush after each literal's scan. Lived in rules/propagate until the S5
// probes needed propagation as a subroutine (rules never call rules) —
// reviewed §5.3 kernel-primitive addition, 2026-07-14.

// Probing bookkeeping log (see ProbingLog in kernel.hpp): snapshot `l`'s watch
// list the FIRST time this scope touches it — either the scan of `l` is about
// to make its first real write, or a delayed relocation is about to push onto
// it. Up to that point the list still holds exactly what it held at
// kernel_probing_begin: a scan copies `*q++ = *p++` with q == p until its first
// drop, so every earlier write was an identity write.
static void probing_snapshot_list(Kernel &k, Lit l) {
  ProbingLog &lg = k.probing;
  if (lg.stamp[l] == lg.epoch)
    return;
  lg.stamp[l] = lg.epoch;
  const WatchHead &h = k.watches.heads[l];
  const uint32_t *const b = k.watches.arena.data() + h.offset;
  lg.lists.push_back({l, static_cast<uint32_t>(lg.words.size()), h.size});
  lg.words.insert(lg.words.end(), b, b + h.size);
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

// returns true iff a conflict was found (recorded in the kernel)
// `ignore` (CaDiCaL propagate.cpp's ignore pointer, reviewed kernel API
// addition for C3 Vivify / V1 Probe): a clause ref that must not
// propagate or conflict during this scan — a rule probing a candidate's
// own negated literals must not derive anything THROUGH the candidate
// RECORD (kissat's separate proprobe.c engine, here one template instantiation
// away): log every watch-list / clause-header update into k.probing so a
// probing scope can undo them. false for search — the generated code, and
// therefore the search trajectory, is bit-identical to before the log existed.
template <bool RECORD>
static bool propagate_literal(Kernel &k, Lit lit, Ref ignore) {
  const Lit not_lit = negate(lit);
  assert(k.delayed.empty());
  // hoisted data pointers (kissat proplit.h idiom): the inlined assigns
  // store through k, so without locals the compiler must re-derive the
  // vector data pointers after every assignment; the underlying arrays
  // never move during a scan (no clause allocation, values sized at
  // reserve time)
  const int8_t *const values = k.values.data();
  uint32_t *const arena = k.arena.data();
  uint32_t *const begin = k.watches.begin(not_lit);
  uint32_t *q = begin;
  const uint32_t *p = q;
  const uint32_t *const end = k.watches.end(not_lit);
  bool conflict = false;
  // search ticks (kissat proplit.h): 1 + cache lines of the watch list
  // scanned, + 1 per large-clause dereference — a deterministic memory-
  // traffic proxy used by the policy's mode-switch budget (watch entries
  // are 4 bytes; 16 per 64-byte line)
  uint64_t ticks = 1 + (static_cast<uint64_t>(end - p) >> 4);

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
      } else if (other_value < 0) { // conflict
        k.conflict_valid = true;
        k.conflict_ref = ref;
        conflict = true;
        break;
      } else { // unit: assign `other` with this clause as reason
        kernel_assign_ref(k, other, ref);
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
  k.stats.ticks += ticks;
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
  const uint32_t *p = k.watches.begin(not_lit);
  const uint32_t *const end = k.watches.end(not_lit);
  k.stats.ticks += 1 + (static_cast<uint64_t>(end - p) >> 4);
  while (p != end) {
    const Watch w{*p};
    if (!w.binary()) { // large watch occupies two slots; skipped here
      p += 2;
      continue;
    }
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
      return true;
    }
    kernel_assign_binary(k, other, not_lit);
    k.stats.propagations++;
  }
  return false;
}

// --- probing propagation (kissat proprobe.c) --------------------------------
// See kernel.hpp for the contract and the measurement that motivated it. Note
// what kissat does and does NOT do here: kissat_probing_propagate is a second
// instantiation of the SAME proplit.h scan (it differs only in the `ignore`
// clause and in which statistics it charges), so kissat's probing does relocate
// watches and does rewrite `searched` — its insulation from search comes from
// elsewhere (probing runs inside its own mode, and the watch lists are rebuilt
// around it). We keep one engine and undo the bookkeeping instead, which is the
// smaller change and is exact: the propagation itself is bit-identical to
// kernel_propagate, so the derived literals, their trail order, and the
// recorded conflict are the same clause and the same literals.

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
    for (const ProbingLog::ListSnap &s : lg.lists) {
      k.watches.ensure_capacity(s.lit, s.size); // a defrag may have clamped it
      std::copy(lg.words.begin() + s.off, lg.words.begin() + s.off + s.size,
                k.watches.arena.begin() + k.watches.heads[s.lit].offset);
      k.watches.heads[s.lit].size = s.size;
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
  for (Lit lit = 0; lit < 2 * k.nvars; lit++) {
    // strip large entries in place, keep binaries
    uint32_t *const begin = k.watches.begin(lit);
    uint32_t *q = begin;
    const uint32_t *p = q;
    const uint32_t *const end = k.watches.end(lit);
    while (p != end) {
      if (Watch{*p}.binary())
        *q++ = *p++;
      else
        p += 2;
    }
    k.watches.truncate(lit, static_cast<uint32_t>(q - begin));
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
    k.watches.push_large(c->lits[0], c->lits[1], ref);
    k.watches.push_large(c->lits[1], c->lits[0], ref);
    if (k.dense && !c->redundant())
      for (const Lit l : *c)
        k.dense->large[l].push_back(ref);
    ref += clause_words(c->size);
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

void kernel_lrat_chain(Kernel &k, uint64_t closing_id) {
  std::sort(k.lrat_entries.begin(), k.lrat_entries.end());
  k.lrat_chain.clear();
  for (const auto &e : k.lrat_entries)
    k.lrat_chain.push_back(e.second);
  k.lrat_chain.push_back(closing_id);
}

void kernel_reason_walk(Kernel &k, Var start, std::vector<uint8_t> &seen,
                        std::vector<std::pair<uint32_t, uint64_t>> &out) {
  std::vector<Var> todo; // explicit stack: trails can be arbitrarily deep
  todo.push_back(start);
  while (!todo.empty()) {
    const Var v = todo.back();
    todo.pop_back();
    if (seen[v])
      continue;
    seen[v] = 1;
    const Assigned &a = k.assigned[v];
    if (a.reason == DECISION_REASON)
      continue; // probe assumption: the negation of the emitted clause
    if (a.reason == UNIT_REASON) {
      out.emplace_back(a.trail, k.unit_ids[v]);
      continue;
    }
    if (a.binary) {
      const Lit assigned_lit = make_lit(v, k.values[make_lit(v, true)] > 0);
      out.emplace_back(a.trail, kernel_binary_id(k, assigned_lit, a.reason));
      todo.push_back(var_of(a.reason));
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
    std::vector<std::pair<uint32_t, uint64_t>> &entries) {
  if (k.conflict_ref == INVALID_REF) { // binary conflict
    const uint64_t last =
        kernel_binary_id(k, k.conflict_bin[0], k.conflict_bin[1]);
    kernel_reason_walk(k, var_of(k.conflict_bin[0]), seen, entries);
    kernel_reason_walk(k, var_of(k.conflict_bin[1]), seen, entries);
    return last;
  }
  const Clause *conf = clause_at(k.arena, k.conflict_ref);
  const uint64_t last = conf->id();
  for (const Lit m : *conf)
    kernel_reason_walk(k, var_of(m), seen, entries);
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
    // since rev 38: true-level assignment + the compacting backtrack leave the
    // trail non-level-sorted. The value-based escaped-unit/conflict
    // completeness checks below are what actually catch propagation bugs.)
    assert(a.level == 0 || i >= k.frames[a.level]);
  }
  // every live large clause is watched on exactly lits[0] and lits[1]
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage()) {
      for (int w = 0; w < 2; w++) {
        const Lit lit = c->lits[w];
        bool found = false;
        const WatchHead &h = k.watches.heads[lit];
        const uint32_t *ws = k.watches.arena.data() + h.offset;
        for (uint32_t i = 0; i < h.size;) {
          if (Watch{ws[i]}.binary()) {
            i += 1;
          } else {
            if (ws[i + 1] == ref)
              found = true;
            i += 2;
          }
        }
        assert(found && "clause not watched on a watched literal");
      }
    }
    ref += clause_words(c->size);
  }
  // binary watch symmetry: every binary entry (l -> other) must have its
  // mirror (other -> l); losing one side silently kills the clause
  for (Lit lit = 0; lit < 2 * k.nvars; lit++) {
    const WatchHead &h = k.watches.heads[lit];
    const uint32_t *ws = k.watches.arena.data() + h.offset;
    for (uint32_t i = 0; i < h.size;) {
      if (Watch{ws[i]}.binary()) {
        const Lit o = Watch{ws[i]}.other();
        const WatchHead &ho = k.watches.heads[o];
        const uint32_t *wso = k.watches.arena.data() + ho.offset;
        bool mirror = false;
        for (uint32_t j = 0; j < ho.size;) {
          if (Watch{wso[j]}.binary()) {
            if (Watch{wso[j]}.other() == lit)
              mirror = true;
            j += 1;
          } else {
            j += 2;
          }
        }
        if (!mirror) {
          fprintf(stderr, "INVARIANT: binary watch (%d %d) lost its mirror\n",
                  externalize(lit), externalize(o));
          assert(false && "asymmetric binary watch");
        }
        i += 1;
      } else {
        i += 2;
      }
    }
  }

  // propagation completeness: fully propagated + no conflict in flight
  // means no clause is falsified
  if (k.propagate_head == k.trail.size() && !k.conflict_valid &&
      !k.pending.valid && !k.inconsistent) {
    for (Lit lit = 0; lit < 2 * k.nvars; lit++) {
      const WatchHead &h = k.watches.heads[lit];
      const uint32_t *ws = k.watches.arena.data() + h.offset;
      for (uint32_t i = 0; i < h.size;) {
        if (Watch{ws[i]}.binary()) {
          // at a propagation fixpoint, a binary watched by a FALSE literal
          // must have its other literal TRUE: other-false is a missed
          // conflict, other-unassigned is a missed unit (both mean
          // propagation escaped — e.g. a decision taken on an un-propagated
          // trail, corpus 004)
          const Lit o = Watch{ws[i]}.other();
          if (k.values[lit] < 0 && k.values[o] <= 0) {
            fprintf(stderr,
                    "INVARIANT: %s binary (%d %d); "
                    "levels %u %u trailpos %u %u head %u size %zu level %u\n",
                    k.values[o] < 0 ? "falsified" : "unit",
                    externalize(lit), externalize(o),
                    k.assigned[var_of(lit)].level, k.assigned[var_of(o)].level,
                    k.assigned[var_of(lit)].trail, k.assigned[var_of(o)].trail,
                    k.propagate_head, k.trail.size(), k.level);
            assert(false && "binary clause escaped propagation");
          }
          i += 1;
        } else {
          i += 2;
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

