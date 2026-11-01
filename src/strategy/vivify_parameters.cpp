#include "strategy/vivify_parameters.hpp"

#include "kernel/kernel.hpp"

namespace tactix::strategy {

VivifyParameters VivifyParameters::from_kernel(const Kernel &kernel) {
  return {kernel.vivify_scope, kernel.vivify_reuse, kernel.vivify_flush,
          kernel.vivify_irredundant_sort_ratio};
}

void install_rule_parameters(Kernel &k, const VivifyParameters &parameters) {
  k.vivify_scope = parameters.scope;
  k.vivify_reuse = parameters.reuse;
  k.vivify_flush = parameters.flush;
  k.vivify_irredundant_sort_ratio =
      parameters.irredundant_sort_ratio;
}

} // namespace tactix::strategy
