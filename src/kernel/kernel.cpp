#include "kernel/kernel.hpp"

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
  // redundant clauses start fully "used" (kissat learn.c: used = MAX_USED)
  c->flags = redundant ? (Clause::REDUNDANT | Clause::USED_MASK) : 0;
  c->set_glue(glue);
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
  kernel_update_target_phases(k); // before unassigning (kissat backtrack.c)
  const uint32_t new_size = k.frames[target + 1];
  const bool stable = k.stable;
  while (k.trail.size() > new_size) {
    const Lit lit = k.trail.back();
    k.trail.pop_back();
    const Var v = var_of(lit);
    k.saved[v] = negated(lit) ? -1 : 1; // phase saving
    k.values[lit] = 0;
    k.values[negate(lit)] = 0;
    k.unassigned++;
    if (stable) {
      // unassigned vars must be findable by the heap again
      if (!kernel_heap_contains(k, v))
        kernel_heap_push(k, v);
    } else if (k.links[v].stamp > k.search_stamp) {
      // keep the VMTF search cache at the highest-stamp unassigned var
      // (kissat/src/inlinequeue.h idea)
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
  if (k.target_assigned >= assigned)
    return;
  k.target_assigned = assigned;
  for (const Lit lit : k.trail)
    k.target[var_of(lit)] = negated(lit) ? -1 : 1;
  k.stats.target_saved++;
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

