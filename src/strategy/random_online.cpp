#include "strategy/random_online.hpp"
#include "rules/lucky/lucky.hpp"

#include <algorithm>
#include <climits>
#include <charconv>
#include <limits>
#include <numeric>
#include <sstream>
#include <string_view>

namespace tactix::strategy {

namespace {

constexpr uint64_t RNG_MULTIPLIER = 0x2545F4914F6CDD1Dull;
constexpr uint64_t LIMIT_SHAPES = 5;
constexpr uint64_t OPTIONAL_CHOICES = 2;
constexpr uint64_t BUDGET_SHAPES = 4;
constexpr uint64_t GRANULARITY_CHOICES = 4;
constexpr uint64_t REPEAT_CHOICES = 3;

uint64_t draw(uint64_t &state) {
  state ^= state >> 12;
  state ^= state << 25;
  state ^= state >> 27;
  return state * RNG_MULTIPLIER;
}

uint64_t bounded(uint64_t &state, uint64_t limit) {
  return limit ? draw(state) % limit : 0;
}

uint32_t choice_limit(uint64_t &state, uint32_t reference) {
  switch (bounded(state, LIMIT_SHAPES)) {
  case 0:
    return 0;
  case 1:
    return 1;
  case 2:
    return reference;
  case 3:
    return reference == UINT32_MAX ? reference : reference + 1;
  default:
    return UINT32_MAX;
  }
}

uint32_t inclusive_u32(uint64_t &state, uint32_t maximum) {
  return maximum == UINT32_MAX
             ? static_cast<uint32_t>(draw(state))
             : static_cast<uint32_t>(bounded(state,
                                             uint64_t{maximum} + 1));
}

uint64_t optional_positive(uint64_t &state, uint64_t maximum) {
  if (!maximum || !bounded(state, OPTIONAL_CHOICES))
    return 0;
  return 1 + bounded(state, maximum);
}

bool executable_exact_cover_profile(
    const ExactCoverExtensionParameters &parameters) {
  return parameters.valid() && parameters.arity && parameters.maximum_items &&
         parameters.maximum_source_clauses &&
         parameters.maximum_source_literals &&
         parameters.maximum_recognition_work &&
         parameters.maximum_premise_references &&
         parameters.maximum_definitions && parameters.maximum_additions &&
         parameters.maximum_literal_occurrences &&
         parameters.maximum_extent_growth &&
         parameters.maximum_external_extent &&
         parameters.maximum_journal_bytes;
}

ActionParameters random_parameters(const Kernel &k, uint64_t &state, const std::string &rule,
                                   const RandomOnlineConfig &config) {
  if (rule == "ConflictAnalysis") {
    AnalyzeParameters p = config.analyze_reference;
    p.minimize_depth_limit =
        choice_limit(state, p.minimize_depth_limit);
    p.conflict_reuse = bounded(state, OPTIONAL_CHOICES) != 0;
    p.chronological_levels = choice_limit(state, p.chronological_levels);
    p.bump_reason_limit =
        inclusive_u32(state, config.maximum_analyze_bump_reason_limit);
    p.bump_reason_depth =
        inclusive_u32(state, config.maximum_analyze_bump_reason_depth);
    p.eager_subsume = static_cast<uint32_t>(
        bounded(state, AnalyzeParameters::EAGER_SUBSUME_CAPACITY + 1));
    p.jump_reasons = bounded(state, OPTIONAL_CHOICES) != 0;
    p.jump_reasons_bigbig = bounded(state, OPTIONAL_CHOICES) != 0;
    p.shrink_glue_promote = bounded(state, OPTIONAL_CHOICES) != 0;
    p.bump_extension_reasons = bounded(state, OPTIONAL_CHOICES) != 0;
    return p;
  }
  if (rule == "BVE") {
    BveParameters p;
    p.gate_mask = static_cast<uint32_t>(draw(state));
    p.growth_bound = inclusive_u32(state, config.maximum_bve_growth_bound);
    p.occurrence_limit =
        choice_limit(state, config.bve_occurrence_reference);
    p.clause_size_limit =
        choice_limit(state, config.bve_clause_size_reference);
    p.resolvent_count_limit =
        choice_limit(state, config.bve_resolvent_count_reference);
    p.xor_arity_limit = BVE_XOR_ARITY_MIN +
                        static_cast<uint32_t>(bounded(
                            state, BVE_XOR_ARITY_CAPACITY -
                                       BVE_XOR_ARITY_MIN + 1));
    p.resolvent_size_limit = choice_limit(
        state, BveParameters::RESOLVENT_SIZE_DEFAULT);
    p.backward_occurrence_limit = choice_limit(
        state, BveParameters::BACKWARD_OCCURRENCE_DEFAULT);
    p.definition_budget = optional_positive(
        state, config.maximum_bve_definition_budget);
    p.definition_cores = 1 + static_cast<uint32_t>(
                                 bounded(state,
                                         config.maximum_bve_definition_cores));
    p.definition_core_budget_multiplier = inclusive_u32(
        state, config.maximum_bve_definition_core_budget_multiplier);
    p.backward = bounded(state, OPTIONAL_CHOICES) != 0;
    // Keep every established draw stable while giving the new independent
    // switch both fuzz arms.
    uint64_t otf_state = state ^ 0x4f54464256455353ull;
    p.on_the_fly_strengthening = bounded(otf_state, OPTIONAL_CHOICES) != 0;
    p.candidate_scope = static_cast<BveCandidateScope>(bounded(state, 2));
    p.traversal = static_cast<BveTraversal>(bounded(state, 2));
    // A derived stream: the draws above and every later one stay unchanged.
    uint64_t closure_state = state ^ 0x50555245434c4f53ull;
    if (p.candidate_scope == BveCandidateScope::PureOnly &&
        bounded(closure_state, 2))
      p.candidate_scope = BveCandidateScope::PureClosure;
    return p;
  }
  if (rule == "SubsumeForward")
    return SubsumeForwardParameters{
        choice_limit(state, config.subsume_reference.clause_size_limit),
        choice_limit(state, config.subsume_reference.large_occurrence_limit),
        choice_limit(state, config.subsume_reference.binary_occurrence_limit)};
  if (rule == "Strengthen")
    return StrengthenParameters{
        choice_limit(state, config.strengthen_reference.clause_size_limit),
        choice_limit(state, config.strengthen_reference.occurrence_limit)};
  if (rule == "ParityConsequence") {
    auto p = config.parity_consequence_reference;
    // Keep the caller's joint effort profile; vary only pivot direction and
    // the selected ordinal within its explicitly supplied range.
    p.descending = bounded(state, 2) != 0;
    p.consequence = p.consequence == UINT64_MAX ? draw(state)
                       : bounded(state, p.consequence + 1);
    return p;
  }
  if (rule == "AffineConsequence" || rule == "AffineMatrixInitialize")
    return config.affine_consequence_reference;
  if (rule == "ExactCoverExtension")
    return config.exact_cover_extension_reference;
  if (rule == "IndependentSetCover")
    return config.independent_set_cover_reference;
  if (rule == "Gf3AffineConsequence")
    return config.gf3_affine_consequence_reference;
  if (rule == "DirectCspSupport")
    return config.direct_csp_support_reference;
  if (rule == "NaryCspEquality")
    return config.nary_csp_equality_reference;
  if (rule == "NaryCspEqualityAmo")
    return config.nary_csp_equality_amo_reference;
  if (rule == "DirectPhpRefutation")
    return config.direct_php_refutation_reference;
  if (rule == "Gf3GaussianConsequence")
    return config.gf3_gaussian_consequence_reference;
  if (rule == "Gf3GeneralRowConsequence")
    return config.gf3_general_row_consequence_reference;
  if (rule == "SemanticVariableAdvice")
    return config.semantic_variable_advice_reference;
  if (rule == "Congruence") {
    CongruenceParameters p;
    p.and_arity_limit =
        choice_limit(state, config.congruence_reference.and_arity_limit);
    p.gate_count_limit = static_cast<size_t>(optional_positive(
        state, config.maximum_congruence_gate_count));
    p.xor_arity_limit = static_cast<uint32_t>(
        bounded(state, CongruenceParameters::XOR_ARITY_CAPACITY + 1));
    return p;
  }
  if (rule == "Instantiate") {
    InstantiateParameters p = config.instantiate_reference;
    p.minimum_clause_size =
        choice_limit(state, config.instantiate_reference.minimum_clause_size);
    p.occurrence_limit =
        choice_limit(state, config.instantiate_reference.occurrence_limit);
    p.minimum_unassigned =
        choice_limit(state, config.instantiate_reference.minimum_unassigned);
    p.include_redundant = bounded(state, OPTIONAL_CHOICES) != 0;
    p.new_material_threshold = choice_limit(
        state, config.instantiate_reference.new_material_threshold);
    return p;
  }
  if (rule == "Backbone") {
    BackboneParameters p;
    p.initial_rounds =
        choice_limit(state, config.backbone_reference.initial_rounds);
    p.maximum_rounds =
        choice_limit(state, config.backbone_reference.maximum_rounds);
    return p;
  }
  if (rule == "Sweep") {
    SweepParameters p = config.sweep_reference;
    p.max_depth = choice_limit(state, p.max_depth);
    p.max_depth_ceiling = choice_limit(state, p.max_depth_ceiling);
    p.max_env_vars = choice_limit(state, p.max_env_vars);
    p.max_env_vars_ceiling = choice_limit(state, p.max_env_vars_ceiling);
    p.max_env_clauses = choice_limit(state, p.max_env_clauses);
    p.max_env_clauses_ceiling =
        choice_limit(state, p.max_env_clauses_ceiling);
    p.max_candidates = choice_limit(state, p.max_candidates);
    p.env_ticks = optional_positive(state, p.env_ticks);
    p.max_proof_steps = choice_limit(state, p.max_proof_steps);
    p.max_seeds = choice_limit(state, p.max_seeds);
    p.round_ticks = optional_positive(state, p.round_ticks);
    p.strip_false = bounded(state, OPTIONAL_CHOICES) != 0;
    p.measure_closure = bounded(state, OPTIONAL_CHOICES) != 0;
    p.full_partition = bounded(state, OPTIONAL_CHOICES) != 0;
    p.skip_embedded_fixed = bounded(state, OPTIONAL_CHOICES) != 0;
    p.flip_rounds = choice_limit(state, p.flip_rounds);
    p.resume = bounded(state, OPTIONAL_CHOICES) != 0;
    p.force_general_proof = bounded(state, OPTIONAL_CHOICES) != 0;
    return p;
  }
  if (rule == "Vivify") {
    VivifyParameters p = config.vivify_reference;
    p.scope = static_cast<uint8_t>(bounded(state, 6));
    p.reuse = bounded(state, OPTIONAL_CHOICES) != 0;
    p.flush = bounded(state, OPTIONAL_CHOICES) != 0;
    p.irredundant_sort_ratio = 1 + static_cast<uint32_t>(bounded(
        state, config.vivify_reference.irredundant_sort_ratio));
    return p;
  }
  if (rule == "BCE")
    return BceParameters{
        choice_limit(state, config.bce_reference.occurrence_limit)};
  if (rule == "HyperTernaryRes")
    return TernaryParameters{
        choice_limit(state, config.ternary_reference.occurrence_limit),
        choice_limit(state, config.ternary_reference.maximum_add_percent)};
  if (rule == "Factor") {
    FactorParameters p;
    p.maximum_clause_size =
        choice_limit(state, config.factor_reference.maximum_clause_size);
    p.growth_bound =
        choice_limit(state, config.factor_reference.growth_bound);
    p.candidate_rounds =
        choice_limit(state, config.factor_reference.candidate_rounds);
    p.composite = bounded(state, OPTIONAL_CHOICES) != 0;
    p.demote_fresh = bounded(state, OPTIONAL_CHOICES) != 0;
    return p;
  }
  if (rule == "TransitiveReduce")
    return TransitiveReduceParameters{
        bounded(state, OPTIONAL_CHOICES) != 0,
        bounded(state, OPTIONAL_CHOICES) != 0,
        bounded(state, OPTIONAL_CHOICES) != 0};
  if (rule == "Substitute")
    return SubstituteParameters{bounded(state, OPTIONAL_CHOICES) != 0};
  if (rule == "Reduce") {
    ReduceParameters p = config.reduce_reference;
    const uint32_t first =
        choice_limit(state, config.reduce_reference.tier1);
    const uint32_t second =
        choice_limit(state, config.reduce_reference.tier2);
    p.tier1 = std::min(first, second);
    p.tier2 = std::max(first, second);
    p.victim_denominator =
        1 + inclusive_u32(state, p.victim_denominator - 1);
    p.victim_numerator = inclusive_u32(state, p.victim_denominator);
    p.protect_tiers = bounded(state, OPTIONAL_CHOICES) != 0;
    p.compact_collection = bounded(state, OPTIONAL_CHOICES) != 0;
    p.reduction_bookkeeping = bounded(state, OPTIONAL_CHOICES) != 0;
    return p;
  }
  if (rule == "Walk") {
    WalkParameters p = config.walk_reference;
    p.step_budget = optional_positive(state, config.maximum_walk_step_budget);
    p.random_seed = draw(state);
    p.even_break_base_milli = 1 + inclusive_u32(
        state, p.even_break_base_milli - 1);
    uint32_t previous = 1;
    for (WalkParameters::ScorePoint &point : p.score_curve) {
      const uint32_t ceiling = std::max(previous, point.break_base_milli);
      point.break_base_milli =
          previous + inclusive_u32(state, ceiling - previous);
      previous = point.break_base_milli;
    }
    return p;
  }
  if (rule == "Reorder") {
    ReorderParameters p = config.reorder_reference;
    p.maximum_clause_size =
        2 + inclusive_u32(state, p.maximum_clause_size - 2);
    p.clause_decay_milli = 1 + inclusive_u32(state, p.clause_decay_milli - 1);
    p.binary_weight_milli =
        1 + inclusive_u32(state, p.binary_weight_milli - 1);
    p.balanced_polarity_milli =
        1 + inclusive_u32(state, p.balanced_polarity_milli - 1);
    return p;
  }
  if (rule == "Lucky") {
    // SAT-only constructor profiles remain explicit and replayable. The
    // generator binds each typed payload to its stable Lucky identity below.
    const uint64_t lucky_kind = bounded(
        state, 12 + static_cast<uint64_t>(config.ramsey_startup_reference.enabled()));
    if (lucky_kind == 12)
      return config.ramsey_startup_reference;
    if (lucky_kind == 11) {
      IndependentSetModelParameters p = config.independent_set_model_reference;
      p.maximum_vertices =
          2 + inclusive_u32(state, p.maximum_vertices - 2);
      p.maximum_steps = bounded(state, p.maximum_steps);
      p.completion_rounds = bounded(state, p.completion_rounds);
      p.seed = draw(state);
      return p;
    }
    if (lucky_kind == 10) {
      ExactCoverModelParameters p = config.exact_cover_model_reference;
      p.block_arity = 3 + inclusive_u32(state, p.block_arity - 3);
      p.maximum_nodes = 1 + bounded(state, p.maximum_nodes);
      p.restarts = bounded(state, p.restarts);
      p.restart_nodes = 1 + bounded(state, p.restart_nodes);
      p.seed = draw(state);
      return p;
    }
    if (lucky_kind == 9) {
      AffineModelParameters p = config.affine_model_reference;
      const bool joint = bounded(state, config.affine_joint_positive_denominator) <
                         config.affine_joint_positive_numerator;
      const auto sample = [&](uint64_t maximum) {
        return joint && maximum ? 1 + bounded(state, maximum)
                                : optional_positive(state, maximum);
      };
      p.seed = draw(state);
      p.maximum_variables = sample(p.maximum_variables);
      p.maximum_constraints = sample(p.maximum_constraints);
      p.maximum_arity = sample(p.maximum_arity);
      p.maximum_projection = sample(p.maximum_projection);
      p.minimum_equations = choice_limit(state, p.minimum_equations);
      p.maximum_entries = sample(p.maximum_entries);
      p.import_bytes = sample(p.import_bytes);
      p.linear_bytes = sample(p.linear_bytes);
      p.table_bytes = sample(p.table_bytes);
      p.maximum_work = sample(
          std::min(p.maximum_work, config.maximum_rule_work_budget));
      p.candidate_budget = sample(p.candidate_budget);
      p.permutations = sample(p.permutations);
      p.automatic_negated = bounded(state, 2);
      p.shuffle_first = bounded(state, 2);
      p.free_phase = bounded(state, 2);
      p.residual_phase = bounded(state, 2);
      p.projection_mode = static_cast<AffineProjectionMode>(bounded(state, 3));
      p.projection.clear();
      if (p.projection_mode == AffineProjectionMode::Explicit) {
        std::vector<int> coordinates;
        for (Var v = 0; v < k.nvars; ++v)
          if (!kernel_var_retired(k, v))
            coordinates.push_back(kernel_externalize(k, make_lit(v, false)));
        const size_t count = bounded(
            state, 1 + std::min<size_t>(
                           coordinates.size(),
                           config.maximum_affine_projection_coordinates));
        for (size_t i = 0; i < count; ++i) {
          const size_t selected = i + bounded(state, coordinates.size() - i);
          std::swap(coordinates[i], coordinates[selected]);
          p.projection.push_back(bounded(state, 2) ? coordinates[i]
                                                   : -coordinates[i]);
        }
      }
      if (bounded(state, 2)) {
        p.scopes.clear();
        const size_t count = bounded(
            state, uint64_t(config.maximum_affine_scopes) + 1);
        for (size_t i = 0; i < count; ++i)
          p.scopes.push_back(
              {bounded(state, 2) ? AffineSearchScope::AUTO_SPLIT
                                 : bounded(state, uint64_t(k.nvars) + 1),
               inclusive_u32(state, config.maximum_affine_weight),
               inclusive_u32(state, config.maximum_affine_weight),
               inclusive_u32(state, config.maximum_affine_weight)});
      }
      return p;
    }
    if (lucky_kind == 8) {
      SimonModelParameters p = config.simon_model_reference;
      p.maximum_rounds = 1 + inclusive_u32(state, p.maximum_rounds - 1);
      p.maximum_candidates = 1 + bounded(state, p.maximum_candidates);
      return p;
    }
    if (lucky_kind == 7) {
      LookupCircuitModelParameters p = config.lookup_circuit_model_reference;
      p.maximum_fanin = 1 + inclusive_u32(state, p.maximum_fanin - 1);
      p.maximum_fanout = 1 + inclusive_u32(state, p.maximum_fanout - 1);
      p.maximum_primary_inputs =
          1 + inclusive_u32(state, p.maximum_primary_inputs - 1);
      p.maximum_support = 1 + inclusive_u32(state, p.maximum_support - 1);
      p.maximum_vector_gate_evaluations =
          1 + bounded(state, p.maximum_vector_gate_evaluations);
      return p;
    }
    if (lucky_kind == 6) {
      FermatModelParameters p = config.fermat_model_reference;
      p.maximum_bits = 1 + inclusive_u32(state, p.maximum_bits - 1);
      p.comparator_tail_clauses =
          2 + inclusive_u32(state, p.comparator_tail_clauses - 2);
      p.rho_constants = 1 + bounded(state, p.rho_constants);
      p.rho_iterations = 1 + bounded(state, p.rho_iterations);
      return p;
    }
    if (lucky_kind == 5)
      return config.balanced_signature_model_reference;
    if (lucky_kind == 4) {
      SignedCardinalityProofParameters p =
          config.signed_cardinality_proof_reference;
      p.maximum_variables =
          1 + inclusive_u32(state, p.maximum_variables - 1);
      p.maximum_groups = 1 + inclusive_u32(state, p.maximum_groups - 1);
      p.maximum_constraints =
          1 + inclusive_u32(state, p.maximum_constraints - 1);
      p.maximum_group_arity =
          3 + inclusive_u32(state, p.maximum_group_arity - 3);
      p.maximum_bdd_nodes = 1 + bounded(state, p.maximum_bdd_nodes);
      p.maximum_proof_additions =
          1 + bounded(state, p.maximum_proof_additions);
      return p;
    }
    if (lucky_kind == 3) {
      StconnProofParameters p = config.stconn_proof_reference;
      p.maximum_width =
          2 + inclusive_u32(state, p.maximum_width - 2);
      p.maximum_columns =
          3 + inclusive_u32(state, p.maximum_columns - 3);
      p.maximum_rup_clauses = 1 + bounded(state, p.maximum_rup_clauses);
      return p;
    }
    if (lucky_kind == 2) {
      OnehotModelParameters p = config.onehot_model_reference;
      p.maximum_variables =
          1 + inclusive_u32(state, config.maximum_onehot_model_variables - 1);
      p.maximum_domains =
          1 + inclusive_u32(state, config.maximum_onehot_model_domains - 1);
      p.maximum_constraints = 1 + inclusive_u32(
          state, config.maximum_onehot_model_constraints - 1);
      p.maximum_equation_arity = 1 + inclusive_u32(
          state, config.maximum_onehot_model_equation_arity - 1);
      p.minimum_domains = inclusive_u32(
          state, std::min(p.minimum_domains, p.maximum_domains));
      p.minimum_equations = inclusive_u32(state, p.minimum_equations);
      p.maximum_free_domains = inclusive_u32(
          state, config.maximum_onehot_model_free_domains);
      p.candidate_budget = optional_positive(
          state, config.maximum_onehot_model_candidates);
      return p;
    }
    if (lucky_kind == 0)
      return std::monostate{};
    ParityModelParameters p = config.parity_model_reference;
    p.maximum_variables =
        1 + inclusive_u32(state, config.maximum_parity_model_variables - 1);
    p.maximum_groups =
        1 + inclusive_u32(state, config.maximum_parity_model_groups - 1);
    p.maximum_constraints =
        1 + inclusive_u32(state, config.maximum_parity_model_constraints - 1);
    p.minimum_group_clauses =
        1 + static_cast<uint32_t>(bounded(state, 4));
    p.minimum_equations = choice_limit(state, p.minimum_equations);
    p.maximum_free_variables = inclusive_u32(
        state, std::min<uint32_t>(63,
                                 config.maximum_parity_model_free_variables));
    p.candidate_budget = optional_positive(
        state, config.maximum_parity_model_candidates);
    return p;
  }
  if (rule == "SubstitutionRedundancy") {
    SubstitutionRedundancyParameters p = config.sr_reference;
    p.maximum_validation_ticks = optional_positive(
        state, config.maximum_sr_validation_ticks);
    p.producer = static_cast<SubstitutionRedundancyProducer>(
        bounded(state, 4));
    p.orbitopal_orientation = static_cast<OrbitopalOrientation>(
        bounded(state, 3));
    p.minimum_orbitopal_rows =
        2 + inclusive_u32(state, std::max<uint32_t>(2, p.minimum_orbitopal_rows) - 2);
    p.minimum_orbitopal_columns = 2 + inclusive_u32(
        state, std::max<uint32_t>(2, p.minimum_orbitopal_columns) - 2);
    p.maximum_orbitopal_matrices =
        optional_positive(state, config.maximum_sr_matrices);
    p.maximum_orbitopal_instances =
        optional_positive(state, config.maximum_sr_instances);
    p.maximum_detection_ticks =
        optional_positive(state, config.maximum_sr_detection_ticks);
    p.maximum_general_candidates_per_advance = optional_positive(
        state, config.maximum_sr_general_candidates_per_advance);
    GraphSymmetryParameters &graph = p.graph_symmetry;
    graph.maximum_graph_vertices =
        optional_positive(state, config.maximum_sr_graph_vertices);
    graph.maximum_graph_edges =
        optional_positive(state, config.maximum_sr_graph_edges);
    graph.maximum_generators =
        optional_positive(state, config.maximum_sr_symmetry_generators);
    graph.backend_search_budget =
        optional_positive(state, config.maximum_sr_backend_search_budget);
    graph.backend_support_budget =
        optional_positive(state, config.maximum_sr_backend_support_budget);
    graph.backend_component_size_limit =
        optional_positive(state, config.maximum_sr_backend_component_size);
    graph.maximum_fixing_instances = optional_positive(
        state, config.maximum_sr_symmetry_fixing_instances);
    graph.maximum_generator_instances_per_pass = optional_positive(
        state, config.maximum_sr_symmetry_generator_instances_per_pass);
    graph.maximum_group_ticks =
        optional_positive(state, config.maximum_sr_symmetry_group_ticks);
    graph.chain_planning_tick_limit =
        optional_positive(state, config.maximum_sr_symmetry_group_ticks);
    graph.seed = static_cast<uint32_t>(draw(state));
    graph.error_bound = 1 + inclusive_u32(
        state, std::max<uint32_t>(1,
                                  config.maximum_sr_symmetry_error_bound) - 1);
    graph.fixing_mode =
        static_cast<SymmetryFixingMode>(bounded(state, 3));
    graph.decompose = bounded(state, 2);
    graph.prefer_depth_first = bounded(state, 2);
    graph.strong_certification = bounded(state, 2);
    // Derive additive scheduling choices from the already-selected symmetry
    // seed so older fields and later random steps retain their historical draw
    // positions. Versioned replay records the derived values explicitly.
    uint64_t scheduling_state =
        static_cast<uint64_t>(graph.seed) ^ 0x53594d4f52444552ull;
    graph.generator_order =
        static_cast<SymmetryTraversalOrder>(bounded(scheduling_state, 3));
    graph.clause_order =
        static_cast<SymmetryTraversalOrder>(bounded(scheduling_state, 3));
    graph.literal_order =
        static_cast<SymmetryTraversalOrder>(bounded(scheduling_state, 3));
    graph.clause_scope =
        static_cast<SymmetryClauseScope>(bounded(scheduling_state, 2));
    graph.pair_orientation =
        static_cast<SymmetryPairOrientation>(bounded(scheduling_state, 3));
    graph.direct_generator_involutions_only = bounded(scheduling_state, 2);
    graph.binary_graph_mode =
        static_cast<SymmetryBinaryGraphMode>(bounded(scheduling_state, 3));
    graph.binary_graph_density_permille = inclusive_u32(
        scheduling_state,
        config.maximum_sr_symmetry_binary_graph_density_permille);
    p.orbitopal_column_order =
        static_cast<OrbitopalColumnOrder>(bounded(scheduling_state, 4));
    p.orbitopal_completion =
        static_cast<OrbitopalCompletion>(bounded(scheduling_state, 2));
    p.orbitopal_discovery_order =
        static_cast<OrbitopalDiscoveryOrder>(bounded(scheduling_state, 4));
    p.orbitopal_row_orbit_source =
        static_cast<OrbitopalRowOrbitSource>(bounded(scheduling_state, 2));
    p.minimum_orbitopal_clique_columns =
        2 + inclusive_u32(
                scheduling_state,
                config.maximum_sr_orbitopal_clique_minimum_columns - 2);
    p.maximum_orbitopal_clique_vertices = optional_positive(
        scheduling_state, config.maximum_sr_orbitopal_clique_vertices);
    p.maximum_orbitopal_clique_ticks = optional_positive(
        scheduling_state, config.maximum_sr_orbitopal_clique_ticks);
    p.maximum_orbitopal_product_states = optional_positive(
        scheduling_state, config.maximum_sr_orbitopal_product_states);
    p.complete_unique_literal_amos = bounded(scheduling_state, 2);
    p.orbitopal_generator_fallback = bounded(scheduling_state, 2);
    p.maximum_orbitopal_refinement_orbits = optional_positive(
        scheduling_state, config.maximum_sr_orbitopal_refinement_orbits);
    p.orbitopal_base_probe_filter = bounded(scheduling_state, 2);
    p.orbitopal_base_probe_factor =
        1 + inclusive_u32(
                scheduling_state,
                config.maximum_sr_orbitopal_base_probe_factor - 1);
    graph.clause_selection =
        static_cast<SymmetryClauseSelection>(bounded(scheduling_state, 3));
    graph.chain_mode =
        static_cast<SymmetryChainMode>(bounded(scheduling_state, 3));
    graph.base_selection =
        static_cast<SymmetryBaseSelection>(bounded(scheduling_state, 7));
    graph.pointwise_root_stabilizer = bounded(scheduling_state, 2);
    graph.factor_support_components = bounded(scheduling_state, 2);
    graph.preserve_detector_order = bounded(scheduling_state, 2);
    graph.use_structured_generators = bounded(scheduling_state, 2);
    graph.stabilize_structured_variables = bounded(scheduling_state, 2);
    graph.structured_scope = static_cast<SymmetryStructuredScope>(bounded(scheduling_state, 2));
    uint64_t cut_admission_state =
        static_cast<uint64_t>(graph.seed) ^ 0x43555441444d4954ull;
    graph.nonforcing_cut_admission =
        static_cast<SymmetryNonforcingCutAdmission>(
            bounded(cut_admission_state, 3));
    // A further derived stream: the earlier parameter draws stay unchanged.
    uint64_t chain_order_state =
        static_cast<uint64_t>(graph.seed) ^ 0x434841494e4f5244ull;
    graph.cut_point =
        static_cast<SymmetryCutPoint>(bounded(chain_order_state, 2));
    graph.negation_fixing_first = bounded(chain_order_state, 2);
    graph.first_literal_fix_anchor = bounded(chain_order_state, 2);
    if (config.sr_joint_positive_numerator &&
        bounded(scheduling_state, config.sr_joint_positive_denominator) <
            config.sr_joint_positive_numerator) {
      // Independent disabling values compound across a producer's admission
      // limits. Keep that boundary arm, but also sample resources together
      // without selecting a particular producer, order, phase or granularity.
      // This derived stream leaves the earlier parameter draws unchanged.
      const auto enable = [&](uint64_t &value, uint64_t maximum) {
        if (!value && maximum)
          value = 1 + bounded(scheduling_state, maximum);
      };
      enable(p.maximum_validation_ticks, config.maximum_sr_validation_ticks);
      enable(p.maximum_detection_ticks, config.maximum_sr_detection_ticks);
      enable(p.maximum_orbitopal_matrices, config.maximum_sr_matrices);
      enable(p.maximum_orbitopal_instances, config.maximum_sr_instances);
      enable(p.maximum_general_candidates_per_advance,
             config.maximum_sr_general_candidates_per_advance);
      enable(p.maximum_orbitopal_clique_vertices,
             config.maximum_sr_orbitopal_clique_vertices);
      enable(p.maximum_orbitopal_clique_ticks,
             config.maximum_sr_orbitopal_clique_ticks);
      enable(p.maximum_orbitopal_product_states,
             config.maximum_sr_orbitopal_product_states);
      enable(p.maximum_orbitopal_refinement_orbits,
             config.maximum_sr_orbitopal_refinement_orbits);
      enable(graph.maximum_graph_vertices, config.maximum_sr_graph_vertices);
      enable(graph.maximum_graph_edges, config.maximum_sr_graph_edges);
      enable(graph.maximum_generators, config.maximum_sr_symmetry_generators);
      enable(graph.backend_search_budget, config.maximum_sr_backend_search_budget);
      enable(graph.backend_support_budget, config.maximum_sr_backend_support_budget);
      enable(graph.backend_component_size_limit,
             config.maximum_sr_backend_component_size);
      enable(graph.maximum_fixing_instances,
             config.maximum_sr_symmetry_fixing_instances);
      enable(graph.maximum_generator_instances_per_pass,
             config.maximum_sr_symmetry_generator_instances_per_pass);
      enable(graph.maximum_group_ticks, config.maximum_sr_symmetry_group_ticks);
    }
    p.orbitopal_progress_feedback = bounded(scheduling_state, 2);
    return p;
  }
  return std::monostate{};
}

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

bool parse_u64(std::string_view text, uint64_t &value) {
  if (text.empty())
    return false;
  const char *begin = text.data(), *end = begin + text.size();
  const auto parsed = std::from_chars(begin, end, value);
  return parsed.ec == std::errc{} && parsed.ptr == end;
}

bool parse_repeat(std::string_view text, RepeatMode &mode) {
  if (text == "once")
    mode = RepeatMode::Once;
  else if (text == "count")
    mode = RepeatMode::Count;
  else if (text == "fixpoint")
    mode = RepeatMode::UntilScopedFixpoint;
  else if (text == "size-scaled-fixpoint")
    mode = RepeatMode::UntilSizeScaledFixpoint;
  else
    return false;
  return true;
}

bool comma_values(std::string_view text, std::vector<uint64_t> &values) {
  values.clear();
  if (text.empty())
    return true;
  while (true) {
    const size_t comma = text.find(',');
    uint64_t value = 0;
    if (!parse_u64(text.substr(0, comma), value))
      return false;
    values.push_back(value);
    if (comma == std::string_view::npos)
      return true;
    text.remove_prefix(comma + 1);
  }
}

bool tuple(std::string_view text, std::string_view name,
           std::vector<uint64_t> &values) {
  if (!text.starts_with(name) || text.size() < name.size() + 2 ||
      text[name.size()] != '(' || text.back() != ')')
    return false;
  return comma_values(text.substr(name.size() + 1,
                                  text.size() - name.size() - 2), values);
}

bool boolean(uint64_t value, bool &out) {
  if (value > 1)
    return false;
  out = value != 0;
  return true;
}

bool parse_parameters(std::string_view text, ActionParameters &parameters) {
  if (text == "inherit") {
    parameters = std::monostate{};
    return true;
  }
  std::vector<uint64_t> v;
#define U32_AT(N, SLOT)                                                        \
  do {                                                                         \
    if (v[N] > UINT32_MAX)                                                     \
      return false;                                                            \
    SLOT = static_cast<uint32_t>(v[N]);                                        \
  } while (false)
  if (tuple(text, "analyze", v) && (v.size() == 6 || v.size() == 10)) {
    AnalyzeParameters p;
    U32_AT(0, p.minimize_depth_limit);
    if (!boolean(v[1], p.conflict_reuse)) return false;
    U32_AT(2, p.chronological_levels);
    U32_AT(3, p.bump_reason_limit);
    U32_AT(4, p.bump_reason_depth);
    U32_AT(5, p.eager_subsume);
    if (v.size() == 10) {
      if (!boolean(v[6], p.jump_reasons) ||
          !boolean(v[7], p.jump_reasons_bigbig) ||
          !boolean(v[8], p.shrink_glue_promote) ||
          !boolean(v[9], p.bump_extension_reasons))
        return false;
    }
    parameters = p;
  } else if ((tuple(text, "bve", v) && v.size() == 11) ||
             (tuple(text, "bve2", v) && v.size() == 12) ||
             (tuple(text, "bve3", v) && v.size() == 13) ||
             (tuple(text, "bve4", v) && v.size() == 14) ||
             (tuple(text, "bve5", v) && v.size() == 15)) {
    BveParameters p;
    U32_AT(0, p.gate_mask); U32_AT(1, p.growth_bound);
    U32_AT(2, p.occurrence_limit); U32_AT(3, p.clause_size_limit);
    U32_AT(4, p.xor_arity_limit); U32_AT(5, p.resolvent_size_limit);
    U32_AT(6, p.backward_occurrence_limit); p.definition_budget = v[7];
    U32_AT(8, p.definition_cores);
    U32_AT(9, p.definition_core_budget_multiplier);
    if (!boolean(v[10], p.backward)) return false;
    if (v.size() >= 12) {
      if (v[11] > static_cast<uint64_t>(BveCandidateScope::PureClosure))
        return false;
      p.candidate_scope = static_cast<BveCandidateScope>(v[11]);
    }
    if (v.size() >= 13)
      U32_AT(12, p.resolvent_count_limit);
    if (v.size() == 14) {
      if (v[13] > static_cast<uint64_t>(BveTraversal::FastEliminate))
        return false;
      p.traversal = static_cast<BveTraversal>(v[13]);
    }
    if (v.size() == 15) {
      if (v[13] > static_cast<uint64_t>(BveTraversal::FastEliminate) ||
          !boolean(v[14], p.on_the_fly_strengthening))
        return false;
      p.traversal = static_cast<BveTraversal>(v[13]);
    }
    parameters = p;
  } else if (tuple(text, "c1", v) && v.size() == 3) {
    SubsumeForwardParameters p;
    U32_AT(0, p.clause_size_limit); U32_AT(1, p.large_occurrence_limit);
    U32_AT(2, p.binary_occurrence_limit); parameters = p;
  } else if (tuple(text, "c2", v) && v.size() == 2) {
    StrengthenParameters p;
    U32_AT(0, p.clause_size_limit); U32_AT(1, p.occurrence_limit);
    parameters = p;
  } else if (tuple(text, "parity-consequence", v) && v.size() == 10) {
    ParityConsequenceParameters p;
    p.maximum_clauses = v[0]; p.maximum_groups = v[1]; p.selection_work = v[2];
    U32_AT(3, p.intermediate_width); U32_AT(4, p.source_count);
    U32_AT(5, p.output_width);
    if (!boolean(v[6], p.descending)) return false;
    p.maximum_proof_additions = v[7]; U32_AT(8, p.maximum_tree_variables);
    p.consequence = v[9]; parameters = p;
  } else if (tuple(text, "affine-consequence1", v) && v.size() == 14) {
    AffineConsequenceParameters p;
    p.maximum_clauses=v[0]; p.maximum_snapshot_literals=v[1];
    p.maximum_snapshot_work=v[2]; p.maximum_groups=v[3];
    p.maximum_patterns=v[4]; p.maximum_elimination_work=v[5];
    U32_AT(6,p.maximum_arity); U32_AT(7,p.maximum_intermediate_width);
    U32_AT(8,p.maximum_sources); U32_AT(9,p.maximum_output_width);
    p.maximum_proof_additions=v[10]; p.maximum_proof_deletions=v[11];
    p.maximum_journal_bytes=v[12]; p.consequence=v[13];
    p.semantics = AffineConsequenceSemantics::SelectedConsequenceV1;
    parameters=p;
  } else if (tuple(text, "affine-consequence2", v) && v.size() == 22) {
    AffineConsequenceParameters p;
    p.maximum_clauses=v[0]; p.maximum_snapshot_literals=v[1];
    p.maximum_snapshot_work=v[2]; p.maximum_groups=v[3];
    p.maximum_patterns=v[4]; p.maximum_elimination_work=v[5];
    U32_AT(6,p.maximum_arity); U32_AT(7,p.maximum_intermediate_width);
    U32_AT(8,p.maximum_sources); U32_AT(9,p.maximum_output_width);
    p.maximum_proof_additions=v[10]; p.maximum_proof_deletions=v[11];
    p.maximum_journal_bytes=v[12];
    p.semantics = AffineConsequenceSemantics::CertifiedBasisV2;
    p.basis_maximum_work=v[13];
    U32_AT(14,p.basis_maximum_intermediate_width);
    U32_AT(15,p.basis_maximum_sources);
    U32_AT(16,p.basis_maximum_premises);
    U32_AT(17,p.basis_maximum_nodes);
    U32_AT(18,p.basis_maximum_direct_width);
    U32_AT(19,p.maximum_external_extent);
    p.maximum_session_keys=v[20]; p.maximum_session_work=v[21];
    parameters=p;
  } else if (tuple(text, "affine-matrix-persistent1", v) && v.size() == 27) {
    AffineConsequenceParameters p;
    p.maximum_clauses=v[0]; p.maximum_snapshot_literals=v[1];
    p.maximum_snapshot_work=v[2]; p.maximum_groups=v[3];
    p.maximum_patterns=v[4]; p.maximum_elimination_work=v[5];
    U32_AT(6,p.maximum_arity); U32_AT(7,p.maximum_intermediate_width);
    U32_AT(8,p.maximum_sources); U32_AT(9,p.maximum_output_width);
    p.maximum_proof_additions=v[10]; p.maximum_proof_deletions=v[11];
    p.maximum_journal_bytes=v[12];
    p.semantics = AffineConsequenceSemantics::CertifiedBasisV2;
    p.basis_maximum_work=v[13];
    U32_AT(14,p.basis_maximum_intermediate_width);
    U32_AT(15,p.basis_maximum_sources);
    U32_AT(16,p.basis_maximum_premises);
    U32_AT(17,p.basis_maximum_nodes);
    U32_AT(18,p.basis_maximum_direct_width);
    U32_AT(19,p.maximum_external_extent);
    p.maximum_session_keys=v[20]; p.maximum_session_work=v[21];
    AffineMatrixInitializeParameters persistent;
    persistent.basis=p;
    if (v[22] != 1 || v[23] < 1 || v[23] > 7) return false;
    persistent.admission=static_cast<AffineMatrixInitializeParameters::Admission>(v[22]);
    persistent.operation=static_cast<AffineMatrixInitializeParameters::Operation>(v[23]);
    persistent.maximum_retained_bytes=v[24];
    persistent.maximum_deferred_deletions=v[25];
    persistent.maximum_deferred_literal_work=v[26];
    if (!persistent.valid()) return false;
    parameters=persistent;
  } else if (tuple(text, "exact-cover-extension1", v) && v.size() == 12) {
    ExactCoverExtensionParameters p;
    U32_AT(0,p.arity); p.maximum_items=v[1]; p.maximum_source_clauses=v[2];
    p.maximum_source_literals=v[3]; p.maximum_recognition_work=v[4];
    p.maximum_premise_references=v[5]; U32_AT(6,p.maximum_definitions);
    p.maximum_additions=v[7]; p.maximum_literal_occurrences=v[8];
    U32_AT(9,p.maximum_extent_growth); U32_AT(10,p.maximum_external_extent);
    p.maximum_journal_bytes=v[11]; parameters=p;
  } else if (tuple(text, "independent-set-cover1", v) && v.size() == 10) {
    IndependentSetCoverParameters p;
    p.maximum_clauses=v[0]; p.maximum_literals=v[1];
    p.maximum_recognition_work=v[2]; p.maximum_proof_work=v[3];
    p.maximum_proof_additions=v[4]; p.maximum_proof_deletions=v[5];
    p.maximum_proof_literal_occurrences=v[6]; p.maximum_journal_bytes=v[7];
    U32_AT(8,p.maximum_vertices); U32_AT(9,p.maximum_target); parameters=p;
  } else if (tuple(text, "gf3-affine-consequence1", v) && v.size() == 22) {
    Gf3AffineConsequenceParameters p;
    p.maximum_source_clauses=v[0]; p.maximum_source_literals=v[1];
    U32_AT(2,p.maximum_domains); U32_AT(3,p.maximum_equations);
    U32_AT(4,p.maximum_equation_arity);
    U32_AT(5,p.maximum_tuples_per_equation);
    U32_AT(6,p.maximum_result_arity); U32_AT(7,p.maximum_result_tuples);
    p.maximum_additions=v[8]; p.maximum_addition_literals=v[9];
    p.maximum_recognition_work=v[10]; p.maximum_planning_work=v[11];
    p.maximum_duplicate_scan_work=v[12]; p.maximum_revalidation_work=v[13];
    p.maximum_proof_work=v[14]; p.maximum_proof_additions=v[15];
    p.maximum_proof_literal_occurrences=v[16]; p.maximum_arena_words=v[17];
    p.maximum_journal_bytes=v[18]; U32_AT(19,p.maximum_external_extent);
    p.maximum_session_keys=v[20]; p.maximum_session_work=v[21]; parameters=p;
  } else if (tuple(text, "direct-csp-guarded-cover2", v) && v.size() == 16) {
    DirectCspGuardedCoverParameters p;
    p.maximum_source_clauses=v[0];
    p.maximum_source_literals=v[1];
    p.maximum_recognition_work=v[2];
    U32_AT(3,p.maximum_domains);
    U32_AT(4,p.maximum_domain_values);
    U32_AT(5,p.maximum_cover_literals);
    U32_AT(6,p.maximum_or_definitions);
    U32_AT(7,p.maximum_or_inputs);
    U32_AT(8,p.maximum_output_literals);
    p.maximum_proof_work=v[9];
    p.maximum_proof_additions=v[10];
    p.maximum_proof_literal_occurrences=v[11];
    p.maximum_journal_bytes=v[12];
    U32_AT(13,p.maximum_external_extent);
    p.maximum_session_keys=v[14];
    p.maximum_session_work=v[15];
    parameters = p;
  } else if (tuple(text, "direct-csp-support1", v) && v.size() == 15) {
    DirectCspSupportParameters p;
    p.maximum_source_clauses=v[0]; p.maximum_source_literals=v[1];
    p.maximum_recognition_work=v[2]; U32_AT(3,p.maximum_domain_values);
    U32_AT(4,p.maximum_direct_clause_literals);
    U32_AT(5,p.maximum_definition_clause_literals);
    p.maximum_proof_work=v[6]; p.maximum_proof_additions=v[7];
    p.maximum_proof_literal_occurrences=v[8]; p.maximum_journal_bytes=v[9];
    U32_AT(10,p.maximum_extent_growth); U32_AT(11,p.maximum_external_extent);
    p.maximum_session_keys=v[12]; p.maximum_session_work=v[13];
    if (!boolean(v[14], p.allow_extension)) return false;
    parameters = p;
  } else if (tuple(text, "nary-csp-equality1", v) && v.size() == 12) {
    NaryCspEqualityParameters p;
    p.maximum_source_clauses=v[0]; p.maximum_source_literals=v[1];
    p.maximum_recognition_work=v[2]; U32_AT(3,p.maximum_domain_values);
    p.maximum_forbidden_tuples=v[4]; p.maximum_proof_work=v[5];
    p.maximum_proof_additions=v[6];
    p.maximum_proof_literal_occurrences=v[7]; p.maximum_journal_bytes=v[8];
    U32_AT(9,p.maximum_extent_growth); U32_AT(10,p.maximum_external_extent);
    p.maximum_session_keys=v[11]; parameters=p;
  } else if (tuple(text, "nary-csp-equality-transitivity1", v) && v.size() == 16) {
    NaryCspEqualityTransitivityParameters p;
    p.maximum_source_clauses=v[0];
    p.maximum_source_literals=v[1];
    p.maximum_recognition_work=v[2];
    p.maximum_planning_work=v[3];
    U32_AT(4,p.maximum_domain_values);
    p.maximum_forbidden_tuples=v[5];
    p.maximum_proof_work=v[6];
    p.maximum_proof_additions=v[7];
    p.maximum_proof_literal_occurrences=v[8];
    p.maximum_journal_bytes=v[9];
    U32_AT(10,p.maximum_external_extent);
    p.maximum_session_keys=v[11];
    p.maximum_session_bytes=v[12];
    p.maximum_arena_words=v[13];
    p.maximum_watch_words=v[14];
    p.maximum_preparation_bytes=v[15];
    parameters=p;
  } else if (tuple(text, "cardinality-search1", v) && v.size() == 11) {
    CardinalitySearchActionLimits p;
    U32_AT(0, p.inspection.maximum_support_variables);
    U32_AT(1, p.inspection.maximum_constraints);
    U32_AT(2, p.inspection.maximum_source_clauses);
    U32_AT(3, p.inspection.maximum_reason_width);
    p.inspection.maximum_work = v[4];
    p.resolution.maximum_work = v[5];
    U32_AT(6, p.resolution.maximum_additions);
    p.resolution.maximum_added_literals = v[7];
    p.maximum_source_scan_work = v[8];
    p.maximum_arena_words = v[9];
    p.maximum_journal_bytes = v[10];
    parameters = p;
  } else if (tuple(text, "nary-csp-equality-amo1", v) && v.size() == 11) {
    NaryCspEqualityAmoParameters p;
    p.maximum_source_clauses=v[0]; p.maximum_source_literals=v[1];
    p.maximum_recognition_work=v[2]; U32_AT(3,p.maximum_domain_values);
    p.maximum_forbidden_tuples=v[4]; p.maximum_proof_work=v[5];
    p.maximum_proof_additions=v[6];
    p.maximum_proof_literal_occurrences=v[7]; p.maximum_journal_bytes=v[8];
    U32_AT(9,p.maximum_external_extent); p.maximum_session_keys=v[10];
    parameters=p;
  } else if (tuple(text, "direct-php-refutation1", v) && v.size() == 16) {
    DirectPhpRefutationParameters p;
    p.maximum_snapshot_clauses=v[0]; p.maximum_snapshot_literals=v[1];
    p.maximum_snapshot_work=v[2]; p.maximum_recognition_clauses=v[3];
    p.maximum_recognition_literals=v[4]; p.maximum_recognition_work=v[5];
    p.maximum_row_candidates=v[6]; p.maximum_row_combinations=v[7];
    p.maximum_mapping_trials=v[8]; U32_AT(9,p.maximum_holes);
    p.maximum_proof_additions=v[10]; p.maximum_proof_deletions=v[11];
    p.maximum_proof_literals=v[12]; p.maximum_fresh_variables=v[13];
    p.maximum_proof_events=v[14]; p.maximum_replay_work=v[15];
    parameters = p;
  } else if (tuple(text, "gf3-gaussian-consequence1", v) && v.size() == 38) {
    Gf3GaussianConsequenceParameters p;
    p.maximum_source_clauses=v[0]; p.maximum_source_literals=v[1];
    U32_AT(2,p.maximum_domains); U32_AT(3,p.maximum_equations);
    U32_AT(4,p.maximum_equation_arity); U32_AT(5,p.maximum_tuples_per_equation);
    p.maximum_bridge_work=v[6]; p.maximum_bridge_source_clauses=v[7];
    p.maximum_bridge_source_literals=v[8]; U32_AT(9,p.maximum_bridge_domains);
    U32_AT(10,p.maximum_bridge_equations); p.maximum_bridge_terms=v[11];
    p.maximum_authentication_references=v[12]; p.maximum_references_per_equation=v[13];
    p.maximum_gaussian_work=v[14]; p.maximum_fill_entries=v[15];
    U32_AT(16,p.maximum_intermediate_width); U32_AT(17,p.maximum_provenance_terms);
    U32_AT(18,p.maximum_derivations); p.maximum_retained_plan_bytes=v[19];
    p.maximum_certificate_work=v[20]; p.maximum_events=v[21];
    p.maximum_event_literals=v[22]; p.maximum_active_clauses=v[23];
    p.maximum_retained_source_references=v[24]; U32_AT(25,p.maximum_event_width);
    U32_AT(26,p.maximum_branch_domains); U32_AT(27,p.maximum_consequences);
    p.maximum_capture_work=v[28]; p.maximum_revalidation_work=v[29];
    p.maximum_proof_additions=v[30]; p.maximum_proof_literal_occurrences=v[31];
    p.maximum_journal_bytes=v[32]; p.maximum_identity_bytes=v[33];
    U32_AT(34,p.maximum_external_extent); p.maximum_session_keys=v[35];
    p.maximum_session_work=v[36]; p.maximum_session_identity_bytes=v[37];
    parameters=p;
  } else if (tuple(text, "gf3-general-row1", v) && v.size() == 42) {
    Gf3GeneralRowConsequenceParameters p;
    auto &g=p.gaussian;
    g.maximum_source_clauses=v[0]; g.maximum_source_literals=v[1];
    U32_AT(2,g.maximum_domains); U32_AT(3,g.maximum_equations);
    U32_AT(4,g.maximum_equation_arity); U32_AT(5,g.maximum_tuples_per_equation);
    g.maximum_bridge_work=v[6]; g.maximum_bridge_source_clauses=v[7];
    g.maximum_bridge_source_literals=v[8]; U32_AT(9,g.maximum_bridge_domains);
    U32_AT(10,g.maximum_bridge_equations); g.maximum_bridge_terms=v[11];
    g.maximum_authentication_references=v[12]; g.maximum_references_per_equation=v[13];
    g.maximum_gaussian_work=v[14]; g.maximum_fill_entries=v[15];
    U32_AT(16,g.maximum_intermediate_width); U32_AT(17,g.maximum_provenance_terms);
    U32_AT(18,g.maximum_derivations); g.maximum_retained_plan_bytes=v[19];
    g.maximum_certificate_work=v[20]; g.maximum_events=v[21];
    g.maximum_event_literals=v[22]; g.maximum_active_clauses=v[23];
    g.maximum_retained_source_references=v[24]; U32_AT(25,g.maximum_event_width);
    U32_AT(26,g.maximum_branch_domains); U32_AT(27,g.maximum_consequences);
    g.maximum_capture_work=v[28]; g.maximum_revalidation_work=v[29];
    g.maximum_proof_additions=v[30]; g.maximum_proof_literal_occurrences=v[31];
    g.maximum_journal_bytes=v[32]; g.maximum_identity_bytes=v[33];
    U32_AT(34,g.maximum_external_extent); g.maximum_session_keys=v[35];
    g.maximum_session_work=v[36]; g.maximum_session_identity_bytes=v[37];
    p.maximum_row_tuples=v[38]; U32_AT(39,p.maximum_row_width);
    p.maximum_duplicate_scan_work=v[40]; p.maximum_arena_words=v[41];
    parameters=p;
  } else if (tuple(text, "tseitin-cycle-breaking1", v) && v.size() == 26) {
    TseitinCycleBreakingParameters p;
    p.maximum_snapshot_clauses=v[0]; p.maximum_snapshot_literals=v[1];
    p.maximum_snapshot_work=v[2]; U32_AT(3,p.maximum_snapshot_variables);
    p.maximum_discovery_work=v[4]; p.maximum_discovery_clauses=v[5];
    p.maximum_discovery_groups=v[6]; p.maximum_discovery_retained_bytes=v[7];
    p.maximum_admission_work=v[8]; p.maximum_planning_work=v[9];
    U32_AT(10,p.maximum_witness_length); U32_AT(11,p.maximum_planning_additions);
    p.maximum_transaction_work=v[12]; p.maximum_transaction_additions=v[13];
    p.maximum_transaction_deletions=v[14]; p.maximum_transaction_helpers=v[15];
    p.maximum_transaction_witness_literals=v[16]; p.maximum_validation_ticks=v[17];
    p.maximum_components=v[18]; p.maximum_identity_bytes=v[19];
    p.maximum_total_identity_bytes=v[20]; p.maximum_identity_work=v[21];
    p.maximum_journal_bytes=v[22]; p.maximum_session_keys=v[23];
    p.maximum_session_work=v[24];
    p.maximum_session_identity_bytes=v[25]; parameters=p;
  } else if (tuple(text, "semantic-variable-advice1", v) && v.size() == 4) {
    SemanticVariableAdviceParameters p;
    U32_AT(0,p.maximum_variables); p.maximum_priority_sum=v[1];
    U32_AT(2,p.existing_theory_priority);
    U32_AT(3,p.introduced_theory_priority); parameters=p;
  } else if (tuple(text, "congruence", v) && v.size() == 3) {
    CongruenceParameters p;
    U32_AT(0, p.and_arity_limit); p.gate_count_limit = v[1];
    U32_AT(2, p.xor_arity_limit); parameters = p;
  } else if (tuple(text, "instantiate", v) && v.size() == 5) {
    InstantiateParameters p;
    U32_AT(0, p.minimum_clause_size); U32_AT(1, p.occurrence_limit);
    U32_AT(2, p.minimum_unassigned);
    if (!boolean(v[3], p.include_redundant)) return false;
    U32_AT(4, p.new_material_threshold); parameters = p;
  } else if (tuple(text, "backbone", v) && v.size() == 2) {
    BackboneParameters p;
    U32_AT(0, p.initial_rounds); U32_AT(1, p.maximum_rounds); parameters = p;
  } else if (tuple(text, "sweep", v) &&
             (v.size() == 17 || v.size() == 18)) {
    SweepParameters p;
    U32_AT(0, p.max_depth); U32_AT(1, p.max_depth_ceiling);
    U32_AT(2, p.max_env_vars); U32_AT(3, p.max_env_vars_ceiling);
    U32_AT(4, p.max_env_clauses); U32_AT(5, p.max_env_clauses_ceiling);
    U32_AT(6, p.max_candidates); p.env_ticks = v[7];
    U32_AT(8, p.max_proof_steps); U32_AT(9, p.max_seeds);
    p.round_ticks = v[10];
    if (!boolean(v[11], p.strip_false) ||
        !boolean(v[12], p.measure_closure) ||
        !boolean(v[13], p.full_partition) ||
        !boolean(v[14], p.skip_embedded_fixed)) return false;
    U32_AT(15, p.flip_rounds);
    if (!boolean(v[16], p.resume)) return false;
    if (v.size() == 18 && !boolean(v[17], p.force_general_proof))
      return false;
    parameters = p;
  } else if (tuple(text, "vivify", v) && (v.size() == 4 || v.size() == 7)) {
    VivifyParameters p;
    if (v[0] > UINT8_MAX) return false;
    p.scope = static_cast<uint8_t>(v[0]);
    if (!boolean(v[1], p.reuse) || !boolean(v[2], p.flush)) return false;
    U32_AT(3, p.irredundant_sort_ratio);
    if (v.size() == 7) {
      if (!boolean(v[4], p.band)) return false;
      p.band_budget = v[5];
      p.band_offset = v[6];
    }
    parameters = p;
  } else if (tuple(text, "bce", v) && v.size() == 1) {
    BceParameters p; U32_AT(0, p.occurrence_limit); parameters = p;
  } else if (tuple(text, "ternary", v) && v.size() == 2) {
    TernaryParameters p; U32_AT(0, p.occurrence_limit);
    U32_AT(1, p.maximum_add_percent); parameters = p;
  } else if (tuple(text, "factor", v) && v.size() == 5) {
    FactorParameters p; U32_AT(0, p.maximum_clause_size);
    U32_AT(1, p.growth_bound); U32_AT(2, p.candidate_rounds);
    if (!boolean(v[3], p.composite) || !boolean(v[4], p.demote_fresh))
      return false;
    parameters = p;
  } else if (tuple(text, "transitive", v) && v.size() == 3) {
    TransitiveReduceParameters p;
    if (!boolean(v[0], p.early_exit) || !boolean(v[1], p.live_meter) ||
        !boolean(v[2], p.checked_budget_meter)) return false;
    parameters = p;
  } else if (tuple(text, "substitute", v) && (v.size() == 1 || v.size() == 3)) {
    SubstituteParameters p;
    if (!boolean(v[0], p.demote_substituted)) return false;
    if (v.size() == 3) {
      U32_AT(1, p.max_rounds);
      if (!boolean(v[2], p.round_effort_ceiling)) return false;
    }
    parameters = p;
  } else if (tuple(text, "reduce", v) &&
             (v.size() == 2 || v.size() == 6 || v.size() == 7)) {
    ReduceParameters p; U32_AT(0, p.tier1); U32_AT(1, p.tier2);
    if (v.size() >= 6) {
      U32_AT(2, p.victim_numerator); U32_AT(3, p.victim_denominator);
      if (!boolean(v[4], p.protect_tiers) ||
          !boolean(v[5], p.compact_collection)) return false;
      if (v.size() == 7 && !boolean(v[6], p.reduction_bookkeeping)) return false;
    }
    parameters = p;
  } else if ((tuple(text, "walk", v) || tuple(text, "walk2", v) || tuple(text, "walk3", v)) &&
             v.size() >= 2) {
    WalkParameters p;
    p.step_budget = v[0];
    p.random_seed = v[1];
    const bool split = text.starts_with("walk3(");
    const size_t offset = split ? 4 : text.starts_with("walk2(") ? 1 : 0;
    if (offset && v.size() < 5) return false;
    if (offset) p.seed_candidate = v[2];
    if (split) {
      if (v.size() < 8 || v[4] > UINT32_MAX || v[5] > UINT32_MAX) return false;
      p.preparation_budget = v[3];
      p.objective = static_cast<WalkParameters::Objective>(v[4]);
      p.admission = static_cast<WalkParameters::Admission>(v[5]);
    }
    if (offset || v.size() != 2) {
      const size_t head = 4 + offset;
      if (v.size() < head || v[2 + offset] > UINT32_MAX ||
          v[3 + offset] > (v.size() - head) / 2 ||
          v.size() != head + 2 * v[3 + offset]) return false;
      p.even_break_base_milli = static_cast<uint32_t>(v[2 + offset]);
      p.score_curve.clear();
      for (size_t i = 0; i < v[3 + offset]; ++i) {
        const uint64_t x = v[head + 2 * i], y = v[head + 2 * i + 1];
        if (x > UINT32_MAX || y > UINT32_MAX) return false;
        p.score_curve.push_back(
            {static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
      }
    }
    parameters = std::move(p);
  } else if (tuple(text, "reorder", v) &&
             (v.size() == 1 || v.size() == 4)) {
    ReorderParameters p;
    U32_AT(0, p.maximum_clause_size);
    if (v.size() == 4) {
      U32_AT(1, p.clause_decay_milli);
      U32_AT(2, p.binary_weight_milli);
      U32_AT(3, p.balanced_polarity_milli);
    }
    parameters = p;
  } else if ((tuple(text, "parity-model", v) && v.size() == 7) ||
             (tuple(text, "parity-model2", v) && v.size() == 14)) {
    ParityModelParameters p;
    U32_AT(0, p.maximum_variables);
    U32_AT(1, p.maximum_groups);
    U32_AT(2, p.maximum_constraints);
    U32_AT(3, p.minimum_group_clauses);
    U32_AT(4, p.minimum_equations);
    U32_AT(5, p.maximum_free_variables);
    p.candidate_budget = v[6];
    if (v.size() == 14) {
      U32_AT(7, p.refutation_maximum_variables);
      U32_AT(8, p.refutation_maximum_groups);
      U32_AT(9, p.refutation_maximum_constraints);
      U32_AT(10, p.refutation_maximum_arity);
      U32_AT(11, p.refutation_probe_constraints);
      U32_AT(12, p.refutation_minimum_probe_repeats);
      p.refutation_maximum_proof_additions = v[13];
    }
    parameters = p;
  } else if ((tuple(text, "onehot-model", v) && v.size() == 8) ||
             (tuple(text, "onehot-model2", v) && v.size() == 13)) {
    OnehotModelParameters p;
    U32_AT(0, p.maximum_variables);
    U32_AT(1, p.maximum_domains);
    U32_AT(2, p.maximum_constraints);
    U32_AT(3, p.maximum_equation_arity);
    U32_AT(4, p.minimum_domains);
    U32_AT(5, p.minimum_equations);
    U32_AT(6, p.maximum_free_domains);
    p.candidate_budget = v[7];
    if (v.size() == 13) {
      U32_AT(8, p.refutation_maximum_variables);
      U32_AT(9, p.refutation_maximum_groups);
      U32_AT(10, p.refutation_maximum_constraints);
      U32_AT(11, p.refutation_maximum_arity);
      p.refutation_maximum_proof_additions = v[12];
    }
    parameters = p;
  } else if (tuple(text, "balanced-signature-model", v) && v.size() == 7) {
    BalancedSignatureModelParameters p;
    U32_AT(0, p.maximum_balls);
    U32_AT(1, p.maximum_width);
    p.restarts = v[2];
    p.steps_per_restart = v[3];
    U32_AT(4, p.samples_per_step);
    p.seed = v[5];
    U32_AT(6, p.completion_rounds);
    parameters = p;
  } else if (tuple(text, "fermat-model", v) && v.size() == 4) {
    FermatModelParameters p;
    U32_AT(0, p.maximum_bits);
    U32_AT(1, p.comparator_tail_clauses);
    p.rho_constants = v[2];
    p.rho_iterations = v[3];
    parameters = p;
  } else if (tuple(text, "lookup-circuit-model", v) && v.size() == 5) {
    LookupCircuitModelParameters p;
    U32_AT(0, p.maximum_fanin);
    U32_AT(1, p.maximum_fanout);
    U32_AT(2, p.maximum_primary_inputs);
    U32_AT(3, p.maximum_support);
    p.maximum_vector_gate_evaluations = v[4];
    parameters = p;
  } else if (tuple(text, "simon-model", v) && v.size() == 2) {
    SimonModelParameters p;
    U32_AT(0, p.maximum_rounds);
    p.maximum_candidates = v[1];
    parameters = p;
  } else if (tuple(text, "stconn-proof", v) && v.size() == 3) {
    StconnProofParameters p;
    U32_AT(0, p.maximum_width);
    U32_AT(1, p.maximum_columns);
    p.maximum_rup_clauses = v[2];
    parameters = p;
  } else if (tuple(text, "signed-cardinality-proof", v) && v.size() == 6) {
    SignedCardinalityProofParameters p;
    U32_AT(0, p.maximum_variables);
    U32_AT(1, p.maximum_groups);
    U32_AT(2, p.maximum_constraints);
    U32_AT(3, p.maximum_group_arity);
    p.maximum_bdd_nodes = v[4];
    p.maximum_proof_additions = v[5];
    parameters = p;
  } else if (tuple(text, "exact-cover-model", v) && v.size() == 5) {
    ExactCoverModelParameters p;
    U32_AT(0, p.block_arity);
    p.maximum_nodes = v[1];
    p.restarts = v[2];
    p.restart_nodes = v[3];
    p.seed = v[4];
    parameters = p;
  } else if (tuple(text, "independent-set-model", v) && v.size() == 4) {
    IndependentSetModelParameters p;
    U32_AT(0, p.maximum_vertices);
    p.maximum_steps = v[1];
    p.completion_rounds = v[2];
    p.seed = v[3];
    parameters = p;
  } else if (tuple(text, "ramsey-startup1", v) && v.size() == 8) {
    RamseyStartupParameters p;
    p.recognition_clauses=v[0]; p.recognition_variables=v[1];
    p.recognition_pair_cells=v[2]; p.recognition_work=v[3];
    p.certificate_work=v[4]; p.maximum_nodes=v[5]; p.maximum_additions=v[6];
    U32_AT(7, p.maximum_external_extent);
    parameters = p;
  } else if (tuple(text, "graph-labeling1", v) && v.size() == 18) {
    GraphLabelingParameters p;
    U32_AT(0, p.maximum_vertices);
    p.workspace_bytes = v[1];
    U32_AT(2, p.maximum_dfs_vertices);
    p.maximum_dfs_nodes = v[3];
    U32_AT(4, p.maximum_dfs_starts);
    U32_AT(5, p.early_dfs_choices);
    p.repair_rounds = v[6];
    p.anneal_iterations = v[7];
    p.stall_anneal_iterations = v[8];
    p.seed = v[9];
    for (size_t i = 0; i < p.work.size(); ++i) p.work[i] = v[10 + i];
    parameters = p;
  } else if (tuple(text, "affine1", v) && v.size() >= 20) {
    AffineModelParameters p;
    p.maximum_variables=v[0]; p.maximum_constraints=v[1]; p.maximum_arity=v[2];
    p.maximum_projection=v[3]; p.minimum_equations=v[4]; p.maximum_entries=v[5];
    p.import_bytes=v[6]; p.linear_bytes=v[7]; p.table_bytes=v[8];
    p.maximum_work=v[9]; p.candidate_budget=v[10]; p.permutations=v[11]; p.seed=v[12];
    if(v[13]>static_cast<uint64_t>(AffineProjectionMode::Explicit) ||
       !boolean(v[14],p.automatic_negated) || !boolean(v[15],p.shuffle_first) ||
       !boolean(v[16],p.free_phase) || !boolean(v[17],p.residual_phase) ||
       v[18]>v.size()-20)return false;
    p.projection_mode=static_cast<AffineProjectionMode>(v[13]);
    const size_t count=static_cast<size_t>(v[18]);
    p.projection.clear();
    for(size_t i=0;i<count;++i) {
      const uint64_t wire=v[19+i];
      if((wire>>1)>=INT_MAX)return false;
      const int positive=static_cast<int>((wire>>1)+1);
      p.projection.push_back(wire&1?-positive:positive);
    }
    const size_t at=19+count;
    if((v.size()-at-1)%4 || v[at]!=(v.size()-at-1)/4)return false;
    p.scopes.clear();
    for(size_t i=at+1;i<v.size();i+=4)
      p.scopes.push_back({v[i],v[i+1],v[i+2],v[i+3]});
    if(!p.valid())return false;
    parameters=std::move(p);
  } else if (tuple(text, "sr", v) && v.size() == 9) {
    SubstitutionRedundancyParameters p;
    p.maximum_validation_ticks = v[0];
    bool orbitopal = false, general = false;
    if (!boolean(v[1], orbitopal))
      return false;
    U32_AT(2, p.minimum_orbitopal_rows);
    U32_AT(3, p.minimum_orbitopal_columns);
    p.maximum_orbitopal_matrices = v[4];
    p.maximum_orbitopal_instances = v[5];
    p.maximum_detection_ticks = v[6];
    if (!boolean(v[7], general) || (orbitopal && general))
      return false;
    p.maximum_general_candidates_per_advance = v[8];
    p.producer = orbitopal
                     ? SubstitutionRedundancyProducer::Orbitopal
                     : general
                           ? SubstitutionRedundancyProducer::GeneralComplete
                           : SubstitutionRedundancyProducer::ExplicitOnly;
    parameters = p;
  } else if ((tuple(text, "sr2", v) && v.size() == 24) ||
             (tuple(text, "sr3", v) && v.size() == 30) ||
             (tuple(text, "sr4", v) && v.size() == 33) ||
             (tuple(text, "sr5", v) && v.size() == 34) ||
             (tuple(text, "sr6", v) && v.size() == 35) ||
             (tuple(text, "sr7", v) && v.size() == 36) ||
             (tuple(text, "sr8", v) && v.size() == 37) ||
             (tuple(text, "sr9", v) && v.size() == 41) ||
             (tuple(text, "sr10", v) && v.size() == 42) ||
             (tuple(text, "sr11", v) && v.size() == 44) ||
             (tuple(text, "sr12", v) && v.size() == 45) ||
             (tuple(text, "sr13", v) && v.size() == 47) ||
             (tuple(text, "sr14", v) && v.size() == 48) ||
             (tuple(text, "sr15", v) && v.size() == 48) ||
             (tuple(text, "sr16", v) && v.size() == 49) ||
             (tuple(text, "sr17", v) && v.size() == 50) ||
             (tuple(text, "sr18", v) && v.size() == 50) ||
             (tuple(text, "sr19", v) && v.size() == 50) ||
             (tuple(text, "sr20", v) && v.size() == 51) ||
             (tuple(text, "sr21", v) && v.size() == 52) ||
             (tuple(text, "sr22", v) && v.size() == 54) ||
             (tuple(text, "sr23", v) && v.size() == 55) ||
             (tuple(text, "sr24", v) && v.size() == 55) ||
             (tuple(text, "sr25", v) && v.size() == 55) ||
             (tuple(text, "sr26", v) && v.size() == 56) ||
             (tuple(text, "sr27", v) && v.size() == 57) ||
             (tuple(text, "sr28", v) && v.size() == 58) ||
             (tuple(text, "sr29", v) && v.size() == 60) ||
             (tuple(text, "sr30", v) && v.size() == 61)) {
    const bool has_fix_anchor = text.starts_with("sr30(");
    const bool has_chain_order = text.starts_with("sr29(") || has_fix_anchor;
    const bool has_cut_admission = text.starts_with("sr28(") || has_chain_order;
    const bool has_progress_feedback = text.starts_with("sr27(") ||
                                       has_cut_admission;
    const bool has_schreier_domain_limit = text.starts_with("sr26(") ||
                                           has_progress_feedback;
    const bool has_first_shortest = text.starts_with("sr25(") ||
                                    has_schreier_domain_limit;
    const bool has_shortest_base = text.starts_with("sr24(") || has_first_shortest;
    const bool has_structured_scope = text.starts_with("sr23(") || has_shortest_base;
    const bool has_structured_handoff = text.starts_with("sr22(") || has_structured_scope;
    const bool has_detector_order = text.starts_with("sr21(") || has_structured_handoff;
    const bool has_components = text.starts_with("sr20(") || has_detector_order;
    const bool has_resift = text.starts_with("sr19(") || has_components;
    const bool has_forward_scan = text.starts_with("sr18(") || has_resift;
    const bool has_planning_limit = text.starts_with("sr17(") || has_forward_scan;
    const bool has_row_orbit_source =
        text.starts_with("sr16(") || has_planning_limit;
    const bool has_bounded_clique_order =
        text.starts_with("sr15(") || has_row_orbit_source;
    const bool has_graph_scheduling = v.size() >= 30;
    const bool has_orbitopal_ordering = v.size() >= 33;
    const bool has_orbitopal_completion = v.size() >= 35;
    const bool has_orbitopal_product_states = v.size() >= 36;
    const bool has_unique_literal_amos = v.size() >= 37;
    const bool has_refinement_scheduling = v.size() >= 41;
    const bool has_discovery_order = v.size() >= 42;
    const bool has_binary_graph = v.size() >= 44;
    const bool has_clause_selection = v.size() >= 45;
    const bool has_fix_and_cut = v.size() >= 47;
    const bool has_root_stabilizer = v.size() >= 48;
    SubstitutionRedundancyParameters p;
    if (v[1] > static_cast<uint64_t>(
                   SubstitutionRedundancyProducer::GraphSymmetry) ||
        v[2] > static_cast<uint64_t>(OrbitopalOrientation::Both))
      return false;
    p.maximum_validation_ticks = v[0];
    p.producer = static_cast<SubstitutionRedundancyProducer>(v[1]);
    p.orbitopal_orientation = static_cast<OrbitopalOrientation>(v[2]);
    U32_AT(3, p.minimum_orbitopal_rows);
    U32_AT(4, p.minimum_orbitopal_columns);
    p.maximum_orbitopal_matrices = v[5];
    p.maximum_orbitopal_instances = v[6];
    p.maximum_detection_ticks = v[7];
    p.maximum_general_candidates_per_advance = v[8];
    GraphSymmetryParameters &graph = p.graph_symmetry;
    graph.maximum_graph_vertices = v[9];
    graph.maximum_graph_edges = v[10];
    graph.maximum_generators = v[11];
    graph.backend_search_budget = v[12];
    graph.backend_support_budget = v[13];
    graph.backend_component_size_limit = v[14];
    graph.maximum_fixing_instances = v[15];
    graph.maximum_generator_instances_per_pass = v[16];
    graph.maximum_group_ticks = v[17];
    if (has_planning_limit)
      graph.chain_planning_tick_limit = v[49];
    U32_AT(18, graph.seed);
    U32_AT(19, graph.error_bound);
    if (v[20] > static_cast<uint64_t>(
                    SymmetryFixingMode::StabilizerThenGenerators))
      return false;
    graph.fixing_mode = static_cast<SymmetryFixingMode>(v[20]);
    if (!boolean(v[21], graph.decompose) ||
        !boolean(v[22], graph.prefer_depth_first) ||
        !boolean(v[23], graph.strong_certification))
      return false;
    if (has_graph_scheduling) {
      if (v[24] > static_cast<uint64_t>(SymmetryTraversalOrder::Seeded) ||
          v[25] > static_cast<uint64_t>(SymmetryTraversalOrder::Seeded) ||
          v[26] > static_cast<uint64_t>(SymmetryTraversalOrder::Seeded) ||
          v[27] >
              static_cast<uint64_t>(SymmetryClauseScope::UniqueLiteralsOnly) ||
          v[28] > static_cast<uint64_t>(SymmetryPairOrientation::Seeded) ||
          v[29] > 1)
        return false;
      graph.generator_order = static_cast<SymmetryTraversalOrder>(v[24]);
      graph.clause_order = static_cast<SymmetryTraversalOrder>(v[25]);
      graph.literal_order = static_cast<SymmetryTraversalOrder>(v[26]);
      graph.clause_scope = static_cast<SymmetryClauseScope>(v[27]);
      graph.pair_orientation = static_cast<SymmetryPairOrientation>(v[28]);
      graph.direct_generator_involutions_only = v[29] != 0;
    }
    if (has_orbitopal_ordering) {
      if (v[30] > static_cast<uint64_t>(
                      has_bounded_clique_order
                          ? OrbitopalColumnOrder::CooccurrenceBoundedGreedyClique
                          : OrbitopalColumnOrder::CooccurrenceGreedyClique))
        return false;
      p.orbitopal_column_order = static_cast<OrbitopalColumnOrder>(v[30]);
      p.maximum_orbitopal_clique_vertices = v[31];
      p.maximum_orbitopal_clique_ticks = v[32];
      // sr4 reordered every legal matrix. Preserve that exact schedule while
      // sr5 records the caller-selected small-matrix admission cutoff.
      if (v.size() >= 34) {
        if (v[33] > UINT32_MAX)
          return false;
        p.minimum_orbitopal_clique_columns = static_cast<uint32_t>(v[33]);
      } else {
        p.minimum_orbitopal_clique_columns = 2;
      }
      if (has_orbitopal_completion) {
        if (v[34] > static_cast<uint64_t>(
                        OrbitopalCompletion::IndividualizedGraphCompletion))
          return false;
        p.orbitopal_completion = static_cast<OrbitopalCompletion>(v[34]);
      }
      if (has_orbitopal_product_states)
        p.maximum_orbitopal_product_states = v[35];
      if (has_unique_literal_amos &&
          !boolean(v[36], p.complete_unique_literal_amos))
        return false;
      if (has_refinement_scheduling) {
        if (!boolean(v[37], p.orbitopal_generator_fallback) ||
            !boolean(v[39], p.orbitopal_base_probe_filter) ||
            v[40] > UINT32_MAX || !v[40])
          return false;
        p.maximum_orbitopal_refinement_orbits = v[38];
        p.orbitopal_base_probe_factor = static_cast<uint32_t>(v[40]);
      }
      if (has_discovery_order) {
        if (v[41] > static_cast<uint64_t>(
                        OrbitopalDiscoveryOrder::UniqueLiteralOnly))
          return false;
        p.orbitopal_discovery_order =
            static_cast<OrbitopalDiscoveryOrder>(v[41]);
      }
      if (has_binary_graph) {
        if (v[42] >
                static_cast<uint64_t>(SymmetryBinaryGraphMode::Automatic) ||
            v[43] > UINT32_MAX)
          return false;
        graph.binary_graph_mode =
            static_cast<SymmetryBinaryGraphMode>(v[42]);
        graph.binary_graph_density_permille = static_cast<uint32_t>(v[43]);
      }
      if (has_clause_selection) {
        if (v[44] >
            static_cast<uint64_t>(SymmetryClauseSelection::AmoThenShortest))
          return false;
        graph.clause_selection =
            static_cast<SymmetryClauseSelection>(v[44]);
      }
      if (has_fix_and_cut) {
        if (v[45] > static_cast<uint64_t>(has_resift
                        ? SymmetryChainMode::FixAndCutResift
                        : SymmetryChainMode::FixAndCut) ||
            v[46] >
                static_cast<uint64_t>(has_first_shortest
                    ? SymmetryBaseSelection::FirstShortestThenForwardRemainder
                    : has_shortest_base ? SymmetryBaseSelection::ShortestClause
                    : has_forward_scan
                    ? SymmetryBaseSelection::FirstAmoThenForwardRemainder
                    : SymmetryBaseSelection::FirstAmoThenOrderedRemainder))
          return false;
        graph.chain_mode = static_cast<SymmetryChainMode>(v[45]);
        graph.base_selection = static_cast<SymmetryBaseSelection>(v[46]);
      }
      if (has_structured_scope) {
        if (v[54] > static_cast<uint64_t>(SymmetryStructuredScope::AxisAnchors))
          return false;
        graph.structured_scope = static_cast<SymmetryStructuredScope>(v[54]);
      }
      if (has_schreier_domain_limit)
        graph.schreier_domain_limit = v[55];
      if (has_progress_feedback &&
          !boolean(v[56], p.orbitopal_progress_feedback))
        return false;
      if (has_cut_admission) {
        if (v[57] > static_cast<uint64_t>(
                        SymmetryNonforcingCutAdmission::RequireCompoundProgress))
          return false;
        graph.nonforcing_cut_admission =
            static_cast<SymmetryNonforcingCutAdmission>(v[57]);
      }
      if (has_chain_order) {
        if (v[58] > static_cast<uint64_t>(SymmetryCutPoint::LargestOrbitLast) ||
            v[59] > 1)
          return false;
        graph.cut_point = static_cast<SymmetryCutPoint>(v[58]);
        graph.negation_fixing_first = v[59];
      }
      if (has_fix_anchor) {
        if (v[60] > 1)
          return false;
        graph.first_literal_fix_anchor = v[60];
      }
      if (has_structured_handoff) {
        if (v[52] > 1 || v[53] > 1) return false;
        graph.use_structured_generators = v[52];
        graph.stabilize_structured_variables = v[53];
      }
      if (has_detector_order) {
        if (v[51] > 1) return false;
        graph.preserve_detector_order = v[51];
      }
      if (has_components) {
        if (v[50] > 1) return false;
        graph.factor_support_components = v[50];
      }
      if (has_root_stabilizer) {
        if (v[47] > 1)
          return false;
        graph.pointwise_root_stabilizer = v[47];
      }
      if (has_row_orbit_source) {
        if (v[48] > static_cast<uint64_t>(OrbitopalRowOrbitSource::RefinementCells))
          return false;
        p.orbitopal_row_orbit_source = static_cast<OrbitopalRowOrbitSource>(v[48]);
      }
    }
    parameters = p;
  } else {
    return false;
  }
#undef U32_AT
  return true;
}

bool parse_grid(std::string_view text, FactorGrid &grid) {
  std::vector<uint64_t> v;
  if (!comma_values(text, v))
    return false;
  size_t at = 0;
  const auto count = [&]() -> uint64_t {
    return at < v.size() ? v[at++] : UINT64_MAX;
  };
  const uint64_t nf = count();
  if (nf == UINT64_MAX || nf > v.size() - at)
    return false;
  for (uint64_t i = 0; i < nf; ++i) {
    if (v[at] > UINT32_MAX) return false;
    grid.factors.push_back(static_cast<Lit>(v[at++]));
  }
  const uint64_t nr = count();
  if (nr == UINT64_MAX || nr > v.size() - at)
    return false;
  for (uint64_t i = 0; i < nr; ++i) {
    const uint64_t nl = count();
    if (nl == UINT64_MAX || nl > v.size() - at) return false;
    grid.remainders.emplace_back();
    for (uint64_t j = 0; j < nl; ++j) {
      if (v[at] > UINT32_MAX) return false;
      grid.remainders.back().push_back(static_cast<Lit>(v[at++]));
    }
  }
  const uint64_t nm = count();
  if (nm == UINT64_MAX || nm > v.size() - at) return false;
  for (uint64_t i = 0; i < nm; ++i) {
    if (v[at] > UINT32_MAX) return false;
    grid.members.push_back(static_cast<Ref>(v[at++]));
  }
  return at == v.size();
}

bool parse_sr_instance(std::string_view text,
                       SubstitutionRedundancyInstance &instance) {
  std::vector<uint64_t> v;
  if (!comma_values(text, v))
    return false;
  size_t at = 0;
  const auto count = [&]() -> uint64_t {
    return at < v.size() ? v[at++] : UINT64_MAX;
  };
  const auto literals = [&](std::vector<Lit> &out) {
    const uint64_t n = count();
    if (n == UINT64_MAX || n > v.size() - at)
      return false;
    for (uint64_t i = 0; i < n; ++i) {
      if (v[at] > UINT32_MAX)
        return false;
      out.push_back(static_cast<Lit>(v[at++]));
    }
    return true;
  };
  if (!literals(instance.clause) || !literals(instance.witness))
    return false;
  const uint64_t pairs = count();
  if (pairs == UINT64_MAX || pairs > (v.size() - at) / 2)
    return false;
  for (uint64_t i = 0; i < pairs; ++i) {
    if (v[at] > UINT32_MAX || v[at + 1] > UINT32_MAX)
      return false;
    instance.substitution.emplace_back(static_cast<Lit>(v[at]),
                                       static_cast<Lit>(v[at + 1]));
    at += 2;
  }
  return at == v.size();
}

bool parse_condition_instance(std::string_view text,
                              ConditionInstance &instance) {
  std::vector<uint64_t> values;
  if (!comma_values(text, values))
    return false;
  size_t at = 0;
  const auto literals = [&](std::vector<Lit> &out) {
    if (at == values.size())
      return false;
    const uint64_t count = values[at++];
    if (count > values.size() - at)
      return false;
    for (uint64_t i = 0; i < count; ++i) {
      if (values[at] > UINT32_MAX)
        return false;
      out.push_back(static_cast<Lit>(values[at++]));
    }
    return true;
  };
  return literals(instance.clause) && literals(instance.witness) &&
         at == values.size();
}

bool parse_independent_set_cover(std::string_view text,
                                 std::vector<uint32_t> &cover) {
  std::vector<uint64_t> values;
  if (!comma_values(text, values) || values.empty() ||
      values.front() != values.size() - 1)
    return false;
  cover.reserve(values.size() - 1);
  for (size_t i = 1; i < values.size(); ++i) {
    if (!values[i] || values[i] > UINT32_MAX) return false;
    cover.push_back(static_cast<uint32_t>(values[i]));
  }
  return true;
}

bool parse_gf3_affine_instance(std::string_view text,
                               Gf3AffineInstance &instance) {
  std::vector<uint64_t> values;
  if (!comma_values(text, values) || values.size() != 2 ||
      values[0] > UINT32_MAX || values[1] > UINT32_MAX)
    return false;
  instance.first_equation = static_cast<uint32_t>(values[0]);
  instance.second_equation = static_cast<uint32_t>(values[1]);
  return true;
}

AffineBasisActionLimits replay_affine_basis_limits(
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

bool parse_affine_basis_row(std::string_view text,
                            const AffineConsequenceParameters &parameters,
                            AffineBasisRowSelection &selection) {
  const auto saturated_add = [](uint64_t left, uint64_t right) {
    return right > UINT64_MAX - left ? UINT64_MAX : left + right;
  };
  const auto saturated_multiply = [](uint64_t left, uint64_t right) {
    return left && right > UINT64_MAX / left ? UINT64_MAX : left * right;
  };
  const uint64_t maximum_field_width = std::max(
      parameters.basis_maximum_intermediate_width,
      parameters.basis_maximum_direct_width);
  uint64_t maximum_fields = saturated_add(7, maximum_field_width);
  maximum_fields = saturated_add(maximum_fields,
                                 parameters.basis_maximum_sources);
  maximum_fields = saturated_add(
      maximum_fields,
      saturated_multiply(parameters.basis_maximum_sources,
                          3 + uint64_t{parameters.maximum_arity}));
  maximum_fields = saturated_add(
      maximum_fields, saturated_multiply(parameters.maximum_clauses, 2));
  maximum_fields = saturated_add(maximum_fields,
                                 parameters.maximum_snapshot_literals);
  std::vector<std::string_view> fields;
  while (!text.empty()) {
    if (fields.size() >= maximum_fields) return false;
    const size_t comma = text.find(',');
    const std::string_view field = text.substr(0, comma);
    if (field.empty()) return false;
    fields.push_back(field);
    if (comma == std::string_view::npos) text = {};
    else text.remove_prefix(comma + 1);
  }
  size_t at = 0;
  const auto u64 = [&](uint64_t &value) {
    return at < fields.size() && parse_u64(fields[at++], value);
  };
  const auto i32 = [&](int32_t &value) {
    if (at == fields.size()) return false;
    int64_t parsed = 0;
    const auto field = fields[at++];
    const auto result = std::from_chars(field.data(), field.data() + field.size(),
                                        parsed);
    if (result.ec != std::errc{} || result.ptr != field.data() + field.size() ||
        parsed < INT32_MIN || parsed > INT32_MAX)
      return false;
    value = static_cast<int32_t>(parsed);
    return true;
  };
  uint64_t value = 0;
  if (!u64(selection.owner_version) || !u64(value) || value > UINT32_MAX)
    return false;
  selection.elimination_profile = static_cast<uint32_t>(value);
  if (!u64(value) ||
      value > static_cast<uint64_t>(
                  affine_semantic_extension::BasisRowKind::Extended))
    return false;
  selection.kind =
      static_cast<affine_semantic_extension::BasisRowKind>(value);
  const bool extended = selection.kind ==
                        affine_semantic_extension::BasisRowKind::Extended;
  const uint32_t maximum_width =
      extended ? parameters.basis_maximum_intermediate_width
               : parameters.basis_maximum_direct_width;
  if (!u64(value) || value > maximum_width ||
      value > fields.size() - at)
    return false;
  selection.variables.reserve(static_cast<size_t>(value));
  for (uint64_t i = 0; i < value; ++i) {
    int32_t variable = 0;
    if (!i32(variable)) return false;
    selection.variables.push_back(variable);
  }
  if (!u64(value) || value > 1) return false;
  selection.rhs = value;
  if (!u64(value) || value < (extended ? 1u : 2u) ||
      value > parameters.basis_maximum_sources ||
      value > fields.size() - at)
    return false;
  const size_t source_count = static_cast<size_t>(value);
  selection.source_equation_ordinals.reserve(source_count);
  for (size_t i = 0; i < source_count; ++i) {
    uint64_t ordinal = 0;
    if (!u64(ordinal) || ordinal > SIZE_MAX) return false;
    selection.source_equation_ordinals.push_back(static_cast<size_t>(ordinal));
  }
  if (!u64(value) || value != source_count) return false;
  selection.source_equations.reserve(source_count);
  uint64_t clauses = 0, literals = 0;
  for (size_t equation_index = 0; equation_index < source_count;
       ++equation_index) {
    AffineBasisEquationIdentity equation;
    if (!u64(value) || !value || value > parameters.maximum_arity ||
        value >= 64 || value > fields.size() - at)
      return false;
    const size_t arity = static_cast<size_t>(value);
    equation.variables.reserve(arity);
    for (size_t i = 0; i < arity; ++i) {
      int32_t variable = 0;
      if (!i32(variable)) return false;
      equation.variables.push_back(variable);
    }
    if (!u64(value) || value > 1) return false;
    equation.rhs = value;
    if (!u64(value) || value != (uint64_t{1} << (arity - 1)) ||
        value > parameters.maximum_patterns ||
        value > parameters.maximum_clauses -
                    std::min(clauses, parameters.maximum_clauses))
      return false;
    equation.premises.reserve(static_cast<size_t>(value));
    clauses += value;
    for (uint64_t premise_index = 0; premise_index < value;
         ++premise_index) {
      AffineBasisSourceIdentity premise;
      uint64_t width = 0;
      if (!u64(premise.native_clause_id) || !u64(width) || width != arity ||
          width > fields.size() - at ||
          width > parameters.maximum_snapshot_literals -
                      std::min(literals,
                               parameters.maximum_snapshot_literals))
        return false;
      literals += width;
      premise.clause.reserve(arity);
      for (size_t i = 0; i < arity; ++i) {
        int32_t literal = 0;
        if (!i32(literal)) return false;
        premise.clause.push_back(literal);
      }
      equation.premises.push_back(std::move(premise));
    }
    selection.source_equations.push_back(std::move(equation));
  }
  return at == fields.size() &&
         affine_basis_row_selection_within_limits(
             selection, replay_affine_basis_limits(parameters));
}

bool parse_guarded_cover(std::string_view text,
                          DirectCspGuardedCoverActionSelection &out) {
  std::vector<int64_t> values;
  while (!text.empty()) {
    const auto comma = text.find(',');
    const auto token = text.substr(0, comma);
    int64_t v;
    const auto parsed = std::from_chars(token.data(), token.data()+token.size(), v);
    if (token.empty() || parsed.ec != std::errc{} || parsed.ptr != token.data()+token.size()) return false;
    values.push_back(v);
    if (comma == std::string_view::npos) text = {};
    else { text.remove_prefix(comma+1); if (text.empty()) return false; }
  }
  size_t at = 0;
  const auto number = [&](uint32_t &v) {
    if (at == values.size() || values[at] < 0 || values[at] > UINT32_MAX) return false;
    v = static_cast<uint32_t>(values[at++]); return true;
  };
  const auto literal = [&](int32_t &v) {
    if (at == values.size() || !values[at] || values[at] <= INT32_MIN || values[at] > INT32_MAX) return false;
    v = static_cast<int32_t>(values[at++]); return true;
  };
  const auto literals = [&](std::vector<int32_t> &v) {
    uint32_t count;
    if (!number(count) || count > values.size()-at) return false;
    for (uint32_t i=0; i<count; ++i) { int32_t l; if (!literal(l)) return false; v.push_back(l); }
    return true;
  };
  uint32_t count;
  if (!number(count) || count > values.size()-at) return false;
  for (uint32_t i=0; i<count; ++i) {
    out.domains.emplace_back(); uint32_t selected;
    if (!literals(out.domains.back()) || !number(selected)) return false;
    out.selected_values.push_back(selected);
  }
  if (!literals(out.cover) || !number(count) || count > values.size()-at) return false;
  for (uint32_t i=0; i<count; ++i) {
    out.exact_or.emplace_back();
    if (!literal(out.exact_or.back().output) || !literals(out.exact_or.back().inputs)) return false;
  }
  if (!number(count) || count > (values.size()-at)/4) return false;
  for (uint32_t i=0; i<count; ++i) {
    out.blockers.emplace_back(); auto &b = out.blockers.back();
    if (!literal(b.literal) || !number(b.domain) || !number(b.value) || !number(b.or_definition)) return false;
  }
  return at == values.size();
}

bool parse_direct_csp_support(std::string_view text,
                              DirectCspSupportActionSelection &selection) {
  std::vector<int64_t> values;
  while (!text.empty()) {
    const size_t comma = text.find(',');
    const std::string_view token = text.substr(0, comma);
    if (token.empty()) return false;
    int64_t value = 0;
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(),
                                        value);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size())
      return false;
    values.push_back(value);
    if (comma == std::string_view::npos) text = {};
    else text.remove_prefix(comma + 1);
  }
  size_t at = 0;
  const auto domain = [&](std::vector<int32_t> &out) {
    if (at == values.size() || values[at] < 0) return false;
    const uint64_t count = static_cast<uint64_t>(values[at++]);
    if (count > values.size() - at) return false;
    out.reserve(static_cast<size_t>(count));
    for (uint64_t i = 0; i < count; ++i) {
      if (values[at] < INT32_MIN || values[at] > INT32_MAX) return false;
      out.push_back(static_cast<int32_t>(values[at++]));
    }
    return true;
  };
  if (!domain(selection.left_values) || !domain(selection.right_values) ||
      at == values.size() || values[at] < 0 || values[at] > UINT32_MAX)
    return false;
  selection.selected_left_value = static_cast<uint32_t>(values[at++]);
  return at == values.size();
}


bool parse_nary_csp_equality(std::string_view text,
                             NaryCspEqualityActionSelection &selection) {
  std::vector<int64_t> values;
  while (!text.empty()) {
    const size_t comma = text.find(',');
    const std::string_view token = text.substr(0, comma);
    if (token.empty()) return false;
    int64_t value = 0;
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(),
                                        value);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size())
      return false;
    values.push_back(value);
    if (comma == std::string_view::npos) text = {};
    else text.remove_prefix(comma + 1);
  }
  size_t at = 0;
  if (at == values.size() || values[at] != 3) return false;
  ++at;
  for (uint32_t domain = 0; domain != 3; ++domain) {
    if (at == values.size() || values[at] < 0) return false;
    const uint64_t count = static_cast<uint64_t>(values[at++]);
    if (count > values.size() - at) return false;
    std::vector<int32_t> literals;
    literals.reserve(static_cast<size_t>(count));
    for (uint64_t value = 0; value < count; ++value) {
      if (values[at] < INT32_MIN || values[at] > INT32_MAX) return false;
      literals.push_back(static_cast<int32_t>(values[at++]));
    }
    selection.domains.push_back(std::move(literals));
  }
  if (values.size() - at != 3) return false;
  for (size_t i = 0; i != 3; ++i)
    if (values[at + i] < 0 || values[at + i] > UINT32_MAX) return false;
  selection.coordinates = {static_cast<uint32_t>(values[at]),
                           static_cast<uint32_t>(values[at + 1]),
                           static_cast<uint32_t>(values[at + 2])};
  return nary_csp_equality_action_selection_valid(selection);
}

bool parse_nary_csp_equality_transitivity(std::string_view text,
                                        NaryCspEqualityTransitivitySelection &out) {
  std::vector<int64_t> values;
  while(!text.empty()) {
    const auto comma=text.find(','); const auto token=text.substr(0,comma);
    int64_t value=0;
    auto parsed=std::from_chars(token.data(),token.data()+token.size(),value);
    if(token.empty() || parsed.ec!=std::errc{} || parsed.ptr!=token.data()+token.size()) return false;
    values.push_back(value);
    if(comma==std::string_view::npos) text={}; else text.remove_prefix(comma+1);
  }
  size_t at=0;
  const auto domain=[&](std::vector<int32_t> &d) {
    if(at==values.size() || values[at]<2 || uint64_t(values[at])>values.size()-at-1) return false;
    const uint64_t count=values[at++];
    for(uint64_t i=0;i<count;++i) {
      if(!values[at] || values[at]<=INT32_MIN || values[at]>INT32_MAX) return false;
      d.push_back(int32_t(values[at++]));
    }
    return true;
  };
  const auto u32=[&](uint32_t &v) {
    if(at==values.size() || values[at]<0 || uint64_t(values[at])>UINT32_MAX) return false;
    v=uint32_t(values[at++]); return true;
  };
  for(auto &d:out.domains) if(!domain(d)) return false;
  for(size_t e=0;e<3;++e) {
    auto &key=out.equalities[e];
    for(size_t d=0;d<3;++d) { key.domains.emplace_back(); if(!domain(key.domains.back())) return false; }
    if(!u32(key.first_equal_domain) || !u32(key.second_equal_domain) || !u32(key.residual_domain) ||
       !u32(out.equality_variables[e])) return false;
  }
  return at==values.size() && nary_csp_equality_transitivity_selection_valid(out);
}

bool parse_nary_csp_equality_amo(std::string_view text,
                                 NaryCspEqualityAmoSelection &selection) {
  std::vector<int64_t> values;
  while (!text.empty()) {
    const size_t comma = text.find(',');
    const std::string_view token = text.substr(0, comma);
    if (token.empty()) return false;
    int64_t value = 0;
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(),
                                        value);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size())
      return false;
    values.push_back(value);
    if (comma == std::string_view::npos) text = {};
    else text.remove_prefix(comma + 1);
  }
  size_t at = 0;
  if (values.empty() || values[at++] != 3) return false;
  for (uint32_t domain = 0; domain != 3; ++domain) {
    if (at == values.size() || values[at] < 0) return false;
    const uint64_t count = static_cast<uint64_t>(values[at++]);
    if (count > values.size() - at) return false;
    std::vector<int32_t> literals;
    for (uint64_t i = 0; i != count; ++i) {
      if (values[at] < INT32_MIN || values[at] > INT32_MAX) return false;
      literals.push_back(static_cast<int32_t>(values[at++]));
    }
    selection.domains.push_back(std::move(literals));
  }
  return at == values.size() &&
         nary_csp_equality_amo_selection_valid(selection);
}

} // namespace

bool random_online_strategy_program(Kernel &k, uint64_t seed,
                                    const RandomOnlineConfig &config,
                                    OnlineStrategyProgram &out,
                                    std::string *error,
                                    volatile sig_atomic_t *stop) {
  const auto reject = [error](const char *message) {
    if (error)
      *error = message;
    return false;
  };
  if (!config.sr_joint_positive_denominator ||
      config.sr_joint_positive_numerator > config.sr_joint_positive_denominator)
    return reject("random-online joint SR probability is invalid");
  if (!config.affine_joint_positive_denominator ||
      config.affine_joint_positive_numerator >
          config.affine_joint_positive_denominator)
    return reject("random-online joint affine probability is invalid");
  if (!config.maximum_sequence_length || !config.maximum_selected_batch ||
      !config.maximum_repeat_count || !config.maximum_application_budget ||
      !config.maximum_bve_tick_budget ||
      !config.maximum_propagation_budget ||
      !config.maximum_rule_work_budget ||
      !config.maximum_bve_definition_cores || !config.analyze_reference.valid() ||
      !config.subsume_reference.valid() ||
      !config.strengthen_reference.valid() ||
      !config.congruence_reference.valid() ||
      !config.vivify_reference.valid() || !config.bce_reference.valid() ||
      !config.ternary_reference.valid() || !config.factor_reference.valid() ||
      !config.transitive_reference.valid() ||
      !config.substitute_reference.valid() ||
      !config.reduce_reference.valid() || !config.walk_reference.valid() ||
      !config.reorder_reference.valid() ||
      !config.maximum_parity_model_variables ||
      !config.maximum_parity_model_groups ||
      !config.maximum_parity_model_constraints ||
      config.maximum_parity_model_free_variables > 63 ||
      !config.maximum_parity_model_candidates ||
      !config.parity_model_reference.valid() ||
      !config.maximum_onehot_model_variables ||
      !config.maximum_onehot_model_domains ||
      !config.maximum_onehot_model_constraints ||
      !config.maximum_onehot_model_equation_arity ||
      !config.maximum_onehot_model_candidates ||
      !config.onehot_model_reference.valid() ||
      !config.balanced_signature_model_reference.valid() ||
      !config.fermat_model_reference.valid() ||
      !config.lookup_circuit_model_reference.valid() ||
      !config.simon_model_reference.valid() ||
      !config.affine_model_reference.valid() ||
      !config.exact_cover_model_reference.valid() ||
      !config.independent_set_model_reference.valid() ||
      !config.graph_labeling_reference.valid() ||
      !config.stconn_proof_reference.valid() ||
      !config.signed_cardinality_proof_reference.valid() ||
      !config.ramsey_startup_reference.valid() ||
      !config.parity_consequence_reference.valid() ||
      !executable_exact_cover_profile(
          config.exact_cover_extension_reference) ||
      !config.direct_php_refutation_reference.valid() ||
      !config.gf3_gaussian_consequence_reference.valid() ||
      !config.gf3_general_row_consequence_reference.valid() ||
      !config.maximum_sr_validation_ticks || !config.maximum_sr_matrices ||
      !config.maximum_sr_instances ||
      config.maximum_sr_orbitopal_clique_minimum_columns < 2 ||
      !config.maximum_sr_orbitopal_clique_vertices ||
      !config.maximum_sr_orbitopal_clique_ticks ||
      !config.maximum_sr_orbitopal_product_states ||
      !config.maximum_sr_detection_ticks ||
      !config.maximum_sr_general_candidates_per_advance ||
      !config.maximum_sr_graph_vertices || !config.maximum_sr_graph_edges ||
      !config.maximum_sr_symmetry_generators ||
      !config.maximum_sr_backend_search_budget ||
      !config.maximum_sr_backend_support_budget ||
      !config.maximum_sr_backend_component_size ||
      !config.maximum_sr_symmetry_fixing_instances ||
      !config.maximum_sr_symmetry_generator_instances_per_pass ||
      !config.maximum_sr_symmetry_group_ticks ||
      !config.maximum_sr_symmetry_error_bound ||
      !config.sr_reference.valid())
    return reject("random-online maxima must be positive");

  std::vector<int> available;
  if (config.rules.empty()) {
    for (const RuleVTable &rule : tactix_rules())
      if (rule_guard(rule, k) && std::string_view(rule.name) != "IndependentSetCover" &&
          std::string_view(rule.name) != "Gf3AffineConsequence" &&
          std::string_view(rule.name) != "DirectCspSupport" &&
          std::string_view(rule.name) != "NaryCspEquality" &&
          std::string_view(rule.name) != "NaryCspEqualityTransitivity" &&
          std::string_view(rule.name) != "NaryCspEqualityAmo" &&
          std::string_view(rule.name) != "Gf3GaussianConsequence" &&
          std::string_view(rule.name) != "Gf3GeneralRowConsequence" &&
          std::string_view(rule.name) != "CardinalitySearch" &&
          std::string_view(rule.name) != "TseitinCycleBreaking")
        available.push_back(static_cast<int>(rule.id));
  } else {
    for (const std::string &name : config.rules) {
      const int id = tactix_rule_id(name.c_str());
      if (id < 0)
        return reject("random-online action is not registered");
      if (rule_guard(tactix_rules()[id], k) && name != "IndependentSetCover" &&
          name != "Gf3AffineConsequence" && name != "DirectCspSupport" &&
          name != "NaryCspEquality" &&
          name != "NaryCspEqualityTransitivity" &&
          name != "NaryCspEqualityAmo" &&
          name != "Gf3GaussianConsequence" &&
          name != "Gf3GeneralRowConsequence" &&
          name != "CardinalitySearch" &&
          name != "TseitinCycleBreaking")
        available.push_back(id);
    }
  }
  if (available.empty())
    return reject("no requested action is applicable");

  uint64_t rng = seed ? seed : 1;
  out = {};
  const uint32_t length =
      1 + static_cast<uint32_t>(bounded(rng, config.maximum_sequence_length));
  for (uint32_t i = 0; i < length; ++i) {
    const RuleVTable &rule =
        tactix_rules()[available[bounded(rng, available.size())]];
    OnlineInvocation step;
    step.id = static_cast<StepId>(i) + 1;
    step.rule = rule.name;
    step.parameters = random_parameters(k, rng, step.rule, config);
    step.work_unit = step.rule == "BVE"
                         ? WorkUnit::BveTicks
                         : step.rule == "Propagate" ? WorkUnit::Propagations
                                                    : WorkUnit::Applications;
    const uint64_t budget_max =
        step.rule == "BVE"
            ? config.maximum_bve_tick_budget
            : step.rule == "Propagate" ? config.maximum_propagation_budget
                                       : config.maximum_application_budget;
    step.budget = bounded(rng, BUDGET_SHAPES) == 0
                      ? WorkLimit::unlimited()
                      : WorkLimit::bounded(1 + bounded(rng, budget_max));
    const uint64_t native_budget_shape = bounded(rng, 3);
    step.rule_work_budget.mode =
        static_cast<RuleWorkBudgetMode>(native_budget_shape);
    if (step.rule_work_budget.mode == RuleWorkBudgetMode::Bounded)
      step.rule_work_budget.amount =
          optional_positive(rng, config.maximum_rule_work_budget);
    step.material.touch_filter =
        static_cast<SettingOverride>(bounded(rng, 3));
    step.material.likely_kept_redundant =
        static_cast<SettingOverride>(bounded(rng, 3));
    const bool sampled_post_propagation =
        config.sample_post_propagation && bounded(rng, 2);
    const bool requires_separate_propagation =
        step.rule == "DirectPhpRefutation" ||
        step.rule == "TseitinCycleBreaking";
    if (sampled_post_propagation && !requires_separate_propagation)
      step.post_application = PostApplication::Propagate;

    std::vector<uint64_t> instances;
    std::vector<SubstitutionRedundancyInstance> sr_instances;
    InvocationStatus enumerated = InvocationStatus::Completed;
    if (step.rule == "SubstitutionRedundancy") {
      const auto &parameters =
          std::get<SubstitutionRedundancyParameters>(step.parameters);
      if (parameters.producer ==
          SubstitutionRedundancyProducer::Orbitopal) {
        const OrbitopalEnumerationResult result =
            substitution_redundancy_enumerate_orbitopal(
                k, parameters,
                [&](const SubstitutionRedundancyInstance &instance) {
                  sr_instances.push_back(instance);
                  return true;
                },
                stop);
        if (result.status == OrbitopalEnumerationStatus::Interrupted)
          enumerated = InvocationStatus::Interrupted;
      } else if (parameters.producer ==
                 SubstitutionRedundancyProducer::GraphSymmetry) {
        const SymmetryEnumerationResult result =
            substitution_redundancy_enumerate_symmetry(
                k, parameters,
                [&](const SubstitutionRedundancyInstance &instance) {
                  sr_instances.push_back(instance);
                  return true;
                },
                stop);
        if (result.status == SymmetryEnumerationStatus::Interrupted)
          enumerated = InvocationStatus::Interrupted;
        else if (result.status == SymmetryEnumerationStatus::Invalid)
          return reject("graph symmetry enumeration rejected its input");
        else if (result.status == SymmetryEnumerationStatus::Failed)
          return reject("graph symmetry enumeration failed");
        else if (result.status == SymmetryEnumerationStatus::ResourceExhausted)
          enumerated = InvocationStatus::ResourceExhausted;
      }
    } else {
      enumerated = enumerate_complete_instances(k, rule, instances, stop);
    }
    if (enumerated == InvocationStatus::Interrupted)
      return reject("random-online generation interrupted");
    if (enumerated != InvocationStatus::Completed)
      return reject("complete instance enumeration exhausted resources");
    const size_t semantic_size = step.rule == "SubstitutionRedundancy"
                                     ? sr_instances.size()
                                     : instances.size();
    const bool whole_rule_only =
        semantic_size == 0 ||
        (instances.size() == 1 && instances.front() == INSTANCE_CANONICAL);
    uint64_t fixed_lucky_instance = INSTANCE_CANONICAL;
    if (std::holds_alternative<ParityModelParameters>(step.parameters))
      fixed_lucky_instance = 6;
    else if (std::holds_alternative<OnehotModelParameters>(step.parameters))
      fixed_lucky_instance = bounded(rng, 2) ? 7 : 9;
    else if (std::holds_alternative<BalancedSignatureModelParameters>(step.parameters))
      fixed_lucky_instance = 10;
    else if (std::holds_alternative<AffineModelParameters>(step.parameters))
      fixed_lucky_instance = 8;
    else if (std::holds_alternative<FermatModelParameters>(step.parameters))
      fixed_lucky_instance = 11;
    else if (std::holds_alternative<LookupCircuitModelParameters>(step.parameters))
      fixed_lucky_instance = 13;
    else if (std::holds_alternative<SimonModelParameters>(step.parameters))
      fixed_lucky_instance = 14;
    else if (std::holds_alternative<StconnProofParameters>(step.parameters))
      fixed_lucky_instance = 15;
    else if (std::holds_alternative<SignedCardinalityProofParameters>(
                 step.parameters))
      fixed_lucky_instance = 16;
    else if (std::holds_alternative<ExactCoverModelParameters>(step.parameters))
      fixed_lucky_instance = 17;
    else if (std::holds_alternative<IndependentSetModelParameters>(step.parameters))
      fixed_lucky_instance = 18;
    else if (std::holds_alternative<GraphLabelingParameters>(step.parameters)) {
      constexpr uint64_t graph_instances[] = {
          LUCKY_GRAPH_LABELING, LUCKY_GRAPH_AUTHENTICATE, LUCKY_GRAPH_CONTINUE};
      fixed_lucky_instance = graph_instances[bounded(rng, 3)];
    }
    else if (std::holds_alternative<RamseyStartupParameters>(step.parameters))
      fixed_lucky_instance = 22;
    const bool fixed_lucky = fixed_lucky_instance != INSTANCE_CANONICAL;
    const uint64_t granularity =
        fixed_lucky ? 0
                    : whole_rule_only ? 2 + bounded(rng, 2)
                                      : bounded(rng, GRANULARITY_CHOICES);
    if (fixed_lucky) {
      step.granularity = InvocationGranularity::OneInstance;
      step.instances = {fixed_lucky_instance};
    } else if (granularity == 0) {
      step.granularity = InvocationGranularity::OneInstance;
      const size_t selected = bounded(rng, semantic_size);
      if (step.rule == "SubstitutionRedundancy")
        step.substitution_redundancy_instances.push_back(
            sr_instances[selected]);
      else
        step.instances.push_back(instances[selected]);
    } else if (granularity == 1) {
      step.granularity = InvocationGranularity::SelectedBatch;
      const size_t count = 1 + static_cast<size_t>(bounded(
          rng, std::min<size_t>(semantic_size,
                                config.maximum_selected_batch)));
      const size_t start = bounded(rng, semantic_size);
      size_t stride = 1 + bounded(rng, semantic_size);
      while (std::gcd(stride, semantic_size) != 1)
        ++stride;
      for (size_t j = 0; j < count; ++j) {
        const size_t selected = (start + j * stride) % semantic_size;
        if (step.rule == "SubstitutionRedundancy")
          step.substitution_redundancy_instances.push_back(
              sr_instances[selected]);
        else
          step.instances.push_back(instances[selected]);
      }
    } else if (granularity == 2) {
      step.granularity = InvocationGranularity::CompleteCurrentPass;
    } else {
      step.granularity = InvocationGranularity::CanonicalApplication;
    }

    // CompleteCurrentPass materializes or lazily visits the whole current
    // semantic frontier. For Factor that frontier is exponential, and for
    // clause rules it can still be formula-sized. Random exploration keeps
    // unlimited budgets in its domain for explicit/canonical applications,
    // but every complete pass receives the caller's finite safety ceiling.
    if (step.granularity == InvocationGranularity::CompleteCurrentPass &&
        step.budget.is_unlimited())
      step.budget = WorkLimit::bounded(1 + bounded(rng, budget_max));

    const uint64_t repeat_choice = bounded(rng, REPEAT_CHOICES);
    if (repeat_choice == 0) {
      step.repeat = {RepeatMode::Once, 1};
    } else if (repeat_choice == 1) {
      step.repeat = {RepeatMode::Count,
                     1 + bounded(rng, config.maximum_repeat_count)};
    } else {
      step.repeat = {RepeatMode::UntilScopedFixpoint,
                     1 + bounded(rng, config.maximum_repeat_count)};
    }
    out.sequence.push_back(std::move(step));
  }

  const uint64_t sequence_repeat = bounded(rng, REPEAT_CHOICES);
  if (sequence_repeat == 0)
    out.repeat = {RepeatMode::Once, 1};
  else if (sequence_repeat == 1)
    out.repeat = {RepeatMode::Count,
                  1 + bounded(rng, config.maximum_repeat_count)};
  else
    out.repeat = {RepeatMode::UntilScopedFixpoint,
                  1 + bounded(rng, config.maximum_repeat_count)};
  // A derived stream: earlier draws and every later one stay unchanged.
  uint64_t size_scale_state = rng ^ 0x53495a455343414cull;
  if (out.repeat.mode == RepeatMode::UntilScopedFixpoint &&
      !bounded(size_scale_state, 4))
    out.repeat.mode = RepeatMode::UntilSizeScaledFixpoint;

  if (!validate_online_strategy_program(out, error))
    return false;
  if (error)
    error->clear();
  return true;
}

std::string serialize_online_strategy_program(
    const OnlineStrategyProgram &program) {
  std::ostringstream out;
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
    else if (const auto *p = std::get_if<BveParameters>(&step.parameters))
      out << ":bve5(" << p->gate_mask << ',' << p->growth_bound << ','
          << p->occurrence_limit << ',' << p->clause_size_limit << ','
          << p->xor_arity_limit << ',' << p->resolvent_size_limit << ','
          << p->backward_occurrence_limit << ',' << p->definition_budget << ','
          << p->definition_cores << ','
          << p->definition_core_budget_multiplier << ',' << p->backward << ','
          << static_cast<unsigned>(p->candidate_scope) << ','
          << p->resolvent_count_limit << ','
          << static_cast<unsigned>(p->traversal) << ','
          << p->on_the_fly_strengthening << ')';
    else if (const auto *p =
                 std::get_if<SubsumeForwardParameters>(&step.parameters))
      out << ":c1(" << p->clause_size_limit << ','
          << p->large_occurrence_limit << ','
          << p->binary_occurrence_limit << ')';
    else if (const auto *p =
                 std::get_if<StrengthenParameters>(&step.parameters))
      out << ":c2(" << p->clause_size_limit << ',' << p->occurrence_limit
          << ')';
    else if (const auto *p =
                 std::get_if<ParityConsequenceParameters>(&step.parameters))
      out << ":parity-consequence(" << p->maximum_clauses << ','
          << p->maximum_groups << ',' << p->selection_work << ','
          << p->intermediate_width << ',' << p->source_count << ','
          << p->output_width << ',' << p->descending << ','
          << p->maximum_proof_additions << ',' << p->maximum_tree_variables
          << ',' << p->consequence << ')';
    else if (const auto *p =
                 std::get_if<AffineConsequenceParameters>(&step.parameters)) {
      if (p->semantics ==
          AffineConsequenceSemantics::SelectedConsequenceV1)
        out << ":affine-consequence1(" << p->maximum_clauses << ','
          << p->maximum_snapshot_literals << ',' << p->maximum_snapshot_work
          << ',' << p->maximum_groups << ',' << p->maximum_patterns << ','
          << p->maximum_elimination_work << ',' << p->maximum_arity << ','
          << p->maximum_intermediate_width << ',' << p->maximum_sources << ','
          << p->maximum_output_width << ',' << p->maximum_proof_additions << ','
          << p->maximum_proof_deletions << ',' << p->maximum_journal_bytes
          << ',' << p->consequence << ')';
      else
        out << ":affine-consequence2(" << p->maximum_clauses << ','
            << p->maximum_snapshot_literals << ','
            << p->maximum_snapshot_work << ',' << p->maximum_groups << ','
            << p->maximum_patterns << ',' << p->maximum_elimination_work
            << ',' << p->maximum_arity << ','
            << p->maximum_intermediate_width << ',' << p->maximum_sources
            << ',' << p->maximum_output_width << ','
            << p->maximum_proof_additions << ','
            << p->maximum_proof_deletions << ','
            << p->maximum_journal_bytes << ',' << p->basis_maximum_work
            << ',' << p->basis_maximum_intermediate_width << ','
            << p->basis_maximum_sources << ',' << p->basis_maximum_premises
            << ',' << p->basis_maximum_nodes << ','
            << p->basis_maximum_direct_width << ','
            << p->maximum_external_extent << ',' << p->maximum_session_keys
            << ',' << p->maximum_session_work << ')';
    }
    else if (const auto *profile=std::get_if<AffineMatrixInitializeParameters>(&step.parameters)) {
      const auto *p=&profile->basis;
        out << ":affine-matrix-persistent1(" << p->maximum_clauses << ','
            << p->maximum_snapshot_literals << ','
            << p->maximum_snapshot_work << ',' << p->maximum_groups << ','
            << p->maximum_patterns << ',' << p->maximum_elimination_work
            << ',' << p->maximum_arity << ','
            << p->maximum_intermediate_width << ',' << p->maximum_sources
            << ',' << p->maximum_output_width << ','
            << p->maximum_proof_additions << ','
            << p->maximum_proof_deletions << ','
            << p->maximum_journal_bytes << ',' << p->basis_maximum_work
            << ',' << p->basis_maximum_intermediate_width << ','
            << p->basis_maximum_sources << ',' << p->basis_maximum_premises
            << ',' << p->basis_maximum_nodes << ','
            << p->basis_maximum_direct_width << ','
            << p->maximum_external_extent << ',' << p->maximum_session_keys
            << ',' << p->maximum_session_work << ',' << static_cast<unsigned>(profile->admission)
            << ',' << static_cast<unsigned>(profile->operation) << ',' << profile->maximum_retained_bytes
            << ',' << profile->maximum_deferred_deletions << ',' << profile->maximum_deferred_literal_work << ')';
    }
    else if (const auto *p =
                 std::get_if<ExactCoverExtensionParameters>(&step.parameters))
      out << ":exact-cover-extension1(" << p->arity << ',' << p->maximum_items
          << ',' << p->maximum_source_clauses << ','
          << p->maximum_source_literals << ',' << p->maximum_recognition_work
          << ',' << p->maximum_premise_references << ','
          << p->maximum_definitions << ',' << p->maximum_additions << ','
          << p->maximum_literal_occurrences << ',' << p->maximum_extent_growth
          << ',' << p->maximum_external_extent << ','
          << p->maximum_journal_bytes << ')';
    else if (const auto *p =
                 std::get_if<IndependentSetCoverParameters>(&step.parameters))
      out << ":independent-set-cover1(" << p->maximum_clauses << ','
          << p->maximum_literals << ',' << p->maximum_recognition_work << ','
          << p->maximum_proof_work << ',' << p->maximum_proof_additions << ','
          << p->maximum_proof_deletions << ','
          << p->maximum_proof_literal_occurrences << ','
          << p->maximum_journal_bytes << ',' << p->maximum_vertices << ','
          << p->maximum_target << ')';
    else if (const auto *p =
                 std::get_if<Gf3AffineConsequenceParameters>(&step.parameters))
      out << ":gf3-affine-consequence1(" << p->maximum_source_clauses << ','
          << p->maximum_source_literals << ',' << p->maximum_domains << ','
          << p->maximum_equations << ',' << p->maximum_equation_arity << ','
          << p->maximum_tuples_per_equation << ',' << p->maximum_result_arity
          << ',' << p->maximum_result_tuples << ',' << p->maximum_additions
          << ',' << p->maximum_addition_literals << ','
          << p->maximum_recognition_work << ',' << p->maximum_planning_work
          << ',' << p->maximum_duplicate_scan_work << ','
          << p->maximum_revalidation_work << ',' << p->maximum_proof_work
          << ',' << p->maximum_proof_additions << ','
          << p->maximum_proof_literal_occurrences << ','
          << p->maximum_arena_words << ',' << p->maximum_journal_bytes << ','
          << p->maximum_external_extent << ',' << p->maximum_session_keys
          << ',' << p->maximum_session_work << ')';
    else if (const auto *p =
                 std::get_if<DirectCspGuardedCoverParameters>(&step.parameters))
      out << ":direct-csp-guarded-cover2(" << p->maximum_source_clauses << ',' << p->maximum_source_literals << ',' << p->maximum_recognition_work << ',' << p->maximum_domains << ',' << p->maximum_domain_values << ',' << p->maximum_cover_literals << ',' << p->maximum_or_definitions << ',' << p->maximum_or_inputs << ',' << p->maximum_output_literals << ',' << p->maximum_proof_work << ',' << p->maximum_proof_additions << ',' << p->maximum_proof_literal_occurrences << ',' << p->maximum_journal_bytes << ',' << p->maximum_external_extent << ',' << p->maximum_session_keys << ',' << p->maximum_session_work << ')';
    else if (const auto *p =
                 std::get_if<DirectCspSupportParameters>(&step.parameters))
      out << ":direct-csp-support1(" << p->maximum_source_clauses << ','
          << p->maximum_source_literals << ',' << p->maximum_recognition_work
          << ',' << p->maximum_domain_values << ','
          << p->maximum_direct_clause_literals << ','
          << p->maximum_definition_clause_literals << ','
          << p->maximum_proof_work << ',' << p->maximum_proof_additions << ','
          << p->maximum_proof_literal_occurrences << ','
          << p->maximum_journal_bytes << ',' << p->maximum_extent_growth << ','
          << p->maximum_external_extent << ',' << p->maximum_session_keys
          << ',' << p->maximum_session_work << ',' << p->allow_extension << ')';
    else if (const auto *p =
                 std::get_if<NaryCspEqualityParameters>(&step.parameters))
      out << ":nary-csp-equality1(" << p->maximum_source_clauses << ','
          << p->maximum_source_literals << ',' << p->maximum_recognition_work
          << ',' << p->maximum_domain_values << ','
          << p->maximum_forbidden_tuples << ',' << p->maximum_proof_work << ','
          << p->maximum_proof_additions << ','
          << p->maximum_proof_literal_occurrences << ','
          << p->maximum_journal_bytes << ',' << p->maximum_extent_growth << ','
          << p->maximum_external_extent << ',' << p->maximum_session_keys << ')';
    else if (const auto *p = std::get_if<NaryCspEqualityTransitivityParameters>(&step.parameters))
      out << ":nary-csp-equality-transitivity1(" << p->maximum_source_clauses << ',' << p->maximum_source_literals << ',' << p->maximum_recognition_work << ',' << p->maximum_planning_work << ',' << p->maximum_domain_values << ',' << p->maximum_forbidden_tuples << ',' << p->maximum_proof_work << ',' << p->maximum_proof_additions << ',' << p->maximum_proof_literal_occurrences << ',' << p->maximum_journal_bytes << ',' << p->maximum_external_extent << ',' << p->maximum_session_keys << ',' << p->maximum_session_bytes << ',' << p->maximum_arena_words << ',' << p->maximum_watch_words << ',' << p->maximum_preparation_bytes << ')';
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
    else if (const auto *p =
                 std::get_if<NaryCspEqualityAmoParameters>(&step.parameters))
      out << ":nary-csp-equality-amo1(" << p->maximum_source_clauses << ','
          << p->maximum_source_literals << ',' << p->maximum_recognition_work
          << ',' << p->maximum_domain_values << ','
          << p->maximum_forbidden_tuples << ',' << p->maximum_proof_work << ','
          << p->maximum_proof_additions << ','
          << p->maximum_proof_literal_occurrences << ','
          << p->maximum_journal_bytes << ',' << p->maximum_external_extent
          << ',' << p->maximum_session_keys << ')';
    else if (const auto *p =
                 std::get_if<DirectPhpRefutationParameters>(&step.parameters))
      out << ":direct-php-refutation1(" << p->maximum_snapshot_clauses << ','
          << p->maximum_snapshot_literals << ',' << p->maximum_snapshot_work
          << ',' << p->maximum_recognition_clauses << ','
          << p->maximum_recognition_literals << ','
          << p->maximum_recognition_work << ',' << p->maximum_row_candidates
          << ',' << p->maximum_row_combinations << ','
          << p->maximum_mapping_trials << ',' << p->maximum_holes << ','
          << p->maximum_proof_additions << ',' << p->maximum_proof_deletions
          << ',' << p->maximum_proof_literals << ','
          << p->maximum_fresh_variables << ',' << p->maximum_proof_events
          << ',' << p->maximum_replay_work << ')';
    else if (const auto *p = std::get_if<Gf3GaussianConsequenceParameters>(
                 &step.parameters))
      out << ":gf3-gaussian-consequence1(" << p->maximum_source_clauses << ','
          << p->maximum_source_literals << ',' << p->maximum_domains << ','
          << p->maximum_equations << ',' << p->maximum_equation_arity << ','
          << p->maximum_tuples_per_equation << ',' << p->maximum_bridge_work << ','
          << p->maximum_bridge_source_clauses << ',' << p->maximum_bridge_source_literals << ','
          << p->maximum_bridge_domains << ',' << p->maximum_bridge_equations << ','
          << p->maximum_bridge_terms << ',' << p->maximum_authentication_references << ','
          << p->maximum_references_per_equation << ',' << p->maximum_gaussian_work << ','
          << p->maximum_fill_entries << ',' << p->maximum_intermediate_width << ','
          << p->maximum_provenance_terms << ',' << p->maximum_derivations << ','
          << p->maximum_retained_plan_bytes << ',' << p->maximum_certificate_work << ','
          << p->maximum_events << ',' << p->maximum_event_literals << ','
          << p->maximum_active_clauses << ',' << p->maximum_retained_source_references << ','
          << p->maximum_event_width << ',' << p->maximum_branch_domains << ','
          << p->maximum_consequences << ',' << p->maximum_capture_work << ','
          << p->maximum_revalidation_work << ',' << p->maximum_proof_additions << ','
          << p->maximum_proof_literal_occurrences << ',' << p->maximum_journal_bytes << ','
          << p->maximum_identity_bytes << ',' << p->maximum_external_extent << ','
          << p->maximum_session_keys << ',' << p->maximum_session_work << ','
          << p->maximum_session_identity_bytes << ')';
    else if (const auto *p = std::get_if<Gf3GeneralRowConsequenceParameters>(
                 &step.parameters)) {
      const auto &g=p->gaussian;
      out << ":gf3-general-row1(" << g.maximum_source_clauses << ','
          << g.maximum_source_literals << ',' << g.maximum_domains << ','
          << g.maximum_equations << ',' << g.maximum_equation_arity << ','
          << g.maximum_tuples_per_equation << ',' << g.maximum_bridge_work << ','
          << g.maximum_bridge_source_clauses << ',' << g.maximum_bridge_source_literals << ','
          << g.maximum_bridge_domains << ',' << g.maximum_bridge_equations << ','
          << g.maximum_bridge_terms << ',' << g.maximum_authentication_references << ','
          << g.maximum_references_per_equation << ',' << g.maximum_gaussian_work << ','
          << g.maximum_fill_entries << ',' << g.maximum_intermediate_width << ','
          << g.maximum_provenance_terms << ',' << g.maximum_derivations << ','
          << g.maximum_retained_plan_bytes << ',' << g.maximum_certificate_work << ','
          << g.maximum_events << ',' << g.maximum_event_literals << ','
          << g.maximum_active_clauses << ',' << g.maximum_retained_source_references << ','
          << g.maximum_event_width << ',' << g.maximum_branch_domains << ','
          << g.maximum_consequences << ',' << g.maximum_capture_work << ','
          << g.maximum_revalidation_work << ',' << g.maximum_proof_additions << ','
          << g.maximum_proof_literal_occurrences << ',' << g.maximum_journal_bytes << ','
          << g.maximum_identity_bytes << ',' << g.maximum_external_extent << ','
          << g.maximum_session_keys << ',' << g.maximum_session_work << ','
          << g.maximum_session_identity_bytes << ',' << p->maximum_row_tuples << ','
          << p->maximum_row_width << ',' << p->maximum_duplicate_scan_work << ','
          << p->maximum_arena_words << ')';
    }
    else if (const auto *p = std::get_if<TseitinCycleBreakingParameters>(
                 &step.parameters))
      out << ":tseitin-cycle-breaking1(" << p->maximum_snapshot_clauses << ','
          << p->maximum_snapshot_literals << ',' << p->maximum_snapshot_work << ','
          << p->maximum_snapshot_variables << ',' << p->maximum_discovery_work << ','
          << p->maximum_discovery_clauses << ',' << p->maximum_discovery_groups << ','
          << p->maximum_discovery_retained_bytes << ',' << p->maximum_admission_work << ','
          << p->maximum_planning_work << ',' << p->maximum_witness_length << ','
          << p->maximum_planning_additions << ',' << p->maximum_transaction_work << ','
          << p->maximum_transaction_additions << ',' << p->maximum_transaction_deletions << ','
          << p->maximum_transaction_helpers << ',' << p->maximum_transaction_witness_literals << ','
          << p->maximum_validation_ticks << ',' << p->maximum_components << ','
          << p->maximum_identity_bytes << ',' << p->maximum_total_identity_bytes << ','
          << p->maximum_identity_work << ',' << p->maximum_journal_bytes << ','
          << p->maximum_session_keys << ','
          << p->maximum_session_work << ',' << p->maximum_session_identity_bytes << ')';
    else if (const auto *p =
                 std::get_if<SemanticVariableAdviceParameters>(&step.parameters))
      out << ":semantic-variable-advice1(" << p->maximum_variables << ','
          << p->maximum_priority_sum << ',' << p->existing_theory_priority
          << ',' << p->introduced_theory_priority << ')';
    else if (const auto *p =
                 std::get_if<CongruenceParameters>(&step.parameters))
      out << ":congruence(" << p->and_arity_limit << ','
          << p->gate_count_limit << ',' << p->xor_arity_limit << ')';
    else if (const auto *p =
                 std::get_if<InstantiateParameters>(&step.parameters))
      out << ":instantiate(" << p->minimum_clause_size << ','
          << p->occurrence_limit << ',' << p->minimum_unassigned << ','
          << p->include_redundant << ',' << p->new_material_threshold << ')';
    else if (const auto *p =
                 std::get_if<BackboneParameters>(&step.parameters))
      out << ":backbone(" << p->initial_rounds << ','
          << p->maximum_rounds << ')';
    else if (const auto *p = std::get_if<SweepParameters>(&step.parameters))
      out << ":sweep(" << p->max_depth << ',' << p->max_depth_ceiling << ','
          << p->max_env_vars << ',' << p->max_env_vars_ceiling << ','
          << p->max_env_clauses << ',' << p->max_env_clauses_ceiling << ','
          << p->max_candidates << ',' << p->env_ticks << ','
          << p->max_proof_steps << ',' << p->max_seeds << ','
          << p->round_ticks << ',' << p->strip_false << ','
          << p->measure_closure << ',' << p->full_partition << ','
          << p->skip_embedded_fixed << ',' << p->flip_rounds << ','
          << p->resume << ',' << p->force_general_proof << ')';
    else if (const auto *p =
                 std::get_if<VivifyParameters>(&step.parameters))
    {
      out << ":vivify(" << unsigned(p->scope) << ',' << p->reuse << ','
          << p->flush << ',' << p->irredundant_sort_ratio;
      // The band ledger is appended only when set, so band-free programs keep
      // their established serialization.
      if (p->band || p->band_budget || p->band_offset)
        out << ',' << p->band << ',' << p->band_budget << ',' << p->band_offset;
      out << ')';
    }
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
    else if (const auto *p = std::get_if<
                 BalancedSignatureModelParameters>(&step.parameters))
      out << ":balanced-signature-model(" << p->maximum_balls << ','
          << p->maximum_width << ',' << p->restarts << ','
          << p->steps_per_restart << ',' << p->samples_per_step << ','
          << p->seed << ',' << p->completion_rounds << ')';
    else if (const auto *p = std::get_if<FermatModelParameters>(
                 &step.parameters))
      out << ":fermat-model(" << p->maximum_bits << ','
          << p->comparator_tail_clauses << ',' << p->rho_constants << ','
          << p->rho_iterations << ')';
    else if (const auto *p = std::get_if<LookupCircuitModelParameters>(
                 &step.parameters))
      out << ":lookup-circuit-model(" << p->maximum_fanin << ','
          << p->maximum_fanout << ',' << p->maximum_primary_inputs << ','
          << p->maximum_support << ','
          << p->maximum_vector_gate_evaluations << ')';
    else if (const auto *p =
                 std::get_if<SimonModelParameters>(&step.parameters))
      out << ":simon-model(" << p->maximum_rounds << ','
          << p->maximum_candidates << ')';
    else if (const auto *p =
                 std::get_if<StconnProofParameters>(&step.parameters))
      out << ":stconn-proof(" << p->maximum_width << ','
          << p->maximum_columns << ',' << p->maximum_rup_clauses << ')';
    else if (const auto *p = std::get_if<SignedCardinalityProofParameters>(
                 &step.parameters))
      out << ":signed-cardinality-proof(" << p->maximum_variables << ','
          << p->maximum_groups << ',' << p->maximum_constraints << ','
          << p->maximum_group_arity << ',' << p->maximum_bdd_nodes << ','
          << p->maximum_proof_additions << ')';
    else if (const auto *p =
                 std::get_if<ExactCoverModelParameters>(&step.parameters))
      out << ":exact-cover-model(" << p->block_arity << ',' << p->maximum_nodes
          << ',' << p->restarts << ',' << p->restart_nodes << ',' << p->seed
          << ')';
    else if (const auto *p =
                 std::get_if<IndependentSetModelParameters>(&step.parameters))
      out << ":independent-set-model(" << p->maximum_vertices << ','
          << p->maximum_steps << ',' << p->completion_rounds << ',' << p->seed
          << ')';
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

bool deserialize_online_strategy_program(const std::string &encoded,
                                         OnlineStrategyProgram &out,
                                         std::string *error,
                                         bool direct_csp_producer_template,
                                         bool allow_native_offer) {
  const auto reject = [error](const char *message) {
    if (error)
      *error = message;
    return false;
  };
  std::istringstream input(encoded);
  std::string token;
  if (!(input >> token) || !token.starts_with("sequence-repeat="))
    return reject("online replay header is malformed");
  std::string_view header(token);
  header.remove_prefix(sizeof("sequence-repeat=") - 1);
  const size_t header_colon = header.find(':');
  OnlineStrategyProgram parsed;
  if (header_colon == std::string_view::npos ||
      !parse_repeat(header.substr(0, header_colon), parsed.repeat.mode) ||
      !parse_u64(header.substr(header_colon + 1), parsed.repeat.limit))
    return reject("online replay sequence repeat is malformed");

  while (input >> token) {
    std::string_view text(token);
    if (!text.starts_with("step="))
      return reject("online replay step prefix is malformed");
    text.remove_prefix(sizeof("step=") - 1);
    const auto field = [&text](std::string_view &value) {
      const size_t colon = text.find(':');
      if (colon == std::string_view::npos)
        return false;
      value = text.substr(0, colon);
      text.remove_prefix(colon + 1);
      return true;
    };
    OnlineInvocation step;
    std::string_view value;
    uint64_t number = 0;
    if (!field(value) || !parse_u64(value, step.id) ||
        !field(value))
      return reject("online replay step identity is malformed");
    step.rule.assign(value);
    if (!field(value) || !parse_u64(value, number) || number > UINT8_MAX)
      return reject("online replay granularity is malformed");
    step.granularity = static_cast<InvocationGranularity>(number);
    if (!field(value) || !parse_repeat(value, step.repeat.mode) ||
        !field(value) || !parse_u64(value, step.repeat.limit) ||
        !field(value) || !parse_u64(value, number) || number > UINT8_MAX)
      return reject("online replay repeat or work unit is malformed");
    step.work_unit = static_cast<WorkUnit>(number);
    if (!field(value))
      return reject("online replay budget is missing");
    if (value == "unlimited")
      step.budget = WorkLimit::unlimited();
    else if (parse_u64(value, number))
      step.budget = WorkLimit::bounded(number);
    else
      return reject("online replay budget is malformed");

    if (text.starts_with("post(")) {
      if (!field(value))
        return reject("online replay post-application action is malformed");
      std::vector<uint64_t> fields;
      if (!tuple(value, "post", fields) || fields.size() != 1 ||
          fields[0] > UINT8_MAX)
        return reject("online replay post-application action is malformed");
      step.post_application =
          static_cast<PostApplication>(fields[0]);
    }

    if (text.starts_with("rulework(")) {
      if (!field(value))
        return reject("online replay rule work budget is malformed");
      std::vector<uint64_t> fields;
      if (!tuple(value, "rulework", fields) || fields.size() != 2 ||
          fields[0] > UINT8_MAX)
        return reject("online replay rule work budget is malformed");
      step.rule_work_budget.mode =
          static_cast<RuleWorkBudgetMode>(fields[0]);
      step.rule_work_budget.amount = fields[1];
    }

    if (text.starts_with("material(")) {
      if (!field(value))
        return reject("online replay rule material settings are malformed");
      std::vector<uint64_t> fields;
      if (!tuple(value, "material", fields) || fields.size() != 2 ||
          fields[0] > UINT8_MAX || fields[1] > UINT8_MAX)
        return reject("online replay rule material settings are malformed");
      step.material.touch_filter =
          static_cast<SettingOverride>(fields[0]);
      step.material.likely_kept_redundant =
          static_cast<SettingOverride>(fields[1]);
    }

    constexpr std::string_view instances_marker = ":instances[";
    const size_t instances_at = text.find(instances_marker);
    if (instances_at == std::string_view::npos ||
        !parse_parameters(text.substr(0, instances_at), step.parameters))
      return reject("online replay parameters are malformed");
    text.remove_prefix(instances_at + instances_marker.size());
    const size_t instances_end = text.find(']');
    if (instances_end == std::string_view::npos ||
        !comma_values(text.substr(0, instances_end), step.instances))
      return reject("online replay instances are malformed");
    text.remove_prefix(instances_end + 1);

    constexpr std::string_view grids_marker = ":grids[";
    if (!text.starts_with(grids_marker))
      return reject("online replay Factor grids are malformed");
    text.remove_prefix(grids_marker.size());
    const size_t grids_end = text.find(']');
    if (grids_end == std::string_view::npos)
      return reject("online replay Factor grids are malformed");
    std::string_view grids = text.substr(0, grids_end);
    text.remove_prefix(grids_end + 1);
    while (!grids.empty()) {
      const size_t semicolon = grids.find(';');
      FactorGrid grid;
      if (!parse_grid(grids.substr(0, semicolon), grid))
        return reject("online replay Factor grid payload is malformed");
      step.factor_grids.push_back(std::move(grid));
      if (semicolon == std::string_view::npos)
        grids = {};
      else
        grids.remove_prefix(semicolon + 1);
    }
    constexpr std::string_view sr_marker = ":sr[";
    if (text.starts_with(sr_marker)) {
      const size_t end = text.find(']');
      if (end == std::string_view::npos)
        return reject("online replay SR payload is malformed");
      text.remove_prefix(sr_marker.size());
      std::string_view members = text.substr(0, end - sr_marker.size());
      text.remove_prefix(end - sr_marker.size() + 1);
      while (!members.empty()) {
        const size_t semicolon = members.find(';');
        SubstitutionRedundancyInstance instance;
        if (!parse_sr_instance(members.substr(0, semicolon), instance))
          return reject("online replay SR instance is malformed");
        step.substitution_redundancy_instances.push_back(std::move(instance));
        if (semicolon == std::string_view::npos)
          members = {};
        else
          members.remove_prefix(semicolon + 1);
      }
    }
    constexpr std::string_view condition_marker = ":condition[";
    if (text.starts_with(condition_marker) && text.back() == ']') {
      text.remove_prefix(condition_marker.size());
      text.remove_suffix(1);
      while (!text.empty()) {
        const size_t semicolon = text.find(';');
        ConditionInstance instance;
        if (!parse_condition_instance(text.substr(0, semicolon), instance))
          return reject("online replay Condition instance is malformed");
        step.condition_instances.push_back(std::move(instance));
        if (semicolon == std::string_view::npos)
          text = {};
        else
          text.remove_prefix(semicolon + 1);
      }
    }
    constexpr std::string_view independent_set_cover_marker =
        ":independent-set-cover[";
    if (text.starts_with(independent_set_cover_marker) && text.back() == ']') {
      text.remove_prefix(independent_set_cover_marker.size());
      text.remove_suffix(1);
      while (!text.empty()) {
        const size_t semicolon = text.find(';');
        std::vector<uint32_t> cover;
        if (!parse_independent_set_cover(text.substr(0, semicolon), cover))
          return reject("online replay IndependentSetCover payload is malformed");
        step.independent_set_cover_instances.push_back(std::move(cover));
        if (semicolon == std::string_view::npos)
          text = {};
        else
          text.remove_prefix(semicolon + 1);
      }
    }
    constexpr std::string_view gf3_affine_marker =
        ":gf3-affine-consequence[";
    if (text.starts_with(gf3_affine_marker) && text.back() == ']') {
      text.remove_prefix(gf3_affine_marker.size());
      text.remove_suffix(1);
      while (!text.empty()) {
        const size_t semicolon = text.find(';');
        Gf3AffineInstance instance;
        if (!parse_gf3_affine_instance(text.substr(0, semicolon), instance))
          return reject("online replay GF3 affine payload is malformed");
        step.gf3_affine_instances.push_back(instance);
        if (semicolon == std::string_view::npos) text = {};
        else text.remove_prefix(semicolon + 1);
      }
    }
    constexpr std::string_view affine_basis_marker = ":affine-basis-row1[";
    if (text.starts_with(affine_basis_marker) && text.back() == ']') {
      const auto *parameters =
          std::get_if<AffineConsequenceParameters>(&step.parameters);
      if (!parameters)
        return reject("online replay GF2 affine row has no typed limits");
      text.remove_prefix(affine_basis_marker.size());
      text.remove_suffix(1);
      if (text.empty())
        return reject("online replay GF2 affine row collection is empty");
      while (!text.empty()) {
        if (step.affine_basis_instances.size() >=
            parameters->maximum_session_keys)
          return reject(
              "online replay GF2 affine row collection exceeds limits");
        const size_t semicolon = text.find(';');
        AffineBasisRowSelection selection;
        if (!parse_affine_basis_row(text.substr(0, semicolon), *parameters,
                                    selection))
          return reject("online replay GF2 affine row is malformed");
        step.affine_basis_instances.push_back(std::move(selection));
        if (semicolon == std::string_view::npos) text = {};
        else text.remove_prefix(semicolon + 1);
      }
    }
    constexpr std::string_view guarded_cover_marker = ":direct-csp-guarded-cover2[";
    if (text.starts_with(guarded_cover_marker) && text.back() == ']') {
      text.remove_prefix(guarded_cover_marker.size()); text.remove_suffix(1);
      while (!text.empty()) {
        const auto semicolon = text.find(';');
        DirectCspGuardedCoverActionSelection instance;
        if (!parse_guarded_cover(text.substr(0,semicolon), instance))
          return reject("online replay GuardedCoverV2 payload is malformed");
        step.direct_csp_guarded_cover_instances.push_back(std::move(instance));
        if (semicolon == std::string_view::npos) text = {};
        else { text.remove_prefix(semicolon+1); if (text.empty()) return reject("empty GuardedCoverV2 selection"); }
      }
    }
    constexpr std::string_view direct_csp_marker = ":direct-csp-support[";
    if (text.starts_with(direct_csp_marker) && text.back() == ']') {
      text.remove_prefix(direct_csp_marker.size());
      text.remove_suffix(1);
      while (!text.empty()) {
        const size_t semicolon = text.find(';');
        DirectCspSupportActionSelection instance;
        if (!parse_direct_csp_support(text.substr(0, semicolon), instance))
          return reject("online replay DirectCSP payload is malformed");
        step.direct_csp_support_instances.push_back(std::move(instance));
        if (semicolon == std::string_view::npos) text = {};
        else text.remove_prefix(semicolon + 1);
      }
    }
    constexpr std::string_view nary_csp_equality_marker =
        ":nary-csp-equality[";
    if (text.starts_with(nary_csp_equality_marker) && text.back() == ']') {
      text.remove_prefix(nary_csp_equality_marker.size());
      text.remove_suffix(1);
      while (!text.empty()) {
        const size_t semicolon = text.find(';');
        NaryCspEqualityActionSelection instance;
        if (!parse_nary_csp_equality(text.substr(0, semicolon), instance))
          return reject("online replay n-ary CSP equality payload is malformed");
        step.nary_csp_equality_instances.push_back(std::move(instance));
        if (semicolon == std::string_view::npos) text = {};
        else text.remove_prefix(semicolon + 1);
      }
    }
    constexpr std::string_view transitivity_marker=":nary-csp-equality-transitivity[";
    if(text.starts_with(transitivity_marker) && text.back()==']') {
      text.remove_prefix(transitivity_marker.size()); text.remove_suffix(1);
      while(!text.empty()) {
        const auto semicolon=text.find(';'); NaryCspEqualityTransitivitySelection v;
        if(!parse_nary_csp_equality_transitivity(text.substr(0,semicolon),v)) return reject("malformed transitivity replay selection");
        step.nary_csp_equality_transitivity_instances.push_back(std::move(v));
        if(semicolon==std::string_view::npos) text={}; else text.remove_prefix(semicolon+1);
      }
    }
    constexpr std::string_view nary_csp_equality_amo_marker =
        ":nary-csp-equality-amo[";
    if (text.starts_with(nary_csp_equality_amo_marker) && text.back() == ']') {
      text.remove_prefix(nary_csp_equality_amo_marker.size());
      text.remove_suffix(1);
      while (!text.empty()) {
        const size_t semicolon = text.find(';');
        NaryCspEqualityAmoSelection instance;
        if (!parse_nary_csp_equality_amo(text.substr(0, semicolon), instance))
          return reject(
              "online replay n-ary CSP equality AMO payload is malformed");
        step.nary_csp_equality_amo_instances.push_back(std::move(instance));
        if (semicolon == std::string_view::npos) text = {};
        else text.remove_prefix(semicolon + 1);
      }
    }
    constexpr std::string_view gf3_gaussian_marker =
        ":gf3-gaussian-consequence[";
    if (text.starts_with(gf3_gaussian_marker) && text.back() == ']') {
      text.remove_prefix(gf3_gaussian_marker.size());
      text.remove_suffix(1);
      while (!text.empty()) {
        const size_t semicolon = text.find(';');
        step.gf3_gaussian_instances.emplace_back(text.substr(0, semicolon));
        if (semicolon == std::string_view::npos) text = {};
        else text.remove_prefix(semicolon + 1);
      }
    }
    constexpr std::string_view gf3_general_row_marker =
        ":gf3-general-row[";
    if (text.starts_with(gf3_general_row_marker) && text.back() == ']') {
      text.remove_prefix(gf3_general_row_marker.size());
      text.remove_suffix(1);
      while (!text.empty()) {
        const size_t semicolon = text.find(';');
        const std::string_view item = text.substr(0, semicolon);
        const size_t first = item.find(',');
        const size_t second = first == std::string_view::npos
                                  ? first
                                  : item.find(',', first + 1);
        uint64_t outcome = 0, tuple_index = 0;
        if (first == std::string_view::npos ||
            second == std::string_view::npos ||
            !parse_u64(item.substr(0, first), outcome) ||
            outcome > UINT32_MAX ||
            !parse_u64(item.substr(first + 1, second - first - 1),
                       tuple_index))
          return reject("online replay GF3 general-row payload is malformed");
        step.gf3_general_row_instances.push_back(
            {static_cast<uint32_t>(outcome), tuple_index,
             std::string(item.substr(second + 1))});
        if (semicolon == std::string_view::npos) text = {};
        else text.remove_prefix(semicolon + 1);
      }
    }
    constexpr std::string_view cardinality_search_marker =
        ":cardinality-search[";
    if (text.starts_with(cardinality_search_marker) && text.back() == ']') {
      text.remove_prefix(cardinality_search_marker.size());
      text.remove_suffix(1);
      std::vector<std::string_view> fields;
      while (!text.empty()) {
        const size_t comma = text.find(',');
        fields.push_back(text.substr(0, comma));
        if (comma == std::string_view::npos) text = {};
        else text.remove_prefix(comma + 1);
      }
      if (fields.empty())
        return reject("online replay cardinality bounds are malformed");
      size_t at = 0;
      uint64_t count = 0;
      const auto *limits =
          std::get_if<CardinalitySearchActionLimits>(&step.parameters);
      if (!limits || !parse_u64(fields[at++], count) || !count ||
          count > limits->inspection.maximum_constraints)
        return reject("online replay cardinality bound count is malformed");
      std::vector<CardinalitySearchBound> bounds;
      bounds.reserve(static_cast<size_t>(count));
      for (uint64_t b = 0; b < count; ++b) {
        uint64_t width = 0, lower = 0, upper = 0;
        if (fields.size() - at < 3 || !parse_u64(fields[at++], width) ||
            !parse_u64(fields[at++], lower) ||
            !parse_u64(fields[at++], upper) ||
            !width || width > limits->inspection.maximum_support_variables ||
            lower > UINT32_MAX ||
            upper > UINT32_MAX || fields.size() - at < width)
          return reject("online replay cardinality bound is malformed");
        CardinalitySearchBound bound;
        bound.lower = static_cast<uint32_t>(lower);
        bound.upper = static_cast<uint32_t>(upper);
        for (uint64_t j = 0; j < width; ++j) {
          const auto value = fields[at++];
          int64_t signed_literal = 0;
          const auto parsed = std::from_chars(
              value.data(), value.data() + value.size(), signed_literal);
          if (parsed.ec != std::errc{} ||
              parsed.ptr != value.data() + value.size() ||
              signed_literal <= INT32_MIN || signed_literal > INT32_MAX ||
              !signed_literal)
            return reject("online replay cardinality literal is malformed");
          bound.literals.push_back(static_cast<int32_t>(signed_literal));
        }
        bounds.push_back(std::move(bound));
      }
      if (at != fields.size())
        return reject("online replay cardinality payload has extra fields");
      step.cardinality_search_instances.push_back(std::move(bounds));
    }
    constexpr std::string_view tseitin_cycle_marker =
        ":tseitin-cycle-breaking[";
    if (text.starts_with(tseitin_cycle_marker) && text.back() == ']') {
      text.remove_prefix(tseitin_cycle_marker.size());
      text.remove_suffix(1);
      while (!text.empty()) {
        const size_t semicolon = text.find(';');
        step.tseitin_cycle_instances.emplace_back(text.substr(0, semicolon));
        if (semicolon == std::string_view::npos) text = {};
        else text.remove_prefix(semicolon + 1);
      }
    }
    if (!text.empty())
      return reject("online replay structured payload is malformed");
    parsed.sequence.push_back(std::move(step));
  }
  if (!input.eof())
    return reject("online replay contains unreadable data");
  // Executable action programs still require exact DirectCSP rows. The one
  // explicit producer-profile caller parses a canonical, payload-free
  // template and binds the row later. Validate a private executable shape so
  // the synthetic row can never enter the returned program or its replay
  // identity.
  OnlineStrategyProgram validated = parsed;
  if (direct_csp_producer_template && !validated.sequence.empty()) {
    auto &step = validated.sequence.front();
    if (step.rule == "DirectCspSupport" &&
        std::holds_alternative<DirectCspSupportParameters>(step.parameters) &&
        step.granularity == InvocationGranularity::CanonicalApplication &&
        step.direct_csp_support_instances.empty()) {
      step.granularity = InvocationGranularity::OneInstance;
      step.direct_csp_support_instances = {{{1, 2}, {3, 4}, 0}};
    }
  }
  if (!validate_online_strategy_program(validated, error,
                                        allow_native_offer))
    return false;
  out = std::move(parsed);
  if (error)
    error->clear();
  return true;
}

} // namespace tactix::strategy
