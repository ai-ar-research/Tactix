#include "strategy/congruence_parameters.hpp"

#include "kernel/kernel.hpp"

namespace tactix::strategy {

ScopedCongruenceParameters::ScopedCongruenceParameters(
    Kernel &kernel, const CongruenceParameters &parameters)
    : kernel_(kernel), saved_(kernel.congruence_parameters) {
  kernel_.congruence_parameters = parameters;
}

ScopedCongruenceParameters::~ScopedCongruenceParameters() {
  kernel_.congruence_parameters = saved_;
}

} // namespace tactix::strategy
