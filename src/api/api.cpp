// C API implementation — v1, frozen at the phase-2 review (plan §5.1);
// post-v1 changes are additive only. Rename the public alias while parsing the
// header so its `tactix` typedef does not collide with the internal namespace;
// the underlying `struct tactix_solver` and C symbols remain identical.
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "frontend/parse.hpp"
#include "policies/heuristics.hpp"
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "diagnostics/rule_journal.hpp"
#include "proof/proof.hpp"
#include "rules/decide/decide.hpp"
#include "rules/registry.hpp"
#include "strategy/online_inprocess.hpp"

#include <memory>
#include <limits>
#include <utility>
#include <vector>

using namespace tactix;

struct tactix_solver {
  Kernel kernel;
  Heuristics heuristics; // the session owns H, Kernel holds a non-owning
                         // pointer. Outlives every solve(), so tactix_decide
                         // before any solve still has an H.
  Proof proof;
  std::vector<Lit> scratch;
  bool input_completed = false;
  // Additive and lazy: ordinary tactix_solve sessions retain the pre-online
  // object layout's allocation behavior and never construct program vectors.
  std::unique_ptr<strategy::OnlineInprocessExecutor> online;
  tactix_solver() {
    kernel.h = &heuristics;
    kernel.saved_phases = &heuristics.saved;
    // Match the CLI/reference default; rule-level fixtures using a bare Kernel
    // can still exercise the non-reuse path explicitly.
    kernel.conflict_reuse = true;
  }
};

namespace {

void external_state_change(tactix_solver *solver) {
  ++solver->kernel.strategy_epoch;
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

bool online_parameters(const tactix_online_parameters &source,
                       strategy::InprocessParameters &target) {
  switch (source.kind) {
  case TACTIX_ONLINE_PARAMETERS_INHERIT:
    target = std::monostate{};
    return true;
  case TACTIX_ONLINE_PARAMETERS_BVE: {
    const tactix_online_bve_parameters &p = source.value.bve;
    target = strategy::BveParameters{
        p.gate_mask, p.growth_bound, p.occurrence_limit,
        p.clause_size_limit, p.xor_arity_limit, p.resolvent_size_limit,
        p.backward_occurrence_limit, p.definition_budget,
        p.definition_cores, p.definition_core_budget_multiplier,
        p.backward != 0};
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
  case TACTIX_ONLINE_PARAMETERS_CONGRUENCE: {
    const tactix_online_congruence_parameters &p = source.value.congruence;
    if (p.gate_count_limit > std::numeric_limits<size_t>::max())
      return false;
    target = strategy::CongruenceParameters{
        p.and_arity_limit, static_cast<size_t>(p.gate_count_limit),
        p.xor_arity_limit};
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
        p.irredundant_sort_ratio};
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
        source.value.substitute.demote_substituted != 0};
    return true;
  case TACTIX_ONLINE_PARAMETERS_REDUCE: {
    const tactix_online_reduce_parameters &source_reduce =
        source.value.reduce;
    if (source_reduce.tier_protection_mode < TACTIX_ONLINE_SETTING_INHERIT ||
        source_reduce.tier_protection_mode > TACTIX_ONLINE_SETTING_ENABLE ||
        source_reduce.compact_collection_mode < TACTIX_ONLINE_SETTING_INHERIT ||
        source_reduce.compact_collection_mode > TACTIX_ONLINE_SETTING_ENABLE)
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
    target = strategy::ParityModelParameters{
        p.maximum_variables, p.maximum_groups, p.maximum_constraints,
        p.minimum_group_clauses, p.minimum_equations,
        p.maximum_free_variables, p.candidate_budget};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_ONEHOT_MODEL: {
    const tactix_online_onehot_model_parameters &p =
        source.value.onehot_model;
    target = strategy::OnehotModelParameters{
        p.maximum_variables, p.maximum_domains, p.maximum_constraints,
        p.maximum_equation_arity, p.minimum_domains, p.minimum_equations,
        p.maximum_free_domains, p.candidate_budget};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY: {
    const tactix_online_sr_parameters &p =
        source.value.substitution_redundancy;
    target = SubstitutionRedundancyParameters{
        p.maximum_validation_ticks, p.orbitopal_producer != 0,
        p.minimum_orbitopal_rows,
        p.minimum_orbitopal_columns, p.maximum_orbitopal_matrices,
        p.maximum_orbitopal_instances, p.maximum_detection_ticks,
        p.general_producer != 0,
        p.maximum_general_candidates_per_advance};
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
        !online_rule_work_budget(in.rule_budget_mode, in.rule_budget,
                                 out.rule_work_budget) ||
        !online_setting_override(in.touch_filter_mode,
                                 out.material.touch_filter) ||
        !online_setting_override(in.likely_kept_redundant_mode,
                                 out.material.likely_kept_redundant) ||
        !online_work_unit(in.work_unit, out.work_unit))
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

bool online_program_v2(const tactix_online_program_v2 &source,
                       const Kernel &kernel,
                       strategy::OnlineInprocessProgram &target) {
  if (source.step_count < 0 || (source.step_count && !source.steps))
    return false;
  std::vector<tactix_online_step> base_steps;
  base_steps.reserve(static_cast<size_t>(source.step_count));
  for (int i = 0; i < source.step_count; ++i)
    base_steps.push_back(source.steps[i].base);
  const tactix_online_program base_program = {
      base_steps.data(), source.step_count, source.repeat_mode,
      source.repeat_limit};
  if (!online_program(base_program, kernel, target))
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

} // namespace

extern "C" {

tactix_solver *tactix_init(void) { return new tactix_solver(); }

void tactix_release(tactix_solver *s) { delete s; }

void tactix_add(tactix_solver *s, int lit) {
  if (lit == 0) {
    ingest_clause(s->kernel, s->scratch);
    s->scratch.clear();
  } else {
    s->scratch.push_back(internalize(lit));
  }
  external_state_change(s);
}

// The C API has no end of file, so "the input is complete" is the first call
// that reasons about it. finish_input settles the proof id space (see
// frontend/parse.hpp), so every entry point below that could consume a derived
// id goes through here first. Idempotent, and free once nothing is pending.
// With a proof attached, adding clauses after this point is outside the v1
// contract: their ids would land above already-derived ones, which no checker's
// file numbering can match.
static void input_complete(tactix_solver *s) {
  finish_input(s->kernel);
  s->input_completed = true;
}

int tactix_proof(tactix_solver *s, const char *path, int format) {
  if (s->input_completed || s->kernel.proof)
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
  default:
    return -1;
  }
  if (!s->proof.open(path, f))
    return -1;
  s->kernel.proof = &s->proof;
  s->proof.bind_variable_map(&s->kernel.internal_to_external);
  external_state_change(s);
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
      and_arity, static_cast<size_t>(gate_count), xor_arity};
  if (uint64_t{parameters.gate_count_limit} != gate_count ||
      !parameters.valid())
    return -1;
  s->kernel.congruence_parameters = parameters;
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

int tactix_solve(tactix_solver *s) {
  input_complete(s);
  const int r = default_solve(s->kernel);
  rule_journal_close();
  if (r == 10)
    kernel_extend_model(s->kernel); // rebuild model through rho before reads
  external_state_change(s);
  return r;
}

int tactix_value(tactix_solver *s, int elit) {
  const Lit external = internalize(elit);
  if (var_of(external) >= kernel_num_output_vars(s->kernel))
    return -elit; // unmentioned variable: any value works
  const int8_t v = s->kernel.model_extended
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
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()))
    return -1;
  return static_cast<int>(rules[rule_id].kind);
}

int tactix_rules_applicable(tactix_solver *s, int *out, int cap) {
  input_complete(s);
  const auto &rules = tactix_rules();
  int n = 0;
  for (size_t i = 0; i < rules.size() && n < cap; i++)
    if (rules[i].guard(s->kernel))
      out[n++] = static_cast<int>(i);
  return n;
}

static int result_code(ApplyResult res) {
  switch (res) {
  case ApplyResult::Ok:
    return 0;
  case ApplyResult::NoOp:
    return 1;
  case ApplyResult::Conflict:
    return 2;
  case ApplyResult::Sat:
    return 10;
  case ApplyResult::Unsat:
    return 20;
  }
  return 1;
}

int tactix_apply(tactix_solver *s, int rule_id) {
  input_complete(s);
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()))
    return -1;
  const RuleVTable &r = rules[rule_id];
  if (!r.guard(s->kernel))
    return -1;
  const ApplyResult res = apply_rule(r, s->kernel);
  heuristics_discard_rule_events(s->kernel);
  kernel_check_invariants(s->kernel); // no-op in NDEBUG
  return result_code(res);
}

int tactix_rule_instances(tactix_solver *s, int rule_id, uint64_t *out,
                          int cap) {
  input_complete(s);
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()) || cap < 0)
    return -1;
  const RuleVTable &r = rules[rule_id];
  if (!r.guard(s->kernel))
    return -1;
  if (!r.instances) { // no per-application choice: canonical only
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
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()))
    return -1;
  const RuleVTable &r = rules[rule_id];
  if (!r.guard(s->kernel))
    return -1;
  // A rule with no instance surface has exactly one application, so a
  // non-canonical payload is an invalid argument. Running the canonical round
  // instead would be payload aliasing: the caller asked for one instance and
  // silently got a whole batch, with no way to notice
  // (tests/test_api_instances.c).
  if (!r.apply_instance && instance != INSTANCE_CANONICAL)
    return -1;
  ApplyResult res;
  if (instance == INSTANCE_CANONICAL || !r.apply_instance)
    res = apply_rule(r, s->kernel); // canonical (the only instance if no slot)
  else
    res = apply_rule_instance(r, s->kernel, instance);
  heuristics_discard_rule_events(s->kernel);
  kernel_check_invariants(s->kernel); // no-op in NDEBUG
  return result_code(res);
}

int tactix_rule_semantic_instances(tactix_solver *s, int rule_id,
                                   uint64_t *out, int cap) {
  input_complete(s);
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()) || cap < 0)
    return -1;
  const RuleVTable &r = rules[rule_id];
  if (!r.guard(s->kernel))
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
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()))
    return -1;
  const RuleVTable &r = rules[rule_id];
  if (!r.guard(s->kernel))
    return -1;
  const auto apply = r.apply_semantic_instance ? r.apply_semantic_instance
                                               : r.apply_instance;
  if (!apply && instance != INSTANCE_CANONICAL)
    return -1;
  ApplyResult result;
  if (instance == INSTANCE_CANONICAL || !apply)
    result = apply_rule(r, s->kernel);
  else if (r.apply_semantic_instance)
    result = apply_rule_semantic_instance(r, s->kernel, instance);
  else
    result = apply_rule_instance(r, s->kernel, instance);
  heuristics_discard_rule_events(s->kernel);
  kernel_check_invariants(s->kernel);
  return result_code(result);
}

int tactix_online_configure(tactix_solver *s,
                            const tactix_online_program *source,
                            uint64_t revision, int reconfigure_mode) {
  if (!s || !source ||
      (reconfigure_mode != TACTIX_ONLINE_RECONFIGURE_RESET &&
       reconfigure_mode != TACTIX_ONLINE_RECONFIGURE_RETAIN_BUDGETS))
    return -1;
  input_complete(s);
  strategy::OnlineInprocessProgram program;
  if (!online_program(*source, s->kernel, program))
    return -1;
  std::unique_ptr<strategy::OnlineInprocessExecutor> replacement;
  strategy::OnlineInprocessExecutor *executor = s->online.get();
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
    s->online = std::move(replacement);
  return 0;
}

int tactix_online_configure_v2(tactix_solver *s,
                               const tactix_online_program_v2 *source,
                               uint64_t revision, int reconfigure_mode) {
  if (!s || !source ||
      (reconfigure_mode != TACTIX_ONLINE_RECONFIGURE_RESET &&
       reconfigure_mode != TACTIX_ONLINE_RECONFIGURE_RETAIN_BUDGETS))
    return -1;
  input_complete(s);
  strategy::OnlineInprocessProgram program;
  if (!online_program_v2(*source, s->kernel, program))
    return -1;
  std::unique_ptr<strategy::OnlineInprocessExecutor> replacement;
  strategy::OnlineInprocessExecutor *executor = s->online.get();
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
    s->online = std::move(replacement);
  return 0;
}

int tactix_online_advance(tactix_solver *s, tactix_online_result *result) {
  if (!s || !result || !s->online)
    return -1;
  input_complete(s);
  const strategy::InvocationResult advanced = s->online->advance(s->kernel);
  heuristics_discard_rule_events(s->kernel);
  kernel_check_invariants(s->kernel);
  if (advanced.status == strategy::InvocationStatus::Sat) {
    kernel_extend_model(s->kernel, /*materialize_internal=*/false);
  }
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
  default:
    return UINT64_MAX;
  }
}

int tactix_decide_candidates(tactix_solver *s, int *lits, int cap) {
  if (cap <= 0)
    return 0;
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
  if (s->kernel.inconsistent)
    return 20;
  // SAT requires every active variable assigned at a propagation fixpoint,
  // with no conflict in flight. Retired, unassigned identities occur in no
  // live clause and kernel_extend_model supplies their external values. A
  // recorded conflict (conflict_valid) or an analyzed but not yet backjumped
  // one (pending.valid) still prevents a verdict.
  if (s->kernel.unassigned_active == 0 && !s->kernel.conflict_valid &&
      !s->kernel.pending.valid && kernel_propagation_fixpoint(s->kernel))
    return 10;
  return 0;
}

} // extern "C"
