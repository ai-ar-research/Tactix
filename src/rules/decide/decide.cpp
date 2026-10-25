#include "rules/decide/decide.hpp"

namespace tactix {

// Pure precondition (docs/dev/calculus.md §3 M2): no conflict, not refuted,
// some variable unassigned. No propagation-fixpoint conjunct (D-LEVEL-1), so a
// policy may take several decisions in a row — the N1 capability a learned
// policy reasons over. Off-fixpoint deciding is made safe by the
// `trail_tainted` kernel repair, not by a guard (kernel.hpp).
RuleEligibility decide_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("no-conflict", !k.conflict_valid)
      .fact("consistent", !k.inconsistent)
      .fact("has-unassigned", k.unassigned > 0);
}

ApplyResult decide_apply(Kernel &k, Lit lit, bool external) {
  if (var_of(lit) >= k.nvars || k.values[lit] != 0)
    return ApplyResult::NoOp;
  // D-LEVEL-1: a decision taken off a propagation fixpoint taints the trail —
  // a not-yet-run watch scan can rest a watch on an "ahead" assignment, so the
  // next backtrack must re-propagate the whole trail (kernel.hpp
  // `trail_tainted`). DefaultPolicy decides only at a fixpoint, so it never
  // sets this and stays bit-exact.
  if (k.propagate_head < k.trail.size())
    k.trail_tainted = true;
  k.stats.decisions++;
  if (k.first_extension_var != UINT32_MAX && var_of(lit) >= k.first_extension_var)
    k.stats.decisions_extension++; // task #11 diagnostic (see kernel.hpp)
  if (kernel_var_retired(k, var_of(lit)))
    k.stats.decisions_substituted++; // see Stats::decisions_substituted
  if (external)
    k.stats.api_decisions++;
  kernel_assign_decision(k, lit);
  return ApplyResult::Ok;
}

ApplyResult decide_apply_canonical(Kernel &k) {
  assert(k.unassigned > 0);
  for (Var v = 0; v < k.nvars; v++)
    if (k.values[make_lit(v, false)] == 0)
      return decide_apply(k, make_lit(v, false), false);
  return ApplyResult::NoOp;
}

uint32_t decide_candidates(const Kernel &k, Lit *out, uint32_t cap) {
  uint32_t n = 0;
  for (Var v = 0; v < k.nvars && n < cap; v++)
    if (k.values[make_lit(v, false)] == 0)
      out[n++] = make_lit(v, false);
  return n;
}

// The rule surface is semantic rather than strategic: stable variable order,
// positive then negative. A policy ranks this complete legal set separately.
static uint32_t decide_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  const uint32_t vars = (cap + 1) / 2;
  std::vector<Lit> buf(vars);
  const uint32_t n = decide_candidates(k, buf.data(), vars);
  uint32_t m = 0;
  for (uint32_t i = 0; i < n && m < cap; i++) {
    out[m++] = lit_instance(k, buf[i]);
    if (m < cap)
      out[m++] = lit_instance(k, negate(buf[i]));
  }
  return m;
}

static ApplyResult decide_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return decide_apply_canonical(k);
  Lit l;
  if (!lit_of_instance(k, inst, &l))
    return ApplyResult::NoOp;
  return decide_apply(k, l, true);
}

TACTIX_REGISTER_RULE(Decide, RuleTier::Micro, decide_guard,
                     decide_apply_canonical, decide_instances,
                     decide_apply_instance);

} // namespace tactix
