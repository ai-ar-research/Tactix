// Search-guidance (`H`) implementation: VMTF decision queue, EVSIDS score heap,
// target/best phase snapshots, and their per-variable growth. See
// heuristics.hpp for the ownership rule.
#include "policies/heuristics.hpp"
#include "diagnostics/event_trace.hpp"

#include <algorithm> // std::max (rescale_scores)
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <type_traits>

namespace tactix {

namespace {

void rebuild_compact_import_position(Kernel &k, uint32_t nvars) {
  k.compact_import_position.assign(nvars, INVALID_VAR);
  for (Var position = 0; position < k.compact_import_order.size(); position++) {
    const Var v = k.compact_import_order[position];
    assert(v < nvars);
    assert(k.compact_import_position[v] == INVALID_VAR);
    k.compact_import_position[v] = position;
  }
}

} // namespace

void heuristics_consume_rule_advice(Kernel &k) {
  if (!k.h) {
    k.fresh_var_events.clear();
    k.substituted_events.clear();
    return;
  }
  if (k.factor_demote_fresh)
    for (const Kernel::FreshVars &event : k.fresh_var_events)
      heuristics_demote_fresh_vars(k, event.first, event.end);
  k.fresh_var_events.clear();

  if (k.subst_demote && !k.substituted_events.empty()) {
    std::vector<uint8_t> mark(k.nvars, 0);
    for (const Var v : k.substituted_events)
      if (v < k.nvars)
        mark[v] = 1;
    heuristics_demote_marked_vars(k, mark);
  }
  k.substituted_events.clear();
  k.substituted_events.shrink_to_fit();
}

void heuristics_consume_retirements(Kernel &k) {
  if (k.h)
    for (const Var v : k.retired_events)
      heuristics_deactivate_var(k, v);
  k.retired_events.clear();
  k.retired_events.shrink_to_fit();
}

void heuristics_discard_rule_events(Kernel &k) {
  k.fresh_var_events.clear();
  k.substituted_events.clear();
  k.retired_events.clear();
}

void heuristics_deactivate_inactive_vars(Kernel &k) {
  if (!k.h)
    return;
  // Kissat's queue and score heap hold exactly the active variables: an
  // identity that is root-fixed (an input unit assigned before this attach)
  // or already retired was deactivated by kissat_mark_fixed_literal /
  // flags.c:deactivate_variable at the moment it became inactive.  Attach
  // reproduces that state after enqueueing every identity, so each variable
  // still consumes its import-order stamp slot exactly as Kissat's
  // activate-then-deactivate does; skipping the enqueue instead would shift
  // every later stamp and change BVE's relevance ranking.
  for (Var v = 0; v < k.nvars; v++) {
    const bool fixed = k.values[make_lit(v, false)] != 0 &&
                       k.assigned[v].level == 0;
    if (fixed || kernel_var_retired(k, v))
      heuristics_deactivate_var(k, v);
  }
}

void heuristics_reserve_vars(Kernel &k, uint32_t old, uint32_t nvars) {
  assert(k.h);
  assert(!k.saved_phases || k.saved_phases == &k.h->saved);
  k.saved_phases = &k.h->saved;
  // Kissat initializes saved phases to zero.  The positive initial phase is a
  // separate fallback in `heuristics_decide_lit`; storing +1 here makes an
  // untouched variable indistinguishable from one actually assigned positive
  // and lets preprocessing assignments leak into the first search trajectory.
  k.h->saved.resize(nvars, 0);
  k.compact_import_position.resize(nvars, INVALID_VAR);
  // A fresh H can attach after a stopped solve or explicit detachment. The
  // kernel's compact index image survives that owner; rebuilding H must not
  // append its existing identities again (or restore compacted fixed slots).
  // Ordinary variable growth still appends every newly allocated identity.
  if (old || k.compact_import_order.empty())
    for (Var v = old; v < nvars; ++v) {
      const Var position = static_cast<Var>(k.compact_import_order.size());
      k.compact_import_order.push_back(v);
      assert(k.compact_import_position[v] == INVALID_VAR);
      k.compact_import_position[v] = position;
    }
  k.h->links.resize(nvars);
  k.h->scores.pos.resize(nvars, UINT32_MAX);
  k.h->scores.score.resize(nvars, 0.0);
  k.h->target.resize(nvars, 0);
  k.h->best.resize(nvars, 0);
  // enqueue new vars into the VMTF queue (most recent = preferred)
  for (Var v = old; v < nvars; v++) {
    const double activation_score =
        1.0 - 1.0 / static_cast<double>(++k.h->activation_count);
    k.h->scores.score[v] = activation_score;
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

void heuristics_compact_vars(Kernel &k, const std::vector<Var> &old_to_new,
                             uint32_t new_nvars) {
  if (!k.h)
    return;
  Heuristics &h = *k.h;
  const uint32_t old_nvars = h.nvars;
  const Var old_search = h.search_idx;
  assert(old_to_new.size() == old_nvars);

  auto compact_values = [&](auto &values, const auto &fallback) {
    using Vector = std::decay_t<decltype(values)>;
    Vector fresh(new_nvars, fallback);
    for (Var old = 0; old < old_nvars; old++) {
      const Var now = old_to_new[old];
      if (now != INVALID_VAR)
        fresh[now] = values[old];
    }
    values.swap(fresh);
  };
  compact_values(h.saved, int8_t{0});
  compact_values(h.target, int8_t{0});
  compact_values(h.best, int8_t{0});
  compact_values(h.scores.score, 0.0);

  std::vector<Var> compact_import_order;
  compact_import_order.reserve(k.compact_import_order.size());
  for (const Var old : k.compact_import_order) {
    const Var now = old_to_new[old];
    if (now != INVALID_VAR)
      compact_import_order.push_back(now);
  }
  k.compact_import_order.swap(compact_import_order);
  rebuild_compact_import_position(k, new_nvars);
  if (h.fixed_representative != INVALID_VAR)
    h.fixed_representative = old_to_new[h.fixed_representative];

  std::vector<Var> queue;
  queue.reserve(new_nvars);
  for (Var old = h.queue_first; old != INVALID_VAR; old = h.links[old].next) {
    const Var now = old_to_new[old];
    if (now != INVALID_VAR)
      queue.push_back(now);
  }
  // Native compact_queue restamps live members, but also copies the retained
  // fixed representative's old stamp. Transitive ordering can read that
  // inactive key through its compact-index image. Preserve detached records
  // through the physical map; the live queue below supplies fresh links.
  compact_values(h.links, Link{});
  h.queue_first = queue.empty() ? INVALID_VAR : queue.front();
  h.queue_last = queue.empty() ? INVALID_VAR : queue.back();
  h.queue_stamp = 0;
  for (size_t i = 0; i < queue.size(); i++) {
    Link &link = h.links[queue[i]];
    link.prev = i ? queue[i - 1] : INVALID_VAR;
    link.next = i + 1 < queue.size() ? queue[i + 1] : INVALID_VAR;
    link.stamp = ++h.queue_stamp;
  }
  // Kissat's compact_queue maps the focused VMTF search cache and preserves
  // its position; it does not reset the cache to the queue tail.  This matters
  // even for Tactix's identity-preserving root-fixed compaction, where only the
  // arena/watch pose changes.  A reset here changes the very next focused
  // decision after returning from stable mode.
  const Var mapped_search = old_search == INVALID_VAR
                                ? INVALID_VAR
                                : old_to_new[old_search];
  if (mapped_search != INVALID_VAR) {
    h.search_idx = mapped_search;
    h.search_stamp = h.links[mapped_search].stamp;
  } else {
    h.search_idx = h.queue_last;
    h.search_stamp = h.queue_last == INVALID_VAR
                         ? 0
                         : h.links[h.queue_last].stamp;
  }

  std::vector<Var> heap;
  heap.reserve(h.scores.stack.size());
  for (const Var old : h.scores.stack) {
    const Var now = old_to_new[old];
    if (now != INVALID_VAR)
      heap.push_back(now);
  }
  h.scores.stack.swap(heap);
  h.scores.pos.assign(new_nvars, INVALID_VAR);
  if (k.fixed_external_count)
    h.scores.pos.shrink_to_fit();
  for (uint32_t i = 0; i < h.scores.stack.size(); i++)
    h.scores.pos[h.scores.stack[i]] = i;

  h.nvars = new_nvars;
  // Kissat compact.c:compact_best_and_target_values recomputes both
  // high-water marks from the phase entries that survive variable mapping.
  // Resetting them to zero is not conservative: the next stable backtrack can
  // then accept a shallower trail as a new record and overwrite target phases
  // that compaction preserved correctly.  In Tactix every variable retained
  // by old_to_new remains in the compact assigned currency, so count the
  // remapped nonzero entries directly.
  h.target_assigned = 0;
  h.best_assigned = 0;
  for (Var v = 0; v < new_nvars; ++v) {
    h.target_assigned += h.target[v] != 0;
    h.best_assigned += h.best[v] != 0;
  }
}

// Kissat's `tumble` (default 1) gives two related orders distinct owners.
// Queue/score seeding follows first activation, while compact internal indices
// follow first import with never-activated variables filtered out.  Random
// decisions and `all_variables` consumers index the latter.  Tactix keeps
// stable public identities, so retain that compact image as a permutation.
void heuristics_seed_activation_order(Kernel &k, const std::vector<Var> &order) {
  if (!k.h || order.empty())
    return;
  std::vector<uint8_t> activated(k.nvars, 0);
  for (const Var v : order) {
    assert(v < k.nvars);
    assert(!activated[v]);
    activated[v] = 1;
  }
  std::vector<Var> import_order;
  kernel_vars_in_import_order(k, import_order);
  k.compact_import_order.clear();
  k.compact_import_order.reserve(order.size());
  for (const Var v : import_order)
    if (activated[v])
      k.compact_import_order.push_back(v);
  assert(k.compact_import_order.size() == order.size());
  rebuild_compact_import_position(k, k.nvars);
  // Last activated ends at the queue tail, the most-preferred end. A variable
  // in no stored clause is never bumped and falls to the front; kissat never
  // imports one at all, so its position is unconstrained.
  for (const Var v : order)
    heuristics_queue_bump(k, v);
  // First activation is enqueueing, not a search bump. Rebuilding its order
  // above must not retain stamps from the provisional allocation queue:
  // native flags.c assigns exactly one increasing stamp per activation.
  // In particular, roots fixed during parsing keep their activation stamp
  // after they leave the queue and through later compact-key lookups.
  for (Link &link : k.h->links)
    link.stamp = 0;
  uint32_t stamp = 0;
  for (const Var v : order)
    k.h->links[v].stamp = ++stamp;
  k.h->queue_stamp = stamp;
  k.h->search_stamp = k.h->search_idx == INVALID_VAR
                          ? 0
                          : k.h->links[k.h->search_idx].stamp;
  // The first activated variable scores exactly 0.0, as does every unactivated
  // one, so the heap ranks unactivated variables no higher than any other.
  // The heap is not built until the first stable entry, matching kissat's
  // `stable`-guarded parse-time push.
  // `heuristics_reserve_vars` gives every newly allocated identity a normal
  // activation score so extension variables can be inserted at the exact
  // score Kissat uses.  Initial parsing can allocate identities that Kissat
  // never activates, though, so reset the parse-time image and seed only the
  // retained first-activation order here.
  std::fill(k.h->scores.score.begin(), k.h->scores.score.end(), 0.0);
  uint64_t rank = 0;
  for (const Var v : order)
    heuristics_score_set(k, v, 1.0 - 1.0 / static_cast<double>(++rank));
  k.h->activation_count = rank;
  k.stats.tumble_seeded = order.size();
}

size_t heuristics_prepare_residual_activation_order(Kernel &k) {
  assert(!k.h);
  assert(k.level == 0);

  struct FirstOccurrence {
    uint64_t clause = std::numeric_limits<uint64_t>::max();
    uint32_t literal = UINT32_MAX;
  };
  std::vector<FirstOccurrence> first(k.nvars);

  // The original tumble order supplies the missing within-binary orientation:
  // Tactix mirrors a binary in two watch lists and intentionally does not keep
  // its file-literal direction.  If a surviving input binary is the first
  // residual occurrence of both variables, their original activation ranks
  // recover that direction.  Generated ties fall back to stable identity.
  std::vector<uint32_t> prior_rank(k.nvars, UINT32_MAX);
  for (uint32_t rank = 0; rank < k.input_activation_order.size(); ++rank)
    prior_rank[k.input_activation_order[rank]] = rank;

  const auto note = [&](Var v, uint64_t clause, uint32_t literal) {
    FirstOccurrence &slot = first[v];
    if (clause < slot.clause ||
        (clause == slot.clause && literal < slot.literal))
      slot = {clause, literal};
  };

  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *const c = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(c);
    if (!c->garbage()) {
      bool satisfied = false;
      uint32_t open = 0;
      for (const Lit lit : *c) {
        satisfied |= k.value(lit) > 0;
        open += k.value(lit) == 0;
      }
      if (!satisfied && open >= 2) {
        uint32_t position = 0;
        for (const Lit lit : *c) {
          if (!k.value(lit))
            note(var_of(lit), c->id(), position++);
        }
      }
    }
    ref += words;
  }

  for (Lit a = 0; a < static_cast<Lit>(2 * k.nvars); ++a) {
    if (k.value(a))
      continue;
    for (const WatchEntry watch : k.watches.entries(a)) {
      if (!watch.binary())
        continue;
      const Lit b = watch.other();
      if (a >= b || k.value(b))
        continue;
      const Var av = var_of(a), bv = var_of(b);
      const bool a_first = prior_rank[av] != prior_rank[bv]
                               ? prior_rank[av] < prior_rank[bv]
                               : av < bv;
      const uint64_t id = kernel_binary_id(k, a, b);
      note(av, id, a_first ? 0u : 1u);
      note(bv, id, a_first ? 1u : 0u);
    }
  }

  std::vector<Var> residual;
  residual.reserve(k.nvars);
  for (Var v = 0; v < k.nvars; ++v)
    if (first[v].clause != std::numeric_limits<uint64_t>::max())
      residual.push_back(v);
  std::sort(residual.begin(), residual.end(), [&](Var a, Var b) {
    if (first[a].clause != first[b].clause)
      return first[a].clause < first[b].clause;
    if (first[a].literal != first[b].literal)
      return first[a].literal < first[b].literal;
    return a < b;
  });

  // Satsuma writes every fixed literal as a unit after its reduced clauses.
  // A fresh Kissat therefore activates and immediately deactivates these
  // variables after the residual population.  Preserve those stamp slots in
  // stable external-variable order; variables absent from both the residual
  // and the unit suffix are never imported by that fresh solver.
  std::vector<Var> external_order;
  kernel_vars_in_external_order(k, external_order);
  for (const Var v : external_order)
    if (k.values[make_lit(v, false)] != 0 && k.assigned[v].level == 0)
      residual.push_back(v);

  k.input_activation_order.swap(residual);
  return k.input_activation_order.size();
}

void heuristics_seed_residual_import_order(Kernel &k,
                                           const std::vector<Var> &order) {
  assert(k.h);
  std::vector<uint8_t> seen(k.nvars, 0);
  for (const Var v : order) {
    assert(v < k.nvars);
    assert(!seen[v]);
    seen[v] = 1;
  }
  k.compact_import_order = order;
  rebuild_compact_import_position(k, k.nvars);
}

void heuristics_compact_random_order_fixed(Kernel &k) {
  if (!k.h || !kernel_root_fixed(k))
    return;
  bool kept_fixed = false;
  auto &order = k.compact_import_order;
  order.erase(std::remove_if(order.begin(), order.end(), [&](Var v) {
                const bool fixed = k.values[make_lit(v, false)] != 0 &&
                                   k.assigned[v].level == 0;
                if (!fixed)
                  return false;
                if (!kept_fixed) {
                  kept_fixed = true;
                  k.h->fixed_representative = v;
                  return false;
                }
                return true;
              }),
              order.end());
  rebuild_compact_import_position(k, k.nvars);

  // Kissat's compact_best_and_target_values counts nonzero phases only for
  // active variables in its physically compacted VARS image.  Tactix keeps
  // every root-fixed kernel identity for proof/model/API reconstruction, so
  // heuristics_compact_vars has necessarily seen a larger physical phase
  // vector at this point.  Once the random/import image above collapses those
  // roots to Kissat's single fixed representative, recompute both watermarks
  // over the active part of that image as well.  The representative occupies
  // a VARS slot but, like Kissat's first fixed variable, is inactive and must
  // not contribute.  Counting either the physical identities or that fixed
  // slot can suppress a later genuine target snapshot even when the two
  // search trails are otherwise identical.
  k.h->target_assigned = 0;
  k.h->best_assigned = 0;
  for (const Var v : order) {
    const bool fixed = k.values[make_lit(v, false)] != 0 &&
                       k.assigned[v].level == 0;
    if (kernel_var_retired(k, v) || fixed)
      continue;
    k.h->target_assigned += k.h->target[v] != 0;
    k.h->best_assigned += k.h->best[v] != 0;
  }
}

static uint32_t append_unranked_decision_candidates(
    const Kernel &k, Lit *out, uint32_t cap, uint32_t n) {
  if (n >= cap)
    return n;
  std::vector<uint8_t> emitted(k.nvars, 0);
  for (uint32_t i = 0; i < n; i++)
    emitted[var_of(out[i])] = 1;
  for (Var v = 0; v < k.nvars && n < cap; v++)
    if (!emitted[v] && k.values[make_lit(v, false)] == 0)
      out[n++] = heuristics_decide_lit(k, v);
  return n;
}

uint32_t heuristics_decide_candidates(const Kernel &k, Lit *out,
                                      uint32_t cap) {
  uint32_t n = 0;
  if (!k.h) {
    for (Var v = 0; v < k.nvars && n < cap; v++)
      if (k.values[make_lit(v, false)] == 0)
        out[n++] = make_lit(v, false);
    return n;
  }
  if (k.h->stable) {
    std::vector<Var> scratch;
    scratch.reserve(k.unassigned);
    for (Var v = 0; v < k.nvars; v++)
      if (k.values[make_lit(v, false)] == 0)
        scratch.push_back(v);
    std::stable_sort(scratch.begin(), scratch.end(),
                     [&](Var a, Var b) {
                       return k.h->scores.score[a] > k.h->scores.score[b];
                     });
    for (const Var v : scratch) {
      if (n >= cap)
        break;
      out[n++] = heuristics_decide_lit(k, v);
    }
    return n;
  }
  for (Var v = k.h->search_idx; v != UINT32_MAX && n < cap;
       v = k.h->links[v].prev)
    if (k.values[make_lit(v, false)] == 0)
      out[n++] = heuristics_decide_lit(k, v);
  return append_unranked_decision_candidates(k, out, cap, n);
}

uint32_t heuristics_restart_reuse_level_for(Kernel &k, Var next) {
  assert(k.h && next < k.nvars);
  assert(k.values[make_lit(next, false)] == 0);
  uint32_t level = 0;
  if (k.h->stable) {
    const double limit = k.h->scores.score[next];
    while (level < k.level) {
      const Lit decision = k.trail[k.frames[level + 1]];
      if (k.h->scores.score[var_of(decision)] <= limit)
        break;
      level++;
    }
  } else {
    const uint32_t limit = k.h->links[next].stamp;
    while (level < k.level) {
      const Lit decision = k.trail[k.frames[level + 1]];
      if (k.h->links[var_of(decision)].stamp <= limit)
        break;
      level++;
    }
  }
  return level;
}

uint32_t heuristics_restart_reuse_level(Kernel &k) {
  if (k.unassigned == 0 || !k.h)
    return 0;
  const Var next = k.h->stable ? heuristics_heap_next_unassigned(k)
                               : heuristics_queue_next_unassigned(k);
  return heuristics_restart_reuse_level_for(k, next);
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
  // Make disconnection explicit. Reorder used to feed a retired variable back
  // through this function and follow these stale links, corrupting the queue.
  l.prev = l.next = UINT32_MAX;
}

// heuristics_var_active and its queue_contains helper are defined inline in
// heuristics.hpp (report 3.50 cross-TU call overhead). Uses below call the
// renamed inline heuristics_queue_contains.

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
// the head of the decision order. Stable mode additionally needs Kissat's
// transient activation-score insertion followed by reverse zero demotion.
// O(#queue) per call, so call it once per rule application, never once per
// fresh variable.
void heuristics_demote_fresh_vars(Kernel &k, Var first, Var end) {
  if (!k.h || first > end || first > k.h->nvars)
    return;
  end = std::min(end, k.h->nvars);
  // Kissat factor.c::adjust_scores_and_phases_of_fresh_variables walks the
  // fresh stack backwards and updates every stable-heap score to zero before
  // changing the VMTF queue.  The downward sift is observable: inserting a
  // fresh variable at zero directly produces a different physical heap among
  // later equal-score candidates.
  for (Var v = end; v-- > first;)
    heuristics_score_set(k, v, 0.0);
  for (Var v = first; v < end; v++)
    queue_dequeue(k, v);
  // Re-insert at the front (least preferred) in creation order, so the var
  // created last is least preferred of the block. Ascending is load-bearing:
  // both references prepend in creation order and neither preserves relative
  // order (kissat factor.c:867-882 over the stack pushed at :826; cadical
  // factor.cpp:799-802, `bury` = prepend, queue.hpp:61). Descending mirrors
  // them and leaves the demotion nearly inert once every original variable is
  // assigned, which is common.
  for (Var v = first; v < end; v++) {
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
  if (first < end)
    k.stats.factor_demotions++;
  k.stats.factor_demoted_vars += end - first;
  // Native Factor performs this housekeeping even for an empty fresh batch.
  // Numeric stamps feed focused BVE's score, not just VMTF relative order.
  queue_restamp(k);
}

static void heap_remove(Kernel &k, Var v); // defined with the heap, below

void heuristics_deactivate_var(Kernel &k, Var v) {
  if (!k.h || v >= k.h->nvars)
    return;
  // Several formula rules can report the same retirement before the policy
  // drains its event queue.  Kissat's active flag makes deactivation
  // idempotent; mirror that property here.  A second queue_dequeue would
  // follow the disconnected link sentinels and erase queue_first/queue_last.
  if (!heuristics_queue_contains(k, v)) {
    k.stats.deactivation_repeats++;
    heap_remove(k, v);
    if (k.h->search_idx == v)
      heuristics_reset_queue_search(k);
    return;
  }
  k.stats.deactivations++;
  queue_dequeue(k, v);            // out of the VMTF queue
  heap_remove(k, v);              // out of the EVSIDS heap, if present
  // The queue search hint may have pointed AT v; re-seat it on the queue tail
  // so the next decision starts from a live link (kissat resets the same way).
  if (k.h->search_idx == v)
    heuristics_reset_queue_search(k);
}

void heuristics_consume_root_assignments(Kernel &k, size_t trail_begin) {
  if (!k.h)
    return;
  for (size_t i = trail_begin; i < k.trail.size(); ++i) {
    const Var v = var_of(k.trail[i]);
    if (k.assigned[v].level == 0 && heuristics_var_active(k, v))
      heuristics_deactivate_var(k, v);
  }
}

void heuristics_demote_marked_vars(Kernel &k, const std::vector<uint8_t> &mark) {
  if (!k.h || mark.empty())
    return;
  const Var n = std::min<Var>(k.h->nvars, static_cast<Var>(mark.size()));
  // Dequeue first, then re-insert at the front (least preferred) in reverse so
  // relative order is preserved — the heuristics_demote_fresh_vars idiom.
  std::vector<Var> active;
  active.reserve(n);
  for (Var v = 0; v < n; v++)
    if (mark[v] && heuristics_queue_contains(k, v)) {
      queue_dequeue(k, v);
      active.push_back(v);
    }
  if (active.empty())
    return;
  for (auto it = active.rbegin(); it != active.rend(); ++it) {
    const Var v = *it;
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
  for (const Var v : active)
    if (k.h->scores.score[v] != 0.0)
      heuristics_score_set(k, v, 0.0);
}

void heuristics_queue_bump(Kernel &k, Var v) {
  // Kissat bump.c filters analyzed variables through flags[idx].active before
  // touching either ranking.  Retired identities and newly root-fixed
  // learned units are both disconnected immediately; either can still occur
  // in the current analyzed stack, so queue membership is the exact active
  // predicate and avoids splicing through sentinel links.
  if (!heuristics_var_active(k, v))
    return;

#ifdef TACTIX_ENABLE_EVENT_TRACE
  static std::FILE *trace = [] {
    const char *path = std::getenv("TACTIX_QUEUE_BUMP_TRACE");
    return path && *path ? std::fopen(path, "w") : nullptr;
  }();
  static const uint64_t first = [] {
    const char *s = std::getenv("SAT_QUEUE_BUMP_FROM");
    return s ? std::strtoull(s, nullptr, 10) : 0;
  }();
  static const uint64_t last = [] {
    const char *s = std::getenv("SAT_QUEUE_BUMP_TO");
    return s ? std::strtoull(s, nullptr, 10) : UINT64_MAX;
  }();
  if (trace && k.stats.conflicts >= first && k.stats.conflicts <= last) {
    const auto ext = [&](Var x) { return x == INVALID_VAR ? 0 :
        std::abs(kernel_externalize(k, make_lit(x, false))); };
    const Link &link = k.h->links[v];
    std::fprintf(trace, "%llu\t%llu\t%d\t%d\t%u\t%d\t%d\t%d\t%u\t%u\t%d\n",
        (unsigned long long)k.stats.conflicts,
        (unsigned long long)k.stats.decisions, int(k.h->stable), ext(v),
        link.stamp, ext(link.prev), ext(link.next), ext(k.h->search_idx),
        k.h->search_stamp, k.h->queue_stamp, int(k.values[make_lit(v,false)]));
  }
#endif
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
// N1 totality backstop for both ranked pickers. `H`'s queue and heap hold only
// the variables the policy still branches on, while M2's guard counts EVERY
// unassigned variable (`unassigned`, not `unassigned_active`) — DefaultPolicy
// keeps the two in step by testing `unassigned_active`, but a policy that
// retires a variable and then applies Decide leaves the ranked structures
// unable to answer. Kissat cannot reach that state, so it walks its queue
// unguarded; here the walk would run off `INVALID_VAR` and read out of bounds.
// Index order is the same total fallback the unguided (`!k.h`) path uses.
static Var heuristics_any_unassigned(const Kernel &k) {
  for (Var v = 0; v < k.nvars; v++)
    if (k.values[make_lit(v, false)] == 0)
      return v;
  return INVALID_VAR; // caller's guard promised one exists
}

Var heuristics_queue_next_unassigned(Kernel &k) {
  Var v = k.h->search_idx;
  while (v != UINT32_MAX && k.values[make_lit(v, false)] != 0)
    v = k.h->links[v].prev;
  if (v == UINT32_MAX)
    return heuristics_any_unassigned(k);
  k.h->search_idx = v;
  k.h->search_stamp = k.h->links[v].stamp;
  return v;
}

Var heuristics_queue_next_active_unassigned(Kernel &k) {
  assert(k.unassigned_active > 0);
  Var v = k.h->search_idx;
  assert(v != INVALID_VAR);
  while (k.values[make_lit(v, false)] != 0) {
    v = k.h->links[v].prev;
    assert(v != INVALID_VAR);
  }
  k.h->search_idx = v;
  k.h->search_stamp = k.h->links[v].stamp;
  return v;
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
    event_trace_heap_pop(k, "move", v, child, child_pos, vpos);
    stack[vpos] = child;
    pos[child] = vpos;
    vpos = child_pos;
  }
  stack[vpos] = v;
  pos[v] = vpos;
}

void heuristics_heap_push(Kernel &k, Var v) {
  assert(!heuristics_heap_contains(k, v));
  const uint32_t from = static_cast<uint32_t>(k.h->scores.stack.size());
  k.h->scores.pos[v] = from;
  k.h->scores.stack.push_back(v);
  heap_bubble_up(k, v);
  event_trace_heap_insert(k, v, from, k.h->scores.pos[v]);
}

// Remove an arbitrary element (kissat inlineheap.h kissat_pop_heap): swap the
// last element into its slot, shrink, then restore the invariant in whichever
// direction the moved element needs. `heap_pop_max` is the pos==0 special case.
static void heap_remove(Kernel &k, Var v) {
  if (!heuristics_heap_contains(k, v))
    return;
  const uint32_t vpos = k.h->scores.pos[v];
  const uint32_t last_pos =
      static_cast<uint32_t>(k.h->scores.stack.size() - 1);
  const Var last = k.h->scores.stack.back();
  k.h->scores.stack.pop_back();
  k.h->scores.pos[v] = UINT32_MAX;
  if (last == v) {
    event_trace_heap_remove(k, v, last, vpos, last_pos, UINT32_MAX);
    return; // v was the last element; nothing moved
  }
  k.h->scores.stack[vpos] = last;
  k.h->scores.pos[last] = vpos;
  // `last` may belong either above or below vpos — try both, each is a no-op
  // when the invariant already holds.
  heap_bubble_up(k, last);
  heap_bubble_down(k, last);
  event_trace_heap_remove(k, v, last, vpos, last_pos,
                          k.h->scores.pos[last]);
}

static void heap_pop_max(Kernel &k) {
  assert(!k.h->scores.stack.empty());
  const Var max = k.h->scores.stack[0];
  const Var last = k.h->scores.stack.back();
  const uint32_t last_pos =
      static_cast<uint32_t>(k.h->scores.stack.size() - 1);
  event_trace_heap_pop(k, "begin", last, max, last_pos, 0);
  k.h->scores.stack.pop_back();
  k.h->scores.pos[max] = UINT32_MAX;
  if (last != max) {
    k.h->scores.pos[last] = 0;
    k.h->scores.stack[0] = last;
    heap_bubble_down(k, last);
    event_trace_heap_pop(k, "end", last, last, k.h->scores.pos[last],
                         k.h->scores.pos[last]);
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
  // Same active-variable filter as the focused queue path above and Kissat's
  // bump_analyzed_variable_scores. Retired variables and root-fixed learned
  // units remain outside both decision structures even while their scratch
  // analyzed mark is cleared at the end of this transaction.
  if (!heuristics_var_active(k, v))
    return;

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
  // Entering stable mode: every variable owned by this policy's live decision
  // queue must be reachable via the heap. DefaultPolicy disconnects retired
  // variables; the policy-neutral API may retain them for N1 decisions.
  // (kissat bump.c kissat_update_scores)
  // Kissat's tumbled INTERNAL indices follow first import, not first
  // activation.  Those orders differ when a variable first occurs in a
  // tautological/satisfied clause: import.c assigns its index immediately,
  // while internal.c activates it only when a retained clause closes.  The
  // `compact_import_order` is the filtered compact index image used by random
  // decisions. Equal scores preserve heap insertion pose, so permanent import
  // ranks emulate the same current internal-index order here.
  std::vector<Var> import_order;
  kernel_vars_in_import_order(k, import_order);
  for (const Var v : import_order)
    if (heuristics_queue_contains(k, v) && !heuristics_heap_contains(k, v) &&
        (k.assigned[v].level != 0 ||
         k.values[make_lit(v, false)] == 0))
      heuristics_heap_push(k, v);
}

Var heuristics_heap_next_unassigned(Kernel &k) {
  while (!k.h->scores.stack.empty()) {
    const Var v = k.h->scores.stack[0];
    if (k.values[make_lit(v, false)] == 0)
      return v;
    heap_pop_max(k); // lazy deletion (kissat decide.c)
    event_trace_heap_scores(k);
  }
  return heuristics_any_unassigned(k); // see the backstop above
}

Var heuristics_heap_next_active_unassigned(Kernel &k) {
  assert(k.unassigned_active > 0);
  assert(!k.h->scores.stack.empty());
  Var v = k.h->scores.stack[0];
  while (k.values[make_lit(v, false)] != 0) {
    heap_pop_max(k);
    event_trace_heap_scores(k);
    assert(!k.h->scores.stack.empty());
    v = k.h->scores.stack[0];
  }
  return v;
}

void heuristics_update_target_phases(Kernel &k) {
  // kissat backtrack.c kissat_update_target_and_best_phases, stable only
  if (!k.h->stable)
    return;
  // Kissat's high-water currency is `kissat_assigned = VARS - unassigned`
  // (internal.h:261-264).  Its `kissat_mark_eliminated_variable` removes an
  // unassigned retired identity from `unassigned` immediately, even though
  // that identity remains in VARS until the next compaction.  Tactix preserves
  // that identity in `compact_import_order` over the same interval and tracks
  // the matching live unassigned population in `unassigned_active`; global
  // `unassigned` deliberately still includes the reconstructible retired
  // identity.  Thus compact VARS minus active-unassigned is the portable
  // currency.  Physical trail height is representation-local too, since
  // compacted root identities remain on Tactix's trail.
  assert(k.compact_import_order.size() >= k.unassigned_active);
  const uint32_t assigned = static_cast<uint32_t>(
      k.compact_import_order.size() - k.unassigned_active);
  if (k.h->target_assigned < assigned) {
    event_trace_target_state(k, "target-save-pre", assigned);
    event_trace_target_save(k);
    k.h->target_assigned = assigned;
    for (const Lit lit : k.trail)
      k.h->target[var_of(lit)] = negated(lit) ? -1 : 1;
    k.stats.target_saved++;
    event_trace_target_state(k, "target-save-post", assigned);
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
  assert(k.saved_phases == &k.h->saved);
  // Otherwise every per-var vector tracks Kernel::nvars: a var-adding rule that
  // grew the kernel but not `H` trips here, before the next phase or score read
  // walks off the end.
  assert(k.h->nvars == k.nvars);
  assert(k.h->saved.size() == k.nvars);
  assert(!k.nvars || !k.compact_import_order.empty());
  assert(k.compact_import_position.size() == k.nvars);
  std::vector<uint8_t> random_ordered(k.nvars, 0);
  for (Var position = 0; position < k.compact_import_order.size(); position++) {
    const Var v = k.compact_import_order[position];
    assert(v < k.nvars);
    assert(!random_ordered[v]);
    random_ordered[v] = 1;
    assert(k.compact_import_position[v] == position);
  }
  for (Var v = 0; v < k.nvars; v++)
    assert(random_ordered[v] || k.compact_import_position[v] == INVALID_VAR);
  assert(k.h->links.size() == k.nvars);
  assert(k.h->scores.pos.size() == k.nvars);
  assert(k.h->scores.score.size() == k.nvars);
  assert(k.h->target.size() == k.nvars);
  assert(k.h->best.size() == k.nvars);

  // Every connected queue variable occurs exactly once with reciprocal links.
  // DefaultPolicy removes retired variables, while the policy-neutral C API
  // deliberately may leave them connected; structural integrity is universal
  // even though membership is policy-owned.
  std::vector<uint8_t> queued(k.nvars, 0);
  uint32_t queue_count = 0;
  Var previous = INVALID_VAR;
  for (Var v = k.h->queue_first; v != INVALID_VAR; v = k.h->links[v].next) {
    assert(v < k.nvars);
    assert(!queued[v]);
    assert(k.h->links[v].prev == previous);
    queued[v] = 1;
    previous = v;
    assert(++queue_count <= k.nvars);
  }
  assert(previous == k.h->queue_last);
  // Heap membership is lazy and policy-owned, but every member is unique and
  // agrees with the inverse position map.
  std::vector<uint8_t> heaped(k.nvars, 0);
  for (uint32_t pos = 0; pos < k.h->scores.stack.size(); pos++) {
    const Var v = k.h->scores.stack[pos];
    assert(v < k.nvars);
    assert(!heaped[v]);
    assert(k.h->scores.pos[v] == pos);
    heaped[v] = 1;
  }
}
#endif

} // namespace tactix
