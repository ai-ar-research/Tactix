#include "strategy/congruence_parameters.hpp"

#include "kernel/kernel.hpp"

namespace tactix::strategy {

void install_rule_parameters(Kernel &k, const CongruenceParameters &parameters) {
  k.congruence_parameters = parameters;
}

ScopedCongruenceParameters::ScopedCongruenceParameters(
    Kernel &kernel, const CongruenceParameters &parameters)
    : kernel_(kernel), saved_(kernel.congruence_parameters) {
  install_rule_parameters(kernel_, parameters);
}

ScopedCongruenceParameters::~ScopedCongruenceParameters() {
  install_rule_parameters(kernel_, saved_);
}

} // namespace tactix::strategy
