// Canonical pick transliterated from kissat/src/decide.c (MIT): VMTF
// last-enqueued-unassigned in focused mode, max-score-unassigned (EVSIDS
// heap, lazy pop) in stable mode; phase priority target (stable) -> saved.
#include "rules/decide/decide.hpp"

#include <algorithm>

namespace tactix {

// formal model (plan §4): Decide iff no conflict and unassigned vars
// exist. A pending Analyze->Backjump handoff does NOT block deciding —
// Backjump tolerates an already-assigned asserting literal.
bool decide_guard(const Kernel &k) {
  return !k.conflict_valid && !k.inconsistent && k.unassigned > 0;
}

ApplyResult decide_apply(Kernel &k, Lit lit, bool external) {
  if (var_of(lit) >= k.nvars || k.values[lit] != 0)
    return ApplyResult::NoOp;
  k.stats.decisions++;
  if (external)
    k.stats.api_decisions++;
  kernel_assign_decision(k, lit);
  return ApplyResult::Ok;
}

ApplyResult decide_apply_canonical(Kernel &k) {
  assert(k.unassigned > 0);
  const Var v = kernel_next_decision_var(k);
  return decide_apply(k, kernel_decide_lit(k, v), false);
}

uint32_t decide_candidates(const Kernel &k, Lit *out, uint32_t cap) {
  uint32_t n = 0;
  if (k.stable) {
    // rank unassigned vars by score, descending (N1: candidates come
    // ranked by the ACTIVE heuristic). Introspection path, not hot: a
    // sort + allocation here is fine; the heap array is not fully ordered.
    std::vector<Var> scratch;
    scratch.reserve(k.unassigned);
    for (Var v = 0; v < k.nvars; v++)
      if (k.values[make_lit(v, false)] == 0)
        scratch.push_back(v);
    std::stable_sort(scratch.begin(), scratch.end(),
                     [&](Var a, Var b) {
                       return k.scores.score[a] > k.scores.score[b];
                     });
    for (const Var v : scratch) {
      if (n >= cap)
        break;
      out[n++] = kernel_decide_lit(k, v);
    }
    return n;
  }
  // walk the VMTF queue from most-recent to least, unassigned vars only
  for (Var v = k.search_idx; v != UINT32_MAX && n < cap;
       v = k.links[v].prev)
    if (k.values[make_lit(v, false)] == 0)
      out[n++] = kernel_decide_lit(k, v);
  return n;
}

TACTIX_REGISTER_RULE(Decide, RuleTier::Micro, decide_guard,
                     decide_apply_canonical);

} // namespace tactix
