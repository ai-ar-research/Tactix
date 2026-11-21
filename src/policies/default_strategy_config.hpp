#pragma once

#include "policies/default_setting_fields.hpp"
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
    TACTIX_DECLARE_DEFAULT_SETTINGS(BRANCHING)
    std::vector<int8_t> focused_phase_pattern{0, 1, 0, -1, 0, 0, 0, 0};
    bool random_sequences = true;
  } branching;

  struct Analysis {
    TACTIX_DECLARE_DEFAULT_SETTINGS(ANALYSIS)
  } analysis;

  struct Restart {
    TACTIX_DECLARE_DEFAULT_SETTINGS(RESTART)
    bool valid() const {
      const auto positive = [](double v) { return std::isfinite(v) && v > 0.0; };
      return positive(glue_margin) && positive(fast_ema_window) &&
             positive(slow_ema_window) && positive(logarithm_offset) &&
             positive(logarithm_bias) && initial_interval && reluctant_period &&
             reluctant_limit;
    }
  } restart;

  struct Mode {
    TACTIX_DECLARE_DEFAULT_SETTINGS(MODE)
  } mode;

  struct Reduction {
    TACTIX_DECLARE_DEFAULT_SETTINGS(REDUCTION)
  } reduction;

  struct Rephase {
    TACTIX_DECLARE_DEFAULT_SETTINGS(REPHASE_CADENCE)
    std::vector<strategy::WalkParameters::ScorePoint> walk_score_curve =
        strategy::WalkParameters::reference_score_curve();
    std::vector<DefaultRephaseSlot> rotation{
        DefaultRephaseSlot::Best, DefaultRephaseSlot::Walk,
        DefaultRephaseSlot::Inverted, DefaultRephaseSlot::Best,
        DefaultRephaseSlot::Walk, DefaultRephaseSlot::Original};
    DefaultRephaseSlot walk_inapplicable_fallback =
        DefaultRephaseSlot::Best;
    TACTIX_DECLARE_DEFAULT_SETTINGS(REPHASE_WALK)
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
    bool matching = false;
  } startup_count;

  struct AffineRootClosure {
    // The checked Rank263 profile bounds this startup-only vertical; execution
    // derives clause/group/source/node/extent allowances from the live formula,
    // and these ceilings bound admission and certificate construction.
    TACTIX_DECLARE_DEFAULT_SETTINGS(AFFINE_ROOT)
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
    TACTIX_DECLARE_DEFAULT_SETTINGS(AFFINE_SEARCH)
  } affine_search;

  struct Inprocess {
    // Production values are shape invariant; these are diagnostic arms.
    TACTIX_DECLARE_DEFAULT_SETTINGS(INPROCESS_FLAGS)
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
    TACTIX_DECLARE_DEFAULT_SETTINGS(STRUCTURED_FEEDBACK)
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
    TACTIX_DECLARE_DEFAULT_SETTINGS(INPROCESS_CADENCE)
    strategy::CongruenceParameters congruence_parameters;
    strategy::InstantiateParameters instantiate_parameters;
    strategy::BackboneParameters backbone_parameters;
    strategy::VivifyParameters vivify_parameters;
    strategy::BceParameters bce_parameters;
    strategy::TernaryParameters ternary_parameters;
    strategy::FactorParameters factor_parameters;
    uint32_t factor_fx_effort_floor = 75;
    strategy::TransitiveReduceParameters transitive_parameters;
    strategy::SubstituteParameters substitute_parameters;
    // Touched-material admission for C1/C2, BCE, CCE, Instantiate, HTR, Factor.
    bool touch_filter = true;
    bool likely_kept_redundant = false;

    std::vector<uint32_t> bve_bounds{0, 1, 2, 4, 8, 16};
    // Kissat's `eliminatebound` is 16; lower ceilings stay poseable (typed
    // config, TACTIX_BVE_RAMP_MAX), production walks the whole ladder before
    // arming its new-material watermark.
    TACTIX_DECLARE_DEFAULT_SETTINGS(INPROCESS_BOUNDS_AND_RATES)

    // Kissat Sweep environment and growth options (options.h), shared by public
    // and default calls; only the reference policy skips literals fixed in the
    // environment.
    strategy::SweepParameters sweep_parameters = [] {
      strategy::SweepParameters result;
      result.skip_embedded_fixed = true;
      return result;
    }();

    TACTIX_DECLARE_DEFAULT_SETTINGS(INPROCESS_PRODUCTIVITY)
    std::array<uint32_t, 3> vivify_tier_shares{{3, 3, 1}};
    std::array<uint8_t, 3> vivify_tier_scopes{{3, 4, 5}};

    TACTIX_DECLARE_DEFAULT_SETTINGS(INPROCESS_GATES)
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
