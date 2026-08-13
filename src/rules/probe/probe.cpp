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

static ApplyResult probe_apply_instance(Kernel &k, uint64_t inst) {
  return failed_literal_apply_instance(k, inst, FailedLiteralPropagation::Full,
                                       probe_stats);
}

TACTIX_REGISTER_RULE(Probe, RuleTier::Inprocess, probe_guard, probe_apply,
                     failed_literal_instances, probe_apply_instance);

} // namespace tactix
