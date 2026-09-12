#include "strategy/vivify_parameters.hpp"

#include "kernel/kernel.hpp"

namespace tactix::strategy {

VivifyParameters VivifyParameters::from_kernel(const Kernel &kernel) {
  return {kernel.vivify_scope, kernel.vivify_reuse, kernel.vivify_flush,
          kernel.vivify_irredundant_sort_ratio};
}

ScopedVivifyParameters::ScopedVivifyParameters(
    Kernel &kernel, const VivifyParameters &parameters)
    : kernel_(kernel), saved_(VivifyParameters::from_kernel(kernel)) {
  install(parameters);
}

ScopedVivifyParameters::~ScopedVivifyParameters() { install(saved_); }

void ScopedVivifyParameters::install(const VivifyParameters &parameters) {
  kernel_.vivify_scope = parameters.scope;
  kernel_.vivify_reuse = parameters.reuse;
  kernel_.vivify_flush = parameters.flush;
  kernel_.vivify_irredundant_sort_ratio =
      parameters.irredundant_sort_ratio;
}

} // namespace tactix::strategy
