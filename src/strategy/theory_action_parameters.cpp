#include "strategy/theory_action_parameters.hpp"

#include "kernel/kernel.hpp"
#include "rules/semantic_extension/direct_csp_support_action.hpp"
#include "rules/semantic_extension/gf3_affine_action.hpp"

namespace tactix::strategy {

bool Gf3AffineConsequenceParameters::valid() const {
  const Gf3AffineActionLimits limits{
      {maximum_source_clauses, maximum_source_literals, maximum_domains,
       maximum_equations, maximum_equation_arity,
       maximum_tuples_per_equation},
      {maximum_result_arity, maximum_result_tuples, maximum_additions,
       maximum_addition_literals},
      maximum_recognition_work,
      maximum_planning_work,
      maximum_duplicate_scan_work,
      maximum_revalidation_work,
      maximum_proof_work,
      maximum_proof_additions,
      maximum_proof_literal_occurrences,
      maximum_arena_words,
      maximum_journal_bytes,
      maximum_external_extent,
      maximum_session_keys,
      maximum_session_work};
  return limits.valid();
}

bool DirectCspSupportParameters::valid() const {
  return DirectCspSupportActionLimits{
             maximum_source_clauses,
             maximum_source_literals,
             maximum_recognition_work,
             maximum_domain_values,
             maximum_direct_clause_literals,
             maximum_definition_clause_literals,
             maximum_proof_work,
             maximum_proof_additions,
             maximum_proof_literal_occurrences,
             maximum_journal_bytes,
             maximum_extent_growth,
             maximum_external_extent,
             maximum_session_keys,
             maximum_session_work,
             allow_extension}
      .valid();
}

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
Gf3AffineConsequenceParameters
Gf3AffineConsequenceParameters::from_kernel(const Kernel &k) {
  return k.gf3_affine_consequence_parameters;
}
DirectCspSupportParameters
DirectCspSupportParameters::from_kernel(const Kernel &k) {
  return k.direct_csp_support_parameters;
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
    Kernel &k, const Gf3AffineConsequenceParameters &parameters) {
  k.gf3_affine_consequence_parameters = parameters;
}
void install_rule_parameters(Kernel &k,
                             const DirectCspSupportParameters &parameters) {
  k.direct_csp_support_parameters = parameters;
}
void install_rule_parameters(
    Kernel &k, const SemanticVariableAdviceParameters &parameters) {
  k.semantic_variable_advice_parameters = parameters;
}

} // namespace tactix::strategy
