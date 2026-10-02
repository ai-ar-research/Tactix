#include "strategy/failed_literal_parameters.hpp"

#include "kernel/kernel.hpp"

namespace tactix::strategy {

void install_rule_parameters(Kernel &k, const BackboneParameters &parameters) {
  k.backbone_parameters = parameters;
}

ScopedBackboneParameters::ScopedBackboneParameters(
    Kernel &kernel, const BackboneParameters &parameters)
    : kernel_(kernel), saved_(kernel.backbone_parameters) {
  install_rule_parameters(kernel_, parameters);
}

ScopedBackboneParameters::~ScopedBackboneParameters() {
  install_rule_parameters(kernel_, saved_);
}

} // namespace tactix::strategy
