#include "kernel/kernel.hpp"

namespace tactix {

void kernel_reserve_vars(Kernel &k, uint32_t nvars) {
  if (nvars <= k.nvars)
    return;
  const uint32_t old = k.nvars;
  k.nvars = nvars;
  k.values.resize(2 * nvars, 0);
  k.assigned.resize(nvars);
  k.saved.resize(nvars, 1); // initial phase: positive (kissat default)
  k.watches.resize(2 * nvars);
  k.links.resize(nvars);
  k.seen.resize(nvars, 0);
  k.marked_levels.resize(nvars + 1, 0);
  k.unassigned += nvars - old;
  if (k.frames.empty())
    k.frames.push_back(0);
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
  k.search_idx = k.queue_last;
  k.search_stamp = k.links.empty() ? 0 : k.links[k.queue_last].stamp;
}

Ref kernel_add_large_clause(Kernel &k, const Lit *lits, uint32_t size,
                            bool redundant, uint32_t glue) {
  assert(size >= 3);
  const Ref ref = static_cast<Ref>(k.arena.size());
  k.arena.resize(k.arena.size() + clause_words(size));
  Clause *c = clause_at(k.arena, ref);
  c->size = size;
  c->flags = (redundant ? Clause::REDUNDANT : 0) |
             (glue << Clause::GLUE_SHIFT);
  c->searched = 2;
  for (uint32_t i = 0; i < size; i++)
    c->lits[i] = lits[i];
  push_large_watch(k.watches[lits[0]], lits[1], ref);
  push_large_watch(k.watches[lits[1]], lits[0], ref);
  return ref;
}

void kernel_add_binary_clause(Kernel &k, Lit a, Lit b) {
  push_binary_watch(k.watches[a], b);
  push_binary_watch(k.watches[b], a);
}

static inline void assign(Kernel &k, Lit lit, uint32_t level,
                          uint32_t reason, bool binary) {
  assert(k.values[lit] == 0);
  const Var v = var_of(lit);
  k.values[lit] = 1;
  k.values[negate(lit)] = -1;
  k.assigned[v] = {level, static_cast<uint32_t>(k.trail.size()), reason,
                   binary};
  k.trail.push_back(lit);
  assert(k.unassigned > 0);
  k.unassigned--;
}

void kernel_assign_decision(Kernel &k, Lit lit) {
  k.level++;
  k.frames.push_back(static_cast<uint32_t>(k.trail.size()));
  assign(k, lit, k.level, DECISION_REASON, false);
}

void kernel_assign_unit(Kernel &k, Lit lit) {
  assign(k, lit, 0, UNIT_REASON, false);
}

void kernel_assign_binary(Kernel &k, Lit lit, Lit other_false) {
  assign(k, lit, k.level, other_false, true);
}

void kernel_assign_ref(Kernel &k, Lit lit, Ref reason) {
  assign(k, lit, k.level, reason, false);
}

void kernel_backtrack(Kernel &k, uint32_t target) {
  assert(target < k.level);
  const uint32_t new_size = k.frames[target + 1];
  while (k.trail.size() > new_size) {
    const Lit lit = k.trail.back();
    k.trail.pop_back();
    const Var v = var_of(lit);
    k.saved[v] = negated(lit) ? -1 : 1; // phase saving
    k.values[lit] = 0;
    k.values[negate(lit)] = 0;
    k.unassigned++;
    // keep the VMTF search cache at the highest-stamp unassigned var
    // (kissat/src/inlinequeue.h idea)
    if (k.links[v].stamp > k.search_stamp) {
      k.search_idx = v;
      k.search_stamp = k.links[v].stamp;
    }
  }
  k.frames.resize(target + 1);
  k.level = target;
  k.propagate_head = new_size;
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
  k.search_idx = k.queue_last;
  k.search_stamp = k.queue_last == UINT32_MAX ? 0
                                              : k.links[k.queue_last].stamp;
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
    if (!a.binary && a.reason != DECISION_REASON && a.reason != UNIT_REASON)
      a.reason = clause_at(k.arena, a.reason)->searched;
  }
  for (WatchList &ws : k.watches) { // strip large entries, keep binaries
    uint32_t *q = ws.data();
    const uint32_t *p = q;
    const uint32_t *const end = ws.data() + ws.size();
    while (p != end) {
      if (Watch{*p}.binary())
        *q++ = *p++;
      else
        p += 2;
    }
    ws.resize(static_cast<size_t>(q - ws.data()));
  }
  k.arena.swap(fresh);
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    push_large_watch(k.watches[c->lits[0]], c->lits[1], ref);
    push_large_watch(k.watches[c->lits[1]], c->lits[0], ref);
    ref += clause_words(c->size);
  }
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

} // namespace tactix
