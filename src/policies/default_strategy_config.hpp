#pragma once

#include "strategy/sweep_parameters.hpp"
#include "strategy/inprocess_program.hpp"
#include "strategy/local_search.hpp"
#include "strategy/online_inprocess.hpp"
#include "strategy/online_sequence.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

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

// One configuration setting's value (DefaultStrategyConfig::setting).
struct StrategySettingValue {
  enum Kind : uint8_t { Integer, Real, Text } kind = Integer;
  uint64_t integer = 0; // signed settings in two's complement
  double real = 0;
  std::string text;
};

// A Lucky opportunity's production switch and its typed recognizer bounds.
template <class Parameters> struct DefaultEnabledModel {
  bool enabled = true;
  Parameters parameters;
};

// Canonical configuration of the shipped strategy: a value belongs here when it
// can change which DefaultPolicy action runs, when, with how much work, or
// which candidates it keeps; representation and diagnostic constants do not.
// Defaults cite Kissat 4.0.4 unless noted; call-site evidence is in
// docs/dev/implementation-notes.md.
struct DefaultStrategyConfig {
  static strategy::OnlineStrategyProgram reference_structured_root_program();
  static std::vector<strategy::OnlineStrategyStage>
  structured_root_preprocessing_stages();
  static std::vector<strategy::OnlineStrategyStage>
  bounded_backtracking_structured_root_preprocessing_stages();
  static std::vector<strategy::OnlineStrategyStage>
  large_domain_structured_root_preprocessing_stages();
  static strategy::OnlineStrategyStage structured_root_stage_after_feedback(
      const strategy::OnlineStrategyStage &, bool prior_stage_progress,
      bool prior_unique_literal_amo_progress,
      bool prior_nonforcing_cut_progress);
  // Named defaults that tests and probes can cite without a literal.
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
  // Kissat `walkeffort=50`, in Walk's per-entry step currency.
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
  // Kissat `mineffort=10`, in SET_EFFORT_LIMIT's reference currency.
  static constexpr uint64_t INPROCESS_PREPROCESS_EFFORT_DEFAULT = 10000000;
  static constexpr uint32_t INPROCESS_PHASE_ROUNDS_DEFAULT = 2;
  // Kissat `substituterounds=2`: a per-call SCC closure cap.
  static constexpr uint32_t SUBSTITUTE_ROUNDS_DEFAULT = 2;
  static constexpr uint64_t FACTOR_INITIAL_EFFORT_DEFAULT = 700000000ull;
  static constexpr uint64_t SMALL_CLAUSE_LIMIT_DEFAULT = 100000;
  static constexpr uint32_t BIG_BINARY_FRACTION_PERMILLE_DEFAULT = 990;
  // Satsuma 1.3 admits graph components up to five million vertices.
  static constexpr uint64_t SYMMETRY_GRAPH_COMPONENT_LIMIT_DEFAULT = 5'000'000;
  static constexpr uint64_t SYMMETRY_GRAPH_EDGE_LIMIT_DEFAULT = 1'000'000;
  static constexpr uint64_t SYMMETRY_GENERATOR_LIMIT_DEFAULT = 128;
  static constexpr uint64_t SYMMETRY_BACKTRACK_LIMIT_DEFAULT = 64;
  static constexpr uint64_t SYMMETRY_SUPPORT_LIMIT_DEFAULT = 64ull << 20;
  static constexpr uint64_t SYMMETRY_FIXING_LIMIT_DEFAULT = 128;
  static constexpr uint64_t SYMMETRY_TOTAL_TICK_LIMIT_DEFAULT =
      20'000'000'000ull;
  static constexpr uint32_t SYMMETRY_BINARY_DENSITY_PERMILLE_DEFAULT = 1000;

  struct Branching {
    uint64_t random_initial_interval = RANDOM_INITIAL_INTERVAL_DEFAULT;
    uint64_t random_sequence_length = RANDOM_SEQUENCE_LENGTH_DEFAULT;
    uint64_t random_seed = RANDOM_SEED_DEFAULT;
    double logarithm_offset = 9.0; // Kissat logarithmic schedule.
    // Focused decision-phase override indexed by (mode switches / switch span)
    // modulo the pattern length: zero selects the saved phase, +/-1 forces the
    // initial/opposite phase (decide.c).
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
    bool valid() const {
      const auto positive = [](double v) { return std::isfinite(v) && v > 0.0; };
      return positive(glue_margin) && positive(fast_ema_window) &&
             positive(slow_ema_window) && positive(logarithm_offset) &&
             positive(logarithm_bias) && initial_interval && reluctant_period &&
             reluctant_limit;
    }
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
    // The profile for an explicit canonical Reduce action.
    bool canonical_compact_collection = false;
    bool bookkeeping = true;
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
    // CCAnr/CryptoMiniSat configuration checking, chosen from structure.
    uint64_t configuration_checking_walk_permille = 1000;
    uint64_t configuration_checking_minimum_step_budget = 200000000;
    uint64_t configuration_checking_conflict_interval = 44000;
    uint32_t configuration_checking_minimum_clauses = 1000;
    uint32_t complementary_ternary_minimum_permille = 900;
    uint32_t configuration_checking_maximum_tries = 100;
    uint64_t configuration_checking_random_seed = 1;
    uint64_t configuration_checking_flips_per_try = 1000000;
    uint32_t configuration_checking_weight_threshold = 50;
    uint32_t configuration_checking_smoothing_old_numerator = 3;
    uint32_t configuration_checking_smoothing_denominator = 10;
    bool configuration_checking_aspiration = true;
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

  DefaultEnabledModel<strategy::ParityModelParameters> parity_model;
  DefaultEnabledModel<strategy::OnehotModelParameters> onehot_model;
  DefaultEnabledModel<strategy::AffineModelParameters> affine_model;
  DefaultEnabledModel<strategy::BalancedSignatureModelParameters>
      balanced_signature_model;
  DefaultEnabledModel<strategy::FermatModelParameters> fermat_model;

  struct CernyModel {
    bool enabled = true;
  } cerny_model;

  DefaultEnabledModel<strategy::LookupCircuitModelParameters>
      lookup_circuit_model;
  DefaultEnabledModel<strategy::SimonModelParameters> simon_model;
  DefaultEnabledModel<strategy::StconnProofParameters> stconn_proof;
  DefaultEnabledModel<strategy::SignedCardinalityProofParameters>
      signed_cardinality_proof;
  DefaultEnabledModel<strategy::ExactCoverModelParameters> exact_cover_model;

  struct IndependentSetModel {
    bool enabled = true;
    strategy::IndependentSetModelParameters parameters = [] {
      strategy::IndependentSetModelParameters p;
      // The stable-extension projection is bounded by default.
      p.maximum_vertices = 4096;
      return p;
    }();
  } independent_set_model;

  struct GraphLabelingModel {
    // Lucky 26/27 schedule: authenticate once, continue at root windows.
    bool enabled = true;
    strategy::GraphLabelingParameters parameters = [] {
      strategy::GraphLabelingParameters p;
      p.maximum_vertices = 140;
      p.workspace_bytes = 512ull << 20;
      p.work[size_t(strategy::GraphLabelingStage::Recognition)] = 100'000'000;
      p.work[size_t(strategy::GraphLabelingStage::Construction)] = 100'000'000;
      p.work[size_t(strategy::GraphLabelingStage::Window)] = 5'000'000'000;
      p.work[size_t(strategy::GraphLabelingStage::Repair)] = 1'000'000'000;
      p.work[size_t(strategy::GraphLabelingStage::Anneal)] = 500'000'000;
      p.work[size_t(strategy::GraphLabelingStage::FinalRepair)] = 1'000'000'000;
      p.work[size_t(strategy::GraphLabelingStage::Completion)] = 100'000'000;
      p.work[size_t(strategy::GraphLabelingStage::Publication)] = 1'000'000;
      p.maximum_dfs_vertices = 140;
      p.maximum_dfs_nodes = 50'000'000;
      p.maximum_dfs_starts = 12;
      p.early_dfs_choices = 28;
      p.repair_rounds = UINT64_MAX;
      p.anneal_iterations = UINT64_MAX;
      p.stall_anneal_iterations = 100'000;
      p.seed = 0;
      return p;
    }();
    // Source capture and trajectory work run only at the initial root and
    // natural full-restart roots, without requesting restarts; the interval
    // follows the shared Kissat-derived root schedule and each run spends the
    // owner's stage doses (docs/dev/implementation-notes.md, S5 Lucky).
  } graph_labeling_model;
  struct RamseyStartup {
    bool enabled = true;
    strategy::RamseyStartupParameters parameters = {
        10'000, 200, 40'000, 10'000'000,
        10'000'000, 100'000, 1'000'000, 200'000};
  } ramsey_startup;
  // Startup counting refutation; the DSR follow-on disables it (no DSR
  // spelling).
  struct StartupCount {
    bool enabled = true;
  } startup_count;

  struct AffineRootClosure {
    // The checked Rank263 profile bounds this startup-only vertical; execution
    // derives clause/group/source/node/extent allowances from the live formula,
    // and these ceilings bound admission and certificate construction.
    bool enabled = true;
    uint64_t maximum_clauses = 12'000;
    uint64_t maximum_snapshot_work = 1'000'000;
    uint64_t maximum_recognition_work = 10'000'000;
    uint64_t maximum_basis_work = 100'000'000;
    uint32_t maximum_row_width = 68;
    uint64_t maximum_proof_additions = 1'000'000;
    uint64_t maximum_proof_deletions = 1'000'000;
    uint64_t maximum_journal_bytes = 100'000'000;
    uint64_t maximum_session_work = 100'000'000;
  } affine_root_closure;

  struct AffineSearch {
    enum class ReasonMode : uint8_t {
      EagerSourceSupport = 0,
      Deferred = 1,
      DetachedMinimal = 2,
      DetachedSource = 3,
      LazySource = 4,
      LazyMinimal = 5,
    };
    // Production switch, disableable for attribution and regression controls.
    bool enabled = true;
    // Keep authenticated source rows in the matrix but publish the smallest
    // checked reason only when analysis reaches the deferred implication; eager
    // source support is an explicit control profile.
    ReasonMode reason_mode = ReasonMode::LazyMinimal;
    // CryptoMiniSat src/solverconf.h GaussConf defaults, pinned at
    // 2bf89cfb3e126e559d472388486327813f735f7e. Tactix also caps retained
    // bytes/work and owns one matrix.
    uint32_t maximum_rows = 100000;
    uint32_t maximum_row_width = 100000;
    uint32_t maximum_arity = 8; // Existing authenticated CNF recognizer capacity.
    bool autodisable = true;
    double minimum_usefulness = 0.2;
    uint64_t autodisable_minimum_calls = 200;
    uint64_t autodisable_check_every = 1024;
    // Zero inherits the preprocessing/search effort quantum. The
    // retained-source cap covers the whole matrix transaction; the 100 MB root
    // journal cap is below two CryptoMiniSat XOR families' 143--218 MB
    // preflight needs.
    uint64_t maximum_initialization_work = 0;
    uint64_t maximum_service_work = 0;
    uint64_t maximum_retained_bytes = 256'000'000;
  } affine_search;

  struct Inprocess {
    // Production values are shape invariant; these are diagnostic arms.
    bool bve_enabled = true;
    bool vivify_enabled = true;
    bool factor_enabled = true;
    bool sweep_enabled = true;
    bool factor_repeated_enabled = true;
    bool preprocess_only = false;
    // SC2024's bounded startup pass composing forward subsumption and BVE.
    bool initial_fast_elimination = true;
    uint32_t initial_fast_elimination_rounds = 4;
    strategy::BveParameters initial_fast_elimination_parameters = [] {
      strategy::BveParameters p;
      p.gate_mask = 0;
      p.growth_bound = UINT32_MAX;
      p.occurrence_limit = 100;
      p.clause_size_limit = 100;
      p.resolvent_count_limit = 8;
      p.resolvent_size_limit = UINT32_MAX;
      p.backward = false;
      p.traversal = strategy::BveTraversal::FastEliminate;
      return p;
    }();
    // Explicit CaDiCaL VMTF/phase-order arm; Kissat order is the default.
    bool cadical_arena_ordering_enabled = false;
    // The shipped pipeline; callers may reorder, omit or repeat occurrences.
    std::vector<strategy::InprocessStep> program =
        strategy::reference_inprocess_program();
    // Formula-independent root-only SR composition, run once at the selected
    // startup boundary before the slot program, with producer parameters,
    // propagation, repetition, budgets and stops explicit. False keeps the
    // reference order after early Lucky at Probe entry.
    bool structured_root_before_lucky = true;
    // Diagnostic selection of SR-containing startup programs.
    bool structured_root_symmetry_enabled = true;
    // Keep the parser's activation and import seed after structured
    // preprocessing; the optional live-formula reseed does not reproduce
    // Satsuma's residual presentation.
    bool structured_root_residual_search_handoff = false;
    // Cumulative hard ceiling charged by every root producer.
    strategy::WorkLimit structured_root_sr_discovery_budget =
        strategy::WorkLimit::bounded(SYMMETRY_TOTAL_TICK_LIMIT_DEFAULT);
    // Satsuma's structured detectors share max(16 * variables, 16 Mi) work:
    // each small-domain producer gets one zero-yield opportunity, and effective
    // transitions or saturated generator prefixes earn more within the hard
    // budget. Deterministic work feedback, not time.
    bool structured_root_progress_feedback = true;
    // Above 50k variables the owner runs one generic generator-fixing scan.
    bool structured_root_large_domain_progress_feedback = false;
    uint64_t structured_root_progress_minimum_ticks = 16ull << 20;
    uint64_t structured_root_progress_ticks_per_variable = 16;
    // Certified ULC completion publishes binary AMO helpers; after a later
    // structured consumer progresses, one BCE pass may retire them with normal
    // proof and reconstruction obligations. Helper-only progress keeps them.
    bool structured_root_bce_after_unique_literal_amo = true;
    std::vector<strategy::OnlineStrategyStage> structured_root_stages =
        DefaultStrategyConfig::structured_root_preprocessing_stages();
    // Satsuma cuts DejaVu's backtracking budget from 64 to one at this size.
    uint64_t structured_root_backtracking_domain_limit = 32'000;
    std::vector<strategy::OnlineStrategyStage>
        structured_root_bounded_backtracking_stages =
            DefaultStrategyConfig::
                bounded_backtracking_structured_root_preprocessing_stages();
    // Above this domain Satsuma switches its symmetry schedule to one
    // exact-generator pass with negation/binary fixing, repeated only after
    // output; UINT64_MAX disables that program.
    uint64_t structured_root_large_domain_limit = 50'000;
    std::vector<strategy::OnlineStrategyStage>
        structured_root_large_domain_stages =
            DefaultStrategyConfig::large_domain_structured_root_preprocessing_stages();
    bool append_unlisted_rules = true;
    uint64_t eliminate_interval = INPROCESS_ELIMINATE_INTERVAL_DEFAULT;
    uint64_t probe_interval = INPROCESS_PROBE_INTERVAL_DEFAULT;
    uint64_t minimum_effort_ticks = INPROCESS_MINIMUM_EFFORT_DEFAULT;
    uint64_t preprocess_initial_ticks = INPROCESS_PREPROCESS_EFFORT_DEFAULT;
    uint32_t phase_rounds = INPROCESS_PHASE_ROUNDS_DEFAULT;
    uint32_t substitute_rounds = SUBSTITUTE_ROUNDS_DEFAULT;
    // Search-time Substitute is one round; follow-ups use `substitute_rounds`.
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
    // Touched-material admission for C1/C2, BCE, CCE, Instantiate, HTR, Factor.
    bool touch_filter = true;
    bool likely_kept_redundant = false;

    std::vector<uint32_t> bve_bounds{0, 1, 2, 4, 8, 16};
    // Kissat's `eliminatebound` is 16; lower ceilings stay poseable (typed
    // config, TACTIX_BVE_RAMP_MAX), production walks the whole ladder before
    // arming its new-material watermark.
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
    // CaDiCaL elim.cpp generation-time self-subsuming resolution; opt-in.
    bool bve_on_the_fly_strengthening = false;
    uint32_t factor_decade_round_cap = 14;
    uint32_t factor_decade_cap = 19;
    uint32_t factor_decade_offset = 5;
    uint32_t factor_decade_base = 10;
    // Diagnostic arm; zero follows Kissat's factordelay predicate.
    uint64_t factor_active_ceiling = 0;

    uint32_t bve_permille = 100;
    // Startup FastEl's separate dose, which its owner scan is charged to.
    uint32_t initial_fast_elimination_bve_permille = 125;
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

    // Kissat Sweep environment and growth options (options.h), shared by public
    // and default calls; only the reference policy skips literals fixed in the
    // environment.
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
    // Hold standalone C1 for Eliminate (the reference disables C1).
    bool hold_subsume_until_eliminate = true;
    // Kissat's forward pass checks `forward_steps` between candidates.
    bool forward_midround_budget = true;
    bool couple_factor_bound = true;
  } inprocess;

  bool valid(std::string *error = nullptr) const;
  // Line-oriented snapshot: every setting as ` <group>.<key>=<value>`.
  std::string serialize() const;
  // The setting a key names ("restart.glue_margin"); false if unknown.
  bool setting(std::string_view key, StrategySettingValue &out) const;
  template <class Sink> void visit_settings(Sink &) const;
};

void pose_query_programs(DefaultStrategyConfig &);

// Borrowed configuration, or the shipped defaults when standalone.
template <class Config>
const Config &configured_or_default(const Config *configured) {
  if (configured)
    return *configured;
  static const Config defaults;
  return defaults;
}

} // namespace tactix
