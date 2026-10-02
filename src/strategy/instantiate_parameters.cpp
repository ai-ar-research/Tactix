#include "strategy/instantiate_parameters.hpp"

#include "kernel/kernel.hpp"

namespace tactix::strategy {

void install_rule_parameters(Kernel &k, const InstantiateParameters &parameters) {
  k.instantiate_parameters = parameters;
}

ScopedInstantiateParameters::ScopedInstantiateParameters(
    Kernel &kernel, const InstantiateParameters &parameters)
    : kernel_(kernel), saved_(kernel.instantiate_parameters) {
  install_rule_parameters(kernel_, parameters);
}

ScopedInstantiateParameters::~ScopedInstantiateParameters() {
  install_rule_parameters(kernel_, saved_);
}

} // namespace tactix::strategy
