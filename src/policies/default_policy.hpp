#pragma once

#include "policies/progress.hpp"
#include "policies/default_inprocess.hpp"
#include "policies/default_periodic.hpp"
#include "policies/default_restart.hpp"
#include "policies/policy.hpp"
#include "strategy/policy_completion.hpp"
#include "rules/lucky/graph_labeling_model.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"
#include <optional>
#include <array>

namespace tactix {

class EventTrace;

struct FormulaClassification {
  bool small = false;
  bool bigbig = false;
};

// Sparse-resume propagation is a physical lifecycle boundary shared with the
// reference. `charge_search_ticks` selects Kissat's ordinary-search Eliminate
// replay versus its probing replay; the caller supplies the retained-trail
// position corresponding to the reference's cleared trail begin.
ApplyResult default_resume_sparse_propagation(Kernel &, uint32_t trail_begin,
                                              bool charge_search_ticks);

// Reduce's root preparation can shorten the trail before deriving new units.
// Consume only those new root identities into DefaultPolicy's rankings.
void default_prepare_reduction(Kernel &);

// Outcome feedback for the one startup BCE pass following unique-literal AMO
// completion.  Exposed as a pure seam so all lifecycle cases remain pinned
// without manufacturing a particular symmetry group in a policy test.
bool default_should_cleanup_unique_literal_amo(
    uint64_t binaries_added, bool later_progress,
    bool generic_graph_root_progress);

// Decide whether structured startup should replace the initial search seed
// with a residual-CNF import seed.  Helper-backed progress earns that reparse
// only when generic graph fixing also reaches a root consequence; otherwise
// the durable certified cut is useful while the original search pose remains
// the stronger owner.  Progress without a helper layer retains the ordinary
// Satsuma-to-Kissat handoff.
bool default_should_seed_structured_residual(
    bool grew_root_trail, uint64_t unique_literal_amo_binaries,
    bool generic_graph_root_progress);

// The SC2024 pass owns startup only when its bounded candidate domain covers
// more live variables than it excludes. Otherwise the ordinary Kissat BVE
// schedule retains the formula without an up-front search-state perturbation.
bool default_should_run_initial_fast_elimination(uint64_t candidates,
                                                 uint64_t active_variables);
bool default_fast_elimination_discovery_admissible(
    uint64_t active_variables, uint64_t remaining_ticks);
bool default_should_suppress_fast_elimination_after_structured(
    bool structured_root_grew_trail, uint64_t extension_candidates);

// Formula-independent bounded profile for persistent authenticated subimages.
// No clause census or recognition is performed by this pure policy helper.
std::optional<strategy::AffineConsequenceParameters>
default_affine_search_parameters(const Kernel &, const DefaultStrategyConfig &);

struct DefaultAffineSearchDiagnostics {
  uint64_t initializations = 0, refreshes = 0, unchanged_refreshes = 0;
  uint64_t empty_maintenance_skips = 0, unchanged_census_skips = 0;
  uint64_t usefulness_resumptions = 0, paused_search_skips = 0;
  uint64_t root_services = 0, search_services = 0;
  uint64_t no_event = 0, duplicates = 0, resource_declines = 0;
  uint64_t service_inapplicable = 0, service_interrupted = 0;
  uint64_t service_unsupported_proof = 0, service_stale = 0;
  uint64_t service_invalid = 0;
  // Stable numeric enum values plus the live-publication audit stage.  These
  // distinguish a matrix synchronization failure (audit stage zero) from a
  // proof/publication decline without retaining an unbounded event trace.
  uint64_t last_service_status = 0, last_terminal_status = 0;
  uint64_t last_terminal_audit_stage = 0;
  uint64_t last_terminal_transaction_stage = 0;
  uint64_t last_terminal_synchronization_stage = 0;
  uint64_t usefulness_disabled = 0;
  uint64_t reasons_published = 0, implications_consumed = 0;
  uint64_t reasons_deferred = 0, reasons_materialized = 0;
  uint64_t reasons_detached = 0;
  uint64_t materialization_attempts = 0, materialization_inapplicable = 0;
  uint64_t materialization_stale = 0, materialization_invalid = 0;
  uint64_t materialization_exhausted = 0, materialization_interrupted = 0;
  uint64_t materialization_unsupported_proof = 0;
  uint64_t materialization_retained_authentications = 0;
  uint64_t materialization_last_audit_stage = 0;
  uint64_t initialization_work = 0, service_work = 0, overhead_nanos = 0;
  uint64_t lifecycle_work = 0, trail_capture_work = 0, synchronization_work = 0;
  uint64_t synchronization_nanos = 0, publication_nanos = 0;
  uint64_t incremental_capture_calls = 0, unchanged_support_calls = 0, capture_delta_literals = 0;
  uint64_t cached_event_calls = 0;
  // Natural full-restart root windows: runs whose retained inputs changed,
  // expired-limit windows that waited for an input change, the input-change
  // scan, Kissat-delayed occurrences, fruitfulness verdicts, the current
  // conflict limit and delay, run effort limits/charged work and steps.
  uint64_t restart_runs = 0, unchanged_skips = 0, trigger_work = 0;
  uint64_t delayed_runs = 0, fruitful_runs = 0, fruitless_runs = 0;
  uint64_t run_conflict_limit = 0, delay_current = 0;
  uint64_t run_effort_last = 0, run_effort_total = 0, run_work = 0, run_steps = 0;
  uint64_t pending_slices=0, resumed_slices=0, completed_slice_steps=0;
  uint64_t slice_work=0, slice_cumulative_work=0, minimum_slice_work=0;
  uint64_t proof_additions = 0, proof_deletions = 0;
  uint64_t root_unit_work = 0, root_unit_additions = 0, premise_maintenance_work = 0;
  uint64_t alias_work = 0, alias_proof_additions = 0;
  uint64_t recognized_equations = 0, admitted_sources = 0, active_sources = 0;
  uint64_t retired_sources = 0, projected_variables = 0;
  // Last observed native-boundary gauges, independent of cumulative
  // admitted-source counts; source count means retained archived equations.
  uint64_t retained_basis_rank = 0, retained_support_variables = 0;
  uint64_t rank_before_last_action = 0, rank_after_last_action = 0;
  uint64_t support_before_last_action = 0, support_after_last_action = 0;
  // Cumulative net rank changes at completed native action boundaries, including
  // finite declines that explicitly disable a retained session.
  uint64_t recognition_rank_gains = 0, recognition_rank_losses = 0;
  uint64_t maintenance_rank_gains = 0, maintenance_rank_losses = 0;
  uint64_t service_rank_losses = 0, isolated_projection_rank_losses = 0;
  uint64_t native_session_disables = 0, optional_failure_disables = 0;
  // Last-pause snapshots, retained after resumption for attribution.
  uint64_t paused_basis_rank = 0, paused_support_variables = 0;
  // Last action: 1 recognition, 2 maintenance, 3 root service, 4 search service.
  // Historical last disable: 1 native action cleared owner,
  // 2 Default terminal failure; retained after a new admission.
  uint64_t last_survival_action = 0, last_disable_cause = 0;

  uint64_t conditioned_rows = 0, unchanged_reductions = 0, widest_reason = 0;
};

struct DefaultGraphLabelingDiagnostics {
  uint64_t authentication_attempts = 0;
  uint64_t authentication_successes = 0;
  uint64_t authentication_exhausted = 0;
  uint64_t authentication_no_structure = 0;
  uint64_t authentication_interrupted = 0;
  uint64_t authentication_work = 0;
  uint64_t slices = 0;
  uint64_t pending_slices = 0;
  uint64_t completed_trajectories = 0;
  uint64_t useful_hints = 0;
  // Runs (each a full owner dose) and their Kissat-derived schedule:
  // delayed occurrences, fruitfulness verdicts, conflict limit and delay.
  uint64_t runs = 0, delayed_runs = 0, fruitful_runs = 0, fruitless_runs = 0;
  uint64_t run_conflict_limit = 0, delay_current = 0;
};

// Root-service run schedule (kissat kimits.h/kimits.c): occurrences drive the
// NLOG2N conflict interval with kissat_scale_delta, `last_ticks` anchors
// SET_EFFORT_LIMIT, and the delay doubles after a fruitless run and halves
// after a fruitful one (plan §9.5).
struct DefaultRootServiceSchedule {
  uint64_t occurrences = 0, next_conflicts = 0, last_ticks = 0;
  uint32_t delay_current = 0, delay_count = 0;
};

class DefaultBranching final : public BranchingPolicy {
public:
  explicit DefaultBranching(
      const DefaultStrategyConfig::Branching *config = nullptr)
      : config_(config) {}
  const char *name() const override { return "vmtf-evsids"; }
  Lit pick(Kernel &) override;
  Lit pick_for_restart_reuse(Kernel &) override;
  // The forced polarity decide_lit applies at the current mode and switch
  // count: +1 positive, -1 negative, 0 H's phase. Warm-up takes no random
  // decision, so its picks are H's next variable with this polarity.
  int8_t forced_phase(const Kernel &) const;
  static constexpr uint64_t RANDEC_INIT =
      DefaultStrategyConfig::RANDOM_INITIAL_INTERVAL_DEFAULT;
  static constexpr uint64_t RANDEC_LENGTH =
      DefaultStrategyConfig::RANDOM_SEQUENCE_LENGTH_DEFAULT;
  static constexpr uint64_t RANDEC_SEED =
      DefaultStrategyConfig::RANDOM_SEED_DEFAULT;
  // Kissat random.h's fixed 64-bit LCG.  These define the generator rather
  // than a tunable strategy arm, so they do not belong in DefaultStrategyConfig.
  static constexpr uint64_t RANDEC_MULTIPLIER = 6364136223846793005ull;
  static constexpr uint64_t RANDEC_INCREMENT = 1442695040888963407ull;
  uint64_t randec_remaining = 0;
  uint64_t randec_next = RANDEC_INIT;
  uint64_t randec_sequences = 0;
  uint64_t randec_rng = RANDEC_SEED;
  bool randec_enabled = true;
  void start_random_sequence(Kernel &);
  void on_search_propagation_conflict() override {
    if (randec_remaining)
      --randec_remaining;
  }
  void set_config(const DefaultStrategyConfig::Branching *config) {
    config_ = config;
    focused_phase_epoch_ = UINT64_MAX;
  }
  void reset(uint64_t conflicts) {
    const DefaultStrategyConfig::Branching defaults;
    const auto &cfg = config_ ? *config_ : defaults;
    randec_remaining = 0;
    randec_next = conflicts + cfg.random_initial_interval;
    randec_sequences = 0;
    randec_rng = cfg.random_seed;
    focused_phase_epoch_ = UINT64_MAX;
  }

private:
  Lit pick_impl(Kernel &, bool restart_reuse);
  Lit decide_lit(const Kernel &, Var);
  const DefaultStrategyConfig::Branching *config_ = nullptr;
  uint64_t focused_phase_epoch_ = UINT64_MAX;
  int8_t focused_phase_ = 0;
};

class GlueTieredReduction final : public ReductionPolicy {
public:
  explicit GlueTieredReduction(
      const DefaultStrategyConfig::Reduction *config = nullptr)
      : config_(config) {}
  const char *name() const override { return "glue-tiered"; }
  size_t victims(const Kernel &, size_t candidates) override;
  bool ramp = false;
  void set_config(const DefaultStrategyConfig::Reduction *config) {
    config_ = config;
  }

private:
  const DefaultStrategyConfig::Reduction *config_ = nullptr;
};

class DefaultPolicy final : public Policy,
                            public DefaultRestartControl,
                            public DefaultModeControl,
                            public DefaultDatabaseControl,
                            public DefaultPhaseControl {
public:
  static constexpr double RESTART_MARGIN =
      DefaultStrategyConfig::RESTART_MARGIN_DEFAULT;
  static constexpr uint64_t RESTART_INTERVAL =
      DefaultStrategyConfig::RESTART_INTERVAL_DEFAULT;
  static constexpr double EMA_FAST_WINDOW =
      DefaultStrategyConfig::FAST_EMA_WINDOW_DEFAULT;
  static constexpr double EMA_SLOW_WINDOW =
      DefaultStrategyConfig::SLOW_EMA_WINDOW_DEFAULT;
  static constexpr uint64_t REDUCE_INTERVAL_INIT =
      DefaultStrategyConfig::REDUCE_INTERVAL_DEFAULT;
  static constexpr uint64_t REDUCE_INTERVAL_ARITH =
      DefaultStrategyConfig::REDUCE_ARITHMETIC_DEFAULT;
  static constexpr uint64_t REDUCE_INTERVAL_STEP =
      DefaultStrategyConfig::REDUCE_ARITHMETIC_STEP_DEFAULT;
  static constexpr uint64_t MODE_INITIAL_CONFLICTS =
      DefaultStrategyConfig::MODE_INITIAL_CONFLICTS_DEFAULT;
  static constexpr uint64_t MODE_INTERVAL =
      DefaultStrategyConfig::MODE_INTERVAL_DEFAULT;
  static constexpr uint64_t RELUCTANT_PERIOD =
      DefaultStrategyConfig::RELUCTANT_PERIOD_DEFAULT;
  static constexpr uint64_t RELUCTANT_LIMIT =
      DefaultStrategyConfig::RELUCTANT_LIMIT_DEFAULT;
  static constexpr uint64_t REPHASE_INTERVAL_INIT =
      DefaultStrategyConfig::REPHASE_INTERVAL_DEFAULT;
  static constexpr uint64_t REPHASE_INTERVAL =
      DefaultStrategyConfig::REPHASE_INTERVAL_DEFAULT;
  static constexpr uint64_t REORDER_INTERVAL_INIT =
      DefaultStrategyConfig::REORDER_INITIAL_INTERVAL_DEFAULT;
  static constexpr uint64_t REORDER_INTERVAL =
      DefaultStrategyConfig::REORDER_INITIAL_INTERVAL_DEFAULT;
  static constexpr uint32_t BUMP_REASONS_LIMIT =
      DefaultStrategyConfig::BUMP_REASON_LIMIT_DEFAULT;
  static constexpr double BUMP_REASONS_RATE =
      DefaultStrategyConfig::BUMP_REASON_RATE_DEFAULT;
  static constexpr uint32_t CHRONO_LEVELS =
      DefaultStrategyConfig::CHRONOLOGICAL_LEVELS_DEFAULT;
  static constexpr uint64_t WALK_PERMILLE =
      DefaultStrategyConfig::WALK_PERMILLE_DEFAULT;

  DefaultPolicy();
  explicit DefaultPolicy(const DefaultStrategyConfig &);
  // The default arms and scheduler point into config_. A compiler-generated
  // copy or move would leave those pointers attached to the old owner.
  DefaultPolicy(const DefaultPolicy &) = delete;
  DefaultPolicy &operator=(const DefaultPolicy &) = delete;
  DefaultPolicy(DefaultPolicy &&) = delete;
  DefaultPolicy &operator=(DefaultPolicy &&) = delete;
  const DefaultStrategyConfig &strategy_config() const { return config_; }
  std::string strategy_config_snapshot() const { return config_.serialize(); }
  void set_feedback(strategy::controller::FeedbackReceiver *receiver) {
    feedback_receiver_ = receiver;
  }
  strategy::controller::Observation observe(const Kernel &) const;

  int solve(Kernel &, volatile sig_atomic_t *stop = nullptr) override;
  const DefaultAffineSearchDiagnostics &affine_search_diagnostics() const {
    return affine_search_diagnostics_;
  }
  const DefaultGraphLabelingDiagnostics &graph_labeling_diagnostics() const {
    return graph_labeling_diagnostics_;
  }

  void reset_schedule(const Kernel &);
  // White-box seams for Kissat's exact `classify.c` predicates. The count
  // overload pins threshold behavior without constructing 100k clauses.
  FormulaClassification classify_formula_counts(
      uint64_t binary_clauses, uint64_t irredundant_large_clauses) const;
  FormulaClassification classify_formula(const Kernel &) const;
  DefaultInprocessScheduler &inprocess() { return inprocess_; }
  const DefaultInprocessScheduler &inprocess() const { return inprocess_; }

  bool reset_schedule_per_solve = true;
  uint64_t solves = 0;
  uint32_t chrono_levels = CHRONO_LEVELS;
  ReductionPolicy *reduction = nullptr;


  // Kissat owns this delay at the finalized-clause reason-side gate, not at
  // search-conflict entry.  Keeping the transition explicit makes conflict
  // reuse and failed-literal exits unable to consume the countdown.
  struct ReasonBumpDelay {
    uint32_t count = 0, current = 0;
    void reached_gate(bool ran, bool aborted) {
      if (count) {
        --count;
        return;
      }
      if (!ran)
        return;
      if (aborted)
        current += current < UINT32_MAX;
      else if (current)
        current /= 2;
      count = current;
    }
  };

  bool want_restart(const Kernel &) const;
  bool want_reduce(const Kernel &) const;
  bool want_switch_mode(const Kernel &) const;
  bool want_rephase(const Kernel &) const;
  DefaultRephaseSlot current_rephase_slot() const {
    return config_.rephase.rotation[rephase.count %
                                    config_.rephase.rotation.size()];
  }
  // Policy composition, not an additional guard on public Restart/Rephase.
  ApplyResult prepare_rephase_root(Kernel &);
  bool want_reorder(const Kernel &) const;
  void tick_reluctant();
  void update_mode_limits(const Kernel &);
  void update_restart_limit(const Kernel &);
  void update_reduce_limit(const Kernel &);
  void update_rephase_limit(const Kernel &);
  void update_reorder_limit(const Kernel &);
  uint64_t walk_effort_reference(const Kernel &) const;
  const RetierSchedule &retier_schedule() const { return retier_; }
  void update_formula_classification(Kernel &) const;

private:
  friend struct DefaultAffineSearchTestAccess;
  friend struct DefaultAffineModelTestAccess;
  friend struct DefaultGraphLabelingTestAccess;
  DefaultAffineSearchDiagnostics affine_search_diagnostics_;
  std::optional<std::array<uint64_t, 13>> affine_completed_census_;
  uint64_t affine_refresh_ticks_ = 0, affine_refresh_work_ = 0;
  uint64_t affine_service_allowance_ = 0;
  AffineMatrixInputCursor affine_inputs_;
  DefaultRootServiceSchedule affine_schedule_;
  uint64_t affine_usefulness_calls_ = 0, affine_usefulness_yield_ = 0;
  uint64_t affine_usefulness_checks_ = 0;
  bool affine_usefulness_paused_ = false;
  ApplyResult consume_affine_model_hint(Kernel &);
  AffineMatrixInitResult initialize_affine_search(Kernel &, bool refresh = false, bool recognize = true);
  AffineMatrixServiceResult service_affine_search(Kernel &, bool root);
  AffineMatrixServiceResult materialize_affine_reason(Kernel &);
  bool discharge_affine_reason(Kernel &);
  Lit search_decide(Kernel &);
  bool record_affine_service_result(Kernel &,const AffineMatrixServiceResult &,bool root);
  bool affine_additional_recognition_due(const Kernel &) const;
  ApplyResult run_affine_search(Kernel &, bool startup);
  ApplyResult affine_restart_service(Kernel &);
  void schedule_root_service_occurrence(const Kernel &, DefaultRootServiceSchedule &) const;
  bool root_service_delaying(const Kernel &, DefaultRootServiceSchedule &) const;
  uint64_t root_service_effort(const Kernel &, const DefaultRootServiceSchedule &,
                               uint64_t permille) const;

  DefaultGraphLabelingDiagnostics graph_labeling_diagnostics_;
  DefaultRootServiceSchedule graph_labeling_schedule_;
  GraphLabelingAuthenticationResult authenticate_graph_labeling(Kernel &);
  GraphLabelingSliceResult service_graph_labeling(Kernel &);
  GraphLabelingSliceResult run_graph_labeling(Kernel &);
  bool graph_labeling_due(const Kernel &);
  void schedule_graph_labeling(const Kernel &, const GraphLabelingSliceResult &);
  void reset_graph_labeling_schedule(const Kernel &);

  uint64_t search_ticks(const Kernel &k) const { return k.stats.search_ticks; }
  void configure_from_env(const Kernel &);
  ProgressReport progress_; // -v progress lines; output only
  void observe_glue(const Kernel &, uint32_t);
  void observe_decision_rate(const Kernel &);
  uint32_t want_bump_reasons(const Kernel &) const;

  uint32_t force_mode_ = 0;
  uint32_t bump_reasons_limit_ = 0;
  double bump_reasons_rate_ = BUMP_REASONS_RATE;
  ReasonBumpDelay bump_reasons_delay_;
  bool reduce_sqrt_ = true;
  bool compact_ = true;
  bool lucky_late_ = true;
  bool dynamic_tiers_ = true;
  // Policy-owned glue-tier schedule and snapshots: each native analysis
  // carries it by value and returns it updated; Reduce retiers the current
  // mode on it; every tier pose reads its snapshots.
  RetierSchedule retier_;
  void sync_retier_profile();
  DefaultStrategyConfig base_config_;
  DefaultStrategyConfig config_;
  DefaultBranching default_branching_;
  GlueTieredReduction default_reduction_;
  DefaultInprocessScheduler inprocess_;
  std::vector<Ref> reduce_scratch_;
  strategy::controller::FeedbackReceiver *feedback_receiver_ = nullptr;
  execution::PolicyCompletion *completion_ = nullptr;
};

int default_solve(Kernel &, volatile sig_atomic_t *stop = nullptr);

} // namespace tactix
