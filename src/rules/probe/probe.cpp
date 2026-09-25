#include "rules/probe/probe.hpp"

#include "rules/support/failed_literal.hpp"

namespace tactix {

static constexpr FailedLiteralStats probe_stats = {
    &Stats::probe_rounds, &Stats::probed, &Stats::probe_failed};

RuleEligibility probe_guard(const Kernel &k) {
  return failed_literal_guard(k);
}

void probe_candidates(const Kernel &k, std::vector<Lit> &out) {
  failed_literal_candidates(k, out);
}

ApplyResult probe_apply(Kernel &k) {
  return failed_literal_apply(k, FailedLiteralPropagation::Full, probe_stats);
}

ApplyResult probe_try(Kernel &k, Lit p) {
  return failed_literal_try(k, p, FailedLiteralPropagation::Full, probe_stats);
}

// The canonical round uses the reference BIG-head schedule. A public caller
// may probe any unassigned literal: root-fixed literals can make a physical
// large clause propagate without a binary watch on the assumed head.
static uint32_t probe_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  uint32_t n = 0;
  for (Var v = 0; v < k.nvars && n < cap; v++) {
    if (k.value(make_lit(v, false)) != 0)
      continue;
    for (int neg = 0; neg <= 1 && n < cap; neg++)
      out[n++] = lit_instance(k, make_lit(v, neg != 0));
  }
  return n;
}

static ApplyResult probe_apply_instance(Kernel &k, uint64_t inst) {
  return failed_literal_apply_instance(k, inst, FailedLiteralPropagation::Full,
                                       probe_stats);
}

TACTIX_REGISTER_RULE(Probe, RuleTier::Inprocess, probe_guard, probe_apply,
                     probe_instances, probe_apply_instance);

} // namespace tactix
