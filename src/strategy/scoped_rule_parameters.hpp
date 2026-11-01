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

// A typed family's field list. Each `visit_parameter_fields(FieldsOf<Family>,
// p, visit)` overload passes the family's fields of `p`, a typed profile or
// its C API mirror with the same member names, to `visit` in declaration
// order, so conversion, all-zero checks and stable text share one order.
template <class Family> struct FieldsOf {};

// Assigns each listed field of `source` to the same field of `target`.
template <class Family, class Target, class Source>
void copy_parameter_fields(Target &target, const Source &source) {
  visit_parameter_fields(FieldsOf<Family>{}, target, [&](auto &...targets) {
    visit_parameter_fields(FieldsOf<Family>{}, source,
                           [&](const auto &...sources) {
                             ((targets = sources), ...);
                           });
  });
}

// Whether any listed field of `source` is nonzero.
template <class Family, class Source>
bool any_parameter_field(const Source &source) {
  return visit_parameter_fields(
      FieldsOf<Family>{}, source,
      [](const auto &...fields) { return (... || fields); });
}

} // namespace strategy
} // namespace tactix
