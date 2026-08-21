#include "rules/backbone/backbone.hpp"

#include "rules/support/failed_literal.hpp"

namespace tactix {

static constexpr FailedLiteralStats backbone_stats = {
    &Stats::backbone_rounds, &Stats::backbone_probed, &Stats::backbone_failed};

RuleEligibility backbone_guard(const Kernel &k) {
  return failed_literal_guard(k);
}

void backbone_candidates(const Kernel &k, std::vector<Lit> &out) {
  failed_literal_candidates(k, out);
}

ApplyResult backbone_apply(Kernel &k) {
  return failed_literal_apply_chained_binary(k, backbone_stats);
}

ApplyResult backbone_try(Kernel &k, Lit p) {
  return failed_literal_try(k, p, FailedLiteralPropagation::Binary,
                            backbone_stats);
}

static ApplyResult backbone_apply_instance(Kernel &k, uint64_t inst) {
  return failed_literal_apply_instance(
      k, inst, FailedLiteralPropagation::Binary, backbone_stats);
}

TACTIX_REGISTER_RULE(Backbone, RuleTier::Inprocess, backbone_guard,
                     backbone_apply, failed_literal_instances,
                     backbone_apply_instance);

} // namespace tactix
