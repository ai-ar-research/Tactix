#include "strategy/forward_parameters.hpp"

#include "kernel/kernel.hpp"

namespace tactix::strategy {

void install_rule_parameters(Kernel &k, const SubsumeForwardParameters &parameters) {
  k.subsume_forward_parameters = parameters;
}

void install_rule_parameters(Kernel &k, const StrengthenParameters &parameters) {
  k.strengthen_parameters = parameters;
}

ScopedSubsumeForwardParameters::ScopedSubsumeForwardParameters(
    Kernel &kernel, const SubsumeForwardParameters &parameters)
    : kernel_(kernel), saved_(kernel.subsume_forward_parameters) {
  install_rule_parameters(kernel_, parameters);
}

ScopedSubsumeForwardParameters::~ScopedSubsumeForwardParameters() {
  install_rule_parameters(kernel_, saved_);
}

ScopedStrengthenParameters::ScopedStrengthenParameters(
    Kernel &kernel, const StrengthenParameters &parameters)
    : kernel_(kernel), saved_(kernel.strengthen_parameters) {
  install_rule_parameters(kernel_, parameters);
}

ScopedStrengthenParameters::~ScopedStrengthenParameters() {
  install_rule_parameters(kernel_, saved_);
}

} // namespace tactix::strategy
