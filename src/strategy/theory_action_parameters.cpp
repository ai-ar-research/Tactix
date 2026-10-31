#include "strategy/theory_action_parameters.hpp"

#include "kernel/kernel.hpp"
#include "rules/semantic_extension/affine_basis_action.hpp"
#include "rules/semantic_extension/direct_csp_support_action.hpp"
#include "rules/semantic_extension/direct_csp_guarded_cover_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_transitivity_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_amo_action.hpp"
#include "rules/semantic_extension/gf3_general_row_action.hpp"
#include "rules/semantic_extension/gf3_affine_action.hpp"
#include "rules/direct_php/direct_php_action.hpp"
#include "rules/lucky/tseitin_cycle_action.hpp"
#include "rules/semantic_extension/gf3_gaussian_action.hpp"

namespace tactix::strategy {

AffineBasisActionLimits action_limits(const AffineConsequenceParameters &p) {
  AffineBasisActionLimits limits;
  limits.image = {p.maximum_clauses, p.maximum_groups, p.maximum_patterns,
                  p.maximum_elimination_work, p.maximum_arity,
                  p.maximum_intermediate_width, p.maximum_sources,
                  p.maximum_output_width};
  limits.basis = {p.basis_maximum_work,
                  p.basis_maximum_intermediate_width,
                  p.basis_maximum_sources,
                  p.basis_maximum_premises,
                  p.basis_maximum_nodes,
                  p.basis_maximum_direct_width};
  limits.maximum_snapshot_literals = p.maximum_snapshot_literals;
  limits.maximum_snapshot_work = p.maximum_snapshot_work;
  limits.maximum_proof_additions = p.maximum_proof_additions;
  limits.maximum_proof_deletions = p.maximum_proof_deletions;
  limits.maximum_journal_bytes = p.maximum_journal_bytes;
  limits.maximum_external_extent = p.maximum_external_extent;
  limits.maximum_session_keys = p.maximum_session_keys;
  limits.maximum_session_work = p.maximum_session_work;
  return limits;
}

Gf3AffineActionLimits action_limits(
    const Gf3AffineConsequenceParameters &p) {
  return {
      {p.maximum_source_clauses, p.maximum_source_literals, p.maximum_domains,
       p.maximum_equations, p.maximum_equation_arity,
       p.maximum_tuples_per_equation},
      {p.maximum_result_arity, p.maximum_result_tuples, p.maximum_additions,
       p.maximum_addition_literals},
      p.maximum_recognition_work, p.maximum_planning_work,
      p.maximum_duplicate_scan_work, p.maximum_revalidation_work,
      p.maximum_proof_work, p.maximum_proof_additions,
      p.maximum_proof_literal_occurrences, p.maximum_arena_words,
      p.maximum_journal_bytes, p.maximum_external_extent,
      p.maximum_session_keys, p.maximum_session_work};
}

bool Gf3AffineConsequenceParameters::valid() const {
  return action_limits(*this).valid();
}

DirectCspGuardedCoverActionLimits action_limits(
    const DirectCspGuardedCoverParameters &p) {
  return DirectCspGuardedCoverActionLimits{
      p.maximum_source_clauses, p.maximum_source_literals,
      p.maximum_recognition_work, p.maximum_domains,
      p.maximum_domain_values, p.maximum_cover_literals,
      p.maximum_or_definitions, p.maximum_or_inputs,
      p.maximum_output_literals, p.maximum_proof_work,
      p.maximum_proof_additions, p.maximum_proof_literal_occurrences,
      p.maximum_journal_bytes, p.maximum_external_extent,
      p.maximum_session_keys, p.maximum_session_work};
}

bool DirectCspGuardedCoverParameters::valid() const {
  return action_limits(*this).valid();
}

DirectCspSupportActionLimits action_limits(
    const DirectCspSupportParameters &p) {
  return DirectCspSupportActionLimits{
      p.maximum_source_clauses, p.maximum_source_literals,
      p.maximum_recognition_work, p.maximum_domain_values,
      p.maximum_direct_clause_literals, p.maximum_definition_clause_literals,
      p.maximum_proof_work, p.maximum_proof_additions,
      p.maximum_proof_literal_occurrences, p.maximum_journal_bytes,
      p.maximum_extent_growth, p.maximum_external_extent,
      p.maximum_session_keys, p.maximum_session_work, p.allow_extension};
}

bool DirectCspSupportParameters::valid() const {
  return action_limits(*this).valid();
}

NaryCspEqualityLimits action_limits(const NaryCspEqualityParameters &p) {
  return NaryCspEqualityLimits{
      p.maximum_source_clauses, p.maximum_source_literals,
      p.maximum_recognition_work, p.maximum_domain_values,
      p.maximum_forbidden_tuples, p.maximum_proof_work,
      p.maximum_proof_additions, p.maximum_proof_literal_occurrences,
      p.maximum_journal_bytes, p.maximum_extent_growth,
      p.maximum_external_extent, p.maximum_session_keys};
}

bool NaryCspEqualityParameters::valid() const {
  return action_limits(*this).valid();
}

NaryCspEqualityAmoLimits action_limits(
    const NaryCspEqualityAmoParameters &p) {
  return NaryCspEqualityAmoLimits{
      p.maximum_source_clauses, p.maximum_source_literals,
      p.maximum_recognition_work, p.maximum_domain_values,
      p.maximum_forbidden_tuples, p.maximum_proof_work,
      p.maximum_proof_additions, p.maximum_proof_literal_occurrences,
      p.maximum_journal_bytes, p.maximum_external_extent,
      p.maximum_session_keys};
}

bool NaryCspEqualityAmoParameters::valid() const {
  return action_limits(*this).valid();
}

NaryCspEqualityTransitivityLimits action_limits(
    const NaryCspEqualityTransitivityParameters &p) {
  return {p.maximum_source_clauses, p.maximum_source_literals,
          p.maximum_recognition_work, p.maximum_planning_work,
          p.maximum_domain_values, p.maximum_forbidden_tuples,
          p.maximum_proof_work, p.maximum_proof_additions,
          p.maximum_proof_literal_occurrences, p.maximum_journal_bytes,
          p.maximum_external_extent, p.maximum_session_keys,
          p.maximum_session_bytes, p.maximum_arena_words,
          p.maximum_watch_words, p.maximum_preparation_bytes};
}

bool NaryCspEqualityTransitivityParameters::valid() const {
  return action_limits(*this).valid();
}

DirectPhpActionLimits action_limits(const DirectPhpRefutationParameters &p) {
  return DirectPhpActionLimits{
      {p.maximum_snapshot_clauses, p.maximum_snapshot_literals,
       p.maximum_snapshot_work},
      {p.maximum_recognition_clauses, p.maximum_recognition_literals,
       p.maximum_recognition_work, p.maximum_row_candidates,
       p.maximum_row_combinations, p.maximum_mapping_trials, p.maximum_holes},
      {p.maximum_proof_additions, p.maximum_proof_deletions,
       p.maximum_proof_literals, p.maximum_fresh_variables,
       p.maximum_proof_events},
      p.maximum_replay_work};
}

bool DirectPhpRefutationParameters::valid() const {
  return action_limits(*this).valid();
}

Gf3GaussianActionLimits action_limits(
    const Gf3GaussianConsequenceParameters &p) {
  return Gf3GaussianActionLimits{
      {p.maximum_source_clauses, p.maximum_source_literals, p.maximum_domains,
       p.maximum_equations, p.maximum_equation_arity,
       p.maximum_tuples_per_equation},
      {{p.maximum_bridge_work, p.maximum_bridge_source_clauses,
        p.maximum_bridge_source_literals, p.maximum_bridge_domains,
        p.maximum_bridge_equations, p.maximum_bridge_terms,
        p.maximum_authentication_references, p.maximum_references_per_equation,
        {p.maximum_gaussian_work, p.maximum_fill_entries,
         p.maximum_intermediate_width, p.maximum_provenance_terms,
         p.maximum_derivations, p.maximum_retained_plan_bytes}},
       p.maximum_certificate_work, p.maximum_events, p.maximum_event_literals,
       p.maximum_active_clauses, p.maximum_retained_source_references,
       p.maximum_event_width, p.maximum_branch_domains,
       p.maximum_consequences},
      p.maximum_capture_work, p.maximum_revalidation_work,
      p.maximum_proof_additions, p.maximum_proof_literal_occurrences,
      p.maximum_journal_bytes, p.maximum_identity_bytes,
      p.maximum_external_extent, p.maximum_session_keys,
      p.maximum_session_work, p.maximum_session_identity_bytes};
}

bool Gf3GaussianConsequenceParameters::valid() const {
  return action_limits(*this).valid();
}

Gf3GeneralRowActionLimits action_limits(
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

bool Gf3GeneralRowConsequenceParameters::valid() const {
  return gaussian.valid() && action_limits(*this).valid();
}

TseitinCycleBreakingLimits action_limits(
    const TseitinCycleBreakingParameters &p) {
  TseitinCycleBreakingLimits limits;
  limits.snapshot = {p.maximum_snapshot_clauses, p.maximum_snapshot_literals,
                     p.maximum_snapshot_work, p.maximum_snapshot_variables,
                     nullptr, nullptr};
  limits.discovery = {p.maximum_discovery_work, p.maximum_discovery_clauses,
                      p.maximum_discovery_groups, 4, nullptr, nullptr,
                      p.maximum_components,
                      p.maximum_discovery_retained_bytes};
  limits.admission = {p.maximum_admission_work, nullptr, nullptr};
  limits.planning = {p.maximum_planning_work, p.maximum_witness_length,
                     p.maximum_planning_additions};
  limits.transaction = {p.maximum_transaction_work,
                        p.maximum_transaction_additions,
                        p.maximum_transaction_deletions,
                        p.maximum_transaction_helpers,
                        p.maximum_transaction_witness_literals,
                        nullptr, nullptr};
  limits.maximum_validation_ticks = p.maximum_validation_ticks;
  limits.maximum_components = p.maximum_components;
  limits.maximum_identity_bytes = p.maximum_identity_bytes;
  limits.maximum_total_identity_bytes = p.maximum_total_identity_bytes;
  limits.maximum_identity_work = p.maximum_identity_work;
  limits.maximum_journal_bytes = p.maximum_journal_bytes;
  limits.maximum_session_keys = p.maximum_session_keys;
  limits.maximum_session_work = p.maximum_session_work;
  limits.maximum_session_identity_bytes = p.maximum_session_identity_bytes;
  return limits;
}

bool TseitinCycleBreakingParameters::valid() const {
  return action_limits(*this).valid();
}

AffineConsequenceParameters
AffineConsequenceParameters::from_kernel(const Kernel &k) {
  return k.affine_consequence_parameters;
}

#define STORED_PARAMETERS(Type, member)                                       \
  Type Type::from_kernel(const Kernel &kernel) { return kernel.member; }       \
  void install_rule_parameters(Kernel &kernel, const Type &parameters) {      \
    kernel.member = parameters;                                                \
  }

STORED_PARAMETERS(ExactCoverExtensionParameters,
                  exact_cover_extension_parameters)
STORED_PARAMETERS(IndependentSetCoverParameters,
                  independent_set_cover_parameters)
STORED_PARAMETERS(Gf3AffineConsequenceParameters,
                  gf3_affine_consequence_parameters)
STORED_PARAMETERS(DirectCspSupportParameters,
                  direct_csp_support_parameters)
STORED_PARAMETERS(NaryCspEqualityParameters, nary_csp_equality_parameters)
STORED_PARAMETERS(NaryCspEqualityAmoParameters,
                  nary_csp_equality_amo_parameters)
STORED_PARAMETERS(NaryCspEqualityTransitivityParameters,
                  nary_csp_equality_transitivity_parameters)
STORED_PARAMETERS(DirectPhpRefutationParameters,
                  direct_php_refutation_parameters)
STORED_PARAMETERS(Gf3GaussianConsequenceParameters,
                  gf3_gaussian_consequence_parameters)
STORED_PARAMETERS(Gf3GeneralRowConsequenceParameters,
                  gf3_general_row_consequence_parameters)
STORED_PARAMETERS(TseitinCycleBreakingParameters,
                  tseitin_cycle_breaking_parameters)
STORED_PARAMETERS(SemanticVariableAdviceParameters,
                  semantic_variable_advice_parameters)

#undef STORED_PARAMETERS

AffineMatrixInitializeParameters AffineMatrixInitializeParameters::from_kernel(const Kernel &k) {
  return k.affine_matrix_initialize_parameters;
}
void install_rule_parameters(Kernel &k, const AffineMatrixInitializeParameters &parameters) {
  k.affine_matrix_initialize_parameters = parameters;
  k.affine_matrix_initialize_parameters_active = true;
  k.affine_consequence_parameters = parameters.basis;
}
void install_rule_parameters(Kernel &k,
                             const AffineConsequenceParameters &parameters) {
  k.affine_matrix_initialize_parameters_active = false;
  k.affine_consequence_parameters = parameters;
}

} // namespace tactix::strategy
