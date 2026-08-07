// Transliterated from kissat/src/reorder.c (MIT). A literal's weight sums
// 2^-(size-2) over the irredundant clauses it occurs in, size being the open
// literal count, so a binary is worth 1 and a ternary 1/2; a variable's weight
// is max(w+,w-) + 2*min(w+,w-), rewarding variables heavy on both polarities.
// Focused mode then moves every variable to the queue front in ascending weight
// order; stable mode rescales the scores and adds the weight, keeping the old
// EVSIDS order only as a tiebreak.
#include "policies/ops/reorder/reorder.hpp"
#include "policies/heuristics.hpp"

#include <algorithm>
#include <cassert>

namespace tactix {

// Deviations from kissat, ledgered in docs/implementation-notes.md §S7:
//  (1) `kissat_reordering` also requires `!solver->level`, a convenience of its
//      call site rather than a precondition; N1 requires keeping it out of the
//      guard, and a mid-trail reorder yields a different but still sound rank.
//  (2) The size table lookup is clamped below 2. Kissat needs no clamp because
//      at its call site an unsatisfied irredundant clause always has >= 2 open
//      literals; off a fixpoint it need not, and kissat would read an
//      uninitialized entry.
//  (3) Kissat skips inactive variables; the closest analogue here is "assigned
//      under the current assignment", which is what the value tests use.

namespace {

// kissat reordermaxsize: clauses above this contribute the smallest weight
// rather than an underflowed one (2^-98 is already indistinguishable from 0).
constexpr uint32_t REORDER_MAX_SIZE = 100;

// weight per clause size, table[s] = 2^-(s-2) for s in [2, REORDER_MAX_SIZE]
const double *size_weight_table() {
  static const std::vector<double> table = [] {
    std::vector<double> t(REORDER_MAX_SIZE + 1, 0.0);
    double w = 1.0;
    for (uint32_t s = 2; s <= REORDER_MAX_SIZE; s++, w /= 2.0)
      t[s] = w;
    return t;
  }();
  return table.data();
}

} // namespace

void reorder_weights(const Kernel &k, std::vector<double> &var_weight) {
  const double *table = size_weight_table();
  std::vector<double> lit_weight(2 * static_cast<size_t>(k.nvars), 0.0);
  // Large irredundant clauses: one arena pass, no occurrence index needed since
  // every clause is visited exactly once.
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    ref += clause_words(c->size);
    if (c->garbage() || c->redundant())
      continue;
    uint32_t open = 0;
    bool satisfied = false;
    for (const Lit l : *c) {
      const int8_t v = k.values[l];
      if (v > 0) {
        satisfied = true;
        break;
      }
      if (v == 0 && open < REORDER_MAX_SIZE)
        open++;
    }
    if (satisfied)
      continue;
    const double w = table[open < 2 ? 2 : open]; // deviation (2)
    for (const Lit l : *c)
      lit_weight[l] += w;
  }
  // Binaries never materialize in the arena (kernel.hpp binary_ids): they live
  // in the watch lists, one slot each, and carry no redundancy bit there, so
  // like kissat's binary pass this counts redundant binaries too.
  const double binary_weight = table[2];
  const uint32_t nlits = 2 * k.nvars;
  for (Lit l = 0; l < nlits; l++) {
    if (k.values[l] != 0) // deviation (3)
      continue;
    for (const WatchEntry w : k.watches.entries(l)) {
      if (!w.binary())
        continue;
      const Lit other = w.other();
      if (other < l || k.values[other] != 0)
        continue; // each pair once, both literals open
      lit_weight[l] += binary_weight;
      lit_weight[other] += binary_weight;
    }
  }
  var_weight.assign(k.nvars, 0.0);
  for (Var v = 0; v < k.nvars; v++) {
    const double pos = lit_weight[make_lit(v, false)];
    const double neg = lit_weight[make_lit(v, true)];
    var_weight[v] = std::max(pos, neg) + 2.0 * std::min(pos, neg);
  }
}

// Requires `H`: Reorder re-ranks the decision queue/heap, which is `H` state —
// a pure precondition of a policy operation that needs `H` (plan §5).
bool reorder_guard(const Kernel &k) {
  return !k.inconsistent && k.h != nullptr;
}

ApplyResult reorder_apply_target(Kernel &k, uint64_t target) {
  assert(reorder_guard(k));
  if (target != REORDER_QUEUE && target != REORDER_HEAP)
    return ApplyResult::NoOp; // unknown payload: never unsound
  if (k.nvars == 0)
    return ApplyResult::NoOp;
  std::vector<double> weight;
  reorder_weights(k, weight);
  std::vector<Var> sorted(k.nvars);
  for (Var v = 0; v < k.nvars; v++)
    sorted[v] = v;
  // Ascending weight throughout; the tiebreak is the ranking's own current
  // order, so a reorder never gratuitously permutes equal-weight variables.
  const Heuristics &h = *k.h;
  if (target == REORDER_QUEUE)
    std::sort(sorted.begin(), sorted.end(), [&](Var a, Var b) {
      if (weight[a] != weight[b])
        return weight[a] < weight[b];
      return h.links[a].stamp < h.links[b].stamp;
    });
  else
    std::sort(sorted.begin(), sorted.end(), [&](Var a, Var b) {
      if (weight[a] != weight[b])
        return weight[a] < weight[b];
      const double sa = h.scores.score[a], sb = h.scores.score[b];
      if (sa != sb)
        return sa < sb;
      return b < a; // total order, so the sort is well-defined (kissat's)
    });
  if (target == REORDER_QUEUE) {
    // The heaviest variable moves to the front last and so becomes the next
    // decision. No search-cache repair is needed: the invariant is that every
    // variable after `search_idx` is assigned, and the only variable that can
    // fail to move is the incoming tail X — if X is unassigned the incoming
    // invariant already forces `search_idx == X`, and the last unassigned
    // variable bumped after it refreshes the cache.
    for (const Var v : sorted)
      heuristics_queue_bump(k, v);
  } else {
    // Scores are brought back into [0,1] first so the structural weight
    // dominates the accumulated EVSIDS order rather than the reverse. Heaviest
    // first, as kissat pops its ascending stack: the final heap is the same
    // either way, but the sift order fixes the layout the determinism traces
    // compare.
    heuristics_rescale_scores(k);
    for (auto it = sorted.rbegin(); it != sorted.rend(); ++it)
      heuristics_score_set(k, *it, k.h->scores.score[*it] + weight[*it]);
  }
  k.stats.reordered++;
  return ApplyResult::Ok;
}

ApplyResult reorder_apply(Kernel &k) {
  return reorder_apply_target(k, k.h->stable ? REORDER_HEAP : REORDER_QUEUE);
}

static uint32_t reorder_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  if (cap == 0)
    return 0;
  // canonical first (the ranking the active mode reads), then the other
  const uint64_t active = k.h && k.h->stable ? REORDER_HEAP : REORDER_QUEUE;
  out[0] = active;
  if (cap < 2)
    return 1;
  out[1] = active == REORDER_HEAP ? REORDER_QUEUE : REORDER_HEAP;
  return 2;
}

static ApplyResult reorder_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return reorder_apply(k);
  return reorder_apply_target(k, inst);
}

// Policy operation (plan §4): H-only, no proof delta. Registered so it is
// N1-appliable and visible to the random-policy fuzzer even though
// DefaultPolicy leaves it unscheduled by default.
TACTIX_REGISTER_POLICY_OP(Reorder, RuleTier::Macro, reorder_guard, reorder_apply,
                          reorder_instances, reorder_apply_instance);

} // namespace tactix
