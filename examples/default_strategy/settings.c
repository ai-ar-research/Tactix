/* The settings the default strategy reads: the session's default-strategy
 * configuration by snapshot key (tactix_strategy_integer/_real/_text). */
#include "strategy.h"

#include <stdio.h>
#include <stdlib.h>

uint64_t ds_integer_setting(tactix *s, const char *key) {
  uint64_t value;
  if (tactix_strategy_integer(s, key, &value)) {
    fprintf(stderr, "tactix-default: no integer setting %s\n", key);
    abort();
  }
  return value;
}

double ds_real_setting(tactix *s, const char *key) {
  double value;
  if (tactix_strategy_real(s, key, &value)) {
    fprintf(stderr, "tactix-default: no real setting %s\n", key);
    abort();
  }
  return value;
}

/* The number of settings `<prefix><i><suffix>` that exist, numbered from 0. */
static int indexed_settings(tactix *s, const char *prefix, const char *suffix) {
  char key[128];
  for (int i = 0;; ++i) {
    uint64_t integer;
    snprintf(key, sizeof key, "%s%d%s", prefix, i, suffix);
    if (tactix_strategy_integer(s, key, &integer) && tactix_strategy_text(s, key, NULL, 0) < 0)
      return i;
  }
}

void ds_read_settings(tactix *s, settings *c) {
#define I(group, key) c->group.key = ds_integer_setting(s, #group "." #key)
#define R(group, key) c->group.key = ds_real_setting(s, #group "." #key)
  I(branching, random_initial_interval);
  I(branching, random_sequence_length);
  I(branching, random_seed);
  R(branching, logarithm_offset);
  I(branching, focused_phase_switch_span);
  c->branching.focused_phases = (uint32_t)indexed_settings(s, "branching.focused_phase", "");
  for (uint32_t i = 0; i < c->branching.focused_phases; ++i) {
    char key[64];
    snprintf(key, sizeof key, "branching.focused_phase%u", i);
    c->branching.focused_phase[i] = (int)(int64_t)ds_integer_setting(s, key);
  }
  I(analysis, minimize_depth_limit);
  I(analysis, bump_reason_limit);
  R(analysis, bump_reason_rate);
  I(analysis, reason_depth);
  I(analysis, chronological_levels);
  I(analysis, conflict_reuse);
  I(analysis, jump_reasons);
  I(analysis, shrink_glue_promote);
  I(analysis, bump_extension_reasons);
  I(analysis, eager_subsume);
  I(analysis, small_clause_limit);
  I(analysis, big_binary_fraction_permille);
  R(restart, glue_margin);
  I(restart, initial_interval);
  R(restart, fast_ema_window);
  R(restart, slow_ema_window);
  R(restart, logarithm_offset);
  R(restart, logarithm_bias);
  I(restart, reluctant_period);
  I(restart, reluctant_limit);
  I(mode, initial_conflicts);
  I(mode, interval);
  R(mode, logarithm_offset);
  I(mode, logarithm_power);
  I(mode, force);
  I(reduction, initial_interval);
  I(reduction, arithmetic_interval);
  I(reduction, arithmetic_step);
  R(reduction, low_percent);
  R(reduction, high_percent);
  R(reduction, logarithm_offset);
  I(reduction, compact_inactive_percent);
  I(reduction, retier_initial_interval);
  I(reduction, retier_max_interval);
  I(reduction, retier_growth);
  I(reduction, tier1_default);
  I(reduction, tier2_default);
  I(reduction, tier1_relative_permille);
  I(reduction, tier2_relative_permille);
  I(reduction, reduce_tier1_mode);
  I(reduction, reduce_tier2_mode);
  I(reduction, victim_numerator);
  I(reduction, victim_denominator);
  I(reduction, protect_tiers);
  I(reduction, canonical_compact_collection);
  I(reduction, bookkeeping);
  I(reduction, square_root_cadence);
  I(reduction, ramp_fraction);
  I(reduction, dynamic_tiers);
  I(reduction, compact_variables);
  I(rephase, initial_interval);
  I(rephase, interval);
  R(rephase, logarithm_offset);
  I(rephase, logarithm_power);
  I(rephase, walk_permille);
  I(rephase, walk_inapplicable_fallback);
  I(rephase, walk_warmup);
  I(rephase, walk_use_branch_seed);
  I(rephase, walk_random_seed);
  I(rephase, configuration_checking_walk_permille);
  I(rephase, configuration_checking_minimum_step_budget);
  I(rephase, configuration_checking_conflict_interval);
  I(rephase, configuration_checking_minimum_clauses);
  I(rephase, complementary_ternary_minimum_permille);
  I(rephase, configuration_checking_maximum_tries);
  I(rephase, configuration_checking_random_seed);
  I(rephase, configuration_checking_flips_per_try);
  I(rephase, configuration_checking_weight_threshold);
  I(rephase, configuration_checking_smoothing_old_numerator);
  I(rephase, configuration_checking_smoothing_denominator);
  I(rephase, configuration_checking_aspiration);
  I(rephase, stable_only);
  I(rephase, late_lucky);
  c->rephase.rotations = (uint32_t)indexed_settings(s, "rephase.rotation", "");
  for (uint32_t i = 0; i < c->rephase.rotations; ++i) {
    char key[64];
    snprintf(key, sizeof key, "rephase.rotation%u", i);
    c->rephase.rotation[i] = (uint8_t)ds_integer_setting(s, key);
  }
  I(reorder, initial_interval);
  I(reorder, interval);
  I(reorder, mode);
  I(reorder, target);
#define E(model) c->enabled.model = ds_integer_setting(s, #model ".enabled")
  E(parity_model);
  E(onehot_model);
  E(balanced_signature_model);
  E(fermat_model);
  E(cerny_model);
  E(lookup_circuit_model);
  E(simon_model);
  E(stconn_proof);
  E(signed_cardinality_proof);
  E(exact_cover_model);
  E(independent_set_model);
  E(graph_labeling_model);
  E(ramsey_startup);
  E(startup_count);
  E(affine_root_closure);
  E(affine_search);
#undef E
  I(inprocess, preprocess_only);
  I(inprocess, initial_fast_elimination);
  I(inprocess, cadical_arena_ordering_enabled);
  I(inprocess, append_unlisted_rules);
  I(inprocess, structured_root_before_lucky);
  I(inprocess, structured_root_residual_search_handoff);
  I(inprocess, structured_root_backtracking_domain_limit);
  I(inprocess, structured_root_large_domain_limit);
  I(inprocess, eliminate_interval);
  I(inprocess, probe_interval);
  I(inprocess, minimum_effort_ticks);
  I(inprocess, preprocess_initial_ticks);
  I(inprocess, phase_rounds);
  R(inprocess, logarithm_offset);
  R(inprocess, scale_quadratic);
  R(inprocess, scale_constant);
  R(inprocess, scale_log_shift);
  I(inprocess, touch_filter);
  I(inprocess, likely_kept_redundant);
  I(inprocess, bve_occurrence_limit);
  I(inprocess, bve_clause_size_limit);
  I(inprocess, bve_xor_arity_limit);
  I(inprocess, bve_resolvent_size_limit);
  I(inprocess, bve_backward_occurrence_limit);
  I(inprocess, bve_definition_budget);
  I(inprocess, bve_definition_cores);
  I(inprocess, bve_definition_core_budget_multiplier);
  I(inprocess, bve_backward);
  I(inprocess, bve_on_the_fly_strengthening);
  I(inprocess, split_clocks);
  I(inprocess, watermarks);
  I(inprocess, hold_subsume_until_eliminate);
  I(inprocess, couple_factor_bound);
  I(inprocess, vivify_tier_mode);
  I(inprocess, bve_enabled);
  I(inprocess, vivify_enabled);
  I(inprocess, factor_enabled);
  I(inprocess, sweep_enabled);
  I(inprocess, factor_repeated_enabled);
  I(inprocess, factor_active_ceiling);
  I(inprocess, factor_initial_ticks);
  I(inprocess, factor_decade_round_cap);
  I(inprocess, factor_decade_cap);
  I(inprocess, factor_decade_offset);
  I(inprocess, factor_decade_base);
  I(inprocess, initial_fast_elimination_bve_permille);
  I(inprocess, substitute_rounds);
  I(inprocess, substitute_search_rounds);
  I(inprocess, closure_delay);
  I(inprocess, preprocess_followups);
  I(inprocess, forward_midround_budget);
  I(inprocess, vivify_irredundant_productivity_percent);
  I(inprocess, vivify_redundant_share);
  I(inprocess, vivify_total_share);
  I(inprocess, bve_default_ceiling);
  I(inprocess, initial_fast_elimination_rounds);
  I(inprocess, structured_root_sr_discovery_unlimited);
  I(inprocess, structured_root_sr_discovery_ticks);
  I(inprocess, structured_root_progress_feedback);
  I(inprocess, structured_root_large_domain_progress_feedback);
  I(inprocess, structured_root_progress_minimum_ticks);
  I(inprocess, structured_root_progress_ticks_per_variable);
  I(inprocess, structured_root_bce_after_unique_literal_amo);
  for (int i = 0; i < 3; ++i) {
    char key[64];
    snprintf(key, sizeof key, "inprocess.vivify_tier_share%d", i);
    c->inprocess.vivify_tier_share[i] = ds_integer_setting(s, key);
    snprintf(key, sizeof key, "inprocess.vivify_tier_scope%d", i);
    c->inprocess.vivify_tier_scope[i] = ds_integer_setting(s, key);
  }
  c->inprocess.bve_bounds = (uint32_t)indexed_settings(s, "inprocess.bve_bound", "");
  for (uint32_t i = 0; i < c->inprocess.bve_bounds && i < 16; ++i) {
    char key[64];
    snprintf(key, sizeof key, "inprocess.bve_bound%u", i);
    c->inprocess.bve_bound[i] = ds_integer_setting(s, key);
  }
#undef I
#undef R
  c->inprocess.program_steps = indexed_settings(s, "inprocess.step", "");
  c->inprocess.structured_stages[0] =
      indexed_settings(s, "inprocess.structured_root_stage", "");
  c->inprocess.structured_stages[1] =
      indexed_settings(s, "inprocess.structured_root_bounded_backtracking_stage", "");
  c->inprocess.structured_stages[2] =
      indexed_settings(s, "inprocess.structured_root_large_stage", "");
}
