// C API implementation — v1, frozen at the phase-2 review (plan §5.1);
// post-v1 changes are additive only. Rename the public alias while parsing the
// header so its `tactix` typedef does not collide with the internal namespace;
// the underlying `struct tactix_solver` and C symbols remain identical.
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "frontend/parse.hpp"
#include "api/api_internal.hpp"
#include "policies/heuristics.hpp"
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/strategy_settings.hpp"
#include "diagnostics/rule_journal.hpp"
#include "proof/proof.hpp"
#include "rules/analyze/analyze.hpp"
#include "rules/decide/decide.hpp"
#include "rules/condition/condition.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"
#include "rules/semantic_extension/affine_basis_action.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"
#include "rules/semantic_extension/exact_cover_action.hpp"
#include "rules/semantic_extension/independent_set_cover_action.hpp"
#include "rules/semantic_extension/gf3_affine_action.hpp"
#include "rules/semantic_extension/direct_csp_support_action.hpp"
#include "rules/semantic_extension/direct_csp_guarded_cover_action.hpp"
#include "rules/semantic_extension/nary_csp_relation_owner.hpp"
#include "rules/semantic_extension/nary_csp_equality_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_transitivity_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_amo_action.hpp"
#include "rules/direct_php/direct_php_action.hpp"
#include "rules/semantic_extension/gf3_gaussian_action.hpp"
#include "rules/semantic_extension/gf3_general_row_action.hpp"
#include "rules/lucky/tseitin_cycle_action.hpp"
#include "strategy/execution_scope.hpp"
#include "strategy/guidance_execution.hpp"
#include "strategy/heuristic_execution.hpp"
#include "strategy/online_inprocess.hpp"
#include "strategy/parameter_execution.hpp"
#include "strategy/phase_execution.hpp"
#include "strategy/publication_execution.hpp"
#include "strategy/reduction_execution.hpp"
#include "strategy/representation_execution.hpp"
#include "strategy/root_execution.hpp"
#include "strategy/slot_execution.hpp"
#include "strategy/work_execution.hpp"
#include "policies/rule_publication.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace tactix;

struct tactix_solver {
  Kernel kernel;
  // Named settings of the default strategy (tactix_set_default_option).
  StrategySettings default_options;
  Heuristics heuristics; // the session owns H, Kernel holds a non-owning
                         // pointer. Outlives every solve(), so tactix_decide
                         // before any solve still has an H.
  Proof proof;
  AffineBasisActionSession affine_basis_session;
  AffineMatrixSearchSession affine_matrix_search_session;
  ExactCoverExtensionActionSession exact_cover_extension_session;
  IndependentSetCoverActionSession independent_set_cover_session;
  Gf3AffineActionSession gf3_affine_action_session;
  DirectCspSupportActionSession direct_csp_support_action_session;
  DirectCspGuardedCoverActionSession direct_csp_guarded_cover_action_session;
  NaryCspRelationOwnerSession nary_csp_relation_owner_session;
  NaryCspEqualitySession nary_csp_equality_session;
  NaryCspEqualityTransitivitySession nary_csp_equality_transitivity_session;
  NaryCspEqualityAmoSession nary_csp_equality_amo_session;
  DirectPhpActionSession direct_php_action_session;
  Gf3GaussianActionSession gf3_gaussian_action_session;
  Gf3GeneralRowActionSession gf3_general_row_action_session;
  TseitinCycleBreakingSession tseitin_cycle_breaking_session;
  std::vector<Lit> scratch;
  bool input_completed = false;
  bool pending_input_proof = false;
  uint64_t model_strategy_epoch = UINT64_MAX;
  int guidance_publication = TACTIX_GUIDANCE_DISCARD;
  // Additive and lazy: ordinary tactix_solve sessions retain the pre-online
  // object layout's allocation behavior and never construct program vectors.
  std::unique_ptr<strategy::OnlineInprocessExecutor> online;
  // Execution contexts entered through tactix_support, innermost last.
  std::vector<std::pair<execution::ExecutionContext,
                        execution::ExecutionContextState>> contexts;
  // The inprocessing phase bracket entered through tactix_support: its
  // default Factor selection and occurrence index outlive the bracket, which
  // is declared last so that it closes before them. `phase_contexts` is the
  // context depth at entry; contexts nest strictly inside the bracket.
  DefaultFactorMatch phase_factor_selection;
  OccIndex phase_occurrences;
  size_t phase_contexts = 0;
  execution::InprocessPhase phase;
  tactix_solver() : direct_php_action_session(kernel) {
    kernel.h = &heuristics;
    kernel.saved_phases = &heuristics.saved;
    kernel.affine_basis_action_session = &affine_basis_session;
    kernel.affine_matrix_search_session = &affine_matrix_search_session;
    kernel.exact_cover_extension_action_session =
        &exact_cover_extension_session;
    kernel.independent_set_cover_action_session =
        &independent_set_cover_session;
    kernel.gf3_affine_action_session = &gf3_affine_action_session;
    kernel.direct_csp_support_action_session =
        &direct_csp_support_action_session;
    kernel.direct_csp_guarded_cover_action_session =
        &direct_csp_guarded_cover_action_session;
    kernel.nary_csp_relation_owner_session = &nary_csp_relation_owner_session;
    kernel.nary_csp_equality_session = &nary_csp_equality_session;
    kernel.nary_csp_equality_transitivity_session = &nary_csp_equality_transitivity_session;
    kernel.nary_csp_equality_amo_session = &nary_csp_equality_amo_session;
    kernel.direct_php_action_session = &direct_php_action_session;
    kernel.gf3_gaussian_action_session = &gf3_gaussian_action_session;
    kernel.gf3_general_row_action_session = &gf3_general_row_action_session;
    kernel.tseitin_cycle_breaking_session = &tseitin_cycle_breaking_session;
    // Match the CLI/reference default; rule-level fixtures using a bare Kernel
    // can still exercise the non-reuse path explicitly.
    kernel.conflict_reuse = true;
  }
};

extern "C" {
static void input_complete(tactix_solver *solver);
}

// Private fixed-test introspection keeps fault injection independent of the
// opaque C handle layout. Production actions still use the public C API.
namespace tactix::api_detail {
Kernel &kernel_for_fixed_testing(void *opaque) {
  assert(opaque);
  return static_cast<::tactix_solver *>(opaque)->kernel;
}
const strategy::OnlineStrategyProgram *online_program_for_fixed_testing(void *opaque) {
  assert(opaque);
  const auto &online = static_cast<::tactix_solver *>(opaque)->online;
  return online ? &online->program() : nullptr;
}
}

namespace {

bool has_sat_assignment(const Kernel &kernel) {
  return !kernel.inconsistent && kernel.unassigned_active == 0 &&
         !kernel.conflict_valid && !kernel.pending.valid &&
         kernel_propagation_fixpoint(kernel);
}

void prepare_external_model(tactix_solver *solver) {
  Kernel &kernel = solver->kernel;
  if (kernel.model_extended &&
      solver->model_strategy_epoch == kernel.strategy_epoch)
    return;
  // Model access is a cache, not a change to the resumable search state.
  // Any public rule/configuration transition changes strategy_epoch; an old
  // reconstructed assignment must not survive a restart and new decisions.
  kernel.model_extended = false;
  kernel_extend_model(kernel, /*materialize_internal=*/false);
  solver->model_strategy_epoch = kernel.strategy_epoch;
}

void external_state_change(tactix_solver *solver) {
  ++solver->kernel.strategy_epoch;
}

// Every public registry dispatch publishes its rule events through the
// session's selected profile, before invariants are checked or the result is
// observed. NATIVE is DefaultPolicy's executor profile with the same publisher.
template <class Apply>
ApplyResult apply_published(tactix_solver *solver, Apply &&apply) {
  if (solver->guidance_publication == TACTIX_GUIDANCE_NATIVE)
    return execution::execute_action(solver->kernel,
                                     execution::NATIVE_RULE_PUBLICATION,
                                     std::forward<Apply>(apply),
                                     HeuristicRulePublisher{})
        .result;
  return execution::execute_action(
             solver->kernel,
             execution::RuleApplicationRequest{
                 execution::RootPublication::Deferred, false, true},
             std::forward<Apply>(apply), DiscardedRulePublisher{})
      .result;
}

bool online_repeat(int mode, uint64_t limit, strategy::RepeatSpec &out) {
  switch (mode) {
  case TACTIX_ONLINE_REPEAT_ONCE:
    out = {strategy::RepeatMode::Once, limit};
    return true;
  case TACTIX_ONLINE_REPEAT_COUNT:
    out = {strategy::RepeatMode::Count, limit};
    return true;
  case TACTIX_ONLINE_REPEAT_FIXPOINT:
    out = {strategy::RepeatMode::UntilScopedFixpoint, limit};
    return true;
  case TACTIX_ONLINE_REPEAT_SIZE_SCALED_FIXPOINT:
    out = {strategy::RepeatMode::UntilSizeScaledFixpoint, limit};
    return true;
  default:
    return false;
  }
}

bool online_post_application(int value, strategy::PostApplication &out) {
  switch (value) {
  case TACTIX_ONLINE_POST_NONE:
    out = strategy::PostApplication::None;
    return true;
  case TACTIX_ONLINE_POST_PROPAGATE:
    out = strategy::PostApplication::Propagate;
    return true;
  case TACTIX_ONLINE_POST_FORWARD_CLOSURE:
    out = strategy::PostApplication::ForwardClosure;
    return true;
  default:
    return false;
  }
}

bool online_granularity(int value, strategy::InvocationGranularity &out) {
  switch (value) {
  case TACTIX_ONLINE_GRANULARITY_ONE:
    out = strategy::InvocationGranularity::OneInstance;
    return true;
  case TACTIX_ONLINE_GRANULARITY_BATCH:
    out = strategy::InvocationGranularity::SelectedBatch;
    return true;
  case TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS:
    out = strategy::InvocationGranularity::CompleteCurrentPass;
    return true;
  case TACTIX_ONLINE_GRANULARITY_CANONICAL:
    out = strategy::InvocationGranularity::CanonicalApplication;
    return true;
  default:
    return false;
  }
}

bool online_work_unit(int value, strategy::WorkUnit &out) {
  switch (value) {
  case TACTIX_ONLINE_WORK_APPLICATIONS:
    out = strategy::WorkUnit::Applications;
    return true;
  case TACTIX_ONLINE_WORK_BVE_TICKS:
    out = strategy::WorkUnit::BveTicks;
    return true;
  case TACTIX_ONLINE_WORK_PROPAGATIONS:
    out = strategy::WorkUnit::Propagations;
    return true;
  default:
    return false;
  }
}

bool online_rule_work_budget(int mode, uint64_t amount,
                             strategy::RuleWorkBudget &out) {
  switch (mode) {
  case TACTIX_ONLINE_RULE_BUDGET_INHERIT:
    if (amount)
      return false;
    out = {strategy::RuleWorkBudgetMode::Inherit, 0};
    return true;
  case TACTIX_ONLINE_RULE_BUDGET_UNLIMITED:
    if (amount)
      return false;
    out = {strategy::RuleWorkBudgetMode::Unlimited, 0};
    return true;
  case TACTIX_ONLINE_RULE_BUDGET_BOUNDED:
    out = {strategy::RuleWorkBudgetMode::Bounded, amount};
    return true;
  default:
    return false;
  }
}

bool online_setting_override(int mode, strategy::SettingOverride &out) {
  switch (mode) {
  case TACTIX_ONLINE_SETTING_INHERIT:
    out = strategy::SettingOverride::Inherit;
    return true;
  case TACTIX_ONLINE_SETTING_DISABLE:
    out = strategy::SettingOverride::Disable;
    return true;
  case TACTIX_ONLINE_SETTING_ENABLE:
    out = strategy::SettingOverride::Enable;
    return true;
  default:
    return false;
  }
}

bool online_graph_symmetry_parameters(const tactix_online_sr_parameters &p,
                                      GraphSymmetryParameters &graph) {
  if (p.symmetry_fixing_mode < TACTIX_ONLINE_SYMMETRY_STABILIZER_ONLY ||
      p.symmetry_fixing_mode >
          TACTIX_ONLINE_SYMMETRY_STABILIZER_THEN_GENERATORS ||
      p.symmetry_generator_order < TACTIX_ONLINE_SYMMETRY_ORDER_CANONICAL ||
      p.symmetry_generator_order > TACTIX_ONLINE_SYMMETRY_ORDER_SEEDED ||
      p.symmetry_clause_order < TACTIX_ONLINE_SYMMETRY_ORDER_CANONICAL ||
      p.symmetry_clause_order > TACTIX_ONLINE_SYMMETRY_ORDER_SEEDED ||
      p.symmetry_literal_order < TACTIX_ONLINE_SYMMETRY_ORDER_CANONICAL ||
      p.symmetry_literal_order > TACTIX_ONLINE_SYMMETRY_ORDER_SEEDED ||
      p.symmetry_clause_scope <
          TACTIX_ONLINE_SYMMETRY_CLAUSES_UNIQUE_LITERALS ||
      p.symmetry_clause_scope > TACTIX_ONLINE_SYMMETRY_CLAUSES_ALL ||
      p.symmetry_pair_orientation < TACTIX_ONLINE_SYMMETRY_PAIR_CANONICAL ||
      p.symmetry_pair_orientation > TACTIX_ONLINE_SYMMETRY_PAIR_SEEDED ||
      p.symmetry_binary_graph_mode <
          TACTIX_ONLINE_SYMMETRY_BINARY_GRAPH_INHERIT ||
      p.symmetry_binary_graph_mode >
          TACTIX_ONLINE_SYMMETRY_BINARY_GRAPH_AUTOMATIC ||
      p.symmetry_clause_selection <
          TACTIX_ONLINE_SYMMETRY_CLAUSE_SELECTION_INHERIT ||
      p.symmetry_clause_selection >
          TACTIX_ONLINE_SYMMETRY_CLAUSE_SELECTION_AMO_THEN_SHORTEST ||
      p.symmetry_chain_mode < TACTIX_ONLINE_SYMMETRY_CHAIN_INHERIT ||
      p.symmetry_chain_mode > TACTIX_ONLINE_SYMMETRY_CHAIN_FIX_AND_CUT_RESIFT ||
      p.symmetry_nonforcing_cut_admission <
          TACTIX_ONLINE_SYMMETRY_CUT_ADMISSION_INHERIT ||
      p.symmetry_nonforcing_cut_admission >
          TACTIX_ONLINE_SYMMETRY_CUT_ADMISSION_REQUIRE_COMPOUND_PROGRESS ||
      p.symmetry_cut_point < TACTIX_ONLINE_SYMMETRY_CUT_POINT_INHERIT ||
      p.symmetry_cut_point >
          TACTIX_ONLINE_SYMMETRY_CUT_POINT_LARGEST_ORBIT_LAST ||
      p.symmetry_negation_fixing_first_mode < TACTIX_ONLINE_SETTING_INHERIT ||
      p.symmetry_negation_fixing_first_mode > TACTIX_ONLINE_SETTING_ENABLE ||
      p.symmetry_first_literal_fix_anchor_mode < TACTIX_ONLINE_SETTING_INHERIT ||
      p.symmetry_first_literal_fix_anchor_mode > TACTIX_ONLINE_SETTING_ENABLE ||
      p.symmetry_base_selection < TACTIX_ONLINE_SYMMETRY_BASE_INHERIT ||
      p.symmetry_base_selection >
          TACTIX_ONLINE_SYMMETRY_BASE_FIRST_SHORTEST_THEN_FORWARD_REMAINDER ||
      p.symmetry_direct_generator_involutions_mode <
          TACTIX_ONLINE_SETTING_INHERIT ||
      p.symmetry_direct_generator_involutions_mode >
          TACTIX_ONLINE_SETTING_ENABLE ||
      p.symmetry_decompose_mode < TACTIX_ONLINE_SETTING_INHERIT ||
      p.symmetry_decompose_mode > TACTIX_ONLINE_SETTING_ENABLE ||
      p.symmetry_prefer_depth_first_mode < TACTIX_ONLINE_SETTING_INHERIT ||
      p.symmetry_prefer_depth_first_mode > TACTIX_ONLINE_SETTING_ENABLE ||
      p.symmetry_strong_certification_mode < TACTIX_ONLINE_SETTING_INHERIT ||
      p.symmetry_strong_certification_mode > TACTIX_ONLINE_SETTING_ENABLE ||
      p.symmetry_pointwise_root_stabilizer_mode < TACTIX_ONLINE_SETTING_INHERIT ||
      p.symmetry_pointwise_root_stabilizer_mode > TACTIX_ONLINE_SETTING_ENABLE ||
      p.symmetry_factor_support_components_mode < TACTIX_ONLINE_SETTING_INHERIT ||
      p.symmetry_factor_support_components_mode > TACTIX_ONLINE_SETTING_ENABLE ||
      p.symmetry_preserve_detector_order_mode < TACTIX_ONLINE_SETTING_INHERIT ||
      p.symmetry_preserve_detector_order_mode > TACTIX_ONLINE_SETTING_ENABLE ||
      p.symmetry_use_structured_generators_mode < TACTIX_ONLINE_SETTING_INHERIT ||
      p.symmetry_use_structured_generators_mode > TACTIX_ONLINE_SETTING_ENABLE ||
      p.symmetry_stabilize_structured_variables_mode < TACTIX_ONLINE_SETTING_INHERIT ||
      p.symmetry_stabilize_structured_variables_mode > TACTIX_ONLINE_SETTING_ENABLE ||
      p.symmetry_structured_scope < TACTIX_ONLINE_SYMMETRY_STRUCTURED_SCOPE_INHERIT ||
      p.symmetry_structured_scope > TACTIX_ONLINE_SYMMETRY_STRUCTURED_SCOPE_AXIS_ANCHORS)
    return false;
  if (p.symmetry_structured_scope != TACTIX_ONLINE_SYMMETRY_STRUCTURED_SCOPE_INHERIT)
    graph.structured_scope = static_cast<SymmetryStructuredScope>(p.symmetry_structured_scope - 1);
  graph.maximum_graph_vertices = p.maximum_graph_vertices;
  graph.maximum_graph_edges = p.maximum_graph_edges;
  graph.maximum_generators = p.maximum_symmetry_generators;
  graph.backend_search_budget = p.symmetry_backend_search_budget;
  graph.backend_support_budget = p.symmetry_backend_support_budget;
  graph.backend_component_size_limit =
      p.symmetry_backend_component_size_limit;
  graph.maximum_fixing_instances = p.maximum_symmetry_fixing_instances;
  graph.maximum_generator_instances_per_pass =
      p.maximum_symmetry_generator_instances_per_pass;
  graph.maximum_group_ticks = p.maximum_symmetry_group_ticks;
  graph.chain_planning_tick_limit = p.symmetry_chain_planning_tick_limit;
  if (p.symmetry_schreier_domain_limit)
    graph.schreier_domain_limit = p.symmetry_schreier_domain_limit;
  graph.seed = p.symmetry_seed;
  if (p.symmetry_error_bound)
    graph.error_bound = p.symmetry_error_bound;
  graph.fixing_mode = static_cast<SymmetryFixingMode>(p.symmetry_fixing_mode);
  graph.generator_order =
      static_cast<SymmetryTraversalOrder>(p.symmetry_generator_order);
  graph.clause_order =
      static_cast<SymmetryTraversalOrder>(p.symmetry_clause_order);
  graph.literal_order =
      static_cast<SymmetryTraversalOrder>(p.symmetry_literal_order);
  graph.clause_scope =
      p.symmetry_clause_scope == TACTIX_ONLINE_SYMMETRY_CLAUSES_ALL
          ? SymmetryClauseScope::All
          : SymmetryClauseScope::UniqueLiteralsOnly;
  graph.pair_orientation =
      static_cast<SymmetryPairOrientation>(p.symmetry_pair_orientation);
  if (p.symmetry_binary_graph_mode !=
      TACTIX_ONLINE_SYMMETRY_BINARY_GRAPH_INHERIT)
    graph.binary_graph_mode = static_cast<SymmetryBinaryGraphMode>(
        p.symmetry_binary_graph_mode - 1);
  if (p.symmetry_binary_graph_density_permille)
    graph.binary_graph_density_permille =
        p.symmetry_binary_graph_density_permille;
  if (p.symmetry_clause_selection !=
      TACTIX_ONLINE_SYMMETRY_CLAUSE_SELECTION_INHERIT)
    graph.clause_selection = static_cast<SymmetryClauseSelection>(
        p.symmetry_clause_selection - 1);
  if (p.symmetry_chain_mode != TACTIX_ONLINE_SYMMETRY_CHAIN_INHERIT)
    graph.chain_mode =
        static_cast<SymmetryChainMode>(p.symmetry_chain_mode - 1);
  if (p.symmetry_nonforcing_cut_admission !=
      TACTIX_ONLINE_SYMMETRY_CUT_ADMISSION_INHERIT)
    graph.nonforcing_cut_admission =
        static_cast<SymmetryNonforcingCutAdmission>(
            p.symmetry_nonforcing_cut_admission - 1);
  if (p.symmetry_base_selection != TACTIX_ONLINE_SYMMETRY_BASE_INHERIT)
    graph.base_selection =
        static_cast<SymmetryBaseSelection>(p.symmetry_base_selection - 1);
  if (p.symmetry_direct_generator_involutions_mode !=
      TACTIX_ONLINE_SETTING_INHERIT)
    graph.direct_generator_involutions_only =
        p.symmetry_direct_generator_involutions_mode ==
        TACTIX_ONLINE_SETTING_ENABLE;
  if (p.symmetry_decompose_mode != TACTIX_ONLINE_SETTING_INHERIT)
    graph.decompose =
        p.symmetry_decompose_mode == TACTIX_ONLINE_SETTING_ENABLE;
  if (p.symmetry_prefer_depth_first_mode != TACTIX_ONLINE_SETTING_INHERIT)
    graph.prefer_depth_first =
        p.symmetry_prefer_depth_first_mode == TACTIX_ONLINE_SETTING_ENABLE;
  if (p.symmetry_strong_certification_mode != TACTIX_ONLINE_SETTING_INHERIT)
    graph.strong_certification =
        p.symmetry_strong_certification_mode == TACTIX_ONLINE_SETTING_ENABLE;
  if (p.symmetry_factor_support_components_mode != TACTIX_ONLINE_SETTING_INHERIT)
    graph.factor_support_components =
        p.symmetry_factor_support_components_mode == TACTIX_ONLINE_SETTING_ENABLE;
  if (p.symmetry_use_structured_generators_mode != TACTIX_ONLINE_SETTING_INHERIT)
    graph.use_structured_generators =
        p.symmetry_use_structured_generators_mode == TACTIX_ONLINE_SETTING_ENABLE;
  if (p.symmetry_stabilize_structured_variables_mode != TACTIX_ONLINE_SETTING_INHERIT)
    graph.stabilize_structured_variables =
        p.symmetry_stabilize_structured_variables_mode == TACTIX_ONLINE_SETTING_ENABLE;
  if (p.symmetry_preserve_detector_order_mode != TACTIX_ONLINE_SETTING_INHERIT)
    graph.preserve_detector_order =
        p.symmetry_preserve_detector_order_mode == TACTIX_ONLINE_SETTING_ENABLE;
  if (p.symmetry_pointwise_root_stabilizer_mode != TACTIX_ONLINE_SETTING_INHERIT)
    graph.pointwise_root_stabilizer =
        p.symmetry_pointwise_root_stabilizer_mode == TACTIX_ONLINE_SETTING_ENABLE;
  if (p.symmetry_cut_point != TACTIX_ONLINE_SYMMETRY_CUT_POINT_INHERIT)
    graph.cut_point = static_cast<SymmetryCutPoint>(p.symmetry_cut_point - 1);
  if (p.symmetry_negation_fixing_first_mode != TACTIX_ONLINE_SETTING_INHERIT)
    graph.negation_fixing_first =
        p.symmetry_negation_fixing_first_mode == TACTIX_ONLINE_SETTING_ENABLE;
  if (p.symmetry_first_literal_fix_anchor_mode != TACTIX_ONLINE_SETTING_INHERIT)
    graph.first_literal_fix_anchor =
        p.symmetry_first_literal_fix_anchor_mode == TACTIX_ONLINE_SETTING_ENABLE;
  return true;
}

// The orbitopal producer's fields of an SR profile, validated and converted.
bool online_orbitopal_parameters(const tactix_online_sr_parameters &p,
                                 SubstitutionRedundancyParameters &sr) {
  if (p.orbitopal_orientation < TACTIX_ONLINE_ORBITOPAL_ROWS ||
      p.orbitopal_orientation > TACTIX_ONLINE_ORBITOPAL_BOTH ||
      p.orbitopal_column_order <
          TACTIX_ONLINE_ORBITOPAL_ORDER_CANONICAL ||
      p.orbitopal_column_order >
          TACTIX_ONLINE_ORBITOPAL_ORDER_COOCCURRENCE_BOUNDED_GREEDY_CLIQUE ||
      p.orbitopal_completion <
          TACTIX_ONLINE_ORBITOPAL_COMPLETION_MATRIX_ONLY ||
      p.orbitopal_completion >
          TACTIX_ONLINE_ORBITOPAL_COMPLETION_INDIVIDUALIZED_GRAPH ||
      p.orbitopal_discovery_order <
          TACTIX_ONLINE_ORBITOPAL_DISCOVERY_REFINEMENT_THEN_UNIQUE_LITERAL ||
      p.orbitopal_discovery_order >
          TACTIX_ONLINE_ORBITOPAL_DISCOVERY_UNIQUE_LITERAL_ONLY ||
      p.orbitopal_row_orbit_source <
          TACTIX_ONLINE_ORBITOPAL_ROW_ORBITS_DETECTED_GENERATORS ||
      p.orbitopal_row_orbit_source >
          TACTIX_ONLINE_ORBITOPAL_ROW_ORBITS_REFINEMENT_CELLS ||
      p.orbitopal_unique_literal_amo_mode <
          TACTIX_ONLINE_SETTING_INHERIT ||
      p.orbitopal_unique_literal_amo_mode > TACTIX_ONLINE_SETTING_ENABLE ||
      p.orbitopal_generator_fallback_mode <
          TACTIX_ONLINE_SETTING_INHERIT ||
      p.orbitopal_generator_fallback_mode > TACTIX_ONLINE_SETTING_ENABLE ||
      p.orbitopal_refinement_mode < TACTIX_ONLINE_SETTING_INHERIT ||
      p.orbitopal_refinement_mode > TACTIX_ONLINE_SETTING_ENABLE ||
      p.orbitopal_progress_feedback_mode <
          TACTIX_ONLINE_SETTING_INHERIT ||
      p.orbitopal_progress_feedback_mode >
          TACTIX_ONLINE_SETTING_ENABLE ||
      p.orbitopal_base_probe_filter_mode <
          TACTIX_ONLINE_SETTING_INHERIT ||
      p.orbitopal_base_probe_filter_mode > TACTIX_ONLINE_SETTING_ENABLE)
    return false;
  sr.orbitopal_orientation =
      static_cast<OrbitopalOrientation>(p.orbitopal_orientation);
  sr.orbitopal_column_order =
      static_cast<OrbitopalColumnOrder>(p.orbitopal_column_order);
  sr.orbitopal_completion =
      static_cast<OrbitopalCompletion>(p.orbitopal_completion);
  sr.orbitopal_discovery_order =
      static_cast<OrbitopalDiscoveryOrder>(p.orbitopal_discovery_order);
  sr.orbitopal_row_orbit_source =
      static_cast<OrbitopalRowOrbitSource>(p.orbitopal_row_orbit_source);
  sr.minimum_orbitopal_rows = p.minimum_orbitopal_rows;
  sr.minimum_orbitopal_columns = p.minimum_orbitopal_columns;
  sr.maximum_orbitopal_matrices = p.maximum_orbitopal_matrices;
  sr.maximum_orbitopal_instances = p.maximum_orbitopal_instances;
  sr.minimum_orbitopal_clique_columns =
      p.minimum_orbitopal_clique_columns
          ? p.minimum_orbitopal_clique_columns
          : sr.minimum_orbitopal_clique_columns;
  sr.maximum_orbitopal_clique_vertices =
      p.maximum_orbitopal_clique_vertices;
  sr.maximum_orbitopal_clique_ticks = p.maximum_orbitopal_clique_ticks;
  sr.maximum_orbitopal_product_states =
      p.maximum_orbitopal_product_states;
  if (p.orbitopal_unique_literal_amo_mode !=
      TACTIX_ONLINE_SETTING_INHERIT)
    sr.complete_unique_literal_amos =
        p.orbitopal_unique_literal_amo_mode ==
        TACTIX_ONLINE_SETTING_ENABLE;
  if (p.orbitopal_generator_fallback_mode !=
      TACTIX_ONLINE_SETTING_INHERIT)
    sr.orbitopal_generator_fallback =
        p.orbitopal_generator_fallback_mode ==
        TACTIX_ONLINE_SETTING_ENABLE;
  if (p.orbitopal_refinement_mode == TACTIX_ONLINE_SETTING_DISABLE)
    sr.maximum_orbitopal_refinement_orbits = 0;
  else if (p.orbitopal_refinement_mode == TACTIX_ONLINE_SETTING_ENABLE)
    sr.maximum_orbitopal_refinement_orbits =
        p.maximum_orbitopal_refinement_orbits;
  if (p.orbitopal_progress_feedback_mode !=
      TACTIX_ONLINE_SETTING_INHERIT)
    sr.orbitopal_progress_feedback =
        p.orbitopal_progress_feedback_mode ==
        TACTIX_ONLINE_SETTING_ENABLE;
  if (p.orbitopal_base_probe_filter_mode !=
      TACTIX_ONLINE_SETTING_INHERIT)
    sr.orbitopal_base_probe_filter =
        p.orbitopal_base_probe_filter_mode ==
        TACTIX_ONLINE_SETTING_ENABLE;
  sr.orbitopal_base_probe_factor =
      p.orbitopal_base_probe_factor
          ? p.orbitopal_base_probe_factor
          : sr.orbitopal_base_probe_factor;
  return true;
}

bool online_parameters(const tactix_online_parameters &source,
                       strategy::InprocessParameters &target) {
  switch (source.kind) {
  case TACTIX_ONLINE_PARAMETERS_INHERIT:
    target = std::monostate{};
    return true;
  case TACTIX_ONLINE_PARAMETERS_BVE: {
    const tactix_online_bve_parameters &p = source.value.bve;
    strategy::BveParameters converted;
    converted.gate_mask = p.gate_mask;
    converted.growth_bound = p.growth_bound;
    converted.occurrence_limit = p.occurrence_limit;
    converted.clause_size_limit = p.clause_size_limit;
    converted.xor_arity_limit = p.xor_arity_limit;
    converted.resolvent_size_limit = p.resolvent_size_limit;
    converted.backward_occurrence_limit = p.backward_occurrence_limit;
    converted.definition_budget = p.definition_budget;
    converted.definition_cores = p.definition_cores;
    converted.definition_core_budget_multiplier =
        p.definition_core_budget_multiplier;
    converted.backward = p.backward != 0;
    converted.on_the_fly_strengthening =
        p.on_the_fly_strengthening != 0;
    converted.candidate_scope =
        static_cast<strategy::BveCandidateScope>(p.candidate_scope);
    converted.resolvent_count_limit =
        p.resolvent_count_limit ? p.resolvent_count_limit : UINT32_MAX;
    converted.traversal =
        static_cast<strategy::BveTraversal>(p.traversal);
    target = converted;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_SUBSUME: {
    const tactix_online_subsume_parameters &p = source.value.subsume;
    target = strategy::SubsumeForwardParameters{
        p.clause_size_limit, p.large_occurrence_limit,
        p.binary_occurrence_limit};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_STRENGTHEN: {
    const tactix_online_strengthen_parameters &p = source.value.strengthen;
    target = strategy::StrengthenParameters{p.clause_size_limit,
                                            p.occurrence_limit};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_PARITY_CONSEQUENCE: {
    const auto &p = source.value.parity_consequence;
    if (p.descending != 0 && p.descending != 1) return false;
    strategy::ParityConsequenceParameters converted{
        p.maximum_clauses, p.maximum_groups, p.selection_work,
        p.intermediate_width, p.source_count, p.output_width,
        p.descending != 0, p.maximum_proof_additions,
        p.maximum_tree_variables, p.consequence};
    if (!converted.valid()) return false;
    target = converted;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_AFFINE_CONSEQUENCE: {
    const auto &p = source.value.affine_consequence;
    strategy::AffineConsequenceParameters converted;
    strategy::copy_affine_parameter_fields(converted, p);
    converted.consequence = p.consequence;
    if (!p.semantics ||
        p.semantics == TACTIX_ONLINE_AFFINE_CONSEQUENCE_SELECTED_V1)
      converted.semantics =
          strategy::AffineConsequenceSemantics::SelectedConsequenceV1;
    else if (p.semantics ==
             TACTIX_ONLINE_AFFINE_CONSEQUENCE_CERTIFIED_BASIS_V2)
      converted.semantics =
          strategy::AffineConsequenceSemantics::CertifiedBasisV2;
    else
      return false;
    if (!converted.valid()) return false;
    target = converted;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_EXACT_COVER_EXTENSION: {
    const auto &p = source.value.exact_cover_extension;
    strategy::ExactCoverExtensionParameters converted{
        p.arity,
        p.maximum_items,
        p.maximum_source_clauses,
        p.maximum_source_literals,
        p.maximum_recognition_work,
        p.maximum_premise_references,
        p.maximum_definitions,
        p.maximum_additions,
        p.maximum_literal_occurrences,
        p.maximum_extent_growth,
        p.maximum_external_extent,
        p.maximum_journal_bytes};
    if (!converted.valid()) return false;
    target = converted;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_INDEPENDENT_SET_COVER: {
    const auto &p = source.value.independent_set_cover;
    strategy::IndependentSetCoverParameters converted{
        p.maximum_clauses,
        p.maximum_literals,
        p.maximum_recognition_work,
        p.maximum_proof_work,
        p.maximum_proof_additions,
        p.maximum_proof_deletions,
        p.maximum_proof_literal_occurrences,
        p.maximum_journal_bytes,
        p.maximum_vertices,
        p.maximum_target};
    if (!converted.valid()) return false;
    target = converted;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_SEMANTIC_VARIABLE_ADVICE: {
    const auto &p = source.value.semantic_variable_advice;
    strategy::SemanticVariableAdviceParameters converted{
        p.maximum_variables, p.maximum_priority_sum,
        p.existing_theory_priority, p.introduced_theory_priority};
    if (!converted.valid()) return false;
    target = converted;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_GF3_AFFINE_CONSEQUENCE: {
    const auto &p = source.value.gf3_affine_consequence;
    strategy::Gf3AffineConsequenceParameters converted{
        p.maximum_source_clauses,
        p.maximum_source_literals,
        p.maximum_domains,
        p.maximum_equations,
        p.maximum_equation_arity,
        p.maximum_tuples_per_equation,
        p.maximum_result_arity,
        p.maximum_result_tuples,
        p.maximum_additions,
        p.maximum_addition_literals,
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
    if (!converted.valid()) return false;
    target = converted;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_DIRECT_CSP_SUPPORT: {
    const auto &p = source.value.direct_csp_support;
    if (p.allow_extension != 0 && p.allow_extension != 1) return false;
    strategy::DirectCspSupportParameters converted{
        p.maximum_source_clauses,
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
        p.allow_extension != 0};
    if (!converted.valid()) return false;
    target = converted;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_NARY_CSP_PREFIX:
    return false; // Retired tag 42; never reinterpret old payloads.
  case TACTIX_ONLINE_PARAMETERS_NARY_CSP_EQUALITY: {
    const auto &p = source.value.nary_csp_equality;
    strategy::NaryCspEqualityParameters converted{
        p.maximum_source_clauses,
        p.maximum_source_literals,
        p.maximum_recognition_work,
        p.maximum_domain_values,
        p.maximum_forbidden_tuples,
        p.maximum_proof_work,
        p.maximum_proof_additions,
        p.maximum_proof_literal_occurrences,
        p.maximum_journal_bytes,
        p.maximum_extent_growth,
        p.maximum_external_extent,
        p.maximum_session_keys};
    if (!converted.valid()) return false;
    target = converted;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_NARY_CSP_EQUALITY_AMO: {
    const auto &p = source.value.nary_csp_equality_amo;
    strategy::NaryCspEqualityAmoParameters converted{
        p.maximum_source_clauses,
        p.maximum_source_literals,
        p.maximum_recognition_work,
        p.maximum_domain_values,
        p.maximum_forbidden_tuples,
        p.maximum_proof_work,
        p.maximum_proof_additions,
        p.maximum_proof_literal_occurrences,
        p.maximum_journal_bytes,
        p.maximum_external_extent,
        p.maximum_session_keys};
    if (!converted.valid()) return false;
    target = converted;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_DIRECT_PHP_REFUTATION: {
    const auto &p = source.value.direct_php_refutation;
    strategy::DirectPhpRefutationParameters converted{
        p.maximum_snapshot_clauses, p.maximum_snapshot_literals,
        p.maximum_snapshot_work, p.maximum_recognition_clauses,
        p.maximum_recognition_literals, p.maximum_recognition_work,
        p.maximum_row_candidates, p.maximum_row_combinations,
        p.maximum_mapping_trials, p.maximum_holes,
        p.maximum_proof_additions, p.maximum_proof_deletions,
        p.maximum_proof_literals, p.maximum_fresh_variables,
        p.maximum_proof_events, p.maximum_replay_work};
    if (!converted.valid()) return false;
    target = converted;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_GF3_GAUSSIAN_CONSEQUENCE: {
    const auto &p = source.value.gf3_gaussian_consequence;
    strategy::Gf3GaussianConsequenceParameters converted;
    strategy::copy_gf3_gaussian_parameter_fields(converted, p);
    if (!converted.valid()) return false;
    target = converted;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_GF3_GENERAL_ROW_CONSEQUENCE: {
    const auto &source_row = source.value.gf3_general_row_consequence;
    const auto &p = source_row.gaussian;
    strategy::Gf3GaussianConsequenceParameters gaussian;
    strategy::copy_gf3_gaussian_parameter_fields(gaussian, p);
    strategy::Gf3GeneralRowConsequenceParameters converted{
        gaussian, source_row.maximum_row_tuples,
        source_row.maximum_row_width,
        source_row.maximum_duplicate_scan_work,
        source_row.maximum_arena_words};
    if (!converted.valid()) return false;
    target = converted;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_TSEITIN_CYCLE_BREAKING: {
    const auto &p = source.value.tseitin_cycle_breaking;
    strategy::TseitinCycleBreakingParameters converted{
        p.maximum_snapshot_clauses, p.maximum_snapshot_literals,
        p.maximum_snapshot_work, p.maximum_snapshot_variables,
        p.maximum_discovery_work, p.maximum_discovery_clauses,
        p.maximum_discovery_groups, p.maximum_discovery_retained_bytes,
        p.maximum_admission_work, p.maximum_planning_work,
        p.maximum_witness_length, p.maximum_planning_additions,
        p.maximum_transaction_work, p.maximum_transaction_additions,
        p.maximum_transaction_deletions, p.maximum_transaction_helpers,
        p.maximum_transaction_witness_literals, p.maximum_validation_ticks,
        p.maximum_components, p.maximum_identity_bytes,
        p.maximum_total_identity_bytes, p.maximum_identity_work,
        p.maximum_journal_bytes,
        p.maximum_session_keys, p.maximum_session_work,
        p.maximum_session_identity_bytes};
    if (!converted.valid()) return false;
    target = converted;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_CONGRUENCE: {
    const tactix_online_congruence_parameters &p = source.value.congruence;
    if (p.gate_count_limit > std::numeric_limits<size_t>::max() ||
        p.binary_watch_order < TACTIX_CONGRUENCE_CANONICAL_WATCH_ORDER ||
        p.binary_watch_order > TACTIX_CONGRUENCE_PRESERVE_WATCH_ORDER)
      return false;
    target = strategy::CongruenceParameters{
        p.and_arity_limit, static_cast<size_t>(p.gate_count_limit),
        p.xor_arity_limit,
        static_cast<strategy::CongruenceBinaryWatchOrder>(p.binary_watch_order)};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_INSTANTIATE: {
    const tactix_online_instantiate_parameters &p = source.value.instantiate;
    target = strategy::InstantiateParameters{
        p.minimum_clause_size, p.occurrence_limit, p.minimum_unassigned,
        p.include_redundant != 0, p.new_material_threshold};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_BACKBONE: {
    const tactix_online_backbone_parameters &p = source.value.backbone;
    target = strategy::BackboneParameters{p.initial_rounds,
                                          p.maximum_rounds};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_SWEEP: {
    const tactix_online_sweep_parameters &p = source.value.sweep;
    target = strategy::SweepParameters{
        p.max_depth, p.max_depth_ceiling, p.max_env_vars,
        p.max_env_vars_ceiling, p.max_env_clauses,
        p.max_env_clauses_ceiling, p.max_candidates, p.env_ticks,
        p.max_proof_steps, p.max_seeds, p.round_ticks,
        p.strip_false != 0, p.measure_closure != 0,
        p.full_partition != 0, p.skip_embedded_fixed != 0,
        p.flip_rounds, p.resume != 0, p.force_general_proof != 0};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_VIVIFY: {
    const tactix_online_vivify_parameters &p = source.value.vivify;
    if (p.scope > UINT8_MAX)
      return false;
    target = strategy::VivifyParameters{
        static_cast<uint8_t>(p.scope), p.reuse != 0, p.flush != 0,
        p.irredundant_sort_ratio, p.band != 0, p.band_budget, p.band_offset};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_ANALYZE: {
    const tactix_online_analyze_parameters &p = source.value.analyze;
    target = strategy::AnalyzeParameters{
        p.minimize_depth_limit, p.conflict_reuse != 0,
        p.chronological_levels, p.bump_reason_limit, p.bump_reason_depth,
        p.eager_subsume, p.disable_jump_reasons == 0,
        p.enable_jump_reasons_bigbig != 0,
        p.disable_shrink_glue_promotion == 0,
        p.exclude_extension_reason_bumps == 0};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_BCE:
    target = strategy::BceParameters{source.value.bce.occurrence_limit};
    return true;
  case TACTIX_ONLINE_PARAMETERS_TERNARY:
    target = strategy::TernaryParameters{
        source.value.ternary.occurrence_limit,
        source.value.ternary.maximum_add_percent};
    return true;
  case TACTIX_ONLINE_PARAMETERS_FACTOR: {
    const tactix_online_factor_parameters &p = source.value.factor;
    target = strategy::FactorParameters{
        p.maximum_clause_size, p.growth_bound, p.candidate_rounds,
        p.composite != 0, p.demote_fresh != 0};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_TRANSITIVE: {
    const tactix_online_transitive_parameters &p = source.value.transitive;
    target = strategy::TransitiveReduceParameters{
        p.early_exit != 0, p.live_meter != 0,
        p.checked_budget_meter != 0};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_SUBSTITUTE:
    target = strategy::SubstituteParameters{
        source.value.substitute.demote_substituted != 0,
        source.value.substitute.max_rounds,
        source.value.substitute.round_effort_ceiling != 0};
    return true;
  case TACTIX_ONLINE_PARAMETERS_REDUCE: {
    const tactix_online_reduce_parameters &source_reduce =
        source.value.reduce;
    if (source_reduce.tier_protection_mode < TACTIX_ONLINE_SETTING_INHERIT ||
        source_reduce.tier_protection_mode > TACTIX_ONLINE_SETTING_ENABLE ||
        source_reduce.compact_collection_mode < TACTIX_ONLINE_SETTING_INHERIT ||
        source_reduce.compact_collection_mode > TACTIX_ONLINE_SETTING_ENABLE ||
        source_reduce.reduction_bookkeeping_mode < TACTIX_ONLINE_SETTING_INHERIT ||
        source_reduce.reduction_bookkeeping_mode > TACTIX_ONLINE_SETTING_ENABLE)
      return false;
    strategy::ReduceParameters reduce;
    reduce.tier1 = source_reduce.tier1;
    reduce.tier2 = source_reduce.tier2;
    if (source_reduce.victim_denominator) {
      reduce.victim_numerator = source_reduce.victim_numerator;
      reduce.victim_denominator = source_reduce.victim_denominator;
    }
    if (source_reduce.tier_protection_mode != TACTIX_ONLINE_SETTING_INHERIT)
      reduce.protect_tiers =
          source_reduce.tier_protection_mode == TACTIX_ONLINE_SETTING_ENABLE;
    if (source_reduce.compact_collection_mode !=
        TACTIX_ONLINE_SETTING_INHERIT)
      reduce.compact_collection = source_reduce.compact_collection_mode ==
                                  TACTIX_ONLINE_SETTING_ENABLE;
    if (source_reduce.reduction_bookkeeping_mode != TACTIX_ONLINE_SETTING_INHERIT)
      reduce.reduction_bookkeeping = source_reduce.reduction_bookkeeping_mode ==
                                     TACTIX_ONLINE_SETTING_ENABLE;
    target = reduce;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_WALK: {
    const tactix_online_walk_parameters &source_walk = source.value.walk;
    if (source_walk.score_curve_count < 0 ||
        (source_walk.score_curve_count && !source_walk.score_curve))
      return false;
    strategy::WalkParameters walk;
    walk.step_budget = source_walk.step_budget;
    walk.random_seed = source_walk.random_seed;
    walk.seed_candidate = source_walk.seed_candidate;
    walk.algorithm = static_cast<strategy::WalkParameters::Algorithm>(
        source_walk.algorithm);
    walk.initialization =
        static_cast<strategy::WalkParameters::Initialization>(
            source_walk.initialization);
    if (source_walk.maximum_tries)
      walk.maximum_tries = source_walk.maximum_tries;
    if (source_walk.flips_per_try)
      walk.flips_per_try = source_walk.flips_per_try;
    if (source_walk.weight_smoothing_threshold)
      walk.weight_smoothing_threshold =
          source_walk.weight_smoothing_threshold;
    if (source_walk.smoothing_denominator) {
      walk.smoothing_old_numerator = source_walk.smoothing_old_numerator;
      walk.smoothing_denominator = source_walk.smoothing_denominator;
    }
    if (source_walk.aspiration_mode != TACTIX_ONLINE_SETTING_INHERIT) {
      if (source_walk.aspiration_mode < TACTIX_ONLINE_SETTING_DISABLE ||
          source_walk.aspiration_mode > TACTIX_ONLINE_SETTING_ENABLE)
        return false;
      walk.aspiration =
          source_walk.aspiration_mode == TACTIX_ONLINE_SETTING_ENABLE;
    }
    if (source_walk.even_break_base_milli)
      walk.even_break_base_milli = source_walk.even_break_base_milli;
    if (source_walk.score_curve_count) {
      walk.score_curve.clear();
      walk.score_curve.reserve(
          static_cast<size_t>(source_walk.score_curve_count));
      for (int i = 0; i < source_walk.score_curve_count; ++i)
        walk.score_curve.push_back(
            {source_walk.score_curve[i].average_clause_size_milli,
             source_walk.score_curve[i].break_base_milli});
    }
    target = std::move(walk);
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_REORDER: {
    const tactix_online_reorder_parameters &source_reorder =
        source.value.reorder;
    strategy::ReorderParameters reorder;
    reorder.maximum_clause_size = source_reorder.maximum_clause_size;
    if (source_reorder.clause_decay_milli)
      reorder.clause_decay_milli = source_reorder.clause_decay_milli;
    if (source_reorder.binary_weight_milli)
      reorder.binary_weight_milli = source_reorder.binary_weight_milli;
    if (source_reorder.balanced_polarity_milli)
      reorder.balanced_polarity_milli =
          source_reorder.balanced_polarity_milli;
    target = reorder;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_PARITY_MODEL: {
    const tactix_online_parity_model_parameters &p =
        source.value.parity_model;
    strategy::ParityModelParameters converted;
    converted.maximum_variables = p.maximum_variables;
    converted.maximum_groups = p.maximum_groups;
    converted.maximum_constraints = p.maximum_constraints;
    converted.minimum_group_clauses = p.minimum_group_clauses;
    converted.minimum_equations = p.minimum_equations;
    converted.maximum_free_variables = p.maximum_free_variables;
    converted.candidate_budget = p.candidate_budget;
    if (p.refutation_maximum_variables)
      converted.refutation_maximum_variables = p.refutation_maximum_variables;
    if (p.refutation_maximum_groups)
      converted.refutation_maximum_groups = p.refutation_maximum_groups;
    if (p.refutation_maximum_constraints)
      converted.refutation_maximum_constraints =
          p.refutation_maximum_constraints;
    if (p.refutation_maximum_arity)
      converted.refutation_maximum_arity = p.refutation_maximum_arity;
    if (p.refutation_probe_constraints)
      converted.refutation_probe_constraints = p.refutation_probe_constraints;
    if (p.refutation_minimum_probe_repeats)
      converted.refutation_minimum_probe_repeats =
          p.refutation_minimum_probe_repeats;
    if (p.refutation_maximum_proof_additions)
      converted.refutation_maximum_proof_additions =
          p.refutation_maximum_proof_additions;
    target = converted;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_ONEHOT_MODEL: {
    const tactix_online_onehot_model_parameters &p =
        source.value.onehot_model;
    strategy::OnehotModelParameters converted{
        p.maximum_variables, p.maximum_domains, p.maximum_constraints,
        p.maximum_equation_arity, p.minimum_domains, p.minimum_equations,
        p.maximum_free_domains, p.candidate_budget};
    if (p.refutation_maximum_variables)
      converted.refutation_maximum_variables = p.refutation_maximum_variables;
    if (p.refutation_maximum_groups)
      converted.refutation_maximum_groups = p.refutation_maximum_groups;
    if (p.refutation_maximum_constraints)
      converted.refutation_maximum_constraints =
          p.refutation_maximum_constraints;
    if (p.refutation_maximum_arity)
      converted.refutation_maximum_arity = p.refutation_maximum_arity;
    if (p.refutation_maximum_proof_additions)
      converted.refutation_maximum_proof_additions =
          p.refutation_maximum_proof_additions;
    target = converted;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_BALANCED_SIGNATURE_MODEL: {
    const tactix_online_balanced_signature_model_parameters &p =
        source.value.balanced_signature_model;
    strategy::BalancedSignatureModelParameters balanced{
        p.maximum_balls, p.maximum_width, p.restarts, p.steps_per_restart,
        p.samples_per_step, p.seed, p.completion_rounds};
    if (!balanced.valid())
      return false;
    target = balanced;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_FERMAT_MODEL: {
    const tactix_online_fermat_model_parameters &p =
        source.value.fermat_model;
    strategy::FermatModelParameters fermat{
        p.maximum_bits, p.comparator_tail_clauses, p.rho_constants,
        p.rho_iterations};
    if (!fermat.valid())
      return false;
    target = fermat;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_LOOKUP_CIRCUIT_MODEL: {
    const tactix_online_lookup_circuit_model_parameters &p =
        source.value.lookup_circuit_model;
    strategy::LookupCircuitModelParameters lookup{
        p.maximum_fanin, p.maximum_fanout, p.maximum_primary_inputs,
        p.maximum_support, p.maximum_vector_gate_evaluations};
    if (!lookup.valid())
      return false;
    target = lookup;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_SIMON_MODEL: {
    const tactix_online_simon_model_parameters &p = source.value.simon_model;
    strategy::SimonModelParameters simon{p.maximum_rounds,
                                         p.maximum_candidates};
    if (!simon.valid())
      return false;
    target = simon;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_STCONN_PROOF: {
    const tactix_online_stconn_proof_parameters &p =
        source.value.stconn_proof;
    strategy::StconnProofParameters stconn{
        p.maximum_width, p.maximum_columns, p.maximum_rup_clauses};
    if (!stconn.valid())
      return false;
    target = stconn;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_SIGNED_CARDINALITY_PROOF: {
    const tactix_online_signed_cardinality_proof_parameters &p =
        source.value.signed_cardinality_proof;
    strategy::SignedCardinalityProofParameters signed_cardinality{
        p.maximum_variables, p.maximum_groups, p.maximum_constraints,
        p.maximum_group_arity, p.maximum_bdd_nodes,
        p.maximum_proof_additions};
    if (!signed_cardinality.valid())
      return false;
    target = signed_cardinality;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_EXACT_COVER_MODEL: {
    const tactix_online_exact_cover_model_parameters &p =
        source.value.exact_cover_model;
    strategy::ExactCoverModelParameters exact_cover{
        p.block_arity, p.maximum_nodes, p.restarts, p.restart_nodes, p.seed};
    if (!exact_cover.valid())
      return false;
    target = exact_cover;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_INDEPENDENT_SET_MODEL: {
    const tactix_online_independent_set_model_parameters &p =
        source.value.independent_set_model;
    strategy::IndependentSetModelParameters independent_set{
        p.maximum_vertices, p.maximum_steps, p.completion_rounds, p.seed};
    if (!independent_set.valid())
      return false;
    target = independent_set;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_RAMSEY_STARTUP: {
    const auto &p = source.value.ramsey_startup;
    strategy::RamseyStartupParameters ramsey{p.recognition_clauses,
        p.recognition_variables, p.recognition_pair_cells, p.recognition_work,
        p.certificate_work, p.maximum_nodes, p.maximum_additions, p.maximum_external_extent};
    if (!ramsey.valid()) return false;
    target = ramsey;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_GRAPH_LABELING: {
    const auto &p = source.value.graph_labeling;
    strategy::GraphLabelingParameters graph;
    graph.maximum_vertices = p.maximum_vertices;
    graph.workspace_bytes = p.workspace_bytes;
    static_assert(TACTIX_GRAPH_STAGES == strategy::GRAPH_LABELING_STAGES);
    std::copy(std::begin(p.work), std::end(p.work), graph.work.begin());
    graph.maximum_dfs_vertices = p.maximum_dfs_vertices;
    graph.maximum_dfs_nodes = p.maximum_dfs_nodes;
    graph.maximum_dfs_starts = p.maximum_dfs_starts;
    graph.early_dfs_choices = p.early_dfs_choices;
    graph.repair_rounds = p.repair_rounds;
    graph.anneal_iterations = p.anneal_iterations;
    graph.stall_anneal_iterations = p.stall_anneal_iterations;
    graph.seed = p.seed;
    if (!graph.valid()) return false;
    target = graph;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_AFFINE_MODEL: {
    const auto &p = source.value.affine_model;
    if (p.projection_count < 0 || p.scope_count < 0 ||
        (p.projection_count && !p.projection) || (p.scope_count && !p.scopes) ||
        p.projection_mode < TACTIX_ONLINE_AFFINE_BOUNDARY ||
        p.projection_mode > TACTIX_ONLINE_AFFINE_EXPLICIT ||
        p.automatic_negated < 0 || p.automatic_negated > 1 ||
        p.shuffle_first < 0 || p.shuffle_first > 1 ||
        p.free_phase < 0 || p.free_phase > 1 ||
        p.residual_phase < 0 || p.residual_phase > 1)
      return false;
    strategy::AffineModelParameters a;
    a.maximum_variables=p.maximum_variables; a.maximum_constraints=p.maximum_constraints;
    a.maximum_arity=p.maximum_arity; a.maximum_projection=p.maximum_projection;
    a.minimum_equations=p.minimum_equations; a.maximum_entries=p.maximum_entries;
    a.import_bytes=p.import_bytes; a.linear_bytes=p.linear_bytes;
    a.table_bytes=p.table_bytes; a.maximum_work=p.maximum_work;
    a.candidate_budget=p.candidate_budget; a.permutations=p.permutations; a.seed=p.seed;
    a.projection_mode=static_cast<strategy::AffineProjectionMode>(p.projection_mode);
    a.automatic_negated=p.automatic_negated; a.shuffle_first=p.shuffle_first;
    a.free_phase=p.free_phase; a.residual_phase=p.residual_phase;
    if(p.projection_count) a.projection.assign(p.projection,p.projection+p.projection_count);
    a.scopes.clear();
    for(int i=0;i<p.scope_count;++i) {
      const auto &scope=p.scopes[i];
      a.scopes.push_back({scope.left_size,scope.left_weight,scope.right_weight,scope.total_weight});
    }
    if(!a.valid())return false;
    target=std::move(a);
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY: {
    const tactix_online_sr_parameters &p =
        source.value.substitution_redundancy;
    if ((p.orbitopal_producer && p.general_producer) ||
        p.producer < TACTIX_ONLINE_SR_EXPLICIT_ONLY ||
        p.producer > TACTIX_ONLINE_SR_GRAPH_SYMMETRY)
      return false;
    SubstitutionRedundancyParameters sr;
    sr.maximum_validation_ticks = p.maximum_validation_ticks;
    sr.producer = static_cast<SubstitutionRedundancyProducer>(p.producer);
    if (p.orbitopal_producer || p.general_producer) {
      const auto legacy = p.orbitopal_producer
                              ? SubstitutionRedundancyProducer::Orbitopal
                              : SubstitutionRedundancyProducer::GeneralComplete;
      if (p.producer != TACTIX_ONLINE_SR_EXPLICIT_ONLY &&
          sr.producer != legacy)
        return false;
      sr.producer = legacy;
    }
    if (p.complete_profile_mode < TACTIX_ONLINE_SETTING_INHERIT ||
        p.complete_profile_mode > TACTIX_ONLINE_SETTING_ENABLE)
      return false;
    const bool complete = p.complete_profile_mode == TACTIX_ONLINE_SETTING_ENABLE;
    if (complete) {
      if (!online_orbitopal_parameters(p, sr) ||
          !online_graph_symmetry_parameters(p, sr.graph_symmetry))
        return false;
      sr.maximum_detection_ticks = p.maximum_detection_ticks;
      sr.maximum_general_candidates_per_advance =
          p.maximum_general_candidates_per_advance;
    } else if (sr.producer == SubstitutionRedundancyProducer::Orbitopal) {
      if (!online_orbitopal_parameters(p, sr))
        return false;
      sr.maximum_detection_ticks = p.maximum_detection_ticks;
      if (sr.orbitopal_completion ==
              OrbitopalCompletion::IndividualizedGraphCompletion &&
          !online_graph_symmetry_parameters(p, sr.graph_symmetry))
        return false;
    } else if (sr.producer ==
               SubstitutionRedundancyProducer::GeneralComplete) {
      sr.maximum_general_candidates_per_advance =
          p.maximum_general_candidates_per_advance;
    } else if (sr.producer ==
               SubstitutionRedundancyProducer::GraphSymmetry) {
      if (!online_graph_symmetry_parameters(p, sr.graph_symmetry))
        return false;
      sr.maximum_detection_ticks = p.maximum_detection_ticks;
    }
    target = sr;
    return true;
  }
  default:
    return false;
  }
}

bool online_program(const tactix_online_program &source, const Kernel &kernel,
                    strategy::OnlineInprocessProgram &target) {
  if (source.step_count < 0 ||
      (source.step_count && !source.steps) ||
      !online_repeat(source.repeat_mode, source.repeat_limit, target.repeat))
    return false;
  target.sequence.clear();
  target.sequence.reserve(static_cast<size_t>(source.step_count));
  const auto &rules = tactix_rules();
  for (int i = 0; i < source.step_count; ++i) {
    const tactix_online_step &in = source.steps[i];
    if (in.rule_id < 0 || in.rule_id >= static_cast<int>(rules.size()) ||
        rules[in.rule_id].retired ||
        in.instance_count < 0 || in.factor_grid_count < 0 ||
        (in.instance_count && !in.instances) ||
        (in.factor_grid_count && !in.factor_grids))
      return false;
    strategy::OnlineInvocation out;
    out.id = in.step_id;
    out.rule = rules[static_cast<size_t>(in.rule_id)].name;
    if (!online_parameters(in.parameters, out.parameters) ||
        !online_granularity(in.granularity, out.granularity) ||
        !online_repeat(in.repeat_mode, in.repeat_limit, out.repeat) ||
        !online_post_application(in.post_application,
                                 out.post_application) ||
        !online_rule_work_budget(in.rule_budget_mode, in.rule_budget,
                                 out.rule_work_budget))
      return false;
    if (!online_setting_override(in.touch_filter_mode,
                                 out.material.touch_filter) ||
        !online_setting_override(in.likely_kept_redundant_mode,
                                 out.material.likely_kept_redundant))
      return false;
    if (!online_work_unit(in.work_unit, out.work_unit))
      return false;
    if (in.instance_count)
      out.instances.assign(in.instances, in.instances + in.instance_count);
    for (int grid_index = 0; grid_index < in.factor_grid_count; ++grid_index) {
      const tactix_online_factor_grid &source_grid =
          in.factor_grids[grid_index];
      if (source_grid.factor_count < 0 || source_grid.remainder_count < 0 ||
          (source_grid.factor_count && !source_grid.factors) ||
          (source_grid.remainder_count && !source_grid.remainders))
        return false;
      FactorGrid grid;
      grid.factors.reserve(static_cast<size_t>(source_grid.factor_count));
      for (int factor_index = 0; factor_index < source_grid.factor_count;
           ++factor_index) {
        const int external = source_grid.factors[factor_index];
        if (!external || external == INT32_MIN)
          return false;
        const Lit literal = kernel_internalize(kernel, external);
        if (literal == INVALID_LIT)
          return false;
        grid.factors.push_back(literal);
      }
      grid.remainders.reserve(
          static_cast<size_t>(source_grid.remainder_count));
      for (int remainder_index = 0;
           remainder_index < source_grid.remainder_count; ++remainder_index) {
        const tactix_online_factor_remainder &source_remainder =
            source_grid.remainders[remainder_index];
        if (source_remainder.literal_count < 0 ||
            (source_remainder.literal_count && !source_remainder.literals))
          return false;
        std::vector<Lit> remainder;
        remainder.reserve(
            static_cast<size_t>(source_remainder.literal_count));
        for (int literal_index = 0;
             literal_index < source_remainder.literal_count; ++literal_index) {
          const int external = source_remainder.literals[literal_index];
          if (!external || external == INT32_MIN)
            return false;
          const Lit literal = kernel_internalize(kernel, external);
          if (literal == INVALID_LIT)
            return false;
          remainder.push_back(literal);
        }
        grid.remainders.push_back(std::move(remainder));
      }
      out.factor_grids.push_back(std::move(grid));
    }
    out.budget = in.budget_unlimited
                     ? strategy::WorkLimit::unlimited()
                     : strategy::WorkLimit::bounded(in.budget);
    target.sequence.push_back(std::move(out));
  }
  return true;
}

template <class BaseProgram, class Source, class Convert>
bool online_base_program(const Source &source, const Kernel &kernel,
                         strategy::OnlineInprocessProgram &target,
                         bool reserve_steps, Convert &&convert) {
  if (source.step_count < 0 || (source.step_count && !source.steps))
    return false;
  std::vector<decltype(source.steps[0].base)> base_steps;
  if (reserve_steps)
    base_steps.reserve(static_cast<size_t>(source.step_count));
  for (int i = 0; i < source.step_count; ++i)
    base_steps.push_back(source.steps[i].base);
  const BaseProgram base{base_steps.data(), source.step_count,
                         source.repeat_mode, source.repeat_limit};
  if constexpr (std::is_invocable_r_v<
                    bool, Convert, const BaseProgram &, const Kernel &,
                    strategy::OnlineInprocessProgram &, bool>)
    return std::forward<Convert>(convert)(base, kernel, target, false);
  else
    return std::forward<Convert>(convert)(base, kernel, target);
}

bool online_program_v2(const tactix_online_program_v2 &source,
                       const Kernel &kernel,
                       strategy::OnlineInprocessProgram &target,
                       bool validate = true) {
  if (!online_base_program<tactix_online_program>(
          source, kernel, target, true, online_program))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const tactix_online_step_v2 &input_step = source.steps[i];
    if (input_step.sr_instance_count < 0 ||
        (input_step.sr_instance_count && !input_step.sr_instances))
      return false;
    strategy::OnlineInvocation &output_step = target.sequence[i];
    output_step.substitution_redundancy_instances.reserve(
        static_cast<size_t>(input_step.sr_instance_count));
    for (int j = 0; j < input_step.sr_instance_count; ++j) {
      const tactix_online_sr_instance &input = input_step.sr_instances[j];
      if (input.clause_count < 0 || input.witness_count < 0 ||
          input.substitution_count < 0 ||
          (input.clause_count && !input.clause) ||
          (input.witness_count && !input.witness) ||
          (input.substitution_count && !input.substitution))
        return false;
      SubstitutionRedundancyInstance output;
      const auto append_literal = [&](int external,
                                      std::vector<Lit> &destination) {
        if (!external || external == INT32_MIN)
          return false;
        const Lit literal = kernel_internalize(kernel, external);
        if (literal == INVALID_LIT)
          return false;
        destination.push_back(literal);
        return true;
      };
      output.clause.reserve(static_cast<size_t>(input.clause_count));
      for (int k = 0; k < input.clause_count; ++k)
        if (!append_literal(input.clause[k], output.clause))
          return false;
      output.witness.reserve(static_cast<size_t>(input.witness_count));
      for (int k = 0; k < input.witness_count; ++k)
        if (!append_literal(input.witness[k], output.witness))
          return false;
      output.substitution.reserve(
          static_cast<size_t>(input.substitution_count));
      for (int k = 0; k < input.substitution_count; ++k) {
        std::vector<Lit> pair;
        pair.reserve(2);
        if (!append_literal(input.substitution[k].source_literal, pair) ||
            !append_literal(input.substitution[k].target_literal, pair))
          return false;
        output.substitution.emplace_back(pair[0], pair[1]);
      }
      output_step.substitution_redundancy_instances.push_back(
          std::move(output));
    }
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v3(const tactix_online_program_v3 &source,
                       const Kernel &kernel,
                       strategy::OnlineInprocessProgram &target,
                       bool validate = true) {
  if (!online_base_program<tactix_online_program_v2>(
          source, kernel, target, true, online_program_v2))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const tactix_online_step_v3 &input_step = source.steps[i];
    if (input_step.condition_instance_count < 0 ||
        (input_step.condition_instance_count &&
         !input_step.condition_instances))
      return false;
    strategy::OnlineInvocation &output_step = target.sequence[i];
    output_step.condition_instances.reserve(
        static_cast<size_t>(input_step.condition_instance_count));
    for (int j = 0; j < input_step.condition_instance_count; ++j) {
      const tactix_online_condition_instance &input =
          input_step.condition_instances[j];
      if (input.clause_count < 0 || input.witness_count < 0 ||
          (input.clause_count && !input.clause) ||
          (input.witness_count && !input.witness))
        return false;
      ConditionInstance output;
      output.clause.reserve(static_cast<size_t>(input.clause_count));
      output.witness.reserve(static_cast<size_t>(input.witness_count));
      for (int k = 0; k < input.clause_count; ++k) {
        const int external = input.clause[k];
        if (!external || external == INT32_MIN)
          return false;
        if (kernel_internalize(kernel, external) == INVALID_LIT)
          return false;
        // Keep the request in external identities: an earlier online step
        // may compact variables before this instance is selected.
        output.clause.push_back(internalize(external));
      }
      for (int k = 0; k < input.witness_count; ++k) {
        const int external = input.witness[k];
        if (!external || external == INT32_MIN)
          return false;
        const Lit literal = internalize(external);
        if (var_of(literal) >= kernel.external_to_internal.size())
          return false;
        output.witness.push_back(literal);
      }
      output_step.condition_instances.push_back(std::move(output));
    }
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v4(const tactix_online_program_v4 &source,
                       const Kernel &kernel,
                       strategy::OnlineInprocessProgram &target,
                       bool validate = true) {
  if (!online_base_program<tactix_online_program_v3>(
          source, kernel, target, true, online_program_v3))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const tactix_online_step_v4 &input_step = source.steps[i];
    if (input_step.independent_set_cover_instance_count < 0 ||
        (input_step.independent_set_cover_instance_count &&
         !input_step.independent_set_cover_instances))
      return false;
    strategy::OnlineInvocation &output_step = target.sequence[i];
    output_step.independent_set_cover_instances.reserve(
        static_cast<size_t>(
            input_step.independent_set_cover_instance_count));
    for (int j = 0; j < input_step.independent_set_cover_instance_count; ++j) {
      const tactix_online_independent_set_cover_instance &input =
          input_step.independent_set_cover_instances[j];
      if (input.vertex_count < 2 || !input.vertices)
        return false;
      std::vector<uint32_t> cover;
      cover.reserve(static_cast<size_t>(input.vertex_count));
      uint32_t previous = 0;
      for (int k = 0; k < input.vertex_count; ++k) {
        const uint32_t external = input.vertices[k];
        if (!external || external > static_cast<uint32_t>(INT32_MAX) ||
            external <= previous ||
            kernel_internalize(kernel, static_cast<int>(external)) ==
                INVALID_LIT)
          return false;
        cover.push_back(external);
        previous = external;
      }
      output_step.independent_set_cover_instances.push_back(std::move(cover));
    }
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v5(const tactix_online_program_v5 &source,
                       const Kernel &kernel,
                       strategy::OnlineInprocessProgram &target,
                       bool validate = true) {
  if (!online_base_program<tactix_online_program_v4>(
          source, kernel, target, true, online_program_v4))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const tactix_online_step_v5 &input_step = source.steps[i];
    if (input_step.gf3_affine_instance_count < 0 ||
        (input_step.gf3_affine_instance_count &&
         !input_step.gf3_affine_instances) ||
        input_step.direct_csp_support_instance_count < 0 ||
        (input_step.direct_csp_support_instance_count &&
         !input_step.direct_csp_support_instances))
      return false;
    strategy::OnlineInvocation &output_step = target.sequence[i];
    output_step.gf3_affine_instances.reserve(
        static_cast<size_t>(input_step.gf3_affine_instance_count));
    for (int j = 0; j < input_step.gf3_affine_instance_count; ++j) {
      const auto &input = input_step.gf3_affine_instances[j];
      output_step.gf3_affine_instances.push_back(
          {input.first_equation, input.second_equation});
    }
    output_step.direct_csp_support_instances.reserve(
        static_cast<size_t>(input_step.direct_csp_support_instance_count));
    for (int j = 0; j < input_step.direct_csp_support_instance_count; ++j) {
      const auto &input = input_step.direct_csp_support_instances[j];
      if (input.left_value_count < 0 || input.right_value_count < 0 ||
          (input.left_value_count && !input.left_values) ||
          (input.right_value_count && !input.right_values))
        return false;
      DirectCspSupportActionSelection output;
      output.left_values.reserve(static_cast<size_t>(input.left_value_count));
      for (int k = 0; k < input.left_value_count; ++k) {
        const int64_t literal = input.left_values[k];
        if (literal < INT32_MIN || literal > INT32_MAX) return false;
        output.left_values.push_back(static_cast<int32_t>(literal));
      }
      output.right_values.reserve(
          static_cast<size_t>(input.right_value_count));
      for (int k = 0; k < input.right_value_count; ++k) {
        const int64_t literal = input.right_values[k];
        if (literal < INT32_MIN || literal > INT32_MAX) return false;
        output.right_values.push_back(static_cast<int32_t>(literal));
      }
      output.selected_left_value = input.selected_left_value;
      output_step.direct_csp_support_instances.push_back(std::move(output));
    }
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v6(const tactix_online_program_v6 &source,
                       const Kernel &kernel,
                       strategy::OnlineInprocessProgram &target,
                       bool validate = true) {
  if (!online_base_program<tactix_online_program_v5>(
          source, kernel, target, true, online_program_v5))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &input = source.steps[i];
    if (input.gf3_gaussian_instance_count < 0 ||
        (input.gf3_gaussian_instance_count && !input.gf3_gaussian_instances))
      return false;
    auto &output = target.sequence[i].gf3_gaussian_instances;
    output.reserve(static_cast<size_t>(input.gf3_gaussian_instance_count));
    for (int j = 0; j < input.gf3_gaussian_instance_count; ++j) {
      const char *identity = input.gf3_gaussian_instances[j].identity;
      if (!identity) return false;
      const auto *p = std::get_if<strategy::Gf3GaussianConsequenceParameters>(
          &target.sequence[i].parameters);
      if (!p) return false;
      const size_t maximum = static_cast<size_t>(std::min<uint64_t>(
          p->maximum_identity_bytes, std::numeric_limits<size_t>::max() - 1));
      const size_t length = strnlen(identity, maximum + 1);
      if (length > maximum) return false;
      output.emplace_back(identity, length);
    }
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v7(const tactix_online_program_v7 &source,
                       const Kernel &kernel,
                       strategy::OnlineInprocessProgram &target,
                       bool validate = true) {
  if (!online_base_program<tactix_online_program_v6>(
          source, kernel, target, true, online_program_v6))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &input = source.steps[i];
    if (input.tseitin_cycle_instance_count < 0 ||
        input.tseitin_cycle_instance_count > 1 ||
        (input.tseitin_cycle_instance_count &&
         !input.tseitin_cycle_instances))
      return false;
    auto &output = target.sequence[i].tseitin_cycle_instances;
    output.reserve(static_cast<size_t>(input.tseitin_cycle_instance_count));
    for (int j = 0; j < input.tseitin_cycle_instance_count; ++j) {
      const char *identity = input.tseitin_cycle_instances[j].identity;
      if (!identity) return false;
      const auto *p =
          std::get_if<strategy::TseitinCycleBreakingParameters>(
              &target.sequence[i].parameters);
      if (!p) return false;
      const size_t maximum = static_cast<size_t>(std::min<uint64_t>(
          p->maximum_identity_bytes, std::numeric_limits<size_t>::max() - 1));
      const size_t length = strnlen(identity, maximum + 1);
      if (length > maximum) return false;
      output.emplace_back(identity, length);
    }
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v8(const tactix_online_program_v8 &source,
                       const Kernel &kernel,
                       strategy::OnlineInprocessProgram &target,
                       bool validate = true) {
  if (!online_base_program<tactix_online_program_v7>(
          source, kernel, target, true, online_program_v7))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &input_step = source.steps[i];
    // The inherited v8 ABI slots stay physical, but X17 payloads are retired.
    if (input_step.nary_csp_prefix_instance_count != 0 ||
        input_step.nary_csp_prefix_instances != nullptr)
      return false;
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v9(const tactix_online_program_v9 &source,
                       const Kernel &kernel,
                       strategy::OnlineInprocessProgram &target,
                       bool validate = true) {
  if (!online_base_program<tactix_online_program_v8>(
          source, kernel, target, true, online_program_v8))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &input_step = source.steps[i];
    auto *parameters = std::get_if<strategy::AffineConsequenceParameters>(
        &target.sequence[i].parameters);
    if (input_step.affine_basis_instance_count < 0 ||
        (input_step.affine_basis_instance_count &&
         !input_step.affine_basis_instances) ||
        (input_step.affine_basis_instance_count &&
         (!parameters ||
          parameters->semantics !=
              strategy::AffineConsequenceSemantics::CertifiedBasisV2)) ||
        (parameters &&
         static_cast<uint64_t>(input_step.affine_basis_instance_count) >
             parameters->maximum_session_keys))
      return false;
    auto &output = target.sequence[i].affine_basis_instances;
    output.reserve(static_cast<size_t>(input_step.affine_basis_instance_count));
    for (int row_index = 0;
         row_index < input_step.affine_basis_instance_count; ++row_index) {
      const auto &input = input_step.affine_basis_instances[row_index];
      if (input.kind < TACTIX_ONLINE_AFFINE_ROW_CONTRADICTION ||
          input.kind > TACTIX_ONLINE_AFFINE_ROW_EXTENDED)
        return false;
      const bool extended = input.kind == TACTIX_ONLINE_AFFINE_ROW_EXTENDED;
      if (input.variable_count < 0 ||
          input.source_equation_ordinal_count < 0 ||
          input.source_equation_count < 0 ||
          (input.variable_count && !input.variables) ||
          (input.source_equation_ordinal_count &&
           !input.source_equation_ordinals) ||
          (input.source_equation_count && !input.source_equations) ||
          static_cast<uint64_t>(input.variable_count) >
              (extended ? parameters->basis_maximum_intermediate_width
                        : parameters->basis_maximum_direct_width) ||
          static_cast<uint64_t>(input.source_equation_ordinal_count) >
              parameters->basis_maximum_sources ||
          input.source_equation_count !=
              input.source_equation_ordinal_count)
        return false;
      AffineBasisRowSelection selection;
      selection.owner_version = input.owner_version;
      selection.elimination_profile = input.elimination_profile;
      selection.kind = static_cast<affine_semantic_extension::BasisRowKind>(
          input.kind);
      if (input.rhs != 0 && input.rhs != 1) return false;
      selection.rhs = input.rhs;
      selection.variables.reserve(static_cast<size_t>(input.variable_count));
      for (int j = 0; j < input.variable_count; ++j) {
        const int64_t variable = input.variables[j];
        if (variable <= 0 || variable > INT32_MAX) return false;
        selection.variables.push_back(static_cast<int32_t>(variable));
      }
      selection.source_equation_ordinals.reserve(
          static_cast<size_t>(input.source_equation_ordinal_count));
      for (int j = 0; j < input.source_equation_ordinal_count; ++j) {
        const uint64_t ordinal = input.source_equation_ordinals[j];
        if (ordinal > SIZE_MAX) return false;
        selection.source_equation_ordinals.push_back(
            static_cast<size_t>(ordinal));
      }
      uint64_t source_clauses = 0, source_literals = 0;
      selection.source_equations.reserve(
          static_cast<size_t>(input.source_equation_count));
      for (int equation_index = 0;
           equation_index < input.source_equation_count; ++equation_index) {
        const auto &source = input.source_equations[equation_index];
        if (source.variable_count <= 0 || source.premise_count < 0 ||
            !source.variables || (source.premise_count && !source.premises) ||
            static_cast<uint64_t>(source.variable_count) >
                parameters->maximum_arity ||
            static_cast<uint64_t>(source.premise_count) >
                parameters->maximum_patterns ||
            static_cast<uint64_t>(source.premise_count) >
                parameters->maximum_clauses -
                    std::min(source_clauses, parameters->maximum_clauses) ||
            (source.rhs != 0 && source.rhs != 1))
          return false;
        AffineBasisEquationIdentity equation;
        equation.rhs = source.rhs;
        equation.variables.reserve(
            static_cast<size_t>(source.variable_count));
        for (int j = 0; j < source.variable_count; ++j) {
          const int64_t variable = source.variables[j];
          if (variable <= 0 || variable > INT32_MAX) return false;
          equation.variables.push_back(static_cast<int32_t>(variable));
        }
        source_clauses += static_cast<uint64_t>(source.premise_count);
        equation.premises.reserve(static_cast<size_t>(source.premise_count));
        for (int premise_index = 0; premise_index < source.premise_count;
             ++premise_index) {
          const auto &source_premise = source.premises[premise_index];
          if (source_premise.clause_count < 0 ||
              (source_premise.clause_count && !source_premise.clause) ||
              source_premise.clause_count != source.variable_count ||
              static_cast<uint64_t>(source_premise.clause_count) >
                  parameters->maximum_snapshot_literals -
                      std::min(source_literals,
                               parameters->maximum_snapshot_literals))
            return false;
          AffineBasisSourceIdentity premise;
          premise.native_clause_id = source_premise.native_clause_id;
          premise.clause.reserve(
              static_cast<size_t>(source_premise.clause_count));
          for (int j = 0; j < source_premise.clause_count; ++j) {
            const int64_t literal = source_premise.clause[j];
            if (!literal || literal < INT32_MIN || literal > INT32_MAX)
              return false;
            premise.clause.push_back(static_cast<int32_t>(literal));
          }
          source_literals +=
              static_cast<uint64_t>(source_premise.clause_count);
          equation.premises.push_back(std::move(premise));
        }
        selection.source_equations.push_back(std::move(equation));
      }
      if (!affine_basis_row_selection_within_limits(
              selection, strategy::action_limits(*parameters)))
        return false;
      output.push_back(std::move(selection));
    }
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v10(const tactix_online_program_v10 &source,
                        const Kernel &kernel,
                        strategy::OnlineInprocessProgram &target,
                        bool validate = true) {
  if (!online_base_program<tactix_online_program_v9>(
          source, kernel, target, true, online_program_v9))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &input_step = source.steps[i];
    if (input_step.nary_csp_equality_instance_count < 0 ||
        (input_step.nary_csp_equality_instance_count &&
         !input_step.nary_csp_equality_instances))
      return false;
    auto &output = target.sequence[i].nary_csp_equality_instances;
    output.reserve(
        static_cast<size_t>(input_step.nary_csp_equality_instance_count));
    for (int j = 0; j < input_step.nary_csp_equality_instance_count; ++j) {
      const auto &input = input_step.nary_csp_equality_instances[j];
      if (input.domain_count < 0 ||
          (input.domain_count && !input.domains))
        return false;
      NaryCspEqualityActionSelection selection;
      selection.domains.reserve(static_cast<size_t>(input.domain_count));
      for (int domain = 0; domain < input.domain_count; ++domain) {
        const auto &source_domain = input.domains[domain];
        if (source_domain.value_count < 0 ||
            (source_domain.value_count && !source_domain.values))
          return false;
        std::vector<int32_t> values;
        values.reserve(static_cast<size_t>(source_domain.value_count));
        for (int value = 0; value < source_domain.value_count; ++value) {
          const int64_t literal = source_domain.values[value];
          if (literal < INT32_MIN || literal > INT32_MAX) return false;
          values.push_back(static_cast<int32_t>(literal));
        }
        selection.domains.push_back(std::move(values));
      }
      selection.coordinates = {input.left_domain, input.right_domain,
                               input.residual_domain};
      if (!nary_csp_equality_action_selection_valid(selection)) return false;
      output.push_back(std::move(selection));
    }
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v11(const tactix_online_program_v11 &source,
                        const Kernel &kernel,
                        strategy::OnlineInprocessProgram &target,
                        bool validate = true) {
  if (!online_base_program<tactix_online_program_v10>(
          source, kernel, target, true, online_program_v10))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &input_step = source.steps[i];
    if (input_step.gf3_general_row_instance_count < 0 ||
        (input_step.gf3_general_row_instance_count &&
         !input_step.gf3_general_row_instances))
      return false;
    auto &output = target.sequence[i].gf3_general_row_instances;
    output.reserve(
        static_cast<size_t>(input_step.gf3_general_row_instance_count));
    for (int j = 0; j < input_step.gf3_general_row_instance_count; ++j) {
      const auto &input = input_step.gf3_general_row_instances[j];
      if (!input.identity) return false;
      const auto *parameters =
          std::get_if<strategy::Gf3GeneralRowConsequenceParameters>(
              &target.sequence[i].parameters);
      if (!parameters) return false;
      const size_t maximum = static_cast<size_t>(std::min<uint64_t>(
          parameters->gaussian.maximum_identity_bytes,
          std::numeric_limits<size_t>::max() - 1));
      const size_t length = strnlen(input.identity, maximum + 1);
      if (length > maximum) return false;
      Gf3GeneralRowActionSelection selection{
          input.outcome, input.tuple,
          std::string(input.identity, length)};
      if (!gf3_general_row_action_selection_valid(selection)) return false;
      output.push_back(std::move(selection));
    }
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v12(const tactix_online_program_v12 &source,
                        const Kernel &kernel,
                        strategy::OnlineInprocessProgram &target,
                        bool validate = true) {
  if (!online_base_program<tactix_online_program_v11>(
          source, kernel, target, true, online_program_v11))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &input_step = source.steps[i];
    if (input_step.nary_csp_equality_amo_instance_count < 0 ||
        (input_step.nary_csp_equality_amo_instance_count &&
         !input_step.nary_csp_equality_amo_instances))
      return false;
    auto &output = target.sequence[i].nary_csp_equality_amo_instances;
    for (int j = 0; j < input_step.nary_csp_equality_amo_instance_count; ++j) {
      const auto &input = input_step.nary_csp_equality_amo_instances[j];
      if (input.domain_count < 0 ||
          (input.domain_count && !input.domains))
        return false;
      NaryCspEqualityAmoSelection selection;
      for (int domain = 0; domain < input.domain_count; ++domain) {
        const auto &source_domain = input.domains[domain];
        if (source_domain.value_count < 0 ||
            (source_domain.value_count && !source_domain.values))
          return false;
        std::vector<int32_t> values;
        for (int value = 0; value < source_domain.value_count; ++value) {
          const int64_t literal = source_domain.values[value];
          if (literal < INT32_MIN || literal > INT32_MAX) return false;
          values.push_back(static_cast<int32_t>(literal));
        }
        selection.domains.push_back(std::move(values));
      }
      if (!nary_csp_equality_amo_selection_valid(selection)) return false;
      output.push_back(std::move(selection));
    }
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v13(const tactix_online_program_v13 &source,
                        const Kernel &kernel,
                        strategy::OnlineInprocessProgram &target,
                        bool validate = true) {
  if (!online_base_program<tactix_online_program_v12>(
          source, kernel, target, false, online_program_v12))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &profile = source.steps[i].walk_profile;
    if (!profile.objective && !profile.admission && !profile.preparation_budget)
      continue;
    auto *walk = std::get_if<strategy::WalkParameters>(&target.sequence[i].parameters);
    if (!walk || target.sequence[i].rule != "Walk") return false;
    walk->objective = static_cast<strategy::WalkParameters::Objective>(profile.objective);
    walk->admission = static_cast<strategy::WalkParameters::Admission>(profile.admission);
    walk->preparation_budget = profile.preparation_budget;
    if (!walk->valid()) return false;
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v14(const tactix_online_program_v14 &source,
                        const Kernel &kernel,
                        strategy::OnlineInprocessProgram &target,
                        bool validate = true) {
  if (!online_base_program<tactix_online_program_v13>(
          source, kernel, target, false, online_program_v13))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &input = source.steps[i];
    auto &step = target.sequence[i];
    if (input.direct_csp_profile == TACTIX_DIRECT_CSP_PROFILE_LEGACY) {
      if (input.guarded_cover_instance_count || input.guarded_cover_instances) return false;
      const auto &p = input.guarded_cover_parameters;
      if (p.maximum_source_clauses || p.maximum_source_literals || p.maximum_recognition_work || p.maximum_domains || p.maximum_domain_values || p.maximum_cover_literals || p.maximum_or_definitions || p.maximum_or_inputs || p.maximum_output_literals || p.maximum_proof_work || p.maximum_proof_additions || p.maximum_proof_literal_occurrences || p.maximum_journal_bytes || p.maximum_external_extent || p.maximum_session_keys || p.maximum_session_work) return false;
      continue;
    }
    if (input.direct_csp_profile != TACTIX_DIRECT_CSP_PROFILE_GUARDED_COVER_V2 ||
        step.rule != "DirectCspSupport" ||
        !std::holds_alternative<std::monostate>(step.parameters) ||
        !step.direct_csp_support_instances.empty()) return false;
    strategy::DirectCspGuardedCoverParameters p;
    p.maximum_source_clauses = input.guarded_cover_parameters.maximum_source_clauses;
    p.maximum_source_literals = input.guarded_cover_parameters.maximum_source_literals;
    p.maximum_recognition_work = input.guarded_cover_parameters.maximum_recognition_work;
    p.maximum_domains = input.guarded_cover_parameters.maximum_domains;
    p.maximum_domain_values = input.guarded_cover_parameters.maximum_domain_values;
    p.maximum_cover_literals = input.guarded_cover_parameters.maximum_cover_literals;
    p.maximum_or_definitions = input.guarded_cover_parameters.maximum_or_definitions;
    p.maximum_or_inputs = input.guarded_cover_parameters.maximum_or_inputs;
    p.maximum_output_literals = input.guarded_cover_parameters.maximum_output_literals;
    p.maximum_proof_work = input.guarded_cover_parameters.maximum_proof_work;
    p.maximum_proof_additions = input.guarded_cover_parameters.maximum_proof_additions;
    p.maximum_proof_literal_occurrences = input.guarded_cover_parameters.maximum_proof_literal_occurrences;
    p.maximum_journal_bytes = input.guarded_cover_parameters.maximum_journal_bytes;
    p.maximum_external_extent = input.guarded_cover_parameters.maximum_external_extent;
    p.maximum_session_keys = input.guarded_cover_parameters.maximum_session_keys;
    p.maximum_session_work = input.guarded_cover_parameters.maximum_session_work;
    if (!p.valid() || input.guarded_cover_instance_count <= 0 ||
        !input.guarded_cover_instances ||
        static_cast<uint64_t>(input.guarded_cover_instance_count) > p.maximum_session_keys)
      return false;
    const auto copy_literals = [](const tactix_direct_csp_literal_array &src,
                                  uint32_t maximum, std::vector<int32_t> &dst) {
      if (src.count < 0 || static_cast<uint64_t>(src.count) > maximum ||
          (src.count && !src.literals)) return false;
      for (int j = 0; j < src.count; ++j) {
        if (!src.literals[j] || src.literals[j] == INT32_MIN) return false;
        dst.push_back(src.literals[j]);
      }
      return true;
    };
    for (int j = 0; j < input.guarded_cover_instance_count; ++j) {
      const auto &src = input.guarded_cover_instances[j];
      if (src.domain_count <= 0 || static_cast<uint64_t>(src.domain_count) > p.maximum_domains ||
          !src.domains || !src.selected_values || src.exact_or_count < 0 ||
          static_cast<uint64_t>(src.exact_or_count) > p.maximum_or_definitions ||
          (src.exact_or_count && !src.exact_or) || src.blocker_count < 0 ||
          static_cast<uint64_t>(src.blocker_count) > p.maximum_cover_literals ||
          (src.blocker_count && !src.blockers)) return false;
      DirectCspGuardedCoverActionSelection dst;
      for (int d = 0; d < src.domain_count; ++d) {
        dst.domains.emplace_back();
        if (!copy_literals(src.domains[d], p.maximum_domain_values, dst.domains.back()) ||
            dst.domains.back().size() < 2 || src.selected_values[d] >= dst.domains.back().size())
          return false;
        dst.selected_values.push_back(src.selected_values[d]);
      }
      if (!copy_literals(src.cover, p.maximum_cover_literals, dst.cover)) return false;
      for (int o = 0; o < src.exact_or_count; ++o) {
        dst.exact_or.push_back({src.exact_or[o].output, {}});
        if (!src.exact_or[o].output || src.exact_or[o].output == INT32_MIN ||
            !copy_literals(src.exact_or[o].inputs, p.maximum_or_inputs, dst.exact_or.back().inputs))
          return false;
      }
      for (int b = 0; b < src.blocker_count; ++b) {
        const auto &v = src.blockers[b];
        dst.blockers.push_back({v.literal, v.domain, v.value, v.or_definition});
      }
      step.direct_csp_guarded_cover_instances.push_back(std::move(dst));
    }
    step.parameters = p;
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v15(const tactix_online_program_v15 &source,
                        const Kernel &kernel, strategy::OnlineInprocessProgram &target, bool validate = true) {
  if (!online_base_program<tactix_online_program_v14>(
          source, kernel, target, false, online_program_v14))
    return false;
  for(int i=0;i<source.step_count;++i) {
    const auto &input=source.steps[i]; auto &step=target.sequence[i];
    const auto &srcp=input.equality_transitivity_parameters;
    if(input.equality_transitivity_profile==TACTIX_EQUALITY_TRANSITIVITY_PROFILE_LEGACY) {
      if(input.equality_transitivity_instances || input.equality_transitivity_instance_count ||
         srcp.maximum_source_clauses || srcp.maximum_source_literals || srcp.maximum_recognition_work || srcp.maximum_planning_work || srcp.maximum_domain_values || srcp.maximum_forbidden_tuples || srcp.maximum_proof_work || srcp.maximum_proof_additions || srcp.maximum_proof_literal_occurrences || srcp.maximum_journal_bytes || srcp.maximum_external_extent || srcp.maximum_session_keys || srcp.maximum_session_bytes || srcp.maximum_arena_words || srcp.maximum_watch_words || srcp.maximum_preparation_bytes) return false;
      continue;
    }
    if(input.equality_transitivity_profile!=TACTIX_EQUALITY_TRANSITIVITY_PROFILE_EXACT ||
       step.rule!="NaryCspEqualityTransitivity" || !std::holds_alternative<std::monostate>(step.parameters)) return false;
    strategy::NaryCspEqualityTransitivityParameters p;
    p.maximum_source_clauses=srcp.maximum_source_clauses;
    p.maximum_source_literals=srcp.maximum_source_literals;
    p.maximum_recognition_work=srcp.maximum_recognition_work;
    p.maximum_planning_work=srcp.maximum_planning_work;
    p.maximum_domain_values=srcp.maximum_domain_values;
    p.maximum_forbidden_tuples=srcp.maximum_forbidden_tuples;
    p.maximum_proof_work=srcp.maximum_proof_work;
    p.maximum_proof_additions=srcp.maximum_proof_additions;
    p.maximum_proof_literal_occurrences=srcp.maximum_proof_literal_occurrences;
    p.maximum_journal_bytes=srcp.maximum_journal_bytes;
    p.maximum_external_extent=srcp.maximum_external_extent;
    p.maximum_session_keys=srcp.maximum_session_keys;
    p.maximum_session_bytes=srcp.maximum_session_bytes;
    p.maximum_arena_words=srcp.maximum_arena_words;
    p.maximum_watch_words=srcp.maximum_watch_words;
    p.maximum_preparation_bytes=srcp.maximum_preparation_bytes;

    if(!p.valid() || input.equality_transitivity_instance_count<=0 || !input.equality_transitivity_instances ||
       uint64_t(input.equality_transitivity_instance_count)>p.maximum_session_keys) return false;
    const auto copy_domain=[&](const tactix_online_nary_csp_domain &d, std::vector<int32_t> &out) {
      if(d.value_count<2 || uint64_t(d.value_count)>p.maximum_domain_values || !d.values) return false;
      for(int j=0;j<d.value_count;++j) {
        const int64_t value=d.values[j];
        if(!value || value==INT32_MIN || uint64_t(value<0?-value:value)>=p.maximum_external_extent) return false;
        out.push_back(int32_t(value));
      }
      return true;
    };
    for(int j=0;j<input.equality_transitivity_instance_count;++j) {
      const auto &input_instance=input.equality_transitivity_instances[j];
      NaryCspEqualityTransitivitySelection choice;
      for(size_t d=0;d<3;++d) {
        if(!copy_domain(input_instance.domains[d],choice.domains[d])) return false;
        const auto &e=input_instance.equalities[d];
        if(e.domain_count!=3 || !e.domains) return false;
        auto &key=choice.equalities[d];
        key.first_equal_domain=e.left_domain; key.second_equal_domain=e.right_domain; key.residual_domain=e.residual_domain;
        for(size_t owner_domain=0;owner_domain<3;++owner_domain) {
          key.domains.emplace_back();
          if(!copy_domain(e.domains[owner_domain],key.domains.back())) return false;
        }
        choice.equality_variables[d]=input_instance.equality_variables[d];
        if(!choice.equality_variables[d] || choice.equality_variables[d]>=p.maximum_external_extent) return false;
      }
      if(!nary_csp_equality_transitivity_selection_valid(choice)) return false;
      step.nary_csp_equality_transitivity_instances.push_back(std::move(choice));
    }
    step.parameters=p;
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v16(const tactix_online_program_v16 &source,
                        const Kernel &kernel, strategy::OnlineInprocessProgram &target,
                        bool validate = true) {
  if (!online_base_program<tactix_online_program_v15>(
          source, kernel, target, false, online_program_v15))
    return false;
  for (int i=0;i<source.step_count;++i) {
    const auto &input=source.steps[i]; const auto &p=input.affine_matrix;
    auto &step=target.sequence[i];
    tactix_online_parameters encoded{};
    encoded.kind=TACTIX_ONLINE_PARAMETERS_AFFINE_CONSEQUENCE;
    encoded.value.affine_consequence=p.basis;
    if (input.affine_matrix_profile==TACTIX_AFFINE_MATRIX_PROFILE_LEGACY) {
      // Require a truly empty extension. Validate fields, not struct padding.
      if (p.admission || p.operation || p.maximum_retained_bytes ||
          p.maximum_deferred_deletions || p.maximum_deferred_literal_work) return false;
      const auto &b=p.basis;
      if (b.maximum_clauses || b.maximum_snapshot_literals || b.maximum_snapshot_work ||
          b.maximum_groups || b.maximum_patterns || b.maximum_elimination_work || b.maximum_arity ||
          b.maximum_intermediate_width || b.maximum_sources || b.maximum_output_width ||
          b.maximum_proof_additions || b.maximum_proof_deletions || b.maximum_journal_bytes ||
          b.consequence || b.semantics || b.basis_maximum_work || b.basis_maximum_intermediate_width ||
          b.basis_maximum_sources || b.basis_maximum_premises || b.basis_maximum_nodes ||
          b.basis_maximum_direct_width || b.maximum_external_extent || b.maximum_session_keys || b.maximum_session_work)
        return false;
      continue;
    }
    if (input.affine_matrix_profile!=TACTIX_AFFINE_MATRIX_PROFILE_PERSISTENT_V1 ||
        step.rule!="AffineMatrixInitialize" || !std::holds_alternative<std::monostate>(step.parameters)) return false;
    strategy::InprocessParameters converted;
    if (!online_parameters(encoded,converted)) return false;
    const auto *basis=std::get_if<strategy::AffineConsequenceParameters>(&converted);
    if (!basis || p.admission<1 || p.admission>2 || p.operation<1 || p.operation>10) return false;
    strategy::AffineMatrixInitializeParameters out;
    out.basis=*basis;
    out.admission=static_cast<strategy::AffineMatrixInitializeParameters::Admission>(p.admission);
    out.operation=static_cast<strategy::AffineMatrixInitializeParameters::Operation>(p.operation);
    out.maximum_retained_bytes=p.maximum_retained_bytes;
    out.maximum_deferred_deletions=p.maximum_deferred_deletions;
    out.maximum_deferred_literal_work=p.maximum_deferred_literal_work;
    if (!out.valid()) return false;
    step.parameters=out;
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v17(const tactix_online_program_v17 &source,
                        const Kernel &kernel,
                        strategy::OnlineInprocessProgram &target) {
  if (!online_base_program<tactix_online_program_v16>(
          source, kernel, target, true, online_program_v16))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &input = source.steps[i];
    const auto &p = input.cardinality_search;
    auto &step = target.sequence[i];
    if (step.rule != "CardinalitySearch") {
      if (input.cardinality_search_instance_count ||
          input.cardinality_search_instances ||
          p.maximum_support_variables || p.maximum_constraints ||
          p.maximum_source_clauses || p.maximum_reason_width ||
          p.maximum_inspection_work || p.maximum_resolution_work ||
          p.maximum_proof_additions || p.maximum_proof_literals ||
          p.maximum_source_scan_work || p.maximum_arena_words ||
          p.maximum_journal_bytes)
        return false;
      continue;
    }
    if (!std::holds_alternative<std::monostate>(step.parameters) ||
        input.cardinality_search_instance_count != 1 ||
        !input.cardinality_search_instances)
      return false;
    CardinalitySearchActionLimits limits;
    limits.inspection = {p.maximum_support_variables, p.maximum_constraints,
                         p.maximum_source_clauses, p.maximum_reason_width,
                         p.maximum_inspection_work};
    limits.resolution = {p.maximum_resolution_work,
                         p.maximum_proof_additions, p.maximum_proof_literals};
    limits.maximum_source_scan_work = p.maximum_source_scan_work;
    limits.maximum_arena_words = p.maximum_arena_words;
    limits.maximum_journal_bytes = p.maximum_journal_bytes;
    if (!limits.valid()) return false;
    const auto &instance = input.cardinality_search_instances[0];
    if (instance.bound_count <= 0 ||
        static_cast<uint32_t>(instance.bound_count) >
            limits.inspection.maximum_constraints ||
        !instance.bounds)
      return false;
    std::vector<CardinalitySearchBound> bounds;
    bounds.reserve(static_cast<size_t>(instance.bound_count));
    for (int b = 0; b < instance.bound_count; ++b) {
      const auto &source_bound = instance.bounds[b];
      if (source_bound.literal_count <= 0 ||
          static_cast<uint32_t>(source_bound.literal_count) >
              limits.inspection.maximum_support_variables ||
          !source_bound.literals)
        return false;
      CardinalitySearchBound bound;
      bound.lower = source_bound.lower;
      bound.upper = source_bound.upper;
      bound.literals.reserve(static_cast<size_t>(source_bound.literal_count));
      for (int j = 0; j < source_bound.literal_count; ++j) {
        const int64_t value = source_bound.literals[j];
        if (value <= INT32_MIN || value > INT32_MAX || !value)
          return false;
        bound.literals.push_back(static_cast<int32_t>(value));
      }
      bounds.push_back(std::move(bound));
    }
    step.parameters = limits;
    step.cardinality_search_instances.push_back(std::move(bounds));
  }
  return strategy::validate_online_strategy_program(target);
}


void online_result(const strategy::InvocationResult &source,
                   tactix_online_result &target) {
  target.status = static_cast<int>(source.status);
  target.rule_result = static_cast<int>(source.rule_result);
  target.budget_status = static_cast<int>(source.budget_status);
  target.work_unit = static_cast<int>(source.work_unit);
  target.work = source.work;
  target.applications = source.applications;
  target.step_id = source.step_id;
  target.rule_id = source.rule_id;
  target.instance = source.instance;
  target.made_rule_progress = source.made_rule_progress;
  target.selection_complete = source.selection_complete;
  target.before_arena_generation = source.before.arena_generation;
  target.before_touch_epoch = source.before.touch_epoch;
  target.after_arena_generation = source.after.arena_generation;
  target.after_touch_epoch = source.after.touch_epoch;
  target.before_strategy_epoch = source.before.strategy_epoch;
  target.after_strategy_epoch = source.after.strategy_epoch;
  target.configuration_revision = source.configuration_revision;
}

template <class Source, class Convert>
int configure_online_program(tactix_solver *solver, const Source *source,
                             uint64_t revision, int reconfigure_mode,
                             Convert &&convert) {
  if (!solver || !source ||
      (reconfigure_mode != TACTIX_ONLINE_RECONFIGURE_RESET &&
       reconfigure_mode != TACTIX_ONLINE_RECONFIGURE_RETAIN_BUDGETS))
    return -1;
  strategy::OnlineInprocessProgram program;
  if (!convert(*source, solver->kernel, program))
    return -1;
  std::unique_ptr<strategy::OnlineInprocessExecutor> replacement;
  strategy::OnlineInprocessExecutor *executor = solver->online.get();
  if (!executor) {
    replacement = std::make_unique<strategy::OnlineInprocessExecutor>();
    executor = replacement.get();
  }
  const strategy::ReconfigureMode mode =
      reconfigure_mode == TACTIX_ONLINE_RECONFIGURE_RETAIN_BUDGETS
          ? strategy::ReconfigureMode::RetainMatchingStepBudgets
          : strategy::ReconfigureMode::ResetRuntime;
  if (!executor->configure(program, revision, mode))
    return -1;
  if (replacement)
    solver->online = std::move(replacement);
  // A rejected request must not close the C input stream. Conversion and
  // executor validation only read the stable external-to-internal map;
  // finish_input retires unused variables but does not remap those literals.
  input_complete(solver);
  return 0;
}

} // namespace

extern "C" {

tactix_solver *tactix_init(void) { return new tactix_solver(); }

void tactix_release(tactix_solver *s) { delete s; }
int tactix_bind_structural_provenance(tactix_solver *s, FILE *stream) {
  if (!s || (stream && std::ferror(stream))) return -1;
  s->kernel.structural_provenance = stream;
  external_state_change(s);
  return 0;
}

// The CLI's ingestion into a fresh session. parse_dimacs calls finish_input,
// which must run exactly once, so the session's own boundary is marked closed.
int tactix_read_dimacs(tactix_solver *s, FILE *in) {
  if (!s || !in || s->input_completed || !s->scratch.empty() ||
      s->kernel.nvars || s->kernel.next_clause_id)
    return -1;
  const char *err = parse_dimacs(in, s->kernel);
  // LRAT selected before file ingestion is deliberately deferred until all
  // original clause ids exist. parse_dimacs has now crossed that boundary, so
  // attach the replay before marking this API session complete.
  if (!err && s->pending_input_proof) {
    attach_replayed_input_proof(s->kernel, s->proof);
    s->pending_input_proof = false;
  }
  s->input_completed = true;
  external_state_change(s);
  return err ? -1 : 0;
}

int tactix_add_checked(tactix_solver *s, int lit) {
  // finish_input retires absent variables and fixes proof input ids. Until O7
  // supplies a full incremental lifecycle, accepting a later clause would
  // leave both the live population and the certificate input out of sync.
  if (s->input_completed)
    return -1;
  if (lit == 0) {
    ingest_clause(s->kernel, s->scratch);
    s->scratch.clear();
  } else {
    s->scratch.push_back(internalize(lit));
  }
  external_state_change(s);
  return 0;
}

void tactix_add(tactix_solver *s, int lit) {
  if (tactix_add_checked(s, lit) != 0)
    std::abort();
}

// The C API has no end of file, so "the input is complete" is the first call
// that reasons about it. finish_input settles the proof id space (see
// frontend/parse.hpp), so every entry point below that could consume a derived
// id goes through here first. Idempotent, and free once nothing is pending.
// Adding clauses after this point is outside the v1 contract: retired
// variables are no longer in the active search population, and with a proof
// attached their ids would also land above already-derived ones.
static void input_complete(tactix_solver *s) {
  if (s->input_completed)
    return;
  if (s->pending_input_proof) {
    attach_replayed_input_proof(s->kernel, s->proof);
    s->pending_input_proof = false;
  }
  if (s->kernel.proof && s->kernel.proof->streaming_pbp() &&
      s->kernel.binary_ids.tracks_values())
    s->kernel.proof->declare_exact_binary_ids();
  finish_input(s->kernel);
  s->input_completed = true;
}

int tactix_proof(tactix_solver *s, const char *path, int format) {
  if (s->input_completed || s->kernel.proof || s->pending_input_proof)
    return -1;
  // Public values are append-only; see TACTIX_PROOF_* in tactix.h.
  ProofFormat f;
  switch (format) {
  case 0:
    f = ProofFormat::DratBinary;
    break;
  case 1:
    f = ProofFormat::DratAscii;
    break;
  case 2:
    f = ProofFormat::Lrat;
    break;
  case 3:
    f = ProofFormat::DsrBinary;
    break;
  case 4:
    f = ProofFormat::DsrAscii;
    break;
  case 5:
    return -1; // retired proof-format value; public values are never reused
  case 6:
    f = ProofFormat::PbpStreaming;
    break;
  default:
    return -1;
  }
  if (!s->proof.open(path, f))
    return -1;
  // Native LRAT cannot derive root units until all checker input ids exist.
  // Import eagerly in the live kernel, then replay its exact raw clauses at
  // the first reasoning boundary, whether proof was requested early or late.
  if (s->kernel.next_clause_id || f == ProofFormat::Lrat) {
    if (f == ProofFormat::Lrat)
      s->kernel.replay_lrat_input = true;
    s->pending_input_proof = true;
    external_state_change(s);
    return 0;
  }
  s->kernel.proof = &s->proof;
  if (f != ProofFormat::PbpStreaming)
    s->kernel.close_startup_certificate();
  s->proof.bind_variable_map(&s->kernel.internal_to_external);
  s->proof.bind_external_variable_map(&s->kernel.external_to_internal);
  external_state_change(s);
  return 0;
}

int tactix_set_default_option(tactix_solver *s, const char *name, const char *value) {
  if (!s || !name || std::strncmp(name, "TACTIX_", 7) != 0) return -1;
  auto &options = s->default_options;
  for (auto it = options.begin(); it != options.end(); ++it)
    if (it->first == name) {
      if (value) it->second = value;
      else options.erase(it);
      return 0;
    }
  if (value) options.emplace_back(name, value);
  return 0;
}

void tactix_set_bve_gates(tactix_solver *s, unsigned mask) {
  s->kernel.bve_gate_mask = mask;
  external_state_change(s);
}

// Kernel configuration setters — the §5.6 pose surface. Plain writes to config
// fields (kernel.hpp documents each field's soundness class); no scheduling
// state, no guard interaction.
void tactix_set_bve_bound(tactix_solver *s, unsigned bound) {
  s->kernel.bve_bound = bound;
  external_state_change(s);
}
void tactix_set_factor_bound(tactix_solver *s, unsigned bound) {
  s->kernel.factor_bound = bound;
  external_state_change(s);
}
void tactix_set_factor_composite(tactix_solver *s, int on) {
  s->kernel.factor_composite = on != 0;
  external_state_change(s);
}
void tactix_set_work_budget(tactix_solver *s, uint64_t budget) {
  s->kernel.work_budget = budget;
  external_state_change(s);
}
void tactix_set_subsume_limits(tactix_solver *s, uint32_t clause_size,
                               uint32_t large_occurrences,
                               uint32_t binary_occurrences) {
  s->kernel.subsume_forward_parameters = {
      clause_size, large_occurrences, binary_occurrences};
  external_state_change(s);
}
void tactix_set_strengthen_limits(tactix_solver *s, uint32_t clause_size,
                                  uint32_t occurrences) {
  s->kernel.strengthen_parameters = {clause_size, occurrences};
  external_state_change(s);
}
void tactix_set_analyze_minimize_depth(tactix_solver *s, uint32_t depth) {
  s->kernel.analyze_parameters.minimize_depth_limit = depth;
  external_state_change(s);
}
int tactix_set_congruence_limits(tactix_solver *s, uint32_t and_arity,
                                 uint64_t gate_count, uint32_t xor_arity) {
  strategy::CongruenceParameters parameters{
      and_arity, static_cast<size_t>(gate_count), xor_arity,
      s->kernel.congruence_parameters.binary_watch_order};
  if (uint64_t{parameters.gate_count_limit} != gate_count ||
      !parameters.valid())
    return -1;
  s->kernel.congruence_parameters = parameters;
  external_state_change(s);
  return 0;
}
int tactix_set_congruence_watch_order(tactix_solver *s, int order) {
  if (order < TACTIX_CONGRUENCE_CANONICAL_WATCH_ORDER ||
      order > TACTIX_CONGRUENCE_PRESERVE_WATCH_ORDER)
    return -1;
  s->kernel.congruence_parameters.binary_watch_order =
      static_cast<strategy::CongruenceBinaryWatchOrder>(order);
  external_state_change(s);
  return 0;
}
void tactix_set_instantiate_limits(tactix_solver *s,
                                   uint32_t minimum_clause_size,
                                   uint32_t occurrence_limit,
                                   uint32_t minimum_unassigned,
                                   int include_redundant) {
  s->kernel.instantiate_parameters.minimum_clause_size = minimum_clause_size;
  s->kernel.instantiate_parameters.occurrence_limit = occurrence_limit;
  s->kernel.instantiate_parameters.minimum_unassigned = minimum_unassigned;
  s->kernel.instantiate_parameters.include_redundant = include_redundant != 0;
  external_state_change(s);
}
void tactix_set_instantiate_material_threshold(tactix_solver *s,
                                               uint32_t threshold) {
  s->kernel.instantiate_parameters.new_material_threshold = threshold;
  external_state_change(s);
}
void tactix_set_backbone_round_limits(tactix_solver *s,
                                      uint32_t initial_rounds,
                                      uint32_t maximum_rounds) {
  s->kernel.backbone_parameters = {initial_rounds, maximum_rounds};
  external_state_change(s);
}
// Every band Vivify implements is settable, and a clamp here would be an N1
// breach rather than input validation: substituting a different band makes
// scopes the policy still uses unreachable through the API, so the guard says
// applicable while apply quietly declines. Out-of-range leaves the pose
// untouched (tests/test_vivify_n1.cpp).
void tactix_set_vivify_scope(tactix_solver *s, unsigned scope) {
  if (scope <= 5) {
    s->kernel.vivify_scope = static_cast<uint8_t>(scope);
    external_state_change(s);
  }
}

int tactix_set_vivify_parameters(tactix_solver *s, unsigned scope, int reuse,
                                  int flush,
                                  uint32_t irredundant_sort_ratio) {
  strategy::VivifyParameters parameters{
      static_cast<uint8_t>(scope), reuse != 0, flush != 0,
      irredundant_sort_ratio};
  if (scope > 5 || !parameters.valid())
    return -1;
  s->kernel.vivify_scope = parameters.scope;
  s->kernel.vivify_reuse = parameters.reuse;
  s->kernel.vivify_flush = parameters.flush;
  s->kernel.vivify_irredundant_sort_ratio =
      parameters.irredundant_sort_ratio;
  external_state_change(s);
  return 0;
}

namespace {
int leave_phase(tactix_solver *s);
}

int tactix_solve(tactix_solver *s) {
  input_complete(s);
  // Search runs outside the inprocessing phase bracket.
  if (s->phase.open()) {
    if (s->phase.occurrences_open()) (void)leave_phase(s);
    else s->phase.exit_terminal(s->kernel);
  }
  int r;
  {
    ScopedStrategySettings settings(&s->default_options);
    r = default_solve(s->kernel);
  }
  rule_journal_flush();
  external_state_change(s);
  if (r == 10)
    // A public solver can be solved again. Replay of ρ belongs in the
    // external model cache only: writing its repaired values into the live
    // assignment would desynchronize values from trail/reasons and make the
    // next solve start from a state that no longer models the live clauses.
    prepare_external_model(s);
  if (r == 20)
    s->proof.flush();
  return r;
}

int tactix_value(tactix_solver *s, int elit) {
  // Direct rule applications can report SAT without an intervening status
  // query. Preserve partial-value reads outside a terminal state.
  if (s->input_completed && has_sat_assignment(s->kernel))
    prepare_external_model(s);
  const Lit external = internalize(elit);
  if (var_of(external) >= kernel_num_output_vars(s->kernel))
    return -elit; // unmentioned variable: any value works
  const int8_t v = s->kernel.model_extended &&
                          s->model_strategy_epoch == s->kernel.strategy_epoch
                       ? s->kernel.model_values[external]
                       : kernel_external_value(s->kernel, external);
  return v > 0 ? elit : -elit;
}

int tactix_rule_count(void) {
  return static_cast<int>(tactix_rules().size());
}

const char *tactix_rule_name(int rule_id) {
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()))
    return nullptr;
  return rules[rule_id].name;
}

int tactix_action_kind(int rule_id) {
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()) ||
      rules[rule_id].retired)
    return -1;
  return static_cast<int>(rules[rule_id].kind);
}

int tactix_rules_applicable(tactix_solver *s, int *out, int cap) {
  input_complete(s);
  const auto &rules = tactix_rules();
  int n = 0;
  for (size_t i = 0; i < rules.size() && n < cap; i++)
    if (rule_guard(rules[i], s->kernel))
      out[n++] = static_cast<int>(i);
  return n;
}

static int result_code(tactix_solver *solver, ApplyResult res) {
  switch (res) {
  case ApplyResult::Ok:
    return 0;
  case ApplyResult::NoOp:
    return 1;
  case ApplyResult::Conflict:
    return 2;
  case ApplyResult::Sat:
    prepare_external_model(solver);
    return 10;
  case ApplyResult::Unsat:
    solver->proof.flush();
    return 20;
  }
  return 1;
}

int tactix_apply(tactix_solver *s, int rule_id) {
  input_complete(s);
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()) ||
      rules[rule_id].retired)
    return -1;
  const RuleVTable &r = rules[rule_id];
  if (!rule_guard(r, s->kernel))
    return -1;
  const ApplyResult res =
      apply_published(s, [&] { return apply_rule(r, s->kernel); });
  kernel_check_invariants(s->kernel); // no-op in NDEBUG
  return result_code(s, res);
}

int tactix_rule_instances(tactix_solver *s, int rule_id, uint64_t *out,
                          int cap) {
  input_complete(s);
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()) ||
      rules[rule_id].retired || cap < 0)
    return -1;
  const RuleVTable &r = rules[rule_id];
  if (!rule_guard(r, s->kernel))
    return -1;
  if (!r.instances) { // no uint64 choice; X5 has a separate structured tuple
    if (cap >= 1)
      out[0] = INSTANCE_CANONICAL;
    return cap >= 1 ? 1 : 0;
  }
  return static_cast<int>(
      r.instances(s->kernel, out, static_cast<uint32_t>(cap)));
}

int tactix_apply_instance(tactix_solver *s, int rule_id, uint64_t instance) {
  input_complete(s);
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()) ||
      rules[rule_id].retired)
    return -1;
  const RuleVTable &r = rules[rule_id];
  if (!rule_guard(r, s->kernel))
    return -1;
  // A rule with no uint64 instance surface rejects a non-canonical payload.
  // Running the canonical round instead would be payload aliasing: the caller
  // asked for one instance and silently got another action, with no way to notice
  // (tests/test_api_instances.c).
  if (!r.apply_instance && instance != INSTANCE_CANONICAL)
    return -1;
  const ApplyResult res = apply_published(s, [&] {
    if (instance == INSTANCE_CANONICAL || !r.apply_instance)
      return apply_rule(r, s->kernel); // canonical (the only instance if no slot)
    return apply_rule_instance(r, s->kernel, instance);
  });
  kernel_check_invariants(s->kernel); // no-op in NDEBUG
  return result_code(s, res);
}

int tactix_apply_condition(tactix_solver *s, const int *clause,
                           int clause_count, const int *witness,
                           int witness_count) {
  input_complete(s);
  if (clause_count < 0 || witness_count < 0 ||
      (clause_count && !clause) || (witness_count && !witness))
    return -1;
  ConditionInstance instance;
  instance.clause.reserve(static_cast<size_t>(clause_count));
  instance.witness.reserve(static_cast<size_t>(witness_count));
  const auto append = [&](int external, std::vector<Lit> &destination) {
    if (!external || external == INT32_MIN)
      return false;
    const Lit lit = kernel_internalize(s->kernel, external);
    if (lit == INVALID_LIT)
      return false;
    destination.push_back(lit);
    return true;
  };
  for (int i = 0; i < clause_count; ++i)
    if (!append(clause[i], instance.clause))
      return -1;
  for (int i = 0; i < witness_count; ++i) {
    const int external = witness[i];
    if (!external || external == INT32_MIN)
      return -1;
    instance.witness.push_back(internalize(external));
  }
  const RuleVTable &rule = tactix_rules()[tactix_rule_id("Condition")];
  ConditionPrepared prepared;
  const uint64_t validation_started = now_nanos();
  const bool applicable = condition_prepare(s->kernel, instance, prepared);
  if (rule.id < Stats::MAX_TIMED_RULES)
    s->kernel.stats.rule_nanos[rule.id] += now_nanos() - validation_started;
  if (!applicable)
    return -1;
  std::string candidate = "clause=";
  for (int i = 0; i < clause_count; ++i) {
    if (i)
      candidate.push_back(',');
    candidate += std::to_string(clause[i]);
  }
  candidate += ";witness=";
  for (int i = 0; i < witness_count; ++i) {
    if (i)
      candidate.push_back(',');
    candidate += std::to_string(witness[i]);
  }
  const ApplyResult result = apply_published(s, [&] {
    return apply_rule_structured(
        rule, s->kernel, candidate.c_str(),
        [&] { return condition_commit(s->kernel, instance, prepared); });
  });
  kernel_check_invariants(s->kernel);
  return result_code(s, result);
}

int tactix_rule_semantic_instances(tactix_solver *s, int rule_id,
                                   uint64_t *out, int cap) {
  input_complete(s);
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()) ||
      rules[rule_id].retired || cap < 0)
    return -1;
  const RuleVTable &r = rules[rule_id];
  if (!rule_guard(r, s->kernel))
    return -1;
  const auto enumerate = r.semantic_instances ? r.semantic_instances
                                              : r.instances;
  if (!enumerate) {
    if (cap >= 1)
      out[0] = INSTANCE_CANONICAL;
    return cap >= 1 ? 1 : 0;
  }
  return static_cast<int>(
      enumerate(s->kernel, out, static_cast<uint32_t>(cap)));
}

int tactix_apply_semantic_instance(tactix_solver *s, int rule_id,
                                   uint64_t instance) {
  input_complete(s);
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()) ||
      rules[rule_id].retired)
    return -1;
  const RuleVTable &r = rules[rule_id];
  if (!rule_guard(r, s->kernel))
    return -1;
  const auto apply = r.apply_semantic_instance ? r.apply_semantic_instance
                                               : r.apply_instance;
  if (!apply && instance != INSTANCE_CANONICAL)
    return -1;
  const ApplyResult result = apply_published(s, [&] {
    if (instance == INSTANCE_CANONICAL || !apply)
      return apply_rule(r, s->kernel);
    if (r.apply_semantic_instance)
      return apply_rule_semantic_instance(r, s->kernel, instance);
    return apply_rule_instance(r, s->kernel, instance);
  });
  kernel_check_invariants(s->kernel);
  return result_code(s, result);
}

static_assert(TACTIX_ANALYZE_TARGET_CHRONO == ANALYZE_TARGET_CHRONO);

int tactix_install_retier_schedule(tactix_solver *s,
                                   const tactix_retier_schedule *schedule) {
  if (!s || !schedule || schedule->growth == 0 ||
      schedule->tier1_relative_permille > 1000 ||
      schedule->tier2_relative_permille > 1000)
    return -1;
  RetierSchedule &installed = s->kernel.retier_schedule;
  installed.enabled = schedule->enabled != 0;
  installed.next_conflicts = schedule->next_conflicts;
  installed.interval = schedule->interval;
  installed.max_interval = schedule->max_interval;
  installed.growth = schedule->growth;
  installed.tier1_default = schedule->tier1_default;
  installed.tier2_default = schedule->tier2_default;
  installed.tier1_relative_permille = schedule->tier1_relative_permille;
  installed.tier2_relative_permille = schedule->tier2_relative_permille;
  for (int mode = 0; mode != 2; ++mode) {
    installed.tier1_by_mode[mode] = schedule->tier1_by_mode[mode];
    installed.tier2_by_mode[mode] = schedule->tier2_by_mode[mode];
  }
  return 0;
}

int tactix_read_retier_schedule(tactix_solver *s, tactix_retier_schedule *out) {
  if (!s || !out)
    return -1;
  const RetierSchedule &installed = s->kernel.retier_schedule;
  *out = {};
  out->enabled = installed.enabled;
  out->next_conflicts = installed.next_conflicts;
  out->interval = installed.interval;
  out->max_interval = installed.max_interval;
  out->growth = installed.growth;
  out->tier1_default = installed.tier1_default;
  out->tier2_default = installed.tier2_default;
  out->tier1_relative_permille = installed.tier1_relative_permille;
  out->tier2_relative_permille = installed.tier2_relative_permille;
  for (int mode = 0; mode != 2; ++mode) {
    out->tier1_by_mode[mode] = installed.tier1_by_mode[mode];
    out->tier2_by_mode[mode] = installed.tier2_by_mode[mode];
  }
  return 0;
}

int tactix_online_configure(tactix_solver *s,
                            const tactix_online_program *source,
                            uint64_t revision, int reconfigure_mode) {
  return configure_online_program(
      s, source, revision, reconfigure_mode,
      [](const auto &input, const auto &kernel, auto &program) {
        return online_program(input, kernel, program);
      });
}

#define TACTIX_DEFINE_ONLINE_CONFIGURE(version)                               \
  int tactix_online_configure_v##version(                                    \
      tactix_solver *s, const tactix_online_program_v##version *source,       \
      uint64_t revision, int reconfigure_mode) {                             \
    return configure_online_program(                                         \
        s, source, revision, reconfigure_mode,                               \
        [](const auto &input, const auto &kernel, auto &program) {            \
          return online_program_v##version(input, kernel, program);           \
        });                                                                  \
  }

TACTIX_DEFINE_ONLINE_CONFIGURE(2)
TACTIX_DEFINE_ONLINE_CONFIGURE(3)
TACTIX_DEFINE_ONLINE_CONFIGURE(4)
TACTIX_DEFINE_ONLINE_CONFIGURE(5)
TACTIX_DEFINE_ONLINE_CONFIGURE(6)
TACTIX_DEFINE_ONLINE_CONFIGURE(7)
TACTIX_DEFINE_ONLINE_CONFIGURE(8)
TACTIX_DEFINE_ONLINE_CONFIGURE(9)
TACTIX_DEFINE_ONLINE_CONFIGURE(10)
TACTIX_DEFINE_ONLINE_CONFIGURE(11)
TACTIX_DEFINE_ONLINE_CONFIGURE(12)
TACTIX_DEFINE_ONLINE_CONFIGURE(13)
TACTIX_DEFINE_ONLINE_CONFIGURE(14)
TACTIX_DEFINE_ONLINE_CONFIGURE(15)
TACTIX_DEFINE_ONLINE_CONFIGURE(16)
TACTIX_DEFINE_ONLINE_CONFIGURE(17)

#undef TACTIX_DEFINE_ONLINE_CONFIGURE

int tactix_online_advance(tactix_solver *s, tactix_online_result *result) {
  if (!s || !result || !s->online)
    return -1;
  input_complete(s);
  // The online executor publishes each application natively; the session
  // profile governs only events left after the composed step.
  strategy::InvocationResult advanced;
  (void)apply_published(s, [&] {
    advanced = s->online->advance(s->kernel);
    return advanced.rule_result;
  });
  kernel_check_invariants(s->kernel);
  if (advanced.status == strategy::InvocationStatus::Sat) {
    prepare_external_model(s);
  }
  if (advanced.status == strategy::InvocationStatus::Unsat)
    s->proof.flush();
  online_result(advanced, *result);
  return 0;
}

uint64_t tactix_online_step_consumed(tactix_solver *s, uint64_t step_id) {
  return s && s->online ? s->online->step_consumed(step_id) : UINT64_MAX;
}

int tactix_online_complete(tactix_solver *s) {
  return s && s->online ? (s->online->complete() ? 1 : 0) : -1;
}

uint64_t tactix_stat(tactix_solver *s, int stat_id) {
  const Kernel &k = s->kernel;
  switch (stat_id) {
  case 0: // TACTIX_STAT_CONFLICTS
    return k.stats.conflicts;
  case 1: // TACTIX_STAT_DECISIONS
    return k.stats.decisions;
  case 2: // TACTIX_STAT_PROPAGATIONS
    return k.stats.propagations;
  case 3: // TACTIX_STAT_TICKS
    return k.stats.ticks;
  case 4: // TACTIX_STAT_LEARNED
    return k.stats.learned;
  case 5: // TACTIX_STAT_RESTARTS
    return k.stats.restarts;
  case 6: // TACTIX_STAT_REDUCTIONS
    return k.stats.reductions;
  case 7: // TACTIX_STAT_SWITCHES
    return k.stats.switched;
  case 8: // TACTIX_STAT_LEVEL
    return k.level;
  case 9: // TACTIX_STAT_UNASSIGNED
    return k.unassigned;
  case 10: // TACTIX_STAT_LAST_LEARNED_GLUE (pending.glue persists
           // after Backjump consumes the pending clause)
    return k.pending.glue;
  case 11: // TACTIX_STAT_STABLE (0 for an unguided solver with no H)
    return k.h && k.h->stable ? 1 : 0;
  case 12: // TACTIX_STAT_REPHASED
    return k.stats.rephased;
  case 13: // TACTIX_STAT_SUBSUMPTIONS
    return k.stats.forward_subsumptions;
  case 14: // TACTIX_STAT_IRREDUNDANT_CLAUSES
    return kernel_irredundant_clauses(k);
  case 15: // TACTIX_STAT_ROOT_FIXED
    return kernel_logical_root_fixed(k);
  case 16: // TACTIX_STAT_BINARY_CLAUSES
    return k.binary_ids.size();
  case 17: // TACTIX_STAT_STRENGTHENED
    return k.stats.strengthened;
  case 18: // TACTIX_STAT_VIVIFIED
    return k.stats.vivified;
  case 19: // TACTIX_STAT_PROBE_FAILED
    return k.stats.probe_failed;
  case 20: // TACTIX_STAT_BACKBONE_FAILED
    return k.stats.backbone_failed;
  case 21: // TACTIX_STAT_TRANSITIVE_DELETED
    return k.stats.transitive_deleted;
  case 22: // TACTIX_STAT_FACTORINGS
    return k.stats.factorings;
  case 23: // TACTIX_STAT_FACTOR_TICKS
    return k.stats.factor_ticks;
  case 24: // TACTIX_STAT_BVE_ELIMINATED
    return k.stats.bve_eliminated;
  case 25: // TACTIX_STAT_BVE_RESOLUTIONS
    return k.stats.bve_resolutions;
  // BCE / HyperTernaryRes / Instantiate / Reorder, each a yield/effort pair
  case 26: // TACTIX_STAT_BCE_DELETED
    return k.stats.bce_deleted;
  case 27: // TACTIX_STAT_BCE_TICKS
    return k.stats.bce_ticks;
  case 28: // TACTIX_STAT_TERNARY_RESOLVENTS
    return k.stats.ternary_resolvents;
  case 29: // TACTIX_STAT_TERNARY_TICKS
    return k.stats.ternary_ticks;
  case 30: // TACTIX_STAT_INSTANTIATED
    return k.stats.instantiated;
  case 31: // TACTIX_STAT_REORDERED
    return k.stats.reordered;
  // Each effort id is the counter that family's EFFORT_SPECS meter reads, so
  // the cost-per-unit axis uses the currency the schedule doses in.
  case 32: // TACTIX_STAT_VIVIFY_TICKS
    return k.stats.vivify_ticks;
  case 33: // TACTIX_STAT_PROBE_TICKS
    return k.stats.probe_ticks;
  case 34: // TACTIX_STAT_BACKBONE_TICKS
    return k.stats.backbone_ticks;
  case 35: // TACTIX_STAT_TRANSITIVE_TICKS
    return k.stats.transitive_ticks;
  case 36: // TACTIX_STAT_FORWARD_SUBSUMED
    return k.stats.forward_subsumed;
  case 37: // TACTIX_STAT_FORWARD_VISITS
    return k.stats.forward_visits;
  case 38: // TACTIX_STAT_STRENGTHEN_VISITS
    return k.stats.strengthen_visits;
  case 39: // TACTIX_STAT_INSTANTIATE_TICKS
    return k.stats.instantiate_ticks;
  case 40: // TACTIX_STAT_SWEEP_EQUIVALENCES
    return k.stats.sweep_equivalences;
  case 41: // TACTIX_STAT_SWEEP_TICKS
    // The public Sweep effort statistic follows the currency DefaultPolicy
    // spends.  `sweep_ticks` remains the broader scan diagnostic.
    return k.stats.sweep_ticks_sub;
  case 42: // TACTIX_STAT_CONGRUENCE_MERGES
    return k.stats.congruence_merges;
  case 43: // TACTIX_STAT_CONGRUENCE_TICKS
    return k.stats.congruence_ticks;
  case 44: // TACTIX_STAT_CCE_DELETED
    return k.stats.cce_deleted;
  case 45: // TACTIX_STAT_CCE_TICKS
    return k.stats.cce_ticks;
  case 46: // TACTIX_STAT_SUBSTITUTE_VARS
    return k.stats.substitute_vars;
  case 47: // TACTIX_STAT_SUBSTITUTE_SCC_LITS
    return k.stats.substitute_scc_lits;
  case 48: // TACTIX_STAT_HBR_BINARIES
    return k.stats.hbr_binaries;
  case 49: // TACTIX_STAT_HBR_TICKS
    return k.stats.hbr_ticks;
  case 50: // TACTIX_STAT_SUBSTITUTE_TICKS
    return k.stats.substitute_ticks;
  case 51: // TACTIX_STAT_HBR_UNITS
    return k.stats.hbr_units;
  case 52: // TACTIX_STAT_PARITY_CONSEQUENCE_ATTEMPTS
    return k.stats.parity_consequence_attempts;
  case 53: // TACTIX_STAT_PARITY_CONSEQUENCE_CLAUSES
    return k.stats.parity_consequence_clauses;
  case 54: // TACTIX_STAT_PARITY_CONSEQUENCE_EQUATIONS
    return k.stats.parity_consequence_equations;
  case 55: // TACTIX_STAT_PARITY_CONSEQUENCE_SELECTION_WORK
    return k.stats.parity_consequence_selection_work;
  case 56: // TACTIX_STAT_PARITY_CONSEQUENCE_PREFLIGHT_ADDITIONS
    return k.stats.parity_consequence_preflight_additions;
  case 57: // TACTIX_STAT_PARITY_CONSEQUENCE_ADDITIONS
    return k.stats.parity_consequence_additions;
  case 58: // TACTIX_STAT_PARITY_CONSEQUENCE_DELETIONS
    return k.stats.parity_consequence_deletions;
  case 59: // TACTIX_STAT_PARITY_CONSEQUENCE_HELPERS
    return k.stats.parity_consequence_helpers;
  case 60: // TACTIX_STAT_PARITY_CONSEQUENCE_PUBLISHED
    return k.stats.parity_consequence_published;
  case 61: // TACTIX_STAT_BACKJUMPS
    return k.stats.backjumps;
  case 62: // TACTIX_STAT_BVE_OTF_STRENGTHENED
    return k.stats.bve_otf_strengthened;
  case 63: // TACTIX_STAT_BVE_OTF_SUBSUMED
    return k.stats.bve_otf_subsumed;
  case 64: // TACTIX_STAT_FASTEL_EXTENSIONS_PROTECTED
    return k.stats.fastel_extensions_protected;

  default:
    return UINT64_MAX;
  }
}

uint64_t tactix_config(tactix_solver *s, int cfg_id) {
  const Kernel &k = s->kernel;
  switch (cfg_id) {
  case 0: // TACTIX_CFG_BVE_GATES
    return k.bve_gate_mask;
  case 1: // TACTIX_CFG_BVE_BOUND
    return k.bve_bound;
  case 2: // TACTIX_CFG_FACTOR_BOUND
    return k.factor_bound;
  case 3: // TACTIX_CFG_FACTOR_COMPOSITE
    return k.factor_composite ? 1 : 0;
  case 4: // TACTIX_CFG_WORK_BUDGET
    return k.work_budget;
  case 5: // TACTIX_CFG_VIVIFY_SCOPE
    return k.vivify_scope;
  case 6: // TACTIX_CFG_SUBSUME_CLAUSE_SIZE
    return k.subsume_forward_parameters.clause_size_limit;
  case 7: // TACTIX_CFG_SUBSUME_LARGE_OCCURRENCES
    return k.subsume_forward_parameters.large_occurrence_limit;
  case 8: // TACTIX_CFG_SUBSUME_BINARY_OCCURRENCES
    return k.subsume_forward_parameters.binary_occurrence_limit;
  case 9: // TACTIX_CFG_STRENGTHEN_CLAUSE_SIZE
    return k.strengthen_parameters.clause_size_limit;
  case 10: // TACTIX_CFG_STRENGTHEN_OCCURRENCES
    return k.strengthen_parameters.occurrence_limit;
  case 11: // TACTIX_CFG_ANALYZE_MINIMIZE_DEPTH
    return k.analyze_parameters.minimize_depth_limit;
  case 12: // TACTIX_CFG_CONGRUENCE_AND_ARITY
    return k.congruence_parameters.and_arity_limit;
  case 13: // TACTIX_CFG_CONGRUENCE_GATE_COUNT
    return k.congruence_parameters.gate_count_limit;
  case 14: // TACTIX_CFG_CONGRUENCE_XOR_ARITY
    return k.congruence_parameters.xor_arity_limit;
  case 26: // TACTIX_CFG_CONGRUENCE_WATCH_ORDER
    return static_cast<uint64_t>(k.congruence_parameters.binary_watch_order);
  case 16: // TACTIX_CFG_INSTANTIATE_MINIMUM_CLAUSE_SIZE
    return k.instantiate_parameters.minimum_clause_size;
  case 17: // TACTIX_CFG_INSTANTIATE_OCCURRENCE_LIMIT
    return k.instantiate_parameters.occurrence_limit;
  case 18: // TACTIX_CFG_INSTANTIATE_MINIMUM_UNASSIGNED
    return k.instantiate_parameters.minimum_unassigned;
  case 19: // TACTIX_CFG_INSTANTIATE_INCLUDE_REDUNDANT
    return k.instantiate_parameters.include_redundant ? 1 : 0;
  case 20: // TACTIX_CFG_BACKBONE_INITIAL_ROUNDS
    return k.backbone_parameters.initial_rounds;
  case 21: // TACTIX_CFG_BACKBONE_MAXIMUM_ROUNDS
    return k.backbone_parameters.maximum_rounds;
  case 22: // TACTIX_CFG_VIVIFY_REUSE
    return k.vivify_reuse ? 1 : 0;
  case 23: // TACTIX_CFG_VIVIFY_FLUSH
    return k.vivify_flush ? 1 : 0;
  case 24: // TACTIX_CFG_VIVIFY_IRREDUNDANT_SORT_RATIO
    return k.vivify_irredundant_sort_ratio;
  case 25: // TACTIX_CFG_INSTANTIATE_NEW_MATERIAL_THRESHOLD
    return k.instantiate_parameters.new_material_threshold;
  case 27: // TACTIX_CFG_GUIDANCE_PUBLICATION
    return static_cast<uint64_t>(s->guidance_publication);
  case 28: // TACTIX_CFG_TOUCH_FILTER
    return k.touch_filter ? 1 : 0;
  case 29: // TACTIX_CFG_LIKELY_KEPT_REDUNDANT
    return k.touch_redundant ? 1 : 0;
  default:
    return UINT64_MAX;
  }
}

int tactix_set_guidance_publication(tactix_solver *s, int mode) {
  if (!s || (mode != TACTIX_GUIDANCE_DISCARD && mode != TACTIX_GUIDANCE_NATIVE))
    return -1;
  s->guidance_publication = mode;
  return 0;
}

int tactix_install_parameters(tactix_solver *s, int rule_id,
                              const tactix_online_parameters *parameters) {
  const auto &rules = tactix_rules();
  if (!s || !parameters || rule_id < 0 ||
      rule_id >= static_cast<int>(rules.size()) ||
      rules[static_cast<size_t>(rule_id)].retired)
    return -1;
  strategy::InprocessParameters converted;
  if (!online_parameters(*parameters, converted) ||
      !strategy::validate_parameter_installation(
          rules[static_cast<size_t>(rule_id)].name, converted))
    return -1;
  return strategy::install_permanent_parameters(s->kernel, converted) ? 0 : -1;
}

int tactix_install_affine_matrix_profile(
    tactix_solver *s, const tactix_online_affine_matrix_initialize_parameters *p) {
  if (!s || !p || p->admission < 1 || p->admission > 2 || p->operation < 1 ||
      p->operation > 10)
    return -1;
  tactix_online_parameters encoded{};
  encoded.kind = TACTIX_ONLINE_PARAMETERS_AFFINE_CONSEQUENCE;
  encoded.value.affine_consequence = p->basis;
  strategy::InprocessParameters basis;
  if (!online_parameters(encoded, basis) ||
      !std::holds_alternative<strategy::AffineConsequenceParameters>(basis))
    return -1;
  strategy::AffineMatrixInitializeParameters profile;
  profile.basis = std::get<strategy::AffineConsequenceParameters>(basis);
  profile.admission =
      static_cast<strategy::AffineMatrixInitializeParameters::Admission>(p->admission);
  profile.operation =
      static_cast<strategy::AffineMatrixInitializeParameters::Operation>(p->operation);
  profile.maximum_retained_bytes = p->maximum_retained_bytes;
  profile.maximum_deferred_deletions = p->maximum_deferred_deletions;
  profile.maximum_deferred_literal_work = p->maximum_deferred_literal_work;
  const strategy::InprocessParameters converted = profile;
  if (!profile.valid() ||
      !strategy::validate_parameter_installation("AffineMatrixInitialize", converted))
    return -1;
  return strategy::install_permanent_parameters(s->kernel, converted) ? 0 : -1;
}

int tactix_read_affine_matrix_result(tactix_solver *s, tactix_affine_matrix_result *out) {
  if (!s || !out) return -1;
  *out = {};
  const AffineMatrixSearchSession &session = s->affine_matrix_search_session;
  if (const auto &init = session.last_initialization()) {
    out->initialization_status = static_cast<int>(init->status) + 1;
    out->initialization_result = static_cast<int>(init->result);
    out->unchanged = init->unchanged;
    out->equations = init->equations;
    out->rows = init->rows;
    out->admitted_sources = init->admitted_sources;
  }
  if (const auto &service = session.last_service()) {
    out->service_status = static_cast<int>(service->status) + 1;
    out->service_result = static_cast<int>(service->result);
    out->bcp_pending = service->bcp_pending;
    out->reason_deferred = service->reason_deferred;
    out->reason_materialized = service->reason_materialized;
    out->reason_detached = service->live.detached_reason;
  }
  out->enabled = session.enabled();
  out->persistent = session.persistent();
  out->basis_rank = session.enabled() ? session.basis_rank() : 0;
  return 0;
}

int tactix_install_material_settings(tactix_solver *s, int touch_filter_mode,
                                     int likely_kept_redundant_mode) {
  strategy::SettingOverride touch, kept;
  if (!s || !online_setting_override(touch_filter_mode, touch) ||
      !online_setting_override(likely_kept_redundant_mode, kept))
    return -1;
  const auto selected = [](strategy::SettingOverride value) -> std::optional<bool> {
    if (value == strategy::SettingOverride::Inherit) return std::nullopt;
    return value == strategy::SettingOverride::Enable;
  };
  execution::execute_action(s->kernel, execution::MaterialSettingsRequest{
                                           selected(touch), selected(kept)});
  return 0;
}

namespace {

// Native support records carry these public arguments as their instance.
static_assert(execution::execution_context_argument(execution::ExecutionContext::Startup) ==
              TACTIX_CONTEXT_STARTUP);
static_assert(execution::execution_context_argument(execution::ExecutionContext::Inprocessing) ==
              TACTIX_CONTEXT_INPROCESSING);
static_assert(execution::execution_context_argument(execution::ExecutionContext::Speculation) ==
              TACTIX_CONTEXT_SPECULATION);
static_assert(execution::root_preparation_argument({true, false}, false) ==
                  TACTIX_ROOT_PREPARATION_PROBING &&
              execution::root_preparation_argument({false, false}, true) ==
                  TACTIX_ROOT_PREPARATION_CAPTURE_TARGET &&
              execution::root_preparation_argument({false, true}, false) ==
                  TACTIX_ROOT_PREPARATION_CONSUME_ROOT_PREFIX);
static_assert(execution::PHASE_ELIMINATE == TACTIX_PHASE_ELIMINATE &&
              execution::inprocess_phase_argument(false, 1) ==
                  uint64_t{1} << TACTIX_PHASE_FACTOR_BOUND_SHIFT);
static_assert(execution::COMPACT_FOLD_FIXED == TACTIX_COMPACT_FOLD_FIXED &&
              execution::COMPACT_PUBLISH_RANDOM_ORDER ==
                  TACTIX_COMPACT_PUBLISH_RANDOM_ORDER);
static_assert(static_cast<int>(execution::WarmupPhase::Heuristic) ==
                  TACTIX_WARMUP_PHASE_HEURISTIC &&
              static_cast<int>(execution::WarmupPhase::Positive) ==
                  TACTIX_WARMUP_PHASE_POSITIVE &&
              static_cast<int>(execution::WarmupPhase::Negative) ==
                  TACTIX_WARMUP_PHASE_NEGATIVE);

// A quiescent root: no pending conflict, analysis, occurrence session or
// probing transaction.
bool support_root(const Kernel &k) {
  return !k.inconsistent && !k.level && !k.conflict_valid && !k.pending.valid &&
         !k.dense && !k.probing.recording;
}

// Roots a support derives reach H as a registry dispatch's would under the
// session profile.
void publish_support_roots(tactix_solver *s, size_t begin) {
  if (s->guidance_publication == TACTIX_GUIDANCE_NATIVE)
    HeuristicRulePublisher{}.roots(s->kernel, begin);
}

// Leave the open inprocessing phase bracket (DefaultPolicy's phase exit).
int leave_phase(tactix_solver *s) {
  Kernel &k = s->kernel;
  const ApplyResult left = s->phase.exit(
      k, k.inconsistent,
      [&] {
        if (s->guidance_publication == TACTIX_GUIDANCE_NATIVE)
          HeuristicRulePublisher{}.retirements(k);
      },
      [&] {
        const size_t begin = k.trail.size();
        const uint64_t roots = k.stats.root_assignments;
        const ApplyResult propagated = propagate_apply(k);
        if (k.stats.root_assignments != roots) publish_support_roots(s, begin);
        return propagated;
      });
  if (left == ApplyResult::Unsat) {
    s->proof.flush();
    return 20;
  }
  return 0;
}

int support_result(tactix_solver *s, ApplyResult result) {
  if (result == ApplyResult::Unsat) {
    s->proof.flush();
    return 20;
  }
  return result == ApplyResult::NoOp ? 1 : 0;
}

} // namespace

int tactix_support(tactix_solver *s, int support, uint64_t argument) {
  if (!s)
    return -1;
  input_complete(s);
  Kernel &k = s->kernel;
  // Support operations are compound state transitions outside the rule
  // registry.  None may cross an outstanding affine reason debt; the sole
  // legal discharge is the public MaterializeReasons rule operation.
  if (k.affine_matrix_search_session &&
      k.affine_matrix_search_session->has_deferred_reason())
    return -1;
  const auto flags = [&](uint64_t allowed) { return !(argument & ~allowed); };
  int result = -1;
  switch (support) {
  case TACTIX_SUPPORT_CLOSE_STARTUP_CERTIFICATE:
    if (argument)
      return -1;
    execution::execute_action(k, execution::CloseStartupCertificateRequest{});
    result = 0;
    break;
  case TACTIX_SUPPORT_PREPARATION_RESTART:
    if (argument > UINT32_MAX || !restart_guard(k))
      return -1;
    result = support_result(s, execution::execute_action(
        k, execution::RestartRequest{static_cast<uint32_t>(argument),
                                     execution::RestartPurpose::Preparation}));
    break;
  case TACTIX_SUPPORT_ROOT_PREPARATION: {
    if (!flags(TACTIX_ROOT_PREPARATION_PROBING |
               TACTIX_ROOT_PREPARATION_CAPTURE_TARGET |
               TACTIX_ROOT_PREPARATION_CONSUME_ROOT_PREFIX) ||
        k.inconsistent || k.pending.valid || k.dense || k.probing.recording)
      return -1;
    const ApplyResult prepared = execution::execute_action(
        k, execution::RootPreparationRequest{
               (argument & TACTIX_ROOT_PREPARATION_PROBING) != 0,
               (argument & TACTIX_ROOT_PREPARATION_CONSUME_ROOT_PREFIX) != 0},
        [&] {
          if (argument & TACTIX_ROOT_PREPARATION_CAPTURE_TARGET)
            heuristics_update_target_phases(k);
        },
        [&](const execution::RestartRequest &request) {
          return execution::execute_action(k, request);
        },
        [&] {
          const size_t begin = k.trail.size();
          const uint64_t roots = k.stats.root_assignments;
          const ApplyResult propagated = propagate_apply(k);
          if (k.stats.root_assignments != roots)
            publish_support_roots(s, begin);
          return propagated;
        });
    result = support_result(s, prepared);
    break;
  }
  case TACTIX_SUPPORT_SPARSE_COLLECT: {
    if (argument || !support_root(k) || !kernel_propagation_fixpoint(k))
      return -1;
    // Derived units stay on the propagation queue for the caller's Propagate,
    // which publishes the roots it assigns.
    const RootCollectOutcome collected =
        execution::execute_action(k, execution::SparseCollectRequest{});
    result = collected == RootCollectOutcome::Refuted ? support_result(s, ApplyResult::Unsat)
             : collected == RootCollectOutcome::Unit  ? 2
             : collected == RootCollectOutcome::NoChange ? 1 : 0;
    break;
  }
  case TACTIX_SUPPORT_COMPACT_RANDOM_ORDER:
    if (argument || !support_root(k) || !k.h)
      return -1;
    execution::execute_action(k, execution::CompactRandomOrderRequest{});
    result = 0;
    break;
  case TACTIX_SUPPORT_COMPACT_VARIABLES: {
    // Any consistent state, as Reduce compacts above the root; the kernel
    // renumbers the trail with the variables.
    if (!flags(TACTIX_COMPACT_FOLD_FIXED | TACTIX_COMPACT_PUBLISH_RANDOM_ORDER) ||
        k.inconsistent || k.conflict_valid || k.pending.valid || k.dense ||
        k.probing.recording)
      return -1;
    const bool compacted = execution::execute_action(
        k, execution::CompactVariablesRequest{(argument & TACTIX_COMPACT_FOLD_FIXED) != 0});
    if (compacted && (argument & TACTIX_COMPACT_PUBLISH_RANDOM_ORDER))
      heuristics_compact_random_order_fixed(k);
    result = compacted ? 0 : 1;
    break;
  }
  case TACTIX_SUPPORT_REDUCTION_PREPARATION:
    if (argument || !restart_guard(k) || k.pending.valid || k.dense ||
        k.probing.recording)
      return -1;
    execution::execute_action(k, execution::ReductionPreparationRequest{},
        [&](size_t begin) { publish_support_roots(s, begin); });
    result = support_result(s, k.inconsistent ? ApplyResult::Unsat : ApplyResult::Ok);
    break;
  case TACTIX_SUPPORT_COLLECT_GARBAGE:
    // kernel_collect_garbage remaps an open occurrence session, so unlike the
    // other root supports it does not require !k.dense.
    if (!flags(TACTIX_COLLECT_SPARSE | TACTIX_COLLECT_PRESERVE_EAGER) ||
        k.inconsistent || k.level || k.conflict_valid || k.pending.valid ||
        k.probing.recording || k.bve_occurrence_count_image)
      return -1;
    execution::execute_action(k, execution::CollectGarbageRequest{
        0, (argument & TACTIX_COLLECT_SPARSE) != 0,
        (argument & TACTIX_COLLECT_PRESERVE_EAGER) != 0});
    result = 0;
    break;
  case TACTIX_SUPPORT_FLUSH_RETIRED_REDUNDANT:
    if (argument || !support_root(k))
      return -1;
    execution::execute_action(k, execution::FlushRetiredRedundantRequest{});
    result = 0;
    break;
  case TACTIX_SUPPORT_BEGIN_BVE_DEFINITION_SESSION:
    if (argument)
      return -1;
    execution::execute_action(k, execution::BeginBveDefinitionSessionRequest{});
    result = 0;
    break;
  case TACTIX_SUPPORT_WARMUP: {
    if (argument > TACTIX_WARMUP_PHASE_NEGATIVE || !support_root(k) || !k.h ||
        !kernel_propagation_fixpoint(k))
      return -1;
    const auto phase = static_cast<execution::WarmupPhase>(argument);
    const auto warmed = execution::execute_action(k, execution::WarmupRequest{},
        [&] { return execution::warmup_decision(k, phase); }, [](Lit) {},
        [](size_t) {});
    result = warmed.interrupted ? 1 : 0;
    break;
  }
  case TACTIX_SUPPORT_RESEED_GUIDANCE:
    if (argument || !support_root(k) || k.h != &s->heuristics)
      return -1;
    // The session's H is replaced by the one a native policy attaches after
    // preparing the residual seed (DefaultPolicy's residual handoff).
    execution::execute_action(k, execution::HeuristicDetachmentRequest{s->heuristics});
    (void)execution::execute_action(k, execution::ResidualActivationRequest{});
    {
      const std::vector<Var> order = k.input_activation_order;
      s->heuristics = Heuristics{};
      execution::execute_action(k, execution::HeuristicAttachmentRequest{s->heuristics});
      execution::execute_action(k, execution::ResidualImportRequest{order});
    }
    result = 0;
    break;
  case TACTIX_SUPPORT_RESET_GRAPH_LABELING_SESSION:
    if (argument)
      return -1;
    result = execution::execute_action(k, execution::ResetGraphLabelingSessionRequest{}) ? 0 : 1;
    break;
  case TACTIX_SUPPORT_RELEASE_INPUT_ORDER:
    if (argument)
      return -1;
    result = execution::execute_action(k, execution::ReleaseInputBinaryOrderRequest{}) ? 0 : 1;
    break;
  case TACTIX_SUPPORT_CONSUME_ROOT_PREFIX:
    if (argument)
      return -1;
    result = execution::execute_action(k, execution::ConsumeRootPrefixRequest{}) ? 0 : 1;
    break;
  case TACTIX_SUPPORT_ENTER_CONTEXT: {
    if (argument < TACTIX_CONTEXT_STARTUP || argument > TACTIX_CONTEXT_SPECULATION)
      return -1;
    const auto context = argument == TACTIX_CONTEXT_STARTUP
                             ? execution::ExecutionContext::Startup
                         : argument == TACTIX_CONTEXT_INPROCESSING
                             ? execution::ExecutionContext::Inprocessing
                             : execution::ExecutionContext::Speculation;
    s->contexts.emplace_back(context, execution::enter_execution_context(k, context));
    return 0; // a context changes no solver state a client observes
  }
  case TACTIX_SUPPORT_BEGIN_FORWARD_ROUND:
    if (argument)
      return -1;
    result = execution::execute_action(k, execution::BeginForwardRoundRequest{}) ? 0 : 1;
    break;
  case TACTIX_SUPPORT_COMPACT_FASTEL_PREFIX: {
    const Lit lit = kernel_internalize(
        k, static_cast<int32_t>(static_cast<uint32_t>(argument)));
    if (lit == INVALID_LIT || !k.dense || !k.dense->track_order)
      return -1;
    result = execution::execute_action(
                 k, execution::CompactFastelPrefixRequest{lit, static_cast<size_t>(argument >> 32)})
                 ? 0 : 1;
    break;
  }
  case TACTIX_SUPPORT_HOLD_RETIREMENTS:
    // HOLD defers retirement publication; RELEASE ends the hold and publishes
    // the queued events under the session profile; RELEASE_ONLY leaves them
    // for the next publication. Guidance only: no state a rule reads.
    if (argument > TACTIX_RETIREMENTS_RELEASE_ONLY)
      return -1;
    k.retirement_publication_held = argument == TACTIX_RETIREMENTS_HOLD;
    if (argument == TACTIX_RETIREMENTS_RELEASE &&
        s->guidance_publication == TACTIX_GUIDANCE_NATIVE)
      HeuristicRulePublisher{}.retirements(k);
    return 0;
  case TACTIX_SUPPORT_RESCHEDULE_BVE:
    if (argument)
      return -1;
    result = execution::execute_action(k, execution::RescheduleBveCandidatesRequest{}) ? 0 : 1;
    break;
  case TACTIX_SUPPORT_RETIRE_BVE_CANDIDATE: {
    if (!argument || argument > static_cast<uint64_t>(INT32_MAX))
      return -1;
    const Lit lit = kernel_internalize(k, static_cast<int>(argument));
    if (lit == INVALID_LIT)
      return -1;
    result = execution::execute_action(
                 k, execution::RetireBveCandidateRequest{var_of(lit)})
                 ? 0 : 1;
    break;
  }
  case TACTIX_SUPPORT_FORWARD_CLOSURE:
    if (argument > k.trail.size() || k.level || k.conflict_valid ||
        k.pending.valid || k.probing.recording || !k.dense ||
        !k.dense->track_order || k.large_watches_connected)
      return -1;
    result = support_result(s, execution::execute_action(
        k, execution::ForwardClosureRequest{static_cast<uint32_t>(argument)}));
    break;
  case TACTIX_SUPPORT_LEAVE_CONTEXT:
    if (argument || s->contexts.empty() ||
        (s->phase.open() && s->contexts.size() <= s->phase_contexts))
      return -1;
    execution::leave_execution_context(k, s->contexts.back().first,
                                       s->contexts.back().second);
    s->contexts.pop_back();
    return 0;
  case TACTIX_SUPPORT_ENTER_INPROCESS_PHASE: {
    const bool eliminate = (argument & TACTIX_PHASE_ELIMINATE) != 0;
    if ((argument & ~(TACTIX_PHASE_ELIMINATE | (uint64_t{UINT32_MAX} << 32))) ||
        s->phase.open() || !support_root(k) || k.factor_selection ||
        (eliminate && !k.large_watches_connected))
      return -1;
    s->phase_contexts = s->contexts.size();
    s->phase.enter(k, execution::InprocessPhaseEntryRequest{
                          eliminate, s->phase_factor_selection,
                          static_cast<uint32_t>(argument >> 32)});
    result = 0;
    break;
  }
  case TACTIX_SUPPORT_OPEN_OCCURRENCE_SESSION:
    if (argument || !s->phase.open() || s->phase.occurrences_open() || k.inconsistent ||
        k.level || k.conflict_valid || k.pending.valid || k.dense || k.probing.recording)
      return -1;
    s->phase.open_occurrences(k, s->phase_occurrences);
    result = 0;
    break;
  case TACTIX_SUPPORT_RELEASE_AFFINE_MATRIX:
    // Drop the affine search session's algebraic and proof-planning state;
    // published clauses stay. DETACHED clears without releasing leases.
    if (argument > TACTIX_AFFINE_MATRIX_RELEASE_DETACHED ||
        !k.affine_matrix_search_session ||
        k.affine_matrix_search_session->has_deferred_reason())
      return -1;
    if (argument) k.affine_matrix_search_session->disable();
    else k.affine_matrix_search_session->disable(k);
    result = 0;
    break;
  case TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE:
    if (argument || !s->phase.open() || s->contexts.size() != s->phase_contexts ||
        k.level || k.conflict_valid || k.pending.valid || k.probing.recording)
      return -1;
    if (!s->phase.occurrences_open()) {
      // Without a session only a Probe bracket may close (a structured
      // startup terminal); an Eliminate bracket must reconnect its watches.
      if (s->phase.eliminate())
        return -1;
      s->phase.exit_terminal(k);
      result = 0;
      break;
    }
    if (k.dense != nullptr && k.dense != &s->phase_occurrences)
      return -1;
    result = leave_phase(s);
    break;
  default:
    return -1;
  }
  if (result == 0 || result == 2 || result == 20)
    external_state_change(s);
  kernel_check_invariants(k); // no-op in NDEBUG
  return result;
}

int tactix_decide_candidates(tactix_solver *s, int *lits, int cap) {
  if (cap <= 0)
    return 0;
  input_complete(s);
  std::vector<Lit> buf(static_cast<size_t>(cap));
  const uint32_t n =
      heuristics_decide_candidates(s->kernel, buf.data(),
                                   static_cast<uint32_t>(cap));
  for (uint32_t i = 0; i < n; i++)
    lits[i] = kernel_externalize(s->kernel, buf[i]);
  return static_cast<int>(n);
}

int tactix_decide(tactix_solver *s, int elit) {
  input_complete(s);
  if (!decide_guard(s->kernel))
    return -1;
  const Lit l = kernel_internalize(s->kernel, elit);
  if (l == INVALID_LIT)
    return -1;
  // This legacy convenience call bypasses registry dispatch, but consumes the
  // same original-input action boundary as the typed Decide rule.
  s->kernel.close_startup_certificate();
  if (s->kernel.proof && !s->kernel.proof->commit_clause_stream())
    return -1;
  const int ok = decide_apply(s->kernel, l, true) == ApplyResult::Ok ? 0 : -1;
  if (!ok)
    external_state_change(s);
  kernel_check_invariants(s->kernel); // no-op in NDEBUG
  return ok;
}

int tactix_status(tactix_solver *s) {
  // Also a verdict-reporting path: a caller that adds a contradictory input and
  // then only asks the status must still get the refutation written.
  input_complete(s);
  if (s->kernel.inconsistent) {
    s->proof.flush();
    return 20;
  }
  // SAT requires every active variable assigned at a propagation fixpoint,
  // with no conflict in flight. Retired, unassigned identities occur in no
  // live clause and kernel_extend_model supplies their external values. A
  // recorded conflict (conflict_valid) or an analyzed but not yet backjumped
  // one (pending.valid) still prevents a verdict.
  if (has_sat_assignment(s->kernel)) {
    prepare_external_model(s);
    return 10;
  }
  return 0;
}

} // extern "C"
