#pragma once

#include "strategy/sweep_parameters.hpp"
#include "strategy/inprocess_program.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace tactix {

enum class DefaultRephaseSlot : uint8_t {
  Best,
  Walk,
  Inverted,
  Original,
};

enum class DefaultReorderTarget : uint8_t {
  Active,
  Queue,
  Heap,
};

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
    std::vector<int8_t> focused_phase_pattern{0, 1, 0, -1, 0, 0, 0, 0};
    bool random_sequences = true;
  } branching;

  struct Analysis {
    uint32_t minimize_depth_limit =
        strategy::AnalyzeParameters::MINIMIZE_DEPTH_DEFAULT;
    uint32_t bump_reason_limit = BUMP_REASON_LIMIT_DEFAULT;
    double bump_reason_rate = BUMP_REASON_RATE_DEFAULT;
    uint32_t reason_depth = 1; // Kissat direct-reason traversal in both modes.
    uint32_t chronological_levels = CHRONOLOGICAL_LEVELS_DEFAULT;
    bool reuse_ranked_trail_suffix = false; // Kissat tie-break over CaDiCaL.
    bool conflict_reuse = true;
    bool jump_reasons = true;
    bool shrink_glue_promote = true;
    bool bump_extension_reasons = true;
    uint32_t eager_subsume =
        strategy::AnalyzeParameters::EAGER_SUBSUME_CAPACITY;
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
    uint8_t reduce_tier1_mode = 0; // focused tier-1 snapshot.
    uint8_t reduce_tier2_mode = 1; // stable tier-2 snapshot.
    uint32_t victim_numerator =
        strategy::ReduceParameters::VICTIM_NUMERATOR_DEFAULT;
    uint32_t victim_denominator =
        strategy::ReduceParameters::VICTIM_DENOMINATOR_DEFAULT;
    bool protect_tiers = true;
    // The ordinary scheduled reduction computes compactness from the live
    // inactive-variable ratio. This is the profile for an explicit canonical
    // Reduce action through the shared strategy interface.
    bool canonical_compact_collection = false;
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
    uint32_t walk_even_break_base_milli =
        strategy::WalkParameters::EVEN_BREAK_BASE_DEFAULT;
    std::vector<strategy::WalkParameters::ScorePoint> walk_score_curve =
        strategy::WalkParameters::reference_score_curve();
    std::vector<DefaultRephaseSlot> rotation{
        DefaultRephaseSlot::Best, DefaultRephaseSlot::Walk,
        DefaultRephaseSlot::Inverted, DefaultRephaseSlot::Best,
        DefaultRephaseSlot::Walk, DefaultRephaseSlot::Original};
    DefaultRephaseSlot walk_inapplicable_fallback =
        DefaultRephaseSlot::Best;
    bool walk_warmup = true;
    bool walk_use_branch_seed = true;
    uint64_t walk_random_seed = 0;
    bool stable_only = true;
    bool late_lucky = true;
  } rephase;

  struct Reorder {
    uint64_t initial_interval = REORDER_INITIAL_INTERVAL_DEFAULT;
    uint64_t interval = REORDER_INITIAL_INTERVAL_DEFAULT;
    // 0 off, 1 stable only, 2 both modes. Kissat ships Reorder in both.
    uint32_t mode = 2;
    DefaultReorderTarget target = DefaultReorderTarget::Active;
    strategy::ReorderParameters parameters;
  } reorder;

  struct Inprocess {
    // Production values are formula-shape invariant. These typed switches
    // remain explicit diagnostic arms and are never selected by classification.
    bool bve_enabled = true;
    bool vivify_enabled = true;
    bool factor_enabled = true;
    bool sweep_enabled = true;
    bool factor_repeated_enabled = true;
    bool preprocess_only = false;
    // Kissat creation order is the production default.  Enabling this switch
    // is the explicit CaDiCaL VMTF/phase-order diagnostic arm; classification
    // and benchmark identity never select it.
    bool cadical_arena_ordering_enabled = false;
    // The shipped program is the exact historical pipeline.  Callers may
    // reorder, omit or repeat occurrences; false makes omission authoritative.
    std::vector<strategy::InprocessStep> program =
        strategy::reference_inprocess_program();
    bool append_unlisted_rules = true;
    uint64_t eliminate_interval = INPROCESS_ELIMINATE_INTERVAL_DEFAULT;
    uint64_t probe_interval = INPROCESS_PROBE_INTERVAL_DEFAULT;
    uint64_t minimum_effort_ticks = INPROCESS_MINIMUM_EFFORT_DEFAULT;
    uint64_t preprocess_initial_ticks = INPROCESS_PREPROCESS_EFFORT_DEFAULT;
    uint32_t phase_rounds = INPROCESS_PHASE_ROUNDS_DEFAULT;
    uint32_t substitute_rounds = SUBSTITUTE_ROUNDS_DEFAULT;
    // Ordinary search-time Substitute is one round; preprocessing follow-ups
    // use `substitute_rounds`. Both are explicit strategy repetition choices.
    uint32_t substitute_search_rounds = 1;
    uint64_t factor_initial_ticks = FACTOR_INITIAL_EFFORT_DEFAULT;
    double logarithm_offset = 9.0;
    double scale_quadratic = 4.5;
    double scale_constant = 25.0;
    double scale_log_shift = 5.0;
    uint64_t candidate_order_seed = 0x9e3779b97f4a7c15ull;
    strategy::CongruenceParameters congruence_parameters;
    strategy::InstantiateParameters instantiate_parameters;
    strategy::BackboneParameters backbone_parameters;
    strategy::VivifyParameters vivify_parameters;
    strategy::BceParameters bce_parameters;
    strategy::TernaryParameters ternary_parameters;
    strategy::FactorParameters factor_parameters;
    strategy::TransitiveReduceParameters transitive_parameters;
    strategy::SubstituteParameters substitute_parameters;
    // Canonical touched-material admission shared by C1/C2, BCE, CCE,
    // Instantiate, HyperTernaryRes and Factor. Explicit semantic instances
    // bypass this scheduling filter through their rule contracts.
    bool touch_filter = true;
    bool likely_kept_redundant = false;

    std::vector<uint32_t> bve_bounds{0, 1, 2, 4, 8, 16};
    // Kissat's shipped `eliminatebound` is 16. Lower ceilings remain
    // explicitly poseable through this typed config / TACTIX_BVE_RAMP_MAX,
    // but production walks the complete ladder before arming its terminal
    // new-material watermark.
    uint32_t bve_default_ceiling = 16;
    uint32_t bve_occurrence_limit = 2000; // kissat eliminateocclim
    uint32_t bve_clause_size_limit = 100; // kissat eliminateclslim
    uint32_t bve_xor_arity_limit =
        strategy::BveParameters::XOR_ARITY_DEFAULT; // CaDiCaL elimxorlim
    uint32_t bve_resolvent_size_limit =
        strategy::BveParameters::RESOLVENT_SIZE_DEFAULT; // Kissat eliminateclslim
    uint32_t bve_backward_occurrence_limit =
        strategy::BveParameters::BACKWARD_OCCURRENCE_DEFAULT; // CaDiCaL elimocclim
    uint64_t bve_definition_budget =
        strategy::BveParameters::DEFINITION_BUDGET_DEFAULT;
    uint32_t bve_definition_cores =
        strategy::BveParameters::DEFINITION_CORES_DEFAULT;
    uint32_t bve_definition_core_budget_multiplier =
        strategy::BveParameters::DEFINITION_CORE_BUDGET_MULTIPLIER_DEFAULT;
    bool bve_backward = false;
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

    // Kissat Sweep environment and growth options (options.h). Public and
    // default-policy calls share one typed shape; the reference policy alone
    // skips literals already fixed in the embedded environment.
    strategy::SweepParameters sweep_parameters = [] {
      strategy::SweepParameters result;
      result.skip_embedded_fixed = true;
      return result;
    }();

    uint32_t congruence_productivity_permille = 1;
    uint32_t sweep_productivity_permille = 1;
    uint32_t vivify_irredundant_productivity_percent = 1;
    uint8_t vivify_tier_mode = 0; // Kissat `vivifyfocusedtiers` default.
    uint32_t vivify_redundant_share = 7;
    uint32_t vivify_total_share = 10;
    std::array<uint32_t, 3> vivify_tier_shares{{3, 3, 1}};
    std::array<uint8_t, 3> vivify_tier_scopes{{3, 4, 5}};

    bool closure_delay = true;
    bool split_clocks = true;
    bool watermarks = true;
    bool preprocess_followups = true;
    // Hold standalone C1 for the Eliminate phase. The reference program keeps
    // C1 disabled because C2 owns the fused forward pass; custom programs may
    // enable it and select this cadence explicitly.
    bool hold_subsume_until_eliminate = true;
    // Kissat's fused forward pass always enforces its `forward_steps` effort
    // ceiling between candidates.  The name is retained for serialized-config
    // compatibility; this is not an inside-candidate interruption.
    bool forward_midround_budget = true;
    bool couple_factor_bound = true;
  } inprocess;

  bool valid(std::string *error = nullptr) const;
  // Stable, line-oriented snapshot used by trace and benchmark metadata.
  std::string serialize() const;
};

} // namespace tactix
