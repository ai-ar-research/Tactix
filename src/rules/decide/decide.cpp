// Canonical pick transliterated from kissat/src/decide.c (MIT): VMTF
// last-enqueued-unassigned in (phase-1) focused mode, saved-phase selection.
#include "rules/decide/decide.hpp"

namespace tactix {

bool decide_guard(const Kernel &k) {
  return !k.conflict_valid && !k.inconsistent && !k.pending.valid &&
         k.unassigned > 0;
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
  const Var v = kernel_queue_next_unassigned(k);
  const Lit lit = make_lit(v, k.saved[v] < 0);
  return decide_apply(k, lit, false);
}

uint32_t decide_candidates(const Kernel &k, Lit *out, uint32_t cap) {
  uint32_t n = 0;
  // walk the VMTF queue from most-recent to least, unassigned vars only
  for (Var v = k.search_idx; v != UINT32_MAX && n < cap;
       v = k.links[v].prev)
    if (k.values[make_lit(v, false)] == 0)
      out[n++] = make_lit(v, k.saved[v] < 0);
  return n;
}

TACTIX_REGISTER_RULE(Decide, RuleTier::Micro, decide_guard,
                     decide_apply_canonical);

} // namespace tactix
