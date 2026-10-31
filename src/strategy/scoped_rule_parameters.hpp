#pragma once

namespace tactix {

struct Kernel;

namespace strategy {

// Installs `parameters` for one scope and restores the kernel's prior values,
// read with `Parameters::from_kernel`, on exit.
template <class Parameters> class ScopedRuleParameters {
public:
  ScopedRuleParameters(Kernel &kernel, const Parameters &parameters)
      : kernel_(kernel), saved_(Parameters::from_kernel(kernel)) {
    install_rule_parameters(kernel_, parameters);
  }
  ScopedRuleParameters(const ScopedRuleParameters &) = delete;
  ScopedRuleParameters &operator=(const ScopedRuleParameters &) = delete;
  ~ScopedRuleParameters() { install_rule_parameters(kernel_, saved_); }

private:
  Kernel &kernel_;
  Parameters saved_;
};

} // namespace strategy
} // namespace tactix
