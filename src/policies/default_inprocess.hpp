#pragma once

#include "policies/default_strategy_config.hpp"
#include "policies/subpolicy.hpp"
#include "rules/registry.hpp"

#include <cstdlib>

namespace tactix {

class DefaultBveBoundRamp final : public BveBoundPolicy {
public:
  explicit DefaultBveBoundRamp(
      const DefaultStrategyConfig::Inprocess *config = nullptr)
      : config_(config) {}
  const char *name() const override { return "kissat-ramp"; }
  uint32_t num_passes() const override;
  uint32_t bound(uint32_t pass) const override;
  void set_config(const DefaultStrategyConfig::Inprocess *config) {
    config_ = config;
  }

private:
  const DefaultStrategyConfig::Inprocess *config_ = nullptr;
};

class DefaultFactorMatch final : public FactorMatchPolicy {
public:
  const char *name() const override { return "watches-size"; }
  double seed_score(const Kernel &, Lit, double reference_score) override {
    return reference_score;
  }
  double next_factor_score(const Kernel &, Lit, uint32_t, Lit,
                           double reference_score) override {
    return reference_score;
  }
  uint32_t bound = 0;
  bool accept(uint32_t, uint32_t, int delta) override {
    return delta >= 1 && delta > static_cast<int>(bound);
  }
};

enum class CandidateOrder : uint8_t {
  Native,
  Reverse,
  Random,
  OccAsc,
  OccDesc,
  NegOccDesc,
  Heuristic
};
const char *candidate_order_name(CandidateOrder);
bool parse_candidate_order(const char *, CandidateOrder *);

// Kissat eliminate.c `variable_score`: preserve its named binary64
// intermediates (`prod`, `sum`, structural `score`, and `occlim2`).  Flattening
// the algebraically equivalent subtractions can move an equal-score candidate
// by one ULP and therefore changes mutable-heap topology.
double kissat_bve_variable_score(double relevance, uint64_t positive,
                                 uint64_t negative,
                                 uint64_t occurrence_limit);

class DefaultCandidateOrder final : public CandidateOrderPolicy {
public:
  const char *name() const override { return "per-rule-arms"; }
  void rank(const Kernel &, uint32_t rule, uint64_t *insts,
            uint32_t n) override;
  std::vector<CandidateOrder> arms;
  CandidateOrder arm_of(uint32_t rule) const {
    return rule < arms.size() ? arms[rule] : CandidateOrder::Native;
  }
  void reset(uint64_t seed) { rng_ = increment_ = seed; }
#ifdef TACTIX_ENABLE_EVENT_TRACE
  size_t diagnostic_storage_bytes() const {
    return arms.capacity() * sizeof(CandidateOrder) +
           scratch_.capacity() * sizeof(std::pair<uint64_t, uint64_t>);
  }
#endif

private:
  std::vector<std::pair<uint64_t, uint64_t>> scratch_;
  uint64_t rng_ = 0;
  uint64_t increment_ = 0;
};

class DefaultArenaOrder final : public ArenaOrderPolicy {
public:
  const char *name() const override { return "vmtf-phase"; }
  void order(const Kernel &, std::vector<Ref> &) override;
};

enum class SlotGroup : uint8_t { Probe, Eliminate };
enum class FollowupKind : uint8_t {
  None,
  AfterCongruence,
  AfterSweepOrFinal,
};
enum class FollowupMode : uint8_t { None, Eager, Final };

struct SlotOrderRow {
  const char *name;
  SlotGroup group;
};
const SlotOrderRow *default_slot_order(size_t *count);

class DefaultInprocessScheduler {
public:
  // Compatibility aliases for tests and external design probes.  The numbers
  // themselves have one home in DefaultStrategyConfig.
  static constexpr uint64_t SUBSUME_INTERVAL =
      DefaultStrategyConfig::INPROCESS_ELIMINATE_INTERVAL_DEFAULT;
  static constexpr uint64_t PROBE_INTERVAL =
      DefaultStrategyConfig::INPROCESS_PROBE_INTERVAL_DEFAULT;
  static constexpr uint64_t MIN_EFFORT_TICKS =
      DefaultStrategyConfig::INPROCESS_MINIMUM_EFFORT_DEFAULT;
  static constexpr uint64_t PREPROCESS_INIT_TICKS =
      DefaultStrategyConfig::INPROCESS_PREPROCESS_EFFORT_DEFAULT;
  static constexpr int PHASE_ROUNDS =
      DefaultStrategyConfig::INPROCESS_PHASE_ROUNDS_DEFAULT;
  static constexpr uint64_t FACTOR_INIT_TICKS =
      DefaultStrategyConfig::FACTOR_INITIAL_EFFORT_DEFAULT;

  DefaultInprocessScheduler() = default;
  explicit DefaultInprocessScheduler(
      const DefaultStrategyConfig::Inprocess *config)
      : config_(config), default_bve_ramp_(config) {}
  void set_config(const DefaultStrategyConfig::Inprocess *config) {
    config_ = config;
    default_bve_ramp_.set_config(config);
  }

  struct SlotEntry {
    int rule = -1;
    bool enabled = true;
    SlotGroup group = SlotGroup::Probe;
    uint32_t permille = 0;
    uint64_t (*meter)(const Kernel &) = nullptr;
    uint64_t Stats::*tick_counter = nullptr;
    bool dosed = false;
    bool budget_aware = false;
    bool closure = false;
    uint64_t limit = 0;
    bool defer_preprocess = false;
    bool preprocess = false;
    FollowupKind followup = FollowupKind::None;
    FollowupMode followup_mode = FollowupMode::None;
    uint64_t arena_mark = 0;
    uint32_t max_applies = 0;
    uint32_t applies = 0;
    uint64_t cursor = 0;
    bool cursor_enabled = false;
    CandidateOrder order = CandidateOrder::Native;
    struct Telem {
      uint64_t occasions = 0;
      uint64_t admitted = 0;
      uint64_t dec_disabled = 0, dec_cap = 0, dec_preprocess = 0;
      uint64_t dec_guard = 0, dec_budget = 0, dec_delay = 0;
      uint64_t dec_material = 0, dec_warmup = 0, dec_profile = 0;
      uint64_t followup_offered = 0, followup_taken = 0;
      uint64_t followup_eager = 0, followup_final = 0;
      uint64_t handed = 0, spent = 0, ok = 0, completed = 0;
      uint64_t wrapped = 0;
    } tm;
    struct OrderTelem {
      uint64_t lists = 0, candidates = 0, rotated = 0, ranked = 0;
      uint64_t moved = 0, advanced = 0;
    } ord;
    uint64_t binaries_mark = 0;
    uint64_t root_fixed_mark = 0;
    uint32_t doses = 0;
    uint32_t delay_count = 0;
    uint32_t delay_current = 0;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
    uint64_t journal_reference = 0;
    uint64_t journal_allowance = 0;
    uint64_t journal_last_round_spent = 0;
    uint32_t journal_rounds = 0;
    bool journal_round_budget_break = false;
#endif
  };

  struct SlotClock {
    uint64_t next_conflicts = 0;
    uint64_t rounds = 0;
    uint64_t reductions_mark = 0;
    uint64_t marks_mark = 0;
    uint64_t fixed_mark = 0;
    bool blocked = false;
    uint64_t blocked_episodes = 0;
    uint64_t anchor = 0;
  };

  BveBoundPolicy *bve_bound = nullptr;
  FactorMatchPolicy *factor_match = nullptr;
  CandidateOrderPolicy *candidate_order = nullptr;
  std::vector<SlotEntry> entries;
  SlotClock probe_clock, elim_clock;

  void reset(const Kernel &);
  void configure_from_env();
  void build(Kernel &);
  void initialize_search_limits(const Kernel &);
  bool want_phase(const Kernel &, SlotGroup);
  int run_phase(Kernel &, SlotGroup, volatile sig_atomic_t *stop);
  void update_limit(const Kernel &, SlotGroup);
  void order_candidates(const Kernel &, SlotEntry &, uint64_t *, uint32_t);
  void set_entry_limit(SlotEntry &, Kernel &);
  void report(const Kernel &) const;
  static FollowupMode preprocessing_followup_mode(FollowupKind, bool, bool);
  ArenaOrdering *arena_ordering() { return &default_arena_order_; }
  void pose_initialized(bool value = true) { initialized_ = value; }
  bool initialized() const { return initialized_; }
#ifdef TACTIX_ENABLE_EVENT_TRACE
  size_t diagnostic_non_dense_storage_bytes() const {
    return entries.capacity() * sizeof(SlotEntry) +
           default_candidate_order_.diagnostic_storage_bytes() +
           payloads_.capacity() * sizeof(uint64_t) +
           order_before_.capacity() * sizeof(uint64_t);
  }
#endif

private:
  const DefaultStrategyConfig::Inprocess &config() const;
  struct SlotPhaseCtx {
    SlotGroup group;
    volatile sig_atomic_t *stop;
    int outer = 0;
    bool phase_unsat = false;
    bool phase_dosed = false;
    bool round_changed = false;
    bool congruence_changed = false;
    bool sweep_changed = false;
    ApplyResult last_result = ApplyResult::NoOp;
  };
  enum class SlotStep : uint8_t { Next, Stop };

  bool slot_entry_admissible(SlotEntry &, const Kernel &, const SlotPhaseCtx &);
  bool followup_admissible(SlotEntry &, const Kernel &, SlotPhaseCtx &);
  static uint64_t ledger_remaining(SlotEntry &, const Kernel &, uint64_t);
  SlotStep run_dosed_entry(Kernel &, SlotEntry &, const RuleVTable &,
                           SlotPhaseCtx &);
  SlotStep run_batch_entry(Kernel &, SlotEntry &, const RuleVTable &,
                           SlotPhaseCtx &);
  bool factor_entry_declines(const Kernel &, SlotEntry &);
  void factor_dose_config(Kernel &, SlotEntry &);
  ApplyResult run_budgeted_batch(Kernel &, SlotEntry &, const RuleVTable &,
                                 uint64_t, bool);
  ApplyResult run_vivify_dose(Kernel &, SlotEntry &, const RuleVTable &,
                              uint64_t);
  uint64_t search_ticks(const Kernel &k) const { return k.stats.search_ticks; }

  uint32_t bve_rounds_run_ = 0;
  bool factor_large_formula_ = false;
  bool slot_order_legacy_ = false;
  bool closure_delay_ = true;
  uint64_t factor_init_ticks_ = FACTOR_INIT_TICKS;
  uint32_t bve_level_ = 0;
  uint32_t bve_level_idx_ = 0;
  bool bve_ran_this_phase_ = false;
  uint32_t factor_bound_snapshot_ = 0;
  bool couple_factor_bound_ = true;
  OccIndex occurrence_index_;
  uint64_t forward_pair_limit_ = 0;
  bool slot_clocks_split_ = true;
  bool slot_watermark_ = true;
  bool preprocess_followups_ = true;
  bool preprocess_only_ = false;
  bool initialized_ = false;
  bool search_limits_initialized_ = false;
  const DefaultStrategyConfig::Inprocess *config_ = nullptr;
  DefaultBveBoundRamp default_bve_ramp_;
  DefaultFactorMatch default_factor_match_;
  DefaultCandidateOrder default_candidate_order_;
  DefaultArenaOrder default_arena_order_;
  std::vector<uint64_t> payloads_;
  std::vector<uint64_t> order_before_;
};

} // namespace tactix
