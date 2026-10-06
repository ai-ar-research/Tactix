#include "strategy/theory_action_parameters.hpp"

#include "kernel/kernel.hpp"
#include "rules/semantic_extension/direct_csp_support_action.hpp"
#include "rules/semantic_extension/nary_csp_prefix_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_amo_action.hpp"
#include "rules/semantic_extension/gf3_general_row_action.hpp"
#include "rules/semantic_extension/gf3_affine_action.hpp"
#include "rules/direct_php/direct_php_action.hpp"
#include "rules/lucky/tseitin_cycle_action.hpp"
#include "rules/semantic_extension/gf3_gaussian_action.hpp"

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

bool NaryCspPrefixParameters::valid() const {
  return NaryCspPrefixActionLimits{
             maximum_source_clauses,
             maximum_source_literals,
             maximum_recognition_work,
             maximum_domains,
             maximum_domain_values,
             maximum_forbidden_tuples,
             maximum_prefix_arity,
             maximum_definition_clause_literals,
             maximum_proof_work,
             maximum_proof_additions,
             maximum_proof_literal_occurrences,
             maximum_journal_bytes,
             maximum_extent_growth,
             maximum_external_extent,
             maximum_session_keys,
             maximum_session_work}
      .valid();
}

bool NaryCspEqualityParameters::valid() const {
  return NaryCspEqualityLimits{
             maximum_source_clauses, maximum_source_literals,
             maximum_recognition_work, maximum_domain_values,
             maximum_forbidden_tuples, maximum_proof_work,
             maximum_proof_additions, maximum_proof_literal_occurrences,
             maximum_journal_bytes, maximum_extent_growth,
             maximum_external_extent, maximum_session_keys}
      .valid();
}

bool NaryCspEqualityAmoParameters::valid() const {
  return NaryCspEqualityAmoLimits{
             maximum_source_clauses, maximum_source_literals,
             maximum_recognition_work, maximum_domain_values,
             maximum_forbidden_tuples, maximum_proof_work,
             maximum_proof_additions, maximum_proof_literal_occurrences,
             maximum_journal_bytes, maximum_external_extent,
             maximum_session_keys}
      .valid();
}

bool DirectPhpRefutationParameters::valid() const {
  return DirectPhpActionLimits{
      {maximum_snapshot_clauses, maximum_snapshot_literals,
       maximum_snapshot_work},
      {maximum_recognition_clauses, maximum_recognition_literals,
       maximum_recognition_work, maximum_row_candidates,
       maximum_row_combinations, maximum_mapping_trials, maximum_holes},
      {maximum_proof_additions, maximum_proof_deletions,
       maximum_proof_literals, maximum_fresh_variables,
       maximum_proof_events},
      maximum_replay_work}.valid();
}

bool Gf3GaussianConsequenceParameters::valid() const {
  return Gf3GaussianActionLimits{
      {maximum_source_clauses, maximum_source_literals, maximum_domains,
       maximum_equations, maximum_equation_arity,
       maximum_tuples_per_equation},
      {{maximum_bridge_work, maximum_bridge_source_clauses,
        maximum_bridge_source_literals, maximum_bridge_domains,
        maximum_bridge_equations, maximum_bridge_terms,
        maximum_authentication_references, maximum_references_per_equation,
        {maximum_gaussian_work, maximum_fill_entries,
         maximum_intermediate_width, maximum_provenance_terms,
         maximum_derivations, maximum_retained_plan_bytes}},
       maximum_certificate_work, maximum_events, maximum_event_literals,
       maximum_active_clauses, maximum_retained_source_references,
       maximum_event_width, maximum_branch_domains, maximum_consequences},
      maximum_capture_work, maximum_revalidation_work,
      maximum_proof_additions, maximum_proof_literal_occurrences,
      maximum_journal_bytes, maximum_identity_bytes, maximum_external_extent,
      maximum_session_keys, maximum_session_work,
      maximum_session_identity_bytes}.valid();
}

namespace {
Gf3GeneralRowActionLimits general_row_limits(
    const Gf3GeneralRowConsequenceParameters &p) {
  const auto &g = p.gaussian;
  Gf3GeneralRowActionLimits result;
  result.live.recognition = {
      g.maximum_source_clauses, g.maximum_source_literals, g.maximum_domains,
      g.maximum_equations, g.maximum_equation_arity,
      g.maximum_tuples_per_equation};
  result.live.planner = {
      {g.maximum_bridge_work, g.maximum_bridge_source_clauses,
       g.maximum_bridge_source_literals, g.maximum_bridge_domains,
       g.maximum_bridge_equations, g.maximum_bridge_terms,
       g.maximum_authentication_references,
       g.maximum_references_per_equation,
       {g.maximum_gaussian_work, g.maximum_fill_entries,
        g.maximum_intermediate_width, g.maximum_provenance_terms,
        g.maximum_derivations, g.maximum_retained_plan_bytes}},
      g.maximum_certificate_work, g.maximum_events,
      g.maximum_event_literals, g.maximum_active_clauses,
      g.maximum_retained_source_references, p.maximum_row_tuples,
      g.maximum_event_width, g.maximum_branch_domains,
      p.maximum_row_width};
  result.live.maximum_capture_work = g.maximum_capture_work;
  result.live.maximum_duplicate_scan_work = p.maximum_duplicate_scan_work;
  result.live.maximum_revalidation_work = g.maximum_revalidation_work;
  result.live.maximum_proof_additions = g.maximum_proof_additions;
  result.live.maximum_proof_literal_occurrences =
      g.maximum_proof_literal_occurrences;
  result.live.maximum_arena_words = p.maximum_arena_words;
  result.live.maximum_journal_bytes = g.maximum_journal_bytes;
  result.live.maximum_identity_bytes = g.maximum_identity_bytes;
  result.live.maximum_external_extent = g.maximum_external_extent;
  result.maximum_session_keys = g.maximum_session_keys;
  result.maximum_session_work = g.maximum_session_work;
  result.maximum_session_identity_bytes = g.maximum_session_identity_bytes;
  return result;
}
} // namespace

bool Gf3GeneralRowConsequenceParameters::valid() const {
  return gaussian.valid() && general_row_limits(*this).valid();
}

bool TseitinCycleBreakingParameters::valid() const {
  TseitinCycleBreakingLimits limits;
  limits.snapshot = {maximum_snapshot_clauses, maximum_snapshot_literals,
                     maximum_snapshot_work, maximum_snapshot_variables,
                     nullptr, nullptr};
  limits.discovery = {maximum_discovery_work, maximum_discovery_clauses,
                      maximum_discovery_groups, 4, nullptr, nullptr,
                      maximum_components, maximum_discovery_retained_bytes};
  limits.admission = {maximum_admission_work, nullptr, nullptr};
  limits.planning = {maximum_planning_work, maximum_witness_length,
                     maximum_planning_additions};
  limits.transaction = {maximum_transaction_work,
                        maximum_transaction_additions,
                        maximum_transaction_deletions,
                        maximum_transaction_helpers,
                        maximum_transaction_witness_literals,
                        nullptr, nullptr};
  limits.maximum_validation_ticks = maximum_validation_ticks;
  limits.maximum_components = maximum_components;
  limits.maximum_identity_bytes = maximum_identity_bytes;
  limits.maximum_total_identity_bytes = maximum_total_identity_bytes;
  limits.maximum_identity_work = maximum_identity_work;
  limits.maximum_journal_bytes = maximum_journal_bytes;
  limits.maximum_session_keys = maximum_session_keys;
  limits.maximum_session_work = maximum_session_work;
  limits.maximum_session_identity_bytes = maximum_session_identity_bytes;
  return limits.valid();
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
NaryCspPrefixParameters
NaryCspPrefixParameters::from_kernel(const Kernel &k) {
  return k.nary_csp_prefix_parameters;
}
NaryCspEqualityParameters
NaryCspEqualityParameters::from_kernel(const Kernel &k) {
  return k.nary_csp_equality_parameters;
}
NaryCspEqualityAmoParameters
NaryCspEqualityAmoParameters::from_kernel(const Kernel &k) {
  return k.nary_csp_equality_amo_parameters;
}
DirectPhpRefutationParameters
DirectPhpRefutationParameters::from_kernel(const Kernel &k) {
  return k.direct_php_refutation_parameters;
}
Gf3GaussianConsequenceParameters
Gf3GaussianConsequenceParameters::from_kernel(const Kernel &k) {
  return k.gf3_gaussian_consequence_parameters;
}
Gf3GeneralRowConsequenceParameters
Gf3GeneralRowConsequenceParameters::from_kernel(const Kernel &k) {
  return k.gf3_general_row_consequence_parameters;
}
TseitinCycleBreakingParameters
TseitinCycleBreakingParameters::from_kernel(const Kernel &k) {
  return k.tseitin_cycle_breaking_parameters;
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
void install_rule_parameters(Kernel &k,
                             const NaryCspPrefixParameters &parameters) {
  k.nary_csp_prefix_parameters = parameters;
}
void install_rule_parameters(Kernel &k,
                             const NaryCspEqualityParameters &parameters) {
  k.nary_csp_equality_parameters = parameters;
}
void install_rule_parameters(
    Kernel &k, const NaryCspEqualityAmoParameters &parameters) {
  k.nary_csp_equality_amo_parameters = parameters;
}
void install_rule_parameters(Kernel &k,
                             const DirectPhpRefutationParameters &parameters) {
  k.direct_php_refutation_parameters = parameters;
}
void install_rule_parameters(
    Kernel &k, const Gf3GaussianConsequenceParameters &parameters) {
  k.gf3_gaussian_consequence_parameters = parameters;
}
void install_rule_parameters(
    Kernel &k, const Gf3GeneralRowConsequenceParameters &parameters) {
  k.gf3_general_row_consequence_parameters = parameters;
}
void install_rule_parameters(
    Kernel &k, const TseitinCycleBreakingParameters &parameters) {
  k.tseitin_cycle_breaking_parameters = parameters;
}
void install_rule_parameters(
    Kernel &k, const SemanticVariableAdviceParameters &parameters) {
  k.semantic_variable_advice_parameters = parameters;
}

} // namespace tactix::strategy
