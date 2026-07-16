// Canonical pick transliterated from kissat/src/decide.c (MIT): VMTF
// last-enqueued-unassigned in focused mode, max-score-unassigned (EVSIDS
// heap, lazy pop) in stable mode; phase priority target (stable) -> saved.
#include "rules/decide/decide.hpp"

#include <algorithm>

namespace tactix {

// formal model (plan §4): Decide iff no conflict and unassigned vars exist,
// AND the trail is propagated to fixpoint (propagate_head == trail.size).
// The propagation precondition is load-bearing for SOUNDNESS in this kernel:
// propagation assigns a forced literal at the CURRENT decision level, not its
// true level, so a decision taken on an un-propagated trail lets the pending
// implications be assigned one level too deep — a binary conflict can then
// escape (fully-propagated state with a falsified clause; corpus 003b /
// invariant "falsified binary escaped propagation"). Requiring a propagated
// trail before every decision keeps every literal at its correct level — the
// DefaultPolicy already does this (it propagates to fixpoint before deciding),
// which is why only free N1 schedules (random policy) could reach the bug.
// The in-kernel workaround for the true-level-assignment limitation filed from
// corpus 002 (§5.4 kernel candidate). A pending Analyze->Backjump handoff does
// NOT block deciding — Backjump tolerates an already-assigned asserting literal
// — but the asserting literal must be propagated first (same rule).
bool decide_guard(const Kernel &k) {
  return !k.conflict_valid && !k.inconsistent && k.unassigned > 0 &&
         k.propagate_head == k.trail.size();
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

// generic instance surface (plan §5.1 v1): payload = DIMACS literal as a
// sign-extended int64; enumeration reuses the ranked candidate order
static uint32_t decide_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Lit> buf(cap);
  const uint32_t n = decide_candidates(k, buf.data(), cap);
  for (uint32_t i = 0; i < n; i++)
    out[i] = static_cast<uint64_t>(static_cast<int64_t>(externalize(buf[i])));
  return n;
}

static ApplyResult decide_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return decide_apply_canonical(k);
  const int64_t elit = static_cast<int64_t>(inst);
  if (elit == 0 || elit > INT32_MAX || elit < INT32_MIN)
    return ApplyResult::NoOp;
  const Lit l = internalize(static_cast<int>(elit));
  return decide_apply(k, l, true);
}

TACTIX_REGISTER_RULE(Decide, RuleTier::Micro, decide_guard,
                     decide_apply_canonical, decide_instances,
                     decide_apply_instance);

} // namespace tactix
