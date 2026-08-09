#pragma once

#include "policies/default_inprocess.hpp"
#include "policies/policy.hpp"

namespace tactix {

class DefaultBranching final : public BranchingPolicy {
public:
  const char *name() const override { return "vmtf-evsids"; }
  Lit pick(Kernel &) override;
  static constexpr uint64_t RANDEC_INIT = 500;
  static constexpr uint64_t RANDEC_LENGTH = 10;
  static constexpr uint64_t RANDEC_SEED = 0x9E3779B97F4A7C15ull;
  uint64_t randec_until = 0;
  uint64_t randec_next = RANDEC_INIT;
  uint64_t randec_sequences = 0;
  uint64_t randec_rng = RANDEC_SEED;
  bool randec_enabled = true;
  void reset(uint64_t conflicts) {
    randec_until = 0;
    randec_next = conflicts + RANDEC_INIT;
    randec_sequences = 0;
    randec_rng = RANDEC_SEED;
  }
};

class GlueTieredReduction final : public ReductionPolicy {
public:
  const char *name() const override { return "glue-tiered"; }
  size_t victims(const Kernel &, size_t candidates) override;
  bool ramp = false;
};

class DefaultPolicy final : public Policy {
public:
  static constexpr double RESTART_MARGIN = 1.10;
  static constexpr uint64_t RESTART_INTERVAL = 1;
  static constexpr double EMA_FAST_WINDOW = 33;
  static constexpr double EMA_SLOW_WINDOW = 1e5;
  static constexpr uint64_t REDUCE_INTERVAL_INIT = 2000;
  static constexpr uint64_t REDUCE_INTERVAL_STEP = 300;
  static constexpr uint64_t REDUCE_INTERVAL_KISSAT = 1000;
  static constexpr uint64_t MODE_INITIAL_CONFLICTS = 1000;
  static constexpr uint64_t MODE_INTERVAL = 1000;
  static constexpr uint64_t RELUCTANT_PERIOD = 1024;
  static constexpr uint64_t RELUCTANT_LIMIT = 1 << 20;
  static constexpr uint64_t REPHASE_INTERVAL_INIT = 1000;
  static constexpr uint64_t REPHASE_INTERVAL = 1000;
  static constexpr uint64_t REORDER_INTERVAL_INIT = 10000;
  static constexpr uint64_t REORDER_INTERVAL = 10000;
  static constexpr uint32_t BUMP_REASONS_LIMIT = 10;
  static constexpr double BUMP_REASONS_RATE = 20.0;
  static constexpr uint32_t CHRONO_LEVELS = 100;
  static constexpr uint64_t WALK_PERMILLE = 560;

  int solve(Kernel &, volatile sig_atomic_t *stop = nullptr) override;
  void reset_schedule(const Kernel &);
  DefaultInprocessScheduler &inprocess() { return inprocess_; }
  const DefaultInprocessScheduler &inprocess() const { return inprocess_; }

  bool reset_schedule_per_solve = true;
  uint64_t solves = 0;
  uint32_t chrono_levels = CHRONO_LEVELS;
  bool chrono_reuse_trail_ = true;
  BranchingPolicy *branching = nullptr;
  ReductionPolicy *reduction = nullptr;

  struct Smooth {
    double value = 0, biased = 0, alpha = 0, beta = 0, exp = 1.0;
    void init(double window) {
      alpha = 1.0 / window;
      beta = 1.0 - alpha;
      value = biased = 0;
      exp = 1.0;
    }
    void update(double y) {
      biased += alpha * (y - biased);
      if (!exp) {
        value = biased;
        return;
      }
      const double old_exp = exp;
      exp *= beta;
      if (exp == old_exp) {
        exp = 0;
        value = biased;
      } else {
        value = biased / (1.0 - exp);
      }
    }
  };
  struct ModeAverages {
    bool initialized = false;
    Smooth fast_glue, slow_glue, decision_rate;
    uint64_t saved_decisions = 0;
  };

  ModeAverages averages[2];
  struct { uint64_t next_conflicts = RESTART_INTERVAL; } restart;
  struct { uint64_t next_conflicts = REDUCE_INTERVAL_INIT; } reduce;
  struct {
    uint64_t count = 0, conflicts_limit = MODE_INITIAL_CONFLICTS;
    uint64_t ticks_limit = 0, ticks_at_focus_entry = 0;
  } mode;
  struct {
    uint64_t u = 1, v = 1, wait = RELUCTANT_PERIOD;
    bool trigger = false;
  } reluctant;
  struct { uint64_t next_conflicts = REPHASE_INTERVAL_INIT, count = 0; } rephase;
  struct { uint64_t next_conflicts = REORDER_INTERVAL_INIT, count = 0; } reorder;
  uint64_t walk_anchor = 0;

  bool want_restart(const Kernel &) const;
  bool want_reduce(const Kernel &) const;
  bool want_switch_mode(const Kernel &) const;
  bool want_rephase(const Kernel &) const;
  bool want_reorder(const Kernel &) const;
  void tick_reluctant();
  void update_mode_limits(const Kernel &);
  void update_restart_limit(const Kernel &);
  void update_reduce_limit(const Kernel &);
  void update_rephase_limit(const Kernel &);
  void update_reorder_limit(const Kernel &);
  uint32_t bump_reason_depth(const Kernel &) const;

private:
  uint64_t search_ticks(const Kernel &k) const { return k.stats.search_ticks; }
  void configure_from_env();
  void observe_glue(const Kernel &, uint32_t);
  void observe_decision_rate(const Kernel &);
  uint32_t want_bump_reasons(const Kernel &);

  uint32_t force_mode_ = 0;
  uint32_t bump_reasons_limit_ = 0;
  double bump_reasons_rate_ = BUMP_REASONS_RATE;
  struct { uint32_t count = 0, current = 0; } bump_reasons_delay_;
  bool reduce_sqrt_ = false;
  bool compact_ = true;
  bool dynamic_tiers_ = true;
  struct { uint64_t next_conflicts = 2, interval = 2; } retier_;
  DefaultBranching default_branching_;
  GlueTieredReduction default_reduction_;
  DefaultInprocessScheduler inprocess_;
  std::vector<Ref> reduce_scratch_;
};

int default_solve(Kernel &, volatile sig_atomic_t *stop = nullptr);

} // namespace tactix
