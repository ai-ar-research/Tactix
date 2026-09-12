#include "strategy/analyze_parameters.hpp"

#include "kernel/kernel.hpp"

namespace tactix::strategy {

AnalyzeParameters AnalyzeParameters::from_kernel(const Kernel &k) {
  return {k.analyze_parameters.minimize_depth_limit,
          k.conflict_reuse,
          k.conflict_reuse_chrono_levels,
          k.bump_reason_limit,
          k.bump_reason_depth,
          k.eager_subsume,
          k.jump_reasons,
          k.jump_reasons_bigbig,
          k.shrink_glue_promote,
          !k.no_ext_reason_bump};
}

static void install(Kernel &k, const AnalyzeParameters &p) {
  k.analyze_parameters.minimize_depth_limit = p.minimize_depth_limit;
  k.conflict_reuse = p.conflict_reuse;
  k.conflict_reuse_chrono_levels = p.chronological_levels;
  k.bump_reason_limit = p.bump_reason_limit;
  k.bump_reason_depth = p.bump_reason_depth;
  k.eager_subsume = p.eager_subsume;
  k.jump_reasons = p.jump_reasons;
  k.jump_reasons_bigbig = p.jump_reasons_bigbig;
  k.shrink_glue_promote = p.shrink_glue_promote;
  k.no_ext_reason_bump = !p.bump_extension_reasons;
}

ScopedAnalyzeParameters::ScopedAnalyzeParameters(
    Kernel &kernel, const AnalyzeParameters &parameters)
    : kernel_(kernel), saved_(AnalyzeParameters::from_kernel(kernel)) {
  install(kernel_, parameters);
}

ScopedAnalyzeParameters::~ScopedAnalyzeParameters() {
  install(kernel_, saved_);
}

} // namespace tactix::strategy
