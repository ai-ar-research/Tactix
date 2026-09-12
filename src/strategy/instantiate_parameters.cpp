#include "strategy/instantiate_parameters.hpp"

#include "kernel/kernel.hpp"

namespace tactix::strategy {

ScopedInstantiateParameters::ScopedInstantiateParameters(
    Kernel &kernel, const InstantiateParameters &parameters)
    : kernel_(kernel), saved_(kernel.instantiate_parameters) {
  kernel_.instantiate_parameters = parameters;
}

ScopedInstantiateParameters::~ScopedInstantiateParameters() {
  kernel_.instantiate_parameters = saved_;
}

} // namespace tactix::strategy
