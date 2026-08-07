// Search-guidance (`H`) implementation: VMTF decision queue, EVSIDS score heap,
// target/best phase snapshots, and their per-variable growth. See
// heuristics.hpp for the ownership rule.
#include "policies/heuristics.hpp"

#include <algorithm> // std::max (rescale_scores)
#include <cassert>

namespace tactix {

void heuristics_reserve_vars(Kernel &k, uint32_t old, uint32_t nvars) {
  k.h->saved.resize(nvars, 1); // initial phase: positive (kissat default)
  k.h->links.resize(nvars);
  k.h->scores.pos.resize(nvars, UINT32_MAX);
  k.h->scores.score.resize(nvars, 0.0);
  k.h->target.resize(nvars, 0);
  k.h->best.resize(nvars, 0);
  // enqueue new vars into the VMTF queue (most recent = preferred)
  for (Var v = old; v < nvars; v++) {
    Link &l = k.h->links[v];
    l.prev = k.h->queue_last;
    l.next = UINT32_MAX;
    l.stamp = ++k.h->queue_stamp;
    if (k.h->queue_last != UINT32_MAX)
      k.h->links[k.h->queue_last].next = v;
    else
      k.h->queue_first = v;
    k.h->queue_last = v;
    // Stable mode requires every var reachable via the heap, but
    // heuristics_update_scores only repairs that at stable entry, so a variable
    // created during stable must join here or the heap can drain while
    // unassigned vars remain.
    if (k.h->stable)
      heuristics_heap_push(k, v);
  }
  heuristics_reset_queue_search(k);
  k.h->nvars = nvars; // heuristics_check asserts this tracks Kernel::nvars
}
// --- VMTF -------------------------------------------------------------------

static void queue_dequeue(Kernel &k, Var v) {
  Link &l = k.h->links[v];
  if (l.prev != UINT32_MAX)
    k.h->links[l.prev].next = l.next;
  else
    k.h->queue_first = l.next;
  if (l.next != UINT32_MAX)
    k.h->links[l.next].prev = l.prev;
  else
    k.h->queue_last = l.prev;
}

static void queue_restamp(Kernel &k) {
  // stamp overflow: renumber the whole queue in order (rare)
  uint32_t stamp = 0;
  for (Var v = k.h->queue_first; v != UINT32_MAX; v = k.h->links[v].next)
    k.h->links[v].stamp = ++stamp;
  k.h->queue_stamp = stamp;
  heuristics_reset_queue_search(k);
}

// Transliterated from kissat/src/factor.c
// `adjust_scores_and_phases_of_fresh_variables` (MIT), cross-checked against
// cadical `factor.cpp:769`: the extension variables a variable-adding rule
// creates must be the least attractive decision candidates, not the most.
// heuristics_reserve_vars appends each new var at `queue_last`, where the VMTF
// decision search starts, so without this every variable Factor adds jumps to
// the head of the decision order. Focused mode only — stable mode's heap push
// uses score 0.0, already the bottom of a max-heap. O(#queue) per call, so call
// it once per rule application, never once per fresh variable.
void heuristics_demote_fresh_vars(Kernel &k, Var first) {
  if (!k.h || first >= k.h->nvars)
    return;
  for (Var v = first; v < k.h->nvars; v++)
    queue_dequeue(k, v);
  // Re-insert at the front (least preferred) in creation order, so the var
  // created last is least preferred of the block. Ascending is load-bearing:
  // both references prepend in creation order and neither preserves relative
  // order (kissat factor.c:867-882 over the stack pushed at :826; cadical
  // factor.cpp:799-802, `bury` = prepend, queue.hpp:61). Descending mirrors
  // them and leaves the demotion nearly inert once every original variable is
  // assigned, which is common.
  for (Var v = first; v < k.h->nvars; v++) {
    Link &l = k.h->links[v];
    l.prev = UINT32_MAX;
    l.next = k.h->queue_first;
    if (k.h->queue_first != UINT32_MAX)
      k.h->links[k.h->queue_first].prev = v;
    else
      k.h->queue_last = v;
    k.h->queue_first = v;
  }
  // Reachability, counted at the site (see Kernel::Stats::factor_demotions).
  k.stats.factor_demotions++;
  k.stats.factor_demoted_vars += k.h->nvars - first;
  queue_restamp(k); // renumber in queue order + reset the search cache
}


static void heap_remove(Kernel &k, Var v); // defined with the heap, below

void heuristics_deactivate_var(Kernel &k, Var v) {
  if (!k.h || v >= k.h->nvars)
    return;
  k.stats.deactivations++;
  queue_dequeue(k, v);            // out of the VMTF queue
  heap_remove(k, v);              // out of the EVSIDS heap, if present
  // The queue search hint may have pointed AT v; re-seat it on the queue tail
  // so the next decision starts from a live link (kissat resets the same way).
  if (k.h->search_idx == v)
    heuristics_reset_queue_search(k);
}

void heuristics_demote_marked_vars(Kernel &k, const std::vector<uint8_t> &mark) {
  if (!k.h || mark.empty())
    return;
  const Var n = std::min<Var>(k.h->nvars, static_cast<Var>(mark.size()));
  // Dequeue first, then re-insert at the front (least preferred) in reverse so
  // relative order is preserved — the heuristics_demote_fresh_vars idiom.
  bool any = false;
  for (Var v = 0; v < n; v++)
    if (mark[v]) {
      queue_dequeue(k, v);
      any = true;
    }
  if (!any)
    return;
  for (Var v = n; v-- > 0;)
    if (mark[v]) {
      Link &l = k.h->links[v];
      l.prev = UINT32_MAX;
      l.next = k.h->queue_first;
      if (k.h->queue_first != UINT32_MAX)
        k.h->links[k.h->queue_first].prev = v;
      else
        k.h->queue_last = v;
      k.h->queue_first = v;
    }
  queue_restamp(k); // renumber in queue order + reset the search cache
  // Stable mode ranks by score, so floor those too (a substituted variable
  // occurs in no clause; any score it still carries is stale).
  for (Var v = 0; v < n; v++)
    if (mark[v] && k.h->scores.score[v] != 0.0)
      heuristics_score_set(k, v, 0.0);
}

void heuristics_queue_bump(Kernel &k, Var v) {
  if (k.retire_enabled && kernel_var_retired(k, v))
    k.stats.reactivations++; // counted: this would undo the deactivation

  if (k.h->queue_last == v)
    return;
  queue_dequeue(k, v);
  Link &l = k.h->links[v];
  l.prev = k.h->queue_last;
  l.next = UINT32_MAX;
  if (k.h->queue_last != UINT32_MAX)
    k.h->links[k.h->queue_last].next = v;
  else
    k.h->queue_first = v;
  k.h->queue_last = v;
  if (k.h->queue_stamp == UINT32_MAX)
    queue_restamp(k);
  else
    l.stamp = ++k.h->queue_stamp;
  if (k.values[make_lit(v, false)] == 0) {
    k.h->search_idx = v;
    k.h->search_stamp = k.h->links[v].stamp;
  }
}
Var heuristics_queue_next_unassigned(Kernel &k) {
  Var v = k.h->search_idx;
  assert(v != UINT32_MAX);
  while (k.values[make_lit(v, false)] != 0) {
    v = k.h->links[v].prev;
    assert(v != UINT32_MAX);
  }
  k.h->search_idx = v;
  k.h->search_stamp = k.h->links[v].stamp;
  return v;
}

// See the header. Diagnostic: prefer any non-extension unassigned variable,
// scanning the ACTIVE order (heap in stable, queue in focused) and falling back
// to the ordinary choice when only extension vars remain.
Var heuristics_next_decision_var_no_ext(Kernel &k) {
  const Var ext = k.first_extension_var;
  if (!k.h->stable) {
    for (Var v = k.h->search_idx; v != UINT32_MAX; v = k.h->links[v].prev)
      if (k.values[make_lit(v, false)] == 0 && v < ext) {
        k.h->search_idx = v;
        k.h->search_stamp = k.h->links[v].stamp;
        return v;
      }
    return heuristics_queue_next_unassigned(k); // only extension vars left
  }
  // Stable: the heap is not fully ordered, so take the best NON-extension var
  // by score directly. O(nvars) per decision — fine for a diagnostic arm only.
  Var best = UINT32_MAX;
  double bs = -1.0;
  for (Var v = 0; v < ext && v < k.nvars; v++)
    if (k.values[make_lit(v, false)] == 0 && k.h->scores.score[v] > bs) {
      bs = k.h->scores.score[v];
      best = v;
    }
  return best != UINT32_MAX ? best : heuristics_heap_next_unassigned(k);
}

void heuristics_reset_queue_search(Kernel &k) {
  // kissat queue.c kissat_reset_search_of_queue: entering focused mode,
  // the cache must again point at the queue tail
  k.h->search_idx = k.h->queue_last;
  k.h->search_stamp =
      k.h->queue_last == UINT32_MAX ? 0 : k.h->links[k.h->queue_last].stamp;
}

// --- EVSIDS score heap ------------------------------------------------------
// Transliterated from kissat/src/inlineheap.h + bump.c (MIT): pos-indexed
// binary max-heap; EVSIDS = bump by a growing increment, rescale both
// scores and increment near overflow (Eén–Sörensson via MiniSat, kissat's
// stable-mode "scores" heap).

static constexpr double MAX_SCORE = 1e150;
static constexpr double SCORE_DECAY = 0.05; // kissat decay=50 per mille

static void heap_bubble_up(Kernel &k, Var v) {
  uint32_t *stack = k.h->scores.stack.data();
  uint32_t *pos = k.h->scores.pos.data();
  const double *score = k.h->scores.score.data();
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
  uint32_t *stack = k.h->scores.stack.data();
  uint32_t *pos = k.h->scores.pos.data();
  const double *score = k.h->scores.score.data();
  const uint32_t end = static_cast<uint32_t>(k.h->scores.stack.size());
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

void heuristics_heap_push(Kernel &k, Var v) {
  assert(!heuristics_heap_contains(k, v));
  k.h->scores.pos[v] = static_cast<uint32_t>(k.h->scores.stack.size());
  k.h->scores.stack.push_back(v);
  heap_bubble_up(k, v);
}

// Remove an arbitrary element (kissat inlineheap.h kissat_pop_heap): swap the
// last element into its slot, shrink, then restore the invariant in whichever
// direction the moved element needs. `heap_pop_max` is the pos==0 special case.
static void heap_remove(Kernel &k, Var v) {
  if (!heuristics_heap_contains(k, v))
    return;
  const uint32_t vpos = k.h->scores.pos[v];
  const Var last = k.h->scores.stack.back();
  k.h->scores.stack.pop_back();
  k.h->scores.pos[v] = UINT32_MAX;
  if (last == v)
    return; // v was the last element; nothing moved
  k.h->scores.stack[vpos] = last;
  k.h->scores.pos[last] = vpos;
  // `last` may belong either above or below vpos — try both, each is a no-op
  // when the invariant already holds.
  heap_bubble_up(k, last);
  heap_bubble_down(k, last);
}

static void heap_pop_max(Kernel &k) {
  assert(!k.h->scores.stack.empty());
  const Var max = k.h->scores.stack[0];
  const Var last = k.h->scores.stack.back();
  k.h->scores.stack.pop_back();
  k.h->scores.pos[max] = UINT32_MAX;
  if (last != max) {
    k.h->scores.pos[last] = 0;
    k.h->scores.stack[0] = last;
    heap_bubble_down(k, last);
  }
}

static void rescale_scores(Kernel &k) {
  // divide everything by the max score so relative order is untouched
  double max_score = k.h->scinc;
  for (const double s : k.h->scores.score)
    max_score = std::max(max_score, s);
  const double factor = 1.0 / max_score;
  for (double &s : k.h->scores.score)
    s *= factor;
  k.h->scinc *= factor;
}

void heuristics_score_bump(Kernel &k, Var v) {
  if (k.retire_enabled && kernel_var_retired(k, v))
    k.stats.reactivations++; // counted: this would undo the deactivation

  const double new_score = k.h->scores.score[v] + k.h->scinc;
  k.h->scores.score[v] = new_score;
  if (heuristics_heap_contains(k, v))
    heap_bubble_up(k, v);
  if (new_score > MAX_SCORE)
    rescale_scores(k);
}

void heuristics_score_set(Kernel &k, Var v, double score) {
  const double old = k.h->scores.score[v];
  if (score == old)
    return;
  k.h->scores.score[v] = score;
  if (heuristics_heap_contains(k, v)) {
    if (score > old)
      heap_bubble_up(k, v);
    else
      heap_bubble_down(k, v);
  }
  if (score > MAX_SCORE)
    rescale_scores(k);
}

void heuristics_rescale_scores(Kernel &k) { rescale_scores(k); }

void heuristics_scores_decay(Kernel &k) {
  k.h->scinc *= 1.0 / (1.0 - SCORE_DECAY);
  if (k.h->scinc > MAX_SCORE)
    rescale_scores(k);
}

void heuristics_update_scores(Kernel &k) {
  // entering stable mode: every var must be reachable via the heap
  // (kissat bump.c kissat_update_scores)
  for (Var v = 0; v < k.nvars; v++)
    if (!heuristics_heap_contains(k, v))
      heuristics_heap_push(k, v);
}

Var heuristics_heap_next_unassigned(Kernel &k) {
  assert(!k.h->scores.stack.empty());
  Var v = k.h->scores.stack[0];
  while (k.values[make_lit(v, false)] != 0) {
    heap_pop_max(k); // lazy deletion (kissat decide.c)
    assert(!k.h->scores.stack.empty());
    v = k.h->scores.stack[0];
  }
  return v;
}

void heuristics_update_target_phases(Kernel &k) {
  // kissat backtrack.c kissat_update_target_and_best_phases, stable only
  if (!k.h->stable)
    return;
  const uint32_t assigned = static_cast<uint32_t>(k.trail.size());
  if (k.h->target_assigned < assigned) {
    k.h->target_assigned = assigned;
    for (const Lit lit : k.trail)
      k.h->target[var_of(lit)] = negated(lit) ? -1 : 1;
    k.stats.target_saved++;
  }
  if (k.h->best_assigned < assigned) {
    k.h->best_assigned = assigned;
    for (const Lit lit : k.trail)
      k.h->best[var_of(lit)] = negated(lit) ? -1 : 1;
    k.stats.best_saved++;
  }
}

#ifndef NDEBUG
void heuristics_check(const Kernel &k) {
  // A null `H` is legal (an unguided solver): nothing to check.
  if (!k.h)
    return;
  // Otherwise every per-var vector tracks Kernel::nvars: a var-adding rule that
  // grew the kernel but not `H` trips here, before the next phase or score read
  // walks off the end.
  assert(k.h->nvars == k.nvars);
  assert(k.h->saved.size() == k.nvars);
  assert(k.h->links.size() == k.nvars);
  assert(k.h->scores.pos.size() == k.nvars);
  assert(k.h->scores.score.size() == k.nvars);
  assert(k.h->target.size() == k.nvars);
  assert(k.h->best.size() == k.nvars);
}
#endif

} // namespace tactix
