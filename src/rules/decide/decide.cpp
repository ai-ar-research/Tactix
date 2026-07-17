// Canonical pick transliterated from kissat/src/decide.c (MIT): VMTF
// last-enqueued-unassigned in focused mode, max-score-unassigned (EVSIDS
// heap, lazy pop) in stable mode; phase priority target (stable) -> saved.
#include "rules/decide/decide.hpp"

#include <algorithm>

namespace tactix {

// formal model (plan §4): Decide iff no conflict and unassigned vars exist and
// F is not refuted — a PURE soundness precondition, no propagation-fixpoint
// requirement (D-LEVEL-1, calculus §3, rev 40). A policy may take multiple
// decisions in a row without unit propagation between them (the N1 capability a
// learned/LLM policy reasons over). Off-fixpoint decide used to break the
// watched-literal invariant — a not-yet-run scan rests a watch on an "ahead"
// assignment, which a later backtrack unassigns, stranding a clause unit with
// no watch firing (an escaped unit). The cure is not a guard: it is the
// `trail_tainted` kernel repair (decide_apply taints; kernel_backtrack then
// re-propagates the whole trail on the next search backtrack — see kernel.hpp).
// DefaultPolicy always decides at a fixpoint so it never taints (bit-exact); a
// pending Analyze->Backjump handoff does NOT block deciding (Backjump tolerates
// an already-assigned asserting literal).
bool decide_guard(const Kernel &k) {
  return !k.conflict_valid && !k.inconsistent && k.unassigned > 0;
}

ApplyResult decide_apply(Kernel &k, Lit lit, bool external) {
  if (var_of(lit) >= k.nvars || k.values[lit] != 0)
    return ApplyResult::NoOp;
  // D-LEVEL-1: a decision taken off a propagation fixpoint taints the trail —
  // a not-yet-run watch scan can see this-or-a-later decision as an "ahead"
  // assignment and rest a watch on it, so any subsequent backtrack must
  // re-propagate the whole trail (kernel_backtrack reads this flag). See the
  // trail_tainted comment in kernel.hpp. DefaultPolicy always decides at a
  // fixpoint, so it never sets this and stays bit-exact.
  if (k.propagate_head < k.trail.size())
    k.trail_tainted = true;
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

// generic instance surface (plan §5.1 v1): payload = the DIMACS literal, the
// shared lit_instance encoding (registry.hpp — zero-extended, so no literal can
// alias INSTANCE_CANONICAL); enumeration reuses the ranked candidate order
static uint32_t decide_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Lit> buf(cap);
  const uint32_t n = decide_candidates(k, buf.data(), cap);
  for (uint32_t i = 0; i < n; i++)
    out[i] = lit_instance(buf[i]);
  return n;
}

static ApplyResult decide_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return decide_apply_canonical(k);
  Lit l;
  if (!lit_of_instance(inst, &l))
    return ApplyResult::NoOp;
  return decide_apply(k, l, true);
}

TACTIX_REGISTER_RULE(Decide, RuleTier::Micro, decide_guard,
                     decide_apply_canonical, decide_instances,
                     decide_apply_instance);

} // namespace tactix
