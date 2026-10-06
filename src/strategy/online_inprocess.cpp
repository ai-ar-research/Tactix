#include "strategy/online_inprocess.hpp"

#include "policies/heuristics.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/semantic_extension/affine_consequence_action.hpp"
#include "rules/semantic_extension/exact_cover_action.hpp"
#include "rules/semantic_extension/independent_set_cover_action.hpp"
#include "rules/semantic_extension/gf3_affine_action.hpp"
#include "rules/semantic_extension/gf3_gaussian_action.hpp"
#include "rules/semantic_extension/direct_csp_support_action.hpp"
#include "rules/direct_php/direct_php_action.hpp"
#include "rules/sweep/sweep.hpp"
#include "strategy/bve_invocation.hpp"
#include "strategy/rule_execution.hpp"
#include "strategy/work_execution.hpp"
#include "strategy/guidance_outcome.hpp"
#include "strategy/parameter_execution.hpp"
#include "strategy/publication_execution.hpp"
#include "strategy/semantic_variable_advice.hpp"
#include "strategy/sweep_execution.hpp"
#include "policies/rule_publication.hpp"

#include <cmath>
#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>
#include <unordered_map>

namespace tactix {
// Private cross-translation-unit admission used only for freshly enumerated
// producer certificates. It is intentionally absent from the rule API.
SubstitutionRedundancyApplicationResult
substitution_redundancy_apply_orbitopal_prefix(
    Kernel &, const SubstitutionRedundancyInstance &,
    const SubstitutionRedundancyParameters &, volatile sig_atomic_t *);
}

namespace tactix::strategy {

namespace {

} // namespace

uint64_t repeat_round_limit(const Kernel &k, const RepeatSpec &repeat) {
  if (repeat.mode != RepeatMode::UntilSizeScaledFixpoint)
    return repeat.limit;
  // satsuma.h: size is the clause database's literal count plus its variables.
  uint64_t size = 2 * k.watches.binary_clauses() + k.nvars;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage())
      size += clause->size;
    ref += clause_actual_words(clause);
  }
  constexpr double scale = static_cast<double>(uint64_t{1} << 23);
  const double raw =
      std::pow(scale / static_cast<double>(std::max<uint64_t>(size, 1)), 1.5);
  // The owner iterates while `iteration < iteration_max`, after one initial
  // pass: its pass count is the clamped scale plus one.
  if (repeat.limit && !(raw < static_cast<double>(repeat.limit)))
    return repeat.limit + 1;
  if (!(raw < 0x1p62))
    return 0;
  return std::max<uint64_t>(1, static_cast<uint64_t>(std::ceil(raw))) + 1;
}

namespace {

bool valid_repeat(const RepeatSpec &repeat) {
  switch (repeat.mode) {
  case RepeatMode::Once:
  case RepeatMode::UntilScopedFixpoint:
  case RepeatMode::UntilSizeScaledFixpoint:
    return true;
  case RepeatMode::Count:
    return repeat.limit != 0;
  }
  return false;
}

bool valid_rule_work_budget(const RuleWorkBudget &budget) {
  switch (budget.mode) {
  case RuleWorkBudgetMode::Inherit:
  case RuleWorkBudgetMode::Unlimited:
    return budget.amount == 0;
  case RuleWorkBudgetMode::Bounded:
    return true;
  }
  return false;
}

bool valid_setting_override(SettingOverride value) {
  switch (value) {
  case SettingOverride::Inherit:
  case SettingOverride::Disable:
  case SettingOverride::Enable:
    return true;
  }
  return false;
}

bool valid_post_application(PostApplication value) {
  switch (value) {
  case PostApplication::None:
  case PostApplication::Propagate:
    return true;
  }
  return false;
}

bool parameter_matches(const OnlineInvocation &step) {
  if (std::holds_alternative<std::monostate>(step.parameters))
    return true;
  if (std::holds_alternative<AnalyzeParameters>(step.parameters))
    return step.rule == "ConflictAnalysis";
  if (std::holds_alternative<BveParameters>(step.parameters))
    return step.rule == "BVE";
  if (std::holds_alternative<SubsumeForwardParameters>(step.parameters))
    return step.rule == "SubsumeForward";
  if (std::holds_alternative<StrengthenParameters>(step.parameters))
    return step.rule == "Strengthen";
  if (std::holds_alternative<ParityConsequenceParameters>(step.parameters))
    return step.rule == "ParityConsequence";
  if (std::holds_alternative<AffineConsequenceParameters>(step.parameters))
    return step.rule == "AffineConsequence" ||
           step.rule == "AffineMatrixInitialize";
  if (std::holds_alternative<ExactCoverExtensionParameters>(step.parameters))
    return step.rule == "ExactCoverExtension";
  if (std::holds_alternative<IndependentSetCoverParameters>(step.parameters))
    return step.rule == "IndependentSetCover";
  if (std::holds_alternative<Gf3AffineConsequenceParameters>(step.parameters))
    return step.rule == "Gf3AffineConsequence";
  if (std::holds_alternative<DirectCspSupportParameters>(step.parameters))
    return step.rule == "DirectCspSupport";
  if (std::holds_alternative<NaryCspPrefixParameters>(step.parameters))
    return step.rule == "NaryCspPrefix";
  if (std::holds_alternative<NaryCspEqualityParameters>(step.parameters))
    return step.rule == "NaryCspEquality";
  if (std::holds_alternative<DirectPhpRefutationParameters>(step.parameters))
    return step.rule == "DirectPhpRefutation";
  if (std::holds_alternative<Gf3GaussianConsequenceParameters>(step.parameters))
    return step.rule == "Gf3GaussianConsequence";
  if (std::holds_alternative<Gf3GeneralRowConsequenceParameters>(
          step.parameters))
    return step.rule == "Gf3GeneralRowConsequence";
  if (std::holds_alternative<TseitinCycleBreakingParameters>(step.parameters))
    return step.rule == "TseitinCycleBreaking";
  if (std::holds_alternative<SemanticVariableAdviceParameters>(step.parameters))
    return step.rule == "SemanticVariableAdvice";
  if (std::holds_alternative<CongruenceParameters>(step.parameters))
    return step.rule == "Congruence";
  if (std::holds_alternative<InstantiateParameters>(step.parameters))
    return step.rule == "Instantiate";
  if (std::holds_alternative<BackboneParameters>(step.parameters))
    return step.rule == "Backbone";
  if (std::holds_alternative<SweepParameters>(step.parameters))
    return step.rule == "Sweep";
  if (std::holds_alternative<VivifyParameters>(step.parameters))
    return step.rule == "Vivify";
  if (std::holds_alternative<BceParameters>(step.parameters))
    return step.rule == "BCE";
  if (std::holds_alternative<TernaryParameters>(step.parameters))
    return step.rule == "HyperTernaryRes";
  if (std::holds_alternative<FactorParameters>(step.parameters))
    return step.rule == "Factor";
  if (std::holds_alternative<TransitiveReduceParameters>(step.parameters))
    return step.rule == "TransitiveReduce";
  if (std::holds_alternative<SubstituteParameters>(step.parameters))
    return step.rule == "Substitute";
  if (std::holds_alternative<ReduceParameters>(step.parameters))
    return step.rule == "Reduce";
  if (std::holds_alternative<WalkParameters>(step.parameters))
    return step.rule == "Walk";
  if (std::holds_alternative<ReorderParameters>(step.parameters))
    return step.rule == "Reorder";
  if (std::holds_alternative<ParityModelParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<OnehotModelParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<BalancedSignatureModelParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<FermatModelParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<LookupCircuitModelParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<SimonModelParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<StconnProofParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<SignedCardinalityProofParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<ExactCoverModelParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<IndependentSetModelParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<RamseyStartupParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<GraphLabelingParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<AffineModelParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<SubstitutionRedundancyParameters>(step.parameters))
    return step.rule == "SubstitutionRedundancy";
  return false;
}

uint64_t saturated_add(uint64_t a, uint64_t b) {
  const uint64_t maximum = std::numeric_limits<uint64_t>::max();
  return b > maximum - a ? maximum : a + b;
}

WorkLimit remaining(const OnlineInvocation &step, uint64_t consumed) {
  if (step.budget.is_unlimited())
    return step.budget;
  return WorkLimit::bounded(consumed < step.budget.amount()
                                ? step.budget.amount() - consumed
                                : 0);
}

WorkUnit action_work_unit(const std::string &rule) {
  if (rule == "BVE")
    return WorkUnit::BveTicks;
  if (rule == "Propagate")
    return WorkUnit::Propagations;
  return WorkUnit::Applications;
}

AffineBasisActionLimits affine_basis_limits(
    const AffineConsequenceParameters &p) {
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

Gf3AffineActionLimits gf3_affine_limits(
    const Gf3AffineConsequenceParameters &p) {
  return {{p.maximum_source_clauses, p.maximum_source_literals,
           p.maximum_domains, p.maximum_equations,
           p.maximum_equation_arity, p.maximum_tuples_per_equation},
          {p.maximum_result_arity, p.maximum_result_tuples,
           p.maximum_additions, p.maximum_addition_literals},
          p.maximum_recognition_work,
          p.maximum_planning_work,
          p.maximum_duplicate_scan_work,
          p.maximum_revalidation_work,
          p.maximum_proof_work,
          p.maximum_proof_additions,
          p.maximum_proof_literal_occurrences,
          p.maximum_arena_words,
          p.maximum_journal_bytes,
          p.maximum_external_extent,
          p.maximum_session_keys,
          p.maximum_session_work};
}

DirectCspSupportActionLimits direct_csp_limits(
    const DirectCspSupportParameters &p) {
  return {p.maximum_source_clauses,
          p.maximum_source_literals,
          p.maximum_recognition_work,
          p.maximum_domain_values,
          p.maximum_direct_clause_literals,
          p.maximum_definition_clause_literals,
          p.maximum_proof_work,
          p.maximum_proof_additions,
          p.maximum_proof_literal_occurrences,
          p.maximum_journal_bytes,
          p.maximum_extent_growth,
          p.maximum_external_extent,
          p.maximum_session_keys,
          p.maximum_session_work,
          p.allow_extension};
}

NaryCspPrefixActionLimits nary_csp_limits(
    const NaryCspPrefixParameters &p) {
  return {p.maximum_source_clauses,
          p.maximum_source_literals,
          p.maximum_recognition_work,
          p.maximum_domains,
          p.maximum_domain_values,
          p.maximum_forbidden_tuples,
          p.maximum_prefix_arity,
          p.maximum_definition_clause_literals,
          p.maximum_proof_work,
          p.maximum_proof_additions,
          p.maximum_proof_literal_occurrences,
          p.maximum_journal_bytes,
          p.maximum_extent_growth,
          p.maximum_external_extent,
          p.maximum_session_keys,
          p.maximum_session_work};
}

NaryCspEqualityLimits nary_csp_equality_limits(
    const NaryCspEqualityParameters &p) {
  return {p.maximum_source_clauses, p.maximum_source_literals,
          p.maximum_recognition_work, p.maximum_domain_values,
          p.maximum_forbidden_tuples, p.maximum_proof_work,
          p.maximum_proof_additions, p.maximum_proof_literal_occurrences,
          p.maximum_journal_bytes, p.maximum_extent_growth,
          p.maximum_external_extent, p.maximum_session_keys};
}

DirectPhpActionLimits direct_php_limits(
    const DirectPhpRefutationParameters &p) {
  return {{p.maximum_snapshot_clauses, p.maximum_snapshot_literals,
           p.maximum_snapshot_work},
          {p.maximum_recognition_clauses, p.maximum_recognition_literals,
           p.maximum_recognition_work, p.maximum_row_candidates,
           p.maximum_row_combinations, p.maximum_mapping_trials,
           p.maximum_holes},
          {p.maximum_proof_additions, p.maximum_proof_deletions,
           p.maximum_proof_literals, p.maximum_fresh_variables,
           p.maximum_proof_events},
          p.maximum_replay_work};
}

Gf3GaussianActionLimits gf3_gaussian_limits(
    const Gf3GaussianConsequenceParameters &p) {
  return {{p.maximum_source_clauses, p.maximum_source_literals,
           p.maximum_domains, p.maximum_equations,
           p.maximum_equation_arity, p.maximum_tuples_per_equation},
          {{p.maximum_bridge_work, p.maximum_bridge_source_clauses,
            p.maximum_bridge_source_literals, p.maximum_bridge_domains,
            p.maximum_bridge_equations, p.maximum_bridge_terms,
            p.maximum_authentication_references,
            p.maximum_references_per_equation,
            {p.maximum_gaussian_work, p.maximum_fill_entries,
             p.maximum_intermediate_width, p.maximum_provenance_terms,
             p.maximum_derivations, p.maximum_retained_plan_bytes}},
           p.maximum_certificate_work, p.maximum_events,
           p.maximum_event_literals, p.maximum_active_clauses,
           p.maximum_retained_source_references, p.maximum_event_width,
           p.maximum_branch_domains, p.maximum_consequences},
          p.maximum_capture_work, p.maximum_revalidation_work,
          p.maximum_proof_additions, p.maximum_proof_literal_occurrences,
          p.maximum_journal_bytes, p.maximum_identity_bytes,
          p.maximum_external_extent, p.maximum_session_keys,
          p.maximum_session_work, p.maximum_session_identity_bytes};
}

Gf3GeneralRowActionLimits gf3_general_row_limits(
    const Gf3GeneralRowConsequenceParameters &p) {
  const auto &g = p.gaussian;
  return {{{g.maximum_source_clauses, g.maximum_source_literals,
            g.maximum_domains, g.maximum_equations,
            g.maximum_equation_arity, g.maximum_tuples_per_equation},
           {{g.maximum_bridge_work, g.maximum_bridge_source_clauses,
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
            p.maximum_row_width},
           g.maximum_capture_work, p.maximum_duplicate_scan_work,
           g.maximum_revalidation_work, g.maximum_proof_additions,
           g.maximum_proof_literal_occurrences, p.maximum_arena_words,
           g.maximum_journal_bytes, g.maximum_identity_bytes,
           g.maximum_external_extent},
          g.maximum_session_keys, g.maximum_session_work,
          g.maximum_session_identity_bytes};
}

TseitinCycleBreakingLimits tseitin_cycle_limits(
    const TseitinCycleBreakingParameters &p) {
  TseitinCycleBreakingLimits result;
  result.snapshot = {p.maximum_snapshot_clauses,
                     p.maximum_snapshot_literals,
                     p.maximum_snapshot_work,
                     p.maximum_snapshot_variables, nullptr, nullptr};
  result.discovery = {p.maximum_discovery_work,
                      p.maximum_discovery_clauses,
                      p.maximum_discovery_groups,
                      4, nullptr, nullptr,
                      p.maximum_components,
                      p.maximum_discovery_retained_bytes};
  result.admission = {p.maximum_admission_work, nullptr, nullptr};
  result.planning = {p.maximum_planning_work, p.maximum_witness_length,
                     p.maximum_planning_additions};
  result.transaction = {p.maximum_transaction_work,
                        p.maximum_transaction_additions,
                        p.maximum_transaction_deletions,
                        p.maximum_transaction_helpers,
                        p.maximum_transaction_witness_literals,
                        nullptr, nullptr};
  result.maximum_validation_ticks = p.maximum_validation_ticks;
  result.maximum_components = p.maximum_components;
  result.maximum_identity_bytes = p.maximum_identity_bytes;
  result.maximum_total_identity_bytes = p.maximum_total_identity_bytes;
  result.maximum_identity_work = p.maximum_identity_work;
  result.maximum_journal_bytes = p.maximum_journal_bytes;
  result.maximum_session_keys = p.maximum_session_keys;
  result.maximum_session_work = p.maximum_session_work;
  result.maximum_session_identity_bytes = p.maximum_session_identity_bytes;
  return result;
}

void record_gf3_affine_feedback(Kernel &k,
                                const Gf3AffineActionResult &result,
                                size_t keys_before) {
  ++k.stats.gf3_affine_attempts;
  switch (result.status) {
  case Gf3AffineActionStatus::Applied: ++k.stats.gf3_affine_applied; break;
  case Gf3AffineActionStatus::NoStructure:
    ++k.stats.gf3_affine_no_structure; break;
  case Gf3AffineActionStatus::NoConsequence:
    ++k.stats.gf3_affine_no_consequence; break;
  case Gf3AffineActionStatus::AlreadyPublished:
    ++k.stats.gf3_affine_already_published; break;
  case Gf3AffineActionStatus::Invalid: ++k.stats.gf3_affine_invalid; break;
  case Gf3AffineActionStatus::Inapplicable:
    ++k.stats.gf3_affine_inapplicable; break;
  case Gf3AffineActionStatus::ResourceExhausted:
    ++k.stats.gf3_affine_resource_exhausted; break;
  case Gf3AffineActionStatus::Interrupted:
    ++k.stats.gf3_affine_interrupted; break;
  case Gf3AffineActionStatus::UnsupportedProof:
    ++k.stats.gf3_affine_unsupported_proof; break;
  case Gf3AffineActionStatus::Stale: ++k.stats.gf3_affine_stale; break;
  }
  const auto &e = result.effort;
  k.stats.gf3_affine_source_clauses += e.source_clauses;
  k.stats.gf3_affine_source_literals += e.source_literals;
  k.stats.gf3_affine_equations += e.equations;
  k.stats.gf3_affine_owner_capture_work += e.owner_capture_work;
  k.stats.gf3_affine_recognition_work += e.recognition_work;
  k.stats.gf3_affine_planning_work += e.planning_work;
  k.stats.gf3_affine_duplicate_scan_work += e.duplicate_scan_work;
  k.stats.gf3_affine_revalidation_work += e.revalidation_work;
  k.stats.gf3_affine_proof_work += e.proof_work;
  k.stats.gf3_affine_planned_additions += e.planned_additions;
  k.stats.gf3_affine_duplicate_additions += e.duplicate_additions;
  k.stats.gf3_affine_emitted_additions += e.emitted_additions;
  k.stats.gf3_affine_proof_literal_occurrences += e.proof_literal_occurrences;
  k.stats.gf3_affine_arena_words += e.arena_words;
  k.stats.gf3_affine_journal_work += e.journal_work;
  k.stats.gf3_affine_journal_bytes += e.journal_bytes;
  k.stats.gf3_affine_session_work += e.session_work;
  k.stats.gf3_affine_session_commits += result.committed_keys > keys_before;
  k.stats.gf3_affine_candidate_variables += result.semantic_candidates.size();
}

void record_direct_csp_feedback(Kernel &k,
                                const DirectCspSupportActionResult &result,
                                size_t keys_before, size_t owners_before) {
  ++k.stats.direct_csp_support_attempts;
  switch (result.status) {
  case DirectCspSupportActionStatus::Applied:
    ++k.stats.direct_csp_support_applied; break;
  case DirectCspSupportActionStatus::NoStructure:
    ++k.stats.direct_csp_support_no_structure; break;
  case DirectCspSupportActionStatus::NoConsequence:
    ++k.stats.direct_csp_support_no_consequence; break;
  case DirectCspSupportActionStatus::AlreadyPublished:
    ++k.stats.direct_csp_support_already_published; break;
  case DirectCspSupportActionStatus::Invalid:
    ++k.stats.direct_csp_support_invalid; break;
  case DirectCspSupportActionStatus::Inapplicable:
    ++k.stats.direct_csp_support_inapplicable; break;
  case DirectCspSupportActionStatus::ResourceExhausted:
    ++k.stats.direct_csp_support_resource_exhausted; break;
  case DirectCspSupportActionStatus::Interrupted:
    ++k.stats.direct_csp_support_interrupted; break;
  case DirectCspSupportActionStatus::UnsupportedProof:
    ++k.stats.direct_csp_support_unsupported_proof; break;
  case DirectCspSupportActionStatus::Stale:
    ++k.stats.direct_csp_support_stale; break;
  }
  const auto &e = result.effort;
  k.stats.direct_csp_support_source_clauses += e.source_clauses;
  k.stats.direct_csp_support_source_literals += e.source_literals;
  k.stats.direct_csp_support_owner_capture_work += e.owner_capture_work;
  k.stats.direct_csp_support_recognition_work += e.recognition_work;
  k.stats.direct_csp_support_source_scan_work += e.source_scan_work;
  k.stats.direct_csp_support_planning_work += e.planning_work;
  k.stats.direct_csp_support_revalidation_work += e.revalidation_work;
  k.stats.direct_csp_support_proof_work += e.proof_work;
  k.stats.direct_csp_support_planned_additions += e.planned_additions;
  k.stats.direct_csp_support_emitted_additions += e.emitted_additions;
  k.stats.direct_csp_support_proof_literal_occurrences +=
      e.proof_literal_occurrences;
  k.stats.direct_csp_support_journal_work += e.journal_work;
  k.stats.direct_csp_support_journal_bytes += e.journal_bytes;
  k.stats.direct_csp_support_extent_growth += e.extent_growth;
  k.stats.direct_csp_support_session_work += e.session_work;
  k.stats.direct_csp_support_session_commits +=
      result.committed_keys > keys_before;
  k.stats.direct_csp_support_owner_commits +=
      result.retained_owners > owners_before;
  k.stats.direct_csp_support_candidate_variables +=
      result.semantic_variables.size();
  k.stats.direct_csp_support_introduced_variables +=
      std::count_if(result.semantic_variables.begin(),
                    result.semantic_variables.end(), [](const auto &variable) {
                      return variable.role ==
                             SemanticVariableRole::IntroducedTheory;
                    });
}

void record_nary_csp_feedback(Kernel &k,
                              const NaryCspPrefixActionResult &result,
                              size_t keys_before, size_t owners_before) {
  ++k.stats.nary_csp_prefix_attempts;
  switch (result.status) {
  case NaryCspPrefixActionStatus::Applied:
    ++k.stats.nary_csp_prefix_applied; break;
  case NaryCspPrefixActionStatus::NoStructure:
    ++k.stats.nary_csp_prefix_no_structure; break;
  case NaryCspPrefixActionStatus::NoConsequence:
    ++k.stats.nary_csp_prefix_no_consequence; break;
  case NaryCspPrefixActionStatus::AlreadyPublished:
    ++k.stats.nary_csp_prefix_already_published; break;
  case NaryCspPrefixActionStatus::Invalid:
    ++k.stats.nary_csp_prefix_invalid; break;
  case NaryCspPrefixActionStatus::Inapplicable:
    ++k.stats.nary_csp_prefix_inapplicable; break;
  case NaryCspPrefixActionStatus::ResourceExhausted:
    ++k.stats.nary_csp_prefix_resource_exhausted; break;
  case NaryCspPrefixActionStatus::Interrupted:
    ++k.stats.nary_csp_prefix_interrupted; break;
  case NaryCspPrefixActionStatus::UnsupportedProof:
    ++k.stats.nary_csp_prefix_unsupported_proof; break;
  case NaryCspPrefixActionStatus::Stale:
    ++k.stats.nary_csp_prefix_stale; break;
  }
  const auto &e = result.effort;
  k.stats.nary_csp_prefix_source_clauses += e.source_clauses;
  k.stats.nary_csp_prefix_source_literals += e.source_literals;
  k.stats.nary_csp_prefix_owner_capture_work += e.owner_capture_work;
  k.stats.nary_csp_prefix_recognition_work += e.recognition_work;
  k.stats.nary_csp_prefix_source_scan_work += e.source_scan_work;
  k.stats.nary_csp_prefix_planning_work += e.planning_work;
  k.stats.nary_csp_prefix_revalidation_work += e.revalidation_work;
  k.stats.nary_csp_prefix_proof_work += e.proof_work;
  k.stats.nary_csp_prefix_planned_additions += e.planned_additions;
  k.stats.nary_csp_prefix_emitted_additions += e.emitted_additions;
  k.stats.nary_csp_prefix_proof_literal_occurrences +=
      e.proof_literal_occurrences;
  k.stats.nary_csp_prefix_journal_work += e.journal_work;
  k.stats.nary_csp_prefix_journal_bytes += e.journal_bytes;
  k.stats.nary_csp_prefix_extent_growth += e.extent_growth;
  k.stats.nary_csp_prefix_session_work += e.session_work;
  k.stats.nary_csp_prefix_session_commits +=
      result.committed_keys > keys_before;
  k.stats.nary_csp_prefix_owner_commits +=
      result.retained_owners > owners_before;
  k.stats.nary_csp_prefix_candidate_variables +=
      result.semantic_variables.size();
  k.stats.nary_csp_prefix_introduced_variables +=
      std::count_if(result.semantic_variables.begin(),
                    result.semantic_variables.end(), [](const auto &variable) {
                      return variable.role ==
                             SemanticVariableRole::IntroducedTheory;
                    });
}

void record_nary_csp_equality_feedback(
    Kernel &k, const NaryCspEqualityActionResult &result,
    size_t keys_before) {
  ++k.stats.nary_csp_equality_attempts;
  switch (result.status) {
  case NaryCspEqualityActionStatus::Applied:
    ++k.stats.nary_csp_equality_applied; break;
  case NaryCspEqualityActionStatus::NoStructure:
    ++k.stats.nary_csp_equality_no_structure; break;
  case NaryCspEqualityActionStatus::NoConsequence:
    ++k.stats.nary_csp_equality_no_consequence; break;
  case NaryCspEqualityActionStatus::AlreadyPublished:
    ++k.stats.nary_csp_equality_already_published; break;
  case NaryCspEqualityActionStatus::Invalid:
    ++k.stats.nary_csp_equality_invalid; break;
  case NaryCspEqualityActionStatus::Inapplicable:
    ++k.stats.nary_csp_equality_inapplicable; break;
  case NaryCspEqualityActionStatus::ResourceExhausted:
    ++k.stats.nary_csp_equality_resource_exhausted; break;
  case NaryCspEqualityActionStatus::Interrupted:
    ++k.stats.nary_csp_equality_interrupted; break;
  case NaryCspEqualityActionStatus::UnsupportedProof:
    ++k.stats.nary_csp_equality_unsupported_proof; break;
  case NaryCspEqualityActionStatus::Stale:
    ++k.stats.nary_csp_equality_stale; break;
  }
  const auto &e = result.live;
  k.stats.nary_csp_equality_source_clauses += e.source_clauses;
  k.stats.nary_csp_equality_source_literals += e.source_literals;
  k.stats.nary_csp_equality_recognition_work += e.recognition_work;
  k.stats.nary_csp_equality_planning_work += e.planning_work;
  k.stats.nary_csp_equality_proof_work += e.proof_work;
  k.stats.nary_csp_equality_planned_additions += e.planned_additions;
  k.stats.nary_csp_equality_emitted_additions += e.emitted_additions;
  k.stats.nary_csp_equality_proof_literal_occurrences +=
      e.proof_literal_occurrences;
  k.stats.nary_csp_equality_journal_bytes += e.journal_bytes;
  k.stats.nary_csp_equality_extent_growth += e.extent_growth;
  k.stats.nary_csp_equality_session_commits +=
      k.nary_csp_equality_session &&
      k.nary_csp_equality_session->keys().size() > keys_before;
  k.stats.nary_csp_equality_candidate_variables +=
      result.semantic_variables.size();
  k.stats.nary_csp_equality_introduced_variables +=
      std::count_if(result.semantic_variables.begin(),
                    result.semantic_variables.end(), [](const auto &variable) {
                      return variable.role ==
                             SemanticVariableRole::IntroducedTheory;
                    });
}

} // namespace

bool validate_online_strategy_program(const OnlineStrategyProgram &program,
                                      std::string *error,
                                      bool allow_native_offer) {
  const auto reject = [error](const char *message) {
    if (error)
      *error = message;
    return false;
  };
  if (!valid_repeat(program.repeat))
    return reject("sequence repeat specification is invalid");
  for (size_t i = 0; i < program.sequence.size(); ++i) {
    const OnlineInvocation &step = program.sequence[i];
    if (!step.id)
      return reject("StepId zero is reserved");
    if (step.rule.empty())
      return reject("rule name is empty");
    for (size_t j = 0; j < i; ++j)
      if (program.sequence[j].id == step.id)
        return reject("StepId is duplicated");
    if (!valid_repeat(step.repeat))
      return reject("step repeat specification is invalid");
    if (!valid_post_application(step.post_application))
      return reject("post-application action is invalid");
    if (step.rule == "DirectPhpRefutation" &&
        step.post_application != PostApplication::None)
      return reject("DirectPhpRefutation is terminal and has no follow-up");
    if (step.rule == "TseitinCycleBreaking" &&
        step.post_application != PostApplication::None)
      return reject(
          "TseitinCycleBreaking requires separately selected propagation");
    if (!valid_rule_work_budget(step.rule_work_budget))
      return reject("rule work budget is invalid");
    if (!valid_setting_override(step.material.touch_filter) ||
        !valid_setting_override(step.material.likely_kept_redundant))
      return reject("rule material setting is invalid");
    if (!parameter_matches(step))
      return reject("parameters do not match rule");
    if (step.rule == "DirectPhpRefutation" &&
        !std::holds_alternative<DirectPhpRefutationParameters>(step.parameters))
      return reject("DirectPhpRefutation requires exact typed limits");
    if (std::holds_alternative<ParityModelParameters>(step.parameters) &&
        (step.granularity != InvocationGranularity::OneInstance ||
         step.instances.size() != 1 ||
         (step.instances[0] != 6 &&
          step.instances[0] != LUCKY_STARTUP_PARITY &&
          step.instances[0] != LUCKY_PARITY_PROOF)))
      return reject("Lucky parity parameters require instance 6, 19, or 23");
    if (std::holds_alternative<OnehotModelParameters>(step.parameters) &&
        (step.granularity != InvocationGranularity::OneInstance ||
         step.instances.size() != 1 ||
         (step.instances[0] != 7 && step.instances[0] != 9 &&
          step.instances[0] != LUCKY_EXACT_ONE_PROOF)))
      return reject("Lucky onehot parameters require instance 7, 9, or 24");
    if (std::holds_alternative<AffineModelParameters>(step.parameters) &&
        (step.granularity != InvocationGranularity::OneInstance ||
         step.instances != std::vector<uint64_t>{8}))
      return reject("Lucky affine-model parameters require instance 8");
    if (std::holds_alternative<BalancedSignatureModelParameters>(
            step.parameters) &&
        (step.granularity != InvocationGranularity::OneInstance ||
         step.instances != std::vector<uint64_t>{10}))
      return reject(
          "Lucky balanced-signature parameters require instance 10");
    if (std::holds_alternative<FermatModelParameters>(step.parameters) &&
        (step.granularity != InvocationGranularity::OneInstance ||
         step.instances != std::vector<uint64_t>{11}))
      return reject("Lucky Fermat parameters require instance 11");
    if (std::holds_alternative<LookupCircuitModelParameters>(step.parameters) &&
        (step.granularity != InvocationGranularity::OneInstance ||
         step.instances != std::vector<uint64_t>{13}))
      return reject("Lucky lookup-circuit parameters require instance 13");
    if (std::holds_alternative<SimonModelParameters>(step.parameters) &&
        (step.granularity != InvocationGranularity::OneInstance ||
         step.instances != std::vector<uint64_t>{14}))
      return reject("Lucky Simon parameters require instance 14");
    if (std::holds_alternative<StconnProofParameters>(step.parameters) &&
        (step.granularity != InvocationGranularity::OneInstance ||
         step.instances != std::vector<uint64_t>{15}))
      return reject("Lucky stconn parameters require instance 15");
    if (std::holds_alternative<SignedCardinalityProofParameters>(
            step.parameters) &&
        (step.granularity != InvocationGranularity::OneInstance ||
         step.instances != std::vector<uint64_t>{16}))
      return reject("Lucky signed-cardinality parameters require instance 16");
    if (std::holds_alternative<ExactCoverModelParameters>(step.parameters) &&
        (step.granularity != InvocationGranularity::OneInstance ||
         step.instances != std::vector<uint64_t>{17}))
      return reject("Lucky exact-cover parameters require instance 17");
    if (std::holds_alternative<IndependentSetModelParameters>(step.parameters) &&
        (step.granularity != InvocationGranularity::OneInstance ||
         step.instances != std::vector<uint64_t>{18}))
      return reject("Lucky independent-set parameters require instance 18");
    if (std::holds_alternative<GraphLabelingParameters>(step.parameters) &&
        (step.granularity != InvocationGranularity::OneInstance ||
         step.instances != std::vector<uint64_t>{LUCKY_GRAPH_LABELING}))
      return reject("Lucky graph-labeling parameters require instance 21");
    if (std::holds_alternative<RamseyStartupParameters>(step.parameters) &&
        (step.granularity != InvocationGranularity::OneInstance ||
         step.instances !=
             std::vector<uint64_t>{LUCKY_STARTUP_RAMSEY}))
      return reject("Lucky Ramsey parameters require instance 22");
    if (const auto *p = std::get_if<AnalyzeParameters>(&step.parameters);
        p && !p->valid())
      return reject("Analyze parameters are invalid");
    if (const auto *p = std::get_if<BveParameters>(&step.parameters);
        p && !p->valid())
      return reject("BVE parameters are invalid");
    if (const auto *p =
            std::get_if<SubsumeForwardParameters>(&step.parameters);
        p && !p->valid())
      return reject("C1 parameters are invalid");
    if (const auto *p = std::get_if<StrengthenParameters>(&step.parameters);
        p && !p->valid())
      return reject("C2 parameters are invalid");
    if (const auto *p = std::get_if<ParityConsequenceParameters>(&step.parameters);
        p && !p->valid())
      return reject("ParityConsequence parameters are invalid");
    if (const auto *p = std::get_if<AffineConsequenceParameters>(&step.parameters);
        p && !p->valid())
      return reject("AffineConsequence parameters are invalid");
    if (const auto *p =
            std::get_if<ExactCoverExtensionParameters>(&step.parameters);
        p && !p->valid())
      return reject("ExactCoverExtension parameters are invalid");
    if (const auto *p =
            std::get_if<IndependentSetCoverParameters>(&step.parameters);
        p && !p->valid())
      return reject("IndependentSetCover parameters are invalid");
    if (const auto *p =
            std::get_if<Gf3AffineConsequenceParameters>(&step.parameters);
        p && !p->valid())
      return reject("Gf3AffineConsequence parameters are invalid");
    if (const auto *p =
            std::get_if<DirectCspSupportParameters>(&step.parameters);
        p && !p->valid())
      return reject("DirectCspSupport parameters are invalid");
    if (const auto *p =
            std::get_if<NaryCspPrefixParameters>(&step.parameters);
        p && !p->valid())
      return reject("NaryCspPrefix parameters are invalid");
    if (const auto *p =
            std::get_if<NaryCspEqualityParameters>(&step.parameters);
        p && !p->valid())
      return reject("NaryCspEquality parameters are invalid");
    if (const auto *p =
            std::get_if<DirectPhpRefutationParameters>(&step.parameters);
        p && !p->valid())
      return reject("DirectPhpRefutation parameters are invalid");
    if (const auto *p =
            std::get_if<Gf3GaussianConsequenceParameters>(&step.parameters);
        p && !p->valid())
      return reject("Gf3GaussianConsequence parameters are invalid");
    if (const auto *p =
            std::get_if<Gf3GeneralRowConsequenceParameters>(&step.parameters);
        p && !p->valid())
      return reject("Gf3GeneralRowConsequence parameters are invalid");
    if (const auto *p =
            std::get_if<TseitinCycleBreakingParameters>(&step.parameters);
        p && !p->valid())
      return reject("TseitinCycleBreaking parameters are invalid");
    if (const auto *p =
            std::get_if<SemanticVariableAdviceParameters>(&step.parameters);
        p && !p->valid())
      return reject("SemanticVariableAdvice parameters are invalid");
    if (const auto *p = std::get_if<CongruenceParameters>(&step.parameters);
        p && !p->valid())
      return reject("Congruence parameters are invalid");
    if (const auto *p = std::get_if<InstantiateParameters>(&step.parameters);
        p && !p->valid())
      return reject("Instantiate parameters are invalid");
    if (const auto *p = std::get_if<BackboneParameters>(&step.parameters);
        p && !p->valid())
      return reject("Backbone parameters are invalid");
    if (const auto *p = std::get_if<SweepParameters>(&step.parameters);
        p && !p->valid())
      return reject("Sweep parameters are invalid");
    if (const auto *p = std::get_if<VivifyParameters>(&step.parameters);
        p && !p->valid())
      return reject("Vivify parameters are invalid");
    if (const auto *p = std::get_if<BceParameters>(&step.parameters);
        p && !p->valid())
      return reject("BCE parameters are invalid");
    if (const auto *p = std::get_if<TernaryParameters>(&step.parameters);
        p && !p->valid())
      return reject("ternary parameters are invalid");
    if (const auto *p = std::get_if<FactorParameters>(&step.parameters);
        p && !p->valid())
      return reject("Factor parameters are invalid");
    if (const auto *p =
            std::get_if<TransitiveReduceParameters>(&step.parameters);
        p && !p->valid())
      return reject("transitive parameters are invalid");
    if (const auto *p = std::get_if<SubstituteParameters>(&step.parameters);
        p && !p->valid())
      return reject("Substitute parameters are invalid");
    if (const auto *p = std::get_if<ReduceParameters>(&step.parameters);
        p && !p->valid())
      return reject("Reduce parameters are invalid");
    if (const auto *p = std::get_if<WalkParameters>(&step.parameters);
        p && !p->valid())
      return reject("Walk parameters are invalid");
    if (const auto *p = std::get_if<ReorderParameters>(&step.parameters);
        p && !p->valid())
      return reject("Reorder parameters are invalid");
    if (const auto *p =
            std::get_if<ParityModelParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky parity-model parameters are invalid");
    if (const auto *p =
            std::get_if<OnehotModelParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky onehot-model parameters are invalid");
    if (const auto *p =
            std::get_if<BalancedSignatureModelParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky balanced-signature-model parameters are invalid");
    if (const auto *p = std::get_if<FermatModelParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky fermat-model parameters are invalid");
    if (const auto *p =
            std::get_if<LookupCircuitModelParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky lookup-circuit-model parameters are invalid");
    if (const auto *p = std::get_if<SimonModelParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky simon-model parameters are invalid");
    if (const auto *p = std::get_if<StconnProofParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky stconn-proof parameters are invalid");
    if (const auto *p =
            std::get_if<SignedCardinalityProofParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky signed-cardinality-proof parameters are invalid");
    if (const auto *p = std::get_if<ExactCoverModelParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky exact-cover-model parameters are invalid");
    if (const auto *p =
            std::get_if<IndependentSetModelParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky independent-set-model parameters are invalid");
    if (const auto *p = std::get_if<RamseyStartupParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky ramsey-startup parameters are invalid");
    if (const auto *p = std::get_if<GraphLabelingParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky graph-labeling parameters are invalid");
    if (const auto *p = std::get_if<AffineModelParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky affine-model parameters are invalid");
    if (const auto *p = std::get_if<SubstitutionRedundancyParameters>(
            &step.parameters);
        p && !p->valid())
      return reject("SR parameters are invalid");
    if (!step.factor_grids.empty() && step.rule != "Factor")
      return reject("exact Factor grids are attached to a different rule");
    if (!step.factor_grids.empty() && !step.instances.empty())
      return reject("scalar and exact-grid selections cannot be mixed");
    if (!step.substitution_redundancy_instances.empty() &&
        step.rule != "SubstitutionRedundancy")
      return reject("SR instances are attached to a different rule");
    if (!step.condition_instances.empty() && step.rule != "Condition")
      return reject("Condition instances are attached to a different rule");
    if (!step.independent_set_cover_instances.empty() &&
        step.rule != "IndependentSetCover")
      return reject("independent-set covers are attached to a different rule");
    if (!step.gf3_affine_instances.empty() &&
        step.rule != "Gf3AffineConsequence")
      return reject("GF3 equation pairs are attached to a different rule");
    if (!step.affine_basis_instances.empty() &&
        step.rule != "AffineConsequence")
      return reject("GF2 affine rows are attached to a different rule");
    if (!step.direct_csp_support_instances.empty() &&
        step.rule != "DirectCspSupport")
      return reject("DirectCSP supports are attached to a different rule");
    if (!step.nary_csp_prefix_instances.empty() &&
        step.rule != "NaryCspPrefix")
      return reject("n-ary CSP prefixes are attached to a different rule");
    if (!step.nary_csp_equality_instances.empty() &&
        step.rule != "NaryCspEquality")
      return reject("n-ary CSP equalities are attached to a different rule");
    if (!step.gf3_gaussian_instances.empty() &&
        step.rule != "Gf3GaussianConsequence")
      return reject("GF3 Gaussian identities are attached to a different rule");
    if (!step.gf3_general_row_instances.empty() &&
        step.rule != "Gf3GeneralRowConsequence")
      return reject("GF3 general-row selections are attached to a different rule");
    if (!step.tseitin_cycle_instances.empty() &&
        step.rule != "TseitinCycleBreaking")
      return reject("Tseitin cycle identities are attached to a different rule");
    if (step.rule == "Gf3AffineConsequence" &&
        step.granularity != InvocationGranularity::NativeProducerOffer &&
        (step.gf3_affine_instances.empty() || !step.instances.empty()))
      return reject("GF3 affine action requires typed equation-pair payloads");
    if (!step.affine_basis_instances.empty()) {
      const auto *p = std::get_if<AffineConsequenceParameters>(&step.parameters);
      if (!p || p->semantics != AffineConsequenceSemantics::CertifiedBasisV2)
        return reject("GF2 affine row requires certified-basis parameters");
      if (step.affine_basis_instances.size() > p->maximum_session_keys)
        return reject("GF2 affine row collection exceeds session limits");
      const auto limits = affine_basis_limits(*p);
      for (const auto &selection : step.affine_basis_instances)
        if (!affine_basis_row_selection_within_limits(selection, limits))
          return reject("GF2 affine row is malformed or exceeds limits");
    }
    if (step.rule == "DirectCspSupport" &&
        step.granularity != InvocationGranularity::NativeProducerOffer &&
        (step.direct_csp_support_instances.empty() || !step.instances.empty()))
      return reject("DirectCSP action requires typed support payloads");
    if (step.rule == "NaryCspPrefix" &&
        step.granularity != InvocationGranularity::NativeProducerOffer &&
        (step.nary_csp_prefix_instances.empty() || !step.instances.empty()))
      return reject("n-ary CSP action requires typed prefix payloads");
    if (step.rule == "NaryCspEquality" &&
        step.granularity != InvocationGranularity::NativeProducerOffer &&
        (step.nary_csp_equality_instances.empty() || !step.instances.empty()))
      return reject("n-ary CSP equality requires typed payloads");
    if (step.rule == "Gf3GaussianConsequence" &&
        step.granularity != InvocationGranularity::NativeProducerOffer &&
        (step.gf3_gaussian_instances.size() != 1 || !step.instances.empty() ||
         step.granularity != InvocationGranularity::OneInstance))
      return reject("GF3 Gaussian action requires exact identity payloads");
    if (step.rule == "Gf3GeneralRowConsequence" &&
        step.granularity != InvocationGranularity::NativeProducerOffer &&
        (step.gf3_general_row_instances.size() != 1 ||
         !step.instances.empty() ||
         step.granularity != InvocationGranularity::OneInstance ||
         step.post_application != PostApplication::Propagate))
      return reject("GF3 general-row action requires one exact payload and Propagate follow-up");
    if (step.rule == "TseitinCycleBreaking" &&
        step.granularity != InvocationGranularity::NativeProducerOffer &&
        (step.tseitin_cycle_instances.size() != 1 || !step.instances.empty() ||
         step.granularity != InvocationGranularity::OneInstance))
      return reject("Tseitin cycle action requires one exact identity payload");
    for (const std::string &identity : step.gf3_gaussian_instances)
      if (identity.size() <= 5 || identity.rfind("g3b1:", 0) ||
          (identity.size() - 5) % 2 ||
          !std::all_of(identity.begin() + 5, identity.end(), [](char ch) {
            return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
          }))
        return reject("GF3 Gaussian identity is malformed");
    if (!step.gf3_gaussian_instances.empty()) {
      const auto *p =
          std::get_if<Gf3GaussianConsequenceParameters>(&step.parameters);
      if (!p) return reject("GF3 Gaussian identity has no typed limits");
      uint64_t bytes = 0;
      for (const auto &identity : step.gf3_gaussian_instances) {
        if (identity.size() > p->maximum_identity_bytes ||
            identity.size() > p->maximum_session_identity_bytes ||
            bytes > p->maximum_session_identity_bytes - identity.size())
          return reject("GF3 Gaussian identity exceeds byte limits");
        bytes += identity.size();
      }
    }
    if (!step.gf3_general_row_instances.empty()) {
      const auto *p =
          std::get_if<Gf3GeneralRowConsequenceParameters>(&step.parameters);
      if (!p) return reject("GF3 general-row selection has no typed limits");
      const auto limits = gf3_general_row_limits(*p);
      for (const auto &selection : step.gf3_general_row_instances)
        if (!gf3_general_row_action_selection_valid(selection) ||
            selection.identity.size() >
                limits.live.maximum_identity_bytes ||
            selection.identity.size() >
                limits.maximum_session_identity_bytes)
          return reject("GF3 general-row selection is malformed or exceeds limits");
    }
    for (const std::string &identity : step.tseitin_cycle_instances) {
      const size_t split = identity.find(':', 5);
      if (identity.size() <= 7 || identity.rfind("tcx2:", 0) ||
          split == std::string::npos || split == 5 ||
          split + 1 == identity.size() ||
          identity.find(':', split + 1) != std::string::npos ||
          (split - 5) % 2 || (identity.size() - split - 1) % 2 ||
          !std::all_of(identity.begin() + 5, identity.end(), [](char ch) {
            return ch == ':' || (ch >= '0' && ch <= '9') ||
                   (ch >= 'a' && ch <= 'f');
          }))
        return reject("Tseitin cycle identity is malformed");
    }
    if (!step.tseitin_cycle_instances.empty()) {
      const auto *p =
          std::get_if<TseitinCycleBreakingParameters>(&step.parameters);
      if (!p) return reject("Tseitin cycle identity has no typed limits");
      if (step.tseitin_cycle_instances.front().size() >
          p->maximum_identity_bytes)
        return reject("Tseitin cycle identity exceeds byte limits");
    }
    for (const auto &cover : step.independent_set_cover_instances)
      if (cover.size() < 2 || !cover.front() ||
          !std::is_sorted(cover.begin(), cover.end()) ||
          std::adjacent_find(cover.begin(), cover.end()) != cover.end() ||
          cover.back() > static_cast<uint32_t>(std::numeric_limits<int>::max()))
        return reject("independent-set cover is not a sorted positive external set");
    for (const Gf3AffineInstance &instance : step.gf3_affine_instances)
      if (instance.first_equation >= instance.second_equation)
        return reject("GF3 equation pair is not canonical");
    const auto direct_literal_variable = [](int32_t literal) {
      return static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
    };
    const auto direct_literal_less = [&](int32_t first, int32_t second) {
      const uint32_t first_variable = direct_literal_variable(first);
      const uint32_t second_variable = direct_literal_variable(second);
      return first_variable < second_variable ||
             (first_variable == second_variable && first < second);
    };
    for (const DirectCspSupportActionSelection &instance :
         step.direct_csp_support_instances) {
      if (instance.left_values.size() < 2 || instance.right_values.size() < 2 ||
          instance.selected_left_value >= instance.left_values.size())
        return reject("DirectCSP support selection is incomplete");
      std::vector<uint32_t> variables;
      variables.reserve(instance.left_values.size() +
                        instance.right_values.size());
      const auto append_domain = [&](const std::vector<int32_t> &domain) {
        for (size_t index = 0; index < domain.size(); ++index) {
          const int32_t literal = domain[index];
          if (!literal || literal == std::numeric_limits<int32_t>::min() ||
              (index && !direct_literal_less(domain[index - 1], literal)))
            return false;
          variables.push_back(direct_literal_variable(literal));
        }
        return true;
      };
      if (!append_domain(instance.left_values) ||
          !append_domain(instance.right_values))
        return reject("DirectCSP domain is not canonically ordered");
      std::sort(variables.begin(), variables.end());
      if (std::adjacent_find(variables.begin(), variables.end()) !=
          variables.end())
        return reject("DirectCSP domains repeat a variable");
    }
    if (!step.nary_csp_prefix_instances.empty()) {
      const auto *p = std::get_if<NaryCspPrefixParameters>(&step.parameters);
      if (!p) return reject("n-ary CSP prefix has no typed limits");
      const NaryCspPrefixActionLimits limits = nary_csp_limits(*p);
      for (const auto &instance : step.nary_csp_prefix_instances)
        if (!nary_csp_prefix_action_selection_within_limits(instance, limits))
          return reject("n-ary CSP prefix is malformed or exceeds limits");
    }
    if (!step.nary_csp_equality_instances.empty()) {
      const auto *p = std::get_if<NaryCspEqualityParameters>(&step.parameters);
      if (!p) return reject("n-ary CSP equality has no typed limits");
      const NaryCspEqualityLimits limits = nary_csp_equality_limits(*p);
      for (const auto &instance : step.nary_csp_equality_instances)
        if (!nary_csp_equality_action_selection_within_limits(instance, limits))
          return reject("n-ary CSP equality is malformed or exceeds limits");
    }
    // These are external Lit encodings. UINT32_MAX has no DIMACS spelling:
    // externalize() would overflow its signed result before a live-state
    // check, even if the malformed tuple came only from replay text.
    const auto representable = [](const std::vector<Lit> &literals) {
      return std::all_of(literals.begin(), literals.end(), [](Lit literal) {
        return var_of(literal) <
               static_cast<Var>(std::numeric_limits<int>::max());
      });
    };
    for (const ConditionInstance &instance : step.condition_instances) {
      if (!representable(instance.clause) ||
          !representable(instance.witness))
        return reject("Condition instance has an invalid external literal");
    }
    const unsigned structured_payload_kinds =
        !step.factor_grids.empty() +
        !step.substitution_redundancy_instances.empty() +
        !step.condition_instances.empty() +
        !step.independent_set_cover_instances.empty() +
        !step.affine_basis_instances.empty() +
        !step.gf3_affine_instances.empty() +
        !step.direct_csp_support_instances.empty() +
        !step.nary_csp_prefix_instances.empty() +
        !step.nary_csp_equality_instances.empty() +
        !step.gf3_gaussian_instances.empty() +
        !step.gf3_general_row_instances.empty() +
        !step.tseitin_cycle_instances.empty();
    if (structured_payload_kinds && !step.instances.empty())
      return reject("scalar and structured selections cannot be mixed");
    if (structured_payload_kinds > 1)
      return reject("different structured selection kinds cannot be mixed");
    if (step.rule == "Lucky" &&
        std::any_of(step.instances.begin(), step.instances.end(),
                    lucky_retired_constructor_instance))
      return reject("Lucky constructor instance is retired");
    const size_t structured_size = !step.factor_grids.empty()
                                       ? step.factor_grids.size()
                                       : !step.substitution_redundancy_instances.empty()
                                             ? step.substitution_redundancy_instances.size()
                                             : !step.condition_instances.empty()
                                                   ? step.condition_instances.size()
                                                   : !step.independent_set_cover_instances.empty()
                                                         ? step.independent_set_cover_instances.size()
                                                         : !step.affine_basis_instances.empty()
                                                               ? step.affine_basis_instances.size()
                                                         : !step.gf3_affine_instances.empty()
                                                               ? step.gf3_affine_instances.size()
                                                               : !step.direct_csp_support_instances.empty()
                                                                     ? step.direct_csp_support_instances.size()
                                                                     : !step.nary_csp_prefix_instances.empty()
                                                                           ? step.nary_csp_prefix_instances.size()
                                                                     : !step.nary_csp_equality_instances.empty()
                                                                           ? step.nary_csp_equality_instances.size()
                                                                     : !step.gf3_gaussian_instances.empty()
                                                                           ? step.gf3_gaussian_instances.size()
                                                                     : !step.gf3_general_row_instances.empty()
                                                                           ? step.gf3_general_row_instances.size()
                                                                           : step.tseitin_cycle_instances.size();
    switch (step.granularity) {
    case InvocationGranularity::OneInstance:
      if ((structured_payload_kinds && structured_size != 1) ||
          (!structured_payload_kinds && step.instances.size() != 1))
        return reject("one-instance selection does not contain one payload");
      if (!structured_payload_kinds &&
          step.instances.front() == INSTANCE_CANONICAL)
        return reject("explicit selection contains the canonical whole-rule payload");
      break;
    case InvocationGranularity::SelectedBatch:
      if (step.instances.empty() && !structured_payload_kinds)
        return reject("selected batch is empty");
      if (std::find(step.instances.begin(), step.instances.end(),
                    INSTANCE_CANONICAL) != step.instances.end())
        return reject("explicit selection contains the canonical whole-rule payload");
      break;
    case InvocationGranularity::CompleteCurrentPass:
      if (!step.affine_basis_instances.empty() ||
          step.rule == "Gf3AffineConsequence" ||
          step.rule == "DirectCspSupport" ||
          step.rule == "NaryCspPrefix" ||
          step.rule == "NaryCspEquality" ||
          step.rule == "Gf3GaussianConsequence" ||
          step.rule == "Gf3GeneralRowConsequence" ||
          step.rule == "TseitinCycleBreaking")
        return reject("structured theory action requires an explicit payload");
      if (!step.instances.empty() || structured_payload_kinds)
        return reject("complete pass carries explicit payloads");
      break;
    case InvocationGranularity::CanonicalApplication:
      if (!step.affine_basis_instances.empty() ||
          step.rule == "Gf3AffineConsequence" ||
          step.rule == "DirectCspSupport" ||
          step.rule == "NaryCspPrefix" ||
          step.rule == "NaryCspEquality" ||
          step.rule == "Gf3GaussianConsequence" ||
          step.rule == "Gf3GeneralRowConsequence" ||
          step.rule == "TseitinCycleBreaking")
        return reject("structured theory action has no canonical payload");
      if (!step.instances.empty() || structured_payload_kinds)
        return reject("canonical application carries explicit payloads");
      break;
    case InvocationGranularity::NativeProducerOffer:
      if (!allow_native_offer)
        return reject("native producer offer is controller-prefix-only");
      if (!step.instances.empty() || structured_payload_kinds)
        return reject("native producer offer carries an explicit payload");
      if (step.rule != "AffineConsequence" &&
          step.rule != "Gf3AffineConsequence" &&
          step.rule != "DirectCspSupport" &&
          step.rule != "NaryCspPrefix" &&
          step.rule != "NaryCspEquality" &&
          step.rule != "Gf3GaussianConsequence" &&
          step.rule != "Gf3GeneralRowConsequence" &&
          step.rule != "TseitinCycleBreaking")
        return reject("action family has no native structured producer");
      break;
    default:
      return reject("granularity is invalid");
    }
    if (step.work_unit != action_work_unit(step.rule))
      return reject("action budget has the wrong currency");
  }
  if (error)
    error->clear();
  return true;
}

InvocationStatus enumerate_complete_instances(Kernel &k,
                                              const RuleVTable &rule,
                                              std::vector<uint64_t> &out,
                                              volatile sig_atomic_t *stop) {
  out.clear();
  if ((stop && *stop) || (k.stop && *k.stop))
    return InvocationStatus::Interrupted;
  const auto enumerate =
      rule.semantic_instances ? rule.semantic_instances : rule.instances;
  if (!enumerate) {
    out.push_back(INSTANCE_CANONICAL);
    return InvocationStatus::Completed;
  }
  uint32_t cap = 64;
  for (;;) {
    if ((stop && *stop) || (k.stop && *k.stop)) {
      out.clear();
      return InvocationStatus::Interrupted;
    }
    out.resize(cap);
    const uint32_t count = enumerate(k, out.data(), cap);
    if ((stop && *stop) || (k.stop && *k.stop)) {
      out.clear();
      return InvocationStatus::Interrupted;
    }
    if (count < cap) {
      out.resize(count);
      // Some frozen v1 surfaces rank the whole-rule sentinel beside finer
      // instances. A complete semantic pass must not run that whole closure
      // and then repeat each constituent action. Preserve it only when it is
      // the rule's sole available granularity.
      if (out.size() > 1)
        out.erase(std::remove(out.begin(), out.end(), INSTANCE_CANONICAL),
                  out.end());
      return InvocationStatus::Completed;
    }
    if (cap > std::numeric_limits<uint32_t>::max() / 2) {
      out.clear();
      return InvocationStatus::ResourceExhausted;
    }
    cap *= 2;
  }
}

bool OnlineStrategyExecutor::configure(const OnlineStrategyProgram &program,
                                       uint64_t revision,
                                       ReconfigureMode mode,
                                       std::string *error) {
  if (mode != ReconfigureMode::ResetRuntime &&
      mode != ReconfigureMode::RetainMatchingStepBudgets) {
    if (error)
      *error = "reconfiguration mode is invalid";
    return false;
  }
  if (!validate_online_strategy_program(program, error))
    return false;
  for (const OnlineInvocation &step : program.sequence) {
    const int id = tactix_rule_id(step.rule.c_str());
    if (id < 0) {
      if (error)
        *error = "program names an unknown action";
      return false;
    }
  }

  struct RetainedBudget {
    std::string rule;
    WorkUnit unit;
    uint64_t consumed;
  };
  std::unordered_map<StepId, RetainedBudget> retained;
  if (configured_ && mode == ReconfigureMode::RetainMatchingStepBudgets)
    for (size_t i = 0; i < program_.sequence.size(); ++i)
      retained.emplace(
          program_.sequence[i].id,
          RetainedBudget{program_.sequence[i].rule,
                         program_.sequence[i].work_unit,
                         runtime_[i].consumed});

  OnlineStrategyProgram replacement = program;
  std::vector<Runtime> replacement_runtime(replacement.sequence.size());
  for (size_t i = 0; i < replacement.sequence.size(); ++i) {
    const auto found = retained.find(replacement.sequence[i].id);
    if (found != retained.end() &&
        found->second.rule == replacement.sequence[i].rule &&
        found->second.unit == replacement.sequence[i].work_unit)
      replacement_runtime[i].consumed = found->second.consumed;
  }
  program_ = std::move(replacement);
  runtime_ = std::move(replacement_runtime);
  revision_ = revision;
  step_index_ = 0;
  sequence_passes_ = 0;
  sequence_progress_ = false;
  complete_ = program_.sequence.empty();
  configured_ = true;
  if (error)
    error->clear();
  return true;
}

void OnlineStrategyExecutor::reset_runtime() {
  runtime_.assign(program_.sequence.size(), Runtime{});
  step_index_ = 0;
  sequence_passes_ = 0;
  sequence_progress_ = false;
  complete_ = program_.sequence.empty();
}

uint64_t OnlineStrategyExecutor::step_consumed(StepId id) const {
  for (size_t i = 0; i < program_.sequence.size(); ++i)
    if (program_.sequence[i].id == id)
      return runtime_[i].consumed;
  return 0;
}

bool OnlineStrategyExecutor::pending_atomic_scope() const {
  if (!configured_ || complete_ || step_index_ >= runtime_.size())
    return false;
  const auto &runtime = runtime_[step_index_];
  return runtime.cursor < runtime.selection.size() ||
         (runtime.sr_general_mode && !runtime.sr_general_cursor.complete);
}

bool OnlineStrategyExecutor::renew_atomic_dose(WorkLimit allowance,
                                              uint64_t revision) {
  if (!pending_atomic_scope() || program_.sequence.size() != 1 ||
      program_.repeat.mode != RepeatMode::Once ||
      program_.sequence[0].repeat.mode != RepeatMode::Once)
    return false;
  program_.sequence[0].budget = allowance;
  runtime_[0].consumed = 0;
  revision_ = revision;
  return true;
}

bool OnlineStrategyExecutor::prepare_selection(Kernel &k,
                                               const OnlineInvocation &step,
                                               Runtime &runtime,
                                               InvocationResult &out,
                                               volatile sig_atomic_t *stop,
                                               Allowance *sr_discovery_allowance,
                                               DiscoveryFeedbackPolicy discovery_feedback) {
  // An explicit scope remains bound across repeated passes/sequences. Only
  // CompleteCurrentPass asks for fresh enumeration after a representation
  // change; repeating a pass is not permission to reinterpret old payloads.
  if (step.granularity != InvocationGranularity::CompleteCurrentPass &&
      step.granularity != InvocationGranularity::CanonicalApplication &&
      runtime.selection_owner &&
      (runtime.selection_owner != &k ||
       runtime.selection_generation != k.arena_generation)) {
    out.status = InvocationStatus::StaleRequest;
    out.step_id = step.id;
    return false;
  }
  const auto *sr_parameters =
      step.rule == "SubstitutionRedundancy"
          ? std::get_if<SubstitutionRedundancyParameters>(&step.parameters)
          : nullptr;
  const bool general_sr =
      step.granularity == InvocationGranularity::CompleteCurrentPass &&
      sr_parameters &&
      sr_parameters->producer ==
          SubstitutionRedundancyProducer::GeneralComplete;
  const bool continuing_general_sr = runtime.sr_general_mode && general_sr;
  runtime.bve_occurrences.reset();
  runtime.selection.clear();
  runtime.structured_selection.clear();
  runtime.sr_selection.clear();
  runtime.cursor = 0;
  if (!continuing_general_sr)
    runtime.pass_progress = false;
  runtime.selection_truncated = false;
  runtime.selection_discovery_exhausted = false;
  runtime.sr_general_mode = general_sr;
  if (!general_sr)
    runtime.sr_general_cursor = {};
  const RuleVTable &rule = tactix_rules()[tactix_rule_id(step.rule.c_str())];
  if (!rule.guard(k) && !explicit_fixpoint_propagation_service(step, k)) {
    out.status = InvocationStatus::Inapplicable;
    return false;
  }
  if (step.granularity == InvocationGranularity::CanonicalApplication) {
    runtime.selection = {INSTANCE_CANONICAL};
  } else if (step.granularity != InvocationGranularity::CompleteCurrentPass) {
    if (step.factor_grids.empty() &&
        step.substitution_redundancy_instances.empty() &&
        step.condition_instances.empty() &&
        step.independent_set_cover_instances.empty() &&
        step.affine_basis_instances.empty() &&
        step.gf3_affine_instances.empty() &&
        step.direct_csp_support_instances.empty() &&
        step.nary_csp_prefix_instances.empty() &&
        step.nary_csp_equality_instances.empty() &&
        step.gf3_gaussian_instances.empty() &&
        step.gf3_general_row_instances.empty() &&
        step.tseitin_cycle_instances.empty()) {
      runtime.selection = step.instances;
    } else {
      const size_t size = !step.factor_grids.empty()
                              ? step.factor_grids.size()
                              : !step.substitution_redundancy_instances.empty()
                                    ? step.substitution_redundancy_instances.size()
                                    : !step.condition_instances.empty()
                                          ? step.condition_instances.size()
                                          : !step.independent_set_cover_instances.empty()
                                                ? step.independent_set_cover_instances.size()
                                                : !step.affine_basis_instances.empty()
                                                      ? step.affine_basis_instances.size()
                                                : !step.gf3_affine_instances.empty()
                                                      ? step.gf3_affine_instances.size()
                                                      : !step.direct_csp_support_instances.empty()
                                                            ? step.direct_csp_support_instances.size()
                                                            : !step.nary_csp_prefix_instances.empty()
                                                                  ? step.nary_csp_prefix_instances.size()
                                                            : !step.nary_csp_equality_instances.empty()
                                                                  ? step.nary_csp_equality_instances.size()
                                                            : !step.gf3_gaussian_instances.empty()
                                                                  ? step.gf3_gaussian_instances.size()
                                                            : !step.gf3_general_row_instances.empty()
                                                                  ? step.gf3_general_row_instances.size()
                                                                  : step.tseitin_cycle_instances.size();
      runtime.selection.resize(size);
      for (size_t i = 0; i < runtime.selection.size(); ++i)
        runtime.selection[i] = i;
    }
  } else if (step.rule == "Factor") {
    uint64_t maximum = std::numeric_limits<uint64_t>::max();
    if (!step.budget.is_unlimited())
      maximum = remaining(step, runtime.consumed).amount();
    const FactorEnumerationResult enumeration = factor_enumerate_grids(
        k,
        [&](const FactorGrid &grid) {
          runtime.structured_selection.push_back(grid);
          return true;
        },
        FactorEnumerationOptions{maximum, stop});
    if (enumeration.status == FactorEnumerationStatus::Stopped) {
      out.status = InvocationStatus::Interrupted;
      return false;
    }
    runtime.selection_truncated =
        enumeration.status == FactorEnumerationStatus::LimitReached;
    runtime.selection.resize(runtime.structured_selection.size());
    for (size_t i = 0; i < runtime.selection.size(); ++i)
      runtime.selection[i] = i;
  } else if (step.rule == "SubstitutionRedundancy") {
    const auto *configured =
        std::get_if<SubstitutionRedundancyParameters>(&step.parameters);
    SubstitutionRedundancyParameters parameters =
        configured ? *configured : SubstitutionRedundancyParameters{};
    const bool shared_discovery =
        sr_discovery_allowance &&
        parameters.producer != SubstitutionRedundancyProducer::GeneralComplete;
    Allowance unlimited_discovery(WorkUnit::SrDiscoveryTicks,
                                  WorkLimit::unlimited());
    Allowance *discovery_pool = shared_discovery
                                    ? sr_discovery_allowance
                                    : &unlimited_discovery;
    std::span<Allowance *const> discovery_pools(&discovery_pool, 1);
    InvocationBudget discovery_budget(WorkUnit::SrDiscoveryTicks,
                                      WorkLimit::unlimited(),
                                      discovery_pools);
    uint64_t discovery_limit_at_entry = 0;
    uint64_t hard_detection_ticks = 0;
    const bool inline_generator_feedback =
        shared_discovery && discovery_feedback.ticks_per_generator &&
        discovery_feedback.hard_limit > sr_discovery_allowance->consumed();
    const auto graph_entry_reaches_detector = [&] {
      const uint64_t binaries = k.watches.binary_clauses();
      const uint64_t binary_visits =
          binaries > UINT64_MAX / 2 ? UINT64_MAX : 2 * binaries;
      const uint64_t snapshot_lower_bound =
          k.nvars > UINT64_MAX - binary_visits
              ? UINT64_MAX
              : binary_visits + static_cast<uint64_t>(k.nvars);
      // snapshot_formula must visit both watches of every live binary and all
      // variables before graph construction can call the detector.  If a
      // fresh owner-scale window cannot cover even this strict lower bound,
      // granting it can only repeat a partial snapshot and yield no feedback.
      return snapshot_lower_bound <=
             discovery_feedback.ticks_per_generator;
    };
    if (shared_discovery) {
      const BudgetStatus status = discovery_budget.status();
      if (status != BudgetStatus::Ready &&
          status != BudgetStatus::Exhausted) {
        out.status = InvocationStatus::AccountingError;
        out.budget_status = status;
        return false;
      }
      const WorkLimit remaining = discovery_budget.remaining();
      if (!remaining.admits_work()) {
        out.status = InvocationStatus::BudgetExhausted;
        out.budget_status = discovery_budget.status();
        if (parameters.producer ==
                SubstitutionRedundancyProducer::GraphSymmetry &&
            discovery_feedback.ticks_per_generator &&
            discovery_feedback.hard_limit >
                sr_discovery_allowance->consumed() &&
            !runtime.graph_entry_requested &&
            graph_entry_reaches_detector()) {
          runtime.graph_entry_requested = true;
          out.discovery_entry_required = true;
        }
        return false;
      }
      if (!remaining.is_unlimited())
        parameters.maximum_detection_ticks =
            std::min(parameters.maximum_detection_ticks, remaining.amount());
      if (!remaining.is_unlimited() &&
          remaining.amount() <= UINT64_MAX - sr_discovery_allowance->consumed())
        discovery_limit_at_entry =
            sr_discovery_allowance->consumed() + remaining.amount();
      if (inline_generator_feedback)
        hard_detection_ticks =
            discovery_feedback.hard_limit - sr_discovery_allowance->consumed();
    }
    if (parameters.producer ==
        SubstitutionRedundancyProducer::GeneralComplete) {
      SubstitutionRedundancyInstance instance;
      const SubstitutionRedundancyGeneralResult enumeration =
          substitution_redundancy_next_general(
              k, parameters, runtime.sr_general_cursor, instance, stop);
      if (enumeration.status ==
          SubstitutionRedundancyGeneralStatus::Interrupted) {
        out.status = InvocationStatus::Interrupted;
        return false;
      }
      if (enumeration.status ==
              SubstitutionRedundancyGeneralStatus::ResourceExhausted ||
          enumeration.status ==
              SubstitutionRedundancyGeneralStatus::LimitReached) {
        out.status = InvocationStatus::ResourceExhausted;
        return false;
      }
      if (enumeration.status == SubstitutionRedundancyGeneralStatus::Found)
        runtime.sr_selection.push_back(std::move(instance));
    } else if (parameters.producer ==
               SubstitutionRedundancyProducer::Orbitopal) {
      const OrbitopalEnumerationResult enumeration =
          substitution_redundancy_enumerate_orbitopal(
              k, parameters,
              [&](const SubstitutionRedundancyInstance &instance) {
                runtime.sr_selection.push_back(instance);
                return true;
              },
              stop);
      out.budget_status = discovery_budget.charge(
          WorkUnit::SrDiscoveryTicks, enumeration.ticks);
      if (out.budget_status == BudgetStatus::AccountingOverflow) {
        out.status = InvocationStatus::AccountingError;
        return false;
      }
      if (enumeration.status == OrbitopalEnumerationStatus::Interrupted) {
        out.status = InvocationStatus::Interrupted;
        return false;
      }
      runtime.selection_truncated =
          enumeration.status == OrbitopalEnumerationStatus::LimitReached;
      // Orbitopal sub-strategies share one structured detector family.  A
      // bounded empty prefix is not evidence that paying another complete
      // snapshot for the next orientation is useful.  Formula progress still
      // renews the shared allowance.  The producer-boundary opportunity below
      // is reserved for the distinct generic graph detector, matching the
      // owning structured-then-generic schedule.
      runtime.selection_discovery_exhausted = false;
    } else if (parameters.producer ==
               SubstitutionRedundancyProducer::GraphSymmetry) {
      const SymmetryEnumerationResult enumeration =
          substitution_redundancy_enumerate_symmetry(
              k, parameters,
              [&](const SubstitutionRedundancyInstance &instance) {
                runtime.sr_selection.push_back(instance);
                return true;
              },
              stop, nullptr,
              SymmetryEnumerationScheduling{
                  shared_discovery,
                  inline_generator_feedback
                      ? discovery_feedback.ticks_per_generator
                      : 0,
                  hard_detection_ticks});
      if (inline_generator_feedback &&
          enumeration.discovery_progress_units) {
        const uint64_t units = enumeration.discovery_progress_units;
        const uint64_t credit =
            units > UINT64_MAX / discovery_feedback.ticks_per_generator
                ? UINT64_MAX
                : units * discovery_feedback.ticks_per_generator;
        uint64_t renewed =
            credit > UINT64_MAX - discovery_limit_at_entry
                ? UINT64_MAX
                : discovery_limit_at_entry + credit;
        renewed = std::min(renewed, discovery_feedback.hard_limit);
        sr_discovery_allowance->set_limit(WorkLimit::bounded(renewed));
      }
      out.budget_status = discovery_budget.charge(
          WorkUnit::SrDiscoveryTicks, enumeration.ticks);
      if (enumeration.nonforcing_orbit_cuts_declined) {
        ++k.stats.sr_nonforcing_cut_batches_declined;
        k.stats.sr_nonforcing_cuts_declined +=
            enumeration.nonforcing_orbit_cuts_declined;
      }
      if (enumeration.singleton_forcing_rows_declined) {
        ++k.stats.sr_singleton_forcing_batches_declined;
        k.stats.sr_singleton_forcing_rows_declined +=
            enumeration.singleton_forcing_rows_declined;
      }
      if (out.budget_status == BudgetStatus::AccountingOverflow) {
        out.status = InvocationStatus::AccountingError;
        return false;
      }
      if (enumeration.discovery_progress_units &&
          !inline_generator_feedback) {
        // The generic callback prefix is exact but intentionally unpublished:
        // its callback-order subgroup is incomplete. Leave this step pending
        // so the root scheduler can reward the measured producer progress and
        // retry with a larger shared allowance.
        runtime.sr_selection.clear();
        out.status = InvocationStatus::BudgetExhausted;
        out.budget_status = BudgetStatus::Exhausted;
        out.discovery_progress_units = enumeration.discovery_progress_units;
        out.step_id = step.id;
        return false;
      }
      if (enumeration.status == SymmetryEnumerationStatus::Interrupted) {
        out.status = InvocationStatus::Interrupted;
        return false;
      }
      if (enumeration.status == SymmetryEnumerationStatus::Invalid) {
        out.status = InvocationStatus::InvalidRequest;
        return false;
      }
      if (enumeration.status ==
              SymmetryEnumerationStatus::ResourceExhausted ||
          enumeration.status == SymmetryEnumerationStatus::Failed) {
        out.status = InvocationStatus::ResourceExhausted;
        return false;
      }
      runtime.selection_truncated =
          enumeration.status == SymmetryEnumerationStatus::LimitReached;
      runtime.selection_discovery_exhausted =
          shared_discovery && runtime.selection_truncated;
      const uint64_t discovery_consumed_before =
          shared_discovery ? sr_discovery_allowance->consumed() : 0;
      const uint64_t discovery_consumed_after =
          enumeration.ticks >
                  UINT64_MAX - discovery_consumed_before
              ? UINT64_MAX
              : discovery_consumed_before + enumeration.ticks;
      if (inline_generator_feedback && runtime.selection_truncated &&
          runtime.sr_selection.empty() &&
          !enumeration.discovery_progress_units &&
          !runtime.graph_entry_requested &&
          graph_entry_reaches_detector() &&
          discovery_feedback.hard_limit >
              discovery_consumed_after) {
        runtime.graph_entry_requested = true;
        out.status = InvocationStatus::BudgetExhausted;
        out.budget_status = BudgetStatus::Exhausted;
        out.discovery_entry_required = true;
        out.step_id = step.id;
        return false;
      }
    }
    runtime.selection.resize(runtime.sr_selection.size());
    for (size_t i = 0; i < runtime.selection.size(); ++i)
      runtime.selection[i] = i;
  } else if (step.rule == "BVE") {
    const auto *configured = std::get_if<BveParameters>(&step.parameters);
    const BveParameters parameters =
        configured ? *configured : BveParameters::from_kernel(k);
    if ((stop && *stop) || (k.stop && *k.stop)) {
      out.status = InvocationStatus::Interrupted;
      return false;
    }
    bve_complete_instances(k, rule, parameters, runtime.selection);
    if ((stop && *stop) || (k.stop && *k.stop)) {
      runtime.selection.clear();
      out.status = InvocationStatus::Interrupted;
      return false;
    }
  } else {
    out.status = enumerate_complete_instances(k, rule, runtime.selection, stop);
    if (out.status != InvocationStatus::Completed)
      return false;
  }
  runtime.selection_owner = &k;
  runtime.selection_generation = k.arena_generation;
  runtime.expected_state = state_version(k);
  runtime.selection_ready = true;
  return true;
}

bool OnlineStrategyExecutor::finish_step_pass(const Kernel &k,
                                              const OnlineInvocation &step,
                                              Runtime &runtime,
                                              InvocationResult &out) {
  runtime.selection_ready = false;
  runtime.bve_occurrences.reset();
  runtime.selection.clear();
  runtime.structured_selection.clear();
  runtime.sr_selection.clear();
  runtime.sr_general_cursor = {};
  runtime.sr_general_mode = false;
  runtime.selection_truncated = false;
  runtime.selection_discovery_exhausted = false;
  runtime.cursor = 0;
  ++runtime.passes;
  const bool progress = runtime.pass_progress;
  sequence_progress_ |= progress;

  bool repeat = false;
  switch (step.repeat.mode) {
  case RepeatMode::Once:
    break;
  case RepeatMode::Count:
    repeat = runtime.passes < step.repeat.limit;
    break;
  case RepeatMode::UntilScopedFixpoint:
  case RepeatMode::UntilSizeScaledFixpoint:
    if (!progress) {
      out.status = InvocationStatus::ScopedFixpoint;
      out.selection_complete = true;
    } else if (const uint64_t limit = repeat_round_limit(k, step.repeat);
               !limit || runtime.passes < limit) {
      repeat = true;
    } else {
      out.status = InvocationStatus::BudgetExhausted;
      out.selection_complete = true;
    }
    break;
  }
  runtime.pass_progress = false;
  if (repeat)
    return false;
  ++step_index_;
  return true;
}

bool OnlineStrategyExecutor::finish_sequence(const Kernel &k,
                                             InvocationResult &out) {
  ++sequence_passes_;
  bool repeat = false;
  switch (program_.repeat.mode) {
  case RepeatMode::Once:
    break;
  case RepeatMode::Count:
    repeat = sequence_passes_ < program_.repeat.limit;
    break;
  case RepeatMode::UntilScopedFixpoint:
  case RepeatMode::UntilSizeScaledFixpoint:
    if (!sequence_progress_) {
      out.status = InvocationStatus::ScopedFixpoint;
      out.selection_complete = true;
    } else if (const uint64_t limit = repeat_round_limit(k, program_.repeat);
               !limit || sequence_passes_ < limit) {
      repeat = true;
    } else {
      out.status = InvocationStatus::BudgetExhausted;
      out.selection_complete = true;
    }
    break;
  }
  if (repeat) {
    step_index_ = 0;
    for (Runtime &runtime : runtime_) {
      runtime.passes = 0;
      runtime.cursor = 0;
      runtime.pass_progress = false;
      runtime.selection_ready = false;
      runtime.selection.clear();
      runtime.structured_selection.clear();
      runtime.sr_selection.clear();
      runtime.sr_general_cursor = {};
      runtime.sr_general_mode = false;
      runtime.selection_truncated = false;
      runtime.selection_discovery_exhausted = false;
    }
    sequence_progress_ = false;
    return false;
  }
  complete_ = true;
  if (out.status != InvocationStatus::ScopedFixpoint &&
      out.status != InvocationStatus::BudgetExhausted)
    out.status = InvocationStatus::Completed;
  out.selection_complete = true;
  return true;
}

InvocationResult OnlineStrategyExecutor::advance(
    Kernel &k, volatile sig_atomic_t *stop,
    Allowance *sr_discovery_allowance,
    DiscoveryFeedbackPolicy discovery_feedback) {
  InvocationResult out;
  out.before = state_version(k);
  out.after = out.before;
  out.configuration_revision = revision_;
  if (!configured_) {
    out.status = InvocationStatus::InvalidRequest;
    return out;
  }
  if (complete_) {
    out.status = InvocationStatus::Completed;
    out.selection_complete = true;
    return out;
  }
  if ((stop && *stop) || (k.stop && *k.stop)) {
    out.status = InvocationStatus::Interrupted;
    return out;
  }

  for (;;) {
    // Empty scopes may repeat without reaching an atomic rule application.
    // Poll at composition boundaries too, so those loops remain interruptible.
    if ((stop && *stop) || (k.stop && *k.stop)) {
      out.status = InvocationStatus::Interrupted;
      out.selection_complete = false;
      return out;
    }
    if (step_index_ == program_.sequence.size()) {
      if (finish_sequence(k, out))
        return out;
      continue;
    }
    const OnlineInvocation &step = program_.sequence[step_index_];
    Runtime &runtime = runtime_[step_index_];
    out.work_unit = step.work_unit;
    if (runtime.selection_ready &&
        (runtime.selection_owner != &k ||
         runtime.expected_state.arena_generation != k.arena_generation ||
         runtime.expected_state.touch_epoch != k.touch_epoch ||
         runtime.expected_state.strategy_epoch != k.strategy_epoch)) {
      out.status = InvocationStatus::StaleRequest;
      out.step_id = step.id;
      out.after = state_version(k);
      return out;
    }
    if (runtime.selection_ready && runtime.cursor < runtime.selection.size() &&
        runtime.selection_generation != k.arena_generation) {
      if (step.granularity != InvocationGranularity::CompleteCurrentPass) {
        out.status = InvocationStatus::StaleRequest;
        out.step_id = step.id;
        out.after = state_version(k);
        return out;
      }
      // A preceding semantic action collected or rebuilt the arena. A
      // complete pass has no caller-owned payload identity, so restart its
      // enumeration at the new epoch while retaining progress already made in
      // this pass. An external transition was rejected by the state check
      // above and can never be silently absorbed here.
      const bool progress = runtime.pass_progress;
      runtime.selection_ready = false;
      if (!prepare_selection(k, step, runtime, out, stop,
                             sr_discovery_allowance, discovery_feedback)) {
        out.after = state_version(k);
        return out;
      }
      runtime.pass_progress |= progress;
    }
    if (!runtime.selection_ready) {
      if (!prepare_selection(k, step, runtime, out, stop,
                             sr_discovery_allowance, discovery_feedback)) {
        // An inapplicable scope is not a fixpoint and is not consumed. The
        // caller may change solver state or replace the program and retry.
        out.after = state_version(k);
        return out;
      }
      if (runtime.selection.empty()) {
        if (runtime.selection_truncated) {
          out.status = InvocationStatus::BudgetExhausted;
          out.budget_status = BudgetStatus::Exhausted;
          out.discovery_exhausted = runtime.selection_discovery_exhausted;
          out.selection_complete = false;
          out.step_id = step.id;
          out.after = state_version(k);
          return out;
        }
        finish_step_pass(k, step, runtime, out);
        if (out.status == InvocationStatus::ScopedFixpoint ||
            out.status == InvocationStatus::BudgetExhausted) {
          out.after = state_version(k);
          return out;
        }
        continue;
      }
    }
    if (runtime.cursor == runtime.selection.size()) {
      if (runtime.sr_general_mode &&
          !runtime.sr_general_cursor.complete) {
        runtime.selection_ready = false;
        if (!prepare_selection(k, step, runtime, out, stop,
                               sr_discovery_allowance, discovery_feedback)) {
          out.after = state_version(k);
          return out;
        }
        continue;
      }
      if (runtime.selection_truncated) {
        out.status = InvocationStatus::BudgetExhausted;
        out.budget_status = BudgetStatus::Exhausted;
        out.discovery_exhausted = runtime.selection_discovery_exhausted;
        out.selection_complete = false;
        out.step_id = step.id;
        out.after = state_version(k);
        return out;
      }
      finish_step_pass(k, step, runtime, out);
      if (out.status == InvocationStatus::ScopedFixpoint ||
          out.status == InvocationStatus::BudgetExhausted) {
        out.after = state_version(k);
        return out;
      }
      continue;
    }

    const WorkLimit allowance = remaining(step, runtime.consumed);
    if (!allowance.admits_work()) {
      out.status = InvocationStatus::BudgetExhausted;
      out.budget_status = BudgetStatus::Exhausted;
      return out;
    }

    const RuleVTable &rule = tactix_rules()[tactix_rule_id(step.rule.c_str())];
    // Eligibility can change between advances even when arena offsets remain
    // valid. A declined guard does not consume an application or certify scope.
    const bool explicit_fixpoint_propagation =
        explicit_fixpoint_propagation_service(step, k);
    if (!rule.guard(k) && !explicit_fixpoint_propagation) {
      out.status = InvocationStatus::Inapplicable;
      out.step_id = step.id;
      out.rule_id = rule.id;
      return out;
    }
    const uint64_t payload = runtime.selection[runtime.cursor];
    const size_t trail_before = k.trail.size();
    InvocationBudget budget(step.work_unit, allowance);
    bool consume_generated_sr_noop = false;
    bool discard_generated_sr_batch = false;
    bool preserve_affine_selection = false;
    bool preserve_theory_selection = false;
    std::optional<Gf3AffineActionResult> gf3_affine_application;
    std::optional<DirectCspSupportActionResult> direct_csp_application;
    std::optional<NaryCspPrefixActionResult> nary_csp_application;
    std::optional<NaryCspEqualityActionResult> nary_csp_equality_application;
    std::optional<DirectPhpActionResult> direct_php_application;
    std::optional<Gf3GaussianActionResult> gf3_gaussian_application;
    std::optional<Gf3GeneralRowActionResult> gf3_general_row_application;
    std::optional<TseitinCycleBreakingResult> tseitin_cycle_application;
    std::optional<SemanticVariableCandidate> deferred_semantic_candidate;
    const uint64_t affine_budget_before = k.stats.affine_model_budget;
    const uint64_t affine_interrupt_before = k.stats.affine_model_interrupted;
    const uint64_t affine_unavailable_before = k.stats.affine_model_unavailable;
    ScopedRuleMaterialSettings material(k, step.material);
    // Attach only for a generated pure-BVE pass and its atomic propagation
    // follow-up. No index survives at the public boundary or changes another
    // rule's eligibility. Borrow an existing caller-owned index unchanged.
    std::optional<BveOccurrenceCache::Scope> bve_occurrences;
    if (step.rule == "BVE" &&
        step.granularity == InvocationGranularity::CompleteCurrentPass) {
      const auto *parameters = std::get_if<BveParameters>(&step.parameters);
      if (parameters && pure_bve_candidate_scope(parameters->candidate_scope))
        bve_occurrences.emplace(runtime.bve_occurrences, k);
    }
    const bool generated_sr_pass =
        step.rule == "SubstitutionRedundancy" &&
        step.granularity == InvocationGranularity::CompleteCurrentPass;
    if (explicit_fixpoint_propagation) {
      // The producer has already left BCP at a fixpoint. Preserve its staged
      // semantic candidate and state identity while still consuming this
      // explicit public action boundary and emitting raw feedback/replay.
      out.before = out.after;
      out.configuration_revision = revision_;
      out.work_unit = step.work_unit;
      out.rule_id = rule.id;
      out.instance = payload;
      out.work = 0;
      out.applications = 1;
      out.rule_result = ApplyResult::NoOp;
      out.made_rule_progress = false;
      out.selection_complete = runtime.cursor + 1 == runtime.selection.size();
      out.budget_status = budget.charge(step.work_unit, 0);
      out.status = InvocationStatus::NoChange;
      out.after = state_version(k);
    } else if (step.rule == "BVE") {
      ScopedRuleWorkBudget rule_work_budget(k, step.rule_work_budget);
      const auto *configured = std::get_if<BveParameters>(&step.parameters);
      const BveParameters parameters =
          configured ? *configured : BveParameters::from_kernel(k);
      BveInvocationSession session(rule, k, parameters, budget, revision_);
      (void)execution::execute_action(
          k, execution::RuleApplicationRequest{}, [&] {
            out = payload == INSTANCE_CANONICAL ? session.apply_canonical()
                                                : session.apply_one(payload);
            return out.rule_result;
          }, HeuristicRulePublisher{});
    } else if (step.rule == "Sweep" &&
               std::holds_alternative<SweepParameters>(step.parameters)) {
      ScopedRuleWorkBudget rule_work_budget(k, step.rule_work_budget);
      const SweepParameters &limits = std::get<SweepParameters>(step.parameters);
      const ApplyResult result = execution::execute_action(
          k, execution::RuleApplicationRequest{}, [&] {
            return execution::execute_action(k, execution::SweepRequest{limits, payload});
          }, HeuristicRulePublisher{}).result;
      out.before = out.after;
      out.configuration_revision = revision_;
      out.work_unit = WorkUnit::Applications;
      out.rule_id = rule.id;
      out.instance = payload;
      out.work = 1;
      out.applications = 1;
      out.rule_result = result;
      out.made_rule_progress = result == ApplyResult::Ok;
      out.selection_complete = runtime.cursor + 1 == runtime.selection.size();
      out.budget_status = budget.charge(WorkUnit::Applications, 1);
      out.status = result == ApplyResult::NoOp
                       ? InvocationStatus::NoChange
                       : result == ApplyResult::Ok
                             ? InvocationStatus::Completed
                             : detail::terminal_status(result);
      out.after = state_version(k);
    } else {
      const uint64_t propagations_before = k.stats.propagations;
      const GuidanceOutcomeSnapshot guidance_before(k);
      execution::ParameterExecutionScope<
          AnalyzeParameters, SubsumeForwardParameters, StrengthenParameters,
          CongruenceParameters, InstantiateParameters, BackboneParameters,
          VivifyParameters, BceParameters, TernaryParameters, FactorParameters,
          TransitiveReduceParameters, SubstituteParameters, ReduceParameters,
          WalkParameters, ReorderParameters, ParityModelParameters,
          OnehotModelParameters, BalancedSignatureModelParameters,
          FermatModelParameters, LookupCircuitModelParameters,
          SimonModelParameters, StconnProofParameters,
          SignedCardinalityProofParameters, ExactCoverModelParameters,
          IndependentSetModelParameters, AffineModelParameters, GraphLabelingParameters,
          RamseyStartupParameters,
          ParityConsequenceParameters, AffineConsequenceParameters,
          ExactCoverExtensionParameters, IndependentSetCoverParameters,
          Gf3AffineConsequenceParameters, DirectCspSupportParameters,
          NaryCspPrefixParameters, NaryCspEqualityParameters,
          DirectPhpRefutationParameters,
          Gf3GaussianConsequenceParameters,
          Gf3GeneralRowConsequenceParameters,
          TseitinCycleBreakingParameters,
          SemanticVariableAdviceParameters>
          parameters(k, step.parameters);
      // Invocation-level native work limits override a rule parameter which
      // happens to share the legacy Kernel::work_budget handoff (Walk).  Both
      // scopes still unwind to the caller profile after this atomic apply.
      ScopedRuleWorkBudget rule_work_budget(k, step.rule_work_budget);
      const bool enumerated_factor_grid =
          step.rule == "Factor" &&
          step.granularity == InvocationGranularity::CompleteCurrentPass;
      const bool enumerated_sr =
          generated_sr_pass;
      std::optional<SubstitutionRedundancyApplicationResult> sr_application;
      std::optional<AffineConsequenceActionResult> affine_application;
      std::optional<ExactCoverExtensionActionResult> exact_cover_application;
      std::optional<IndependentSetCoverActionResult>
          independent_set_application;
      bool condition_declined = false;
      bool condition_interrupted = false;
      const ApplyResult result = execution::execute_action(
          k, execution::RuleApplicationRequest{}, [&] { return
          !step.affine_basis_instances.empty()
              ? apply_rule_structured_defer_candidate(
                    rule, k, "affine-basis-row", [&] {
                  if (!k.affine_basis_action_session)
                    return ApplyResult::NoOp;
                  affine_application.emplace(affine_consequence_action_apply(
                      k, *k.affine_basis_action_session,
                      step.affine_basis_instances[payload]));
                  return affine_application->result;
                })
          : !step.gf3_affine_instances.empty()
              ? apply_rule_structured(rule, k, "gf3-affine-consequence", [&] {
                  const auto &parameters =
                      std::get<Gf3AffineConsequenceParameters>(step.parameters);
                  const Gf3AffineInstance &selected =
                      step.gf3_affine_instances[payload];
                  const size_t keys_before =
                      k.gf3_affine_action_session->committed_keys();
                  gf3_affine_application.emplace(gf3_affine_action_apply(
                      k, *k.gf3_affine_action_session,
                      gf3_affine_limits(parameters), selected.first_equation,
                      selected.second_equation));
                  record_gf3_affine_feedback(k, *gf3_affine_application,
                                             keys_before);
                  return gf3_affine_application->result;
                })
          : !step.direct_csp_support_instances.empty()
              ? apply_rule_structured(rule, k, "direct-csp-support", [&] {
                  const auto &parameters =
                      std::get<DirectCspSupportParameters>(step.parameters);
                  const size_t keys_before =
                      k.direct_csp_support_action_session->committed_keys();
                  const size_t owners_before =
                      k.direct_csp_support_action_session->retained_owners();
                  direct_csp_application.emplace(direct_csp_support_action_apply(
                      k, *k.direct_csp_support_action_session,
                      direct_csp_limits(parameters),
                      step.direct_csp_support_instances[payload]));
                  record_direct_csp_feedback(k, *direct_csp_application,
                                             keys_before, owners_before);
                  return direct_csp_application->result;
                })
          : !step.nary_csp_prefix_instances.empty()
              ? apply_rule_structured(rule, k, "nary-csp-prefix", [&] {
                  const auto &parameters =
                      std::get<NaryCspPrefixParameters>(step.parameters);
                  const size_t keys_before =
                      k.nary_csp_prefix_action_session->committed_keys();
                  const size_t owners_before =
                      k.nary_csp_prefix_action_session->retained_owners();
                  nary_csp_application.emplace(nary_csp_prefix_action_apply(
                      k, *k.nary_csp_prefix_action_session,
                      nary_csp_limits(parameters),
                      step.nary_csp_prefix_instances[payload]));
                  record_nary_csp_feedback(k, *nary_csp_application,
                                           keys_before, owners_before);
                  return nary_csp_application->result;
                })
          : !step.nary_csp_equality_instances.empty()
              ? apply_rule_structured(rule, k, "nary-csp-equality", [&] {
                  const auto &parameters =
                      std::get<NaryCspEqualityParameters>(step.parameters);
                  if (!k.nary_csp_prefix_action_session ||
                      !k.nary_csp_equality_session)
                    return ApplyResult::NoOp;
                  const size_t keys_before =
                      k.nary_csp_equality_session->keys().size();
                  nary_csp_equality_application.emplace(
                      nary_csp_equality_action_apply(
                          k, *k.nary_csp_prefix_action_session,
                          *k.nary_csp_equality_session,
                          nary_csp_equality_limits(parameters),
                          step.nary_csp_equality_instances[payload]));
                  record_nary_csp_equality_feedback(
                      k, *nary_csp_equality_application, keys_before);
                  return nary_csp_equality_application->result;
                })
          : !step.gf3_gaussian_instances.empty()
              ? apply_rule_structured(rule, k, "gf3-gaussian-consequence", [&] {
                  const auto &parameters =
                      std::get<Gf3GaussianConsequenceParameters>(step.parameters);
                  gf3_gaussian_application.emplace(gf3_gaussian_action_apply(
                      k, *k.gf3_gaussian_action_session,
                      gf3_gaussian_limits(parameters),
                      step.gf3_gaussian_instances[payload]));
                  record_gf3_gaussian_action_feedback(
                      k, *gf3_gaussian_application);
                  return gf3_gaussian_application->result;
                })
          : !step.gf3_general_row_instances.empty()
              ? apply_rule_structured(rule, k, "gf3-general-row", [&] {
                  const auto &parameters =
                      std::get<Gf3GeneralRowConsequenceParameters>(
                          step.parameters);
                  if (!k.gf3_general_row_action_session)
                    return ApplyResult::NoOp;
                  gf3_general_row_application.emplace(
                      gf3_general_row_action_apply(
                          k, *k.gf3_general_row_action_session,
                          gf3_general_row_limits(parameters),
                          step.gf3_general_row_instances[payload]));
                  record_gf3_general_row_action_feedback(
                      k, *gf3_general_row_application);
                  return gf3_general_row_application->result;
                })
          : !step.tseitin_cycle_instances.empty()
              ? apply_rule_structured_preserve_startup(
                    rule, k, "tseitin-cycle-breaking", [&] {
                  const auto &parameters =
                      std::get<TseitinCycleBreakingParameters>(step.parameters);
                  tseitin_cycle_application.emplace(
                      tseitin_cycle_breaking_apply(
                          k, *k.tseitin_cycle_breaking_session,
                          tseitin_cycle_limits(parameters),
                          step.tseitin_cycle_instances[payload]));
                  record_tseitin_cycle_breaking_feedback(
                      k, *tseitin_cycle_application);
                  return tseitin_cycle_application->result;
                    })
          : !step.independent_set_cover_instances.empty()
              ? apply_rule_structured_defer_candidate(
                    rule, k, "independent-set-cover", [&] {
                  independent_set_application.emplace(
                      independent_set_cover_action_apply(
                          k, *k.independent_set_cover_action_session,
                          step.independent_set_cover_instances[payload]));
                  return independent_set_application->result;
                })
          : step.rule == "DirectPhpRefutation" &&
                    payload == INSTANCE_CANONICAL
              ? apply_rule_structured(rule, k, "canonical", [&] {
                  if (!k.direct_php_action_session)
                    return ApplyResult::NoOp;
                  const auto &parameters =
                      std::get<DirectPhpRefutationParameters>(step.parameters);
                  direct_php_application.emplace(direct_php_action_apply(
                      k, *k.direct_php_action_session,
                      DirectPhpActionRequest{1, 1,
                                             direct_php_limits(parameters)}));
                  record_direct_php_action_feedback(k,
                                                    *direct_php_application);
                  return direct_php_application->status ==
                                 DirectPhpActionStatus::Applied
                             ? ApplyResult::Unsat
                             : ApplyResult::NoOp;
                })
          : step.rule == "AffineConsequence" && payload == INSTANCE_CANONICAL
              ? apply_rule_structured_defer_candidate(
                    rule, k, "canonical", [&] {
                  affine_application.emplace(
                      affine_consequence_action_apply(k));
                  return affine_application->result;
                })
          : step.rule == "ExactCoverExtension" &&
                    payload == INSTANCE_CANONICAL
              ? apply_rule_structured_defer_candidate(
                    rule, k, "canonical", [&] {
                  exact_cover_application.emplace(
                      exact_cover_extension_action_apply(
                          k, *k.exact_cover_extension_action_session));
                  return exact_cover_application->live.result;
                })
          : !step.condition_instances.empty()
              ? [&] {
                  const ConditionInstance &request =
                      step.condition_instances[payload];
                  ConditionInstance selected = request;
                  for (Lit &literal : selected.clause) {
                    const Var external = var_of(literal);
                    if (external >= k.external_to_internal.size() ||
                        k.external_to_internal[external] == INVALID_VAR) {
                      condition_declined = true;
                      return ApplyResult::NoOp;
                    }
                    literal = make_lit(k.external_to_internal[external],
                                       negated(literal));
                  }
                  ConditionPrepared prepared;
                  if (!condition_prepare(k, selected, prepared)) {
                    condition_declined = true;
                    condition_interrupted =
                        (stop && *stop) || (k.stop && *k.stop);
                    return ApplyResult::NoOp;
                  }
                  return apply_rule_structured(rule, k, "exact-condition", [&] {
                    return condition_commit(k, selected, prepared);
                  });
                }()
          : (!step.substitution_redundancy_instances.empty() || enumerated_sr)
              ? apply_rule_structured(rule, k, "exact-sr", [&] {
                  const auto *configured =
                      std::get_if<SubstitutionRedundancyParameters>(
                          &step.parameters);
                  const SubstitutionRedundancyParameters parameters =
                      configured ? *configured
                                 : SubstitutionRedundancyParameters{};
                  const SubstitutionRedundancyInstance &selected =
                      !step.substitution_redundancy_instances.empty()
                          ? step.substitution_redundancy_instances[payload]
                          : runtime.sr_selection[payload];
                  SubstitutionRedundancyInstance reoriented;
                  const SubstitutionRedundancyInstance *instance = &selected;
                  if (enumerated_sr &&
                      selected.unique_literal_amo_certificate &&
                      selected.clause.size() == 2 &&
                      k.value(selected.clause[0]) < 0 &&
                      k.value(selected.clause[1]) >= 0) {
                    // ULC discovery certifies either member as the SR pivot.
                    // Satsuma publishes the whole AMO before propagation, but
                    // the online executor observes root consequences between
                    // atomic members. Keep caller-owned ordering observable;
                    // only the private generated batch turns a now-false
                    // pivot around so the still-open endpoint can realize the
                    // same add-then-propagate closure.
                    reoriented = selected;
                    std::swap(reoriented.clause[0], reoriented.clause[1]);
                    instance = &reoriented;
                  }
                  sr_application =
                      enumerated_sr &&
                              (instance->orbitopal_prefix_certificate ||
                               instance->symmetry_chain_certificate ||
                               instance->unique_literal_amo_certificate)
                          ? substitution_redundancy_apply_orbitopal_prefix(
                                k, *instance, parameters, stop)
                          : substitution_redundancy_apply_detailed(
                                k, *instance, parameters, stop);
                  return sr_application->rule_result;
                })
          : (!step.factor_grids.empty() || enumerated_factor_grid)
              ? apply_rule_structured(rule, k, "exact-grid", [&] {
                  const FactorGrid &grid = !step.factor_grids.empty()
                                               ? step.factor_grids[payload]
                                               : runtime.structured_selection
                                                     [payload];
                  return factor_apply_grid(k, grid);
                })
          : execution::execute_action(
                k, execution::RegisteredRuleRequest{rule, payload});
          }, HeuristicRulePublisher{}).result;
      // Factor/Substitute parameter scopes control how their newly published
      // ranking advice is consumed. Drain it before those scopes restore the
      // caller's profile; retirement itself is representation state and is
      // always reflected in H.
      out.before = out.after;
      out.configuration_revision = revision_;
      SemanticVariableCandidate staged;
      if (take_staged_semantic_variable_candidate(k, staged))
        deferred_semantic_candidate = std::move(staged);
      out.work_unit = step.work_unit;
      out.rule_id = rule.id;
      out.instance = payload;
      out.work = step.work_unit == WorkUnit::Propagations
                     ? k.stats.propagations - propagations_before
                     : 1;
      out.applications = 1;
      out.rule_result = result;
      out.made_rule_progress = result == ApplyResult::Ok;
      out.selection_complete = runtime.cursor + 1 == runtime.selection.size();
      out.budget_status = budget.charge(step.work_unit, out.work);
      out.status = result == ApplyResult::NoOp
                       ? InvocationStatus::NoChange
                       : result == ApplyResult::Ok
                             ? InvocationStatus::Completed
                             : detail::terminal_status(result);
      out.status = guidance_before.refine(k, result, out.status);
      const auto retain_theory_selection = [&] {
        // Recognition/proof attempts consume their one application dose even
        // when their typed local limit or live-state recheck declines.  The
        // detailed raw counters in the before/after observations carry the
        // actual heterogeneous effort.  Keep the selected payload pending so
        // an explicit reconfiguration can retry it with different limits.
        out.selection_complete = false;
        preserve_theory_selection = true;
      };
      if (affine_application) {
        switch (affine_application->status) {
        case AffineConsequenceActionStatus::Invalid:
          out.status = InvocationStatus::InvalidRequest;
          retain_theory_selection();
          break;
        case AffineConsequenceActionStatus::Inapplicable:
        case AffineConsequenceActionStatus::UnsupportedProof:
          out.status = InvocationStatus::Inapplicable;
          retain_theory_selection();
          break;
        case AffineConsequenceActionStatus::ResourceExhausted:
          out.status = InvocationStatus::ResourceExhausted;
          retain_theory_selection();
          break;
        case AffineConsequenceActionStatus::Interrupted:
          out.status = InvocationStatus::Interrupted;
          retain_theory_selection();
          break;
        case AffineConsequenceActionStatus::Stale:
          out.status = InvocationStatus::StaleRequest;
          retain_theory_selection();
          break;
        case AffineConsequenceActionStatus::NoStructure:
        case AffineConsequenceActionStatus::AlreadyPublished:
        case AffineConsequenceActionStatus::Applied:
          break;
        }
      }
      if (exact_cover_application) {
        switch (exact_cover_application->live.status) {
        case ExactCoverLiveStatus::Invalid:
          out.status = InvocationStatus::InvalidRequest;
          retain_theory_selection();
          break;
        case ExactCoverLiveStatus::Inapplicable:
        case ExactCoverLiveStatus::UnsupportedProof:
          out.status = InvocationStatus::Inapplicable;
          retain_theory_selection();
          break;
        case ExactCoverLiveStatus::ResourceExhausted:
          out.status = InvocationStatus::ResourceExhausted;
          retain_theory_selection();
          break;
        case ExactCoverLiveStatus::Interrupted:
          out.status = InvocationStatus::Interrupted;
          retain_theory_selection();
          break;
        case ExactCoverLiveStatus::Stale:
          out.status = InvocationStatus::StaleRequest;
          retain_theory_selection();
          break;
        case ExactCoverLiveStatus::NoStructure:
        case ExactCoverLiveStatus::AlreadyPublished:
        case ExactCoverLiveStatus::Applied:
          break;
        }
      }
      if (independent_set_application) {
        switch (independent_set_application->status) {
        case IndependentSetLiveStatus::Invalid:
          out.status = InvocationStatus::InvalidRequest;
          retain_theory_selection();
          break;
        case IndependentSetLiveStatus::Inapplicable:
        case IndependentSetLiveStatus::UnsupportedProof:
          out.status = InvocationStatus::Inapplicable;
          retain_theory_selection();
          break;
        case IndependentSetLiveStatus::ResourceExhausted:
          out.status = InvocationStatus::ResourceExhausted;
          retain_theory_selection();
          break;
        case IndependentSetLiveStatus::Interrupted:
          out.status = InvocationStatus::Interrupted;
          retain_theory_selection();
          break;
        case IndependentSetLiveStatus::Stale:
          out.status = InvocationStatus::StaleRequest;
          retain_theory_selection();
          break;
        case IndependentSetLiveStatus::NoStructure:
        case IndependentSetLiveStatus::Duplicate:
        case IndependentSetLiveStatus::Applied:
          break;
        }
      }
      if (gf3_affine_application) {
        switch (gf3_affine_application->status) {
        case Gf3AffineActionStatus::Invalid:
          out.status = InvocationStatus::InvalidRequest;
          break;
        case Gf3AffineActionStatus::Inapplicable:
        case Gf3AffineActionStatus::UnsupportedProof:
          out.status = InvocationStatus::Inapplicable;
          break;
        case Gf3AffineActionStatus::ResourceExhausted:
          out.status = InvocationStatus::ResourceExhausted;
          retain_theory_selection();
          break;
        case Gf3AffineActionStatus::Interrupted:
          out.status = InvocationStatus::Interrupted;
          retain_theory_selection();
          break;
        case Gf3AffineActionStatus::Stale:
          out.status = InvocationStatus::StaleRequest;
          break;
        case Gf3AffineActionStatus::NoStructure:
        case Gf3AffineActionStatus::NoConsequence:
        case Gf3AffineActionStatus::AlreadyPublished:
        case Gf3AffineActionStatus::Applied:
          break;
        }
      }
      if (direct_csp_application) {
        switch (direct_csp_application->status) {
        case DirectCspSupportActionStatus::Invalid:
          out.status = InvocationStatus::InvalidRequest;
          break;
        case DirectCspSupportActionStatus::Inapplicable:
        case DirectCspSupportActionStatus::UnsupportedProof:
          out.status = InvocationStatus::Inapplicable;
          break;
        case DirectCspSupportActionStatus::ResourceExhausted:
          out.status = InvocationStatus::ResourceExhausted;
          retain_theory_selection();
          break;
        case DirectCspSupportActionStatus::Interrupted:
          out.status = InvocationStatus::Interrupted;
          retain_theory_selection();
          break;
        case DirectCspSupportActionStatus::Stale:
          out.status = InvocationStatus::StaleRequest;
          break;
        case DirectCspSupportActionStatus::NoStructure:
        case DirectCspSupportActionStatus::NoConsequence:
        case DirectCspSupportActionStatus::AlreadyPublished:
        case DirectCspSupportActionStatus::Applied:
          break;
        }
      }
      if (nary_csp_application) {
        switch (nary_csp_application->status) {
        case NaryCspPrefixActionStatus::Invalid:
          out.status = InvocationStatus::InvalidRequest;
          break;
        case NaryCspPrefixActionStatus::Inapplicable:
        case NaryCspPrefixActionStatus::UnsupportedProof:
          out.status = InvocationStatus::Inapplicable;
          break;
        case NaryCspPrefixActionStatus::ResourceExhausted:
          out.status = InvocationStatus::ResourceExhausted;
          retain_theory_selection();
          break;
        case NaryCspPrefixActionStatus::Interrupted:
          out.status = InvocationStatus::Interrupted;
          retain_theory_selection();
          break;
        case NaryCspPrefixActionStatus::Stale:
          out.status = InvocationStatus::StaleRequest;
          break;
        case NaryCspPrefixActionStatus::NoStructure:
        case NaryCspPrefixActionStatus::NoConsequence:
        case NaryCspPrefixActionStatus::AlreadyPublished:
        case NaryCspPrefixActionStatus::Applied:
          break;
        }
      }
      if (nary_csp_equality_application) {
        switch (nary_csp_equality_application->status) {
        case NaryCspEqualityActionStatus::Invalid:
          out.status = InvocationStatus::InvalidRequest;
          break;
        case NaryCspEqualityActionStatus::Inapplicable:
        case NaryCspEqualityActionStatus::UnsupportedProof:
          out.status = InvocationStatus::Inapplicable;
          break;
        case NaryCspEqualityActionStatus::ResourceExhausted:
          out.status = InvocationStatus::ResourceExhausted;
          retain_theory_selection();
          break;
        case NaryCspEqualityActionStatus::Interrupted:
          out.status = InvocationStatus::Interrupted;
          retain_theory_selection();
          break;
        case NaryCspEqualityActionStatus::Stale:
          out.status = InvocationStatus::StaleRequest;
          break;
        case NaryCspEqualityActionStatus::NoStructure:
        case NaryCspEqualityActionStatus::NoConsequence:
        case NaryCspEqualityActionStatus::AlreadyPublished:
        case NaryCspEqualityActionStatus::Applied:
          break;
        }
      }
      if (gf3_gaussian_application) {
        switch (gf3_gaussian_application->status) {
        case Gf3GaussianActionStatus::Invalid:
          out.status = InvocationStatus::InvalidRequest;
          break;
        case Gf3GaussianActionStatus::Inapplicable:
        case Gf3GaussianActionStatus::UnsupportedProof:
          out.status = InvocationStatus::Inapplicable;
          break;
        case Gf3GaussianActionStatus::ResourceExhausted:
          out.status = InvocationStatus::ResourceExhausted;
          retain_theory_selection();
          break;
        case Gf3GaussianActionStatus::Interrupted:
          out.status = InvocationStatus::Interrupted;
          retain_theory_selection();
          break;
        case Gf3GaussianActionStatus::Stale:
          out.status = InvocationStatus::StaleRequest;
          break;
        case Gf3GaussianActionStatus::NoStructure:
        case Gf3GaussianActionStatus::NoSupportedOutcome:
        case Gf3GaussianActionStatus::AlreadyPublished:
        case Gf3GaussianActionStatus::Applied:
          break;
        }
      }
      if (gf3_general_row_application) {
        switch (gf3_general_row_application->status) {
        case Gf3GeneralRowActionStatus::Invalid:
          out.status = InvocationStatus::InvalidRequest;
          break;
        case Gf3GeneralRowActionStatus::Inapplicable:
        case Gf3GeneralRowActionStatus::UnsupportedProof:
          out.status = InvocationStatus::Inapplicable;
          break;
        case Gf3GeneralRowActionStatus::ResourceExhausted:
          out.status = InvocationStatus::ResourceExhausted;
          retain_theory_selection();
          break;
        case Gf3GeneralRowActionStatus::Interrupted:
          out.status = InvocationStatus::Interrupted;
          retain_theory_selection();
          break;
        case Gf3GeneralRowActionStatus::Stale:
          out.status = InvocationStatus::StaleRequest;
          break;
        case Gf3GeneralRowActionStatus::NoStructure:
        case Gf3GeneralRowActionStatus::NoConsequence:
        case Gf3GeneralRowActionStatus::AlreadyPublished:
        case Gf3GeneralRowActionStatus::Applied:
          break;
        }
      }
      if (tseitin_cycle_application) {
        switch (tseitin_cycle_application->status) {
        case TseitinCycleBreakingStatus::Invalid:
          out.status = InvocationStatus::InvalidRequest;
          break;
        case TseitinCycleBreakingStatus::Inapplicable:
        case TseitinCycleBreakingStatus::UnsupportedProof:
          out.status = InvocationStatus::Inapplicable;
          break;
        case TseitinCycleBreakingStatus::ResourceExhausted:
          out.status = InvocationStatus::ResourceExhausted;
          retain_theory_selection();
          break;
        case TseitinCycleBreakingStatus::Interrupted:
          out.status = InvocationStatus::Interrupted;
          retain_theory_selection();
          break;
        case TseitinCycleBreakingStatus::Stale:
          out.status = InvocationStatus::StaleRequest;
          break;
        case TseitinCycleBreakingStatus::NoStructure:
        case TseitinCycleBreakingStatus::AlreadyApplied:
        case TseitinCycleBreakingStatus::Applied:
          break;
        }
      }
      if (direct_php_application) {
        switch (direct_php_application->status) {
        case DirectPhpActionStatus::Invalid:
        case DirectPhpActionStatus::ReplayMismatch:
        case DirectPhpActionStatus::Prepared:
          out.status = InvocationStatus::InvalidRequest;
          break;
        case DirectPhpActionStatus::Inapplicable:
        case DirectPhpActionStatus::UnsupportedProof:
        case DirectPhpActionStatus::UnsupportedProofSplit:
          out.status = InvocationStatus::Inapplicable;
          break;
        case DirectPhpActionStatus::ResourceExhausted:
          out.status = InvocationStatus::ResourceExhausted;
          retain_theory_selection();
          break;
        case DirectPhpActionStatus::Interrupted:
          out.status = InvocationStatus::Interrupted;
          retain_theory_selection();
          break;
        case DirectPhpActionStatus::Stale:
          out.status = InvocationStatus::StaleRequest;
          break;
        case DirectPhpActionStatus::NotRecognized:
        case DirectPhpActionStatus::Malformed:
        case DirectPhpActionStatus::Ambiguous:
        case DirectPhpActionStatus::AlreadyApplied:
        case DirectPhpActionStatus::Applied:
          break;
        }
      }
      if (sr_application) {
        switch (sr_application->status) {
        case SubstitutionRedundancyApplicationStatus::Invalid:
          // Once an earlier batch member changes the formula, a later SR
          // precondition is judged against a different state. Treat that as
          // the same stale-snapshot boundary used by other exact batches;
          // malformed storage was already rejected before any mutation.
          if (enumerated_sr) {
            out.status = InvocationStatus::NoChange;
            consume_generated_sr_noop = true;
          } else
            out.status = runtime.pass_progress
                             ? InvocationStatus::StaleRequest
                             : InvocationStatus::InvalidRequest;
          break;
        case SubstitutionRedundancyApplicationStatus::Inapplicable:
          if (enumerated_sr &&
              runtime.sr_selection[payload].symmetry_chain_certificate) {
            // A later Schreier member is certified only for the contiguous
            // prefix selected from the same snapshot. If that obligation ever
            // fails, discard the rest of this snapshot.  A repeated step may
            // enumerate again from the strengthened live state; it must never
            // skip the failed premise and continue through the old batch.
            out.status = InvocationStatus::NoChange;
            discard_generated_sr_batch = true;
          } else if (enumerated_sr) {
            out.status = InvocationStatus::NoChange;
            consume_generated_sr_noop = true;
          } else
            out.status = InvocationStatus::Inapplicable;
          break;
        case SubstitutionRedundancyApplicationStatus::UnsupportedProofFormat:
          out.status = InvocationStatus::Inapplicable;
          break;
        case SubstitutionRedundancyApplicationStatus::Interrupted:
          out.status = InvocationStatus::Interrupted;
          break;
        case SubstitutionRedundancyApplicationStatus::ResourceExhausted:
          out.status = InvocationStatus::ResourceExhausted;
          break;
        default:
          break;
        }
        if (out.status == InvocationStatus::InvalidRequest ||
            out.status == InvocationStatus::StaleRequest ||
            out.status == InvocationStatus::Inapplicable ||
            out.status == InvocationStatus::Interrupted ||
            out.status == InvocationStatus::ResourceExhausted) {
          out.work = 0;
          out.applications = 0;
          out.selection_complete = false;
          out.budget_status = BudgetStatus::Ready;
        }
        if (consume_generated_sr_noop) {
          out.work = 0;
          out.applications = 0;
          out.budget_status = BudgetStatus::Ready;
        }
      }
      if (step.rule == "Lucky" && payload == 8 && result == ApplyResult::NoOp) {
        // An unfinished private model search is not a scoped fixpoint. Keep
        // its explicit selection pending for reconfiguration; attempted work
        // stays charged even though its private candidate state was discarded.
        if (k.stats.affine_model_interrupted != affine_interrupt_before)
          out.status = InvocationStatus::Interrupted;
        else if (k.stats.affine_model_budget != affine_budget_before) {
          out.status = InvocationStatus::BudgetExhausted;
          out.budget_status = BudgetStatus::Exhausted;
        } else if (k.stats.affine_model_unavailable != affine_unavailable_before)
          out.status = InvocationStatus::Inapplicable;
        preserve_affine_selection = out.status == InvocationStatus::Interrupted ||
            out.status == InvocationStatus::BudgetExhausted ||
            out.status == InvocationStatus::Inapplicable;
        if (preserve_affine_selection) out.selection_complete = false;
      }
      out.after = state_version(k);
      if (condition_declined) {
        // A selected cube is checked against the live formula at application
        // time. A later batch member may lose its premise after earlier
        // progress; never count or advance that stale selection.
        out.status = condition_interrupted
                         ? InvocationStatus::Interrupted
                         : runtime.pass_progress
                               ? InvocationStatus::StaleRequest
                               : InvocationStatus::Inapplicable;
        out.work = out.applications = 0;
        out.selection_complete = false;
        out.budget_status = BudgetStatus::Ready;
        return out;
      }
    }
    // Keep the selected follow-up inside the same executor transaction.  A
    // caller-side propagation here would change touch/strategy epochs after
    // expected_state was recorded and correctly make the retained batch stale.
    // The follow-up is scheduling scaffolding: only the primary rule decides
    // whether this pass made progress for repeat-to-fixpoint.
    if (step.post_application == PostApplication::Propagate &&
        out.status != InvocationStatus::Unsat &&
        out.status != InvocationStatus::Sat &&
        out.status != InvocationStatus::Conflict &&
        out.status != InvocationStatus::Interrupted &&
        out.status != InvocationStatus::ResourceExhausted &&
        propagate_guard(k)) {
      const int propagation_id = tactix_rule_id("Propagate");
      assert(propagation_id >= 0);
      const ApplyResult propagated =
          execution::execute_action(
              k, execution::RegisteredRuleRequest{
                     tactix_rules()[static_cast<size_t>(propagation_id)]});
      if (propagated == ApplyResult::Unsat) {
        out.status = InvocationStatus::Unsat;
        out.rule_result = propagated;
      } else if (propagated == ApplyResult::Conflict) {
        out.status = InvocationStatus::Conflict;
        out.rule_result = propagated;
      }
    }
    // Online execution can run with DefaultPolicy's H already attached.  Root
    // units produced directly or by the configured follow-up must leave its
    // queue and heap before the next composed action observes the state.
    heuristics_consume_root_assignments(k, trail_before);
    // A bundled follow-up is part of this composed controller action.  Publish
    // semantic advice only after that follow-up has settled the live state, so
    // its version is current.  Root propagation can make some producer values
    // non-branchable; omit those values rather than advertising advice that the
    // consumer must reject immediately.
    const auto publish_theory_candidate =
        [&](SemanticVariableProducer producer, uint64_t action_id,
            const std::vector<SemanticVariable> &variables) {
          if (k.inconsistent) return;
          std::vector<SemanticVariable> branchable;
          branchable.reserve(variables.size());
          for (const SemanticVariable variable : variables) {
            if (!variable.external ||
                variable.external >
                    static_cast<uint32_t>(std::numeric_limits<int>::max()))
              continue;
            const Var external =
                var_of(internalize(static_cast<int>(variable.external)));
            if (external >= k.external_to_internal.size()) continue;
            const Var internal = k.external_to_internal[external];
            if (internal == INVALID_VAR || internal >= k.nvars ||
                kernel_var_retired(k, internal) ||
                kernel_root_value(k, make_lit(internal, false)))
              continue;
            branchable.push_back(variable);
          }
          if (branchable.empty()) return;
          if (!stage_semantic_variable_candidate(k, producer, action_id,
                                                 branchable))
            std::abort();
          finish_semantic_variable_candidate_action(k, ApplyResult::Ok);
        };
    if (deferred_semantic_candidate)
      publish_theory_candidate(deferred_semantic_candidate->producer,
                               deferred_semantic_candidate->action_id,
                               deferred_semantic_candidate->variables);
    if (gf3_affine_application &&
        gf3_affine_application->semantic_candidate_ready)
      publish_theory_candidate(
          SemanticVariableProducer::Gf3Affine,
          gf3_affine_application->committed_action_id,
          gf3_affine_application->semantic_candidates);
    if (direct_csp_application &&
        direct_csp_application->semantic_candidate_ready)
      publish_theory_candidate(
          SemanticVariableProducer::DirectCsp,
          direct_csp_application->committed_action_id,
          direct_csp_application->semantic_variables);
    if (nary_csp_application &&
        nary_csp_application->semantic_candidate_ready)
      publish_theory_candidate(
          SemanticVariableProducer::NaryCsp,
          nary_csp_application->committed_action_id,
          nary_csp_application->semantic_variables);
    if (nary_csp_equality_application &&
        nary_csp_equality_application->semantic_candidate_ready)
      publish_theory_candidate(
          SemanticVariableProducer::NaryCspEquality,
          nary_csp_equality_application->committed_action_id,
          nary_csp_equality_application->semantic_variables);
    if (gf3_general_row_application &&
        gf3_general_row_application->status ==
            Gf3GeneralRowActionStatus::Applied) {
      std::vector<SemanticVariable> variables;
      variables.reserve(
          gf3_general_row_application->live.semantic_variables.size());
      for (const int32_t external :
           gf3_general_row_application->live.semantic_variables)
        if (external > 0)
          variables.push_back(
              {static_cast<uint32_t>(external),
               SemanticVariableRole::ExistingTheory, 1});
      publish_theory_candidate(
          SemanticVariableProducer::Gf3GeneralRow,
          gf3_general_row_application->committed_action_id, variables);
    }
    out.after = state_version(k);
    runtime.expected_state = state_version(k);
    // Generated SR payloads contain only stable literal identities. A prior
    // member may collect/rebuild clause storage while reaching root. Ordinary
    // members are checked against that strengthened formula; a certified
    // orbitopal member already carries a base automorphism that preserves its
    // complete preceding unit prefix. The expected-state check above rejects
    // external transitions. Preserve the batch instead of repeating expensive
    // structured discovery after each successful unit.
    if (generated_sr_pass ||
        !step.independent_set_cover_instances.empty() ||
        !step.affine_basis_instances.empty() ||
        !step.gf3_affine_instances.empty() ||
        !step.direct_csp_support_instances.empty() ||
        !step.nary_csp_prefix_instances.empty() ||
        !step.nary_csp_equality_instances.empty() ||
        !step.gf3_gaussian_instances.empty() ||
        !step.gf3_general_row_instances.empty())
      runtime.selection_generation = k.arena_generation;
    runtime.consumed = saturated_add(runtime.consumed, out.work);
    runtime.pass_progress |= out.made_rule_progress;
    sequence_progress_ |= out.made_rule_progress;
    if (discard_generated_sr_batch)
      runtime.cursor = runtime.selection.size();
    else if (!preserve_affine_selection && !preserve_theory_selection &&
             (out.applications || consume_generated_sr_noop))
      ++runtime.cursor;
    out.configuration_revision = revision_;
    out.step_id = step.id;
    out.after = state_version(k);
    return out;
  }
}

} // namespace tactix::strategy
