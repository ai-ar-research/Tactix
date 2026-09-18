#pragma once

#include "policies/default_inprocess.hpp"
#include "policies/default_periodic.hpp"
#include "policies/default_restart.hpp"
#include "policies/policy.hpp"

namespace tactix {

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

class DefaultBranching final : public BranchingPolicy {
public:
  explicit DefaultBranching(
      const DefaultStrategyConfig::Branching *config = nullptr)
      : config_(config) {}
  const char *name() const override { return "vmtf-evsids"; }
  Lit pick(Kernel &) override;
  Lit pick_for_restart_reuse(Kernel &) override;
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
  const DefaultStrategyConfig &strategy_config() const { return config_; }
  std::string strategy_config_snapshot() const { return config_.serialize(); }

  int solve(Kernel &, volatile sig_atomic_t *stop = nullptr) override;
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
  // Kissat tie-break: ordinary chronological backtracking remains enabled,
  // while CaDiCaL's additional ranked trail-suffix reuse is an opt-in arm.
  bool chrono_reuse_trail_ = false;
  BranchingPolicy *branching = nullptr;
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
  bool update_tiers_after_analysis(Kernel &);
  void update_formula_classification(Kernel &) const;

private:
  uint64_t search_ticks(const Kernel &k) const { return k.stats.search_ticks; }
  void configure_from_env(const Kernel &);
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
  uint64_t structural_seed_cursor_ = 0;
  bool dynamic_tiers_ = true;
  struct { uint64_t next_conflicts = 0, interval = 0; } retier_;
  DefaultStrategyConfig base_config_;
  DefaultStrategyConfig config_;
  DefaultBranching default_branching_;
  GlueTieredReduction default_reduction_;
  DefaultInprocessScheduler inprocess_;
  std::vector<Ref> reduce_scratch_;
};

int default_solve(Kernel &, volatile sig_atomic_t *stop = nullptr);

} // namespace tactix
