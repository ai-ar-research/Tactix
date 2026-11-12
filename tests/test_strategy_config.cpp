#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rule_test_utils.hpp"
#include "rules/reduce/reduce.hpp"
#include "rules/semantic_extension/affine_image.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

using namespace tactix;

static void add_root_affine_contradiction(Kernel &k) {
  kernel_reserve_vars(k, 4);
  for (const auto &equation : {std::vector<int32_t>{1, 2, 3}, {1, 2, 4}})
    for (const auto &clause :
         affine_semantic_extension::clausal_form(equation, false)) {
      std::vector<Lit> lits;
      for (const int32_t literal : clause)
        lits.push_back(kernel_internalize(k, literal));
      kernel_add_large_clause(k, lits.data(), lits.size(), false, 0,
                              ++k.next_clause_id);
    }
  for (const int32_t external : {-3, 4}) {
    const Lit unit = kernel_internalize(k, external);
    k.unit_ids[var_of(unit)] = ++k.next_clause_id;
    kernel_assign_unit(k, unit);
  }
}

static void add_affine_model_fixture(Kernel &k) {
  kernel_reserve_vars(k, 5);
  for (unsigned mask = 0; mask < 16; ++mask) {
    if (std::popcount(mask) % 2)
      continue;
    Lit clause[4];
    for (unsigned i = 0; i < 4; ++i)
      clause[i] = make_lit(i, (mask >> i) & 1);
    kernel_add_large_clause(k, clause, 4, false, 0);
  }
  for (unsigned i = 0; i < 3; ++i)
    kernel_add_binary_clause(k, make_lit(i, true), make_lit(i + 1, true));
}

static void add_php(Kernel &k, int pigeons, int holes) {
  kernel_reserve_vars(k, pigeons * holes);
  const auto variable = [holes](int pigeon, int hole) {
    return pigeon * holes + hole + 1;
  };
  std::vector<Lit> clause;
  for (int pigeon = 0; pigeon < pigeons; ++pigeon) {
    clause.clear();
    for (int hole = 0; hole < holes; ++hole)
      clause.push_back(internalize(variable(pigeon, hole)));
    kernel_add_large_clause(k, clause.data(), clause.size(), false, 0);
  }
  for (int hole = 0; hole < holes; ++hole)
    for (int first = 0; first < pigeons; ++first)
      for (int second = first + 1; second < pigeons; ++second)
        kernel_add_binary_clause(k, internalize(-variable(first, hole)),
                                 internalize(-variable(second, hole)));
}

static void add_blocked_amo_cleanup_fixture(Kernel &k) {
  kernel_reserve_vars(k, 9);
  for (const auto &clause : {std::initializer_list<int>{1, 2, 3}, {4, 5, 6},
                             {7, 8, 9}, {-1, -4, -7}, {-1, -5, -8},
                             {-1, -6, -9}, {-2, -4, -8}, {-2, -5, -9},
                             {-2, -6, -7}, {-3, -4, -9}, {-3, -5, -7},
                             {-3, -6, -8}, {-1, -4, -8}})
    add_external_clause(k, clause, false, 0, ++k.next_clause_id);
}

static bool schedules_lucky_instance(
    const strategy::OnlineStrategyProgram &program, uint64_t instance) {
  for (const auto &step : program.sequence)
    if (step.rule == "Lucky")
      for (const uint64_t scheduled : step.instances)
        if (scheduled == instance)
          return true;
  return false;
}

static strategy::OnlineInvocation
sr_step(uint64_t id, SubstitutionRedundancyParameters parameters,
        strategy::InvocationGranularity granularity) {
  strategy::OnlineInvocation step;
  step.id = id;
  step.rule = "SubstitutionRedundancy";
  step.parameters = parameters;
  step.granularity = granularity;
  return step;
}

static SubstitutionRedundancyParameters explicit_only() {
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::ExplicitOnly;
  return parameters;
}

// Complete the unique-literal AMOs without orbitopal matrices.
static SubstitutionRedundancyParameters ulc_completion() {
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
  parameters.maximum_orbitopal_matrices = 0;
  parameters.complete_unique_literal_amos = true;
  return parameters;
}

static void set_root_stage(DefaultStrategyConfig &config,
                           const strategy::OnlineInvocation &step) {
  strategy::OnlineStrategyProgram program;
  program.sequence = {step};
  config.inprocess.structured_root_stages = {
      {program, strategy::BudgetExhaustionPolicy::Continue}};
}

static void disable_structured_models(DefaultStrategyConfig &config) {
  config.parity_model.enabled = false;
  config.onehot_model.enabled = false;
  config.balanced_signature_model.enabled = false;
  config.fermat_model.enabled = false;
  config.cerny_model.enabled = false;
  config.lookup_circuit_model.enabled = false;
  config.simon_model.enabled = false;
  config.stconn_proof.enabled = false;
  config.signed_cardinality_proof.enabled = false;
}

template <class Config> static auto root_stage_lists(Config &config) {
  return std::array{&config.inprocess.structured_root_stages,
                    &config.inprocess.structured_root_bounded_backtracking_stages,
                    &config.inprocess.structured_root_large_domain_stages};
}

int main() {
  DefaultStrategyConfig defaults;
  const std::string default_snapshot = defaults.serialize();
  // Affine construction is the public Lucky-8 action, owned by DefaultPolicy
  // as the first structured-root stage.
  assert(default_snapshot.find(
             "affine_root_closure.maximum_basis_work=100000000") !=
         std::string::npos);
  assert(default_snapshot.find(
             "inprocess.structured_root_sr_discovery_ticks=20000000000") !=
         std::string::npos);
  for (const auto &step : defaults.inprocess.program)
    assert(step.rule != "Lucky");
  for (const auto *stages : root_stage_lists(defaults)) {
    assert(!stages->empty());
    assert(stages->front().program.sequence.size() == 1);
    const auto &affine = stages->front().program.sequence.front();
    assert(affine.id == 109 && affine.rule == "Lucky");
    assert(schedules_lucky_instance(stages->front().program, 8));
    assert(affine.granularity == strategy::InvocationGranularity::OneInstance);
    assert(!affine.budget.is_unlimited() && affine.budget.amount() == 1);
    for (size_t index = 1; index < stages->size(); ++index)
      assert(!schedules_lucky_instance((*stages)[index].program, 8));
  }
  assert(defaults.inprocess.structured_root_before_lucky);
  assert(!defaults.inprocess.structured_root_residual_search_handoff);
  assert(defaults.inprocess.structured_root_progress_feedback);
  assert(!defaults.inprocess.structured_root_large_domain_progress_feedback);
  assert(defaults.inprocess.structured_root_progress_minimum_ticks ==
         (16ull << 20));
  assert(defaults.inprocess.structured_root_progress_ticks_per_variable == 16);
  assert(!defaults.inprocess.structured_root_sr_discovery_budget.is_unlimited());
  assert(defaults.inprocess.structured_root_bce_after_unique_literal_amo);
  bool saw_default_graph = false;
  for (const auto &stage : defaults.inprocess.structured_root_stages)
    for (const auto &step : stage.program.sequence)
      if (const auto *parameters =
              std::get_if<SubstitutionRedundancyParameters>(&step.parameters);
          parameters &&
          parameters->producer == SubstitutionRedundancyProducer::GraphSymmetry) {
        saw_default_graph = true;
        assert(parameters->graph_symmetry.nonforcing_cut_admission ==
               SymmetryNonforcingCutAdmission::RequireCompoundProgress);
      }
  assert(saw_default_graph);
  assert(defaults.inprocess.structured_root_symmetry_enabled);
  for (unsigned domain = 0; domain != 3; ++domain) {
    DefaultStrategyConfig diagnostic = defaults;
    diagnostic.inprocess.structured_root_symmetry_enabled = false;
    if (domain) {
      diagnostic.inprocess.structured_root_backtracking_domain_limit = 1;
      diagnostic.inprocess.structured_root_large_domain_limit =
          domain == 1 ? 100 : 2;
    }
    DefaultPolicy policy(diagnostic);
    assert(policy.strategy_config().serialize().find(
               "inprocess.structured_root_symmetry_enabled=0") !=
           std::string::npos);
    for (const auto *stages : root_stage_lists(policy.strategy_config())) {
      assert(stages->size() == 1);
      assert(schedules_lucky_instance(stages->front().program, 8));
    }
    Kernel kernel;
    add_php(kernel, 4, 3);
    assert(policy.inprocess().run_structured_root(kernel, nullptr) == 0);
    assert(kernel.stats.affine_model_calls == 1);
    assert(kernel.stats.sr_applications == 0);
    assert(policy.inprocess().structured_root.stages.size() == 1);
    assert(policy.inprocess().structured_root.stages.front().runs == 1);
    assert(!kernel.inprocessing && !kernel.phase_saving_suppressed);
    kernel_check_invariants(kernel);
    diagnostic.affine_model.enabled = false;
    DefaultPolicy neither(diagnostic);
    for (const auto *stages : root_stage_lists(neither.strategy_config()))
      assert(stages->empty());
  }
  {
    // Empty formulas exercise the guards and counters without constructor
    // search: the default attempts affine and graph-labeling authentication.
    Kernel production;
    assert(DefaultPolicy(defaults).solve(production) == 10);
    assert(production.stats.affine_model_calls == 1);
    assert(production.stats.graph_labeling_calls == 1);
    assert(production.stats.affine_matrix_init_attempts == 0);

    DefaultStrategyConfig affine_disabled = defaults;
    affine_disabled.affine_model.enabled = false;
    DefaultPolicy without_affine(affine_disabled);
    for (const auto *stages : root_stage_lists(without_affine.strategy_config()))
      for (const auto &stage : *stages)
        assert(!schedules_lucky_instance(stage.program, 8));
    Kernel disabled;
    assert(without_affine.solve(disabled) == 10);
    assert(disabled.stats.affine_model_calls == 0);

    DefaultStrategyConfig graph_disabled = defaults;
    graph_disabled.graph_labeling_model.enabled = false;
    Kernel graph;
    assert(DefaultPolicy(graph_disabled).solve(graph) == 10);
    assert(graph.stats.affine_model_calls == 1);
    assert(graph.stats.graph_labeling_calls == 0);
  }
  {
    // The authenticated matrix service runs before formula-mutating startup
    // passes on a complete affine image.
    DefaultStrategyConfig config = defaults;
    config.affine_model.enabled = false;
    Kernel affine;
    AffineMatrixSearchSession session;
    affine.affine_matrix_search_session = &session;
    FILE *journal = std::tmpfile();
    assert(journal);
    affine.structural_provenance = journal;
    add_root_affine_contradiction(affine);
    assert(DefaultPolicy(config).solve(affine) == 20);
    assert(affine.stats.affine_matrix_init_attempts == 1 &&
           affine.stats.affine_matrix_init_applied == 1 &&
           affine.stats.affine_matrix_service_applied == 1 &&
           affine.stats.decisions == 0 && affine.stats.conflicts == 1);
    std::fclose(journal);
  }
  {
    // Lucky 8 is a real DefaultPolicy opportunity before CDCL on a small
    // affine image, with terminal whole-formula validation.
    DefaultStrategyConfig config = defaults;
    config.parity_model.enabled = false;
    Kernel affine;
    add_affine_model_fixture(affine);
    assert(DefaultPolicy(config).solve(affine) == 10);
    assert(affine.stats.affine_model_calls == 1);
    assert(affine.stats.affine_model_sat == 1);
    assert(affine.stats.decisions == 0 && affine.stats.conflicts == 0);
    kernel_check_invariants(affine);
  }
  assert(!default_should_cleanup_unique_literal_amo(0, true, false));
  assert(!default_should_cleanup_unique_literal_amo(3, false, false));
  assert(default_should_cleanup_unique_literal_amo(3, true, false));
  assert(!default_should_cleanup_unique_literal_amo(3, true, true));
  assert(!default_should_seed_structured_residual(false, 0, false));
  assert(default_should_seed_structured_residual(true, 0, false));
  assert(!default_should_seed_structured_residual(true, 3, false));
  assert(default_should_seed_structured_residual(true, 3, true));
  assert(!default_should_run_initial_fast_elimination(0, 0));
  assert(!default_should_run_initial_fast_elimination(5, 10));
  assert(default_should_run_initial_fast_elimination(6, 10));
  assert(!default_fast_elimination_discovery_admissible(0, 10));
  assert(default_fast_elimination_discovery_admissible(4, 8));
  assert(!default_fast_elimination_discovery_admissible(5, 8));
  assert(!default_should_suppress_fast_elimination_after_structured(false, 0));
  assert(!default_should_suppress_fast_elimination_after_structured(false, 1));
  assert(!default_should_suppress_fast_elimination_after_structured(true, 0));
  assert(default_should_suppress_fast_elimination_after_structured(true, 1));
  {
    // A checked SR unit grows the root trail without leaving a structured
    // helper in FastEl's candidate domain; root growth alone does not
    // suppress the bounded pass.
    DefaultStrategyConfig config = defaults;
    strategy::OnlineInvocation fixing =
        sr_step(112, explicit_only(), strategy::InvocationGranularity::OneInstance);
    fixing.substitution_redundancy_instances = {
        SubstitutionRedundancyInstance{{internalize(-57)}, {}, {}}};
    set_root_stage(config, fixing);
    config.inprocess.structured_root_before_lucky = false;
    config.inprocess.program = {
        {15, "BVE", strategy::InprocessPhase::Eliminate, true}};
    config.inprocess.append_unlisted_rules = false;
    disable_structured_models(config);

    Kernel productive;
    add_php(productive, 8, 7);
    kernel_reserve_vars(productive, 58);
    kernel_add_binary_clause(productive, internalize(-57), internalize(58));
    kernel_add_binary_clause(productive, internalize(-57), internalize(-58));
    DefaultPolicy productive_policy(config);
    assert(productive_policy.solve(productive) == 20);
    assert(productive.stats.sr_applications > 0);
    assert(productive.stats.sr_unique_literal_amo_binaries == 0);
    const auto &ownership =
        productive_policy.inprocess().structured_fastel_ownership();
    assert(!ownership.grew_root_trail && !ownership.symmetry_applied &&
           ownership.external_begin == ownership.external_end &&
           ownership.coupled_factor_ranges.empty() &&
           "startup FastEl must consume structured provenance exactly once");
    assert(productive.stats.fastel_rounds > 0);
    assert(productive.stats.fastel_root_suppressed == 0);
  }
  {
    // A checked SR clause that does not grow the root trail also composes with
    // FastEl.
    DefaultStrategyConfig config = defaults;
    strategy::OnlineInvocation clause_only =
        sr_step(113, explicit_only(), strategy::InvocationGranularity::OneInstance);
    clause_only.substitution_redundancy_instances = {
        SubstitutionRedundancyInstance{
            {internalize(58), internalize(59)}, {}, {}}};
    set_root_stage(config, clause_only);
    disable_structured_models(config);

    Kernel compositional;
    add_php(compositional, 8, 7);
    kernel_reserve_vars(compositional, 59);
    kernel_add_binary_clause(compositional, internalize(57), internalize(58));
    kernel_add_binary_clause(compositional, internalize(-57), internalize(59));
    assert(DefaultPolicy(config).solve(compositional) == 20);
    assert(compositional.stats.sr_applications > 0);
    assert(compositional.stats.fastel_rounds > 0);
    assert(compositional.stats.fastel_root_suppressed == 0);
  }
  {
    // Generic graph symmetry consumes the completed BIG and fixes a root,
    // which suppresses the optional cleanup in both configurations.
    Kernel enabled, disabled;
    add_blocked_amo_cleanup_fixture(enabled);
    add_blocked_amo_cleanup_fixture(disabled);
    DefaultStrategyConfig enabled_config = defaults;
    DefaultStrategyConfig disabled_config = defaults;
    disabled_config.inprocess.structured_root_bce_after_unique_literal_amo =
        false;
    assert(DefaultPolicy(enabled_config).solve(enabled) == 10);
    assert(DefaultPolicy(disabled_config).solve(disabled) == 10);
    assert(enabled.stats.sr_unique_literal_amo_binaries > 0);
    assert(enabled.stats.sr_unique_literal_amo_binaries ==
           disabled.stats.sr_unique_literal_amo_binaries);
    assert(enabled.stats.sr_applied >
           enabled.stats.sr_unique_literal_amo_binaries);
    assert(enabled.stats.sr_post_amo_nonhelper_applied > 0);
    assert(enabled.stats.sr_post_amo_graph_root_units > 0);
    assert(enabled.stats.bce_deleted == disabled.stats.bce_deleted);
    // A graph-symmetry root fix does not record FastEl policy suppression.
    assert(enabled.stats.fastel_root_suppressed == 0 &&
           disabled.stats.fastel_root_suppressed == 0);
  }
  {
    // Completion with no later SR consequence is the structured result: its
    // binaries stay live and no cleanup pass runs.
    Kernel helper_only;
    kernel_reserve_vars(helper_only, 3);
    add_external_clause(helper_only, {1, 2, 3}, false, 0,
                        ++helper_only.next_clause_id);
    DefaultStrategyConfig config = defaults;
    set_root_stage(config,
                   sr_step(110, ulc_completion(),
                           strategy::InvocationGranularity::CompleteCurrentPass));
    // Keep startup elimination out of this ULC-cleanup fixture.
    config.inprocess.initial_fast_elimination = false;
    assert(DefaultPolicy(config).solve(helper_only) == 10);
    assert(helper_only.stats.sr_unique_literal_amo_binaries == 3);
    assert(helper_only.stats.sr_applied == 3);
    assert(helper_only.stats.sr_post_amo_nonhelper_applied == 0);
    assert(helper_only.stats.sr_post_amo_graph_root_units == 0);
    assert(helper_only.stats.bce_deleted == 0);
    assert(kernel_binary_live(helper_only, internalize(-1), internalize(-2)));
    assert(kernel_binary_live(helper_only, internalize(-1), internalize(-3)));
    assert(kernel_binary_live(helper_only, internalize(-2), internalize(-3)));
  }
  {
    // A ULC completed during structured startup immediately exposes the
    // consequences of an already-fixed member, as when parsing Satsuma's
    // residual.
    Kernel rooted_helper;
    kernel_reserve_vars(rooted_helper, 3);
    add_external_clause(rooted_helper, {1, 2, 3}, false, 0,
                        ++rooted_helper.next_clause_id);
    kernel_assign_unit(rooted_helper, internalize(2));
    DefaultStrategyConfig config = defaults;
    strategy::OnlineInvocation completion = sr_step(
        111, ulc_completion(), strategy::InvocationGranularity::CompleteCurrentPass);
    completion.post_application = strategy::PostApplication::Propagate;
    set_root_stage(config, completion);
    assert(DefaultPolicy(config).solve(rooted_helper) == 10);
    assert(rooted_helper.value(internalize(-1)) > 0);
    assert(rooted_helper.value(internalize(-3)) > 0);
    assert(rooted_helper.assigned[var_of(internalize(-1))].level == 0);
    assert(rooted_helper.assigned[var_of(internalize(-3))].level == 0);
  }
  // Kissat reference values.
  assert(defaults.inprocess.substitute_search_rounds == 1);
  // Kissat options.h tier1=2, tier2=6.
  assert(defaults.reduction.tier1_default == TIER1_STATIC &&
         TIER1_STATIC == 2);
  assert(defaults.reduction.tier2_default == TIER2_STATIC &&
         TIER2_STATIC == 6);
  assert(defaults.reduction.victim_numerator == 3);
  assert(defaults.reduction.victim_denominator == 4);
  assert(defaults.reduction.protect_tiers);
  assert(defaults.branching.random_seed == 0);
  assert(defaults.rephase.rotation.size() == 6);
  assert(defaults.rephase.rotation[0] == DefaultRephaseSlot::Best &&
         defaults.rephase.rotation[1] == DefaultRephaseSlot::Walk &&
         defaults.rephase.rotation[2] == DefaultRephaseSlot::Inverted &&
         defaults.rephase.rotation[5] == DefaultRephaseSlot::Original);
  assert(defaults.rephase.walk_even_break_base_milli == 2000);
  assert(defaults.rephase.walk_score_curve.size() == 6);
  assert(defaults.reorder.parameters.valid());
  if (defaults.branching.focused_phase_switch_span != 2)
    return 1;
  const std::vector<int8_t> focused_phases{0, 1, 0, -1, 0, 0, 0, 0};
  if (defaults.branching.focused_phase_pattern != focused_phases)
    return 1;

  assert(defaults.analysis.small_clause_limit == 100000);
  assert(defaults.analysis.big_binary_fraction_permille == 990);
  assert(defaults.analysis.conflict_reuse && defaults.analysis.jump_reasons);
  assert(defaults.analysis.shrink_glue_promote &&
         defaults.analysis.bump_extension_reasons);
  assert(defaults.inprocess.bve_enabled &&
         defaults.inprocess.bve_permille == 100);
  assert(defaults.inprocess.bve_xor_arity_limit == 5);
  assert(defaults.inprocess.bve_resolvent_size_limit == 100);
  assert(defaults.inprocess.bve_backward_occurrence_limit == 100);
  assert(defaults.inprocess.bve_definition_budget == 1000000);
  assert(defaults.inprocess.bve_definition_cores == 2);
  assert(defaults.inprocess.bve_definition_core_budget_multiplier == 10);
  assert(!defaults.inprocess.bve_on_the_fly_strengthening);
  assert(defaults.inprocess.initial_fast_elimination);
  assert(defaults.inprocess.initial_fast_elimination_rounds == 4);
  const auto &fastel =
      defaults.inprocess.initial_fast_elimination_parameters;
  assert(fastel.gate_mask == 0);
  assert(fastel.growth_bound == UINT32_MAX);
  assert(fastel.occurrence_limit == 100);
  assert(fastel.clause_size_limit == 100);
  assert(fastel.resolvent_count_limit == 8);
  assert(fastel.resolvent_size_limit == UINT32_MAX);
  assert(!fastel.backward);
  assert(fastel.traversal == strategy::BveTraversal::FastEliminate);
  assert(defaults.inprocess.factor_repeated_enabled &&
         defaults.inprocess.factor_permille == 50 &&
         defaults.inprocess.factor_active_ceiling == 0);
  assert(!defaults.inprocess.cadical_arena_ordering_enabled &&
         "production must retain Kissat creation order");
  assert(defaults.inprocess.subsume_forward_permille == 100);
  assert(defaults.inprocess.vivify_enabled &&
         defaults.inprocess.vivify_permille == 100);
  std::string validation_error;
  assert(defaults.valid(&validation_error) && validation_error.empty());
  using Mutation = void (*)(DefaultStrategyConfig &);
  static const Mutation invalid_mutations[] = {
      [](DefaultStrategyConfig &c) { c.inprocess.vivify_total_share = 0; },
      [](DefaultStrategyConfig &c) { c.inprocess.program[0].effort_permille = 1001; },
      [](DefaultStrategyConfig &c) {
        c.inprocess.vivify_irredundant_productivity_percent = 101;
      },
      [](DefaultStrategyConfig &c) { c.branching.focused_phase_pattern.clear(); },
      [](DefaultStrategyConfig &c) { c.inprocess.bve_bounds.clear(); },
      [](DefaultStrategyConfig &c) {
        c.inprocess.bve_bounds = {3, 4};
        c.inprocess.bve_default_ceiling = 2;
      },
      [](DefaultStrategyConfig &c) { c.rephase.rotation.clear(); },
      [](DefaultStrategyConfig &c) {
        c.rephase.rotation[0] = static_cast<DefaultRephaseSlot>(99);
      },
      [](DefaultStrategyConfig &c) {
        c.rephase.walk_inapplicable_fallback = DefaultRephaseSlot::Walk;
      },
      [](DefaultStrategyConfig &c) {
        c.rephase.walk_score_curve[1].average_clause_size_milli = 0;
      },
      [](DefaultStrategyConfig &c) { c.reorder.parameters.clause_decay_milli = 0; },
      [](DefaultStrategyConfig &c) {
        c.reorder.target = static_cast<DefaultReorderTarget>(99);
      },
      [](DefaultStrategyConfig &c) { c.reduction.reduce_tier2_mode = 2; },
      [](DefaultStrategyConfig &c) { c.reduction.victim_denominator = 0; },
      [](DefaultStrategyConfig &c) {
        c.reduction.victim_numerator = 5;
        c.reduction.victim_denominator = 4;
      },
      [](DefaultStrategyConfig &c) { c.inprocess.vivify_tier_mode = 2; },
      [](DefaultStrategyConfig &c) {
        c.analysis.eager_subsume =
            strategy::AnalyzeParameters::EAGER_SUBSUME_CAPACITY + 1;
      },
      [](DefaultStrategyConfig &c) {
        c.inprocess.structured_root_stages[0].on_budget_exhaustion =
            static_cast<strategy::BudgetExhaustionPolicy>(99);
      },
  };
  for (const Mutation mutate : invalid_mutations) {
    DefaultStrategyConfig invalid = defaults;
    mutate(invalid);
    validation_error.clear();
    assert(!invalid.valid(&validation_error) && !validation_error.empty());
  }
  {
    DefaultStrategyConfig invalid = defaults;
    invalid.inprocess.program.push_back(
        {900, "SubstitutionRedundancy",
         strategy::InprocessPhase::Probe, true, true});
    assert(!invalid.valid(&validation_error));
    assert(validation_error ==
           "slot program duplicates structured root SR ownership");
    invalid = defaults;
    invalid.inprocess.structured_root_stages[0].program.sequence[0].rule = "Sweep";
    assert(!invalid.valid(&validation_error));
    assert(validation_error == "parameters do not match rule");
    invalid = defaults;
    invalid.inprocess.structured_root_stages[0].program.sequence[0].repeat =
        {strategy::RepeatMode::Count, 2};
    assert(invalid.valid(&validation_error));
  }
  for (const double invalid_percent :
       {-1.0, 100.01, std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN()}) {
    DefaultStrategyConfig invalid_low = defaults;
    invalid_low.reduction.low_percent = invalid_percent;
    assert(!invalid_low.valid(&validation_error));
    DefaultStrategyConfig invalid_high = defaults;
    invalid_high.reduction.high_percent = invalid_percent;
    assert(!invalid_high.valid(&validation_error));
  }
  {
    // Kissat accepts both endpoints independently; when low is at least
    // high, the reduction fraction stays at low.
    DefaultStrategyConfig reverse = defaults;
    reverse.reduction.low_percent = 70.0;
    reverse.reduction.high_percent = 30.0;
    assert(reverse.valid(&validation_error));
    GlueTieredReduction reduction(&reverse.reduction);
    reduction.ramp = true;
    Kernel k;
    k.stats.reductions = 99;
    assert(reduction.victims(k, 100) == 70);

    reverse.reduction.ramp_fraction = false;
    reverse.reduction.victim_numerator = 2;
    reverse.reduction.victim_denominator = 5;
    GlueTieredReduction fixed(&reverse.reduction);
    assert(fixed.victims(k, 101) == 40);
  }

  {
    DefaultStrategyConfig typed = defaults;
    typed.inprocess.bve_backward = true;
    typed.inprocess.bve_on_the_fly_strengthening = false;
    typed.inprocess.bce_parameters.occurrence_limit = 11;
    typed.inprocess.ternary_parameters = {12, 13};
    typed.inprocess.factor_parameters = {4, 5, 6, true, false};
    typed.inprocess.transitive_parameters = {false, false, true};
    typed.inprocess.substitute_parameters = {true};
    typed.inprocess.vivify_parameters = {3, false, true, 14};
    Kernel posed;
    kernel_reserve_vars(posed, 1);
    DefaultPolicy typed_policy(typed);
    typed_policy.inprocess().build(posed);
    assert(posed.bve_backward && !posed.bve_on_the_fly_strengthening &&
           posed.bce_occ_limit == 11);
    assert(posed.ternary_occ_limit == 12 &&
           posed.ternary_max_add_percent == 13);
    assert(posed.factor_size == 4 && posed.factor_bound == 5 &&
           posed.factor_cand_rounds == 6 && posed.factor_composite &&
           !posed.factor_demote_fresh);
    assert(!posed.transitive_early_exit && !posed.transitive_live_meter &&
           posed.transitive_budget_checked && posed.subst_demote);
    assert(posed.vivify_scope == 3 && !posed.vivify_reuse &&
           posed.vivify_flush && posed.vivify_irredundant_sort_ratio == 14);
  }

  DefaultPolicy classifier;
  const auto at_limit = classifier.classify_formula_counts(100000, 0);
  assert(at_limit.small && !at_limit.bigbig);
  const auto above_below_fraction =
      classifier.classify_formula_counts(990, 100001 - 990);
  assert(!above_below_fraction.small && !above_below_fraction.bigbig);
  const auto above_at_fraction =
      classifier.classify_formula_counts(991, 100001 - 991);
  assert(!above_at_fraction.small && above_at_fraction.bigbig);

  // Kissat's `classification.small` is write-only and `bigbig` only feeds fast
  // binary assignment, so classification mutates no typed strategy field.
  const std::string production_snapshot = classifier.strategy_config_snapshot();
  static const uint64_t GRID_VARS[] = {1, 128, 862, 1042, 6360, 126152,
                                       3000000, 10000000};
  static const uint64_t GRID_RATIOS[] = {0, 1, 4, 10, 50, 500, 1000};
  for (const uint64_t vars : GRID_VARS)
    for (const uint64_t ratio : GRID_RATIOS) {
      const uint64_t clauses = vars * ratio;
      (void)classifier.classify_formula_counts(clauses / 2,
                                               clauses - clauses / 2);
      assert(classifier.strategy_config_snapshot() == production_snapshot &&
             "formula size/density must not change production strategy");
    }

  // Classification neither activates nor erases explicit typed overrides.
  DefaultStrategyConfig diagnostic = defaults;
  diagnostic.inprocess.bve_enabled = false;
  diagnostic.inprocess.factor_repeated_enabled = false;
  diagnostic.inprocess.bve_permille = 7;
  diagnostic.inprocess.factor_permille = 11;
  diagnostic.inprocess.subsume_forward_permille = 13;
  diagnostic.inprocess.vivify_enabled = false;
  diagnostic.inprocess.vivify_permille = 17;
  diagnostic.inprocess.forward_midround_budget = true;
  diagnostic.inprocess.cadical_arena_ordering_enabled = true;
  DefaultPolicy explicit_arm(diagnostic);
  const std::string diagnostic_snapshot = explicit_arm.strategy_config_snapshot();
  (void)explicit_arm.classify_formula_counts(1000000, 1);
  assert(explicit_arm.strategy_config_snapshot() == diagnostic_snapshot);

  // Kissat rotates two focused epochs through forced initial/opposite phases,
  // for ranked and random decisions alike.
  DefaultStrategyConfig no_random = defaults;
  no_random.branching.random_sequences = false;
  Kernel phase;
  ScopedHeuristics phase_heuristics(phase);
  kernel_reserve_vars(phase, 1);
  DefaultBranching phase_branching(&no_random.branching);
  phase.h->saved[0] = -1;
  phase.stats.switched = 2;
  if (phase_branching.pick(phase) != internalize(1))
    return 1;
  phase.stats.switched = 4;
  if (phase_branching.pick(phase) != internalize(-1))
    return 1;
  phase.h->saved[0] = 1;
  phase.stats.switched = 6;
  if (phase_branching.pick(phase) != internalize(-1))
    return 1;

  DefaultStrategyConfig variable_sequences = defaults;
  variable_sequences.branching.random_sequences = false;
  variable_sequences.branching.focused_phase_switch_span = 1;
  variable_sequences.branching.focused_phase_pattern = {-1, 1, 0};
  DefaultBranching variable_phase(&variable_sequences.branching);
  phase.h->saved[0] = -1;
  phase.stats.switched = 0;
  assert(variable_phase.pick(phase) == internalize(-1));
  phase.stats.switched = 1;
  assert(variable_phase.pick(phase) == internalize(1));
  phase.stats.switched = 2;
  assert(variable_phase.pick(phase) == internalize(-1));
  variable_sequences.inprocess.bve_bounds = {2, 5, 9};
  variable_sequences.inprocess.bve_default_ceiling = 9;
  DefaultBveBoundRamp variable_ramp(&variable_sequences.inprocess);
  assert(variable_ramp.num_passes() == 3 && variable_ramp.bound(0) == 2 &&
         variable_ramp.bound(1) == 5 && variable_ramp.bound(2) == 9);

  // Kissat's first seed-0 LCG value selects internal variable 254 of 260.
  Kernel random;
  ScopedHeuristics random_heuristics(random);
  kernel_reserve_vars(random, 260);
  random.stats.conflicts = 855;
  DefaultBranching reference_random(&defaults.branching);
  reference_random.reset(0);
  const Lit random_lit = reference_random.pick(random);
  if (kernel_externalize(random, random_lit) != 255 ||
      random.stats.random_sequences != 1 ||
      random.stats.random_decisions != 1)
    return 1;
  // Kissat counts the restart-reuse preview draw; the decision gets draw two.
  if (kernel_externalize(random, reference_random.pick(random)) != 130 ||
      random.stats.random_decisions != 2)
    return 1;

  Kernel tumbled;
  ScopedHeuristics tumbled_heuristics(tumbled);
  kernel_reserve_vars(tumbled, 3);
  // Under Kissat's tumble, compact indices follow import order and
  // queue/score seeding follows activation order.
  kernel_record_import(tumbled, 1);
  kernel_record_import(tumbled, 2);
  kernel_record_import(tumbled, 0);
  heuristics_seed_activation_order(tumbled, {2, 0, 1});
  if (tumbled.compact_import_order != std::vector<Var>({1, 2, 0}))
    return 1;
  tumbled.stats.conflicts = 855;
  DefaultBranching tumbled_random(&defaults.branching);
  tumbled_random.reset(0);
  // First seed-0 index is 1 modulo 3, the second compact import (variable 2).
  if (kernel_externalize(tumbled, tumbled_random.pick(tumbled)) != 3)
    return 1;

  // Parsing before H is attached keeps the tumble population: header-only
  // variables do not change Kissat's random-index modulus.
  Kernel late_tumbled;
  kernel_reserve_vars(late_tumbled, 5);
  late_tumbled.input_activation_order = {0, 2, 4};
  {
    ScopedHeuristics late_heuristics(late_tumbled);
    if (late_tumbled.compact_import_order != std::vector<Var>({0, 2, 4}) ||
        !late_tumbled.input_activation_order.empty())
      return 1;
  }

  Kernel compacted_random;
  ScopedHeuristics compacted_heuristics(compacted_random);
  kernel_reserve_vars(compacted_random, 5);
  heuristics_seed_activation_order(compacted_random, {2, 0, 4, 1, 3});
  kernel_assign_unit(compacted_random, make_lit(0, false));
  kernel_assign_unit(compacted_random, make_lit(1, true));
  heuristics_compact_random_order_fixed(compacted_random);
  if (compacted_random.compact_import_order !=
          std::vector<Var>({0, 2, 3, 4}) ||
      compacted_heuristics.h.fixed_representative != 0)
    return 1;
  // A later root unit stays in the random-index population but, like the
  // fixed compacted representative, is excluded from the first stable heap.
  kernel_assign_unit(compacted_random, make_lit(3, false));
  heuristics_update_scores(compacted_random);
  if (compacted_random.compact_import_order !=
          std::vector<Var>({0, 2, 3, 4}) ||
      heuristics_heap_contains(compacted_random, 0) ||
      heuristics_heap_contains(compacted_random, 3))
    return 1;

  // `kissat_decide` increments the level before the random-sequence deadline
  // check, so a reused level-one prefix is Kissat level two and stays ranked.
  Kernel reused;
  ScopedHeuristics reused_heuristics(reused);
  kernel_reserve_vars(reused, 501);
  reused.level = 1;
  reused.stats.conflicts = 855;
  DefaultBranching reused_random(&defaults.branching);
  reused_random.reset(0);
  if (kernel_externalize(reused, reused_random.pick(reused)) != 501 ||
      reused.stats.random_sequences != 0 ||
      reused.stats.random_decisions != 0)
    return 1;

  // Restart reuse selects without adding a frame: at retained level one an
  // overdue random sequence starts and consumes one draw.
  DefaultBranching restart_reused_random(&defaults.branching);
  restart_reused_random.reset(0);
  const Lit restart_reused_choice =
      restart_reused_random.pick_for_restart_reuse(reused);
  if (reused.stats.random_sequences != 1 ||
      reused.stats.random_decisions != 1 ||
      restart_reused_random.randec_remaining != 10 ||
      restart_reused_choice == INVALID_LIT)
    return 1;

  // Kissat rejects the same due sequence above retained level one.
  Kernel deep_reused;
  ScopedHeuristics deep_reused_heuristics(deep_reused);
  kernel_reserve_vars(deep_reused, 3);
  deep_reused.level = 2;
  deep_reused.stats.conflicts = 855;
  DefaultBranching deep_reused_random(&defaults.branching);
  deep_reused_random.reset(0);
  (void)deep_reused_random.pick_for_restart_reuse(deep_reused);
  if (deep_reused.stats.random_sequences != 0 ||
      deep_reused.stats.random_decisions != 0)
    return 1;

  // Formula size adds no admission path: a small universe at level one stays
  // ranked.
  Kernel compact_reused;
  ScopedHeuristics compact_reused_heuristics(compact_reused);
  kernel_reserve_vars(compact_reused, 3);
  compact_reused.level = 1;
  compact_reused.stats.conflicts = 855;
  DefaultBranching compact_reused_random(&defaults.branching);
  compact_reused_random.reset(0);
  if (kernel_externalize(compact_reused,
                         compact_reused_random.pick(compact_reused)) != 3 ||
      compact_reused.stats.random_sequences != 0 ||
      compact_reused.stats.random_decisions != 0)
    return 1;

  // A stable->focused transition starts a sequence even above decision level
  // one and before the ordinary randec conflict deadline.
  Kernel switched;
  ScopedHeuristics switched_heuristics(switched);
  kernel_reserve_vars(switched, 3);
  switched.level = 12;
  switched.stats.conflicts = 1844;
  DefaultBranching switched_random(&defaults.branching);
  switched_random.reset(1500); // ordinary deadline 2000: not due yet
  switched_random.start_random_sequence(switched);
  if (switched.stats.random_sequences != 1 ||
      switched_random.randec_remaining != 10)
    return 1;

  // Kissat charges random-sequence lifetime only for search-propagation
  // conflicts, not for OTFS conflicts added in analysis.
  switched_random.on_search_propagation_conflict();
  switched.stats.conflicts++;
  if (switched_random.randec_remaining != 9)
    return 1;
  switched.stats.conflicts++; // analysis/OTFS conflict: deliberately uncharged
  if (switched_random.randec_remaining != 9)
    return 1;
  for (unsigned i = 0; i != 8; ++i)
    switched_random.on_search_propagation_conflict();
  if (switched_random.randec_remaining != 1)
    return 1;
  const uint64_t random_decisions_before = switched.stats.random_decisions;
  (void)switched_random.pick(switched);
  if (switched.stats.random_decisions != random_decisions_before + 1)
    return 1;
  switched_random.on_search_propagation_conflict();
  (void)switched_random.pick(switched);
  if (switched_random.randec_remaining ||
      switched.stats.random_decisions != random_decisions_before + 1)
    return 1;

  DefaultStrategyConfig custom = defaults;
  custom.reduction.initial_interval = 37;
  custom.mode.initial_conflicts = 41;
  custom.rephase.initial_interval = 43;
  custom.reorder.initial_interval = 47;
  custom.analysis.chronological_levels = 53;
  custom.inprocess.probe_permille = 59;
  custom.inprocess.sweep_parameters.max_env_vars = 67;
  custom.inprocess.factor_active_ceiling = 71;
  custom.inprocess.sweep_parameters.skip_embedded_fixed = false;
  custom.inprocess.sweep_parameters.resume = false;
  custom.inprocess.sweep_parameters.force_general_proof = true;
  custom.rephase.rotation = {DefaultRephaseSlot::Original,
                             DefaultRephaseSlot::Walk};
  custom.rephase.walk_even_break_base_milli = 3100;
  custom.rephase.walk_warmup = false;
  custom.rephase.walk_use_branch_seed = false;
  custom.rephase.walk_random_seed = 313;
  custom.rephase.walk_inapplicable_fallback =
      DefaultRephaseSlot::Original;
  custom.reorder.target = DefaultReorderTarget::Queue;
  custom.reorder.parameters = {17, 400, 900, 1700};
  custom.reduction.reduce_tier1_mode = 1;
  custom.reduction.reduce_tier2_mode = 0;
  custom.reduction.victim_numerator = 2;
  custom.reduction.victim_denominator = 5;
  custom.reduction.protect_tiers = false;
  custom.reduction.canonical_compact_collection = true;
  custom.inprocess.vivify_tier_mode = 1;
  DefaultPolicy policy(custom);
  Kernel k;
  ScopedHeuristics heuristics(k);
  policy.reset_schedule(k);
  assert(policy.reduce.next_conflicts == 37);
  assert(policy.mode.conflicts_limit == 41);
  assert(policy.rephase.next_conflicts == 43);
  assert(policy.reorder.next_conflicts == 47);
  assert(policy.chrono_levels == 53);
  assert(policy.current_rephase_slot() == DefaultRephaseSlot::Original);
  policy.rephase.count = 1;
  assert(policy.current_rephase_slot() == DefaultRephaseSlot::Walk);
  const std::string snapshot = policy.strategy_config_snapshot();
  for (const char *setting :
       {"reduction.initial_interval=37", "inprocess.probe_permille=59",
        "inprocess.sweep_vars=67", "inprocess.factor_active_ceiling=71",
        "inprocess.sweep_skip_embedded_fixed=0", "inprocess.sweep_resume=0",
        "inprocess.sweep_force_general_proof=1",
        "rephase.walk_even_break_base_milli=3100", "rephase.rotation0=3",
        "rephase.walk_warmup=0", "rephase.walk_random_seed=313",
        "reorder.target=1", "reduction.reduce_tier1_mode=1",
        "reduction.victim_numerator=2", "reduction.victim_denominator=5",
        "reduction.protect_tiers=0",
        "reduction.canonical_compact_collection=1",
        "inprocess.vivify_tier_mode=1", "reorder.maximum_clause_size=17"})
    assert(snapshot.find(setting) != std::string::npos);
  assert(policy.solve(k) == 10);
  assert(k.walk_even_break_base_milli == 3100 &&
         k.reorder_parameters.maximum_clause_size == 17 &&
         k.reorder_parameters.clause_decay_milli == 400 &&
         k.reorder_parameters.binary_weight_milli == 900 &&
         k.reorder_parameters.balanced_polarity_milli == 1700 &&
         k.reduce_victim_numerator == 2 &&
         k.reduce_victim_denominator == 5 && !k.reduce_protect_tiers &&
         k.reduce_compact_collection);

  // Kissat seeds the first reduce deadline from `reduceinit` under either
  // cadence.
  custom.reduction.square_root_cadence = false;
  custom.reduction.arithmetic_interval = 91;
  DefaultPolicy arithmetic(custom);
  arithmetic.reset_schedule(k);
  assert(arithmetic.reduce.next_conflicts == 37);

  // The configured reluctant period governs the first cycle too.
  custom.restart.reluctant_period = 3;
  DefaultPolicy short_restart(custom);
  short_restart.reset_schedule(k);
  assert(short_restart.reluctant.wait == 3);
  short_restart.tick_reluctant();
  short_restart.tick_reluctant();
  assert(!short_restart.reluctant.trigger);
  short_restart.tick_reluctant();
  assert(short_restart.reluctant.trigger);

  // Environment arms overlay the typed profile and appear in its readback.
  setenv("TACTIX_BUMP_REASONS_RATE", "17", 1);
  setenv("TACTIX_WALK_PERMILLE", "61", 1);
  Kernel one;
  kernel_reserve_vars(one, 1);
  DefaultPolicy overlaid;
  assert(overlaid.solve(one) == 10);
  assert(overlaid.strategy_config().analysis.bump_reason_rate == 17.0);
  assert(overlaid.strategy_config().rephase.walk_permille == 61);
  unsetenv("TACTIX_BUMP_REASONS_RATE");
  unsetenv("TACTIX_WALK_PERMILLE");

  // Kissat recomputes the current mode's tier snapshot at every Reduce; the
  // scheduled retier is far away so each recomputation belongs to a Reduce.
  DefaultStrategyConfig cadence = defaults;
  cadence.reduction.retier_initial_interval = 1ull << 40;
  cadence.inprocess.structured_root_stages.clear();
  // Startup elimination would settle the formula before search.
  cadence.inprocess.initial_fast_elimination = false;
  Kernel php;
  ScopedHeuristics php_heuristics(php);
  add_php(php, 4, 3);
  DefaultPolicy scheduled_only(cadence);
  scheduled_only.reset_schedule_per_solve = false;
  scheduled_only.reduce.next_conflicts = 0;
  assert(scheduled_only.solve(php) == 20);
  assert(php.stats.reductions > 0);
  assert(php.stats.retiered == php.stats.reductions &&
         "every Reduce must refresh exactly one current-mode tier snapshot");
}
