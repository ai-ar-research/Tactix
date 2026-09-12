#include "strategy/failed_literal_parameters.hpp"

#include "kernel/kernel.hpp"

namespace tactix::strategy {

ScopedBackboneParameters::ScopedBackboneParameters(
    Kernel &kernel, const BackboneParameters &parameters)
    : kernel_(kernel), saved_(kernel.backbone_parameters) {
  kernel_.backbone_parameters = parameters;
}

ScopedBackboneParameters::~ScopedBackboneParameters() {
  kernel_.backbone_parameters = saved_;
}

} // namespace tactix::strategy
