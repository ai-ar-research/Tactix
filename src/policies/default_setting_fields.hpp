#pragma once

// Scalar spans shared by default-setting declarations and ordered snapshots.
// Rows preserve type, name, default and snapshot conversion. Context is the
// member prefix at the snapshot site; declarations ignore it. Non-scalar and
// differently ordered settings stay explicit in their owning class/visitor.
#define TACTIX_DECLARE_DEFAULT_SETTING(Context, Type, Name, Default, Cast) Type Name = Default;
#define TACTIX_DECLARE_DEFAULT_SETTINGS(Group) \
  TACTIX_DEFAULT_##Group(TACTIX_DECLARE_DEFAULT_SETTING, unused)
#define TACTIX_DEFAULT_SETTING_IDENTITY(Value) Value
#define TACTIX_DEFAULT_SETTING_UNSIGNED(Value) unsigned(Value)
#define TACTIX_SNAPSHOT_DEFAULT_SETTING(Context, Type, Name, Default, Cast) \
  (#Name, TACTIX_DEFAULT_SETTING_##Cast(Context.Name))
#define TACTIX_SNAPSHOT_DEFAULT_SETTINGS(Group, Context) \
  TACTIX_DEFAULT_##Group(TACTIX_SNAPSHOT_DEFAULT_SETTING, Context)

#define TACTIX_DEFAULT_BRANCHING(FIELD, Context) \
  FIELD(Context, uint64_t, random_initial_interval, RANDOM_INITIAL_INTERVAL_DEFAULT, IDENTITY) \
  FIELD(Context, uint64_t, random_sequence_length, RANDOM_SEQUENCE_LENGTH_DEFAULT, IDENTITY) \
  FIELD(Context, uint64_t, random_seed, RANDOM_SEED_DEFAULT, IDENTITY) \
  FIELD(Context, double, logarithm_offset, 9.0, IDENTITY) /* Kissat logarithmic schedule. */ \
  /* Focused decision-phase override indexed by (mode switches / switch span) */ \
  /* modulo the pattern length: zero selects the saved phase, +/-1 forces the */ \
  /* initial/opposite phase (decide.c). */ \
  FIELD(Context, uint32_t, focused_phase_switch_span, 2, IDENTITY)

#define TACTIX_DEFAULT_ANALYSIS(FIELD, Context) \
  FIELD(Context, uint32_t, minimize_depth_limit, strategy::AnalyzeParameters::MINIMIZE_DEPTH_DEFAULT, IDENTITY) \
  FIELD(Context, uint32_t, bump_reason_limit, BUMP_REASON_LIMIT_DEFAULT, IDENTITY) \
  FIELD(Context, double, bump_reason_rate, BUMP_REASON_RATE_DEFAULT, IDENTITY) \
  FIELD(Context, uint32_t, reason_depth, 1, IDENTITY) /* Kissat direct-reason traversal in both modes. */ \
  FIELD(Context, uint32_t, chronological_levels, CHRONOLOGICAL_LEVELS_DEFAULT, IDENTITY) \
  FIELD(Context, bool, conflict_reuse, true, IDENTITY) \
  FIELD(Context, bool, jump_reasons, true, IDENTITY) \
  FIELD(Context, bool, shrink_glue_promote, true, IDENTITY) \
  FIELD(Context, bool, bump_extension_reasons, true, IDENTITY) \
  FIELD(Context, uint32_t, eager_subsume, strategy::AnalyzeParameters::EAGER_SUBSUME_CAPACITY, IDENTITY) \
  FIELD(Context, uint64_t, small_clause_limit, SMALL_CLAUSE_LIMIT_DEFAULT, IDENTITY) \
  FIELD(Context, uint32_t, big_binary_fraction_permille, BIG_BINARY_FRACTION_PERMILLE_DEFAULT, IDENTITY)

#define TACTIX_DEFAULT_RESTART(FIELD, Context) \
  FIELD(Context, double, glue_margin, RESTART_MARGIN_DEFAULT, IDENTITY) \
  FIELD(Context, uint64_t, initial_interval, RESTART_INTERVAL_DEFAULT, IDENTITY) \
  FIELD(Context, double, fast_ema_window, FAST_EMA_WINDOW_DEFAULT, IDENTITY) \
  FIELD(Context, double, slow_ema_window, SLOW_EMA_WINDOW_DEFAULT, IDENTITY) \
  FIELD(Context, double, logarithm_offset, 9.0, IDENTITY) \
  FIELD(Context, double, logarithm_bias, 1.0, IDENTITY) \
  FIELD(Context, uint64_t, reluctant_period, RELUCTANT_PERIOD_DEFAULT, IDENTITY) \
  FIELD(Context, uint64_t, reluctant_limit, RELUCTANT_LIMIT_DEFAULT, IDENTITY)

#define TACTIX_DEFAULT_MODE(FIELD, Context) \
  FIELD(Context, uint64_t, initial_conflicts, MODE_INITIAL_CONFLICTS_DEFAULT, IDENTITY) \
  FIELD(Context, uint64_t, interval, MODE_INTERVAL_DEFAULT, IDENTITY) \
  FIELD(Context, double, logarithm_offset, 9.0, IDENTITY) \
  FIELD(Context, unsigned, logarithm_power, 4, IDENTITY) \
  FIELD(Context, uint32_t, force, 0, IDENTITY) /* 0 automatic, 1 focused, 2 stable. */

#define TACTIX_DEFAULT_REDUCTION(FIELD, Context) \
  FIELD(Context, uint64_t, initial_interval, REDUCE_INTERVAL_DEFAULT, IDENTITY) /* Kissat reduceinit. */ \
  FIELD(Context, uint64_t, arithmetic_interval, REDUCE_ARITHMETIC_DEFAULT, IDENTITY) \
  FIELD(Context, uint64_t, arithmetic_step, REDUCE_ARITHMETIC_STEP_DEFAULT, IDENTITY) \
  FIELD(Context, double, low_percent, 50.0, IDENTITY) /* Kissat reducelow. */ \
  FIELD(Context, double, high_percent, 90.0, IDENTITY) /* Kissat reducehigh. */ \
  FIELD(Context, double, logarithm_offset, 9.0, IDENTITY) \
  FIELD(Context, uint32_t, compact_inactive_percent, 10, IDENTITY) /* Kissat compactlim. */ \
  FIELD(Context, uint64_t, retier_initial_interval, 2, IDENTITY) \
  FIELD(Context, uint64_t, retier_max_interval, 1ull << 16, IDENTITY) \
  FIELD(Context, uint32_t, retier_growth, 2, IDENTITY) \
  FIELD(Context, uint32_t, tier1_default, 2, IDENTITY) /* Kissat tier1. */ \
  FIELD(Context, uint32_t, tier2_default, 6, IDENTITY) /* Kissat tier2. */ \
  FIELD(Context, uint32_t, tier1_relative_permille, 500, IDENTITY) /* Kissat tier1relative. */ \
  FIELD(Context, uint32_t, tier2_relative_permille, 900, IDENTITY) /* Kissat tier2relative. */ \
  FIELD(Context, uint8_t, reduce_tier1_mode, 0, UNSIGNED) /* focused tier-1 snapshot. */ \
  FIELD(Context, uint8_t, reduce_tier2_mode, 1, UNSIGNED) /* stable tier-2 snapshot. */ \
  FIELD(Context, uint32_t, victim_numerator, strategy::ReduceParameters::VICTIM_NUMERATOR_DEFAULT, IDENTITY) \
  FIELD(Context, uint32_t, victim_denominator, strategy::ReduceParameters::VICTIM_DENOMINATOR_DEFAULT, IDENTITY) \
  FIELD(Context, bool, protect_tiers, true, IDENTITY) \
  /* The profile for an explicit canonical Reduce action. */ \
  FIELD(Context, bool, canonical_compact_collection, false, IDENTITY) \
  FIELD(Context, bool, bookkeeping, true, IDENTITY) \
  FIELD(Context, bool, square_root_cadence, true, IDENTITY) \
  FIELD(Context, bool, ramp_fraction, true, IDENTITY) \
  FIELD(Context, bool, dynamic_tiers, true, IDENTITY) \
  FIELD(Context, bool, compact_variables, true, IDENTITY)

#define TACTIX_DEFAULT_REPHASE_CADENCE(FIELD, Context) \
  FIELD(Context, uint64_t, initial_interval, REPHASE_INTERVAL_DEFAULT, IDENTITY) \
  FIELD(Context, uint64_t, interval, REPHASE_INTERVAL_DEFAULT, IDENTITY) \
  FIELD(Context, double, logarithm_offset, 9.0, IDENTITY) \
  FIELD(Context, unsigned, logarithm_power, 3, IDENTITY) \
  FIELD(Context, uint64_t, walk_permille, WALK_PERMILLE_DEFAULT, IDENTITY) \
  FIELD(Context, uint32_t, walk_even_break_base_milli, strategy::WalkParameters::EVEN_BREAK_BASE_DEFAULT, IDENTITY)

#define TACTIX_DEFAULT_REPHASE_WALK(FIELD, Context) \
  FIELD(Context, bool, walk_warmup, true, IDENTITY) \
  FIELD(Context, bool, walk_use_branch_seed, true, IDENTITY) \
  FIELD(Context, uint64_t, walk_random_seed, 0, IDENTITY) \
  /* CCAnr/CryptoMiniSat configuration checking, chosen from structure. */ \
  FIELD(Context, uint64_t, configuration_checking_walk_permille, 1000, IDENTITY) \
  FIELD(Context, uint64_t, configuration_checking_minimum_step_budget, 200000000, IDENTITY) \
  FIELD(Context, uint64_t, configuration_checking_conflict_interval, 44000, IDENTITY) \
  FIELD(Context, uint32_t, configuration_checking_minimum_clauses, 1000, IDENTITY) \
  FIELD(Context, uint32_t, complementary_ternary_minimum_permille, 900, IDENTITY) \
  FIELD(Context, uint32_t, configuration_checking_maximum_tries, 100, IDENTITY) \
  FIELD(Context, uint64_t, configuration_checking_random_seed, 1, IDENTITY) \
  FIELD(Context, uint64_t, configuration_checking_flips_per_try, 1000000, IDENTITY) \
  FIELD(Context, uint32_t, configuration_checking_weight_threshold, 50, IDENTITY) \
  FIELD(Context, uint32_t, configuration_checking_smoothing_old_numerator, 3, IDENTITY) \
  FIELD(Context, uint32_t, configuration_checking_smoothing_denominator, 10, IDENTITY) \
  FIELD(Context, bool, configuration_checking_aspiration, true, IDENTITY) \
  FIELD(Context, bool, stable_only, true, IDENTITY) \
  FIELD(Context, bool, late_lucky, true, IDENTITY)

#define TACTIX_DEFAULT_AFFINE_ROOT(FIELD, Context) \
  FIELD(Context, bool, enabled, true, IDENTITY) \
  FIELD(Context, uint64_t, maximum_clauses, 12'000, IDENTITY) \
  FIELD(Context, uint64_t, maximum_snapshot_work, 1'000'000, IDENTITY) \
  FIELD(Context, uint64_t, maximum_recognition_work, 10'000'000, IDENTITY) \
  FIELD(Context, uint64_t, maximum_basis_work, 100'000'000, IDENTITY) \
  FIELD(Context, uint32_t, maximum_row_width, 68, IDENTITY) \
  FIELD(Context, uint64_t, maximum_proof_additions, 1'000'000, IDENTITY) \
  FIELD(Context, uint64_t, maximum_proof_deletions, 1'000'000, IDENTITY) \
  FIELD(Context, uint64_t, maximum_journal_bytes, 100'000'000, IDENTITY) \
  FIELD(Context, uint64_t, maximum_session_work, 100'000'000, IDENTITY)

#define TACTIX_DEFAULT_AFFINE_SEARCH(FIELD, Context) \
  FIELD(Context, uint32_t, maximum_rows, 100000, IDENTITY) \
  FIELD(Context, uint32_t, maximum_row_width, 100000, IDENTITY) \
  FIELD(Context, uint32_t, maximum_arity, 8, IDENTITY) /* Existing authenticated CNF recognizer capacity. */ \
  FIELD(Context, bool, autodisable, true, IDENTITY) \
  FIELD(Context, double, minimum_usefulness, 0.2, IDENTITY) \
  FIELD(Context, uint64_t, autodisable_minimum_calls, 200, IDENTITY) \
  FIELD(Context, uint64_t, autodisable_check_every, 1024, IDENTITY) \
  /* Zero inherits the preprocessing/search effort quantum. The */ \
  /* retained-source cap covers the whole matrix transaction; the 100 MB root */ \
  /* journal cap is below two CryptoMiniSat XOR families' 143--218 MB */ \
  /* preflight needs. */ \
  FIELD(Context, uint64_t, maximum_initialization_work, 0, IDENTITY) \
  FIELD(Context, uint64_t, maximum_service_work, 0, IDENTITY) \
  FIELD(Context, uint64_t, maximum_retained_bytes, 256'000'000, IDENTITY)

#define TACTIX_DEFAULT_INPROCESS_FLAGS(FIELD, Context) \
  FIELD(Context, bool, bve_enabled, true, IDENTITY) \
  FIELD(Context, bool, vivify_enabled, true, IDENTITY) \
  FIELD(Context, bool, factor_enabled, true, IDENTITY) \
  FIELD(Context, bool, sweep_enabled, true, IDENTITY) \
  FIELD(Context, bool, factor_repeated_enabled, true, IDENTITY) \
  FIELD(Context, bool, preprocess_only, false, IDENTITY) \
  /* SC2024's bounded startup pass composing forward subsumption and BVE. */ \
  FIELD(Context, bool, initial_fast_elimination, true, IDENTITY) \
  FIELD(Context, uint32_t, initial_fast_elimination_rounds, 4, IDENTITY)

#define TACTIX_DEFAULT_STRUCTURED_FEEDBACK(FIELD, Context) \
  FIELD(Context, bool, structured_root_progress_feedback, true, IDENTITY) \
  /* Above 50k variables the owner runs one generic generator-fixing scan. */ \
  FIELD(Context, bool, structured_root_large_domain_progress_feedback, false, IDENTITY) \
  FIELD(Context, uint64_t, structured_root_progress_minimum_ticks, 16ull << 20, IDENTITY) \
  FIELD(Context, uint64_t, structured_root_progress_ticks_per_variable, 16, IDENTITY) \
  /* Certified ULC completion publishes binary AMO helpers; after a later */ \
  /* structured consumer progresses, one BCE pass may retire them with normal */ \
  /* proof and reconstruction obligations. Helper-only progress keeps them. */ \
  FIELD(Context, bool, structured_root_bce_after_unique_literal_amo, true, IDENTITY)

#define TACTIX_DEFAULT_INPROCESS_CADENCE(FIELD, Context) \
  FIELD(Context, bool, append_unlisted_rules, true, IDENTITY) \
  FIELD(Context, uint64_t, eliminate_interval, INPROCESS_ELIMINATE_INTERVAL_DEFAULT, IDENTITY) \
  FIELD(Context, uint64_t, probe_interval, INPROCESS_PROBE_INTERVAL_DEFAULT, IDENTITY) \
  /* Plan 5.6 central clock for search-time Probe and Eliminate. Default 3, */ \
  /* events: a check opens when a deadline or an eligibility input changes, */ \
  /* which runs every pass where Kissat's per-decision check (0) would. */ \
  /* 1 fixed and 2 Luby conflict windows and 4 fixed decision windows */ \
  /* delay due passes. */ \
  FIELD(Context, uint32_t, inprocessing_clock, INPROCESSING_CLOCK_EVENT, IDENTITY) \
  /* Window base: Kissat's probeint, the finest stock inprocessing cadence. */ \
  FIELD(Context, uint64_t, inprocessing_clock_interval, INPROCESS_PROBE_INTERVAL_DEFAULT, IDENTITY) \
  /* After a root window above level zero, replay the valid decision prefix */ \
  /* instead of re-descending from the heap as Kissat's RootPreparation */ \
  /* backtrack does. Off by default: replay changes the search. */ \
  FIELD(Context, bool, inprocessing_clock_replay, false, IDENTITY) \
  /* Event clock: also open a check every K decisions (0: none). */ \
  FIELD(Context, uint64_t, inprocessing_decision_points, 0, IDENTITY) \
  FIELD(Context, uint64_t, minimum_effort_ticks, INPROCESS_MINIMUM_EFFORT_DEFAULT, IDENTITY) \
  FIELD(Context, uint64_t, preprocess_initial_ticks, INPROCESS_PREPROCESS_EFFORT_DEFAULT, IDENTITY) \
  FIELD(Context, uint32_t, phase_rounds, INPROCESS_PHASE_ROUNDS_DEFAULT, IDENTITY) \
  FIELD(Context, uint32_t, substitute_rounds, SUBSTITUTE_ROUNDS_DEFAULT, IDENTITY) \
  /* Search-time Substitute is one round; follow-ups use `substitute_rounds`. */ \
  FIELD(Context, uint32_t, substitute_search_rounds, 1, IDENTITY) \
  FIELD(Context, uint64_t, factor_initial_ticks, FACTOR_INITIAL_EFFORT_DEFAULT, IDENTITY) \
  FIELD(Context, double, logarithm_offset, 9.0, IDENTITY) \
  FIELD(Context, double, scale_quadratic, 4.5, IDENTITY) \
  FIELD(Context, double, scale_constant, 25.0, IDENTITY) \
  FIELD(Context, double, scale_log_shift, 5.0, IDENTITY) \
  FIELD(Context, uint64_t, candidate_order_seed, 0x9e3779b97f4a7c15ull, IDENTITY)

#define TACTIX_DEFAULT_INPROCESS_BOUNDS_AND_RATES(FIELD, Context) \
  FIELD(Context, uint32_t, bve_default_ceiling, 16, IDENTITY) \
  FIELD(Context, uint32_t, bve_occurrence_limit, 2000, IDENTITY) /* kissat eliminateocclim */ \
  FIELD(Context, uint32_t, bve_clause_size_limit, 100, IDENTITY) /* kissat eliminateclslim */ \
  FIELD(Context, uint32_t, bve_xor_arity_limit, strategy::BveParameters::XOR_ARITY_DEFAULT, IDENTITY) /* CaDiCaL elimxorlim */ \
  FIELD(Context, uint32_t, bve_resolvent_size_limit, strategy::BveParameters::RESOLVENT_SIZE_DEFAULT, IDENTITY) /* Kissat eliminateclslim */ \
  FIELD(Context, uint32_t, bve_backward_occurrence_limit, strategy::BveParameters::BACKWARD_OCCURRENCE_DEFAULT, IDENTITY) /* CaDiCaL elimocclim */ \
  FIELD(Context, uint64_t, bve_definition_budget, strategy::BveParameters::DEFINITION_BUDGET_DEFAULT, IDENTITY) \
  FIELD(Context, uint32_t, bve_definition_cores, strategy::BveParameters::DEFINITION_CORES_DEFAULT, IDENTITY) \
  FIELD(Context, uint32_t, bve_definition_core_budget_multiplier, strategy::BveParameters::DEFINITION_CORE_BUDGET_MULTIPLIER_DEFAULT, IDENTITY) \
  FIELD(Context, bool, bve_backward, false, IDENTITY) \
  /* CaDiCaL elim.cpp generation-time self-subsuming resolution; opt-in. */ \
  FIELD(Context, bool, bve_on_the_fly_strengthening, false, IDENTITY) \
  FIELD(Context, uint32_t, factor_decade_round_cap, 14, IDENTITY) \
  FIELD(Context, uint32_t, factor_decade_cap, 19, IDENTITY) \
  FIELD(Context, uint32_t, factor_decade_offset, 5, IDENTITY) \
  FIELD(Context, uint32_t, factor_decade_base, 10, IDENTITY) \
  /* Diagnostic arm; zero follows Kissat's factordelay predicate. */ \
  FIELD(Context, uint64_t, factor_active_ceiling, 0, IDENTITY) \
  FIELD(Context, uint32_t, bve_permille, 100, IDENTITY) \
  /* Startup FastEl's separate dose, which its owner scan is charged to. */ \
  FIELD(Context, uint32_t, initial_fast_elimination_bve_permille, 125, IDENTITY) \
  FIELD(Context, uint32_t, probe_permille, 8, IDENTITY) \
  FIELD(Context, uint32_t, hyper_binary_permille, 8, IDENTITY) \
  FIELD(Context, uint32_t, backbone_permille, 20, IDENTITY) \
  FIELD(Context, uint32_t, transitive_permille, 20, IDENTITY) \
  FIELD(Context, uint32_t, vivify_permille, 100, IDENTITY) \
  FIELD(Context, uint32_t, strengthen_permille, 100, IDENTITY) \
  FIELD(Context, uint32_t, subsume_forward_permille, 100, IDENTITY) \
  FIELD(Context, uint32_t, factor_permille, 50, IDENTITY) \
  FIELD(Context, uint32_t, hyper_ternary_permille, 8, IDENTITY) \
  FIELD(Context, uint32_t, bce_permille, 20, IDENTITY) \
  FIELD(Context, uint32_t, cce_permille, 4, IDENTITY) \
  FIELD(Context, uint32_t, instantiate_permille, 20, IDENTITY) \
  FIELD(Context, uint32_t, sweep_permille, 100, IDENTITY) \
  FIELD(Context, uint32_t, substitute_permille, 10, IDENTITY)

#define TACTIX_DEFAULT_INPROCESS_PRODUCTIVITY(FIELD, Context) \
  FIELD(Context, uint32_t, congruence_productivity_permille, 1, IDENTITY) \
  FIELD(Context, uint32_t, sweep_productivity_permille, 1, IDENTITY) \
  FIELD(Context, uint32_t, vivify_irredundant_productivity_percent, 1, IDENTITY) \
  FIELD(Context, uint8_t, vivify_tier_mode, 0, UNSIGNED) /* Kissat `vivifyfocusedtiers` default. */ \
  FIELD(Context, uint32_t, vivify_redundant_share, 7, IDENTITY) \
  FIELD(Context, uint32_t, vivify_total_share, 10, IDENTITY)

#define TACTIX_DEFAULT_INPROCESS_GATES(FIELD, Context) \
  FIELD(Context, bool, closure_delay, true, IDENTITY) \
  FIELD(Context, bool, split_clocks, true, IDENTITY) \
  FIELD(Context, bool, watermarks, true, IDENTITY) \
  FIELD(Context, bool, preprocess_followups, true, IDENTITY) \
  /* Hold standalone C1 for Eliminate (the reference disables C1). */ \
  FIELD(Context, bool, hold_subsume_until_eliminate, true, IDENTITY) \
  /* Kissat's forward pass checks `forward_steps` between candidates. */ \
  FIELD(Context, bool, forward_midround_budget, true, IDENTITY) \
  FIELD(Context, bool, couple_factor_bound, true, IDENTITY)
