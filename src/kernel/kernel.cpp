#include "kernel/kernel.hpp"

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
  k.seen.resize(nvars, 0);
  k.marked_levels.resize(nvars + 1, 0);
  k.unassigned += nvars - old;
  // fixed-capacity trail/frames: the inlined assign hot path must never
  // reallocate, and its push_back capacity branch becomes always-false
  k.trail.reserve(nvars);
  k.frames.reserve(nvars + 1);
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
  k.watches.push_large(lits[0], lits[1], ref);
  k.watches.push_large(lits[1], lits[0], ref);
  return ref;
}

void kernel_add_binary_clause(Kernel &k, Lit a, Lit b) {
  k.watches.push_binary(a, b);
  k.watches.push_binary(b, a);
}

// kernel_assign* live header-inline in kernel.hpp (fastassign.h pattern)

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
  // never move the head FORWARD: under schedules that backtrack with
  // unpropagated older entries (legal — e.g. Decide before propagation
  // fixpoint, then Backjump), skipping them would lose propagations
  if (k.propagate_head > new_size)
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
    // trail position within the frame of its level
    assert(a.level == 0 || i >= k.frames[a.level]);
    assert(a.level == k.level || i < k.frames[a.level + 1]);
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
          if (k.values[lit] < 0 && k.values[Watch{ws[i]}.other()] < 0) {
            const Lit o = Watch{ws[i]}.other();
            fprintf(stderr,
                    "INVARIANT: falsified binary (%d %d); "
                    "levels %u %u trailpos %u %u head %u size %zu level %u\n",
                    externalize(lit), externalize(o),
                    k.assigned[var_of(lit)].level, k.assigned[var_of(o)].level,
                    k.assigned[var_of(lit)].trail, k.assigned[var_of(o)].trail,
                    k.propagate_head, k.trail.size(), k.level);
            assert(false && "falsified binary clause escaped propagation");
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
        bool nonfalse = false;
        for (const Lit l : *c)
          if (k.values[l] >= 0)
            nonfalse = true;
        assert(nonfalse && "falsified large clause escaped propagation");
      }
      ref += clause_words(c->size);
    }
  }
}
#else
void kernel_check_invariants(const Kernel &) {}
#endif

} // namespace tactix

