#pragma once

#include "policies/subpolicy.hpp"
#include "rules/registry.hpp"

#include <cstdlib>

namespace tactix {

class DefaultBveBoundRamp final : public BveBoundPolicy {
public:
  const char *name() const override { return "kissat-ramp"; }
  static uint32_t ceiling() {
    if (const char *s = std::getenv("TACTIX_BVE_RAMP_MAX"))
      return static_cast<uint32_t>(std::atoi(s));
    return 8;
  }
  uint32_t num_passes() const override {
    const uint32_t cap = ceiling();
    static constexpr uint32_t levels[6] = {0, 1, 2, 4, 8, 16};
    uint32_t n = 0;
    for (uint32_t i = 0; i < 6; i++)
      if (levels[i] <= cap)
        n = i + 1;
    return n ? n : 1;
  }
  uint32_t bound(uint32_t pass) const override {
    static constexpr uint32_t levels[6] = {0, 1, 2, 4, 8, 16};
    return levels[pass < 6 ? pass : 5];
  }
};

class DefaultFactorMatch final : public FactorMatchPolicy {
public:
  const char *name() const override { return "watches-size"; }
  double seed_score(const Kernel &k, Lit s) override {
    return k.watches.heads[s].size;
  }
  double next_factor_score(const Kernel &k, Lit c, uint32_t, Lit) override {
    return k.watches.heads[c].size;
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

class DefaultCandidateOrder final : public CandidateOrderPolicy {
public:
  const char *name() const override { return "per-rule-arms"; }
  void rank(const Kernel &, uint32_t rule, uint64_t *insts,
            uint32_t n) override;
  std::vector<CandidateOrder> arms;
  CandidateOrder arm_of(uint32_t rule) const {
    return rule < arms.size() ? arms[rule] : CandidateOrder::Native;
  }
  void reset() { rng_ = SEED; }

private:
  static constexpr uint64_t SEED = 0x9e3779b97f4a7c15ull;
  std::vector<std::pair<uint64_t, uint64_t>> scratch_;
  uint64_t rng_ = SEED;
};

enum class SlotGroup : uint8_t { Probe, Eliminate };

struct SlotOrderRow {
  const char *name;
  SlotGroup group;
};
const SlotOrderRow *default_slot_order(size_t *count);

class DefaultInprocessScheduler {
public:
  static constexpr uint64_t SUBSUME_INTERVAL = 500;
  static constexpr uint64_t PROBE_INTERVAL = 100;
  static constexpr uint64_t MIN_EFFORT_TICKS = 10000000;
  static constexpr int PHASE_ROUNDS = 2;
  static constexpr uint64_t FACTOR_INIT_TICKS = 700000000ull;
  static constexpr uint64_t CONGRUENCE_INIT_TICKS = 1000000000000ull;

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
      uint64_t dec_material = 0, dec_warmup = 0;
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
  bool want_phase(const Kernel &, SlotGroup);
  int run_phase(Kernel &, SlotGroup, volatile sig_atomic_t *stop);
  void update_limit(const Kernel &, SlotGroup);
  void order_candidates(const Kernel &, SlotEntry &, uint64_t *, uint32_t);
  void set_entry_limit(SlotEntry &, Kernel &);
  void report(const Kernel &) const;
  void pose_initialized(bool value = true) { initialized_ = value; }
  bool initialized() const { return initialized_; }

private:
  struct SlotPhaseCtx {
    SlotGroup group;
    volatile sig_atomic_t *stop;
    int outer = 0;
    bool phase_unsat = false;
    bool phase_dosed = false;
    bool round_changed = false;
  };
  enum class SlotStep : uint8_t { Next, Stop };

  bool slot_entry_admissible(SlotEntry &, const Kernel &, const SlotPhaseCtx &);
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
  bool slot_order_legacy_ = false;
  bool closure_delay_ = true;
  uint64_t factor_init_ticks_ = FACTOR_INIT_TICKS;
  uint64_t congruence_init_ticks_ = CONGRUENCE_INIT_TICKS;
  uint32_t bve_level_ = 0;
  uint32_t bve_level_idx_ = 0;
  bool bve_ran_this_phase_ = false;
  uint32_t factor_bound_snapshot_ = 0;
  bool couple_factor_bound_ = true;
  OccIndex occurrence_index_;
  uint64_t forward_pair_limit_ = 0;
  bool slot_clocks_split_ = true;
  bool slot_watermark_ = true;
  bool initialized_ = false;
  DefaultBveBoundRamp default_bve_ramp_;
  DefaultFactorMatch default_factor_match_;
  DefaultCandidateOrder default_candidate_order_;
  std::vector<uint64_t> payloads_;
  std::vector<uint64_t> order_before_;
};

} // namespace tactix
