#include "strategy/online_inprocess.hpp"
#include "rules/lucky/lucky.hpp"
#include "strategy/common_parameter_serialization.hpp"

#include <algorithm>
#include <climits>
#include <charconv>
#include <limits>
#include <locale>
#include <numeric>
#include <sstream>
#include <string_view>

namespace tactix::strategy {

namespace {

const char *repeat_name(RepeatMode mode) {
  switch (mode) {
  case RepeatMode::Once:
    return "once";
  case RepeatMode::Count:
    return "count";
  case RepeatMode::UntilScopedFixpoint:
    return "fixpoint";
  case RepeatMode::UntilSizeScaledFixpoint:
    return "size-scaled-fixpoint";
  }
  return "invalid";
}

template <class First, class... Rest>
void serialize_values(std::ostream &out, const First &first,
                      const Rest &...rest) {
  out << first;
  ((out << ',' << rest), ...);
}

template <class Family, class Parameters>
void serialize_fields(std::ostream &out, const Parameters &p) {
  visit_parameter_fields(FieldsOf<Family>{}, p, [&](const auto &...fields) {
    serialize_values(out, fields...);
  });
}

void serialize_affine_basis_values(
    std::ostream &out, const AffineConsequenceParameters &p) {
  serialize_fields<AffineImageFields>(out, p);
  out << ',';
  serialize_fields<AffineBasisFields>(out, p);
}

template <class Parameters> struct Listed {
  const char *tag;
};

template <class Parameters>
bool serialize_family(std::ostream &out, const ActionParameters &parameters,
                      Listed<Parameters> family) {
  const auto *p = std::get_if<Parameters>(&parameters);
  if (p) {
    out << family.tag;
    serialize_fields<Parameters>(out, *p);
    out << ')';
  }
  return p != nullptr;
}

// Families whose stable text is their tag, their field list and ')'.
template <class... Families>
bool serialize_listed(std::ostream &out, const ActionParameters &parameters,
                      Listed<Families>... families) {
  return (serialize_family(out, parameters, families) || ...);
}

} // namespace

std::string serialize_online_strategy_program(
    const OnlineStrategyProgram &program) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << "sequence-repeat=" << repeat_name(program.repeat.mode) << ':'
      << program.repeat.limit;
  for (const OnlineInvocation &step : program.sequence) {
    out << " step=" << step.id << ':' << step.rule << ':'
        << static_cast<unsigned>(step.granularity) << ':'
        << repeat_name(step.repeat.mode) << ':' << step.repeat.limit << ':'
        << static_cast<unsigned>(step.work_unit) << ':'
        << (step.budget.is_unlimited() ? "unlimited"
                                       : std::to_string(step.budget.amount()))
        << ":post(" << static_cast<unsigned>(step.post_application) << ')'
        << ":rulework("
        << static_cast<unsigned>(step.rule_work_budget.mode) << ','
        << step.rule_work_budget.amount << ")"
        << ":material(" << static_cast<unsigned>(step.material.touch_filter)
        << ','
        << static_cast<unsigned>(step.material.likely_kept_redundant) << ')';
    if (const auto *p = std::get_if<AnalyzeParameters>(&step.parameters))
      out << ":analyze(" << p->minimize_depth_limit << ','
          << p->conflict_reuse << ',' << p->chronological_levels << ','
          << p->bump_reason_limit << ',' << p->bump_reason_depth << ','
          << p->eager_subsume << ',' << p->jump_reasons << ','
          << p->jump_reasons_bigbig << ',' << p->shrink_glue_promote << ','
          << p->bump_extension_reasons << ')';
    else if (serialize_common_parameters(
                 out, step.parameters,
                 CommonParameterFormat::OnlineProgram)) {
      // Serialized by the common stable-text owner.
    }
    else if (const auto *p =
                 std::get_if<AffineConsequenceParameters>(&step.parameters)) {
      if (p->semantics == AffineConsequenceSemantics::SelectedConsequenceV1) {
        out << ":affine-consequence1(";
        serialize_fields<AffineImageFields>(out, *p);
        out << ',' << p->consequence << ')';
      } else {
        out << ":affine-consequence2(";
        serialize_affine_basis_values(out, *p);
        out << ')';
      }
    }
    else if (const auto *profile =
                 std::get_if<AffineMatrixInitializeParameters>(
                     &step.parameters)) {
      out << ":affine-matrix-persistent1(";
      serialize_affine_basis_values(out, profile->basis);
      out << ',' << static_cast<unsigned>(profile->admission) << ','
          << static_cast<unsigned>(profile->operation) << ','
          << profile->maximum_retained_bytes << ','
          << profile->maximum_deferred_deletions << ','
          << profile->maximum_deferred_literal_work << ')';
    }
    else if (serialize_listed(
                 out, step.parameters,
                 Listed<ExactCoverExtensionParameters>{":exact-cover-extension1("},
                 Listed<IndependentSetCoverParameters>{":independent-set-cover1("},
                 Listed<Gf3AffineConsequenceParameters>{":gf3-affine-consequence1("},
                 Listed<DirectCspGuardedCoverParameters>{":direct-csp-guarded-cover2("},
                 Listed<DirectCspSupportParameters>{":direct-csp-support1("},
                 Listed<NaryCspEqualityParameters>{":nary-csp-equality1("},
                 Listed<NaryCspEqualityTransitivityParameters>{
                     ":nary-csp-equality-transitivity1("},
                 Listed<NaryCspEqualityAmoParameters>{":nary-csp-equality-amo1("},
                 Listed<DirectPhpRefutationParameters>{":direct-php-refutation1("},
                 Listed<Gf3GaussianConsequenceParameters>{
                     ":gf3-gaussian-consequence1("},
                 Listed<Gf3GeneralRowConsequenceParameters>{":gf3-general-row1("},
                 Listed<TseitinCycleBreakingParameters>{":tseitin-cycle-breaking1("},
                 Listed<SemanticVariableAdviceParameters>{
                     ":semantic-variable-advice1("},
                 Listed<BalancedSignatureModelParameters>{
                     ":balanced-signature-model("},
                 Listed<FermatModelParameters>{":fermat-model("},
                 Listed<LookupCircuitModelParameters>{":lookup-circuit-model("},
                 Listed<SimonModelParameters>{":simon-model("},
                 Listed<StconnProofParameters>{":stconn-proof("},
                 Listed<SignedCardinalityProofParameters>{
                     ":signed-cardinality-proof("},
                 Listed<ExactCoverModelParameters>{":exact-cover-model("},
                 Listed<IndependentSetModelParameters>{":independent-set-model("})) {
      // Serialized from the family's shared field list.
    }
    else if (const auto *p = std::get_if<CardinalitySearchActionLimits>(&step.parameters))
      out << ":cardinality-search1(" << p->inspection.maximum_support_variables
          << ',' << p->inspection.maximum_constraints << ','
          << p->inspection.maximum_source_clauses << ','
          << p->inspection.maximum_reason_width << ','
          << p->inspection.maximum_work << ',' << p->resolution.maximum_work
          << ',' << p->resolution.maximum_additions << ','
          << p->resolution.maximum_added_literals << ','
          << p->maximum_source_scan_work << ',' << p->maximum_arena_words
          << ',' << p->maximum_journal_bytes << ')';
    else if (const auto *p = std::get_if<BceParameters>(&step.parameters))
      out << ":bce(" << p->occurrence_limit << ')';
    else if (const auto *p =
                 std::get_if<TernaryParameters>(&step.parameters))
      out << ":ternary(" << p->occurrence_limit << ','
          << p->maximum_add_percent << ')';
    else if (const auto *p =
                 std::get_if<FactorParameters>(&step.parameters))
      out << ":factor(" << p->maximum_clause_size << ',' << p->growth_bound
          << ',' << p->candidate_rounds << ',' << p->composite << ','
          << p->demote_fresh << ')';
    else if (const auto *p =
                 std::get_if<TransitiveReduceParameters>(&step.parameters))
      out << ":transitive(" << p->early_exit << ',' << p->live_meter << ','
          << p->checked_budget_meter << ')';
    else if (const auto *p =
                 std::get_if<SubstituteParameters>(&step.parameters))
    {
      out << ":substitute(" << p->demote_substituted;
      if (p->max_rounds || p->round_effort_ceiling)
        out << ',' << p->max_rounds << ',' << p->round_effort_ceiling;
      out << ')';
    }
    else if (const auto *p =
                 std::get_if<ReduceParameters>(&step.parameters))
      out << ":reduce(" << p->tier1 << ',' << p->tier2 << ','
          << p->victim_numerator << ',' << p->victim_denominator << ','
          << p->protect_tiers << ',' << p->compact_collection << ','
          << p->reduction_bookkeeping << ')';
    else if (const auto *p = std::get_if<WalkParameters>(&step.parameters)) {
      const bool split = p->objective == WalkParameters::Objective::FullCurrentCnf;
      const bool configured =
          p->algorithm != WalkParameters::Algorithm::ProbSat ||
          p->initialization != WalkParameters::Initialization::BestPhases;
      if (configured) {
        out << ":walk4(" << p->step_budget << ',' << p->random_seed << ','
            << uint32_t(p->algorithm) << ',' << uint32_t(p->initialization)
            << ',' << p->maximum_tries << ',' << p->flips_per_try << ','
            << p->weight_smoothing_threshold << ','
            << p->smoothing_old_numerator << ',' << p->smoothing_denominator
            << ',' << p->aspiration << ',' << p->even_break_base_milli << ','
            << p->score_curve.size();
        for (const WalkParameters::ScorePoint point : p->score_curve)
          out << ',' << point.average_clause_size_milli << ','
              << point.break_base_milli;
        out << ')';
      } else {
        out << (split ? ":walk3(" : p->seed_candidate ? ":walk2(" : ":walk(")
            << p->step_budget << ',' << p->random_seed << ',';
        if (p->seed_candidate) out << p->seed_candidate << ',';
        if (split) out << p->preparation_budget << ',' << uint32_t(p->objective)
                       << ',' << uint32_t(p->admission) << ',';
        out << p->even_break_base_milli << ',' << p->score_curve.size();
        for (const WalkParameters::ScorePoint point : p->score_curve)
          out << ',' << point.average_clause_size_milli << ','
              << point.break_base_milli;
        out << ')';
      }
    }
    else if (const auto *p =
                 std::get_if<ReorderParameters>(&step.parameters))
      out << ":reorder(" << p->maximum_clause_size << ','
          << p->clause_decay_milli << ',' << p->binary_weight_milli << ','
          << p->balanced_polarity_milli << ')';
    else if (const auto *p =
                 std::get_if<ParityModelParameters>(&step.parameters))
      out << ":parity-model2(" << p->maximum_variables << ','
          << p->maximum_groups << ',' << p->maximum_constraints << ','
          << p->minimum_group_clauses << ',' << p->minimum_equations << ','
          << p->maximum_free_variables << ',' << p->candidate_budget << ','
          << p->refutation_maximum_variables << ','
          << p->refutation_maximum_groups << ','
          << p->refutation_maximum_constraints << ','
          << p->refutation_maximum_arity << ','
          << p->refutation_probe_constraints << ','
          << p->refutation_minimum_probe_repeats << ','
          << p->refutation_maximum_proof_additions << ')';
    else if (const auto *p =
                 std::get_if<OnehotModelParameters>(&step.parameters))
      out << ":onehot-model2(" << p->maximum_variables << ','
          << p->maximum_domains << ',' << p->maximum_constraints << ','
          << p->maximum_equation_arity << ',' << p->minimum_domains << ','
          << p->minimum_equations << ',' << p->maximum_free_domains << ','
          << p->candidate_budget << ','
          << p->refutation_maximum_variables << ','
          << p->refutation_maximum_groups << ','
          << p->refutation_maximum_constraints << ','
          << p->refutation_maximum_arity << ','
          << p->refutation_maximum_proof_additions << ')';
    else if (const auto *p = std::get_if<RamseyStartupParameters>(&step.parameters))
      out << ":ramsey-startup1(" << p->recognition_clauses << "," << p->recognition_variables
          << "," << p->recognition_pair_cells << "," << p->recognition_work
          << "," << p->certificate_work << "," << p->maximum_nodes
          << "," << p->maximum_additions << "," << p->maximum_external_extent << ")";
    else if (const auto *p = std::get_if<GraphLabelingParameters>(&step.parameters)) {
      out << ":graph-labeling1(" << p->maximum_vertices << ',' << p->workspace_bytes
          << ',' << p->maximum_dfs_vertices << ',' << p->maximum_dfs_nodes
          << ',' << p->maximum_dfs_starts << ',' << p->early_dfs_choices
          << ',' << p->repair_rounds << ',' << p->anneal_iterations
          << ',' << p->stall_anneal_iterations << ',' << p->seed;
      for (uint64_t allowance : p->work) out << ',' << allowance;
      out << ')';
    }
    else if (const auto *p = std::get_if<AffineModelParameters>(&step.parameters)) {
      out << ":affine1(" << p->maximum_variables << ',' << p->maximum_constraints
          << ',' << p->maximum_arity << ',' << p->maximum_projection << ','
          << p->minimum_equations << ',' << p->maximum_entries << ','
          << p->import_bytes << ',' << p->linear_bytes << ',' << p->table_bytes << ','
          << p->maximum_work << ',' << p->candidate_budget << ',' << p->permutations
          << ',' << p->seed << ',' << static_cast<unsigned>(p->projection_mode)
          << ',' << p->automatic_negated << ',' << p->shuffle_first << ','
          << p->free_phase << ',' << p->residual_phase << ',' << p->projection.size();
      for(int literal:p->projection) {
        const uint64_t positive=static_cast<uint64_t>(literal<0?-int64_t(literal):int64_t(literal));
        out << ',' << 2*(positive-1)+(literal<0);
      }
      out << ',' << p->scopes.size();
      for(const auto &scope:p->scopes)
        out << ',' << scope.left_size << ',' << scope.left_weight << ','
            << scope.right_weight << ',' << scope.total_weight;
      out << ')';
    }
    else if (const auto *p = std::get_if<SubstitutionRedundancyParameters>(
                 &step.parameters)) {
      const bool bounded_schreier =
          p->graph_symmetry.schreier_domain_limit != UINT64_MAX;
      const bool nonforcing_cut_admission =
          p->graph_symmetry.nonforcing_cut_admission !=
          SymmetryNonforcingCutAdmission::Publish;
      const bool fix_anchor = p->graph_symmetry.first_literal_fix_anchor;
      const bool chain_order =
          p->graph_symmetry.cut_point != SymmetryCutPoint::FirstOpen ||
          p->graph_symmetry.negation_fixing_first || fix_anchor;
      const bool first_shortest = p->graph_symmetry.base_selection ==
          SymmetryBaseSelection::FirstShortestThenForwardRemainder;
      const bool shortest_base = first_shortest ||
          p->graph_symmetry.base_selection == SymmetryBaseSelection::ShortestClause;
      out << (fix_anchor ? ":sr30(" : chain_order ? ":sr29(" : nonforcing_cut_admission ? ":sr28(" : p->orbitopal_progress_feedback ? ":sr27(" : bounded_schreier ? ":sr26(" : first_shortest ? ":sr25(" : shortest_base ? ":sr24("
                  : p->graph_symmetry.structured_scope == SymmetryStructuredScope::WholeMatrix
                  ? ":sr22(" : ":sr23(") << p->maximum_validation_ticks << ','
          << static_cast<unsigned>(p->producer) << ','
          << static_cast<unsigned>(p->orbitopal_orientation) << ','
          << p->minimum_orbitopal_rows << ','
          << p->minimum_orbitopal_columns << ','
          << p->maximum_orbitopal_matrices << ','
          << p->maximum_orbitopal_instances << ','
          << p->maximum_detection_ticks << ','
          << p->maximum_general_candidates_per_advance << ','
          << p->graph_symmetry.maximum_graph_vertices << ','
          << p->graph_symmetry.maximum_graph_edges << ','
          << p->graph_symmetry.maximum_generators << ','
          << p->graph_symmetry.backend_search_budget << ','
          << p->graph_symmetry.backend_support_budget << ','
          << p->graph_symmetry.backend_component_size_limit << ','
          << p->graph_symmetry.maximum_fixing_instances << ','
          << p->graph_symmetry.maximum_generator_instances_per_pass << ','
          << p->graph_symmetry.maximum_group_ticks << ','
          << p->graph_symmetry.seed << ','
          << p->graph_symmetry.error_bound << ','
          << static_cast<unsigned>(p->graph_symmetry.fixing_mode) << ','
          << p->graph_symmetry.decompose << ','
          << p->graph_symmetry.prefer_depth_first << ','
          << p->graph_symmetry.strong_certification << ','
          << static_cast<unsigned>(p->graph_symmetry.generator_order) << ','
          << static_cast<unsigned>(p->graph_symmetry.clause_order) << ','
          << static_cast<unsigned>(p->graph_symmetry.literal_order) << ','
          << static_cast<unsigned>(p->graph_symmetry.clause_scope) << ','
          << static_cast<unsigned>(p->graph_symmetry.pair_orientation) << ','
          << p->graph_symmetry.direct_generator_involutions_only << ','
          << static_cast<unsigned>(p->orbitopal_column_order) << ','
          << p->maximum_orbitopal_clique_vertices << ','
          << p->maximum_orbitopal_clique_ticks << ','
          << p->minimum_orbitopal_clique_columns << ','
          << static_cast<unsigned>(p->orbitopal_completion) << ','
          << p->maximum_orbitopal_product_states << ','
          << p->complete_unique_literal_amos << ','
          << p->orbitopal_generator_fallback << ','
          << p->maximum_orbitopal_refinement_orbits << ','
          << p->orbitopal_base_probe_filter << ','
          << p->orbitopal_base_probe_factor << ','
          << static_cast<unsigned>(p->orbitopal_discovery_order) << ','
          << static_cast<unsigned>(p->graph_symmetry.binary_graph_mode) << ','
          << p->graph_symmetry.binary_graph_density_permille << ','
          << static_cast<unsigned>(p->graph_symmetry.clause_selection) << ','
          << static_cast<unsigned>(p->graph_symmetry.chain_mode) << ','
          << static_cast<unsigned>(p->graph_symmetry.base_selection) << ','
          << p->graph_symmetry.pointwise_root_stabilizer << ','
          << static_cast<unsigned>(p->orbitopal_row_orbit_source) << ','
          << p->graph_symmetry.chain_planning_tick_limit << ','
          << p->graph_symmetry.factor_support_components << ','
          << p->graph_symmetry.preserve_detector_order << ','
          << p->graph_symmetry.use_structured_generators << ','
          << p->graph_symmetry.stabilize_structured_variables;
      if (chain_order || nonforcing_cut_admission || p->orbitopal_progress_feedback || bounded_schreier || shortest_base ||
          p->graph_symmetry.structured_scope != SymmetryStructuredScope::WholeMatrix)
        out << ',' << static_cast<unsigned>(p->graph_symmetry.structured_scope);
      if (chain_order || nonforcing_cut_admission || p->orbitopal_progress_feedback || bounded_schreier)
        out << ',' << p->graph_symmetry.schreier_domain_limit;
      if (chain_order || nonforcing_cut_admission || p->orbitopal_progress_feedback)
        out << ',' << p->orbitopal_progress_feedback;
      if (chain_order || nonforcing_cut_admission)
        out << ',' << static_cast<unsigned>(
            p->graph_symmetry.nonforcing_cut_admission);
      if (chain_order)
        out << ',' << static_cast<unsigned>(p->graph_symmetry.cut_point) << ','
            << p->graph_symmetry.negation_fixing_first;
      if (fix_anchor)
        out << ',' << p->graph_symmetry.first_literal_fix_anchor;
      out << ')';
    }
    else
      out << ":inherit";
    out << ":instances[";
    for (size_t i = 0; i < step.instances.size(); ++i) {
      if (i)
        out << ',';
      out << step.instances[i];
    }
    out << "]:grids[";
    for (size_t g = 0; g < step.factor_grids.size(); ++g) {
      if (g)
        out << ';';
      const FactorGrid &grid = step.factor_grids[g];
      out << grid.factors.size();
      for (const Lit lit : grid.factors)
        out << ',' << lit;
      out << ',' << grid.remainders.size();
      for (const auto &remainder : grid.remainders) {
        out << ',' << remainder.size();
        for (const Lit lit : remainder)
          out << ',' << lit;
      }
      out << ',' << grid.members.size();
      for (const Ref member : grid.members)
        out << ',' << member;
    }
    out << "]:sr[";
    for (size_t s = 0; s < step.substitution_redundancy_instances.size(); ++s) {
      if (s)
        out << ';';
      const SubstitutionRedundancyInstance &instance =
          step.substitution_redundancy_instances[s];
      out << instance.clause.size();
      for (const Lit lit : instance.clause)
        out << ',' << lit;
      out << ',' << instance.witness.size();
      for (const Lit lit : instance.witness)
        out << ',' << lit;
      out << ',' << instance.substitution.size();
      for (const auto &[source, target] : instance.substitution)
        out << ',' << source << ',' << target;
    }
    out << ']';
    if (!step.condition_instances.empty()) {
      out << ":condition[";
      for (size_t c = 0; c < step.condition_instances.size(); ++c) {
        if (c)
          out << ';';
        const ConditionInstance &instance = step.condition_instances[c];
        out << instance.clause.size();
        for (const Lit lit : instance.clause)
          out << ',' << lit;
        out << ',' << instance.witness.size();
        for (const Lit lit : instance.witness)
          out << ',' << lit;
      }
      out << ']';
    }
    if (!step.independent_set_cover_instances.empty()) {
      out << ":independent-set-cover[";
      for (size_t i = 0; i < step.independent_set_cover_instances.size(); ++i) {
        if (i) out << ';';
        const auto &cover = step.independent_set_cover_instances[i];
        out << cover.size();
        for (const uint32_t external : cover) out << ',' << external;
      }
      out << ']';
    }
    if (!step.gf3_affine_instances.empty()) {
      out << ":gf3-affine-consequence[";
      for (size_t i = 0; i < step.gf3_affine_instances.size(); ++i) {
        if (i) out << ';';
        out << step.gf3_affine_instances[i].first_equation << ','
            << step.gf3_affine_instances[i].second_equation;
      }
      out << ']';
    }
    if (!step.affine_basis_instances.empty()) {
      out << ":affine-basis-row1[";
      for (size_t row = 0; row < step.affine_basis_instances.size(); ++row) {
        if (row) out << ';';
        const auto &selection = step.affine_basis_instances[row];
        out << selection.owner_version << ','
            << selection.elimination_profile << ','
            << static_cast<unsigned>(selection.kind) << ','
            << selection.variables.size();
        for (const int32_t variable : selection.variables)
          out << ',' << variable;
        out << ',' << selection.rhs << ','
            << selection.source_equation_ordinals.size();
        for (const size_t ordinal : selection.source_equation_ordinals)
          out << ',' << ordinal;
        out << ',' << selection.source_equations.size();
        for (const auto &equation : selection.source_equations) {
          out << ',' << equation.variables.size();
          for (const int32_t variable : equation.variables)
            out << ',' << variable;
          out << ',' << equation.rhs << ',' << equation.premises.size();
          for (const auto &premise : equation.premises) {
            out << ',' << premise.native_clause_id << ','
                << premise.clause.size();
            for (const int32_t literal : premise.clause)
              out << ',' << literal;
          }
        }
      }
      out << ']';
    }
    if (!step.direct_csp_guarded_cover_instances.empty()) {
      out << ":direct-csp-guarded-cover2[";
      for (size_t i=0; i<step.direct_csp_guarded_cover_instances.size(); ++i) {
        if (i) out << ';';
        const auto &v = step.direct_csp_guarded_cover_instances[i];
        out << v.domains.size();
        for (size_t d=0; d<v.domains.size(); ++d) {
          out << ',' << v.domains[d].size();
          for (auto l : v.domains[d]) out << ',' << l;
          out << ',' << v.selected_values[d];
        }
        out << ',' << v.cover.size(); for (auto l : v.cover) out << ',' << l;
        out << ',' << v.exact_or.size();
        for (const auto &o : v.exact_or) {
          out << ',' << o.output << ',' << o.inputs.size();
          for (auto l : o.inputs) out << ',' << l;
        }
        out << ',' << v.blockers.size();
        for (const auto &b : v.blockers)
          out << ',' << b.literal << ',' << b.domain << ',' << b.value << ',' << b.or_definition;
      }
      out << ']';
    }
    if (!step.direct_csp_support_instances.empty()) {
      out << ":direct-csp-support[";
      for (size_t i = 0; i < step.direct_csp_support_instances.size(); ++i) {
        if (i) out << ';';
        const auto &instance = step.direct_csp_support_instances[i];
        out << instance.left_values.size();
        for (const int32_t literal : instance.left_values) out << ',' << literal;
        out << ',' << instance.right_values.size();
        for (const int32_t literal : instance.right_values) out << ',' << literal;
        out << ',' << instance.selected_left_value;
      }
      out << ']';
    }
    if (!step.nary_csp_equality_instances.empty()) {
      out << ":nary-csp-equality[";
      for (size_t i = 0; i < step.nary_csp_equality_instances.size(); ++i) {
        if (i) out << ';';
        const auto &instance = step.nary_csp_equality_instances[i];
        out << instance.domains.size();
        for (const auto &domain : instance.domains) {
          out << ',' << domain.size();
          for (const int32_t literal : domain) out << ',' << literal;
        }
        out << ',' << instance.coordinates.left_domain << ','
            << instance.coordinates.right_domain << ','
            << instance.coordinates.residual_domain;
      }
      out << ']';
    }
    if (!step.nary_csp_equality_transitivity_instances.empty()) {
      out << ":nary-csp-equality-transitivity[";
      for(size_t i=0;i<step.nary_csp_equality_transitivity_instances.size();++i) {
        if(i) out << ';';
        const auto &v=step.nary_csp_equality_transitivity_instances[i];
        bool first=true;
        const auto domain=[&](const auto &d) {
          if(!first) out << ',';
          first=false;
          out << d.size(); for(auto l:d) out << ',' << l;
        };
        for(const auto &d:v.domains) domain(d);
        for(size_t e=0;e<3;++e) {
          const auto &key=v.equalities[e]; for(const auto &d:key.domains) domain(d);
          out << ',' << key.first_equal_domain << ',' << key.second_equal_domain << ',' << key.residual_domain << ',' << v.equality_variables[e];
        }
      }
      out << ']';
    }
    if (!step.nary_csp_equality_amo_instances.empty()) {
      out << ":nary-csp-equality-amo[";
      for (size_t i = 0; i < step.nary_csp_equality_amo_instances.size(); ++i) {
        if (i) out << ';';
        const auto &instance = step.nary_csp_equality_amo_instances[i];
        out << instance.domains.size();
        for (const auto &domain : instance.domains) {
          out << ',' << domain.size();
          for (int32_t literal : domain) out << ',' << literal;
        }
      }
      out << ']';
    }
    if (!step.gf3_gaussian_instances.empty()) {
      out << ":gf3-gaussian-consequence[";
      for (size_t i = 0; i < step.gf3_gaussian_instances.size(); ++i) {
        if (i) out << ';';
        out << step.gf3_gaussian_instances[i];
      }
      out << ']';
    }
    if (!step.gf3_general_row_instances.empty()) {
      out << ":gf3-general-row[";
      for (size_t i = 0; i < step.gf3_general_row_instances.size(); ++i) {
        if (i) out << ';';
        const auto &selection = step.gf3_general_row_instances[i];
        out << selection.outcome << ',' << selection.tuple << ','
            << selection.identity;
      }
      out << ']';
    }
    if (!step.cardinality_search_instances.empty()) {
      out << ":cardinality-search[";
      const auto &bounds = step.cardinality_search_instances.front();
      out << bounds.size();
      for (const auto &bound : bounds) {
        out << ',' << bound.literals.size() << ',' << bound.lower << ','
            << bound.upper;
        for (const int32_t literal : bound.literals) out << ',' << literal;
      }
      out << ']';
    }
    if (!step.tseitin_cycle_instances.empty()) {
      out << ":tseitin-cycle-breaking[";
      for (size_t i = 0; i < step.tseitin_cycle_instances.size(); ++i) {
        if (i) out << ';';
        out << step.tseitin_cycle_instances[i];
      }
      out << ']';
    }
  }
  return out.str();
}

} // namespace tactix::strategy
