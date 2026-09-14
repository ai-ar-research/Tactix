#include "strategy/random_online.hpp"

#include <algorithm>
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

ActionParameters random_parameters(uint64_t &state, const std::string &rule,
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
    if (bounded(state, 2)) {
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
    const uint64_t producer = bounded(state, 3);
    p.orbitopal_producer = producer == 1;
    p.general_producer = producer == 2;
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
  } else if (tuple(text, "bve", v) && v.size() == 11) {
    BveParameters p;
    U32_AT(0, p.gate_mask); U32_AT(1, p.growth_bound);
    U32_AT(2, p.occurrence_limit); U32_AT(3, p.clause_size_limit);
    U32_AT(4, p.xor_arity_limit); U32_AT(5, p.resolvent_size_limit);
    U32_AT(6, p.backward_occurrence_limit); p.definition_budget = v[7];
    U32_AT(8, p.definition_cores);
    U32_AT(9, p.definition_core_budget_multiplier);
    if (!boolean(v[10], p.backward)) return false;
    parameters = p;
  } else if (tuple(text, "c1", v) && v.size() == 3) {
    SubsumeForwardParameters p;
    U32_AT(0, p.clause_size_limit); U32_AT(1, p.large_occurrence_limit);
    U32_AT(2, p.binary_occurrence_limit); parameters = p;
  } else if (tuple(text, "c2", v) && v.size() == 2) {
    StrengthenParameters p;
    U32_AT(0, p.clause_size_limit); U32_AT(1, p.occurrence_limit);
    parameters = p;
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
  } else if (tuple(text, "vivify", v) && v.size() == 4) {
    VivifyParameters p;
    if (v[0] > UINT8_MAX) return false;
    p.scope = static_cast<uint8_t>(v[0]);
    if (!boolean(v[1], p.reuse) || !boolean(v[2], p.flush)) return false;
    U32_AT(3, p.irredundant_sort_ratio); parameters = p;
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
  } else if (tuple(text, "substitute", v) && v.size() == 1) {
    SubstituteParameters p;
    if (!boolean(v[0], p.demote_substituted)) return false;
    parameters = p;
  } else if (tuple(text, "reduce", v) &&
             (v.size() == 2 || v.size() == 6)) {
    ReduceParameters p; U32_AT(0, p.tier1); U32_AT(1, p.tier2);
    if (v.size() == 6) {
      U32_AT(2, p.victim_numerator); U32_AT(3, p.victim_denominator);
      if (!boolean(v[4], p.protect_tiers) ||
          !boolean(v[5], p.compact_collection)) return false;
    }
    parameters = p;
  } else if (tuple(text, "walk", v) && v.size() >= 2) {
    WalkParameters p;
    p.step_budget = v[0];
    p.random_seed = v[1];
    if (v.size() != 2) {
      if (v.size() < 4 || v[2] > UINT32_MAX ||
          v[3] > (v.size() - 4) / 2 || v.size() != 4 + 2 * v[3])
        return false;
      p.even_break_base_milli = static_cast<uint32_t>(v[2]);
      p.score_curve.clear();
      for (size_t i = 0; i < v[3]; ++i) {
        const uint64_t x = v[4 + 2 * i], y = v[5 + 2 * i];
        if (x > UINT32_MAX || y > UINT32_MAX)
          return false;
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
  } else if (tuple(text, "parity-model", v) && v.size() == 7) {
    ParityModelParameters p;
    U32_AT(0, p.maximum_variables);
    U32_AT(1, p.maximum_groups);
    U32_AT(2, p.maximum_constraints);
    U32_AT(3, p.minimum_group_clauses);
    U32_AT(4, p.minimum_equations);
    U32_AT(5, p.maximum_free_variables);
    p.candidate_budget = v[6];
    parameters = p;
  } else if (tuple(text, "onehot-model", v) && v.size() == 8) {
    OnehotModelParameters p;
    U32_AT(0, p.maximum_variables);
    U32_AT(1, p.maximum_domains);
    U32_AT(2, p.maximum_constraints);
    U32_AT(3, p.maximum_equation_arity);
    U32_AT(4, p.minimum_domains);
    U32_AT(5, p.minimum_equations);
    U32_AT(6, p.maximum_free_domains);
    p.candidate_budget = v[7];
    parameters = p;
  } else if (tuple(text, "sr", v) && v.size() == 9) {
    SubstitutionRedundancyParameters p;
    p.maximum_validation_ticks = v[0];
    if (!boolean(v[1], p.orbitopal_producer))
      return false;
    U32_AT(2, p.minimum_orbitopal_rows);
    U32_AT(3, p.minimum_orbitopal_columns);
    p.maximum_orbitopal_matrices = v[4];
    p.maximum_orbitopal_instances = v[5];
    p.maximum_detection_ticks = v[6];
    if (!boolean(v[7], p.general_producer))
      return false;
    p.maximum_general_candidates_per_advance = v[8];
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
      !config.maximum_sr_validation_ticks || !config.maximum_sr_matrices ||
      !config.maximum_sr_instances || !config.maximum_sr_detection_ticks ||
      !config.maximum_sr_general_candidates_per_advance ||
      !config.sr_reference.valid())
    return reject("random-online maxima must be positive");

  std::vector<int> available;
  if (config.rules.empty()) {
    for (const RuleVTable &rule : tactix_rules())
      if (rule.guard(k))
        available.push_back(static_cast<int>(rule.id));
  } else {
    for (const std::string &name : config.rules) {
      const int id = tactix_rule_id(name.c_str());
      if (id < 0)
        return reject("random-online action is not registered");
      if (tactix_rules()[id].guard(k))
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
    step.parameters = random_parameters(rng, step.rule, config);
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

    std::vector<uint64_t> instances;
    std::vector<SubstitutionRedundancyInstance> sr_instances;
    InvocationStatus enumerated = InvocationStatus::Completed;
    if (step.rule == "SubstitutionRedundancy") {
      const auto &parameters =
          std::get<SubstitutionRedundancyParameters>(step.parameters);
      if (!parameters.general_producer) {
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
    const uint64_t granularity = whole_rule_only
                                     ? 2 + bounded(rng, 2)
                                     : bounded(rng, GRANULARITY_CHOICES);
    if (granularity == 0) {
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
      out << ":bve(" << p->gate_mask << ',' << p->growth_bound << ','
          << p->occurrence_limit << ',' << p->clause_size_limit << ','
          << p->xor_arity_limit << ',' << p->resolvent_size_limit << ','
          << p->backward_occurrence_limit << ',' << p->definition_budget << ','
          << p->definition_cores << ','
          << p->definition_core_budget_multiplier << ',' << p->backward
          << ')';
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
      out << ":vivify(" << unsigned(p->scope) << ',' << p->reuse << ','
          << p->flush << ',' << p->irredundant_sort_ratio << ')';
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
      out << ":substitute(" << p->demote_substituted << ')';
    else if (const auto *p =
                 std::get_if<ReduceParameters>(&step.parameters))
      out << ":reduce(" << p->tier1 << ',' << p->tier2 << ','
          << p->victim_numerator << ',' << p->victim_denominator << ','
          << p->protect_tiers << ',' << p->compact_collection << ')';
    else if (const auto *p = std::get_if<WalkParameters>(&step.parameters)) {
      out << ":walk(" << p->step_budget << ',' << p->random_seed << ','
          << p->even_break_base_milli << ',' << p->score_curve.size();
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
      out << ":parity-model(" << p->maximum_variables << ','
          << p->maximum_groups << ',' << p->maximum_constraints << ','
          << p->minimum_group_clauses << ',' << p->minimum_equations << ','
          << p->maximum_free_variables << ',' << p->candidate_budget << ')';
    else if (const auto *p =
                 std::get_if<OnehotModelParameters>(&step.parameters))
      out << ":onehot-model(" << p->maximum_variables << ','
          << p->maximum_domains << ',' << p->maximum_constraints << ','
          << p->maximum_equation_arity << ',' << p->minimum_domains << ','
          << p->minimum_equations << ',' << p->maximum_free_domains << ','
          << p->candidate_budget << ')';
    else if (const auto *p = std::get_if<SubstitutionRedundancyParameters>(
                 &step.parameters))
      out << ":sr(" << p->maximum_validation_ticks << ','
          << p->orbitopal_producer << ','
          << p->minimum_orbitopal_rows << ','
          << p->minimum_orbitopal_columns << ','
          << p->maximum_orbitopal_matrices << ','
          << p->maximum_orbitopal_instances << ','
          << p->maximum_detection_ticks << ',' << p->general_producer << ','
          << p->maximum_general_candidates_per_advance << ')';
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
  }
  return out.str();
}

bool deserialize_online_strategy_program(const std::string &encoded,
                                         OnlineStrategyProgram &out,
                                         std::string *error) {
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
    if (!text.empty()) {
      if (!text.starts_with(sr_marker) || text.back() != ']')
        return reject("online replay SR payload is malformed");
      text.remove_prefix(sr_marker.size());
      text.remove_suffix(1);
      while (!text.empty()) {
        const size_t semicolon = text.find(';');
        SubstitutionRedundancyInstance instance;
        if (!parse_sr_instance(text.substr(0, semicolon), instance))
          return reject("online replay SR instance is malformed");
        step.substitution_redundancy_instances.push_back(std::move(instance));
        if (semicolon == std::string_view::npos)
          text = {};
        else
          text.remove_prefix(semicolon + 1);
      }
    }
    parsed.sequence.push_back(std::move(step));
  }
  if (!input.eof())
    return reject("online replay contains unreadable data");
  if (!validate_online_strategy_program(parsed, error))
    return false;
  out = std::move(parsed);
  if (error)
    error->clear();
  return true;
}

} // namespace tactix::strategy
