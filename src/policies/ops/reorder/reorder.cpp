// Transliterated from kissat/src/reorder.c (MIT). The weight of a literal is
// the sum over the irredundant clauses it occurs in of 2^-(size-2), size being
// the clause's OPEN literal count (so a binary is worth 1, a ternary 1/2, ...);
// the weight of a variable is max(w+,w-) + 2·min(w+,w-), which rewards variables
// that are heavy on BOTH polarities. Focused mode then moves every variable to
// the queue front in ascending weight order (so the heaviest ends up the next
// decision); stable mode rescales the scores and ADDS the weight, keeping the
// old EVSIDS order only as a tiebreak.
//
// Deviations from kissat (recorded in docs/implementation-notes.md §S7):
//  (1) kissat's `kissat_reordering` also requires `!solver->level`; that is a
//      convenience of its call site, not a precondition — the weight is simply
//      read off the CURRENT assignment, and at a drained root the two coincide.
//      Keeping it out of the guard is what N1 requires (a policy may reorder
//      mid-trail; the result is a different, still sound, ranking).
//  (2) kissat has no clamp on `size < 2` because at its call site (level 0, at a
//      propagation fixpoint) an unsatisfied irredundant clause always has ≥ 2
//      open literals. Off a fixpoint it need not, so the table lookup is
//      clamped; kissat would read an uninitialized table entry there.
//  (3) kissat skips inactive (fixed/eliminated/substituted) variables; our
//      closest analogue is "assigned under the current assignment", which is
//      what the value tests below use.
#include "policies/ops/reorder/reorder.hpp"
#include "policies/heuristics.hpp"

#include <algorithm>
#include <cassert>

namespace tactix {

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
  // large irredundant clauses: one arena pass (this rule has no occurrence
  // index of its own — it visits every clause exactly once anyway)
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
  // binaries never materialize in the arena (kernel.hpp binary_ids) — they live
  // in the watch lists, one slot each, and carry no redundancy bit there, so
  // like kissat's binary pass this counts redundant binaries too.
  const double binary_weight = table[2];
  const uint32_t nlits = 2 * k.nvars;
  for (Lit l = 0; l < nlits; l++) {
    if (k.values[l] != 0) // deviation (3)
      continue;
    const uint32_t *ws = k.watches.arena.data() + k.watches.heads[l].offset;
    const uint32_t size = k.watches.heads[l].size;
    for (uint32_t i = 0; i < size;) {
      const Watch w{ws[i]};
      if (!w.binary()) {
        i += 2;
        continue;
      }
      i += 1;
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

// requires `H`: Reorder re-ranks the decision queue/heap, which is `H` state — a
// pure precondition of a policy operation that needs `H` (§5, H5).
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
    // the heaviest variable is moved to the front LAST and so becomes the next
    // decision.
    // No search-cache repair is needed, which is worth an argument because
    // `queue_bump` refreshes the cache only for the variables it MOVES and
    // early-returns on one that is already the tail (so does
    // `kissat_move_to_front`). The invariant is "every variable after
    // `search_idx` is assigned". The only variable that can fail to move is the
    // incoming tail X; if X is unassigned then the incoming invariant already
    // forces `search_idx == X`, and everything bumped after it here is bumped
    // in ascending weight order with the last unassigned one refreshing the
    // cache. Either way the suffix past the cache stays all-assigned.
    for (const Var v : sorted)
      heuristics_queue_bump(k, v);
  } else {
    // scores are brought back into [0,1] first so the structural weight
    // dominates the accumulated EVSIDS order rather than the other way round.
    // Heaviest first, as kissat pops its ascending stack — the final heap is
    // the same either way, but the sift order is what fixes the layout, and a
    // fixed layout is what the determinism traces compare.
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

// Policy operation (plan §4): H-only (re-ranks the decision queue/heap),
// no proof delta — registered as such, so it is N1-appliable and visible to the
// random-policy fuzzer while DefaultPolicy leaves it unscheduled by default.
TACTIX_REGISTER_POLICY_OP(Reorder, RuleTier::Macro, reorder_guard, reorder_apply,
                          reorder_instances, reorder_apply_instance);

} // namespace tactix
