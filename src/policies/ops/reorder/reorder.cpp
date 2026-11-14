// Transliterated from kissat/src/reorder.c (MIT). A literal's weight sums
// 2^-(size-2) over its irredundant clauses (size = open literals), and a
// variable's is max(w+,w-) + 2*min(w+,w-). Focused mode moves variables to the
// queue front in ascending weight; stable mode rescales scores and adds the
// weight, EVSIDS order breaking ties.
#include "policies/ops/reorder/reorder.hpp"
#include "policies/heuristics.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace tactix {

// Deviations from kissat (docs/dev/implementation-notes.md §S7): (1)
// `kissat_reordering`'s `!solver->level` stays out of the guard for N1; a
// mid-trail reorder gives a different sound rank. (2) The size lookup is
// clamped below 2, since off a fixpoint a clause can have fewer open literals.
// (3) Retired variables stay addressable for N1 but are disconnected from H and
// not reinserted.

void reorder_weights(const Kernel &k, std::vector<double> &var_weight) {
  const strategy::ReorderParameters parameters =
      strategy::ReorderParameters::from_kernel(k);
  assert(parameters.valid());
  const uint32_t maximum = parameters.maximum_clause_size;
  const auto weight = [&](uint32_t size) {
    if (size > static_cast<uint32_t>(std::numeric_limits<int>::max()))
      return 0.0;
    if (parameters.clause_decay_milli ==
            strategy::ReorderParameters::CLAUSE_DECAY_DEFAULT &&
        parameters.binary_weight_milli ==
            strategy::ReorderParameters::BINARY_WEIGHT_DEFAULT)
      return std::ldexp(1.0, 2 - static_cast<int>(size));
    const double scale = strategy::ReorderParameters::WEIGHT_SCALE;
    const double binary = parameters.binary_weight_milli / scale;
    const double decay = parameters.clause_decay_milli / scale;
    return binary * std::pow(decay, static_cast<int>(size) - 2);
  };
  std::vector<double> lit_weight(2 * static_cast<size_t>(k.nvars), 0.0);
  // Large irredundant clauses: one arena pass.
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    ref += clause_actual_words(c);
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
      if (v == 0 && open < maximum && ++open == maximum)
        break;
    }
    if (satisfied)
      continue;
    const double w = weight(open < 2 ? 2 : open); // deviation (2)
    for (const Lit l : *c)
      lit_weight[l] += w;
  }
  // Binaries live only in watch lists without a redundancy bit (kernel.hpp
  // binary_ids), so like kissat's binary pass this counts redundant binaries
  // too.
  const double binary_weight = weight(2);
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
  const double polarity_multiplier =
      static_cast<double>(parameters.balanced_polarity_milli) /
      strategy::ReorderParameters::WEIGHT_SCALE;
  for (Var v = 0; v < k.nvars; v++) {
    const double pos = lit_weight[make_lit(v, false)];
    const double neg = lit_weight[make_lit(v, true)];
    var_weight[v] =
        std::max(pos, neg) + polarity_multiplier * std::min(pos, neg);
  }
}

// Requires `H` (plan §5).
RuleEligibility reorder_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("heuristics-attached", k.h != nullptr);
}

ApplyResult reorder_apply_target(Kernel &k, uint64_t target) {
  assert(reorder_guard(k));
  if (target != REORDER_QUEUE && target != REORDER_HEAP)
    return ApplyResult::NoOp; // unknown payload: never unsound
  if (k.nvars == 0)
    return ApplyResult::NoOp;
  std::vector<double> weight;
  reorder_weights(k, weight);
  std::vector<Var> sorted;
  sorted.reserve(k.nvars);
  for (Var compact = 0; compact < k.compact_import_order.size(); ++compact) {
    const Var v = k.compact_import_order[compact];
    if (v >= k.nvars)
      continue;
    if (heuristics_var_active(k, v))
      sorted.push_back(v);
  }
  // Kissat rescales before `sort_active_variables_by_weight`; multiplication
  // can collapse tiny scores, and then the index tiebreak fixes the update
  // order and physical heap.
  if (target == REORDER_HEAP)
    heuristics_rescale_scores(k);
  // Ascending weight; ties keep the ranking's own order.
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
      // Kissat breaks ties by compact internal index; permanent first-import
      // rank (skipping retired holes) preserves that order
      // (compact_import_order is activation order).
      return kernel_import_var_order(k, b) < kernel_import_var_order(k, a);
    });
  if (target == REORDER_QUEUE) {
    // The heaviest variable moves to the front last and becomes the next
    // decision. The search cache needs no repair: every variable after
    // `search_idx` is assigned, and only the incoming tail X can fail to move,
    // in which case `search_idx == X` already or a later bump refreshes it.
    for (const Var v : sorted)
      heuristics_queue_bump(k, v);
  } else {
    // Scores are in [0,1] before sorting, as in Kissat; apply heaviest first as
    // Kissat pops its ascending stack, fixing the physical layout later pops
    // consume.
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

// Policy operation (plan §4): H-only, registered for N1 and fuzzing.
TACTIX_REGISTER_POLICY_OP(Reorder, RuleTier::Macro, reorder_guard, reorder_apply,
                          reorder_instances, reorder_apply_instance);

} // namespace tactix
