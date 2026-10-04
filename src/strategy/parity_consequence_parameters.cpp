#include "strategy/parity_consequence_parameters.hpp"
#include "kernel/kernel.hpp"
namespace tactix::strategy {
ParityConsequenceParameters ParityConsequenceParameters::from_kernel(const Kernel &k) {
  return k.parity_consequence_parameters;
}
void install_rule_parameters(Kernel &k, const ParityConsequenceParameters &p) {
  k.parity_consequence_parameters = p;
}
} // namespace tactix::strategy
