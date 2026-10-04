#include "strategy/theory_action_parameters.hpp"

#include "kernel/kernel.hpp"

namespace tactix::strategy {

AffineConsequenceParameters
AffineConsequenceParameters::from_kernel(const Kernel &k) {
  return k.affine_consequence_parameters;
}
ExactCoverExtensionParameters
ExactCoverExtensionParameters::from_kernel(const Kernel &k) {
  return k.exact_cover_extension_parameters;
}
IndependentSetCoverParameters
IndependentSetCoverParameters::from_kernel(const Kernel &k) {
  return k.independent_set_cover_parameters;
}
SemanticVariableAdviceParameters
SemanticVariableAdviceParameters::from_kernel(const Kernel &k) {
  return k.semantic_variable_advice_parameters;
}

void install_rule_parameters(Kernel &k,
                             const AffineConsequenceParameters &parameters) {
  k.affine_consequence_parameters = parameters;
}
void install_rule_parameters(
    Kernel &k, const ExactCoverExtensionParameters &parameters) {
  k.exact_cover_extension_parameters = parameters;
}
void install_rule_parameters(Kernel &k,
                             const IndependentSetCoverParameters &parameters) {
  k.independent_set_cover_parameters = parameters;
}
void install_rule_parameters(
    Kernel &k, const SemanticVariableAdviceParameters &parameters) {
  k.semantic_variable_advice_parameters = parameters;
}

} // namespace tactix::strategy
