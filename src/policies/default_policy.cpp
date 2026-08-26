#include "policies/default_policy.hpp"
#include "diagnostics/event_trace.hpp"
#include "diagnostics/rule_journal.hpp"
#include "policies/heuristics.hpp"

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/bve/bve.hpp"
#include "rules/decide/decide.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/reduce/reduce.hpp"
#include "policies/ops/rephase/rephase.hpp"
#include "policies/ops/reorder/reorder.hpp"
#include "policies/ops/walk/walk.hpp"
#include "rules/restart/restart.hpp"
#include "rules/registry.hpp"
#include "rules/substitute/substitute.hpp"
#include "rules/sweep/sweep.hpp"
#include "rules/vivify/vivify.hpp"
#include "policies/ops/switchmode/switchmode.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <sstream>
#include <utility>

namespace tactix {

double kissat_bve_variable_score(double relevance, uint64_t positive,
                                 uint64_t negative,
                                 uint64_t occurrence_limit) {
  positive = std::min(positive, occurrence_limit);
  negative = std::min(negative, occurrence_limit);
  const double product = positive * negative;
  const double sum = positive + negative;
  const double occurrence_limit_squared =
      occurrence_limit * static_cast<double>(occurrence_limit);
  const double structural_score = product - sum;
  return relevance + structural_score - occurrence_limit_squared;
}

namespace {
template <class Apply>
ApplyResult default_direct_rule(Kernel &k, const char *name,
                                const std::string &candidate, Apply &&apply,
                                ApplyResult declined_result =
                                    ApplyResult::NoOp) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  return journal_direct_rule(k, name, candidate,
                             std::function<ApplyResult()>(apply),
                             declined_result);
#else
  (void)k;
  (void)name;
  (void)candidate;
  (void)declined_result;
  return apply();
#endif
}

ApplyResult default_propagate(Kernel &k, const char *candidate) {
  const size_t trail_begin = k.trail.size();
  const ApplyResult result = default_direct_rule(
      k, "Propagate", candidate, [&] { return propagate_apply(k); },
      ApplyResult::Ok);
  // Kissat marks every true-level-zero assignment fixed in its assignment
  // primitive. Keep the proof rule independent of policy state, but consume
  // the same lifecycle transition before DefaultPolicy next chooses a rule or
  // decision. This includes root implications discovered above level zero.
  heuristics_consume_root_assignments(k, trail_begin);
  return result;
}
} // namespace

namespace {
// Unit conversions and protocol values, not strategy parameters.
constexpr uint64_t PERMILLE_SCALE = 1000;

// Kissat's SET_EFFORT_LIMIT converts the per-mille option to a scale factor
// and only truncates after multiplying by the complete reference.  Splitting
// the quotient and remainder keeps that floor exact without overflowing the
// uint64_t product for the supported 0..1000 per-mille range.
static uint64_t scale_permille(uint64_t reference, uint32_t permille) {
  assert(permille <= PERMILLE_SCALE);
  return reference / PERMILLE_SCALE * permille +
         reference % PERMILLE_SCALE * permille / PERMILLE_SCALE;
}
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
      << " branching.random_seed=" << branching.random_seed
      << " branching.logarithm_offset=" << branching.logarithm_offset
      << " branching.focused_phase_switch_span="
      << branching.focused_phase_switch_span
      << " branching.random_sequences=" << branching.random_sequences;
  for (size_t i = 0; i < branching.focused_phase_pattern.size(); ++i)
    out << " branching.focused_phase" << i << '='
        << static_cast<int>(branching.focused_phase_pattern[i]);
  out << " analysis.bump_reason_limit=" << analysis.bump_reason_limit
      << " analysis.bump_reason_rate=" << analysis.bump_reason_rate
      << " analysis.reason_depth=" << analysis.reason_depth
      << " analysis.chronological_levels=" << analysis.chronological_levels
      << " analysis.reuse_ranked_trail_suffix="
      << analysis.reuse_ranked_trail_suffix
      << " analysis.small_clause_limit=" << analysis.small_clause_limit
      << " analysis.big_binary_fraction_permille="
      << analysis.big_binary_fraction_permille
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
      << " inprocess.bve_enabled=" << inprocess.bve_enabled
      << " inprocess.vivify_enabled=" << inprocess.vivify_enabled
      << " inprocess.factor_repeated_enabled="
      << inprocess.factor_repeated_enabled
      << " inprocess.preprocess_only=" << inprocess.preprocess_only
      << " inprocess.eliminate_interval=" << inprocess.eliminate_interval
      << " inprocess.probe_interval=" << inprocess.probe_interval
      << " inprocess.minimum_effort_ticks="
      << inprocess.minimum_effort_ticks
      << " inprocess.preprocess_initial_ticks="
      << inprocess.preprocess_initial_ticks
      << " inprocess.phase_rounds=" << inprocess.phase_rounds
      << " inprocess.substitute_rounds=" << inprocess.substitute_rounds
      << " inprocess.factor_initial_ticks=" << inprocess.factor_initial_ticks
      << " inprocess.logarithm_offset=" << inprocess.logarithm_offset
      << " inprocess.scale_quadratic=" << inprocess.scale_quadratic
      << " inprocess.scale_constant=" << inprocess.scale_constant
      << " inprocess.scale_log_shift=" << inprocess.scale_log_shift
      << " inprocess.candidate_order_seed="
      << inprocess.candidate_order_seed;
  for (size_t i = 0; i < inprocess.bve_bounds.size(); ++i)
    out << " inprocess.bve_bound" << i << '=' << inprocess.bve_bounds[i];
  out << " inprocess.bve_default_ceiling=" << inprocess.bve_default_ceiling
      << " inprocess.bve_occurrence_limit="
      << inprocess.bve_occurrence_limit
      << " inprocess.bve_clause_size_limit="
      << inprocess.bve_clause_size_limit
      << " inprocess.factor_decade_round_cap="
      << inprocess.factor_decade_round_cap
      << " inprocess.factor_decade_cap=" << inprocess.factor_decade_cap
      << " inprocess.factor_decade_offset=" << inprocess.factor_decade_offset
      << " inprocess.factor_decade_base=" << inprocess.factor_decade_base
      << " inprocess.factor_active_ceiling=" << inprocess.factor_active_ceiling
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
      << " inprocess.substitute_permille=" << inprocess.substitute_permille
      << " inprocess.sweep_depth=" << inprocess.sweep_depth
      << " inprocess.sweep_max_depth=" << inprocess.sweep_max_depth
      << " inprocess.sweep_vars=" << inprocess.sweep_vars
      << " inprocess.sweep_max_vars=" << inprocess.sweep_max_vars
      << " inprocess.sweep_clauses=" << inprocess.sweep_clauses
      << " inprocess.sweep_max_clauses=" << inprocess.sweep_max_clauses
      << " inprocess.sweep_candidate_pairs="
      << inprocess.sweep_candidate_pairs
      << " inprocess.sweep_flip_rounds=" << inprocess.sweep_flip_rounds
      << " inprocess.sweep_seed_cap=" << inprocess.sweep_seed_cap
      << " inprocess.sweep_environment_ticks="
      << inprocess.sweep_environment_ticks
      << " inprocess.sweep_strip_root_false="
      << inprocess.sweep_strip_root_false
      << " inprocess.sweep_full_partition="
      << inprocess.sweep_full_partition
      << " inprocess.sweep_skip_embedded_fixed="
      << inprocess.sweep_skip_embedded_fixed
      << " inprocess.sweep_resume=" << inprocess.sweep_resume
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
      << " inprocess.forward_midround_budget="
      << inprocess.forward_midround_budget
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

uint32_t DefaultPolicy::want_bump_reasons(const Kernel &k) const {
  if (!bump_reasons_limit_)
    return 0;
  if (bump_reasons_delay_.count)
    return 0;
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
  // Kissat initializes Reduce from `reduceinit` independently of the cadence
  // used after a completed reduction (kimits.c:kissat_init_limits).  The
  // arithmetic interval is a recurrence delta, not an alternative first
  // deadline: using it here postponed LegacySearch's first Reduce from
  // conflict 1000 to 2000.
  reduce.next_conflicts = conflicts + config_.reduction.initial_interval;
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
  // kimits.c calls `kissat_logn(C + 1)`, while kissat_logn(count) computes
  // log10(count + 9).  Preserve both additions: the scale is therefore based
  // on log10(C + 10), not log10(C + 1).  The distinction crosses an integer
  // deadline at common states (for example 22,575 clauses and probe round 2).
  const double f =
      std::log10(static_cast<double>(kernel_irredundant_clauses(k)) + 1.0 +
                 config.logarithm_offset) -
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

#ifdef TACTIX_ENABLE_RULE_JOURNAL
struct RuleJournalMeterScope {
  explicit RuleJournalMeterScope(RuleJournalMeter meter) {
    rule_journal_set_effort_meter(meter);
  }
  ~RuleJournalMeterScope() { rule_journal_set_effort_meter(nullptr); }
  RuleJournalMeterScope(const RuleJournalMeterScope &) = delete;
  RuleJournalMeterScope &operator=(const RuleJournalMeterScope &) = delete;
};
#endif

struct InprocessScope {
  Kernel &k;
  bool previous_inprocessing;
  bool previous_phase_saving_suppressed;
  explicit InprocessScope(Kernel &kk)
      : k(kk), previous_inprocessing(kk.inprocessing),
        previous_phase_saving_suppressed(kk.phase_saving_suppressed) {
    k.inprocessing = true;
    // Kissat keeps `solver->probing` set across the complete Probe pipeline.
    // All speculative and learned assignments in that scope therefore leave
    // `phases.saved` untouched.  Individual Tactix rules may use different
    // propagation engines, so the strategy scope owns the equivalent wire.
    k.phase_saving_suppressed = true;
  }
  ~InprocessScope() {
    k.phase_saving_suppressed = previous_phase_saving_suppressed;
    k.inprocessing = previous_inprocessing;
  }
  InprocessScope(const InprocessScope &) = delete;
  InprocessScope &operator=(const InprocessScope &) = delete;
};

// Kissat's `search_ticks` is charged only by the ordinary CDCL search
// propagator.  Startup propagation, both Lucky trials, sparse root collection,
// and `probe_initially` all run before search and therefore must not seed mode
// or inprocessing effort references.  This scope suppresses only that tick
// currency; unlike `InprocessScope` it deliberately leaves assignment-time
// phase saving unchanged outside the actual Probe pipeline.
struct PreSearchTickScope {
  Kernel &k;
  bool previous_inprocessing;
  explicit PreSearchTickScope(Kernel &kk)
      : k(kk), previous_inprocessing(kk.inprocessing) {
    k.inprocessing = true;
  }
  ~PreSearchTickScope() { k.inprocessing = previous_inprocessing; }
  PreSearchTickScope(const PreSearchTickScope &) = delete;
  PreSearchTickScope &operator=(const PreSearchTickScope &) = delete;
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
  return k.transitive_budget_checked ? k.stats.transitive_checked
                                     : k.stats.transitive_ticks;
}
uint64_t meter_factor(const Kernel &k) { return k.stats.factor_ticks; }
uint64_t meter_ternary(const Kernel &k) { return k.stats.ternary_ticks; }
uint64_t meter_hbr(const Kernel &k) { return k.stats.hbr_ticks; }
uint64_t meter_bce(const Kernel &k) { return k.stats.bce_ticks; }
uint64_t meter_cce(const Kernel &k) { return k.stats.cce_ticks; }
uint64_t meter_sweep(const Kernel &k) { return sweep_effort(k); }
#ifdef TACTIX_ENABLE_RULE_JOURNAL
// Congruence has no shipped effort ceiling in Kissat.  Keep its inclusive
// logical counter available to compile-time rule journals without placing it
// back in EFFORT_SPECS (which would make the meter behavior-affecting).
uint64_t meter_congruence(const Kernel &k) { return k.stats.congruence_ticks; }
#endif
uint64_t meter_substitute(const Kernel &k) { return k.stats.substitute_ticks; }

SweepLimits configured_sweep_limits(
    const DefaultStrategyConfig::Inprocess &config) {
  SweepLimits lim;
  lim.max_depth = config.sweep_depth;
  lim.max_depth_ceiling = config.sweep_max_depth;
  lim.max_env_vars = config.sweep_vars;
  lim.max_env_vars_ceiling = config.sweep_max_vars;
  lim.max_env_clauses = config.sweep_clauses;
  lim.max_env_clauses_ceiling = config.sweep_max_clauses;
  lim.max_candidates = config.sweep_candidate_pairs;
  lim.flip_rounds = config.sweep_flip_rounds;
  lim.max_seeds = config.sweep_seed_cap;
  lim.env_ticks = config.sweep_environment_ticks;
  lim.strip_false = config.sweep_strip_root_false;
  lim.full_partition = config.sweep_full_partition;
  lim.skip_embedded_fixed = config.sweep_skip_embedded_fixed;
  lim.resume = config.sweep_resume;
  return lim;
}

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
    {"Sweep", [](const Kernel &k) {
       return k.stats.sweep_equivalences + k.stats.sweep_units;
     },
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
     meter_backbone, false, true, &Stats::backbone_ticks},
    {"TransitiveReduce", &DefaultStrategyConfig::Inprocess::transitive_permille,
     meter_transitive, true, true},
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
    {"Substitute", &DefaultStrategyConfig::Inprocess::substitute_permille,
     meter_substitute, false, true},
};

const EffortSpec *effort_spec(const char *name) {
  for (const EffortSpec &s : EFFORT_SPECS)
    if (!std::strcmp(s.name, name))
      return &s;
  return nullptr; // all-or-nothing rule: no titration, no meter
}

#ifdef TACTIX_ENABLE_RULE_JOURNAL
const char *effort_currency(const char *name) {
  if (!std::strcmp(name, "BVE"))
    return "resolutions";
  if (!std::strcmp(name, "Strengthen") ||
      !std::strcmp(name, "SubsumeForward"))
    return "forward-plus-strengthen-visits";
  if (!std::strcmp(name, "Sweep"))
    return "sweep-scheduled-effort";
  if (!std::strcmp(name, "TransitiveReduce"))
    return "transitive-cache-lines";
  if (!std::strcmp(name, "Congruence"))
    return "congruence-ticks";
  if (!std::strcmp(name, "Substitute"))
    return "substitute-cache-lines";
  return "probing-rule-ticks";
}

const char *journal_result(ApplyResult result) {
  switch (result) {
  case ApplyResult::Ok:
    return "applied";
  case ApplyResult::NoOp:
    return "no-op";
  case ApplyResult::Conflict:
    return "conflict";
  case ApplyResult::Sat:
    return "sat";
  case ApplyResult::Unsat:
    return "refuted";
  }
  return "unknown";
}
#endif

template <class Apply>
ApplyResult metered_rule_apply(Kernel &k,
                               uint64_t (*meter)(const Kernel &),
                               Apply &&apply) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  RuleJournalMeterScope journal_meter(meter);
#else
  (void)meter;
#endif
  (void)k;
  return apply();
}

// The meter the compile-time journal prices an entry in.  For every metered
// rule this is the ledger meter; Congruence has none, so the journal alone
// reads its inclusive counter.  Scheduling never consults this function, so
// production binaries see exactly `e.meter`.
uint64_t (*journal_effort_meter(
    const DefaultInprocessScheduler::SlotEntry &e,
    const RuleVTable &r))(const Kernel &) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  if (!e.meter && !std::strcmp(r.name, "Congruence"))
    return meter_congruence;
#else
  (void)r;
#endif
  return e.meter;
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

uint64_t order_key(const Kernel &k, CandidateOrder arm, uint64_t inst,
                   bool transitive_reference_order,
                   const std::vector<Var> *reference_compact_order,
                   const std::vector<Var> *reference_compact_position) {
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
  case CandidateOrder::Heuristic: {
    // transitive.c schedules variable indices but its comparator applies IDX
    // once more before consulting activity.  Keep that literal reference
    // behavior local to TransitiveReduce; other users of the generic arm rank
    // the candidate's own variable as expected.
    Var proxy = transitive_reference_order ? v >> 1 : v;
    if (transitive_reference_order && reference_compact_order &&
        reference_compact_position &&
        v < reference_compact_position->size()) {
      const Var compact = (*reference_compact_position)[v];
      if (compact != INVALID_VAR &&
          (compact >> 1) < reference_compact_order->size())
        proxy = (*reference_compact_order)[compact >> 1];
    }
    if (k.h->stable) {
      const double s = k.h->scores.score[proxy];
      uint64_t bits = 0;
      std::memcpy(&bits, &s, sizeof bits);
      return bits;
    }
    return k.h->links[proxy].stamp;
  }
  default:
    return 0;
  }
}

template <class Less>
void kissat_reference_sort(std::vector<Var> &values, Less less) {
  const size_t n = values.size();
  if (n <= 1)
    return;

  auto greater_swap = [&](size_t p, size_t q) {
    if (less(values[q], values[p]))
      std::swap(values[p], values[q]);
  };

  constexpr size_t quick_sort_limit = 10;
  size_t left = 0, right = n - 1;
  std::vector<std::pair<size_t, size_t>> pending;
  if (right - left > quick_sort_limit) {
    for (;;) {
      const size_t middle = left + (right - left) / 2;
      std::swap(values[middle], values[right - 1]);
      greater_swap(left, right - 1);
      greater_swap(left, right);
      greater_swap(right - 1, right);

      size_t i = left;
      size_t j = right - 1;
      const Var pivot = values[j];
      for (;;) {
        while (less(values[++i], pivot))
          ;
        while (less(pivot, values[--j]))
          if (j == left)
            break;
        if (i >= j)
          break;
        std::swap(values[i], values[j]);
      }
      std::swap(values[i], values[right - 1]);

      size_t other_left, other_right;
      if (i - left < right - i) {
        other_left = i + 1;
        other_right = right;
        right = i - 1;
      } else {
        other_left = left;
        other_right = i - 1;
        left = i + 1;
      }
      if (right - left > quick_sort_limit) {
        assert(other_right - other_left > quick_sort_limit);
        pending.emplace_back(other_left, other_right);
      } else if (other_right - other_left > quick_sort_limit) {
        left = other_left;
        right = other_right;
      } else if (!pending.empty()) {
        left = pending.back().first;
        right = pending.back().second;
        pending.pop_back();
      } else {
        break;
      }
    }
  }

  // Kissat finishes the whole array with the same sentinel-seeded insertion
  // sort after quicksort partitions it (sort.h:83-104).
  for (size_t i = n - 1; i > 0; i--)
    greater_swap(i - 1, i);
  for (size_t i = 2; i < n; i++) {
    const Var pivot = values[i];
    size_t j = i;
    while (less(pivot, values[j - 1])) {
      values[j] = values[j - 1];
      j--;
    }
    values[j] = pivot;
  }
}

bool rank_transitive_reference(const Kernel &k, uint64_t *insts,
                               uint32_t n) {
  if (!k.h || k.compact_import_order.empty())
    return false;
  std::vector<Var> compact_position(k.nvars, INVALID_VAR);
  for (Var compact = 0; compact < k.compact_import_order.size(); compact++)
    compact_position[k.compact_import_order[compact]] = compact;

  std::vector<uint64_t> positive(k.nvars, UINT64_MAX);
  std::vector<uint64_t> negative(k.nvars, UINT64_MAX);
  std::vector<Var> compact_candidates;
  std::vector<uint8_t> seen(k.compact_import_order.size(), 0);
  for (uint32_t i = 0; i < n; i++) {
    Lit lit;
    if (!lit_of_instance(k, insts[i], &lit) || var_of(lit) >= k.nvars)
      return false;
    const Var v = var_of(lit);
    const Var compact = compact_position[v];
    if (compact == INVALID_VAR)
      return false;
    (negated(lit) ? negative[v] : positive[v]) = insts[i];
    if (!seen[compact]) {
      seen[compact] = 1;
      compact_candidates.push_back(compact);
    }
  }
  // Kissat's `all_variables` seeds the stack in current compact-index order.
  std::sort(compact_candidates.begin(), compact_candidates.end());
  auto less = [&](Var a, Var b) {
    const Var i = a >> 1, j = b >> 1; // transitive.c's second IDX
    const Var iv = k.compact_import_order[i], jv = k.compact_import_order[j];
    if (k.h->stable) {
      const double s = k.h->scores.score[iv];
      const double t = k.h->scores.score[jv];
      if (s < t)
        return true;
      if (s > t)
        return false;
      return i < j;
    }
    return k.h->links[iv].stamp < k.h->links[jv].stamp;
  };
  kissat_reference_sort(compact_candidates, less);

  uint32_t out = 0;
  for (auto it = compact_candidates.rbegin(); it != compact_candidates.rend();
       ++it) {
    const Var v = k.compact_import_order[*it];
    if (positive[v] != UINT64_MAX)
      insts[out++] = positive[v];
    if (negative[v] != UINT64_MAX)
      insts[out++] = negative[v];
  }
  assert(out == n);
  return true;
}

// Kissat's elimination schedule is a mutable indexed max-heap.  It stores the
// negated elimination score, inserts candidates in compact-variable order,
// stops bubble-up at equality, and prefers the left child at equality while
// bubbling down.  Those topology rules are semantic scheduling state: after
// one candidate mutates several occurrence lists, a global re-sort can choose
// a different equal-score variable even when every scalar score is correct.
class BveCandidateHeap {
public:
  void initialize(Kernel &k, const uint64_t *insts, uint32_t n) {
    kernel_ = &k;
    assert(k.dense && !k.bve_occurrence_count_image);
    // Kissat eliminate.c::variable_score reads SIZE_WATCHES for each polarity:
    // one physical dense-stack length containing binary and large entries.
    // Tactix stores those populations separately, so materialize their exact
    // sum once and let the kernel mirror subsequent physical mutations.
    occurrence_counts_.assign(2 * static_cast<size_t>(k.nvars), 0);
    for (Lit lit = 0; lit < 2 * k.nvars; ++lit) {
      uint64_t count = k.dense->large[lit].size();
      for (const WatchEntry watch : k.watches.entries(lit))
        count += watch.binary();
      occurrence_counts_[lit] =
          static_cast<uint32_t>(std::min<uint64_t>(count, UINT32_MAX));
    }
    k.bve_occurrence_count_image = &occurrence_counts_;
    stack_.clear();
    pos_.assign(k.nvars, UINT32_MAX);
    score_.assign(k.nvars, 0.0);
    payload_.assign(k.nvars, INSTANCE_CANONICAL);
    eligible_.assign(k.nvars, 0);
    std::vector<uint8_t> inserted(k.nvars, 0);
    for (uint32_t i = 0; i < n; i++) {
      Lit lit;
      if (!lit_of_instance(k, insts[i], &lit) || var_of(lit) >= k.nvars)
        continue;
      const Var v = var_of(lit);
      eligible_[v] = 1;
      payload_[v] = insts[i];
    }
    // `all_variables` in Kissat walks current compact internal indices.
    for (const Var v : k.compact_import_order)
      if (v < k.nvars && eligible_[v]) {
        insert_initial(v);
        inserted[v] = 1;
      }
    // Stable identities normally all occur in compact_import_order.  Preserve
    // a deterministic complete fallback for bare API kernels that have no H.
    for (Var v = 0; v < k.nvars; v++)
      if (eligible_[v] && !inserted[v])
        insert_initial(v);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    if (const char *selected = std::getenv("TACTIX_EVENT_TRACE_BVE_HEAP")) {
      const char *p = selected;
      while (*p) {
        char *end = nullptr;
        const long external = std::strtol(p, &end, 10);
        if (end == p)
          break;
        for (Var v = 0; v < k.nvars; v++)
          if (static_cast<long>(k.internal_to_external[v] + 1) == external) {
            trace_vars_.push_back(v);
            break;
          }
        p = *end ? end + 1 : end;
      }
    }
#endif
    trace_last_pos_.assign(trace_vars_.size(), UINT32_MAX);
    trace_last_score_.assign(trace_vars_.size(), 0.0);
    trace_selected("initial", INVALID_VAR, true);
  }

  bool empty() const { return stack_.empty(); }

  uint64_t pop_max() {
    assert(!stack_.empty());
    const Var result = stack_[0];
    const Var last = stack_.back();
    stack_.pop_back();
    pos_[result] = UINT32_MAX;
    if (!stack_.empty()) {
      stack_[0] = last;
      pos_[last] = 0;
      bubble_down(last);
    }
    trace_selected("pop", result, false);
    return payload_[result];
  }

  static void update_hook(Kernel &, void *state, Var v, bool reinsert) {
    static_cast<BveCandidateHeap *>(state)->update(v, reinsert);
  }

private:
  bool contains(Var v) const {
    return v < pos_.size() && pos_[v] != UINT32_MAX;
  }

  bool active(Var v) const {
    const Kernel &k = *kernel_;
    return v < k.nvars && k.values[make_lit(v, false)] == 0 &&
           !kernel_var_retired(k, v);
  }

  double schedule_score(Var v) const {
    const Kernel &k = *kernel_;
#ifndef NDEBUG
    for (const Lit lit : {make_lit(v, false), make_lit(v, true)}) {
      uint64_t physical = k.dense->large[lit].size();
      for (const WatchEntry watch : k.watches.entries(lit))
        physical += watch.binary();
      assert(physical == occurrence_counts_[lit]);
    }
#endif
    const double relevance =
        k.h ? (k.h->stable ? k.h->scores.score[v] : k.h->links[v].stamp) : 0;
    const double reference = kissat_bve_variable_score(
        relevance, bve_schedule_occurrence_count(k, make_lit(v, false)),
        bve_schedule_occurrence_count(k, make_lit(v, true)),
        k.bve_occurrence_limit);
    return -reference; // Kissat stores the negated score in a max-heap.
  }

  void insert_initial(Var v) {
    const Kernel &k = *kernel_;
    if (!active(v) || v >= k.bve_retry.size() || !k.bve_retry[v])
      return;
    score_[v] = schedule_score(v);
    pos_[v] = static_cast<uint32_t>(stack_.size());
    stack_.push_back(v);
    bubble_up(v);
  }

  void update(Var v, bool reinsert) {
    if (v >= eligible_.size() || !eligible_[v] || !active(v))
      return;
    const double old = score_[v];
    const double now = schedule_score(v);
    score_[v] = now;
    if (contains(v)) {
      if (now > old)
        bubble_up(v);
      else if (now < old)
        bubble_down(v);
    } else if (reinsert && v < kernel_->bve_retry.size() &&
               kernel_->bve_retry[v]) {
      pos_[v] = static_cast<uint32_t>(stack_.size());
      stack_.push_back(v);
      bubble_up(v);
    }
    trace_selected(reinsert ? "update-reinsert" : "update-add", v, false);
  }

  void trace_selected(const char *stage, Var trigger, bool force) {
    const Var root = stack_.empty() ? INVALID_VAR : stack_[0];
    for (size_t i = 0; i < trace_vars_.size(); i++) {
      const Var v = trace_vars_[i];
      const uint32_t position = contains(v) ? pos_[v] : UINT32_MAX;
      const double score = score_[v];
      if (!force && position == trace_last_pos_[i] &&
          score == trace_last_score_[i])
        continue;
      event_trace_bve_heap(*kernel_, stage, trigger, v, score, position, root,
                           static_cast<uint32_t>(stack_.size()));
      trace_last_pos_[i] = position;
      trace_last_score_[i] = score;
    }
  }

  void bubble_up(Var v) {
    uint32_t at = pos_[v];
    while (at) {
      const uint32_t parent_at = (at - 1) / 2;
      const Var parent = stack_[parent_at];
      if (score_[parent] >= score_[v])
        break;
      stack_[at] = parent;
      pos_[parent] = at;
      at = parent_at;
    }
    stack_[at] = v;
    pos_[v] = at;
  }

  void bubble_down(Var v) {
    uint32_t at = pos_[v];
    for (;;) {
      uint32_t child_at = 2 * at + 1;
      if (child_at >= stack_.size())
        break;
      Var child = stack_[child_at];
      const uint32_t sibling_at = child_at + 1;
      if (sibling_at < stack_.size() &&
          score_[stack_[sibling_at]] > score_[child]) {
        child_at = sibling_at;
        child = stack_[child_at];
      }
      if (score_[child] <= score_[v])
        break;
      stack_[at] = child;
      pos_[child] = at;
      at = child_at;
    }
    stack_[at] = v;
    pos_[v] = at;
  }

  Kernel *kernel_ = nullptr;
  std::vector<Var> stack_;
  std::vector<uint32_t> pos_;
  std::vector<uint32_t> occurrence_counts_;
  std::vector<double> score_;
  std::vector<uint64_t> payload_;
  std::vector<uint8_t> eligible_;
  std::vector<Var> trace_vars_;
  std::vector<uint32_t> trace_last_pos_;
  std::vector<double> trace_last_score_;
};

bool rank_bve_reference(const Kernel &k, uint64_t *insts, uint32_t n) {
  if (!k.h)
    return false;
  BveOccurrences occ;
  bve_build_occurrences(k, occ);
  std::vector<std::pair<double, uint64_t>> ranked;
  ranked.reserve(n);
  const uint64_t occurrence_limit = k.bve_occurrence_limit;
  for (uint32_t i = 0; i < n; i++) {
    Lit lit;
    if (!lit_of_instance(k, insts[i], &lit) || var_of(lit) >= k.nvars)
      return false;
    const Var v = var_of(lit);
    const double relevance =
        k.h->stable ? k.h->scores.score[v] : k.h->links[v].stamp;
    const double score = kissat_bve_variable_score(
        relevance, occ.count(make_lit(v, false)),
        occ.count(make_lit(v, true)), occurrence_limit);
    ranked.emplace_back(score, insts[i]);
  }
  std::stable_sort(ranked.begin(), ranked.end(),
                   [](const auto &a, const auto &b) {
                     return a.first < b.first;
                   });
  for (uint32_t i = 0; i < n; i++)
    insts[i] = ranked[i].second;
  return true;
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
  const std::vector<RuleVTable> &rules = tactix_rules();
  const bool transitive_reference_order =
      arm == CandidateOrder::Heuristic && rule < rules.size() &&
      !std::strcmp(rules[rule].name, "TransitiveReduce");
  const bool bve_reference_order =
      arm == CandidateOrder::Heuristic && rule < rules.size() &&
      !std::strcmp(rules[rule].name, "BVE");
  if (transitive_reference_order && rank_transitive_reference(k, insts, n))
    return;
  if (bve_reference_order && rank_bve_reference(k, insts, n))
    return;
  std::vector<Var> compact_position;
  const bool has_reference_compact_order =
      transitive_reference_order && k.h && !k.compact_import_order.empty();
  if (has_reference_compact_order) {
    compact_position.assign(k.nvars, INVALID_VAR);
    for (Var i = 0; i < k.compact_import_order.size(); i++)
      compact_position[k.compact_import_order[i]] = i;
  }
  scratch_.resize(n);
  for (uint32_t i = 0; i < n; i++)
    scratch_[i] = {order_key(k, arm, insts[i], transitive_reference_order,
                                has_reference_compact_order
                                    ? &k.compact_import_order
                                    : nullptr,
                                has_reference_compact_order ? &compact_position
                                                            : nullptr),
                   insts[i]};
  std::stable_sort(
      scratch_.begin(), scratch_.end(),
      [transitive_reference_order, has_reference_compact_order,
       &compact_position](
          const std::pair<uint64_t, uint64_t> &a,
          const std::pair<uint64_t, uint64_t> &b) {
        if (a.first != b.first)
          return transitive_reference_order ? a.first > b.first
                                            : a.first < b.first;
        if (transitive_reference_order) {
          Lit al, bl;
          if (lit_of_instance(a.second, &al) &&
              lit_of_instance(b.second, &bl)) {
            const Var av = var_of(al), bv = var_of(bl);
            const Var ac = has_reference_compact_order &&
                                   av < compact_position.size() &&
                                   compact_position[av] != INVALID_VAR
                               ? compact_position[av]
                               : av;
            const Var bc = has_reference_compact_order &&
                                   bv < compact_position.size() &&
                                   compact_position[bv] != INVALID_VAR
                               ? compact_position[bv]
                               : bv;
            // The second IDX maps each adjacent pair of compact candidate
            // variables to one activity owner.  Kissat's sort/pop pose consumes
            // the lower compact candidate first inside that equal-key pair;
            // See benchmarks/reports/2026-08-21-tierd-rank27-transitive-reference-order.md.
            if ((ac >> 1) != (bc >> 1))
              return (ac >> 1) < (bc >> 1);
            if (ac != bc)
              return ac < bc;
          }
        }
        return false;
      });
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
  if (preprocess_only_ && k.stats.conflicts)
    return false;
  if (k.stats.conflicts == 0 && g == SlotGroup::Eliminate)
    return false;
  SlotClock &c = g == SlotGroup::Probe ? probe_clock : elim_clock;
  // Kissat's two guards deliberately differ.  Probe tests
  // `limit < conflicts` (probe.c:kissat_probing), while Eliminate declines
  // only for `limit > conflicts` (eliminate.c:kissat_eliminating) and is due
  // at equality.  The round-zero probe clock is the explicit startup-
  // preprocessing invitation, not such a deadline, and remains due at zero.
  const bool startup_probe =
      g == SlotGroup::Probe && c.rounds == 0 &&
      k.stats.conflicts == c.next_conflicts;
  const bool before_deadline =
      g == SlotGroup::Probe ? k.stats.conflicts <= c.next_conflicts
                            : k.stats.conflicts < c.next_conflicts;
  if (!startup_probe && before_deadline)
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
      k.tier1 = k.tier1_by_mode[checkpoint.mode];
      k.tier2 = std::max(k.tier1, k.tier2_by_mode[checkpoint.mode]);
      if (retier_.interval < config_.reduction.retier_max_interval)
        retier_.interval *= config_.reduction.retier_growth;
      retier_.next_conflicts = checkpoint.conflicts + retier_.interval;
      event_trace_retier(k, checkpoint.mode, checkpoint.used_glue);
      updated = true;
    }
  k.analysis_conflict_checkpoints.clear();
  return updated;
}

void DefaultPolicy::update_formula_classification(Kernel &k) const {
  uint64_t binary_words = 0;
  for (Lit lit = 0; lit < 2 * k.nvars; lit++)
    for (const WatchEntry w : k.watches.entries(lit))
      binary_words += w.binary();
  assert(!(binary_words & 1));
  const uint64_t binaries = binary_words / 2;
  uint64_t large = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage() && !clause->redundant())
      large++;
    ref += clause_actual_words(clause);
  }
  const uint64_t clauses = binaries + large;
  const auto &cfg = config_.analysis;
  bool bigbig = false;
  if (clauses > cfg.small_clause_limit && clauses) {
    // `kissat_percent` returns 0..100 while bigbigfraction is per mille:
    // 100*binaries/clauses >= fraction/1000.
    const unsigned __int128 lhs =
        static_cast<unsigned __int128>(100000) * binaries;
    const unsigned __int128 rhs =
        static_cast<unsigned __int128>(cfg.big_binary_fraction_permille) *
        clauses;
    bigbig = lhs >= rhs;
  }
  k.jump_reasons_bigbig = bigbig;
  k.stats.jump_reason_classifications++;
  k.stats.jump_reason_bigbig += bigbig;
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
  assert(k.unassigned > 0);
  if (!k.warming && randec_enabled && !k.h->stable) {
    const uint64_t c = k.stats.conflicts;
    event_trace_randec_state(k, "pre", randec_remaining, randec_next,
                             randec_sequences);
    // Kissat increments `solver->level` before `next_random_decision` tests
    // whether that level is at most one.  Tactix picks before Decide creates
    // the new frame, so the equivalent pre-decision condition is level zero.
    // In particular, a restart which reuses level one must not start a due
    // random sequence for its first post-restart decision.
    // Stronger compact-universe diversification: when the original branching
    // universe fits within one reference random-sequence interval and no
    // extension variables have entered it, a due sequence may replace the
    // first ranked branch after a restart retained level one.  Larger or
    // extended universes keep Kissat's faithful pre-decision root boundary.
    const uint64_t compact_universe_limit =
        config_ ? config_->random_initial_interval : RANDEC_INIT;
    const bool random_sequence_level =
        k.level == 0 ||
        (k.level == 1 && k.first_extension_var == UINT32_MAX &&
         kernel_num_original_internal_vars(k) <= compact_universe_limit);
    if (!randec_remaining && random_sequence_level && c >= randec_next)
      start_random_sequence(k);
    event_trace_randec_state(k, randec_remaining ? "active" : "inactive",
                             randec_remaining, randec_next, randec_sequences);
    if (randec_remaining) {
      for (;;) {
        randec_rng = randec_rng * RANDEC_MULTIPLIER + RANDEC_INCREMENT;
        const uint32_t random = static_cast<uint32_t>(randec_rng >> 32);
        assert(!k.compact_import_order.empty());
        const uint32_t slot = random % k.compact_import_order.size();
        const Var v = k.compact_import_order[slot];
        const bool accepted = k.values[make_lit(v, true)] == 0 &&
                              !kernel_var_retired(k, v);
        event_trace_randec_draw(k, random, slot, v, accepted);
        if (accepted) {
          k.stats.random_decisions++;
          return decide_lit(k, v);
        }
      }
    }
  }
  const Var v = heuristics_next_active_decision_var(k);
  return decide_lit(k, v);
}

Lit DefaultBranching::decide_lit(const Kernel &k, Var v) const {
  static const DefaultStrategyConfig defaults;
  const auto &cfg = config_ ? *config_ : defaults.branching;
  int8_t phase = 0;
  if (!k.h->stable && cfg.focused_phase_switch_span &&
      !cfg.focused_phase_pattern.empty()) {
    const size_t index =
        (k.stats.switched / cfg.focused_phase_switch_span) %
        cfg.focused_phase_pattern.size();
    phase = cfg.focused_phase_pattern[index];
  }
  const int8_t target = k.h->stable ? k.h->target[v] : 0;
  const int8_t saved = k.h->saved[v];
  const Lit result =
      phase ? make_lit(v, phase < 0) : heuristics_decide_lit(k, v);
  event_trace_phase_pick(k, v, target, saved,
                         negated(result) ? -1 : 1);
  return result;
}

void DefaultBranching::start_random_sequence(Kernel &k) {
  if (!randec_enabled || k.h->stable || randec_remaining)
    return;
  static const DefaultStrategyConfig defaults;
  const auto &cfg = config_ ? *config_ : defaults.branching;
  randec_sequences++;
  const double n = static_cast<double>(randec_sequences);
  const double logn = std::log10(n + cfg.logarithm_offset);
  randec_remaining =
      static_cast<uint64_t>(cfg.random_sequence_length * logn);
  randec_next =
      k.stats.conflicts +
      static_cast<uint64_t>(cfg.random_initial_interval * logn);
  k.stats.random_sequences++;
}

void DefaultPolicy::classify_formula_strategy(const Kernel &k) {
  enum class FormulaProfile {
    Reference,
    StartupFactorOnly,
    Dose5,
    Dose5Vivify5,
    Factor5,
    NoBve,
    NoBveLight,
  };

  const uint64_t vars = kernel_num_output_vars(k);
  const uint64_t clauses = k.original_input_clauses;
  if (!vars || clauses == UINT64_MAX)
    return;

  FormulaProfile profile = FormulaProfile::Reference;
  // Large dense formulas retain the measured lighter repeated-Factor dose,
  // but classification may not suppress the complete search-time Probe
  // pipeline.  Kissat admits that pipeline from its conflict clock on every
  // formula; per-entry budgets and productivity delays own its cost.
  const bool grain_shape =
      vars >= DefaultStrategyConfig::PROFILE_GRAIN_MIN_VARS &&
      vars < DefaultStrategyConfig::PROFILE_GRAIN_MAX_VARS &&
      clauses >= DefaultStrategyConfig::PROFILE_GRAIN_MIN_CLAUSES;

  const auto at_least = [vars, clauses](uint64_t ratio) {
    return clauses >= ratio * vars;
  };
  // Classification may dose mechanisms, but never withdraw Vivify: Kissat's
  // Probe pipeline admits it on every formula and its effort/productivity
  // controls decide how much work is performed.
  if (grain_shape &&
      vars >= DefaultStrategyConfig::PROFILE_GRAIN_LEGACY_MIN_VARS)
    profile = FormulaProfile::StartupFactorOnly;
  else if (vars >= DefaultStrategyConfig::PROFILE_VARS_900 &&
           vars < DefaultStrategyConfig::PROFILE_VARS_1000 &&
           clauses < DefaultStrategyConfig::PROFILE_DENSITY_SPARSE * vars)
    profile = FormulaProfile::Reference;
  else if (vars >= DefaultStrategyConfig::PROFILE_PTN_MIN_VARS &&
      vars < DefaultStrategyConfig::PROFILE_PTN_MAX_VARS &&
      clauses < DefaultStrategyConfig::PROFILE_LIGHT_PERMILLE * vars)
    profile = FormulaProfile::StartupFactorOnly;
  else if (vars >= DefaultStrategyConfig::PROFILE_VARS_6000 &&
           vars < DefaultStrategyConfig::PROFILE_VARS_7000 &&
           clauses < DefaultStrategyConfig::PROFILE_LIGHT_PERMILLE * vars)
    profile = FormulaProfile::StartupFactorOnly;
  else if (vars >= DefaultStrategyConfig::PROFILE_VARS_10000 &&
           vars < DefaultStrategyConfig::PROFILE_VARS_15000 &&
           clauses < DefaultStrategyConfig::PROFILE_DENSITY_CONTEXT_MAX * vars)
    profile = FormulaProfile::Reference;
  else if (vars < DefaultStrategyConfig::PROFILE_TINY_MAX_VARS)
    profile = FormulaProfile::Dose5;
  else if (at_least(DefaultStrategyConfig::PROFILE_DENSITY_EXTREME))
    profile = vars < DefaultStrategyConfig::PROFILE_VARS_1000
                  ? FormulaProfile::NoBveLight
                  : FormulaProfile::NoBve;
  else if (at_least(DefaultStrategyConfig::PROFILE_DENSITY_VERY_HIGH))
    profile = FormulaProfile::StartupFactorOnly;
  else if (at_least(DefaultStrategyConfig::PROFILE_DENSITY_HIGH))
    profile = FormulaProfile::Reference;
  else if (at_least(DefaultStrategyConfig::PROFILE_DENSITY_MEDIUM)) {
    if (vars >= DefaultStrategyConfig::PROFILE_VARS_800 &&
        vars < DefaultStrategyConfig::PROFILE_VARS_900)
      profile = FormulaProfile::StartupFactorOnly;
    else if (vars < DefaultStrategyConfig::PROFILE_VARS_900)
      profile = FormulaProfile::StartupFactorOnly;
    else if (vars < DefaultStrategyConfig::PROFILE_VARS_1000 &&
             at_least(DefaultStrategyConfig::PROFILE_DENSITY_FACTOR_ONLY))
      profile = FormulaProfile::StartupFactorOnly;
    else if (vars < DefaultStrategyConfig::PROFILE_VARS_1500)
      profile = FormulaProfile::Dose5;
    else
      profile = FormulaProfile::StartupFactorOnly;
  } else if (at_least(DefaultStrategyConfig::PROFILE_DENSITY_LOW)) {
    if (vars < DefaultStrategyConfig::PROFILE_VARS_300)
      profile = FormulaProfile::NoBve;
    else if (vars < DefaultStrategyConfig::PROFILE_VARS_1000)
      profile = FormulaProfile::StartupFactorOnly;
    else if (vars < DefaultStrategyConfig::PROFILE_VARS_4000)
      profile = FormulaProfile::StartupFactorOnly;
    else
      profile = FormulaProfile::Dose5Vivify5;
  } else if (at_least(DefaultStrategyConfig::PROFILE_DENSITY_SPARSE)) {
    if (vars < DefaultStrategyConfig::PROFILE_VARS_500)
      profile = FormulaProfile::NoBve;
    else if (vars < DefaultStrategyConfig::PROFILE_VARS_5000)
      profile = FormulaProfile::Factor5;
    else if (vars < DefaultStrategyConfig::PROFILE_VARS_10000)
      profile = FormulaProfile::StartupFactorOnly;
    else if (vars >= DefaultStrategyConfig::PROFILE_VARS_20000 &&
             vars < DefaultStrategyConfig::PROFILE_VARS_25000)
      profile = FormulaProfile::StartupFactorOnly;
    else
      profile = FormulaProfile::Dose5Vivify5;
  } else if (vars < DefaultStrategyConfig::PROFILE_VARS_500)
    profile = FormulaProfile::Dose5Vivify5;
  else if (vars < DefaultStrategyConfig::PROFILE_VARS_2000)
    profile = FormulaProfile::Dose5Vivify5;
  else if (vars < DefaultStrategyConfig::PROFILE_VARS_6500)
    profile = FormulaProfile::Reference;
  else if (vars < DefaultStrategyConfig::PROFILE_VARS_10000)
    profile = FormulaProfile::Factor5;
  else if (vars < DefaultStrategyConfig::PROFILE_VARS_25000)
    profile = FormulaProfile::StartupFactorOnly;
  else if (vars < DefaultStrategyConfig::PROFILE_VARS_32000)
    profile = FormulaProfile::Factor5;
  else if (vars < DefaultStrategyConfig::PROFILE_VARS_40000)
    profile = FormulaProfile::Dose5;
  else if (vars < DefaultStrategyConfig::PROFILE_VARS_50000)
      profile = FormulaProfile::StartupFactorOnly;
  else if (vars < DefaultStrategyConfig::PROFILE_VARS_70000)
    profile = clauses < DefaultStrategyConfig::PROFILE_DENSITY_VERY_SPARSE *
                            vars
                  ? FormulaProfile::Reference
                  : FormulaProfile::StartupFactorOnly;
  else if (vars < DefaultStrategyConfig::PROFILE_VARS_120000)
    profile = FormulaProfile::StartupFactorOnly;
  else if (vars < DefaultStrategyConfig::PROFILE_VARS_200000)
    profile = FormulaProfile::Reference;
  else if (vars < DefaultStrategyConfig::PROFILE_VARS_300000)
    profile = FormulaProfile::NoBve;
  else if (vars < DefaultStrategyConfig::PROFILE_VARS_800000)
    profile = vars < DefaultStrategyConfig::PROFILE_VARS_500000
                  ? FormulaProfile::StartupFactorOnly
                  : clauses >=
                            DefaultStrategyConfig::PROFILE_DENSITY_VERY_SPARSE *
                                vars
                        ? FormulaProfile::StartupFactorOnly
                        : FormulaProfile::Dose5;

  auto &in = config_.inprocess;
  in.forward_midround_budget =
      at_least(DefaultStrategyConfig::PROFILE_DENSITY_EXTREME);
  // Static formula profiles never withdraw universally admitted Congruence,
  // Vivify, or late Lucky opportunities; they only dose their later work.
  // Source evidence and the diagnostic/N1 surfaces live in
  // docs/implementation-notes.md (`Formula classification`).
  switch (profile) {
  case FormulaProfile::Reference:
    break;
  case FormulaProfile::StartupFactorOnly:
    in.factor_repeated_enabled = false;
    break;
  case FormulaProfile::Dose5:
    in.bve_permille = in.factor_permille = in.subsume_forward_permille =
        DefaultStrategyConfig::PROFILE_LIGHT_PERMILLE;
    break;
  case FormulaProfile::Dose5Vivify5:
    in.bve_permille = in.factor_permille = in.subsume_forward_permille =
        DefaultStrategyConfig::PROFILE_LIGHT_PERMILLE;
    in.vivify_permille = DefaultStrategyConfig::PROFILE_LIGHT_PERMILLE;
    break;
  case FormulaProfile::Factor5:
    in.factor_permille = DefaultStrategyConfig::PROFILE_LIGHT_PERMILLE;
    break;
  case FormulaProfile::NoBve:
    in.bve_enabled = false;
    break;
  case FormulaProfile::NoBveLight:
    in.bve_enabled = false;
    in.vivify_permille = in.subsume_forward_permille =
        DefaultStrategyConfig::PROFILE_LIGHT_PERMILLE;
    break;
  }
}

void DefaultPolicy::configure_from_env(const Kernel &k) {
  config_ = base_config_;
  // Preserve the pre-existing white-box policy pose surface: callers and
  // focused tests may set these public members before solve().
  config_.analysis.chronological_levels = chrono_levels;
  config_.analysis.reuse_ranked_trail_suffix = chrono_reuse_trail_;
  config_.branching.random_sequences = default_branching_.randec_enabled;
  classify_formula_strategy(k);
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
  const auto sweep_u32 = [](const char *name, uint32_t &slot) {
    if (const char *s = std::getenv(name))
      slot = static_cast<uint32_t>(std::strtoul(s, nullptr, 10));
  };
  const auto sweep_bool = [](const char *name, bool &slot) {
    if (const char *s = std::getenv(name))
      slot = std::strtoul(s, nullptr, 10) != 0;
  };
  sweep_u32("TACTIX_SWEEP_DEPTH", config_.inprocess.sweep_depth);
  sweep_u32("TACTIX_SWEEP_VARS", config_.inprocess.sweep_vars);
  sweep_u32("TACTIX_SWEEP_CLAUSES", config_.inprocess.sweep_clauses);
  sweep_u32("TACTIX_SWEEP_CANDIDATES",
            config_.inprocess.sweep_candidate_pairs);
  sweep_u32("TACTIX_SWEEP_FLIP_ROUNDS",
            config_.inprocess.sweep_flip_rounds);
  sweep_u32("TACTIX_SWEEP_SEEDS", config_.inprocess.sweep_seed_cap);
  if (const char *s = std::getenv("TACTIX_SWEEP_TICKS"))
    config_.inprocess.sweep_environment_ticks = std::strtoull(s, nullptr, 10);
  sweep_bool("TACTIX_SWEEP_STRIP",
             config_.inprocess.sweep_strip_root_false);
  sweep_bool("TACTIX_SWEEP_PARTITION",
             config_.inprocess.sweep_full_partition);
  sweep_bool("TACTIX_SWEEP_RESUME", config_.inprocess.sweep_resume);

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
  preprocess_followups_ = cfg.preprocess_followups;
  preprocess_only_ = cfg.preprocess_only ||
                     std::getenv("TACTIX_PREPROCESS_ONLY") != nullptr;
  if (const char *pf = std::getenv("TACTIX_PREPROCESS_FOLLOWUPS"))
    preprocess_followups_ = std::strtoul(pf, nullptr, 10) != 0;
}

void DefaultInprocessScheduler::reset(const Kernel &k) {
  initialized_ = false;
  search_limits_initialized_ = false;
  const uint64_t ticks = search_ticks(k);
  probe_clock = elim_clock = {};
  probe_clock.anchor = elim_clock.anchor = ticks;
  bve_rounds_run_ = 0;
  factor_large_formula_ = false;
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
        // DefaultPolicy's Phase-4 reference is original Kissat.  Keep the
        // CaDiCaL-derived proof rules registered and explicitly poseable, but
        // do not splice them into Kissat's shipped probe/eliminate pipelines:
        // doing so changes the formula before the next common control choice.
        // Sweep remains in the reference pipeline and therefore ships on.
        const bool non_kissat_pipeline =
            !std::strcmp(nm, "HyperBinaryRes") ||
            !std::strcmp(nm, "Probe") ||
            !std::strcmp(nm, "HyperTernaryRes") ||
            !std::strcmp(nm, "Instantiate") || !std::strcmp(nm, "BCE") ||
            !std::strcmp(nm, "CCE");
        // Strengthen is the one fused Kissat forward pass: it subsumes and
        // self-subsumes under the shared allowance.  Keep standalone C1 fully
        // registered/poseable, but do not run the duplicated decomposition in
        // the reference-shaped default strategy.
        const bool duplicated_decomposition =
            !std::strcmp(nm, "SubsumeForward");
        bool on = !non_kissat_pipeline && !duplicated_decomposition &&
                  !( !config().bve_enabled && !std::strcmp(nm, "BVE")) &&
                  !( !config().vivify_enabled && !std::strcmp(nm, "Vivify")) &&
                  !(bve_off && !std::strcmp(nm, "BVE")) &&
                  !(factor_off && !std::strcmp(nm, "Factor"));
        if (env_list("TACTIX_SLOT_ENABLE", nm))
          on = true;
        if (env_list("TACTIX_SLOT_DISABLE", nm))
          on = false;
        SlotEntry e;
        e.defer_preprocess = !std::strcmp(nm, "BVE");
        e.rule = i;
        // Kissat transitive.c activity-sorts its variable stack and consumes
        // it from the back.  The dedicated comparator path above also retains
        // its exact IDX-on-variable reference behavior.
        if (!std::strcmp(nm, "TransitiveReduce") || !std::strcmp(nm, "BVE"))
          e.order = CandidateOrder::Heuristic;
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
  // `eliminateocclim` is a Kissat option, so `variable_score` reads the same
  // bound wherever it runs, not only inside an elimination round.  Install it
  // when the scheduler attaches to the kernel: the candidate order is a public
  // action surface and may be consulted before the first phase, and the
  // kernel's unlimited default squares to a magnitude that cancels every
  // score difference in `kissat_bve_variable_score`.
  k.bve_occurrence_limit = config().bve_occurrence_limit;
  k.bve_clause_size_limit = config().bve_clause_size_limit;
  if (!initialized_) {
    initialized_ = true;
    // Probe is due immediately because it owns initial preprocessing.
    // Eliminate is not part of that pipeline.  Kissat initializes its first
    // Eliminate deadline in start_search, after preprocessing has established
    // the formula population used by kissat_scale_delta.  Keep it unreachable
    // until initialize_search_limits observes that same lifecycle boundary.
    probe_clock.next_conflicts = k.stats.conflicts;
    elim_clock.next_conflicts = UINT64_MAX;
  }
}

void DefaultInprocessScheduler::initialize_search_limits(const Kernel &k) {
  if (search_limits_initialized_)
    return;
  // kimits.c:kissat_init_limits runs from search.c:start_search, after
  // preprocessing, late Lucky, and classification.  rounds==0 makes the
  // existing NLOG2N updater use n=1, i.e. eliminateinit=500 with the exact
  // post-preprocessing BINIRR_CLAUSES scaling population.
  update_limit(k, SlotGroup::Eliminate);
  search_limits_initialized_ = true;
}

int DefaultPolicy::solve(Kernel &k, volatile sig_atomic_t *stop) {
  KernelStopPublisher stop_published(k, stop);
  KernelAnalysisConflictPublisher analysis_conflict_published(
      k,
      [](Kernel &kernel, void *state) {
        static_cast<DefaultPolicy *>(state)->update_tiers_after_analysis(kernel);
      },
      this);
  ScopedArenaOrdering arena_ordered(k, inprocess_.arena_ordering());
  if (k.inconsistent)
    return 20;
  // A solve always starts from the root closure.  Besides matching the
  // reference lifecycle, this makes repeated/incremental calls independent of
  // a caller's last search level before startup preprocessing inspects clauses.
  if (k.level)
    kernel_backtrack(k, 0);
  std::optional<ScopedHeuristics> owned_h;
  if (!k.h)
    owned_h.emplace(k);
  solves++;
  configure_from_env(k);
  k.conflict_reuse_chrono_levels = chrono_levels;
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
#ifdef TACTIX_ENABLE_CONFLICT_STOP
  uint64_t diagnostic_stop_conflict = UINT64_MAX;
  if (const char *stop = std::getenv("TACTIX_STOP_AFTER_CONFLICT"))
    diagnostic_stop_conflict = std::strtoull(stop, nullptr, 10);
  const auto diagnostic_conflict_stop = [&] {
    if (k.stats.conflicts >= diagnostic_stop_conflict)
      std::raise(SIGTERM);
  };
#else
  const auto diagnostic_conflict_stop = [] {};
#endif
  event_trace.macro(k, "startup_begin");
  auto finish = [&](int verdict) {
    char payload[16];
    std::snprintf(payload, sizeof payload, "%d", verdict);
    event_trace.macro(k, "terminate", payload);
    event_trace.close();
    return verdict;
  };

  std::optional<PreSearchTickScope> presearch_ticks;
  presearch_ticks.emplace(k);

  // The references expose two distinct Lucky opportunities around initial
  // preprocessing. Keep them out of the CDCL loop so neither can repeat after
  // a later inprocessing phase.
  ApplyResult startup = default_propagate(k, "startup");
  if (startup != ApplyResult::Ok)
    return finish(20); // startup is at root: a conflict is a refutation
  if (lucky_guard(k)) {
    event_trace.macro(k, "lucky_early_begin");
    k.stats.lucky_early_calls++;
    if (default_direct_rule(k, "Lucky", "early",
                            [&] { return lucky_apply(k); }) ==
        ApplyResult::Sat) {
      event_trace.macro(k, "lucky_early_end", "sat");
      return finish(10);
    }
    event_trace.macro(k, "lucky_early_end", "unknown");
  }
  event_trace.macro(k, "preprocess_begin");
  // Kissat preprocess.c initially propagates, then consumes those root units
  // by sparse-collecting the whole large-clause arena before the first Probe
  // technique. This is lifecycle scaffolding, not an authored Strengthen slot.
  RootCollectOutcome collected;
  do {
#ifdef TACTIX_ENABLE_EVENT_TRACE
    for (Lit l = 0; l < static_cast<Lit>(2 * k.nvars); l++)
      event_trace_substitute_map(k, l, l);
#endif
    event_trace_slot_boundary(k, "SparseCollect", "begin");
    collected = kernel_sparse_collect_root(k);
    event_trace_slot_boundary(k, "SparseCollect", "end");
    if (collected == RootCollectOutcome::Refuted)
      return finish(20);
    if (collected == RootCollectOutcome::Unit) {
      const ApplyResult root_result = default_propagate(k, "root-collect");
      if (root_result == ApplyResult::Unsat)
        return finish(20);
    }
  } while (collected == RootCollectOutcome::Unit);
  // Kissat's initial sparse collection compacts every fixed internal variable
  // onto one representative.  Tactix preserves kernel identities for its
  // universal rule surface, but the random brancher's index permutation is
  // policy state and must use the same compacted population and ordering.
  event_trace_target_state(k, "startup-compact-pre",
                           static_cast<uint32_t>(k.trail.size()));
  heuristics_compact_random_order_fixed(k);
  event_trace_target_state(
      k, "startup-compact-post",
      static_cast<uint32_t>(k.compact_import_order.size() -
                            k.unassigned_active));
#ifdef TACTIX_ENABLE_EVENT_TRACE
  for (Var v = 0; v < k.nvars; v++) {
    uint32_t ordinal = 0;
    for (uint32_t sign = 0; sign < 2; sign++)
      for (const WatchEntry w : k.watches.entries(make_lit(v, sign != 0)))
        if (w.binary())
          event_trace_sweep_watch(k, v, ordinal++, w.other());
  }
#endif
  if (inprocess_.want_phase(k, SlotGroup::Probe) &&
      inprocess_.run_phase(k, SlotGroup::Probe, stop) == 20)
    return finish(20);
  event_trace.macro(k, "preprocess_end");
  startup = default_propagate(k, "post-preprocess");
  if (startup != ApplyResult::Ok)
    return finish(20);
  if (lucky_late_ && lucky_guard(k)) {
    event_trace.macro(k, "lucky_late_begin");
    k.stats.lucky_late_calls++;
    if (default_direct_rule(k, "Lucky", "late",
                            [&] { return lucky_apply(k); }) ==
        ApplyResult::Sat) {
      k.stats.lucky_late_sat++;
      event_trace.macro(k, "lucky_late_end", "sat");
      return finish(10);
    }
    event_trace.macro(k, "lucky_late_end", "unknown");
  }
  presearch_ticks.reset();
  // There is no Kissat collection point between late Lucky and search
  // (preprocess.c:57, reduce.c:160-185).  In particular, collecting the whole
  // arena here would apply DefaultArenaOrder after preprocessing and invent a
  // representation transition the reference never performs.  Tactix models
  // initial fixed-variable compaction's decision-population effect above;
  // subsequent variable/arena compaction remains coupled to Reduce below.
  // Preprocessing has consumed the root-unit trail prefix.  Tactix retains
  // those literals for stable IDs/proof witnesses, so only the lifecycle
  // marker is cleared (Kissat physically clears that prefix).
  k.unflushed_root = 0;
#ifdef TACTIX_ENABLE_EVENT_TRACE
  if (const char *selected = std::getenv("TACTIX_EVENT_TRACE_FACTOR_LITERAL"))
    for (Lit lit = 0; lit < static_cast<Lit>(2 * k.nvars); lit++)
      if (std::atoi(selected) == kernel_externalize(k, lit)) {
        event_trace_factor_literal_state(
            k, lit, k.factor_marks[lit], kernel_var_retired(k, var_of(lit)),
            k.value(lit), k.watches.size(lit));
        uint32_t ordinal = 0;
        for (const WatchEntry watch : k.watches.entries(lit)) {
          const Clause *clause =
              watch.binary() ? nullptr : clause_at(k.arena, watch.ref());
          event_trace_factor_literal_entry(
              k, lit, ordinal++, !clause || !clause->garbage(),
              watch.binary() ? watch.other() : INVALID_LIT,
              watch.binary() ? kernel_binary_id(k, lit, watch.other())
                             : clause->id(),
              clause);
        }
      }
#endif

  // Kissat classifies once after startup simplification and again while
  // entering search (`search.c:kissat_search`, then `start_search`).  The
  // calls are adjacent there too; preserve both so the reach counter and the
  // classification lifetime match the reference exactly.
  update_formula_classification(k);
  update_formula_classification(k);

  // Kissat initializes the first search-time Eliminate limit only from
  // start_search, after startup simplification has changed BINIRR_CLAUSES.
  inprocess_.initialize_search_limits(k);

  // Kissat initializes the first focused-mode tick anchor in `start_search`,
  // after Lucky and preprocessing.  Tactix's schedule object is reset before
  // those startup phases, whose kernel work also uses the shared tick meter;
  // rebase this one clock so startup work cannot lengthen the first stable
  // interval.  Later anchors are maintained by `update_mode_limits`.
  mode.ticks_at_focus_entry = search_ticks(k);

  for (;;) {
    event_trace.checkpoint(k);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    if (k.propagate_head < k.trail.size()) {
      const std::string cursor_payload =
          "next=" + std::to_string(kernel_externalize(k, k.trail[k.propagate_head])) +
          "|pos=" + std::to_string(k.propagate_head) +
          "|trail=" + std::to_string(k.trail.size());
      event_trace.detail_text(k, "propagate_cursor", cursor_payload.c_str());
    }
#endif
    const size_t trace_trail_begin = k.trail.size();
    const ApplyResult r = default_propagate(k, "search");
    event_trace.implications(k, trace_trail_begin);
    if (r == ApplyResult::Unsat)
      return finish(20);
    if (r == ApplyResult::Conflict) {
      BranchingPolicy &bp = branching ? *branching : default_branching_;
      bp.on_search_propagation_conflict();
      event_trace.conflict(k);
      event_trace.detail(k, "analysis_begin", k.level);
      event_trace.mode_clock(k, "pre_analyze", mode.ticks_limit,
                             mode.ticks_at_focus_entry, mode.count);
      observe_decision_rate(k); // kissat: before the analysis, once per conflict
#ifdef TACTIX_ENABLE_EVENT_TRACE
      event_trace.detail(k, "reason_bump_delay_before",
                         bump_reasons_delay_.count);
      event_trace.detail(k, "reason_bump_rate_milli",
                         static_cast<int64_t>(
                             averages[k.h->stable].decision_rate.value * 1000));
#endif
      k.bump_reason_limit = want_bump_reasons(k);
      event_trace.detail(k, "reason_bump_limit", k.bump_reason_limit);
      k.bump_reason_depth = config_.analysis.reason_depth;
      const uint64_t learned_before = k.stats.learned;
      const uint64_t br_rounds = k.stats.reason_bump_rounds;
      const uint64_t br_aborted = k.stats.reason_bump_aborted;
      const ApplyResult ar = default_direct_rule(
          k, "ConflictAnalysis", "conflict=" +
              std::to_string(k.stats.conflicts),
          [&] { return analyze_apply(k); });
      k.bump_reason_limit = 0;
      k.bump_reason_depth = 1;
      // Kissat calls DELAYING(bumpreasons) inside
      // analyze_reason_side_literals, after ordinary analysis has produced a
      // final clause.  Reused conflicts, failed-literal unit batches, and
      // other early analysis exits must not consume this countdown.
      if (k.pending.valid && k.stats.learned != learned_before)
        bump_reasons_delay_.reached_gate(
            k.stats.reason_bump_rounds != br_rounds,
            k.stats.reason_bump_aborted != br_aborted);
      if (ar == ApplyResult::Unsat) {
        event_trace.detail(k, "analysis_end", -1);
        return finish(20);
      }
      update_tiers_after_analysis(k);
      if (!k.pending.valid) { // reused conflict or direct level-one unit batch
        // Level-one failed-literal analysis owns its root-unit batch inside
        // the atomic M3 transition, so it has no pending clause for policy to
        // observe. Kissat still updates the glue averages and reluctant clock
        // once for that size-one/glue-zero conflict result (learn.c:120-130).
        if (k.stats.learned != learned_before) {
          observe_glue(k, 0);
          if (k.h->stable)
            tick_reluctant();
        }
        event_trace.detail(k, "analysis_end", k.level);
        event_trace.mode_clock(k, "post_analyze", mode.ticks_limit,
                               mode.ticks_at_focus_entry, mode.count);
        diagnostic_conflict_stop();
        continue;
      }
      event_trace.learned(k, k.pending.lits);
      event_trace.detail(k, "analysis_tail_learned_emitted",
                         k.pending.lits.size());
      observe_glue(k, k.pending.glue); // policy observes; Analyze is pure
      if (k.h->stable)
        tick_reluctant(); // per learned clause (kissat learn.c)
      event_trace.detail(k, "analysis_tail_policy_updated", k.pending.glue);
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
        event_trace.detail(k, "analysis_tail_target", target);
        backjump_apply_to(k, target);
      }
      event_trace.detail(k, "analysis_tail_backjump_return", k.level);
      event_trace.heap_scores(k);
      event_trace.detail(k, "analysis_propagate_head", k.propagate_head);
      event_trace.detail(k, "analysis_trail_size", k.trail.size());
      event_trace.detail(k, "analysis_end", k.level);
      event_trace.mode_clock(k, "post_analyze", mode.ticks_limit,
                             mode.ticks_at_focus_entry, mode.count);
      diagnostic_conflict_stop();
      continue;
    }
    if (k.retire_enabled ? k.unassigned_active == 0 : k.unassigned == 0)
      return finish(10);
    if (want_reduce(k) && reduce_guard(k)) {
      event_trace.macro(k, "reduce_begin");
      reduce_prepare(k);
      if (k.inconsistent)
        return finish(20);
      ReductionPolicy &rp = reduction ? *reduction : default_reduction_;
      if (dynamic_tiers_)
        retier_current_mode(k, config_.reduction);
      pose_reduce_tiers(k);
      // Kissat samples compactlim before selecting/deleting victims and uses
      // that decision to choose sparse-collection start 0 versus the first
      // reducible clause.  Reusing the decision after Reduce also prevents a
      // formula-changing pass from silently changing the admission boundary.
      const bool compact_due =
          compact_ && kernel_should_compact_variables(
                          k, config_.reduction.compact_inactive_percent);
      reduce_candidates(k, reduce_scratch_);
      const size_t reduce_victims =
          std::min(rp.victims(k, reduce_scratch_.size()),
                   reduce_scratch_.size());
      event_trace.reduction(k, reduce_scratch_, reduce_victims);
      (void)default_direct_rule(
          k, "Reduce", "victims=" + std::to_string(reduce_victims), [&] {
            return reduce_apply_refs(k, reduce_scratch_.data(), reduce_victims,
                                     compact_due);
          });
      if (compact_due)
        event_trace_target_state(
            k, "reduce-compact-pre",
            static_cast<uint32_t>(k.compact_import_order.size() -
                                  k.unassigned_active));
      if (compact_due && kernel_compact_variables(k))
        heuristics_compact_random_order_fixed(k);
      if (compact_due)
        event_trace_target_state(
            k, "reduce-compact-post",
            static_cast<uint32_t>(k.compact_import_order.size() -
                                  k.unassigned_active));
      update_reduce_limit(k);
      pose_focused_tiers(k); // vivifyfocusedtiers=1 is the shipped default.
      update_formula_classification(k);
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
      // Kissat mode.c calls `kissat_start_random_sequence` after every mode
      // transition.  It is a no-op on stable entry and starts (or continues)
      // a focused sequence regardless of decision level or its ordinary
      // conflict deadline.
      if (!branching)
        default_branching_.start_random_sequence(k);
      event_trace.macro(k, "switch_end");
      event_trace.heap_scores(k);
      event_trace.mode_clock(k, "post_switch", mode.ticks_limit,
                             mode.ticks_at_focus_entry, mode.count);
      continue;
    }
    if (want_restart(k) && restart_guard(k)) {
      event_trace.macro(k, "restart_begin");
      // Kissat restart.c asks `kissat_next_decision_variable` for the reuse
      // threshold.  That call is semantically part of restart: during a
      // random-decision sequence it consumes and counts a draw before the
      // post-restart decision.  Route through the active branching subpolicy,
      // then hand only its variable to the policy-neutral Restart rule.
      BranchingPolicy &bp = branching ? *branching : default_branching_;
      const Lit next = bp.pick(k);
      const uint32_t keep =
          heuristics_restart_reuse_level_for(k, var_of(next));
      if (keep < k.level)
        heuristics_update_target_phases(k);
      (void)default_direct_rule(
          k, "Restart", "keep-level=" + std::to_string(keep),
          [&] { return restart_apply_to(k, keep); });
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
      if (restart_guard(k)) {
        heuristics_update_target_phases(k);
        (void)default_direct_rule(k, "Restart", "keep-level=0;rephase=1",
                                  [&] { return restart_backtrack_to(k, 0); });
      }
      k.unflushed_root = 0; // Kissat Rephase flushes the root trail here.
      static constexpr uint64_t WALK_SLOT = UINT64_MAX;
      static constexpr uint64_t ROTATION[6] = {
          REPHASE_BEST, WALK_SLOT, REPHASE_INVERTED,
          REPHASE_BEST, WALK_SLOT, REPHASE_ORIGINAL};
      const uint64_t type = ROTATION[rephase.count % 6];
      rephase.count++; // ONE rephase event, whatever it is composed of
      if (type == WALK_SLOT && walk_guard(k)) {
        // Kissat's W slot replaces Best.  The old Tactix default composed
        // B+W, changing the assignment imported by the walker before its
        // first flip.  Retain the composition only as an explicit diagnostic.
        static const bool augment_walk_with_best = [] {
          const char *value = std::getenv("TACTIX_WALK_AUGMENT_BEST");
          return value && std::strtoul(value, nullptr, 10) != 0;
        }();
        if (augment_walk_with_best)
          rephase_apply_type(k, REPHASE_BEST);
        // Kissat warmup.c: before importing phases into the walker, complete a
        // temporary assignment with the active branch ranking and propagate
        // beyond conflicts. Assignment-time phase saving is the output; the
        // trail is then discarded without undoing it.
        BranchingPolicy &bp = branching ? *branching : default_branching_;
        assert(k.level == 0 && k.propagate_head == k.trail.size());
        k.warming = true;
        k.stats.warmups++;
        while (k.retire_enabled ? k.unassigned_active : k.unassigned) {
          if (kernel_terminating(k))
            break;
          const Lit decision = bp.pick(k);
          kernel_assign_decision(k, decision);
          k.stats.warming_decisions++;
          event_trace.detail(k, "decision", kernel_externalize(k, decision));
          event_trace.heap_scores(k);
          const size_t warmup_trail_begin = k.trail.size();
          kernel_propagate_beyond_conflicts(k);
          event_trace.implications(k, warmup_trail_begin);
        }
        if (k.level)
          kernel_backtrack(k, 0);
        k.warming = false;
        uint64_t reference = search_ticks(k) - walk_anchor;
        if (reference < config_.inprocess.minimum_effort_ticks)
          reference = config_.inprocess.minimum_effort_ticks;
        k.work_budget = scale_permille(reference, config_.rephase.walk_permille);
        // Kissat seeds the walker from the search policy's shared generator.
        // DefaultBranching owns that generator in Tactix; snapshot it here so
        // the H-only Walk operation need not reach back into its caller.
        k.walk_random_seed = default_branching_.randec_rng;
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
      update_formula_classification(k);
      event_trace.macro(k, "probe_end");
      continue;
    }
    if (inprocess_.want_phase(k, SlotGroup::Eliminate)) {
      event_trace.macro(k, "eliminate_begin");
      if (inprocess_.run_phase(k, SlotGroup::Eliminate, stop) == 20)
        return finish(20);
      update_formula_classification(k);
      event_trace.macro(k, "eliminate_end");
      continue;
    }
    if (stop && *stop)
      return finish(0);
    BranchingPolicy &bp = branching ? *branching : default_branching_;
    const Lit decision = bp.pick(k);
    (void)default_direct_rule(
        k, "Decide",
        "literal=" + std::to_string(kernel_externalize(k, decision)),
        [&] { return decide_apply(k, decision, false); });
    event_trace.detail(k, "decision", kernel_externalize(k, decision));
    event_trace.heap_scores(k);
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
  // Kissat constructs one embedded Kitten at the beginning of each
  // `eliminate` invocation and releases it at the end.  Candidate-to-candidate
  // clears advance its deterministic seed, while a later BVE slot restarts
  // that lifetime.  The policy owns this phase boundary; BVE and SubSolver own
  // only definition and legal core-refinement mechanics.
  if (bve_entry)
    k.bve_definition_session = k.bve_definition_session + 1;
  BveBoundPolicy *ramp = nullptr;
  if (bve_entry && !std::getenv("TACTIX_NO_BVE_RAMP"))
    ramp = bve_bound ? bve_bound : &default_bve_ramp_;
  const bool level_mode = bve_entry && ramp && !bve_oneshot;
  const uint32_t passes = level_mode ? 1 : ramp ? ramp->num_passes() : 1;
  uint64_t elim_at_last_pass = k.stats.bve_eliminated;
  const bool use_retry = bve_entry && !ramp;
  k.bve_track_min_bound = ramp != nullptr;
  bool engaged = false; // a pass got past the budget break and ran instances
  for (uint32_t pass = 0; pass < passes && !ctx.phase_unsat; pass++) {
    if (ramp && e.meter(k) > limit) {
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
    const bool dynamic_bve =
        bve_entry && !candidate_order && e.order == CandidateOrder::Heuristic;
    BveCandidateHeap bve_heap;
    if (dynamic_bve)
      bve_heap.initialize(k, payloads_.data(), n);
    else
      order_candidates(k, e, payloads_.data(), n);
    if (std::getenv("TACTIX_SLOT_TRACE"))
      std::fprintf(stderr,
                   "[slot]   %s pass=%u n=%u bve_bound=%u meter=%llu "
                   "limit=%llu\n",
                   r.name, pass, n, k.bve_bound,
                   (unsigned long long)e.meter(k), (unsigned long long)limit);
    if (dynamic_bve) {
      assert(!k.bve_score_update_hook);
      k.bve_score_update_hook = BveCandidateHeap::update_hook;
      k.bve_score_update_state = &bve_heap;
    }
    uint32_t i = 0;
    while (dynamic_bve ? !bve_heap.empty() : i < n) {
      const uint64_t payload =
          dynamic_bve ? bve_heap.pop_max() : payloads_[i++];
      if (e.meter(k) > limit) {
        break; // dose spent
      }
      if (!r.guard(k)) {
        break;
      }
      if (use_retry || ramp) {
        Lit rl;
        if (lit_of_instance(k, payload, &rl) &&
            var_of(rl) < k.nvars && !k.bve_retry[var_of(rl)] &&
            (!ramp || k.bve_min_bound[var_of(rl)] > k.bve_bound))
          continue;
      }
      if (ctx.stop && *ctx.stop)
        break; // §6: a dose can enumerate thousands of candidates
      if (e.cursor_enabled) {
        e.cursor = instance_key(payload) + 1;
        e.ord.advanced++;
      }
      const bool hand_residual = e.budget_aware && e.meter;
      if (hand_residual) {
        const uint64_t remaining = ledger_remaining(e, k, limit);
        k.work_budget = remaining ? remaining : 1;
      }
      const ApplyResult ar = [&] {
        TickSinkScope sink(k, e.tick_counter);
#ifdef TACTIX_ENABLE_EVENT_TRACE
        Var trace_bve_var = INVALID_VAR;
        if (bve_entry) {
          Lit candidate;
          if (lit_of_instance(k, payload, &candidate)) {
            trace_bve_var = var_of(candidate);
            event_trace_bve_candidate(k, trace_bve_var, "begin", e.meter(k),
                                      limit, false);
          }
        }
#endif
        const ApplyResult result = metered_rule_apply(k, e.meter, [&] {
          return apply_rule_instance(r, k, payload);
        });
#ifdef TACTIX_ENABLE_EVENT_TRACE
        if (trace_bve_var != INVALID_VAR)
          event_trace_bve_candidate(k, trace_bve_var, "end", e.meter(k),
                                    limit, result == ApplyResult::Ok);
#endif
        return result;
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
      const ApplyResult propagation =
          default_propagate(k, "inprocess-candidate");
      if (propagation == ApplyResult::Unsat) {
        ctx.phase_unsat = true;
        break;
      }
    }
    if (dynamic_bve) {
      k.bve_score_update_hook = nullptr;
      k.bve_score_update_state = nullptr;
      assert(k.bve_occurrence_count_image);
      k.bve_occurrence_count_image = nullptr;
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
  // Formula profiles may suppress repeated Factor work, but never the startup
  // entry. Kissat 4.0.4 admits `preprocessfactor` independently of formula
  // shape; keeping the entry prevents density classification from hiding
  // productive BVA. The resource and decade governors below remain separate
  // policy decisions and may still decline before rule dispatch.
  if (k.stats.conflicts > 0 && !cfg.factor_repeated_enabled) {
    e.tm.dec_profile++;
    return true;
  }
  const uint64_t active = active_variables(k);
  // Factor's 700M-tick first dose can dominate an entire P4 allocation after
  // one BVE round relaxes the reference factordelay exponent.  Keep that
  // startup population as a permanent ceiling: smaller formulas preserve the
  // reference round-dependent admission exactly, while large hardware
  // encodings continue through search instead of spending the gate in a
  // candidate scan that yields no factorization.
  if (active >= cfg.factor_active_ceiling)
    factor_large_formula_ = true;
  if (factor_large_formula_) {
    e.tm.dec_warmup++;
    return true;
  }
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
  // Kissat's call-level watermark is exactly the population represented by
  // `flags->factor`: a clause addition sets a literal's bit and Factor clears
  // it when that seed is popped (factor.c:1093-1117).  The old proxy compared
  // monotone binary-map/arena sizes and root units.  An elimination can replace
  // thousands of clauses while leaving all three proxy values below their
  // previous highs, delaying Factor by a whole probe phase even though the
  // precise per-literal marks say new material exists.
  const bool no_marked_literal =
      std::none_of(k.factor_marks.begin(), k.factor_marks.end(),
                   [](uint8_t marked) { return marked != 0; });
  if (k.stats.conflicts > 0 && no_marked_literal) {
    e.tm.dec_material++;
    return true;
  }
  return false;
}

void DefaultInprocessScheduler::factor_dose_config(Kernel &k, SlotEntry &) {
  k.factor_composite = true;
}

ApplyResult DefaultInprocessScheduler::run_vivify_dose(Kernel &k, SlotEntry &e,
                                           const RuleVTable &r,
                                           uint64_t limit) {
  // `vivifyfocusedtiers=1` is Kissat's shipped policy: every Vivify band reads
  // the focused-mode tier snapshot, independent of the current search mode.
  // A stable OTFS retier legitimately poses the stable snapshot for the
  // analysis it resumes, so pose the Vivify consumer here instead of relying
  // on the last Reduce to have left these shared fields in focused mode.
  pose_focused_tiers(k);
  const auto &cfg = config();
  const uint64_t remaining0 = ledger_remaining(e, k, limit);
  const uint64_t remaining = remaining0 ? remaining0 : 1;
  const uint64_t dose_start = e.meter(k);
  const bool irr_delayed = k.stats.conflicts > 0 && e.delay_count > 0;
  if (irr_delayed)
    e.delay_count--;
  const char *no_t3 = std::getenv("TACTIX_VIVIFY_TIER3");
  const bool split = !(no_t3 && *no_t3 == '0');
  ApplyResult ar = ApplyResult::NoOp;
  VivifyReclamationBatch reclamation;
  const auto apply_band = [](Kernel &kernel, void *state) {
    return vivify_apply_batched(
        kernel, *static_cast<VivifyReclamationBatch *>(state));
  };
  // Kissat advances one cumulative probing-tick ceiling by each band's
  // proportional share (vivify.c:1455-1490).  Keep the same ledger here:
  // unused work from an early tier belongs to every later tier, while an
  // overrun is precharged rather than granting the next call a fresh dose.
  const uint64_t denominator = irr_delayed ? cfg.vivify_redundant_share
                                           : cfg.vivify_total_share;
  auto share = [&](uint32_t weight) {
    return remaining / denominator * weight +
           remaining % denominator * weight / denominator;
  };
  uint64_t cumulative_budget = 0;
  k.work_budget_bounded = true;
  const uint32_t passes =
      split ? static_cast<uint32_t>(cfg.vivify_tier_scopes.size()) : 1u;
  for (uint32_t p = 0; p < passes && ar != ApplyResult::Unsat && !k.inconsistent;
       p++) {
    k.vivify_scope = split ? cfg.vivify_tier_scopes[p] : uint8_t{1};
    cumulative_budget +=
        share(split ? cfg.vivify_tier_shares[p]
                    : cfg.vivify_redundant_share);
    k.work_budget = cumulative_budget;
    k.work_budget_offset = e.meter(k) - dose_start;
    TickSinkScope sink(k, e.tick_counter);
    const ApplyResult a =
        metered_rule_apply(k, e.meter, [&] {
          return apply_rule_with(r, k, apply_band, &reclamation);
        });
    if (a == ApplyResult::Unsat ||
        (a == ApplyResult::Ok && ar != ApplyResult::Unsat))
      ar = a;
  }
  if (!irr_delayed && ar != ApplyResult::Unsat && !k.inconsistent) {
    cumulative_budget += share(cfg.vivify_total_share -
                               cfg.vivify_redundant_share);
    const uint64_t tried_before = k.stats.vivify_checked;
    const auto produced = [&] {
      // Kissat increments vivifier.vivified for every successful candidate
      // terminal, not only learned replacements (vivify.c:1129-1217,
      // 1313-1315).  Root-schedule cleanup is outside the tried population.
      return k.stats.vivify_subsumed + k.stats.vivify_shrunken +
             k.stats.vivify_implied + k.stats.vivify_asymmetric +
             k.stats.vivify_instantiated;
    };
    const uint64_t produced_before = produced();
    k.vivify_scope = 2;
    k.work_budget = cumulative_budget;
    k.work_budget_offset = e.meter(k) - dose_start;
    const ApplyResult ar2 = [&] {
      TickSinkScope sink(k, e.tick_counter);
      return metered_rule_apply(k, e.meter,
                                [&] {
                                  return apply_rule_with(
                                      r, k, apply_band, &reclamation);
                                });
    }();
    if (ar2 == ApplyResult::Unsat)
      ar = ar2;
    else if (ar2 == ApplyResult::Ok && ar != ApplyResult::Ok)
      ar = ar2;
    if (k.stats.conflicts > 0) {
      const uint64_t tried = k.stats.vivify_checked - tried_before;
      const uint64_t produced_now = produced() - produced_before;
      if (tried == 0 ||
          produced_now * PERCENT_SCALE <
              tried * cfg.vivify_irredundant_productivity_percent) {
        e.delay_current += (e.delay_current < UINT32_MAX);
        e.delay_count = e.delay_current;
      } else {
        e.delay_current /= 2;
        e.delay_count = e.delay_current;
      }
    }
  }
  if (ar != ApplyResult::Unsat && !k.inconsistent)
    reclamation.collect(k);
  k.vivify_scope = 0;
  k.work_budget = 0;
  k.work_budget_offset = 0;
  k.work_budget_bounded = false;
  return ar;
}

ApplyResult DefaultInprocessScheduler::run_budgeted_batch(Kernel &k, SlotEntry &e,
                                              const RuleVTable &r,
                                              uint64_t limit,
                                              bool factor_entry) {
  const uint64_t remaining0 = ledger_remaining(e, k, limit);
  const bool substitute = !std::strcmp(r.name, "Substitute");
  // Substitute's effort is tested after an indivisible productive round.
  // A zero allowance still admits that mandatory first round; other
  // budget-aware rules retain a one-tick hand-off so they can make progress.
  const uint64_t handed = substitute ? remaining0
                                     : (remaining0 ? remaining0 : 1);
  const uint64_t spent_before = e.meter(k);
  bool substitute_round_budget_break = false;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  uint32_t substitute_rounds_run = 0;
  e.journal_last_round_spent = 0;
  e.journal_rounds = 0;
  e.journal_round_budget_break = false;
#endif
  if (factor_entry)
    factor_dose_config(k, e);
  k.work_budget = handed;
  // These rules meter partner visits internally.  A dose is only a real
  // scheduling bound when the implementation is allowed to stop inside a
  // candidate; otherwise one saturated occurrence list can overrun the whole
  // phase by orders of magnitude.  A cut is resumable: each rule re-arms the
  // current candidate and the untouched tail for the next dose.
  const bool previous_midround_budget = k.midround_budget;
  k.midround_budget = config().forward_midround_budget &&
                      (!std::strcmp(r.name, "SubsumeForward") ||
                       !std::strcmp(r.name, "Strengthen"));
  ApplyResult ar;
  {
    TickSinkScope sink(k, e.tick_counter);
    if (!std::strcmp(r.name, "Sweep")) {
      const SweepLimits lim = configured_sweep_limits(config());
      const uint64_t t0 = now_nanos();
      ar = sweep_apply_with_limits(k, lim);
      if (r.id < Stats::MAX_TIMED_RULES) {
        k.stats.rule_nanos[r.id] += now_nanos() - t0;
        k.stats.rule_calls[r.id]++;
      }
    } else if (substitute) {
      // kissat substitute.c:568-602 runs round one of every authored call
      // unconditionally.  Only after that round retires a variable does its
      // own tick delta get compared with 10 per mille of the Probe reference;
      // an overrun suppresses the optional next round, never a later call.
      // `complete=true` during initial probing ignores the comparison.
      const uint64_t t0 = now_nanos();
      ar = ApplyResult::NoOp;
      const bool complete = k.stats.conflicts == 0;
      for (uint32_t round = 0; round < config().substitute_rounds; round++) {
        const uint64_t active_before = active_variables(k);
        const uint64_t round_entry = e.meter(k);
        const ApplyResult current = substitute_apply(k);
        const uint64_t round_spent = e.meter(k) - round_entry;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
        substitute_rounds_run++;
        e.journal_rounds = substitute_rounds_run;
        e.journal_last_round_spent = round_spent;
#endif
        if (current == ApplyResult::Unsat) {
          ar = current;
          break;
        }
        if (current == ApplyResult::Ok)
          ar = current;
        const bool productive = active_variables(k) < active_before;
        if (!productive)
          break;
        if (!complete && round_spent > handed) {
          substitute_round_budget_break = true;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
          e.journal_round_budget_break = true;
#endif
          break;
        }
      }
      if (r.id < Stats::MAX_TIMED_RULES) {
        k.stats.rule_nanos[r.id] += now_nanos() - t0;
        k.stats.rule_calls[r.id]++;
      }
    } else {
      ar = metered_rule_apply(k, e.meter, [&] { return apply_rule(r, k); });
    }
  }
  k.midround_budget = previous_midround_budget;
  k.work_budget = 0;
  k.factor_composite = false;
  const uint64_t spent = e.meter(k) - spent_before;
  e.tm.handed += handed;
  e.tm.spent += spent;
  if (spent > 0)
    e.doses++;
  if (spent > 0 &&
      (substitute ? !substitute_round_budget_break : spent <= handed)) {
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
  const bool substitute = !std::strcmp(r.name, "Substitute");
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  e.journal_last_round_spent = 0;
  e.journal_rounds = 0;
  e.journal_round_budget_break = false;
#endif
  // Substitute has no call-entry effort gate in Kissat. Its first round is
  // mandatory and the round-local comparison lives in run_budgeted_batch.
  if (!substitute && e.meter && e.meter(k) > limit) {
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
    // Kissat's two preprocessing follow-ups call substitute with
    // `complete=true`: a productive rewrite can expose another binary SCC,
    // and the shipped `substituterounds=2` consumes that closure immediately.
    // Search-time calls retain the ordinary one-round, effort-ledger path.
    if (k.stats.conflicts == 0 && !std::strcmp(r.name, "Substitute")) {
      const uint64_t t0 = now_nanos();
      ar = substitute_apply_complete(k);
      if (r.id < Stats::MAX_TIMED_RULES) {
        k.stats.rule_nanos[r.id] += now_nanos() - t0;
        k.stats.rule_calls[r.id]++;
      }
    } else {
      ar = metered_rule_apply(k, journal_effort_meter(e, r),
                              [&] { return apply_rule(r, k); });
    }
  }
  ctx.last_result = ar;
  heuristics_consume_rule_advice(k);
  if (e.closure) {
    e.binaries_mark = k.binary_ids.size();
    e.arena_mark = k.arena.size();
    e.root_fixed_mark = kernel_root_fixed(k);
  }
  // Kissat bumps/reduces the named Congruence and Sweep delay ladders after
  // their preprocessing calls too.  Suppressing this update at conflict zero
  // made Tactix run the first search-time Congruence while Kissat consumed its
  // one-call delay.  Substitute has no Kissat delay object; retain its local
  // closure backoff only for search-time applications.
  const bool update_named_delay = dspec;
  const bool update_local_closure_delay = e.closure && k.stats.conflicts > 0;
  if (closure_delay_ &&
      (update_named_delay || update_local_closure_delay)) {
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
  const ApplyResult propagation = default_propagate(k, "inprocess-batch");
  if (propagation == ApplyResult::Unsat) {
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
  k.bve_occurrence_limit = config().bve_occurrence_limit;
  k.bve_clause_size_limit = config().bve_clause_size_limit;
  factor_bound_snapshot_ =
      couple_factor_bound_
          ? (std::getenv("TACTIX_NO_BVE_RAMP") ? k.bve_bound : bve_level_)
          : 0; // ramp off => BVE runs at k.bve_bound, so couple to THAT value,
  InprocessScope inprocess_scope(k);
  const uint64_t phase_props_at_entry = k.stats.propagations;
  const uint64_t phase_nanos_at_entry = now_nanos();
  k.stats.inprocess_phases++;
  if (k.level > 0 && restart_guard(k)) {
    // `kissat_probe` sets `solver->probing` before `probe` enters
    // `kissat_backtrack_propagate_and_flush_trail`; the target/best updater
    // explicitly returns in probing mode.  Eliminate enters the same root
    // restart without that flag and therefore retains the snapshot.
    if (group != SlotGroup::Probe)
      heuristics_update_target_phases(k);
    (void)default_direct_rule(k, "Restart", "keep-level=0;inprocess=1",
                              [&] { return restart_apply_to(k, 0); });
  }
  // Probe and Eliminate enter through
  // `kissat_backtrack_propagate_and_flush_trail`; dense Tactix keeps the root
  // literals but has consumed the same pending-root lifecycle state.
  k.unflushed_root = 0;
  kernel_dense_connect(k, occurrence_index_);
  default_factor_match_.bound = std::max(k.factor_bound, factor_bound_snapshot_); // see DefaultFactorMatch
  k.factor_selection = factor_match ? factor_match : &default_factor_match_;
  if (factor_match)
    factor_match->set_bound(factor_bound_snapshot_);
  SlotPhaseCtx ctx{group, stop};
  const std::vector<RuleVTable> &rules = tactix_rules();
  uint64_t active_before = active_variables(k);
  size_t sweep_retired_begin = SIZE_MAX;
  size_t sweep_substituted_begin = SIZE_MAX;
  const int rounds =
      k.stats.conflicts == 0 ? 1 : static_cast<int>(config().phase_rounds);
  for (ctx.outer = 0;
       ctx.outer < rounds && !ctx.phase_unsat &&
       !(stop && *stop);
       ctx.outer++) {
    ctx.round_changed = false;
    k.subsume_complete = true;
    k.strengthen_complete = true;
    for (size_t slot = 0; slot < entries.size(); slot++) {
      SlotEntry &e = entries[slot];
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
        if (substitute || group == SlotGroup::Probe || ctx.outer == 0) {
          set_entry_limit(e, k);
        }
      }
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      const std::string invocation =
          std::string("group=") +
          (group == SlotGroup::Probe ? "probe" : "eliminate") +
          ";phase=" + std::to_string(clock.rounds) +
          ";round=" + std::to_string(ctx.outer) +
          ";slot=" + std::to_string(slot) +
          ";occasion=" + std::to_string(e.tm.occasions);
      const bool journal_invocation =
          rule_journal_wants(k, r.name, invocation);
      const auto invocation_meter = journal_effort_meter(e, r);
      const uint64_t invocation_entry =
          journal_invocation && invocation_meter ? invocation_meter(k) : 0;
      const uint64_t invocation_pre_digest =
          journal_invocation ? rule_journal_digest(k) : 0;
      const uint64_t invocation_formula_epoch = k.arena_generation;
      const uint64_t invocation_root_epoch = k.touch_fixed_head;
      const SlotEntry::Telem invocation_tm = e.tm;
#endif
      event_trace_slot_boundary(k, r.name, "begin");
      const bool sweep = !std::strcmp(r.name, "Sweep");
      const size_t trail_before = k.trail.size();
      const size_t retired_before = k.retired_events.size();
      const size_t substituted_before = sweep ? k.substituted_events.size() : 0;
      const SlotStep step = (e.dosed && r.instances && r.apply_instance)
                                ? run_dosed_entry(k, e, r, ctx)
                                : run_batch_entry(k, e, r, ctx);
      // Kissat's root assignment primitive marks a variable fixed and removes
      // it from both decision rankings immediately.  Formula rules remain
      // policy-agnostic in Tactix, so publish that same lifecycle at the rule
      // boundary, before consuming the rule's eliminated/substituted events.
      // This ordering is observable for equal-score heaps: Substitute can
      // derive roots while rewriting and only then deactivates its class
      // members (substitute.c remove_representative_equivalences).
      heuristics_consume_root_assignments(k, trail_before);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      if (journal_invocation) {
        const bool admitted = e.tm.admitted != invocation_tm.admitted;
        const char *decline = "none";
        if (!admitted) {
          if (e.tm.dec_budget != invocation_tm.dec_budget)
            decline = "budget";
          else if (e.tm.dec_guard != invocation_tm.dec_guard)
            decline = "guard";
          else if (e.tm.dec_delay != invocation_tm.dec_delay)
            decline = "delay";
          else if (e.tm.dec_material != invocation_tm.dec_material)
            decline = "material";
          else if (e.tm.dec_warmup != invocation_tm.dec_warmup)
            decline = "warmup";
          else if (e.tm.dec_profile != invocation_tm.dec_profile)
            decline = "profile";
          else
            decline = "scheduler";
        }
        const bool bounded = e.meter != nullptr;
        const uint64_t exit =
            invocation_meter ? invocation_meter(k) : invocation_entry;
        const uint64_t delta = exit - invocation_entry;
        const uint64_t terminal_spent = substitute
                                            ? e.journal_last_round_spent
                                            : delta;
        const uint64_t overshoot = bounded && terminal_spent > e.journal_allowance
                                       ? terminal_spent - e.journal_allowance
                                       : 0;
        const uint64_t post_digest = rule_journal_digest(k);
        const char *terminal =
            !bounded ? "unbounded"
            : overshoot ? "overshot"
                      : terminal_spent == e.journal_allowance ? "exhausted"
                                                              : "within";
        RuleJournalRecord record;
        record.kind = "schedule";
        record.rule = r.name;
        record.event = "inprocess-invocation";
        record.candidate = invocation;
        record.trigger = "default-inprocess-scheduler";
        record.context =
            std::string("currency=") + effort_currency(r.name) +
            ";reference=" + std::to_string(e.journal_reference) +
            ";allowance=" + std::to_string(e.journal_allowance) +
            ";bounded=" + (bounded ? "1" : "0") +
            ";entry=" + std::to_string(invocation_entry) +
            ";exit=" + std::to_string(exit) +
            ";delta=" + std::to_string(delta) +
            ";overshoot=" + std::to_string(overshoot) +
            ";terminal=" + terminal +
            (substitute
                 ? ";rounds=" + std::to_string(e.journal_rounds) +
                       ";last-round=" +
                       std::to_string(e.journal_last_round_spent) +
                       ";round-budget-break=" +
                       (e.journal_round_budget_break ? "1" : "0")
                 : "");
        record.formula_epoch = invocation_formula_epoch;
        record.root_epoch = invocation_root_epoch;
        record.pre_digest = invocation_pre_digest;
        record.guards = std::string("admitted=") + (admitted ? '1' : '0');
        record.antecedents = "slot-ledger";
        record.outcome = admitted ? journal_result(ctx.last_result) : "declined";
        record.decline = decline;
        record.deltas = invocation_pre_digest == post_digest
                            ? "none"
                            : "state-digest-change";
        record.witnesses = "rule-owned";
        record.effort = delta;
        record.budget = bounded ? e.limit : 0;
        record.post_digest = post_digest;
        rule_journal_emit(k, record);
      }
#endif
      if (!std::strcmp(r.name, "Congruence"))
        ctx.congruence_changed = ctx.last_result == ApplyResult::Ok;
      if (sweep) {
        ctx.sweep_changed = ctx.last_result == ApplyResult::Ok;
        if (ctx.sweep_changed) {
          sweep_retired_begin = retired_before;
          sweep_substituted_begin = substituted_before;
        } else if (sweep_retired_begin == SIZE_MAX) {
          heuristics_consume_retirements(k);
        }
      } else {
        if (substitute && sweep_retired_begin != SIZE_MAX) {
          // Kissat's post-Sweep Substitute scans one union containing both the
          // representatives proved by Sweep and any older SCC exposed by
          // their binaries.  Sweep has already ordered its own portion;
          // extend that same scan boundary across this mandated follow-up
          // before H consumes either portion at this call boundary.
          sweep_order_retirement_suffix(k, sweep_retired_begin,
                                        sweep_substituted_begin);
          sweep_retired_begin = sweep_substituted_begin = SIZE_MAX;
        }
        if (sweep_retired_begin == SIZE_MAX) {
          // All other Kissat rule-local eliminations are active-flag
          // transitions owned by that call.  Do not let their arbitrary heap
          // removals drift to phase exit.  A productive Sweep is the sole
          // exception: its inline rewrites deliberately wait for the
          // mandatory follow-up Substitute to retire the complete
          // import-ordered union.
          heuristics_consume_retirements(k);
        }
      }
      event_trace_slot_boundary(k, r.name, "end");
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
  if (group == SlotGroup::Eliminate && !ctx.phase_unsat)
    event_trace_eliminate_cleanup(k, "before");
  if (ctx.phase_dosed && !ctx.phase_unsat)
    kernel_collect_garbage(k);
  if (!ctx.phase_unsat)
    kernel_flush_retired_redundant(k);
  if (group == SlotGroup::Eliminate && !ctx.phase_unsat)
    event_trace_eliminate_cleanup(k, "after");
  heuristics_consume_retirements(k);
  kernel_dense_disconnect(k);
  // Kissat leaves every probe/eliminate pipeline by rebuilding sparse large
  // watches.  This is ordering state, not optional garbage collection: in
  // particular it resets the mutations made by unsuccessful early Lucky
  // trials before late Lucky and first search.
  // The full dense-mode exit corresponds to Kissat's arena-order reconnect.
  // Tactix may have physically permuted that arena through its independent
  // CaDiCaL-derived policy; replay the persistent source positions here.  The
  // rule-local Vivify, Congruence, and Factor reconnects keep their own arena
  // contexts and deliberately use the default physical traversal.
  kernel_rebuild_large_watches(k, /*inherited_source_order=*/true);
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
  // `kissat_probe_initially` does not update `last.ticks.probe`; the first
  // search-time Probe phase therefore measures effort from search tick zero.
  // Only a completed post-search Probe phase advances that anchor.
  if (group != SlotGroup::Probe || k.stats.conflicts)
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
  const uint64_t delta = scale_permille(reference, e.permille);
  e.limit = e.meter(k) + delta;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  e.journal_reference = reference;
#endif
  if (std::getenv("TACTIX_SLOT_TRACE"))
    std::fprintf(stderr,
                 "[slot-limit] %s now=%llu anchor=%llu reference=%llu "
                 "permille=%u delta=%llu meter=%llu\n",
                 rules[e.rule].name, (unsigned long long)now,
                 (unsigned long long)clock.anchor,
                 (unsigned long long)reference, e.permille,
                 (unsigned long long)delta,
                 (unsigned long long)e.meter(k));
  if (e.meter == meter_forward_pair) {
    if (forward_pair_limit_)
      e.limit = forward_pair_limit_;
    else
      forward_pair_limit_ = e.limit;
  }
  // Kissat's `factoriniticks` belongs to the first factorization invocation,
  // including the preprocessing call from `probe_initially`.  Restricting it
  // to post-search phases silently replaced the shipped 700M first dose with
  // 50 permille of the 10M preprocessing reference (only 0.5M ticks).
  if (e.doses == 0 && e.budget_aware &&
      !std::strcmp(rules[e.rule].name, "Factor"))
    e.limit = e.meter(k) + factor_init_ticks_;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  const uint64_t entry = e.meter(k);
  e.journal_allowance = e.limit >= entry ? e.limit - entry : 0;
#endif
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
        t.dec_budget + t.dec_delay + t.dec_material + t.dec_warmup +
        t.dec_profile;
    const char *balance = (t.admitted + declined == t.occasions) ? "" : "  ***UNACCOUNTED***";
    std::printf(
        "c slot %-18s occ %6llu adm %5llu ok %5llu | dec dis %llu cap %llu "
        "pre %llu grd %llu bud %llu dly %llu mat %llu warm %llu prof %llu "
        "| handed %llu spent %llu completed %llu | "
        "wrapped %llu%s\n",
        rules[e.rule].name, (unsigned long long)t.occasions,
        (unsigned long long)t.admitted, (unsigned long long)t.ok,
        (unsigned long long)t.dec_disabled, (unsigned long long)t.dec_cap,
        (unsigned long long)t.dec_preprocess, (unsigned long long)t.dec_guard,
        (unsigned long long)t.dec_budget, (unsigned long long)t.dec_delay,
        (unsigned long long)t.dec_material, (unsigned long long)t.dec_warmup,
        (unsigned long long)t.dec_profile,
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
