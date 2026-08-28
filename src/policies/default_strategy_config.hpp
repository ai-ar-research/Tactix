#pragma once

#include "rules/sweep/sweep_defaults.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace tactix {

// Canonical configuration of the shipped strategy.  A value belongs here when
// changing it can alter which DefaultPolicy action runs, when it runs, how much
// work it receives, or which candidates it keeps.  Constants intrinsic to data
// representation and diagnostics deliberately do not belong here.
//
// Defaults cite Kissat 4.0.4 unless a comment says otherwise.  The executable
// call-site evidence is maintained in docs/implementation-notes.md.
struct DefaultStrategyConfig {
  // Named compatibility constants are kept beside the typed fields so tests
  // and design probes can cite a default without duplicating its literal.
  static constexpr uint64_t RESTART_INTERVAL_DEFAULT = 1;
  static constexpr double RESTART_MARGIN_DEFAULT = 1.10;
  static constexpr double FAST_EMA_WINDOW_DEFAULT = 33.0;
  static constexpr double SLOW_EMA_WINDOW_DEFAULT = 1e5;
  static constexpr uint64_t RANDOM_INITIAL_INTERVAL_DEFAULT = 500;
  static constexpr uint64_t RANDOM_SEQUENCE_LENGTH_DEFAULT = 10;
  static constexpr uint64_t RANDOM_SEED_DEFAULT = 0; // Kissat `seed=0`.
  static constexpr uint32_t BUMP_REASON_LIMIT_DEFAULT = 10;
  static constexpr double BUMP_REASON_RATE_DEFAULT = 10.0;
  static constexpr uint32_t CHRONOLOGICAL_LEVELS_DEFAULT = 100;
  // Kissat `walkeffort=50`.  Walk uses the same per-entry step currency now,
  // so the former CaDiCaL-derived 560 conversion is no longer appropriate.
  static constexpr uint64_t WALK_PERMILLE_DEFAULT = 50;
  static constexpr uint64_t REDUCE_INTERVAL_DEFAULT = 1000;
  static constexpr uint64_t REDUCE_ARITHMETIC_DEFAULT = 2000;
  static constexpr uint64_t REDUCE_ARITHMETIC_STEP_DEFAULT = 300;
  static constexpr uint64_t MODE_INITIAL_CONFLICTS_DEFAULT = 1000;
  static constexpr uint64_t MODE_INTERVAL_DEFAULT = 1000;
  static constexpr uint64_t RELUCTANT_PERIOD_DEFAULT = 1024;
  static constexpr uint64_t RELUCTANT_LIMIT_DEFAULT = 1ull << 20;
  static constexpr uint64_t REPHASE_INTERVAL_DEFAULT = 1000;
  static constexpr uint64_t REORDER_INITIAL_INTERVAL_DEFAULT = 10000;
  static constexpr uint64_t INPROCESS_ELIMINATE_INTERVAL_DEFAULT = 500;
  static constexpr uint64_t INPROCESS_PROBE_INTERVAL_DEFAULT = 100;
  static constexpr uint64_t INPROCESS_MINIMUM_EFFORT_DEFAULT = 10000000;
  // Kissat's default `mineffort=10`, in the `SET_EFFORT_LIMIT` reference
  // currency used by its startup probing pipeline.
  static constexpr uint64_t INPROCESS_PREPROCESS_EFFORT_DEFAULT = 10000000;
  static constexpr uint32_t INPROCESS_PHASE_ROUNDS_DEFAULT = 2;
  // Kissat `substituterounds=2`: this is a per-call SCC closure cap, distinct
  // from `proberounds`, which repeats the complete probing pipeline.
  static constexpr uint32_t SUBSTITUTE_ROUNDS_DEFAULT = 2;
  static constexpr uint64_t FACTOR_INITIAL_EFFORT_DEFAULT = 700000000ull;
  static constexpr uint32_t TIER_RELATIVE_SCALE = 1000;
  static constexpr uint64_t SMALL_CLAUSE_LIMIT_DEFAULT = 100000;
  static constexpr uint32_t BIG_BINARY_FRACTION_PERMILLE_DEFAULT = 990;

  struct Branching {
    uint64_t random_initial_interval = RANDOM_INITIAL_INTERVAL_DEFAULT;
    uint64_t random_sequence_length = RANDOM_SEQUENCE_LENGTH_DEFAULT;
    uint64_t random_seed = RANDOM_SEED_DEFAULT;
    double logarithm_offset = 9.0; // Kissat logarithmic schedule.
    // Focused decision-phase override indexed by
    // (mode switches / switch span) modulo the pattern length.  Zero selects
    // the saved phase, +/-1 forces the initial/opposite phase (decide.c).
    uint32_t focused_phase_switch_span = 2;
    std::array<int8_t, 8> focused_phase_pattern{{0, 1, 0, -1, 0, 0, 0, 0}};
    bool random_sequences = true;
  } branching;

  struct Analysis {
    uint32_t bump_reason_limit = BUMP_REASON_LIMIT_DEFAULT;
    double bump_reason_rate = BUMP_REASON_RATE_DEFAULT;
    uint32_t reason_depth = 1; // Kissat direct-reason traversal in both modes.
    uint32_t chronological_levels = CHRONOLOGICAL_LEVELS_DEFAULT;
    bool reuse_ranked_trail_suffix = false; // Kissat tie-break over CaDiCaL.
    uint64_t small_clause_limit = SMALL_CLAUSE_LIMIT_DEFAULT;
    uint32_t big_binary_fraction_permille =
        BIG_BINARY_FRACTION_PERMILLE_DEFAULT;
  } analysis;

  struct Restart {
    double glue_margin = RESTART_MARGIN_DEFAULT;
    uint64_t initial_interval = RESTART_INTERVAL_DEFAULT;
    double fast_ema_window = FAST_EMA_WINDOW_DEFAULT;
    double slow_ema_window = SLOW_EMA_WINDOW_DEFAULT;
    double logarithm_offset = 9.0;
    double logarithm_bias = 1.0;
    uint64_t reluctant_period = RELUCTANT_PERIOD_DEFAULT;
    uint64_t reluctant_limit = RELUCTANT_LIMIT_DEFAULT;
  } restart;

  struct Mode {
    uint64_t initial_conflicts = MODE_INITIAL_CONFLICTS_DEFAULT;
    uint64_t interval = MODE_INTERVAL_DEFAULT;
    double logarithm_offset = 9.0;
    unsigned logarithm_power = 4;
    uint32_t force = 0; // 0 automatic, 1 focused, 2 stable.
  } mode;

  struct Reduction {
    uint64_t initial_interval = REDUCE_INTERVAL_DEFAULT; // Kissat reduceinit.
    uint64_t arithmetic_interval = REDUCE_ARITHMETIC_DEFAULT;
    uint64_t arithmetic_step = REDUCE_ARITHMETIC_STEP_DEFAULT;
    double low_percent = 50.0;  // Kissat reducelow.
    double high_percent = 90.0; // Kissat reducehigh.
    double logarithm_offset = 9.0;
    uint32_t compact_inactive_percent = 10; // Kissat compactlim.
    uint64_t retier_initial_interval = 2;
    uint64_t retier_max_interval = 1ull << 16;
    uint32_t retier_growth = 2;
    uint32_t tier1_default = 2; // Kissat tier1.
    uint32_t tier2_default = 6; // Kissat tier2.
    uint32_t tier1_relative_permille = 500; // Kissat tier1relative.
    uint32_t tier2_relative_permille = 900; // Kissat tier2relative.
    bool square_root_cadence = true;
    bool ramp_fraction = true;
    bool dynamic_tiers = true;
    bool compact_variables = true;
  } reduction;

  struct Rephase {
    uint64_t initial_interval = REPHASE_INTERVAL_DEFAULT;
    uint64_t interval = REPHASE_INTERVAL_DEFAULT;
    double logarithm_offset = 9.0;
    unsigned logarithm_power = 3;
    uint64_t walk_permille = WALK_PERMILLE_DEFAULT;
    bool stable_only = true;
    bool late_lucky = true;
  } rephase;

  struct Reorder {
    uint64_t initial_interval = REORDER_INITIAL_INTERVAL_DEFAULT;
    uint64_t interval = REORDER_INITIAL_INTERVAL_DEFAULT;
    // 0 off, 1 both modes, 2 stable only.  Kissat ships reorder enabled.
    uint32_t mode = 2;
  } reorder;

  struct Inprocess {
    // Production values are formula-shape invariant. These typed switches
    // remain explicit diagnostic arms and are never selected by classification.
    bool bve_enabled = true;
    bool vivify_enabled = true;
    bool factor_repeated_enabled = true;
    bool preprocess_only = false;
    // Accepted CaDiCaL-style arena permutation. The false value is the
    // explicit Kissat creation-order parity arm, never a formula classifier.
    bool arena_ordering_enabled = true;
    uint64_t eliminate_interval = INPROCESS_ELIMINATE_INTERVAL_DEFAULT;
    uint64_t probe_interval = INPROCESS_PROBE_INTERVAL_DEFAULT;
    uint64_t minimum_effort_ticks = INPROCESS_MINIMUM_EFFORT_DEFAULT;
    uint64_t preprocess_initial_ticks = INPROCESS_PREPROCESS_EFFORT_DEFAULT;
    uint32_t phase_rounds = INPROCESS_PHASE_ROUNDS_DEFAULT;
    uint32_t substitute_rounds = SUBSTITUTE_ROUNDS_DEFAULT;
    uint64_t factor_initial_ticks = FACTOR_INITIAL_EFFORT_DEFAULT;
    double logarithm_offset = 9.0;
    double scale_quadratic = 4.5;
    double scale_constant = 25.0;
    double scale_log_shift = 5.0;
    uint64_t candidate_order_seed = 0x9e3779b97f4a7c15ull;

    std::array<uint32_t, 6> bve_bounds{{0, 1, 2, 4, 8, 16}};
    uint32_t bve_default_ceiling = 8;
    uint32_t bve_occurrence_limit = 2000; // kissat eliminateocclim
    uint32_t bve_clause_size_limit = 100; // kissat eliminateclslim
    uint32_t factor_decade_round_cap = 14;
    uint32_t factor_decade_cap = 19;
    uint32_t factor_decade_offset = 5;
    uint32_t factor_decade_base = 10;
    // Explicit diagnostic arm only. Zero disables the former Tactix-specific
    // fixed-population gate; production follows Kissat's factordelay predicate.
    uint64_t factor_active_ceiling = 0;

    uint32_t bve_permille = 100;
    uint32_t probe_permille = 8;
    uint32_t hyper_binary_permille = 8;
    uint32_t backbone_permille = 20;
    uint32_t transitive_permille = 20;
    uint32_t vivify_permille = 100;
    uint32_t strengthen_permille = 100;
    uint32_t subsume_forward_permille = 100;
    uint32_t factor_permille = 50;
    uint32_t hyper_ternary_permille = 8;
    uint32_t bce_permille = 20;
    uint32_t cce_permille = 4;
    uint32_t instantiate_permille = 20;
    uint32_t sweep_permille = 100;
    uint32_t substitute_permille = 10;

    // Kissat Sweep environment and growth options (options.h).  These belong
    // to the strategy profile even though Sweep also exposes a caller-owned
    // limits object for public rule applications.
    uint32_t sweep_depth = SweepReferenceDefaults::DEPTH;
    uint32_t sweep_max_depth = SweepReferenceDefaults::MAX_DEPTH;
    uint32_t sweep_vars = SweepReferenceDefaults::VARS;
    uint32_t sweep_max_vars = SweepReferenceDefaults::MAX_VARS;
    uint32_t sweep_clauses = SweepReferenceDefaults::CLAUSES;
    uint32_t sweep_max_clauses = SweepReferenceDefaults::MAX_CLAUSES;
    uint32_t sweep_candidate_pairs = SweepReferenceDefaults::CANDIDATE_PAIRS;
    uint32_t sweep_flip_rounds = SweepReferenceDefaults::FLIP_ROUNDS;
    uint32_t sweep_seed_cap = SweepReferenceDefaults::SEED_CAP;
    uint64_t sweep_environment_ticks =
        SweepReferenceDefaults::ENVIRONMENT_TICKS;
    bool sweep_strip_root_false = true;
    bool sweep_full_partition = true;
    bool sweep_skip_embedded_fixed = true;
    bool sweep_resume = true;

    uint32_t congruence_productivity_permille = 1;
    uint32_t sweep_productivity_permille = 1;
    uint32_t vivify_irredundant_productivity_percent = 1;
    uint32_t vivify_redundant_share = 7;
    uint32_t vivify_total_share = 10;
    std::array<uint32_t, 3> vivify_tier_shares{{3, 3, 1}};
    std::array<uint8_t, 3> vivify_tier_scopes{{3, 4, 5}};

    bool closure_delay = true;
    bool split_clocks = true;
    bool watermarks = true;
    bool preprocess_followups = true;
    bool forward_midround_budget = false;
    bool couple_factor_bound = true;
  } inprocess;

  // Stable, line-oriented snapshot used by trace and benchmark metadata.
  std::string serialize() const;
};

} // namespace tactix
