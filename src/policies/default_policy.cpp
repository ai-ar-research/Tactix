#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/reduce/reduce.hpp"
#include "policies/ops/rephase/rephase.hpp"
#include "policies/ops/reorder/reorder.hpp"
#include "policies/ops/walk/walk.hpp"
#include "rules/restart/restart.hpp"
#include "rules/registry.hpp"
#include "rules/sweep/sweep.hpp"
#include "policies/ops/switchmode/switchmode.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <utility>

namespace tactix {


void DefaultPolicy::observe_glue(const Kernel &k, uint32_t glue) {
  ModeAverages &avg = averages[k.h->stable];
  if (!avg.initialized) { // lazy, once per mode (kissat init_averages)
    avg.fast_glue.init(EMA_FAST_WINDOW);
    avg.slow_glue.init(EMA_SLOW_WINDOW);
    avg.decision_rate.init(EMA_SLOW_WINDOW);
    avg.initialized = true;
  }
  avg.fast_glue.update(glue);
  avg.slow_glue.update(glue);
}

void DefaultPolicy::observe_decision_rate(const Kernel &k) {
  ModeAverages &avg = averages[k.h->stable];
  if (!avg.initialized) {
    avg.fast_glue.init(EMA_FAST_WINDOW);
    avg.slow_glue.init(EMA_SLOW_WINDOW);
    avg.decision_rate.init(EMA_SLOW_WINDOW);
    avg.initialized = true;
  }
  const uint64_t cur = k.stats.decisions;
  const uint64_t prev = avg.saved_decisions;
  avg.saved_decisions = cur;
  avg.decision_rate.update(static_cast<double>(cur - prev));
}

uint32_t DefaultPolicy::want_bump_reasons(const Kernel &k) {
  if (!bump_reasons_limit_)
    return 0;
  if (bump_reasons_delay_.count) {
    bump_reasons_delay_.count--;
    return 0;
  }
  if (averages[k.h->stable].decision_rate.value >= bump_reasons_rate_)
    return 0;
  return bump_reasons_limit_;
}

void DefaultPolicy::tick_reluctant() {
  if (reluctant.trigger)
    return;
  if (--reluctant.wait)
    return;
  uint64_t u = reluctant.u, v = reluctant.v;
  if ((u & -u) == v) {
    u++;
    v = 1;
  } else {
    v *= 2;
  }
  uint64_t wait = v * RELUCTANT_PERIOD;
  if (wait > RELUCTANT_LIMIT) {
    u = v = 1;
    wait = RELUCTANT_PERIOD;
  }
  reluctant.trigger = true;
  reluctant.wait = wait;
  reluctant.u = u;
  reluctant.v = v;
}

bool DefaultPolicy::want_restart(const Kernel &k) const {
  if (k.stats.conflicts < restart.next_conflicts)
    return false;
  if (k.h->stable)
    return reluctant.trigger;
  const ModeAverages &avg = averages[0];
  return RESTART_MARGIN * avg.slow_glue.value <= avg.fast_glue.value;
}

bool DefaultPolicy::want_switch_mode(const Kernel &k) const {
  if (mode.count & 1) // in stable: leave after the ticks budget
    return search_ticks(k) >= mode.ticks_limit;
  return k.stats.conflicts >= mode.conflicts_limit;
}

void DefaultPolicy::update_mode_limits(const Kernel &k) {
  if (mode.count & 1) { // just entered stable
    const uint64_t focused_ticks = search_ticks(k) - mode.ticks_at_focus_entry;
    mode.ticks_limit = search_ticks(k) + focused_ticks;
  } else { // just entered focused
    const uint64_t n = mode.count / 2 ? mode.count / 2 : 1;
    const double log4 = std::pow(std::log10(static_cast<double>(n) + 9), 4);
    mode.conflicts_limit =
        k.stats.conflicts +
        static_cast<uint64_t>(MODE_INTERVAL * static_cast<double>(n) * log4);
    mode.ticks_at_focus_entry = search_ticks(k);
    update_restart_limit(k);
  }
}

void DefaultPolicy::update_restart_limit(const Kernel &k) {
  uint64_t delta = RESTART_INTERVAL;
  if (k.stats.restarts)
    delta += static_cast<uint64_t>(
        std::log10(static_cast<double>(k.stats.restarts) + 9) - 1.0);
  restart.next_conflicts = k.stats.conflicts + delta;
}

bool DefaultPolicy::want_reduce(const Kernel &k) const {
  return k.stats.conflicts >= reduce.next_conflicts;
}

bool DefaultPolicy::want_rephase(const Kernel &k) const {
  static const bool both = std::getenv("TACTIX_REPHASE_BOTH_MODES") != nullptr;
  return (both || k.h->stable) && k.stats.conflicts > rephase.next_conflicts;
}

bool DefaultPolicy::want_reorder(const Kernel &k) const {
  static const char *const env = std::getenv("TACTIX_REORDER");
  static const int mode = env ? std::atoi(env) : 0;
  if (mode <= 0)
    return false;
  if (!k.h->stable && mode < 2)
    return false;
  if (k.level != 0)
    return false; // kissat's own trigger: reorder only at the root
  return k.stats.conflicts >= reorder.next_conflicts;
}

void DefaultPolicy::update_reorder_limit(const Kernel &k) {
  reorder.next_conflicts =
      k.stats.conflicts + REORDER_INTERVAL * reorder.count;
}

void DefaultPolicy::update_rephase_limit(const Kernel &k) {
  const double n = static_cast<double>(rephase.count);
  const double lg = std::log10(n + 9);
  rephase.next_conflicts =
      k.stats.conflicts +
      static_cast<uint64_t>(REPHASE_INTERVAL * n * lg * lg * lg);
}

void DefaultPolicy::reset_schedule(const Kernel &k) {
  const uint64_t conflicts = k.stats.conflicts;
  const uint64_t ticks = search_ticks(k);

  averages[0] = averages[1] = {};
  update_restart_limit(k);
  reduce.next_conflicts =
      conflicts +
      (reduce_sqrt_ ? REDUCE_INTERVAL_KISSAT : REDUCE_INTERVAL_INIT);
  mode = {};
  mode.conflicts_limit = conflicts + MODE_INITIAL_CONFLICTS;
  mode.ticks_at_focus_entry = ticks;
  reluctant = {};
  rephase = {};
  rephase.next_conflicts = conflicts + REPHASE_INTERVAL_INIT;
  reorder = {};
  reorder.next_conflicts = conflicts + REORDER_INTERVAL_INIT;
  walk_anchor = ticks;
  bump_reasons_delay_ = {};
  retier_ = {};
  if (dynamic_tiers_)
    retier_.next_conflicts = conflicts + retier_.interval;
  default_branching_.reset(conflicts);
  inprocess_.reset(k);
}

static uint64_t active_variables(const Kernel &k) {
  const uint64_t retired = static_cast<uint64_t>(kernel_root_fixed(k)) +
                           k.stats.bve_eliminated + k.stats.substitute_vars;
  const uint64_t n = k.nvars;
  return n > retired ? n - retired : 0;
}

static uint64_t scale_inprocessing_delta(const Kernel &k, uint64_t delta) {
  const double f =
      std::log10(static_cast<double>(kernel_irredundant_clauses(k)) + 1) - 5;
  return static_cast<uint64_t>((4.5 * f * f + 25) *
                               static_cast<double>(delta));
}

namespace {

uint32_t reuse_trail_level(const Kernel &k, uint32_t jump) {
  const uint32_t begin = k.frames[jump + 1];
  const uint32_t end = static_cast<uint32_t>(k.trail.size());
  if (begin >= end || !k.h)
    return jump;
  uint32_t best_pos = begin;
  Var best = 0;
  bool have_best = false;
  for (uint32_t i = begin; i < end; i++) {
    const Var v = var_of(k.trail[i]);
    if (kernel_var_retired(k, v))
      continue;
    if (!have_best) {
      best = v;
      best_pos = i;
      have_best = true;
      continue;
    }
    const bool better =
        k.h->stable
            ? (k.h->scores.score[v] > k.h->scores.score[best] ||
               (k.h->scores.score[v] == k.h->scores.score[best] && v < best))
            : k.h->links[v].stamp > k.h->links[best].stamp;
    if (better) {
      best = v;
      best_pos = i;
    }
  }
  if (!have_best) // the whole suffix is retired variables: nothing to keep
    return jump;
  uint32_t res = jump;
  while (res + 1 < k.level && k.frames[res + 1] <= best_pos)
    res++;
  return res;
}


struct TickSinkScope {
  Kernel &k;
  TickSinkScope(Kernel &kk, uint64_t Stats::*ctr) : k(kk) {
    k.tick_sink = ctr ? &(k.stats.*ctr) : nullptr;
  }
  ~TickSinkScope() { k.tick_sink = nullptr; }
  TickSinkScope(const TickSinkScope &) = delete;
  TickSinkScope &operator=(const TickSinkScope &) = delete;
};

struct InprocessScope {
  Kernel &k;
  explicit InprocessScope(Kernel &kk) : k(kk) { k.inprocessing = true; }
  ~InprocessScope() { k.inprocessing = false; }
  InprocessScope(const InprocessScope &) = delete;
  InprocessScope &operator=(const InprocessScope &) = delete;
};

uint64_t meter_probe(const Kernel &k) { return k.stats.probe_ticks; }
uint64_t meter_backbone(const Kernel &k) { return k.stats.backbone_ticks; }
uint64_t meter_vivify(const Kernel &k) { return k.stats.vivify_ticks; }
uint64_t meter_instantiate(const Kernel &k) { return k.stats.instantiate_ticks; }
uint64_t meter_bve(const Kernel &k) { return k.stats.bve_resolutions; }
uint64_t meter_forward_pair(const Kernel &k) {
  return k.stats.strengthen_visits + k.stats.forward_visits;
}
uint64_t meter_transitive(const Kernel &k) {
  static const bool checked = [] {
    const char *e = std::getenv("TACTIX_V4_METER");
    return e && !std::strcmp(e, "checked");
  }();
  return checked ? k.stats.transitive_checked : k.stats.transitive_ticks;
}
uint64_t meter_factor(const Kernel &k) { return k.stats.factor_ticks; }
uint64_t meter_ternary(const Kernel &k) { return k.stats.ternary_ticks; }
uint64_t meter_hbr(const Kernel &k) { return k.stats.hbr_ticks; }
uint64_t meter_bce(const Kernel &k) { return k.stats.bce_ticks; }
uint64_t meter_cce(const Kernel &k) { return k.stats.cce_ticks; }
uint64_t meter_sweep(const Kernel &k) { return k.stats.sweep_ticks; }
uint64_t meter_congruence(const Kernel &k) { return k.stats.congruence_ticks; }

struct EffortSpec {
  const char *name;
  uint32_t permille;
  uint64_t (*meter)(const Kernel &);
  bool dosed;
  bool budget_aware;
  uint64_t Stats::*tick_counter = nullptr;
  bool closure = false;
};

struct DelaySpec {
  const char *name;
  uint64_t (*yield)(const Kernel &);
  uint64_t (*denom)(const Kernel &);
  uint32_t permille; // bump the delay when yield * 1000 < permille * denom
  bool denom_delta;
};
constexpr DelaySpec DELAY_SPECS[] = {
    {"Congruence", [](const Kernel &k) { return k.stats.congruence_merges; },
     active_variables, 1, false},
    {"Sweep", [](const Kernel &k) { return k.stats.sweep_equivalences; },
     [](const Kernel &k) { return k.stats.sweep_candidates; }, 1, true},
};
static const DelaySpec *delay_spec(const char *name) {
  for (const DelaySpec &d : DELAY_SPECS)
    if (!std::strcmp(d.name, name))
      return &d;
  return nullptr;
}

constexpr EffortSpec EFFORT_SPECS[] = {
    {"BVE", 100, meter_bve, true, false, nullptr},
    {"Probe", 8, meter_probe, true, false, &Stats::probe_ticks},
    {"HyperBinaryRes", 8, meter_hbr, true, true, &Stats::hbr_ticks},
    {"Backbone", 20, meter_backbone, true, false, &Stats::backbone_ticks},
    {"TransitiveReduce", 20, meter_transitive, true, false},
    {"Vivify", 100, meter_vivify, false, true, &Stats::vivify_ticks},
    {"Strengthen", 100, meter_forward_pair, false, true, nullptr},
    {"SubsumeForward", 100, meter_forward_pair, false, true, nullptr},
    {"Factor", 50, meter_factor, false, true, nullptr},
    {"HyperTernaryRes", 8, meter_ternary, false, true, nullptr},
    {"BCE", 20, meter_bce, false, true},
    {"CCE", 4, meter_cce, false, true},
    {"Instantiate", 20, meter_instantiate, false, true, &Stats::instantiate_ticks},
    {"Sweep", 100, meter_sweep, false, true},
    {"Congruence", 100, meter_congruence, false, true, nullptr, true},
};

const EffortSpec *effort_spec(const char *name) {
  for (const EffortSpec &s : EFFORT_SPECS)
    if (!std::strcmp(s.name, name))
      return &s;
  return nullptr; // all-or-nothing rule: no titration, no meter
}

constexpr SlotOrderRow AUTHORED_SLOT_ORDER[] = {
    {"Congruence", SlotGroup::Probe},
    {"Substitute", SlotGroup::Probe},
    {"HyperBinaryRes", SlotGroup::Probe},
    {"Backbone", SlotGroup::Probe},
    {"Probe", SlotGroup::Probe},
    {"Vivify", SlotGroup::Probe},
    {"Sweep", SlotGroup::Probe},
    {"Substitute", SlotGroup::Probe}, // kissat probe.c:38 — retires Sweep's work
    {"TransitiveReduce", SlotGroup::Probe},
    {"HyperTernaryRes", SlotGroup::Probe}, // cadical's ternary; kissat has none
    {"Backbone", SlotGroup::Probe},        // kissat probe.c:40
    {"Factor", SlotGroup::Probe},
    {"SubsumeForward", SlotGroup::Eliminate},
    {"Strengthen", SlotGroup::Eliminate},
    {"BVE", SlotGroup::Eliminate},
    {"Instantiate", SlotGroup::Eliminate},
    {"BCE", SlotGroup::Eliminate},
    {"CCE", SlotGroup::Eliminate},
};


} // namespace

const SlotOrderRow *default_slot_order(size_t *count) {
  if (count)
    *count = sizeof(AUTHORED_SLOT_ORDER) / sizeof(AUTHORED_SLOT_ORDER[0]);
  return AUTHORED_SLOT_ORDER;
}


const char *candidate_order_name(CandidateOrder o) {
  switch (o) {
  case CandidateOrder::Native:
    return "native";
  case CandidateOrder::Reverse:
    return "reverse";
  case CandidateOrder::Random:
    return "random";
  case CandidateOrder::OccAsc:
    return "occ-asc";
  case CandidateOrder::OccDesc:
    return "occ-desc";
  case CandidateOrder::NegOccDesc:
    return "negocc-desc";
  case CandidateOrder::Heuristic:
    return "heuristic";
  }
  return "native";
}

bool parse_candidate_order(const char *s, CandidateOrder *out) {
  static const struct {
    const char *n;
    CandidateOrder o;
  } table[] = {
      {"native", CandidateOrder::Native},
      {"reverse", CandidateOrder::Reverse},
      {"random", CandidateOrder::Random},
      {"occ-asc", CandidateOrder::OccAsc},
      {"occ-desc", CandidateOrder::OccDesc},
      {"negocc-desc", CandidateOrder::NegOccDesc},
      {"heuristic", CandidateOrder::Heuristic},
  };
  for (const auto &row : table)
    if (!std::strcmp(s, row.n)) {
      *out = row.o;
      return true;
    }
  return false;
}

namespace {

uint64_t instance_key(const Kernel &k, uint64_t inst) {
  Lit l;
  return lit_of_instance(k, inst, &l) ? static_cast<uint64_t>(l) : inst;
}

uint32_t lit_occurrences(const Kernel &k, Lit l, bool binary_only) {
  uint32_t n = 0;
  for (const WatchEntry w : k.watches.entries(l))
    n += w.binary() ? 1u : (binary_only ? 0u : 1u);
  return n;
}

uint64_t order_key(const Kernel &k, CandidateOrder arm, uint64_t inst) {
  Lit l;
  if (!lit_of_instance(k, inst, &l) || var_of(l) >= k.nvars)
    return 0; // not a literal payload: leave it where the native order had it
  const Var v = var_of(l);
  switch (arm) {
  case CandidateOrder::OccAsc:
  case CandidateOrder::OccDesc: {
    const uint64_t occ = lit_occurrences(k, make_lit(v, false), false) +
                         lit_occurrences(k, make_lit(v, true), false);
    return arm == CandidateOrder::OccAsc ? occ : ~occ;
  }
  case CandidateOrder::NegOccDesc:
    return ~static_cast<uint64_t>(lit_occurrences(k, negate(l), true));
  case CandidateOrder::Heuristic:
    if (k.h->stable) {
      const double s = k.h->scores.score[v];
      uint64_t bits = 0;
      std::memcpy(&bits, &s, sizeof bits);
      return bits;
    }
    return k.h->links[v].stamp;
  default:
    return 0;
  }
}

} // namespace

void DefaultCandidateOrder::rank(const Kernel &k, uint32_t rule,
                                 uint64_t *insts, uint32_t n) {
  const CandidateOrder arm = arm_of(rule);
  if (arm == CandidateOrder::Native || n < 2)
    return;
  if (arm == CandidateOrder::Reverse) {
    std::reverse(insts, insts + n);
    return;
  }
  if (arm == CandidateOrder::Random) {
    for (uint32_t i = n; i > 1; i--) {
      rng_ += 0x9e3779b97f4a7c15ull;
      uint64_t z = rng_;
      z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
      z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
      z ^= z >> 31;
      std::swap(insts[i - 1], insts[z % i]);
    }
    return;
  }
  if (arm == CandidateOrder::Heuristic && !k.h)
    return;
  scratch_.resize(n);
  for (uint32_t i = 0; i < n; i++)
    scratch_[i] = {order_key(k, arm, insts[i]), insts[i]};
  std::stable_sort(
      scratch_.begin(), scratch_.end(),
      [](const std::pair<uint64_t, uint64_t> &a,
         const std::pair<uint64_t, uint64_t> &b) { return a.first < b.first; });
  for (uint32_t i = 0; i < n; i++)
    insts[i] = scratch_[i].second;
}

void DefaultInprocessScheduler::order_candidates(const Kernel &k, SlotEntry &e,
                                     uint64_t *insts, uint32_t n) {
  if (n == 0)
    return;
  e.ord.lists++;
  e.ord.candidates += n;
  if (e.cursor_enabled) {
    uint32_t start = 0;
    while (start < n && instance_key(k, insts[start]) < e.cursor)
      start++;
    if (start >= n)
      start = 0; // the cursor ran past the set: begin again at the head
    if (start) {
      std::rotate(insts, insts + start, insts + n);
      e.ord.rotated++;
    }
  }
  if (!candidate_order && e.order == CandidateOrder::Native)
    return;
  CandidateOrderPolicy &cp =
      candidate_order ? *candidate_order : default_candidate_order_;
  order_before_.assign(insts, insts + n);
  cp.rank(k, static_cast<uint32_t>(e.rule), insts, n);
  uint64_t moved = 0;
  for (uint32_t i = 0; i < n; i++)
    moved += (order_before_[i] != insts[i]);
  if (moved) {
    e.ord.ranked++;
    e.ord.moved += moved;
  }
}

bool DefaultInprocessScheduler::want_phase(const Kernel &k, SlotGroup g) {
  if (!initialized_)
    return false;
  SlotClock &c = g == SlotGroup::Probe ? probe_clock : elim_clock;
  if (k.stats.conflicts < c.next_conflicts)
    return false;
  const bool gated =
      slot_watermark_ && c.rounds != 0 &&
      (g == SlotGroup::Probe
           ? k.stats.reductions == c.reductions_mark
           : (k.stats.bve_marks <= c.marks_mark &&
              kernel_root_fixed(k) <= c.fixed_mark));
  if (gated) {
    if (!c.blocked) { // count episodes, not search steps
      c.blocked = true;
      c.blocked_episodes++;
    }
    return false;
  }
  return true;
}

void DefaultInprocessScheduler::update_limit(const Kernel &k, SlotGroup g) {
  SlotClock &c = g == SlotGroup::Probe ? probe_clock : elim_clock;
  const bool split = slot_clocks_split_;
  uint64_t n = split ? c.rounds
                     : std::max(probe_clock.rounds, elim_clock.rounds);
  if (!n)
    n = 1;
  const double lg = std::log10(static_cast<double>(n) + 9);
  const double base = split && g == SlotGroup::Probe
                          ? PROBE_INTERVAL * static_cast<double>(n) * lg
                          : SUBSUME_INTERVAL * static_cast<double>(n) * lg * lg;
  c.next_conflicts = k.stats.conflicts +
                     scale_inprocessing_delta(k, static_cast<uint64_t>(base));
}

void DefaultPolicy::update_reduce_limit(const Kernel &k) {
  if (reduce_sqrt_) {
    const double n = static_cast<double>(
        k.stats.reductions ? k.stats.reductions : 1);
    reduce.next_conflicts =
        k.stats.conflicts +
        static_cast<uint64_t>(REDUCE_INTERVAL_KISSAT * std::sqrt(n));
    return;
  }
  reduce.next_conflicts = k.stats.conflicts + REDUCE_INTERVAL_INIT +
                          REDUCE_INTERVAL_STEP * k.stats.reductions;
}

size_t GlueTieredReduction::victims(const Kernel &k, size_t candidates) {
  if (!ramp)
    return reduce_canonical_victims(candidates);
  const double r = static_cast<double>(k.stats.reductions) + 1.0;
  const double percent = 90.0 - 40.0 / std::log10(r + 9.0);
  return static_cast<size_t>(static_cast<double>(candidates) * percent /
                             100.0);
}

Lit DefaultBranching::pick(Kernel &k) {
  assert(k.unassigned > 0);
  if (randec_enabled && !k.h->stable) {
    const uint64_t c = k.stats.conflicts;
    if (c >= randec_until && k.level <= 1 && c >= randec_next) {
      randec_sequences++;
      const double n = static_cast<double>(randec_sequences);
      const double logn = std::log10(n + 9.0);
      randec_until = c + static_cast<uint64_t>(RANDEC_LENGTH * logn) + 1;
      randec_next = c + static_cast<uint64_t>(RANDEC_INIT * logn);
      k.stats.random_sequences++;
    }
    if (c < randec_until) {
      for (uint32_t tries = 0; tries < 64; tries++) {
        randec_rng += 0x9E3779B97F4A7C15ull;
        uint64_t z = randec_rng;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z ^= z >> 31;
        const Var v = static_cast<Var>(z % k.nvars);
        if (k.values[make_lit(v, true)] == 0 && !kernel_var_retired(k, v)) {
          k.stats.random_decisions++;
          return heuristics_decide_lit(k, v);
        }
      }
    }
  }
  const Var v = heuristics_next_decision_var(k);
  return heuristics_decide_lit(k, v);
}

void DefaultPolicy::configure_from_env() {
  force_mode_ = 0;
  if (const char *fm = std::getenv("TACTIX_FORCE_MODE")) {
    if (!std::strcmp(fm, "focused"))
      force_mode_ = 1;
    else if (!std::strcmp(fm, "stable"))
      force_mode_ = 2;
  }
  bump_reasons_limit_ =
      std::getenv("TACTIX_NO_BUMP_REASONS") ? 0 : BUMP_REASONS_LIMIT;
  if (const char *bl = std::getenv("TACTIX_BUMP_REASONS_LIMIT"))
    bump_reasons_limit_ = static_cast<uint32_t>(std::strtoul(bl, nullptr, 10));
  bump_reasons_rate_ = BUMP_REASONS_RATE;
  if (const char *br = std::getenv("TACTIX_BUMP_REASONS_RATE"))
    bump_reasons_rate_ = std::strtod(br, nullptr);
  const bool both = std::getenv("TACTIX_REDUCE_SQRT") != nullptr;
  const char *cad = std::getenv("TACTIX_REDUCE_CADENCE"); // "sqrt" | "arith"
  const char *rmp = std::getenv("TACTIX_REDUCE_RAMP");    // "1" | "0"
  reduce_sqrt_ = cad ? !std::strcmp(cad, "sqrt") : both;
  default_reduction_.ramp = rmp ? (rmp[0] == '1') : both;
  dynamic_tiers_ = std::getenv("TACTIX_NO_DYNAMIC_TIERS") == nullptr;
  compact_ = !std::getenv("TACTIX_NO_COMPACT");
  inprocess_.configure_from_env();
}

void DefaultInprocessScheduler::configure_from_env() {
  closure_delay_ = std::getenv("TACTIX_NO_CLOSURE_DELAY") == nullptr;
  factor_init_ticks_ = FACTOR_INIT_TICKS;
  if (const char *fi = std::getenv("TACTIX_FACTOR_INIT_TICKS"))
    factor_init_ticks_ = std::strtoull(fi, nullptr, 10);
  congruence_init_ticks_ = CONGRUENCE_INIT_TICKS;
  if (const char *ci = std::getenv("TACTIX_CONG_INIT_TICKS"))
    congruence_init_ticks_ = std::strtoull(ci, nullptr, 10);
}

void DefaultInprocessScheduler::reset(const Kernel &k) {
  initialized_ = false;
  const uint64_t ticks = search_ticks(k);
  probe_clock = elim_clock = {};
  probe_clock.anchor = elim_clock.anchor = ticks;
  bve_rounds_run_ = 0;
  bve_level_ = 0;
  bve_level_idx_ = 0;
  default_candidate_order_.reset();
  for (SlotEntry &e : entries) {
    e.limit = 0;
    e.applies = 0;
    e.doses = 0;
    e.cursor = 0;
    e.tm = {};
    e.ord = {};
    e.binaries_mark = 0;
    e.root_fixed_mark = 0;
    e.arena_mark = 0;
    e.delay_count = 0;
    e.delay_current = 0;
  }
  forward_pair_limit_ = 0;
}

void DefaultInprocessScheduler::build(Kernel &k) {
  if (entries.empty()) {
    const std::vector<RuleVTable> &rules = tactix_rules();
    const bool bve_off = std::getenv("TACTIX_NO_BVE") != nullptr;
    const bool factor_off = std::getenv("TACTIX_NO_FACTOR") != nullptr;
    const auto env_list = [](const char *var, const char *nm) {
      const char *s = std::getenv(var);
      if (!s)
        return false;
      const size_t n = std::strlen(nm);
      for (const char *p = s; (p = std::strstr(p, nm)); p += n)
        if ((p == s || p[-1] == ',') && (p[n] == '\0' || p[n] == ','))
          return true;
      return false;
    };
    const auto add_entry = [&](int i) {
      {
        const char *nm = rules[i].name;
        bool on = !(bve_off && !std::strcmp(nm, "BVE")) &&
                  !(factor_off && !std::strcmp(nm, "Factor"));
        if (env_list("TACTIX_SLOT_ENABLE", nm))
          on = true;
        if (env_list("TACTIX_SLOT_DISABLE", nm))
          on = false;
        SlotEntry e;
        e.defer_preprocess = !std::strcmp(nm, "BVE");
        e.rule = i;
        e.enabled = on;
        {
          size_t norder = 0;
          const SlotOrderRow *order = default_slot_order(&norder);
          for (size_t o = 0; o < norder; o++)
            if (!std::strcmp(order[o].name, nm)) {
              e.group = order[o].group;
              break;
            }
        }
        e.closure = true;
        if (const EffortSpec *s = effort_spec(nm)) {
          e.closure = s->closure;
          e.permille = s->permille;
          e.meter = s->meter;
          e.tick_counter = s->tick_counter;
          e.dosed = s->dosed;
          e.budget_aware = s->budget_aware;
          if (const char *pm = std::getenv("TACTIX_SLOT_PERMILLE")) {
            const size_t n = std::strlen(nm);
            for (const char *p = pm; (p = std::strstr(p, nm)); p += n)
              if ((p == pm || p[-1] == ',') && p[n] == ':') {
                e.permille = static_cast<uint32_t>(strtoul(p + n + 1, nullptr, 10));
                break;
              }
          }
        }
        if (const char *mx = std::getenv("TACTIX_SLOT_MAX_APPLIES")) {
          const size_t n = std::strlen(nm);
          for (const char *p = mx; (p = std::strstr(p, nm)); p += n)
            if ((p == mx || p[-1] == ',') && p[n] == ':') {
              e.max_applies =
                  static_cast<uint32_t>(strtoul(p + n + 1, nullptr, 10));
              break;
            }
        }
        if (e.dosed)
          e.cursor_enabled = std::strcmp(nm, "BVE") != 0;
        char knob[96];
        const auto knob_name = [&knob](const char *prefix,
                                       const char *name) {
          size_t p = 0;
          for (const char *s = prefix; *s && p + 1 < sizeof knob; s++)
            knob[p++] = *s;
          for (const char *s = name; *s && p + 1 < sizeof knob; s++)
            knob[p++] = static_cast<char>(
                std::toupper(static_cast<unsigned char>(*s)));
          knob[p] = '\0';
        };
        const auto reject = [&knob](const char *why) {
          std::fprintf(stderr, "c ERROR: %s: %s\n", knob, why);
          std::abort();
        };
        knob_name("TACTIX_CURSOR_", nm);
        if (const char *cv = std::getenv(knob)) {
          if (!e.dosed)
            reject("this entry runs as a canonical batch round, so the "
                   "policy never walks its candidates and a cursor cannot "
                   "take effect");
          e.cursor_enabled = std::strtoul(cv, nullptr, 10) != 0;
        }
        knob_name("TACTIX_ORDER_", nm);
        if (const char *ov = std::getenv(knob)) {
          if (!e.dosed)
            reject("this entry runs as a canonical batch round, so the "
                   "policy never sees its candidate list and an order arm "
                   "cannot take effect");
          if (!parse_candidate_order(ov, &e.order))
            reject("unknown candidate-order arm (native, reverse, random, "
                   "occ-asc, occ-desc, negocc-desc, heuristic)");
        }
        entries.push_back(e);
      }
    };
    const char *order_env = std::getenv("TACTIX_SLOT_ORDER");
    slot_order_legacy_ = order_env && !std::strcmp(order_env, "legacy");
    if (const char *cl = std::getenv("TACTIX_SLOT_CLOCKS"))
      slot_clocks_split_ = std::strtoul(cl, nullptr, 10) != 1;
    if (const char *wm = std::getenv("TACTIX_SLOT_WATERMARK"))
      slot_watermark_ = std::strtoul(wm, nullptr, 10) != 0;
    if (const char *fc = std::getenv("TACTIX_FACTOR_COUPLE"))
      couple_factor_bound_ = std::strtoul(fc, nullptr, 10) != 0;
    if (slot_order_legacy_) {
      for (int i = 0; i < static_cast<int>(rules.size()); i++)
        if (rules[i].tier == RuleTier::Inprocess)
          add_entry(i);
      const auto move_to_back = [this](const char *name) {
        const std::vector<RuleVTable> &rr = tactix_rules();
        for (size_t j = 0; j < entries.size(); j++)
          if (!std::strcmp(rr[entries[j].rule].name, name)) {
            const SlotEntry e = entries[j];
            entries.erase(entries.begin() + j);
            entries.push_back(e);
            break;
          }
      };
      move_to_back("BVE");
      move_to_back("Factor");
    } else {
      size_t norder = 0;
      const SlotOrderRow *order = default_slot_order(&norder);
      std::vector<bool> placed(rules.size(), false);
      for (size_t o = 0; o < norder; o++)
        for (int i = 0; i < static_cast<int>(rules.size()); i++)
          if (rules[i].tier == RuleTier::Inprocess &&
              !std::strcmp(rules[i].name, order[o].name)) {
            placed[i] = true;
            add_entry(i);
            break;
          }
      for (int i = 0; i < static_cast<int>(rules.size()); i++)
        if (rules[i].tier == RuleTier::Inprocess && !placed[i])
          add_entry(i);
    }
    default_candidate_order_.arms.assign(rules.size(),
                                         CandidateOrder::Native);
    for (const SlotEntry &se : entries)
      default_candidate_order_.arms[se.rule] = se.order;
  }
  if (!initialized_) {
    initialized_ = true;
    probe_clock.next_conflicts = k.stats.conflicts;
    elim_clock.next_conflicts = k.stats.conflicts;
  }
}

int DefaultPolicy::solve(Kernel &k, volatile sig_atomic_t *stop) {
  KernelStopPublisher stop_published(k, stop);
  if (const char *cl = std::getenv("TACTIX_CHRONO_LEVELS")) {
    const long n = std::strtol(cl, nullptr, 10);
    if (n >= 0)
      chrono_levels = static_cast<uint32_t>(n);
  }
  if (const char *cr = std::getenv("TACTIX_CHRONO_REUSE"))
    chrono_reuse_trail_ = std::strtoul(cr, nullptr, 10) != 0;
  if (const char *rd = std::getenv("TACTIX_RANDEC"))
    default_branching_.randec_enabled = (*rd != '0');
  if (k.inconsistent)
    return 20;
  std::optional<ScopedHeuristics> owned_h;
  if (!k.h)
    owned_h.emplace(k);
  solves++;
  configure_from_env();
  if (reset_schedule_per_solve)
    reset_schedule(k);
  inprocess_.build(k);
  bool lucky_tried = false;
  for (;;) {
    const ApplyResult r = propagate_apply(k);
    if (r == ApplyResult::Unsat)
      return 20;
    if (!lucky_tried && r == ApplyResult::Ok) {
      lucky_tried = true;
      if (lucky_guard(k) && lucky_apply(k) == ApplyResult::Sat)
        return 10;
    }
    if (r == ApplyResult::Conflict) {
      observe_decision_rate(k); // kissat: before the analysis, once per conflict
      k.bump_reason_limit = want_bump_reasons(k);
      const uint64_t br_rounds = k.stats.reason_bump_rounds;
      const uint64_t br_aborted = k.stats.reason_bump_aborted;
      const ApplyResult ar = analyze_apply(k);
      k.bump_reason_limit = 0;
      if (k.stats.reason_bump_rounds != br_rounds) { // the pass actually ran
        if (k.stats.reason_bump_aborted != br_aborted) // over the mark limit
          bump_reasons_delay_.current +=
              (bump_reasons_delay_.current < UINT32_MAX);
        else if (bump_reasons_delay_.current)
          bump_reasons_delay_.current /= 2;
        bump_reasons_delay_.count = bump_reasons_delay_.current;
      }
      if (ar == ApplyResult::Unsat)
        return 20;
      if (!k.pending.valid) // `stats.conflict_reused` already counted it
        continue;
      observe_glue(k, k.pending.glue); // policy observes; Analyze is pure
      if (dynamic_tiers_ && k.stats.conflicts > retier_.next_conflicts) {
        reduce_retier(k);
        if (retier_.interval < (1u << 16))
          retier_.interval *= 2;
        retier_.next_conflicts = k.stats.conflicts + retier_.interval;
      }
      if (k.h->stable)
        tick_reluctant(); // per learned clause (kissat learn.c)
      {
        const uint32_t jump = k.pending.jump_level;
        const uint32_t back = k.level - 1;
        uint32_t target = jump;
        if (back - jump > chrono_levels) {
          k.stats.chronological++;
          target = back;
        } else if (chrono_reuse_trail_ && chrono_levels != UINT32_MAX &&
                   jump < back) {
          target = reuse_trail_level(k, jump);
          if (target != jump)
            k.stats.chronological++;
        }
        backjump_apply_to(k, target);
      }
      continue;
    }
    if (k.retire_enabled ? k.unassigned_active == 0 : k.unassigned == 0)
      return 10;
    if (want_reduce(k) && reduce_guard(k)) {
      ReductionPolicy &rp = reduction ? *reduction : default_reduction_;
      if (dynamic_tiers_)
        reduce_retier(k);
      reduce_candidates(k, reduce_scratch_);
      reduce_apply_refs(
          k, reduce_scratch_.data(),
          std::min(rp.victims(k, reduce_scratch_.size()),
                   reduce_scratch_.size()));
      if (compact_ && kernel_should_compact_variables(k, 10))
        kernel_compact_variables(k);
      update_reduce_limit(k);
      continue;
    }
    const bool pin_mode =
        force_mode_ == 1 || (force_mode_ == 2 && k.h->stable);
    if (!pin_mode && want_switch_mode(k) && switchmode_guard(k)) {
      switchmode_apply(k);
      mode.count++;
      averages[k.h->stable].saved_decisions = k.stats.decisions;
      if (k.h->stable) // entering stable: fresh reluctant sequence
        reluctant = {};
      update_mode_limits(k);
      continue;
    }
    if (want_restart(k) && restart_guard(k)) {
      restart_apply(k);
      reluctant.trigger = false; // consumed (kissat reluctant.h)
      if (!k.h->stable) // kissat restart.c: focused-only limit update
        update_restart_limit(k);
      continue;
    }
    if (want_reorder(k) && reorder_guard(k)) {
      reorder_apply(k);
      reorder.count++;
      update_reorder_limit(k);
      continue;
    }
    if (want_rephase(k) && rephase_guard(k)) {
      if (restart_guard(k))
        restart_apply_to(k, 0);
      static constexpr uint64_t WALK_SLOT = UINT64_MAX;
      static constexpr uint64_t ROTATION[6] = {
          REPHASE_BEST, WALK_SLOT, REPHASE_INVERTED,
          REPHASE_BEST, WALK_SLOT, REPHASE_ORIGINAL};
      const uint64_t type = ROTATION[rephase.count % 6];
      rephase.count++; // ONE rephase event, whatever it is composed of
      if (type == WALK_SLOT && walk_guard(k)) {
        rephase_apply_type(k, REPHASE_BEST); // the slot keeps its Best
        uint64_t reference = search_ticks(k) - walk_anchor;
        if (reference < DefaultInprocessScheduler::MIN_EFFORT_TICKS)
          reference = DefaultInprocessScheduler::MIN_EFFORT_TICKS;
        static const char *const pm_env = std::getenv("TACTIX_WALK_PERMILLE");
        const uint64_t permille =
            pm_env ? std::strtoull(pm_env, nullptr, 10) : WALK_PERMILLE;
        k.work_budget = reference / 1000 * permille;
        walk_apply(k);
        k.work_budget = 0;
        walk_anchor = search_ticks(k);
      } else {
        rephase_apply_type(k, type == WALK_SLOT ? REPHASE_BEST : type);
      }
      update_rephase_limit(k);
      continue;
    }
    if (inprocess_.want_phase(k, SlotGroup::Probe)) {
      if (inprocess_.run_phase(k, SlotGroup::Probe, stop) == 20)
        return 20;
      continue;
    }
    if (inprocess_.want_phase(k, SlotGroup::Eliminate)) {
      if (inprocess_.run_phase(k, SlotGroup::Eliminate, stop) == 20)
        return 20;
      continue;
    }
    if (stop && *stop)
      return 0;
    BranchingPolicy &bp = branching ? *branching : default_branching_;
    decide_apply(k, bp.pick(k), false);
  }
}


namespace {
void slot_trace_entry(const Kernel &k, const DefaultInprocessScheduler::SlotEntry &e,
                      const RuleVTable &r, int outer) {
  if (!std::getenv("TACTIX_SLOT_TRACE"))
    return;
  std::fprintf(stderr,
               "[slot] phase=%llu outer=%d %s enabled=%d defer=%d "
               "guard=%d dosed=%d meter=%llu limit=%llu\n",
               (unsigned long long)k.stats.inprocess_phases, outer, r.name,
               (int)e.enabled, (int)e.defer_preprocess, (int)r.guard(k),
               (int)e.dosed, (unsigned long long)(e.meter ? e.meter(k) : 0),
               (unsigned long long)e.limit);
}
} // namespace

bool DefaultInprocessScheduler::slot_entry_admissible(SlotEntry &e, const Kernel &k,
                                          const SlotPhaseCtx &ctx) {
  if (!e.enabled || ctx.phase_unsat) {
    e.tm.dec_disabled++;
    return false;
  }
  if (e.max_applies && e.applies >= e.max_applies) {
    e.tm.dec_cap++;
    return false; // diagnostic round cap (see SlotEntry::max_applies)
  }
  if (e.defer_preprocess && k.stats.conflicts == 0) {
    e.tm.dec_preprocess++;
    return false; // no elimination at preprocessing (kissat fastel=0)
  }
  e.applies++;
  return true;
}

uint64_t DefaultInprocessScheduler::ledger_remaining(SlotEntry &e, const Kernel &k,
                                        uint64_t limit) {
  const uint64_t spent = e.meter(k);
  if (limit >= spent)
    return limit - spent;
  e.tm.wrapped++;
  return 0;
}

DefaultInprocessScheduler::SlotStep
DefaultInprocessScheduler::run_dosed_entry(Kernel &k, SlotEntry &e, const RuleVTable &r,
                               SlotPhaseCtx &ctx) {
  const uint64_t limit = e.limit;
  if (!r.guard(k)) {
    e.tm.dec_guard++;
    return SlotStep::Next;
  }
  const bool bve_entry = !std::strcmp(r.name, "BVE");
  const bool bve_oneshot = bve_entry && std::getenv("TACTIX_BVE_ONESHOT");
  if (bve_oneshot && e.doses > 0)
    return SlotStep::Next; // the one shot is consumed
  if (bve_entry && ctx.outer > 0 && std::getenv("TACTIX_BVE_CADENCE"))
    return SlotStep::Next;
  BveBoundPolicy *ramp = nullptr;
  if (bve_entry && !std::getenv("TACTIX_NO_BVE_RAMP"))
    ramp = bve_bound ? bve_bound : &default_bve_ramp_;
  const bool level_mode = bve_entry && ramp && !bve_oneshot;
  const uint32_t passes = level_mode ? 1 : ramp ? ramp->num_passes() : 1;
  bool dose_cut = false;
  uint64_t elim_at_last_pass = k.stats.bve_eliminated;
  const bool use_retry = bve_entry && !ramp;
  k.bve_track_min_bound = ramp != nullptr;
  bool engaged = false; // a pass got past the budget break and ran instances
  for (uint32_t pass = 0; pass < passes && !ctx.phase_unsat; pass++) {
    if (ramp && e.meter(k) > limit) {
      dose_cut = true;
      break;
    }
    engaged = true;
    if (ramp)
      k.bve_bound = level_mode ? bve_level_ : ramp->bound(pass);
    elim_at_last_pass = k.stats.bve_eliminated; // fixpoint test above
    if (bve_entry)
      kernel_bve_retry_sync_fixed(k);
    payloads_.resize(2 * static_cast<size_t>(k.nvars) + 1);
    const uint32_t n =
        r.instances(k, payloads_.data(),
                    static_cast<uint32_t>(payloads_.size()));
    order_candidates(k, e, payloads_.data(), n);
    if (std::getenv("TACTIX_SLOT_TRACE"))
      std::fprintf(stderr,
                   "[slot]   %s pass=%u n=%u bve_bound=%u meter=%llu "
                   "limit=%llu\n",
                   r.name, pass, n, k.bve_bound,
                   (unsigned long long)e.meter(k), (unsigned long long)limit);
    for (uint32_t i = 0; i < n; i++) {
      if (e.meter(k) > limit) {
        dose_cut = true;
        break; // dose spent
      }
      if (!r.guard(k)) {
        dose_cut = true; // conservative: not a completed schedule
        break;
      }
      if (use_retry || ramp) {
        Lit rl;
        if (lit_of_instance(k, payloads_[i], &rl) &&
            var_of(rl) < k.nvars && !k.bve_retry[var_of(rl)] &&
            (!ramp || k.bve_min_bound[var_of(rl)] > k.bve_bound))
          continue;
      }
      if (ctx.stop && *ctx.stop)
        break; // §6: a dose can enumerate thousands of candidates
      if (e.cursor_enabled) {
        e.cursor = instance_key(k, payloads_[i]) + 1;
        e.ord.advanced++;
      }
      const bool hand_residual = e.budget_aware && e.meter;
      if (hand_residual) {
        const uint64_t remaining = ledger_remaining(e, k, limit);
        k.work_budget = remaining ? remaining : 1;
      }
      const ApplyResult ar = [&] {
        TickSinkScope sink(k, e.tick_counter);
        return apply_rule_instance(r, k, payloads_[i]);
      }();
      heuristics_consume_rule_advice(k);
      if (hand_residual)
        k.work_budget = 0;
      if (ar == ApplyResult::Unsat || k.inconsistent) {
        ctx.phase_unsat = true;
        break;
      }
      if (ar != ApplyResult::Ok)
        continue; // this candidate was not viable; try the next
      ctx.round_changed = true;
      ctx.phase_dosed = true;
      if (propagate_apply(k) == ApplyResult::Unsat) {
        ctx.phase_unsat = true;
        break;
      }
    }
  }
  k.bve_track_min_bound = false; // scope the caching to this ramp
  if (level_mode && engaged && !ctx.phase_unsat)
    bve_ran_this_phase_ = true;
  (void)elim_at_last_pass;
  if (bve_entry && ctx.outer == 0)
    bve_rounds_run_++;
  if (bve_oneshot)
    e.doses++; // consume the one shot (skipped hereafter)
  e.tm.admitted++; // the dosed branch ran; it exits here, not below
  return SlotStep::Next;
}

bool DefaultInprocessScheduler::factor_entry_declines(const Kernel &k, SlotEntry &e) {
  const uint64_t active = active_variables(k);
  uint64_t threshold = 1;
  const uint32_t decades = bve_rounds_run_ > 14 ? 19u : bve_rounds_run_ + 5;
  for (uint32_t d = 0; d < decades; d++)
    threshold *= 10;
  if (active >= threshold) {
    e.tm.dec_warmup++;
    return true;
  }
  if (k.stats.conflicts > 0 && k.binary_ids.size() <= e.binaries_mark &&
      k.arena.size() <= e.arena_mark &&
      kernel_root_fixed(k) <= e.root_fixed_mark) {
    e.tm.dec_material++;
    return true;
  }
  return false;
}

void DefaultInprocessScheduler::factor_dose_config(Kernel &k, SlotEntry &) {
  k.factor_composite = true;
  k.factor_admit = true;
}

ApplyResult DefaultInprocessScheduler::run_vivify_dose(Kernel &k, SlotEntry &e,
                                           const RuleVTable &r,
                                           uint64_t limit) {
  const uint64_t remaining0 = ledger_remaining(e, k, limit);
  const uint64_t remaining = remaining0 ? remaining0 : 1;
  const bool irr_delayed = k.stats.conflicts > 0 && e.delay_count > 0;
  if (irr_delayed)
    e.delay_count--;
  const char *no_t3 = std::getenv("TACTIX_VIVIFY_TIER3");
  const bool split = !(no_t3 && *no_t3 == '0');
  ApplyResult ar = ApplyResult::NoOp;
  const uint64_t red = irr_delayed ? remaining : remaining * 7 / 10;
  const uint8_t scopes[3] = {3, 4, 5};
  const uint64_t shares[3] = {red * 3 / 7, red * 3 / 7, red / 7};
  const uint32_t passes = split ? 3u : 1u;
  for (uint32_t p = 0; p < passes && ar != ApplyResult::Unsat && !k.inconsistent;
       p++) {
    k.vivify_scope = split ? scopes[p] : uint8_t{1};
    k.work_budget = std::max<uint64_t>(1, split ? shares[p] : red);
    TickSinkScope sink(k, e.tick_counter);
    const ApplyResult a = apply_rule(r, k);
    if (a == ApplyResult::Unsat ||
        (a == ApplyResult::Ok && ar != ApplyResult::Unsat))
      ar = a;
  }
  if (!irr_delayed && ar != ApplyResult::Unsat && !k.inconsistent) {
    const uint64_t spent = e.meter(k);
    const uint64_t irr_budget =
        spent < limit ? std::max<uint64_t>(1, limit - spent) : 1;
    const uint64_t tried_before = k.stats.vivify_checked;
    const uint64_t produced_before = k.stats.vivified + k.stats.vivify_deleted;
    k.vivify_scope = 2;
    k.work_budget = irr_budget;
    const ApplyResult ar2 = [&] {
      TickSinkScope sink(k, e.tick_counter);
      return apply_rule(r, k);
    }();
    if (ar2 == ApplyResult::Unsat)
      ar = ar2;
    else if (ar2 == ApplyResult::Ok && ar != ApplyResult::Ok)
      ar = ar2;
    if (k.stats.conflicts > 0) {
      const uint64_t tried = k.stats.vivify_checked - tried_before;
      const uint64_t produced =
          k.stats.vivified + k.stats.vivify_deleted - produced_before;
      if (tried == 0 || produced * 100 < tried) {
        e.delay_current += (e.delay_current < UINT32_MAX);
        e.delay_count = e.delay_current;
      } else {
        e.delay_current /= 2;
        e.delay_count = e.delay_current;
      }
    }
  }
  k.vivify_scope = 0;
  k.work_budget = 0;
  return ar;
}

ApplyResult DefaultInprocessScheduler::run_budgeted_batch(Kernel &k, SlotEntry &e,
                                              const RuleVTable &r,
                                              uint64_t limit,
                                              bool factor_entry) {
  const uint64_t remaining0 = ledger_remaining(e, k, limit);
  const uint64_t handed = remaining0 ? remaining0 : 1;
  const uint64_t spent_before = e.meter(k);
  if (factor_entry)
    factor_dose_config(k, e);
  k.work_budget = handed;
  ApplyResult ar;
  {
    TickSinkScope sink(k, e.tick_counter);
    ar = apply_rule(r, k);
  }
  k.work_budget = 0;
  k.factor_admit = true; // advice is per-occasion; default is NO restriction
  k.factor_composite = false;
  const uint64_t spent = e.meter(k) - spent_before;
  e.tm.handed += handed;
  e.tm.spent += spent;
  if (spent > 0)
    e.doses++;
  if (spent > 0 && spent <= handed) {
    e.tm.completed++;
    e.binaries_mark = k.binary_ids.size();
    e.arena_mark = k.arena.size();
    e.root_fixed_mark = kernel_root_fixed(k);
  }
  return ar;
}

DefaultInprocessScheduler::SlotStep
DefaultInprocessScheduler::run_batch_entry(Kernel &k, SlotEntry &e, const RuleVTable &r,
                               SlotPhaseCtx &ctx) {
  const uint64_t limit = e.limit;
  if (e.meter && e.meter(k) > limit) {
    e.tm.dec_budget++;
    return SlotStep::Next;
  }
  const bool factor_entry = e.budget_aware && !std::strcmp(r.name, "Factor");
  if (factor_entry && factor_entry_declines(k, e))
    return SlotStep::Next;
  if (e.closure && k.stats.conflicts > 0 &&
      k.binary_ids.size() <= e.binaries_mark &&
      kernel_root_fixed(k) <= e.root_fixed_mark) {
    e.tm.dec_material++;
    return SlotStep::Next;
  }
  if (closure_delay_ && (e.closure || delay_spec(r.name)) &&
      k.stats.conflicts > 0 && e.delay_count > 0) {
    e.delay_count--;
    e.tm.dec_delay++;
    return SlotStep::Next;
  }
  if (!r.guard(k)) {
    e.tm.dec_guard++;
    return SlotStep::Next;
  }
  e.tm.admitted++;
  const DelaySpec *const dspec = delay_spec(r.name);
  const uint64_t yield_before = dspec ? dspec->yield(k) : 0;
  const uint64_t denom_before = dspec ? dspec->denom(k) : 0;
  const bool budgeted = e.meter && !e.dosed && e.budget_aware;
  const bool vivify_dose = budgeted && !std::strcmp(r.name, "Vivify");
  ApplyResult ar;
  if (vivify_dose) {
    ar = run_vivify_dose(k, e, r, limit);
  } else if (budgeted) {
    ar = run_budgeted_batch(k, e, r, limit, factor_entry);
  } else {
    TickSinkScope sink(k, e.tick_counter);
    ar = apply_rule(r, k);
  }
  heuristics_consume_rule_advice(k);
  if (e.closure) {
    e.binaries_mark = k.binary_ids.size();
    e.arena_mark = k.arena.size();
    e.root_fixed_mark = kernel_root_fixed(k);
  }
  if (closure_delay_ && k.stats.conflicts > 0 && (e.closure || dspec)) {
    bool unproductive;
    if (dspec) {
      const uint64_t yield = dspec->yield(k) - yield_before;
      const uint64_t denom =
          dspec->denom_delta ? dspec->denom(k) - denom_before : dspec->denom(k);
      unproductive = denom == 0 || yield * 1000 < uint64_t(dspec->permille) * denom;
    } else {
      unproductive = ar != ApplyResult::Ok;
    }
    if (unproductive)
      e.delay_current += (e.delay_current < UINT32_MAX);
    else
      e.delay_current /= 2;
    e.delay_count = e.delay_current;
  }
  if (ar == ApplyResult::Ok)
    e.tm.ok++;
  if (ar == ApplyResult::Unsat || k.inconsistent) {
    ctx.phase_unsat = true;
    return SlotStep::Stop;
  }
  if (ar != ApplyResult::Ok)
    return SlotStep::Next;
  ctx.round_changed = true;
  if (propagate_apply(k) == ApplyResult::Unsat) {
    ctx.phase_unsat = true;
    return SlotStep::Stop;
  }
  return SlotStep::Next;
}

int DefaultInprocessScheduler::run_phase(Kernel &k, SlotGroup group,
                                  volatile sig_atomic_t *stop) {
  SlotClock &clock = group == SlotGroup::Probe ? probe_clock : elim_clock;
  clock.reductions_mark = k.stats.reductions;
  clock.marks_mark = k.stats.bve_marks;
  clock.fixed_mark = kernel_root_fixed(k);
  clock.blocked = false; // a new blocking episode may now be counted
  forward_pair_limit_ = 0; // the pair's shared allowance is per phase
  bve_ran_this_phase_ = false;
  factor_bound_snapshot_ =
      couple_factor_bound_
          ? (std::getenv("TACTIX_NO_BVE_RAMP") ? k.bve_bound : bve_level_)
          : 0; // ramp off => BVE runs at k.bve_bound, so couple to THAT value,
  InprocessScope inprocess_scope(k);
  const uint64_t phase_props_at_entry = k.stats.propagations;
  const uint64_t phase_nanos_at_entry = now_nanos();
  k.stats.inprocess_phases++;
  if (k.level > 0 && restart_guard(k))
    restart_apply_to(k, 0);
  kernel_dense_connect(k, occurrence_index_);
  default_factor_match_.bound = std::max(k.factor_bound, factor_bound_snapshot_); // see DefaultFactorMatch
  k.factor_selection = factor_match ? factor_match : &default_factor_match_;
  if (factor_match)
    factor_match->set_bound(factor_bound_snapshot_);
  SlotPhaseCtx ctx{group, stop};
  const std::vector<RuleVTable> &rules = tactix_rules();
  uint64_t active_before = active_variables(k);
  for (ctx.outer = 0;
       ctx.outer < PHASE_ROUNDS && !ctx.phase_unsat &&
       !(stop && *stop);
       ctx.outer++) {
    ctx.round_changed = false;
    k.subsume_complete = true;
    k.strengthen_complete = true;
    for (SlotEntry &e : entries) {
      if (e.group != group)
        continue; // the other clock's pipeline: not offered here at all
      if (stop && *stop)
        break; // §6: leave the phase promptly; cleanup runs at its exit
      const RuleVTable &r = rules[e.rule];
      slot_trace_entry(k, e, r, ctx.outer);
      e.tm.occasions++;
      if (!slot_entry_admissible(e, k, ctx))
        continue;
      if (e.enabled && e.meter &&
          (group == SlotGroup::Probe || ctx.outer == 0))
        set_entry_limit(e, k);
      const SlotStep step = (e.dosed && r.instances && r.apply_instance)
                                ? run_dosed_entry(k, e, r, ctx)
                                : run_batch_entry(k, e, r, ctx);
      if (step == SlotStep::Stop)
        break;
    }
    const bool subsume_pending =
        group == SlotGroup::Eliminate && k.subsume_holds_elim &&
        (!k.subsume_complete || !k.strengthen_complete);
    if (!ctx.round_changed && subsume_pending)
      k.stats.subsume_holds++;
    const uint64_t active_now = active_variables(k);
    if (active_now == active_before && !subsume_pending)
      break; // outer fixpoint: the round retired and created nothing
    active_before = active_now;
  }
  if (ctx.phase_dosed && !ctx.phase_unsat)
    kernel_collect_garbage(k);
  if (!ctx.phase_unsat)
    kernel_flush_retired_redundant(k);
  heuristics_consume_retirements(k);
  kernel_dense_disconnect(k);
  k.factor_selection = nullptr;
  k.stats.inprocess_propagations +=
      k.stats.propagations - phase_props_at_entry;
  k.stats.inprocess_nanos += now_nanos() - phase_nanos_at_entry;
  if (group == SlotGroup::Eliminate && bve_ran_this_phase_ && !ctx.phase_unsat) {
    BveBoundPolicy &bramp = bve_bound ? *bve_bound : default_bve_ramp_;
    const uint32_t last = bramp.num_passes() - 1;
    const uint32_t blim = bramp.bound(last);
    if (bve_level_idx_ >= last) {
      clock.marks_mark = k.stats.bve_marks;
      clock.fixed_mark = kernel_root_fixed(k);
    } else {
      bve_level_ = std::min(bramp.bound(++bve_level_idx_), blim);
      clock.marks_mark = 0;
      clock.fixed_mark = 0;
    }
  }
  clock.anchor = search_ticks(k);
  if (ctx.phase_unsat)
    return 20;
  clock.rounds++;
  update_limit(k, group);
  return 0;
}

void DefaultInprocessScheduler::set_entry_limit(SlotEntry &e, Kernel &k) {
  const std::vector<RuleVTable> &rules = tactix_rules();
  const SlotClock &clock =
      e.group == SlotGroup::Probe ? probe_clock : elim_clock;
  const uint64_t now = search_ticks(k);
  uint64_t reference = 0;
  if (now >= clock.anchor)
    reference = now - clock.anchor;
  else
    e.tm.wrapped++; // see Telem::wrapped
  if (reference < MIN_EFFORT_TICKS)
    reference = MIN_EFFORT_TICKS;
  const uint64_t delta = reference / 1000 * e.permille;
  e.limit = e.meter(k) + delta;
  if (e.meter == meter_forward_pair) {
    if (forward_pair_limit_)
      e.limit = forward_pair_limit_;
    else
      forward_pair_limit_ = e.limit;
  }
  if (e.doses == 0 && e.budget_aware &&
      !std::strcmp(rules[e.rule].name, "Factor"))
    e.limit = e.meter(k) + factor_init_ticks_;
  if (e.doses == 0 && e.budget_aware &&
      !std::strcmp(rules[e.rule].name, "Congruence"))
    e.limit = e.meter(k) + congruence_init_ticks_;
}

void DefaultInprocessScheduler::report(const Kernel &k) const {
  std::printf("c meters      probe %llu backbone %llu vivify %llu "
              "instantiate %llu | global ticks %llu\n",
              (unsigned long long)k.stats.probe_ticks,
              (unsigned long long)k.stats.backbone_ticks,
              (unsigned long long)k.stats.vivify_ticks,
              (unsigned long long)k.stats.instantiate_ticks,
              (unsigned long long)k.stats.ticks);
  const std::vector<RuleVTable> &rules = tactix_rules();
  std::printf("c slot-order  %s", slot_order_legacy_ ? "legacy  " : "authored");
  for (const SlotEntry &e : entries)
    std::printf(" %s", rules[e.rule].name);
  std::printf("\n");
  std::printf("c slot-clock  probe phases %llu next %llu blocked %llu | "
              "eliminate phases %llu next %llu blocked %llu | bve-marks %llu\n",
              (unsigned long long)probe_clock.rounds,
              (unsigned long long)probe_clock.next_conflicts,
              (unsigned long long)probe_clock.blocked_episodes,
              (unsigned long long)elim_clock.rounds,
              (unsigned long long)elim_clock.next_conflicts,
              (unsigned long long)elim_clock.blocked_episodes,
              (unsigned long long)k.stats.bve_marks);
  for (const SlotEntry &e : entries) {
    if (!e.dosed)
      continue;
    std::printf("c cand-order  %-18s arm %-12s cursor %s | lists %llu "
                "candidates %llu rotated %llu advanced %llu ranked %llu "
                "moved %llu\n",
                rules[e.rule].name, candidate_order_name(e.order),
                e.cursor_enabled ? "on " : "off",
                (unsigned long long)e.ord.lists,
                (unsigned long long)e.ord.candidates,
                (unsigned long long)e.ord.rotated,
                (unsigned long long)e.ord.advanced,
                (unsigned long long)e.ord.ranked,
                (unsigned long long)e.ord.moved);
  }
  for (const SlotEntry &e : entries) {
    const SlotEntry::Telem &t = e.tm;
    if (!t.occasions)
      continue;
    const uint64_t declined =
        t.dec_disabled + t.dec_cap + t.dec_preprocess + t.dec_guard +
        t.dec_budget + t.dec_delay + t.dec_material + t.dec_warmup;
    const char *balance = (t.admitted + declined == t.occasions) ? "" : "  ***UNACCOUNTED***";
    std::printf(
        "c slot %-18s occ %6llu adm %5llu ok %5llu | dec dis %llu cap %llu "
        "pre %llu grd %llu bud %llu dly %llu mat %llu warm %llu "
        "| handed %llu spent %llu completed %llu | "
        "wrapped %llu%s\n",
        rules[e.rule].name, (unsigned long long)t.occasions,
        (unsigned long long)t.admitted, (unsigned long long)t.ok,
        (unsigned long long)t.dec_disabled, (unsigned long long)t.dec_cap,
        (unsigned long long)t.dec_preprocess, (unsigned long long)t.dec_guard,
        (unsigned long long)t.dec_budget, (unsigned long long)t.dec_delay,
        (unsigned long long)t.dec_material, (unsigned long long)t.dec_warmup,
        (unsigned long long)t.handed,
        (unsigned long long)t.spent, (unsigned long long)t.completed,
        (unsigned long long)t.wrapped, balance);
  }
}

int default_solve(Kernel &k, volatile sig_atomic_t *stop) {
  DefaultPolicy policy;
  const int res = policy.solve(k, stop);
  if (std::getenv("TACTIX_SLOT_STATS"))
    policy.inprocess().report(k);
  return res;
}

} // namespace tactix
