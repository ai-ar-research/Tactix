#include "kernel/kernel.hpp"

#include "proof/proof.hpp" // kernel_delete_binary emits the delta atomically

#include <algorithm>
#include <cstdio>

namespace tactix {

void kernel_reserve_vars(Kernel &k, uint32_t nvars) {
  if (nvars <= k.nvars)
    return;
  const uint32_t old = k.nvars;
  k.nvars = nvars;
  k.values.resize(2 * nvars, 0);
  k.assigned.resize(nvars);
  k.saved.resize(nvars, 1); // initial phase: positive (kissat default)
  k.watches.resize_lits(2 * nvars);
  k.links.resize(nvars);
  k.scores.pos.resize(nvars, UINT32_MAX);
  k.scores.score.resize(nvars, 0.0);
  k.target.resize(nvars, 0);
  k.best.resize(nvars, 0);
  k.unit_ids.resize(nvars, 0);
  k.seen.resize(nvars, 0);
  k.marked_levels.resize(nvars + 1, 0);
  k.unassigned += nvars - old;
  // fixed-capacity trail/frames: the inlined assign hot path must never
  // reallocate, and its push_back capacity branch becomes always-false
  k.trail.reserve(nvars);
  k.frames.reserve(nvars + 1);
  // enqueue new vars into the VMTF queue (most recent = preferred)
  for (Var v = old; v < nvars; v++) {
    Link &l = k.links[v];
    l.prev = k.queue_last;
    l.next = UINT32_MAX;
    l.stamp = ++k.queue_stamp;
    if (k.queue_last != UINT32_MAX)
      k.links[k.queue_last].next = v;
    else
      k.queue_first = v;
    k.queue_last = v;
  }
  kernel_reset_queue_search(k);
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
  return true;
}

// kernel_assign* live header-inline in kernel.hpp (fastassign.h pattern)

void kernel_backtrack(Kernel &k, uint32_t target, bool update_phases) {
  assert(target < k.level);
  if (update_phases)
    kernel_update_target_phases(k); // before unassigning (backtrack.c)
  // COMPACTING backtrack (rev 38, D-LEVEL-2): with true-level assignment the
  // trail is no longer level-sorted, so we cannot truncate by position. Scan
  // the suffix above the target frame and KEEP (compact down, re-propagate)
  // every literal whose true level <= target; unassign only those above it.
  // Transliterated from cadical backtrack.cpp / kissat backtrack.c. Under a
  // level-sorted trail (DefaultPolicy) the suffix is exactly the level>target
  // literals, so nothing is kept and this reduces to the old truncation —
  // s055 bit-exact.
  const uint32_t boundary = k.frames[target + 1];
  const bool stable = k.stable;
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
    if (update_phases)
      k.saved[v] = negated(lit) ? -1 : 1; // phase saving
    k.values[lit] = 0;
    k.values[negate(lit)] = 0;
    k.unassigned++;
    if (stable) {
      if (!kernel_heap_contains(k, v))
        kernel_heap_push(k, v);
    } else if (k.links[v].stamp > k.search_stamp) {
      k.search_idx = v;
      k.search_stamp = k.links[v].stamp;
    }
  }
  k.trail.resize(j);
  k.frames.resize(target + 1);
  k.level = target;
  // re-propagate the kept out-of-order literals from the boundary (cadical
  // sets propagated = the boundary). Never move the head forward.
  if (k.propagate_head > boundary)
    k.propagate_head = boundary;
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

static inline void flush_delayed(Kernel &k) {
  for (size_t i = 0; i < k.delayed.size(); i += 3) {
    const Lit lit = k.delayed[i];
    const Lit blocking = k.delayed[i + 1];
    const Ref ref = k.delayed[i + 2];
    k.watches.push_large(lit, blocking, ref);
  }
  k.delayed.clear();
}

// returns true iff a conflict was found (recorded in the kernel)
// `ignore` (CaDiCaL propagate.cpp's ignore pointer, reviewed kernel API
// addition for C3 Vivify / V1 Probe): a clause ref that must not
// propagate or conflict during this scan — a rule probing a candidate's
// own negated literals must not derive anything THROUGH the candidate
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
        q -= 2;
        continue;
      }
      Lit *lits = c->lits;
      const Lit other = lits[0] ^ lits[1] ^ not_lit;
      const int8_t other_value = values[other];
      if (other_value > 0) { // satisfied: remember as blocking literal
        Watch w = blocking_watch(other);
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
  k.watches.truncate(not_lit, static_cast<uint32_t>(q - begin));
  flush_delayed(k);
  k.stats.ticks += ticks;
  return conflict;
}

bool kernel_propagate(Kernel &k, Ref ignore) {
  while (k.propagate_head < k.trail.size()) {
    const Lit lit = k.trail[k.propagate_head];
    if (propagate_literal(k, lit, ignore)) {
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

// --- VMTF -------------------------------------------------------------------

static void queue_dequeue(Kernel &k, Var v) {
  Link &l = k.links[v];
  if (l.prev != UINT32_MAX)
    k.links[l.prev].next = l.next;
  else
    k.queue_first = l.next;
  if (l.next != UINT32_MAX)
    k.links[l.next].prev = l.prev;
  else
    k.queue_last = l.prev;
}

static void queue_restamp(Kernel &k) {
  // stamp overflow: renumber the whole queue in order (rare)
  uint32_t stamp = 0;
  for (Var v = k.queue_first; v != UINT32_MAX; v = k.links[v].next)
    k.links[v].stamp = ++stamp;
  k.queue_stamp = stamp;
  kernel_reset_queue_search(k);
}

void kernel_queue_bump(Kernel &k, Var v) {
  if (k.queue_last == v)
    return;
  queue_dequeue(k, v);
  Link &l = k.links[v];
  l.prev = k.queue_last;
  l.next = UINT32_MAX;
  if (k.queue_last != UINT32_MAX)
    k.links[k.queue_last].next = v;
  else
    k.queue_first = v;
  k.queue_last = v;
  if (k.queue_stamp == UINT32_MAX)
    queue_restamp(k);
  else
    l.stamp = ++k.queue_stamp;
  if (k.values[make_lit(v, false)] == 0) {
    k.search_idx = v;
    k.search_stamp = k.links[v].stamp;
  }
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
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    k.watches.push_large(c->lits[0], c->lits[1], ref);
    k.watches.push_large(c->lits[1], c->lits[0], ref);
    ref += clause_words(c->size);
  }
}

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

Var kernel_queue_next_unassigned(Kernel &k) {
  Var v = k.search_idx;
  assert(v != UINT32_MAX);
  while (k.values[make_lit(v, false)] != 0) {
    v = k.links[v].prev;
    assert(v != UINT32_MAX);
  }
  k.search_idx = v;
  k.search_stamp = k.links[v].stamp;
  return v;
}

void kernel_reset_queue_search(Kernel &k) {
  // kissat queue.c kissat_reset_search_of_queue: entering focused mode,
  // the cache must again point at the queue tail
  k.search_idx = k.queue_last;
  k.search_stamp =
      k.queue_last == UINT32_MAX ? 0 : k.links[k.queue_last].stamp;
}

// --- EVSIDS score heap ------------------------------------------------------
// Transliterated from kissat/src/inlineheap.h + bump.c (MIT): pos-indexed
// binary max-heap; EVSIDS = bump by a growing increment, rescale both
// scores and increment near overflow (Eén–Sörensson via MiniSat, kissat's
// stable-mode "scores" heap).

static constexpr double MAX_SCORE = 1e150;
static constexpr double SCORE_DECAY = 0.05; // kissat decay=50 per mille

static void heap_bubble_up(Kernel &k, Var v) {
  uint32_t *stack = k.scores.stack.data();
  uint32_t *pos = k.scores.pos.data();
  const double *score = k.scores.score.data();
  uint32_t vpos = pos[v];
  const double vscore = score[v];
  while (vpos > 0) {
    const uint32_t parent_pos = (vpos - 1) / 2;
    const Var parent = stack[parent_pos];
    if (score[parent] >= vscore)
      break;
    stack[vpos] = parent;
    pos[parent] = vpos;
    vpos = parent_pos;
  }
  stack[vpos] = v;
  pos[v] = vpos;
}

static void heap_bubble_down(Kernel &k, Var v) {
  uint32_t *stack = k.scores.stack.data();
  uint32_t *pos = k.scores.pos.data();
  const double *score = k.scores.score.data();
  const uint32_t end = static_cast<uint32_t>(k.scores.stack.size());
  uint32_t vpos = pos[v];
  const double vscore = score[v];
  for (;;) {
    uint32_t child_pos = 2 * vpos + 1;
    if (child_pos >= end)
      break;
    Var child = stack[child_pos];
    double child_score = score[child];
    const uint32_t sibling_pos = child_pos + 1;
    if (sibling_pos < end) {
      const Var sibling = stack[sibling_pos];
      if (score[sibling] > child_score) {
        child = sibling;
        child_pos = sibling_pos;
        child_score = score[sibling];
      }
    }
    if (child_score <= vscore)
      break;
    stack[vpos] = child;
    pos[child] = vpos;
    vpos = child_pos;
  }
  stack[vpos] = v;
  pos[v] = vpos;
}

void kernel_heap_push(Kernel &k, Var v) {
  assert(!kernel_heap_contains(k, v));
  k.scores.pos[v] = static_cast<uint32_t>(k.scores.stack.size());
  k.scores.stack.push_back(v);
  heap_bubble_up(k, v);
}

static void heap_pop_max(Kernel &k) {
  assert(!k.scores.stack.empty());
  const Var max = k.scores.stack[0];
  const Var last = k.scores.stack.back();
  k.scores.stack.pop_back();
  k.scores.pos[max] = UINT32_MAX;
  if (last != max) {
    k.scores.pos[last] = 0;
    k.scores.stack[0] = last;
    heap_bubble_down(k, last);
  }
}

static void rescale_scores(Kernel &k) {
  // divide everything by the max score so relative order is untouched
  double max_score = k.scinc;
  for (const double s : k.scores.score)
    max_score = std::max(max_score, s);
  const double factor = 1.0 / max_score;
  for (double &s : k.scores.score)
    s *= factor;
  k.scinc *= factor;
}

void kernel_score_bump(Kernel &k, Var v) {
  const double new_score = k.scores.score[v] + k.scinc;
  k.scores.score[v] = new_score;
  if (kernel_heap_contains(k, v))
    heap_bubble_up(k, v);
  if (new_score > MAX_SCORE)
    rescale_scores(k);
}

void kernel_scores_decay(Kernel &k) {
  k.scinc *= 1.0 / (1.0 - SCORE_DECAY);
  if (k.scinc > MAX_SCORE)
    rescale_scores(k);
}

void kernel_update_scores(Kernel &k) {
  // entering stable mode: every var must be reachable via the heap
  // (kissat bump.c kissat_update_scores)
  for (Var v = 0; v < k.nvars; v++)
    if (!kernel_heap_contains(k, v))
      kernel_heap_push(k, v);
}

Var kernel_heap_next_unassigned(Kernel &k) {
  assert(!k.scores.stack.empty());
  Var v = k.scores.stack[0];
  while (k.values[make_lit(v, false)] != 0) {
    heap_pop_max(k); // lazy deletion (kissat decide.c)
    assert(!k.scores.stack.empty());
    v = k.scores.stack[0];
  }
  return v;
}

void kernel_update_target_phases(Kernel &k) {
  // kissat backtrack.c kissat_update_target_and_best_phases, stable only
  if (!k.stable)
    return;
  const uint32_t assigned = static_cast<uint32_t>(k.trail.size());
  if (k.target_assigned < assigned) {
    k.target_assigned = assigned;
    for (const Lit lit : k.trail)
      k.target[var_of(lit)] = negated(lit) ? -1 : 1;
    k.stats.target_saved++;
  }
  if (k.best_assigned < assigned) {
    k.best_assigned = assigned;
    for (const Lit lit : k.trail)
      k.best[var_of(lit)] = negated(lit) ? -1 : 1;
    k.stats.best_saved++;
  }
}

#ifndef NDEBUG
void kernel_check_invariants(const Kernel &k) {
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

