#include "strategy/forward_parameters.hpp"

#include "kernel/kernel.hpp"

namespace tactix::strategy {

ScopedSubsumeForwardParameters::ScopedSubsumeForwardParameters(
    Kernel &kernel, const SubsumeForwardParameters &parameters)
    : kernel_(kernel), saved_(kernel.subsume_forward_parameters) {
  kernel_.subsume_forward_parameters = parameters;
}

ScopedSubsumeForwardParameters::~ScopedSubsumeForwardParameters() {
  kernel_.subsume_forward_parameters = saved_;
}

ScopedStrengthenParameters::ScopedStrengthenParameters(
    Kernel &kernel, const StrengthenParameters &parameters)
    : kernel_(kernel), saved_(kernel.strengthen_parameters) {
  kernel_.strengthen_parameters = parameters;
}

ScopedStrengthenParameters::~ScopedStrengthenParameters() {
  kernel_.strengthen_parameters = saved_;
}

} // namespace tactix::strategy
