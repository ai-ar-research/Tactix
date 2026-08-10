#include "policies/default_policy.hpp"
#include "policies/event_trace.hpp"
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
#include <sstream>
#include <utility>

namespace tactix {

namespace {
// Unit conversions and protocol values, not strategy parameters.
constexpr uint64_t PERMILLE_SCALE = 1000;
constexpr uint64_t PERCENT_SCALE = 100;
constexpr uint32_t MODES_PER_CYCLE = 2;
constexpr uint32_t REORDER_STABLE_ONLY = 2;

void pose_focused_tiers(Kernel &k) {
  k.tier1 = k.tier1_by_mode[0];
  k.tier2 = std::max(k.tier1, k.tier2_by_mode[0]);
}

void pose_reduce_tiers(Kernel &k) {
  // Kissat 4.0.4's shipped internal.h deliberately uses focused tier-1 and
  // stable tier-2 for Reduce (the general focusedtiers selector is compiled
  // out). Keep the unusual hybrid explicit: it is the first-reduction witness
  // on P4A tr-n300-s055, not an inferred simplification.
  k.tier1 = k.tier1_by_mode[0];
  k.tier2 = std::max(k.tier1, k.tier2_by_mode[1]);
}

void retier_current_mode(Kernel &k,
                         const DefaultStrategyConfig::Reduction &config) {
  const uint32_t mode = k.h && k.h->stable ? 1u : 0u;
  reduce_retier(k, mode, config.tier1_default, config.tier2_default,
                config.tier1_relative_permille,
                config.tier2_relative_permille);
}
} // namespace

std::string DefaultStrategyConfig::serialize() const {
  std::ostringstream out;
  out << "strategy-v1"
      << " branching.random_initial_interval="
      << branching.random_initial_interval
      << " branching.random_sequence_length="
      << branching.random_sequence_length
      << " branching.random_pick_attempts=" << branching.random_pick_attempts
      << " branching.random_seed=" << branching.random_seed
      << " branching.logarithm_offset=" << branching.logarithm_offset
      << " branching.random_sequences=" << branching.random_sequences
      << " analysis.bump_reason_limit=" << analysis.bump_reason_limit
      << " analysis.bump_reason_rate=" << analysis.bump_reason_rate
      << " analysis.reason_depth=" << analysis.reason_depth
      << " analysis.chronological_levels=" << analysis.chronological_levels
      << " analysis.reuse_ranked_trail_suffix="
      << analysis.reuse_ranked_trail_suffix
      << " restart.glue_margin=" << restart.glue_margin
      << " restart.initial_interval=" << restart.initial_interval
      << " restart.fast_ema_window=" << restart.fast_ema_window
      << " restart.slow_ema_window=" << restart.slow_ema_window
      << " restart.logarithm_offset=" << restart.logarithm_offset
      << " restart.logarithm_bias=" << restart.logarithm_bias
      << " restart.reluctant_period=" << restart.reluctant_period
      << " restart.reluctant_limit=" << restart.reluctant_limit
      << " mode.initial_conflicts=" << mode.initial_conflicts
      << " mode.interval=" << mode.interval
      << " mode.logarithm_offset=" << mode.logarithm_offset
      << " mode.logarithm_power=" << mode.logarithm_power
      << " mode.force=" << mode.force
      << " reduction.initial_interval=" << reduction.initial_interval
      << " reduction.arithmetic_interval=" << reduction.arithmetic_interval
      << " reduction.arithmetic_step=" << reduction.arithmetic_step
      << " reduction.low_percent=" << reduction.low_percent
      << " reduction.high_percent=" << reduction.high_percent
      << " reduction.logarithm_offset=" << reduction.logarithm_offset
      << " reduction.compact_inactive_percent="
      << reduction.compact_inactive_percent
      << " reduction.retier_initial_interval="
      << reduction.retier_initial_interval
      << " reduction.retier_max_interval=" << reduction.retier_max_interval
      << " reduction.retier_growth=" << reduction.retier_growth
      << " reduction.tier1_default=" << reduction.tier1_default
      << " reduction.tier2_default=" << reduction.tier2_default
      << " reduction.tier1_relative_permille="
      << reduction.tier1_relative_permille
      << " reduction.tier2_relative_permille="
      << reduction.tier2_relative_permille
      << " reduction.square_root_cadence="
      << reduction.square_root_cadence
      << " reduction.ramp_fraction=" << reduction.ramp_fraction
      << " reduction.dynamic_tiers=" << reduction.dynamic_tiers
      << " reduction.compact_variables=" << reduction.compact_variables
      << " rephase.initial_interval=" << rephase.initial_interval
      << " rephase.interval=" << rephase.interval
      << " rephase.logarithm_offset=" << rephase.logarithm_offset
      << " rephase.logarithm_power=" << rephase.logarithm_power
      << " rephase.walk_permille=" << rephase.walk_permille
      << " rephase.stable_only=" << rephase.stable_only
      << " rephase.late_lucky=" << rephase.late_lucky
      << " reorder.initial_interval=" << reorder.initial_interval
      << " reorder.interval=" << reorder.interval
      << " reorder.mode=" << reorder.mode
      << " inprocess.eliminate_interval=" << inprocess.eliminate_interval
      << " inprocess.probe_interval=" << inprocess.probe_interval
      << " inprocess.minimum_effort_ticks="
      << inprocess.minimum_effort_ticks
      << " inprocess.preprocess_initial_ticks="
      << inprocess.preprocess_initial_ticks
      << " inprocess.phase_rounds=" << inprocess.phase_rounds
      << " inprocess.factor_initial_ticks=" << inprocess.factor_initial_ticks
      << " inprocess.congruence_initial_ticks="
      << inprocess.congruence_initial_ticks
      << " inprocess.logarithm_offset=" << inprocess.logarithm_offset
      << " inprocess.scale_quadratic=" << inprocess.scale_quadratic
      << " inprocess.scale_constant=" << inprocess.scale_constant
      << " inprocess.scale_log_shift=" << inprocess.scale_log_shift
      << " inprocess.candidate_order_seed="
      << inprocess.candidate_order_seed;
  for (size_t i = 0; i < inprocess.bve_bounds.size(); ++i)
    out << " inprocess.bve_bound" << i << '=' << inprocess.bve_bounds[i];
  out << " inprocess.bve_default_ceiling=" << inprocess.bve_default_ceiling
      << " inprocess.factor_decade_round_cap="
      << inprocess.factor_decade_round_cap
      << " inprocess.factor_decade_cap=" << inprocess.factor_decade_cap
      << " inprocess.factor_decade_offset=" << inprocess.factor_decade_offset
      << " inprocess.factor_decade_base=" << inprocess.factor_decade_base
      << " inprocess.bve_permille=" << inprocess.bve_permille
      << " inprocess.probe_permille=" << inprocess.probe_permille
      << " inprocess.hyper_binary_permille="
      << inprocess.hyper_binary_permille
      << " inprocess.backbone_permille=" << inprocess.backbone_permille
      << " inprocess.transitive_permille=" << inprocess.transitive_permille
      << " inprocess.vivify_permille=" << inprocess.vivify_permille
      << " inprocess.strengthen_permille=" << inprocess.strengthen_permille
      << " inprocess.subsume_forward_permille="
      << inprocess.subsume_forward_permille
      << " inprocess.factor_permille=" << inprocess.factor_permille
      << " inprocess.hyper_ternary_permille="
      << inprocess.hyper_ternary_permille
      << " inprocess.bce_permille=" << inprocess.bce_permille
      << " inprocess.cce_permille=" << inprocess.cce_permille
      << " inprocess.instantiate_permille=" << inprocess.instantiate_permille
      << " inprocess.sweep_permille=" << inprocess.sweep_permille
      << " inprocess.congruence_permille=" << inprocess.congruence_permille
      << " inprocess.substitute_permille=" << inprocess.substitute_permille
      << " inprocess.congruence_productivity_permille="
      << inprocess.congruence_productivity_permille
      << " inprocess.sweep_productivity_permille="
      << inprocess.sweep_productivity_permille
      << " inprocess.vivify_irredundant_productivity_percent="
      << inprocess.vivify_irredundant_productivity_percent
      << " inprocess.vivify_redundant_share="
      << inprocess.vivify_redundant_share
      << " inprocess.vivify_total_share=" << inprocess.vivify_total_share;
  for (size_t i = 0; i < inprocess.vivify_tier_shares.size(); ++i)
    out << " inprocess.vivify_tier_share" << i << '='
        << inprocess.vivify_tier_shares[i]
        << " inprocess.vivify_tier_scope" << i << '='
        << unsigned(inprocess.vivify_tier_scopes[i]);
  out << " inprocess.closure_delay=" << inprocess.closure_delay
      << " inprocess.split_clocks=" << inprocess.split_clocks
      << " inprocess.watermarks=" << inprocess.watermarks
      << " inprocess.preprocess_followups="
      << inprocess.preprocess_followups
      << " inprocess.couple_factor_bound="
      << inprocess.couple_factor_bound;
  return out.str();
}

DefaultPolicy::DefaultPolicy() : DefaultPolicy(DefaultStrategyConfig{}) {}

DefaultPolicy::DefaultPolicy(const DefaultStrategyConfig &config)
    : chrono_levels(config.analysis.chronological_levels),
      chrono_reuse_trail_(config.analysis.reuse_ranked_trail_suffix),
      base_config_(config), config_(config),
      default_branching_(&config_.branching),
      default_reduction_(&config_.reduction), inprocess_(&config_.inprocess) {
  default_reduction_.ramp = config_.reduction.ramp_fraction;
  retier_.next_conflicts = retier_.interval =
      config_.reduction.retier_initial_interval;
}

const DefaultStrategyConfig::Inprocess &
DefaultInprocessScheduler::config() const {
  static const DefaultStrategyConfig defaults;
  return config_ ? *config_ : defaults.inprocess;
}

uint32_t DefaultBveBoundRamp::num_passes() const {
  static const DefaultStrategyConfig defaults;
  const auto &cfg = config_ ? *config_ : defaults.inprocess;
  uint32_t cap = cfg.bve_default_ceiling;
  if (const char *s = std::getenv("TACTIX_BVE_RAMP_MAX"))
    cap = static_cast<uint32_t>(std::atoi(s));
  uint32_t n = 0;
  for (uint32_t i = 0; i < cfg.bve_bounds.size(); i++)
    if (cfg.bve_bounds[i] <= cap)
      n = i + 1;
  return n ? n : 1;
}

uint32_t DefaultBveBoundRamp::bound(uint32_t pass) const {
  static const DefaultStrategyConfig defaults;
  const auto &cfg = config_ ? *config_ : defaults.inprocess;
  const size_t last = cfg.bve_bounds.size() - 1;
  return cfg.bve_bounds[pass < cfg.bve_bounds.size() ? pass : last];
}

void DefaultArenaOrder::order(const Kernel &k, std::vector<Ref> &refs) {
  if (!k.h || k.h->queue_last == INVALID_VAR || refs.empty())
    return;
  std::vector<uint8_t> live(k.arena.size());
  for (const Ref ref : refs)
    live[ref] = 1;
  std::vector<Ref> ordered;
  ordered.reserve(refs.size());
  for (const bool opposite : {true, false})
    for (Var v = k.h->queue_last; v != INVALID_VAR; v = k.h->links[v].prev) {
      Lit lit = make_lit(v, k.h->saved[v] < 0);
      if (opposite)
        lit = negate(lit);
      for (const WatchEntry w : k.watches.entries(lit)) {
        if (w.binary())
          continue;
        const Ref ref = w.ref();
        if (ref < live.size() && live[ref]) {
          ordered.push_back(ref);
          live[ref] = 0;
        }
      }
    }
  for (const Ref ref : refs)
    if (live[ref])
      ordered.push_back(ref);
  refs.swap(ordered);
}

namespace {
struct ScopedArenaOrdering {
  Kernel &k;
  ArenaOrdering *saved;
  ScopedArenaOrdering(Kernel &kernel, ArenaOrdering *ordering)
      : k(kernel), saved(kernel.arena_ordering) {
    k.arena_ordering = ordering;
  }
  ~ScopedArenaOrdering() { k.arena_ordering = saved; }
};
} // namespace

void DefaultPolicy::observe_glue(const Kernel &k, uint32_t glue) {
  ModeAverages &avg = averages[k.h->stable];
  if (!avg.initialized) { // lazy, once per mode (kissat init_averages)
    avg.fast_glue.init(config_.restart.fast_ema_window);
    avg.slow_glue.init(config_.restart.slow_ema_window);
    avg.decision_rate.init(config_.restart.slow_ema_window);
    avg.initialized = true;
  }
  avg.fast_glue.update(glue);
  avg.slow_glue.update(glue);
}

void DefaultPolicy::observe_decision_rate(const Kernel &k) {
  ModeAverages &avg = averages[k.h->stable];
  if (!avg.initialized) {
    avg.fast_glue.init(config_.restart.fast_ema_window);
    avg.slow_glue.init(config_.restart.slow_ema_window);
    avg.decision_rate.init(config_.restart.slow_ema_window);
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
  uint64_t wait = v * config_.restart.reluctant_period;
  if (wait > config_.restart.reluctant_limit) {
    u = v = 1;
    wait = config_.restart.reluctant_period;
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
  return config_.restart.glue_margin * avg.slow_glue.value <=
         avg.fast_glue.value;
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
    const uint64_t n = mode.count / MODES_PER_CYCLE
                           ? mode.count / MODES_PER_CYCLE
                           : 1;
    const double log4 = std::pow(
        std::log10(static_cast<double>(n) + config_.mode.logarithm_offset),
        config_.mode.logarithm_power);
    mode.conflicts_limit =
        k.stats.conflicts +
        static_cast<uint64_t>(config_.mode.interval * static_cast<double>(n) *
                              log4);
    mode.ticks_at_focus_entry = search_ticks(k);
    update_restart_limit(k);
  }
}

void DefaultPolicy::update_restart_limit(const Kernel &k) {
  uint64_t delta = config_.restart.initial_interval;
  if (k.stats.restarts)
    delta += static_cast<uint64_t>(
        std::log10(static_cast<double>(k.stats.restarts) +
                   config_.restart.logarithm_offset) -
        config_.restart.logarithm_bias);
  restart.next_conflicts = k.stats.conflicts + delta;
}

bool DefaultPolicy::want_reduce(const Kernel &k) const {
  return k.stats.conflicts >= reduce.next_conflicts;
}

bool DefaultPolicy::want_rephase(const Kernel &k) const {
  return (!config_.rephase.stable_only || k.h->stable) &&
         k.stats.conflicts > rephase.next_conflicts;
}

bool DefaultPolicy::want_reorder(const Kernel &k) const {
  if (!config_.reorder.mode)
    return false;
  if (!k.h->stable && config_.reorder.mode < REORDER_STABLE_ONLY)
    return false;
  if (k.level != 0)
    return false; // kissat's own trigger: reorder only at the root
  return k.stats.conflicts >= reorder.next_conflicts;
}

void DefaultPolicy::update_reorder_limit(const Kernel &k) {
  reorder.next_conflicts =
      k.stats.conflicts + config_.reorder.interval * reorder.count;
}

void DefaultPolicy::update_rephase_limit(const Kernel &k) {
  const double n = static_cast<double>(rephase.count);
  const double lg = std::log10(n + config_.rephase.logarithm_offset);
  rephase.next_conflicts = k.stats.conflicts +
                           static_cast<uint64_t>(
                               config_.rephase.interval * n *
                               std::pow(lg, config_.rephase.logarithm_power));
}

void DefaultPolicy::reset_schedule(const Kernel &k) {
  const uint64_t conflicts = k.stats.conflicts;
  const uint64_t ticks = search_ticks(k);

  averages[0] = averages[1] = {};
  update_restart_limit(k);
  reduce.next_conflicts =
      conflicts +
      (reduce_sqrt_ ? config_.reduction.initial_interval
                    : config_.reduction.arithmetic_interval);
  mode = {};
  mode.conflicts_limit = conflicts + config_.mode.initial_conflicts;
  mode.ticks_at_focus_entry = ticks;
  reluctant = {};
  rephase = {};
  rephase.next_conflicts = conflicts + config_.rephase.initial_interval;
  reorder = {};
  reorder.next_conflicts = conflicts + config_.reorder.initial_interval;
  walk_anchor = ticks;
  bump_reasons_delay_ = {};
  retier_.next_conflicts = config_.reduction.retier_initial_interval;
  retier_.interval = config_.reduction.retier_initial_interval;
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

static uint64_t scale_inprocessing_delta(
    const Kernel &k, uint64_t delta,
    const DefaultStrategyConfig::Inprocess &config) {
  const double f =
      std::log10(static_cast<double>(kernel_irredundant_clauses(k)) + 1) -
      config.scale_log_shift;
  return static_cast<uint64_t>((config.scale_quadratic * f * f +
                                config.scale_constant) *
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
uint64_t meter_substitute(const Kernel &k) { return k.stats.substitute_ticks; }

struct EffortSpec {
  const char *name;
  uint32_t DefaultStrategyConfig::Inprocess::*permille;
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
  uint32_t DefaultStrategyConfig::Inprocess::*permille;
  bool denom_delta;
};
constexpr DelaySpec DELAY_SPECS[] = {
    {"Congruence", [](const Kernel &k) { return k.stats.congruence_merges; },
     active_variables,
     &DefaultStrategyConfig::Inprocess::congruence_productivity_permille,
     false},
    {"Sweep", [](const Kernel &k) { return k.stats.sweep_equivalences; },
     [](const Kernel &k) { return k.stats.sweep_candidates; },
     &DefaultStrategyConfig::Inprocess::sweep_productivity_permille, true},
};
static const DelaySpec *delay_spec(const char *name) {
  for (const DelaySpec &d : DELAY_SPECS)
    if (!std::strcmp(d.name, name))
      return &d;
  return nullptr;
}

constexpr EffortSpec EFFORT_SPECS[] = {
    {"BVE", &DefaultStrategyConfig::Inprocess::bve_permille, meter_bve, true,
     false, nullptr},
    {"Probe", &DefaultStrategyConfig::Inprocess::probe_permille, meter_probe,
     true, false, &Stats::probe_ticks},
    {"HyperBinaryRes",
     &DefaultStrategyConfig::Inprocess::hyper_binary_permille, meter_hbr, true,
     true, &Stats::hbr_ticks},
    {"Backbone", &DefaultStrategyConfig::Inprocess::backbone_permille,
     meter_backbone, true, false, &Stats::backbone_ticks},
    {"TransitiveReduce", &DefaultStrategyConfig::Inprocess::transitive_permille,
     meter_transitive, true, false},
    {"Vivify", &DefaultStrategyConfig::Inprocess::vivify_permille,
     meter_vivify, false, true, &Stats::vivify_ticks},
    {"Strengthen", &DefaultStrategyConfig::Inprocess::strengthen_permille,
     meter_forward_pair, false, true, nullptr},
    {"SubsumeForward",
     &DefaultStrategyConfig::Inprocess::subsume_forward_permille,
     meter_forward_pair, false, true, nullptr},
    {"Factor", &DefaultStrategyConfig::Inprocess::factor_permille,
     meter_factor, false, true, nullptr},
    {"HyperTernaryRes",
     &DefaultStrategyConfig::Inprocess::hyper_ternary_permille, meter_ternary,
     false, true, nullptr},
    {"BCE", &DefaultStrategyConfig::Inprocess::bce_permille, meter_bce, false,
     true},
    {"CCE", &DefaultStrategyConfig::Inprocess::cce_permille, meter_cce, false,
     true},
    {"Instantiate", &DefaultStrategyConfig::Inprocess::instantiate_permille,
     meter_instantiate, false, true, &Stats::instantiate_ticks},
    {"Sweep", &DefaultStrategyConfig::Inprocess::sweep_permille, meter_sweep,
     false, true},
    {"Congruence", &DefaultStrategyConfig::Inprocess::congruence_permille,
     meter_congruence, false, true, nullptr, true},
    {"Substitute", &DefaultStrategyConfig::Inprocess::substitute_permille,
     meter_substitute, false, true, nullptr, true},
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

uint64_t instance_key(uint64_t inst) {
  Lit l;
  return lit_of_instance(inst, &l) ? static_cast<uint64_t>(l) : inst;
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
      rng_ += increment_;
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
    while (start < n && instance_key(insts[start]) < e.cursor)
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
  if (k.stats.conflicts == 0 && g == SlotGroup::Eliminate)
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
  const auto &cfg = config();
  SlotClock &c = g == SlotGroup::Probe ? probe_clock : elim_clock;
  const bool split = slot_clocks_split_;
  uint64_t n = split ? c.rounds
                     : std::max(probe_clock.rounds, elim_clock.rounds);
  if (!n)
    n = 1;
  const double lg =
      std::log10(static_cast<double>(n) + cfg.logarithm_offset);
  const double base = split && g == SlotGroup::Probe
                          ? cfg.probe_interval * static_cast<double>(n) * lg
                          : cfg.eliminate_interval * static_cast<double>(n) *
                                lg * lg;
  c.next_conflicts = k.stats.conflicts +
                     scale_inprocessing_delta(k, static_cast<uint64_t>(base),
                                              cfg);
}

void DefaultPolicy::update_reduce_limit(const Kernel &k) {
  if (reduce_sqrt_) {
    const double n = static_cast<double>(
        k.stats.reductions ? k.stats.reductions : 1);
    reduce.next_conflicts =
        k.stats.conflicts +
        static_cast<uint64_t>(config_.reduction.initial_interval *
                              std::sqrt(n));
    return;
  }
  reduce.next_conflicts = k.stats.conflicts +
                          config_.reduction.arithmetic_interval +
                          config_.reduction.arithmetic_step *
                              k.stats.reductions;
}

bool DefaultPolicy::update_tiers_after_analysis(Kernel &k) {
  // Kissat analyze.c advances this exponential schedule only on the extra
  // conflict created when OTFS restarts 1-UIP analysis. Ordinary propagated
  // conflicts do not test the glue limit. Analyze remains atomic at the rule
  // boundary, so it records the histogram seen at each internal OTFS boundary
  // and the policy consumes those observations here. Reduce has its own
  // independent current-mode refresh.
  bool updated = false;
  if (dynamic_tiers_)
    for (const AnalysisConflictCheckpoint &checkpoint :
         k.analysis_conflict_checkpoints) {
      if (checkpoint.conflicts <= retier_.next_conflicts)
        continue;
      reduce_retier_from_histogram(
          k, checkpoint.mode, checkpoint.used_glue,
          config_.reduction.tier1_default,
          config_.reduction.tier2_default,
          config_.reduction.tier1_relative_permille,
          config_.reduction.tier2_relative_permille);
      pose_focused_tiers(k);
      if (retier_.interval < config_.reduction.retier_max_interval)
        retier_.interval *= config_.reduction.retier_growth;
      retier_.next_conflicts = checkpoint.conflicts + retier_.interval;
      event_trace_retier(k, checkpoint.mode, checkpoint.used_glue);
      updated = true;
    }
  k.analysis_conflict_checkpoints.clear();
  return updated;
}

size_t GlueTieredReduction::victims(const Kernel &k, size_t candidates) {
  if (!ramp)
    return reduce_canonical_victims(candidates);
  static const DefaultStrategyConfig defaults;
  const auto &cfg = config_ ? *config_ : defaults.reduction;
  const double r = static_cast<double>(k.stats.reductions) + 1.0;
  const double percent =
      cfg.high_percent - (cfg.high_percent - cfg.low_percent) /
                             std::log10(r + cfg.logarithm_offset);
  return static_cast<size_t>(static_cast<double>(candidates) * percent /
                             PERCENT_SCALE);
}

Lit DefaultBranching::pick(Kernel &k) {
  static const DefaultStrategyConfig defaults;
  const auto &cfg = config_ ? *config_ : defaults.branching;
  assert(k.unassigned > 0);
  if (randec_enabled && !k.h->stable) {
    const uint64_t c = k.stats.conflicts;
    if (c >= randec_until && k.level <= 1 && c >= randec_next) {
      randec_sequences++;
      const double n = static_cast<double>(randec_sequences);
      const double logn = std::log10(n + cfg.logarithm_offset);
      randec_until =
          c + static_cast<uint64_t>(cfg.random_sequence_length * logn) + 1;
      randec_next =
          c + static_cast<uint64_t>(cfg.random_initial_interval * logn);
      k.stats.random_sequences++;
    }
    if (c < randec_until) {
      for (uint32_t tries = 0; tries < cfg.random_pick_attempts; tries++) {
        randec_rng += cfg.random_seed;
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
  config_ = base_config_;
  // Preserve the pre-existing white-box policy pose surface: callers and
  // focused tests may set these public members before solve().
  config_.analysis.chronological_levels = chrono_levels;
  config_.analysis.reuse_ranked_trail_suffix = chrono_reuse_trail_;
  config_.branching.random_sequences = default_branching_.randec_enabled;
  if (const char *fm = std::getenv("TACTIX_FORCE_MODE")) {
    if (!std::strcmp(fm, "focused"))
      config_.mode.force = 1;
    else if (!std::strcmp(fm, "stable"))
      config_.mode.force = 2;
  }
  if (std::getenv("TACTIX_NO_BUMP_REASONS"))
    config_.analysis.bump_reason_limit = 0;
  if (const char *bl = std::getenv("TACTIX_BUMP_REASONS_LIMIT"))
    config_.analysis.bump_reason_limit =
        static_cast<uint32_t>(std::strtoul(bl, nullptr, 10));
  if (const char *br = std::getenv("TACTIX_BUMP_REASONS_RATE"))
    config_.analysis.bump_reason_rate = std::strtod(br, nullptr);
  const char *cad = std::getenv("TACTIX_REDUCE_CADENCE"); // "sqrt" | "arith"
  const char *rmp = std::getenv("TACTIX_REDUCE_RAMP");    // "1" | "0"
  if (cad)
    config_.reduction.square_root_cadence =
        std::strcmp(cad, "arith") != 0;
  if (rmp)
    config_.reduction.ramp_fraction = rmp[0] == '1';
  else if (cad)
    config_.reduction.ramp_fraction =
        config_.reduction.square_root_cadence;
  if (std::getenv("TACTIX_NO_DYNAMIC_TIERS"))
    config_.reduction.dynamic_tiers = false;
  if (std::getenv("TACTIX_NO_COMPACT"))
    config_.reduction.compact_variables = false;
  if (const char *ll = std::getenv("TACTIX_LUCKY_LATE"))
    config_.rephase.late_lucky = std::strtoul(ll, nullptr, 10) != 0;
  if (std::getenv("TACTIX_REPHASE_BOTH_MODES"))
    config_.rephase.stable_only = false;
  if (const char *ro = std::getenv("TACTIX_REORDER"))
    config_.reorder.mode =
        static_cast<uint32_t>(std::strtoul(ro, nullptr, 10));
  if (const char *cl = std::getenv("TACTIX_CHRONO_LEVELS")) {
    const long n = std::strtol(cl, nullptr, 10);
    if (n >= 0)
      config_.analysis.chronological_levels = static_cast<uint32_t>(n);
  }
  if (const char *cr = std::getenv("TACTIX_CHRONO_REUSE"))
    config_.analysis.reuse_ranked_trail_suffix =
        std::strtoul(cr, nullptr, 10) != 0;
  if (const char *rd = std::getenv("TACTIX_RANDEC"))
    config_.branching.random_sequences = *rd != '0';
  if (const char *wp = std::getenv("TACTIX_WALK_PERMILLE"))
    config_.rephase.walk_permille = std::strtoull(wp, nullptr, 10);

  force_mode_ = config_.mode.force;
  bump_reasons_limit_ = config_.analysis.bump_reason_limit;
  bump_reasons_rate_ = config_.analysis.bump_reason_rate;
  reduce_sqrt_ = config_.reduction.square_root_cadence;
  dynamic_tiers_ = config_.reduction.dynamic_tiers;
  compact_ = config_.reduction.compact_variables;
  lucky_late_ = config_.rephase.late_lucky;
  chrono_levels = config_.analysis.chronological_levels;
  chrono_reuse_trail_ = config_.analysis.reuse_ranked_trail_suffix;
  default_branching_.set_config(&config_.branching);
  default_branching_.randec_enabled = config_.branching.random_sequences;
  default_reduction_.set_config(&config_.reduction);
  default_reduction_.ramp = config_.reduction.ramp_fraction;
  inprocess_.set_config(&config_.inprocess);
  inprocess_.configure_from_env();
}

void DefaultInprocessScheduler::configure_from_env() {
  const auto &cfg = config();
  closure_delay_ = cfg.closure_delay;
  slot_clocks_split_ = cfg.split_clocks;
  slot_watermark_ = cfg.watermarks;
  couple_factor_bound_ = cfg.couple_factor_bound;
  if (std::getenv("TACTIX_NO_CLOSURE_DELAY"))
    closure_delay_ = false;
  factor_init_ticks_ = cfg.factor_initial_ticks;
  if (const char *fi = std::getenv("TACTIX_FACTOR_INIT_TICKS"))
    factor_init_ticks_ = std::strtoull(fi, nullptr, 10);
  congruence_init_ticks_ = cfg.congruence_initial_ticks;
  if (const char *ci = std::getenv("TACTIX_CONG_INIT_TICKS"))
    congruence_init_ticks_ = std::strtoull(ci, nullptr, 10);
  preprocess_followups_ = cfg.preprocess_followups;
  if (const char *pf = std::getenv("TACTIX_PREPROCESS_FOLLOWUPS"))
    preprocess_followups_ = std::strtoul(pf, nullptr, 10) != 0;
}

void DefaultInprocessScheduler::reset(const Kernel &k) {
  initialized_ = false;
  const uint64_t ticks = search_ticks(k);
  probe_clock = elim_clock = {};
  probe_clock.anchor = elim_clock.anchor = ticks;
  bve_rounds_run_ = 0;
  bve_level_ = 0;
  bve_level_idx_ = 0;
  default_candidate_order_.reset(config().candidate_order_seed);
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
          e.permille = config().*(s->permille);
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
    static constexpr const char *PREPROCESS_ORDER[] = {
        "Congruence", "Substitute", "Backbone",
        "Sweep", "Substitute", "Factor"};
    size_t begin = 0;
    unsigned substitutes = 0;
    for (const char *name : PREPROCESS_ORDER)
      for (size_t i = begin; i < entries.size(); i++)
        if (!std::strcmp(rules[entries[i].rule].name, name)) {
          entries[i].preprocess = true;
          if (!std::strcmp(name, "Substitute"))
            entries[i].followup = substitutes++ == 0
                                      ? FollowupKind::AfterCongruence
                                      : FollowupKind::AfterSweepOrFinal;
          begin = i + 1;
          break;
        }
    default_candidate_order_.arms.assign(rules.size(),
                                         CandidateOrder::Native);
    for (const SlotEntry &se : entries)
      default_candidate_order_.arms[se.rule] = se.order;
  }
  if (!initialized_) {
    initialized_ = true;
    // The probe clock is due immediately because it owns initial
    // preprocessing.  Elimination is not part of that pipeline: Kissat seeds
    // its first search-time occurrence at scale(eliminateinit=500)
    // (kimits.c kissat_init_limits), then applies NLOG2N only after a phase.
    // update_limit with rounds==0 uses n=1 and therefore gives that exact
    // initial interval.  Seeding both clocks at zero used to run the entire
    // elimination pipeline immediately after the first conflict.
    probe_clock.next_conflicts = k.stats.conflicts;
    update_limit(k, SlotGroup::Eliminate);
  }
}

int DefaultPolicy::solve(Kernel &k, volatile sig_atomic_t *stop) {
  KernelStopPublisher stop_published(k, stop);
  ScopedArenaOrdering arena_ordered(k, inprocess_.arena_ordering());
  if (k.inconsistent)
    return 20;
  std::optional<ScopedHeuristics> owned_h;
  if (!k.h)
    owned_h.emplace(k);
  solves++;
  configure_from_env();
  if (reset_schedule_per_solve) {
    for (uint32_t mode = 0; mode != 2; ++mode) {
      k.tier1_by_mode[mode] = config_.reduction.tier1_default;
      k.tier2_by_mode[mode] = std::max(config_.reduction.tier1_default,
                                       config_.reduction.tier2_default);
    }
    pose_focused_tiers(k);
    reset_schedule(k);
  }
  inprocess_.build(k);
  EventTrace event_trace;
  event_trace.open_from_env(k);
  event_trace.macro(k, "startup_begin");
  auto finish = [&](int verdict) {
    char payload[16];
    std::snprintf(payload, sizeof payload, "%d", verdict);
    event_trace.macro(k, "terminate", payload);
    event_trace.close();
    return verdict;
  };

  // The references expose two distinct Lucky opportunities around initial
  // preprocessing. Keep them out of the CDCL loop so neither can repeat after
  // a later inprocessing phase.
  ApplyResult startup = propagate_apply(k);
  if (startup != ApplyResult::Ok)
    return finish(20); // startup is at root: a conflict is a refutation
  if (lucky_guard(k)) {
    event_trace.macro(k, "lucky_early_begin");
    k.stats.lucky_early_calls++;
    if (lucky_apply(k) == ApplyResult::Sat) {
      event_trace.macro(k, "lucky_early_end", "sat");
      return finish(10);
    }
    event_trace.macro(k, "lucky_early_end", "unknown");
  }
  event_trace.macro(k, "preprocess_begin");
  if (inprocess_.want_phase(k, SlotGroup::Probe) &&
      inprocess_.run_phase(k, SlotGroup::Probe, stop) == 20)
    return finish(20);
  event_trace.macro(k, "preprocess_end");
  startup = propagate_apply(k);
  if (startup != ApplyResult::Ok)
    return finish(20);
  if (lucky_late_ && lucky_guard(k)) {
    event_trace.macro(k, "lucky_late_begin");
    k.stats.lucky_late_calls++;
    if (lucky_apply(k) == ApplyResult::Sat) {
      k.stats.lucky_late_sat++;
      event_trace.macro(k, "lucky_late_end", "sat");
      return finish(10);
    }
    event_trace.macro(k, "lucky_late_end", "unknown");
  }

  // Kissat initializes the first focused-mode tick anchor in `start_search`,
  // after Lucky and preprocessing.  Tactix's schedule object is reset before
  // those startup phases, whose kernel work also uses the shared tick meter;
  // rebase this one clock so startup work cannot lengthen the first stable
  // interval.  Later anchors are maintained by `update_mode_limits`.
  mode.ticks_at_focus_entry = search_ticks(k);

  for (;;) {
    event_trace.checkpoint(k);
    const size_t trace_trail_begin = k.trail.size();
    const ApplyResult r = propagate_apply(k);
    event_trace.implications(k, trace_trail_begin);
    if (r == ApplyResult::Unsat)
      return finish(20);
    if (r == ApplyResult::Conflict) {
      event_trace.conflict(k);
      event_trace.detail(k, "analysis_begin", k.level);
      event_trace.mode_clock(k, "pre_analyze", mode.ticks_limit,
                             mode.ticks_at_focus_entry, mode.count);
      observe_decision_rate(k); // kissat: before the analysis, once per conflict
      k.bump_reason_limit = want_bump_reasons(k);
      k.bump_reason_depth = config_.analysis.reason_depth;
      const uint64_t br_rounds = k.stats.reason_bump_rounds;
      const uint64_t br_aborted = k.stats.reason_bump_aborted;
      const ApplyResult ar = analyze_apply(k);
      k.bump_reason_limit = 0;
      k.bump_reason_depth = 1;
      if (k.stats.reason_bump_rounds != br_rounds) { // the pass actually ran
        if (k.stats.reason_bump_aborted != br_aborted) // over the mark limit
          bump_reasons_delay_.current +=
              (bump_reasons_delay_.current < UINT32_MAX);
        else if (bump_reasons_delay_.current)
          bump_reasons_delay_.current /= 2;
        bump_reasons_delay_.count = bump_reasons_delay_.current;
      }
      if (ar == ApplyResult::Unsat) {
        event_trace.detail(k, "analysis_end", -1);
        return finish(20);
      }
      update_tiers_after_analysis(k);
      if (!k.pending.valid) { // `stats.conflict_reused` already counted it
        event_trace.detail(k, "analysis_end", k.level);
        event_trace.mode_clock(k, "post_analyze", mode.ticks_limit,
                               mode.ticks_at_focus_entry, mode.count);
        continue;
      }
      event_trace.learned(k, k.pending.lits);
      observe_glue(k, k.pending.glue); // policy observes; Analyze is pure
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
      event_trace.detail(k, "analysis_end", k.level);
      event_trace.mode_clock(k, "post_analyze", mode.ticks_limit,
                             mode.ticks_at_focus_entry, mode.count);
      continue;
    }
    if (k.retire_enabled ? k.unassigned_active == 0 : k.unassigned == 0)
      return finish(10);
    if (want_reduce(k) && reduce_guard(k)) {
      event_trace.macro(k, "reduce_begin");
      ReductionPolicy &rp = reduction ? *reduction : default_reduction_;
      if (dynamic_tiers_)
        retier_current_mode(k, config_.reduction);
      pose_reduce_tiers(k);
      reduce_candidates(k, reduce_scratch_);
      const size_t reduce_victims =
          std::min(rp.victims(k, reduce_scratch_.size()),
                   reduce_scratch_.size());
      event_trace.reduction(k, reduce_scratch_, reduce_victims);
      reduce_apply_refs(
          k, reduce_scratch_.data(), reduce_victims);
      if (compact_ && kernel_should_compact_variables(
                          k, config_.reduction.compact_inactive_percent))
        kernel_compact_variables(k);
      update_reduce_limit(k);
      pose_focused_tiers(k); // vivifyfocusedtiers=1 is the shipped default.
      event_trace.macro(k, "reduce_end");
      continue;
    }
    const bool pin_mode =
        force_mode_ == 1 || (force_mode_ == 2 && k.h->stable);
    if (!pin_mode && want_switch_mode(k) && switchmode_guard(k)) {
      event_trace.macro(k, "switch_begin");
      switchmode_apply(k);
      mode.count++;
      averages[k.h->stable].saved_decisions = k.stats.decisions;
      if (k.h->stable) // entering stable: fresh reluctant sequence
        reluctant = {};
      update_mode_limits(k);
      event_trace.macro(k, "switch_end");
      event_trace.mode_clock(k, "post_switch", mode.ticks_limit,
                             mode.ticks_at_focus_entry, mode.count);
      continue;
    }
    if (want_restart(k) && restart_guard(k)) {
      event_trace.macro(k, "restart_begin");
      restart_apply(k);
      reluctant.trigger = false; // consumed (kissat reluctant.h)
      if (!k.h->stable) // kissat restart.c: focused-only limit update
        update_restart_limit(k);
      event_trace.macro(k, "restart_end");
      continue;
    }
    if (want_reorder(k)) {
      k.stats.reorder_eligible++;
      if (reorder_guard(k)) {
        event_trace.reorder_begin(k);
        reorder_apply(k);
        reorder.count++;
        update_reorder_limit(k);
        event_trace.reorder_end(k);
        continue;
      }
    }
    if (want_rephase(k) && rephase_guard(k)) {
      event_trace.macro(k, "rephase_begin");
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
        if (reference < config_.inprocess.minimum_effort_ticks)
          reference = config_.inprocess.minimum_effort_ticks;
        k.work_budget =
            reference / PERMILLE_SCALE * config_.rephase.walk_permille;
        walk_apply(k);
        k.work_budget = 0;
        walk_anchor = search_ticks(k);
      } else {
        rephase_apply_type(k, type == WALK_SLOT ? REPHASE_BEST : type);
      }
      update_rephase_limit(k);
      event_trace.macro(k, "rephase_end");
      continue;
    }
    if (inprocess_.want_phase(k, SlotGroup::Probe)) {
      event_trace.macro(k, "probe_begin");
      if (inprocess_.run_phase(k, SlotGroup::Probe, stop) == 20)
        return finish(20);
      event_trace.macro(k, "probe_end");
      continue;
    }
    if (inprocess_.want_phase(k, SlotGroup::Eliminate)) {
      event_trace.macro(k, "eliminate_begin");
      if (inprocess_.run_phase(k, SlotGroup::Eliminate, stop) == 20)
        return finish(20);
      event_trace.macro(k, "eliminate_end");
      continue;
    }
    if (stop && *stop)
      return finish(0);
    BranchingPolicy &bp = branching ? *branching : default_branching_;
    const Lit decision = bp.pick(k);
    decide_apply(k, decision, false);
    event_trace.detail(k, "decision", kernel_externalize(k, decision));
  }
}


namespace {
void slot_trace_entry(const Kernel &k, const DefaultInprocessScheduler::SlotEntry &e,
                      const RuleVTable &r, int outer) {
  if (!std::getenv("TACTIX_SLOT_TRACE"))
    return;
  std::fprintf(stderr,
               "[slot] phase=%llu outer=%d %s enabled=%d defer=%d "
               "guard=%d dosed=%d followup=%u mode=%u meter=%llu limit=%llu\n",
               (unsigned long long)k.stats.inprocess_phases, outer, r.name,
               (int)e.enabled, (int)e.defer_preprocess, (int)r.guard(k),
               (int)e.dosed, static_cast<unsigned>(e.followup),
               static_cast<unsigned>(e.followup_mode),
               (unsigned long long)(e.meter ? e.meter(k) : 0),
               (unsigned long long)e.limit);
}
} // namespace

bool DefaultInprocessScheduler::slot_entry_admissible(SlotEntry &e, const Kernel &k,
                                          const SlotPhaseCtx &ctx) {
  if (!e.enabled || ctx.phase_unsat) {
    e.tm.dec_disabled++;
    return false;
  }
  if (k.stats.conflicts == 0 && !e.preprocess) {
    e.tm.dec_preprocess++;
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

bool DefaultInprocessScheduler::followup_admissible(SlotEntry &e,
                                                     const Kernel &k,
                                                     SlotPhaseCtx &ctx) {
  e.followup_mode = FollowupMode::None;
  if (k.stats.conflicts != 0 || e.followup == FollowupKind::None ||
      !preprocess_followups_)
    return true;
  e.tm.followup_offered++;
  e.followup_mode = preprocessing_followup_mode(
      e.followup, ctx.congruence_changed, ctx.sweep_changed);
  if (e.followup_mode == FollowupMode::None)
    return false;
  e.tm.followup_taken++;
  if (e.followup_mode == FollowupMode::Eager)
    e.tm.followup_eager++;
  else
    e.tm.followup_final++;
  return true;
}

FollowupMode DefaultInprocessScheduler::preprocessing_followup_mode(
    FollowupKind kind, bool congruence_changed, bool sweep_changed) {
  if (kind == FollowupKind::AfterCongruence)
    return congruence_changed ? FollowupMode::Eager : FollowupMode::None;
  if (kind == FollowupKind::AfterSweepOrFinal) {
    if (sweep_changed)
      return FollowupMode::Eager;
    if (!congruence_changed)
      return FollowupMode::Final;
  }
  return FollowupMode::None;
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
  ctx.last_result = ApplyResult::NoOp;
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
        e.cursor = instance_key(payloads_[i]) + 1;
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
      ctx.last_result = ar;
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
  const auto &cfg = config();
  const uint64_t active = active_variables(k);
  uint64_t threshold = 1;
  const uint32_t decades =
      bve_rounds_run_ > cfg.factor_decade_round_cap
          ? cfg.factor_decade_cap
          : bve_rounds_run_ + cfg.factor_decade_offset;
  for (uint32_t d = 0; d < decades; d++)
    threshold *= cfg.factor_decade_base;
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
  const auto &cfg = config();
  const uint64_t remaining0 = ledger_remaining(e, k, limit);
  const uint64_t remaining = remaining0 ? remaining0 : 1;
  const bool irr_delayed = k.stats.conflicts > 0 && e.delay_count > 0;
  if (irr_delayed)
    e.delay_count--;
  const char *no_t3 = std::getenv("TACTIX_VIVIFY_TIER3");
  const bool split = !(no_t3 && *no_t3 == '0');
  ApplyResult ar = ApplyResult::NoOp;
  const uint64_t red = irr_delayed
                           ? remaining
                           : remaining * cfg.vivify_redundant_share /
                                 cfg.vivify_total_share;
  const uint32_t passes =
      split ? static_cast<uint32_t>(cfg.vivify_tier_scopes.size()) : 1u;
  for (uint32_t p = 0; p < passes && ar != ApplyResult::Unsat && !k.inconsistent;
       p++) {
    k.vivify_scope = split ? cfg.vivify_tier_scopes[p] : uint8_t{1};
    const uint64_t tier_share =
        red * cfg.vivify_tier_shares[p] / cfg.vivify_redundant_share;
    k.work_budget = std::max<uint64_t>(1, split ? tier_share : red);
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
      if (tried == 0 ||
          produced * PERCENT_SCALE <
              tried * cfg.vivify_irredundant_productivity_percent) {
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
  ctx.last_result = ApplyResult::NoOp;
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
  ctx.last_result = ar;
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
      const uint32_t permille = config().*(dspec->permille);
      unproductive = denom == 0 ||
                     yield * PERMILLE_SCALE < uint64_t(permille) * denom;
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
  const int rounds =
      k.stats.conflicts == 0 ? 1 : static_cast<int>(config().phase_rounds);
  for (ctx.outer = 0;
       ctx.outer < rounds && !ctx.phase_unsat &&
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
      e.tm.occasions++;
      const bool followup = followup_admissible(e, k, ctx);
      slot_trace_entry(k, e, r, ctx.outer);
      if (!followup) {
        e.tm.dec_preprocess++;
        continue;
      }
      if (!slot_entry_admissible(e, k, ctx))
        continue;
      const bool substitute = !std::strcmp(r.name, "Substitute");
      if (e.enabled && e.meter) {
        if (substitute) {
          if (ctx.outer == 0 || e.meter(k) <= e.limit)
            set_entry_limit(e, k);
        } else if (group == SlotGroup::Probe || ctx.outer == 0) {
          set_entry_limit(e, k);
        }
      }
      const SlotStep step = (e.dosed && r.instances && r.apply_instance)
                                ? run_dosed_entry(k, e, r, ctx)
                                : run_batch_entry(k, e, r, ctx);
      if (!std::strcmp(r.name, "Congruence"))
        ctx.congruence_changed = ctx.last_result == ApplyResult::Ok;
      else if (!std::strcmp(r.name, "Sweep"))
        ctx.sweep_changed = ctx.last_result == ApplyResult::Ok;
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
  // Kissat leaves every probe/eliminate pipeline by rebuilding sparse large
  // watches.  This is ordering state, not optional garbage collection: in
  // particular it resets the mutations made by unsuccessful early Lucky
  // trials before late Lucky and first search.
  kernel_rebuild_large_watches(k);
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
  const bool substitute =
      !std::strcmp(rules[e.rule].name, "Substitute");
  if (k.stats.conflicts == 0)
    reference = config().preprocess_initial_ticks;
  else if (!substitute && reference < config().minimum_effort_ticks)
    reference = config().minimum_effort_ticks;
  const uint64_t delta = substitute
                             ? reference / PERMILLE_SCALE * e.permille +
                                   reference % PERMILLE_SCALE * e.permille /
                                       PERMILLE_SCALE
                             : reference / PERMILLE_SCALE * e.permille;
  e.limit = e.meter(k) + delta;
  if (e.meter == meter_forward_pair) {
    if (forward_pair_limit_)
      e.limit = forward_pair_limit_;
    else
      forward_pair_limit_ = e.limit;
  }
  if (k.stats.conflicts > 0 && e.doses == 0 && e.budget_aware &&
      !std::strcmp(rules[e.rule].name, "Factor"))
    e.limit = e.meter(k) + factor_init_ticks_;
  if (k.stats.conflicts > 0 && e.doses == 0 && e.budget_aware &&
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
    if (t.followup_offered)
      std::printf("c followup %-14s offered %llu taken %llu eager %llu final %llu\n",
                  rules[e.rule].name,
                  (unsigned long long)t.followup_offered,
                  (unsigned long long)t.followup_taken,
                  (unsigned long long)t.followup_eager,
                  (unsigned long long)t.followup_final);
  }
}

int default_solve(Kernel &k, volatile sig_atomic_t *stop) {
  DefaultPolicy policy;
  const int res = policy.solve(k, stop);
  if (std::getenv("TACTIX_STRATEGY_CONFIG"))
    std::printf("c strategy-config %s\n",
                policy.strategy_config_snapshot().c_str());
  if (std::getenv("TACTIX_SLOT_STATS"))
    policy.inprocess().report(k);
  return res;
}

} // namespace tactix
