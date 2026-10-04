#include "strategy/ramsey_startup_parameters.hpp"
#include "kernel/kernel.hpp"
namespace tactix::strategy {
RamseyStartupParameters RamseyStartupParameters::from_kernel(const Kernel &k) {
  return k.ramsey_startup_parameters;
}
void install_rule_parameters(Kernel &k, const RamseyStartupParameters &p) {
  k.ramsey_startup_parameters = p;
}
} // namespace tactix::strategy
