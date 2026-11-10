/* The CDCL search and its controllers: DefaultPolicy's restart, mode,
 * database, phase and branching sub-strategies, conflict analysis and the
 * macro events that end a search interval. */
#include "strategy.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* kimits.c kissat_scale_delta: log10(C + 1 + offset) with C the irredundant
 * clauses (DefaultPolicy scale_inprocessing_delta). */
uint64_t ds_scale_inprocessing_delta(strategy *t, uint64_t delta) {
  const double f =
      log10((double)stat(t, TACTIX_STAT_IRREDUNDANT_CLAUSES) + 1.0 + t->set.inprocess.logarithm_offset) -
      t->set.inprocess.scale_log_shift;
  return (uint64_t)((t->set.inprocess.scale_quadratic * f * f + t->set.inprocess.scale_constant) *
                    (double)delta);
}

/* ------------------------------------------------------------------------ */
/* DefaultRestartControl, DefaultModeControl, DefaultDatabaseControl and    */
/* DefaultPhaseControl (default_restart.hpp, default_periodic.hpp).         */

static void smooth_init(smooth *s, double window) {
  s->alpha = 1.0 / window;
  s->beta = 1.0 - s->alpha;
  s->value = s->biased = 0;
  s->exp = 1.0;
}

static void smooth_update(smooth *s, double y) {
  s->biased += s->alpha * (y - s->biased);
  if (!s->exp) {
    s->value = s->biased;
    return;
  }
  const double old = s->exp;
  s->exp *= s->beta;
  if (s->exp == old) {
    s->exp = 0;
    s->value = s->biased;
  } else {
    s->value = s->biased / (1.0 - s->exp);
  }
}

/* kissat init_averages: lazily, once per mode. */
static mode_averages *averages(strategy *t) {
  mode_averages *avg = &t->averages[t->stable];
  if (!avg->initialized) {
    smooth_init(&avg->fast_glue, t->set.restart.fast_ema_window);
    smooth_init(&avg->slow_glue, t->set.restart.slow_ema_window);
    smooth_init(&avg->decision_rate, t->set.restart.slow_ema_window);
    avg->initialized = 1;
  }
  return avg;
}

static void observe_glue(strategy *t, uint32_t glue) {
  mode_averages *avg = averages(t);
  smooth_update(&avg->fast_glue, glue);
  smooth_update(&avg->slow_glue, glue);
}

static void observe_decision_rate(strategy *t) {
  mode_averages *avg = averages(t);
  const uint64_t now = *t->decisions, before = avg->saved_decisions;
  avg->saved_decisions = now;
  smooth_update(&avg->decision_rate, (double)(now - before));
}

void ds_reset_reluctant(strategy *t) {
  t->reluctant.u = t->reluctant.v = 1;
  t->reluctant.wait = t->set.restart.reluctant_period;
  t->reluctant.trigger = 0;
}

/* kissat reluctant.h: Luby-style doubling per learned clause while stable. */
static void tick_reluctant(strategy *t) {
  if (t->reluctant.trigger || --t->reluctant.wait) return;
  uint64_t u = t->reluctant.u, v = t->reluctant.v;
  if ((u & -u) == v) {
    u++;
    v = 1;
  } else {
    v *= 2;
  }
  uint64_t wait = v * t->set.restart.reluctant_period;
  if (wait > t->set.restart.reluctant_limit) {
    u = v = 1;
    wait = t->set.restart.reluctant_period;
  }
  t->reluctant.trigger = 1;
  t->reluctant.wait = wait;
  t->reluctant.u = u;
  t->reluctant.v = v;
}

static int want_restart(strategy *t) {
  if (*t->conflicts < t->restart_next_conflicts) return 0;
  if (t->stable) return t->reluctant.trigger;
  return t->set.restart.glue_margin * t->averages[0].slow_glue.value <=
         t->averages[0].fast_glue.value;
}

void ds_update_restart_limit(strategy *t) {
  uint64_t delta = t->set.restart.initial_interval;
  if (*t->restarts)
    delta += (uint64_t)(log10((double)*t->restarts + t->set.restart.logarithm_offset) -
                        t->set.restart.logarithm_bias);
  t->restart_next_conflicts = *t->conflicts + delta;
}

static int want_switch_mode(strategy *t) {
  return t->mode.count & 1 ? *t->search_ticks >= t->mode.ticks_limit
                           : *t->conflicts >= t->mode.conflicts_limit;
}

static void update_mode_limits(strategy *t) {
  if (t->mode.count & 1) {
    t->mode.ticks_limit = *t->search_ticks + (*t->search_ticks - t->mode.ticks_at_focus_entry);
    return;
  }
  const uint64_t cycle = t->mode.count / 2 ? t->mode.count / 2 : 1;
  const double scale = pow(log10((double)cycle + t->set.mode.logarithm_offset),
                           (double)t->set.mode.logarithm_power);
  t->mode.conflicts_limit =
      *t->conflicts + (uint64_t)((double)t->set.mode.interval * (double)cycle * scale);
  t->mode.ticks_at_focus_entry = *t->search_ticks;
  ds_update_restart_limit(t);
}

static void update_reduce_limit(strategy *t) {
  const settings *c = &t->set;
  if (c->reduction.square_root_cadence) {
    const double n = (double)(*t->reductions ? *t->reductions : 1);
    t->reduce_next_conflicts =
        *t->conflicts + (uint64_t)((double)c->reduction.initial_interval * sqrt(n));
  } else {
    t->reduce_next_conflicts = *t->conflicts + c->reduction.arithmetic_interval +
                               c->reduction.arithmetic_step * *t->reductions;
  }
}

static int want_rephase(strategy *t) {
  return (!t->set.rephase.stable_only || t->stable) &&
         *t->conflicts > t->rephase.next_conflicts;
}

static void update_rephase_limit(strategy *t) {
  const double n = (double)t->rephase.count;
  const double lg = log10(n + t->set.rephase.logarithm_offset);
  t->rephase.next_conflicts =
      *t->conflicts + (uint64_t)((double)t->set.rephase.interval * n *
                                 pow(lg, (double)t->set.rephase.logarithm_power));
}

static int want_reorder(strategy *t) {
  if (!t->set.reorder.mode || (!t->stable && t->set.reorder.mode < 2)) return 0;
  return stat(t, TACTIX_STAT_LEVEL) == 0 && *t->conflicts >= t->reorder.next_conflicts;
}

static void update_reorder_limit(strategy *t) {
  t->reorder.next_conflicts = *t->conflicts + t->set.reorder.interval * t->reorder.count;
}

/* ------------------------------------------------------------------------ */
/* DefaultBranching: Kissat's random decision sequences and the focused     */
/* phase rotation over H's ranking.                                          */

/* Kissat random.h's 64-bit LCG (the generator, not a strategy setting). */
#define RANDEC_MULTIPLIER UINT64_C(6364136223846793005)
#define RANDEC_INCREMENT UINT64_C(1442695040888963407)

static void start_random_sequence(strategy *t) {
  if (t->stable || t->randec_remaining) return;
  t->randec_sequences++;
  const double logn = log10((double)t->randec_sequences + t->set.branching.logarithm_offset);
  t->randec_remaining = (uint64_t)((double)t->set.branching.random_sequence_length * logn);
  t->randec_next =
      *t->conflicts + (uint64_t)((double)t->set.branching.random_initial_interval * logn);
}

/* The focused-mode forced polarity: +1, -1, or 0 for H's phase. */
static int forced_phase(strategy *t) {
  const settings *c = &t->set;
  if (t->stable || !c->branching.focused_phase_switch_span || !c->branching.focused_phases)
    return 0;
  const uint64_t epoch = stat(t, TACTIX_STAT_SWITCHES) / c->branching.focused_phase_switch_span;
  if (t->focused_phase_epoch != epoch) {
    t->focused_phase_epoch = epoch;
    t->focused_phase = c->branching.focused_phase[epoch % c->branching.focused_phases];
  }
  return t->focused_phase;
}

static int decide_literal(strategy *t, int variable) {
  const int phase = forced_phase(t);
  return phase ? (phase < 0 ? -variable : variable) : tactix_branch_phase(t->s, variable);
}

/* DefaultBranching::pick_impl. A restart's reuse query is Kissat's direct
 * next_decision_variable call, which sees the retained level unincremented,
 * so it may start a sequence at levels zero or one. Random sequences are
 * always on: DefaultPolicy's randec_enabled member overrides
 * branching.random_sequences. */
static int pick(strategy *t, int restart_reuse) {
  if (!t->stable) {
    const uint64_t level = stat(t, TACTIX_STAT_LEVEL);
    if (!t->randec_remaining && (restart_reuse ? level <= 1 : level == 0) &&
        *t->conflicts >= t->randec_next)
      start_random_sequence(t);
    if (t->randec_remaining) {
      const uint64_t population = observe(t, TACTIX_OBSERVE_BRANCH_POPULATION);
      for (;;) {
        t->randec_rng = t->randec_rng * RANDEC_MULTIPLIER + RANDEC_INCREMENT;
        const uint32_t random = (uint32_t)(t->randec_rng >> 32);
        const int variable = tactix_branch_population(t->s, (uint32_t)(random % population));
        if (tactix_observe(t->s, TACTIX_OBSERVE_VARIABLE_OPEN, (uint64_t)variable) == 1)
          return decide_literal(t, variable);
      }
    }
  }
  return decide_literal(t, tactix_branch_next(t->s));
}

/* ------------------------------------------------------------------------ */
/* Profiles.                                                                 */

tactix_online_parameters ds_read_profile(strategy *t, int rule_id, int kind, int source) {
  tactix_online_parameters profile;
  if (tactix_read_parameters(t->s, rule_id, kind, source, &profile)) {
    fprintf(stderr, "tactix-default: no profile %d for %s\n", kind, tactix_rule_name(rule_id));
    abort();
  }
  return profile;
}

void ds_install(strategy *t, int rule_id, const tactix_online_parameters *profile) {
  if (tactix_install_parameters(t->s, rule_id, profile)) {
    fprintf(stderr, "tactix-default: %s refused profile %d\n", tactix_rule_name(rule_id),
            profile->kind);
    abort();
  }
}

/* Install the profile the configuration poses for a family. */
void ds_install_configured(strategy *t, const char *rule_name, int kind) {
  const int id = ds_rule(rule_name);
  const tactix_online_parameters profile = ds_read_profile(t, id, kind, TACTIX_PARAMETERS_STRATEGY);
  ds_install(t, id, &profile);
}

/* pose_tiers: Reduce's tier1/tier2 profile from the retier snapshots. */
static void pose_tiers(strategy *t, uint64_t tier1_mode, uint64_t tier2_mode) {
  tactix_online_parameters reduce =
      ds_read_profile(t, t->reduce_rule, TACTIX_ONLINE_PARAMETERS_REDUCE, TACTIX_PARAMETERS_INSTALLED);
  reduce.value.reduce.tier1 = t->retier.tier1_by_mode[tier1_mode];
  reduce.value.reduce.tier2 = t->retier.tier2_by_mode[tier2_mode] > reduce.value.reduce.tier1
                                  ? t->retier.tier2_by_mode[tier2_mode]
                                  : reduce.value.reduce.tier1;
  ds_install(t, t->reduce_rule, &reduce);
}

/* Kissat 4.0.4 Reduce: focused tier-1, stable tier-2 (pose_reduce_tiers). */
static void pose_reduce_tiers(strategy *t) {
  pose_tiers(t, t->set.reduction.reduce_tier1_mode, t->set.reduction.reduce_tier2_mode);
}
void ds_pose_vivify_tiers(strategy *t) {
  pose_tiers(t, t->set.inprocess.vivify_tier_mode, t->set.inprocess.vivify_tier_mode);
}

/* reduce_retier: the current mode's tier1/tier2 from the used-glue
 * histogram's relative quantiles (the defaults when it is empty). */
static void retier_current_mode(strategy *t) {
  const uint32_t length = tactix_counter_length(t->used_glue) / 2;
  const uint32_t mode = (uint32_t)t->stable;
  uint64_t total = 0;
  for (uint32_t g = 0; g < length; ++g) total += tactix_counter(t->s, t->used_glue, mode * length + g);
  if (!total) {
    t->retier.tier1_by_mode[mode] = t->retier.tier1_default;
    t->retier.tier2_by_mode[mode] = t->retier.tier2_default > t->retier.tier1_default
                                        ? t->retier.tier2_default
                                        : t->retier.tier1_default;
    return;
  }
  const uint64_t lim1 = total * t->retier.tier1_relative_permille / PERMILLE;
  const uint64_t lim2 = total * t->retier.tier2_relative_permille / PERMILLE;
  uint64_t acc = 0;
  uint32_t t1 = 0;
  for (uint32_t g = 0;; ++g) {
    acc += tactix_counter(t->s, t->used_glue, mode * length + g);
    if (acc >= lim1 || g == length - 1) {
      t1 = g;
      break;
    }
  }
  uint32_t t2 = t1;
  if (acc < lim2)
    for (uint32_t g = t1 + 1; g < length; ++g) {
      acc += tactix_counter(t->s, t->used_glue, mode * length + g);
      if (acc >= lim2) {
        t2 = g;
        break;
      }
    }
  t->retier.tier1_by_mode[mode] = t1;
  t->retier.tier2_by_mode[mode] = t2;
}

/* update_formula_classification: Kissat classify.c's `bigbig` gates fast
 * binary assignment through the installed ConflictAnalysis profile. Kissat's
 * percent/permille unit mismatch is preserved: 990 means 0.99 %. */
void ds_update_formula_classification(strategy *t) {
  const uint64_t binaries = observe(t, TACTIX_OBSERVE_LIVE_BINARY);
  const uint64_t clauses = binaries + observe(t, TACTIX_OBSERVE_LIVE_LARGE_IRREDUNDANT);
  int bigbig = 0;
  if (clauses > t->set.analysis.small_clause_limit)
    bigbig = (wide)100000 * binaries >= (wide)t->set.analysis.big_binary_fraction_permille * clauses;
  t->analysis_profile.value.analyze.enable_jump_reasons_bigbig = bigbig;
  ds_install(t, t->conflict_analysis, &t->analysis_profile);
}

/* ------------------------------------------------------------------------ */
/* Actions.                                                                  */

/* default_propagate. The public guard refuses a propagation fixpoint, where
 * the native service still consumes the root-prefix marker at level zero. */
int ds_propagate(strategy *t) {
  const int code = tactix_apply(t->s, t->propagate);
  if (code != REFUSED) return code;
  if (observe(t, TACTIX_OBSERVE_INCONSISTENT)) return UNSAT;
  if (!stat(t, TACTIX_STAT_LEVEL)) support(t, TACTIX_SUPPORT_CONSUME_ROOT_PREFIX, 0);
  return OK;
}

/* search_decide: the branching pick, then Decide (whose NATIVE publication
 * records the target note). */
static void decide(strategy *t) { (void)tactix_decide(t->s, pick(t, 0)); }

/* ReasonBumpDelay::reached_gate (kissat DELAYING(bumpreasons)). */
static void reason_bump_gate(strategy *t, int ran, int aborted) {
  if (t->bump_delay.count) {
    --t->bump_delay.count;
    return;
  }
  if (!ran) return;
  if (aborted)
    t->bump_delay.current += t->bump_delay.current < UINT32_MAX;
  else if (t->bump_delay.current)
    t->bump_delay.current /= 2;
  t->bump_delay.count = t->bump_delay.current;
}

static uint32_t want_bump_reasons(strategy *t) {
  const uint64_t limit = t->set.analysis.bump_reason_limit;
  if (!limit || t->bump_delay.count ||
      t->averages[t->stable].decision_rate.value >= t->set.analysis.bump_reason_rate)
    return 0;
  return (uint32_t)limit;
}

/* Enter the search interval's analysis setting: the policy's retier
 * schedule and the payload for `bump_limit`. */
static void install_search_analysis(strategy *t, uint32_t bump_limit) {
  if (t->in_search_interval && t->installed_bump_limit == bump_limit) return;
  tactix_online_parameters payload = t->analysis_profile;
  payload.value.analyze.bump_reason_limit = bump_limit;
  payload.value.analyze.bump_reason_depth = (uint32_t)t->set.analysis.reason_depth;
  ds_install(t, t->conflict_analysis, &payload);
  if (!t->in_search_interval) tactix_install_retier_schedule(t->s, &t->retier);
  t->in_search_interval = 1;
  t->installed_bump_limit = bump_limit;
}

/* Leave the search interval: read the schedule the analyses advanced back,
 * and restore the neutral profile and the idle schedule. */
void ds_end_search_interval(strategy *t) {
  if (!t->in_search_interval) return;
  tactix_read_retier_schedule(t->s, &t->retier);
  ds_install(t, t->conflict_analysis, &t->analysis_profile);
  tactix_install_retier_schedule(t->s, &t->idle_retier);
  t->in_search_interval = 0;
}

/* The native search conflict: ConflictAnalysis' chronological target under
 * the per-conflict bump payload and the policy-owned retier schedule, then
 * the policy's observation of the completed transaction. */
static int analyze_conflict(strategy *t) {
  if (t->randec_remaining) --t->randec_remaining; /* on_search_propagation_conflict */
  observe_decision_rate(t);
  install_search_analysis(t, want_bump_reasons(t));
  const uint64_t learned = *t->learned, glue_sum = *t->learned_glue_sum;
  const uint64_t backjumps = *t->backjumps;
  const uint64_t rounds = *t->reason_bump_rounds, aborted = *t->reason_bump_aborted;
  const int code =
      tactix_apply_semantic_instance(t->s, t->conflict_analysis, TACTIX_ANALYZE_TARGET_CHRONO);
  if (code == REFUSED) return ds_unported(t, "refused search ConflictAnalysis");
  if (code == UNSAT) return UNSAT;
  /* A learned result (a backjumped clause or a level-one unit batch) feeds
   * the glue averages and the reluctant clock; only a counted backjump
   * reaches Kissat's bump-reasons delay. */
  if (*t->learned != learned) {
    if (*t->backjumps != backjumps)
      reason_bump_gate(t, *t->reason_bump_rounds != rounds, *t->reason_bump_aborted != aborted);
    observe_glue(t, (uint32_t)(*t->learned_glue_sum - glue_sum));
    if (t->stable) tick_reluctant(t);
  }
  return OK;
}

/* GlueTieredReduction::victims: Kissat's reducelow..reducehigh ramp. */
static uint64_t ramped_victims(strategy *t, uint64_t candidates) {
  const double low = t->set.reduction.low_percent, high = t->set.reduction.high_percent;
  if (low >= high) return (uint64_t)((double)candidates * low / PERCENT);
  const double r = (double)*t->reductions + 1.0;
  const double percent = high - (high - low) / log10(r + t->set.reduction.logarithm_offset);
  return (uint64_t)((double)candidates * percent / PERCENT);
}

static int reduce(strategy *t) {
  const settings *c = &t->set;
  if (support(t, TACTIX_SUPPORT_REDUCTION_PREPARATION, 0) == UNSAT) return UNSAT;
  if (c->reduction.dynamic_tiers) retier_current_mode(t);
  pose_reduce_tiers(t);
  /* Kissat samples compactlim before choosing victims. */
  const int compact = c->reduction.compact_variables &&
                      tactix_observe(t->s, TACTIX_OBSERVE_COMPACTION_DUE,
                                     c->reduction.compact_inactive_percent) == 1;
  uint64_t instance = TACTIX_INSTANCE_CANONICAL;
  if (c->reduction.ramp_fraction) {
    const uint64_t candidates =
        tactix_observe(t->s, TACTIX_OBSERVE_REDUCE_CANDIDATES, c->reduction.protect_tiers);
    const uint64_t victims = ramped_victims(t, candidates);
    instance = victims < candidates ? victims : candidates;
  }
  const tactix_online_parameters saved =
      ds_read_profile(t, t->reduce_rule, TACTIX_ONLINE_PARAMETERS_REDUCE, TACTIX_PARAMETERS_INSTALLED);
  tactix_online_parameters selected = saved;
  selected.value.reduce.victim_numerator = (uint32_t)c->reduction.victim_numerator;
  selected.value.reduce.victim_denominator = (uint32_t)c->reduction.victim_denominator;
  selected.value.reduce.tier_protection_mode = setting_mode(c->reduction.protect_tiers);
  selected.value.reduce.compact_collection_mode = setting_mode((uint64_t)compact);
  ds_install(t, t->reduce_rule, &selected);
  (void)tactix_apply_instance(t->s, t->reduce_rule, instance);
  ds_install(t, t->reduce_rule, &saved);
  if (compact)
    support(t, TACTIX_SUPPORT_COMPACT_VARIABLES,
            TACTIX_COMPACT_FOLD_FIXED | TACTIX_COMPACT_PUBLISH_RANDOM_ORDER);
  update_reduce_limit(t);
  ds_pose_vivify_tiers(t);
  ds_update_formula_classification(t);
  t->last_reduce_conflicts = *t->conflicts;
  return OK;
}

static void switch_mode(strategy *t) {
  (void)tactix_apply(t->s, t->switchmode);
  t->mode.count++;
  t->stable = (int)stat(t, TACTIX_STAT_STABLE);
  t->averages[t->stable].saved_decisions = *t->decisions;
  if (t->stable) ds_reset_reluctant(t); /* kissat_init_reluctant */
  update_mode_limits(t);
  start_random_sequence(t); /* kissat mode.c, after every transition */
}

/* A scheduled restart keeping the decision prefix that outranks the next
 * pick (kissat restart.c, which consumes that pick's random draw). */
static int restart(strategy *t) {
  const int next = pick(t, 1);
  const int keep = tactix_branch_restart_level(t->s, next < 0 ? -next : next);
  if ((uint64_t)keep < stat(t, TACTIX_STAT_LEVEL))
    support(t, TACTIX_SUPPORT_CAPTURE_TARGET_PHASES, 0);
  const int code = tactix_apply_instance(t->s, t->restart_rule, (uint64_t)keep);
  t->reluctant.trigger = 0;
  if (!t->stable) ds_update_restart_limit(t); /* focused only */
  if (code == UNSAT) return UNSAT;
  /* The retained root services run only in a natural full restart's root
   * window (graph_labeling_due is consulted twice, as there). */
  if (!stat(t, TACTIX_STAT_LEVEL)) {
    const int affine = t->set.enabled.affine_search && (observe(t, TACTIX_OBSERVE_AFFINE_SESSION) & 2);
    if ((affine || ds_graph_labeling_due(t)) && ds_propagate(t) != OK) return UNSAT;
    if (affine && ds_affine_restart_service(t) == UNSAT) return UNSAT;
    if (ds_graph_labeling_due(t) && ds_run_graph_labeling(t) == SAT) return SAT;
  }
  return OK;
}

static void reorder(strategy *t) {
  const uint64_t target = t->set.reorder.target == 1   ? TACTIX_REORDER_QUEUE
                          : t->set.reorder.target == 2 ? TACTIX_REORDER_HEAP
                                                       : TACTIX_INSTANCE_CANONICAL;
  (void)tactix_apply_instance(t->s, t->reorder_rule, target);
  t->reorder.count++;
  update_reorder_limit(t);
}

/* Kissat rephase.c: backtrack, capture target phases, propagate and flush
 * the root before changing phases. */
static int prepare_rephase_root(strategy *t) {
  return support(t, TACTIX_SUPPORT_ROOT_PREPARATION,
                 TACTIX_ROOT_PREPARATION_CAPTURE_TARGET |
                     TACTIX_ROOT_PREPARATION_CONSUME_ROOT_PREFIX) == UNSAT
             ? UNSAT
             : OK;
}

/* Walk's Kissat SET_EFFORT_LIMIT reference: search ticks since the
 * Eliminate anchor, floored at mineffort. */
static uint64_t ds_walk_effort_reference(strategy *t) {
  const uint64_t now = *t->search_ticks, anchor = t->clocks[ELIMINATE].anchor;
  const uint64_t reference = now >= anchor ? now - anchor : 0;
  return reference < t->set.inprocess.minimum_effort_ticks ? t->set.inprocess.minimum_effort_ticks
                                                           : reference;
}

/* One online Walk step under `walk` (the installed profile with this
 * application's budget and seed). */
static void walk(strategy *t, tactix_online_parameters walk) {
  tactix_online_step_v2 step;
  memset(&step, 0, sizeof step);
  step.base.step_id = 1;
  step.base.rule_id = t->walk;
  step.base.parameters = walk;
  step.base.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
  step.base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.base.repeat_limit = 1;
  step.base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.base.budget_unlimited = 1;
  const tactix_online_program_v2 program = {&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  tactix_online_result result;
  if (tactix_online_configure_v2(t->s, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) ||
      tactix_online_advance(t->s, &result))
    (void)ds_unported(t, "refused Walk step");
}

static tactix_online_parameters walk_profile(strategy *t, uint64_t allowance, uint64_t seed) {
  tactix_online_parameters profile =
      ds_read_profile(t, t->walk, TACTIX_ONLINE_PARAMETERS_WALK, TACTIX_PARAMETERS_INSTALLED);
  profile.value.walk.step_budget = allowance;
  profile.value.walk.random_seed = seed;
  profile.value.walk.seed_candidate = 0;
  return profile;
}

static int rephase(strategy *t) {
  const settings *c = &t->set;
  if (prepare_rephase_root(t) == UNSAT) return UNSAT;
  const uint8_t slot = c->rephase.rotation[t->rephase.count % c->rephase.rotations];
  t->rephase.count++; /* one rephase event, whatever it is composed of */
  if (slot == 1 /* Walk */ && tactix_rule_applicable(t->s, t->walk) == 1) {
    /* Kissat warmup.c: a temporary assignment in the active ranking whose
     * assignment-time phase saving seeds the walker. */
    if (c->rephase.walk_warmup) {
      const int forced = forced_phase(t);
      support(t, TACTIX_SUPPORT_WARMUP,
              forced > 0   ? TACTIX_WARMUP_PHASE_POSITIVE
              : forced < 0 ? TACTIX_WARMUP_PHASE_NEGATIVE
                           : TACTIX_WARMUP_PHASE_HEURISTIC);
    }
    /* Kissat seeds the walker from the search's shared generator. */
    const uint64_t seed = c->rephase.walk_use_branch_seed ? t->randec_rng : c->rephase.walk_random_seed;
    walk(t, walk_profile(t, scale_permille(ds_walk_effort_reference(t), c->rephase.walk_permille), seed));
  } else {
    const uint8_t effective = slot == 1 ? (uint8_t)c->rephase.walk_inapplicable_fallback : slot;
    const uint64_t type = effective == 2   ? TACTIX_REPHASE_INVERTED
                          : effective == 3 ? TACTIX_REPHASE_ORIGINAL
                                           : TACTIX_REPHASE_BEST;
    (void)tactix_apply_instance(t->s, t->rephase_rule, type);
  }
  update_rephase_limit(t);
  return OK;
}

/* The CCAnr-style Walk the complementary-ternary census selects, retried on
 * a geometric back-off. */
static int configuration_checking_walk(strategy *t) {
  const settings *c = &t->set;
  if (prepare_rephase_root(t) == UNSAT) return UNSAT;
  const uint64_t scaled =
      scale_permille(ds_walk_effort_reference(t), c->rephase.configuration_checking_walk_permille);
  const uint64_t allowance = scaled > c->rephase.configuration_checking_minimum_step_budget
                                 ? scaled
                                 : c->rephase.configuration_checking_minimum_step_budget;
  tactix_online_parameters profile =
      walk_profile(t, allowance, c->rephase.configuration_checking_random_seed);
  tactix_online_walk_parameters *w = &profile.value.walk;
  w->algorithm = TACTIX_WALK_ALGORITHM_CONFIGURATION_CHECKING;
  w->initialization = TACTIX_WALK_INITIALIZATION_NEGATIVE;
  w->maximum_tries = (uint32_t)c->rephase.configuration_checking_maximum_tries;
  w->flips_per_try = c->rephase.configuration_checking_flips_per_try;
  w->weight_smoothing_threshold = (uint32_t)c->rephase.configuration_checking_weight_threshold;
  w->smoothing_old_numerator = (uint32_t)c->rephase.configuration_checking_smoothing_old_numerator;
  w->smoothing_denominator = (uint32_t)c->rephase.configuration_checking_smoothing_denominator;
  w->aspiration_mode = setting_mode(c->rephase.configuration_checking_aspiration);
  walk(t, profile);
  /* The first run gets one undisturbed search trajectory before generic
   * preprocessing is released; later retries back off geometrically. */
  if (!t->cc_applications) t->cc_release_conflicts = saturated_sum(*t->conflicts, 1);
  ++t->cc_applications;
  const uint32_t exponent = t->cc_applications - 1 < 6 ? t->cc_applications - 1 : 6;
  t->cc_next_conflicts = saturated_sum(
      *t->conflicts,
      saturated_product(c->rephase.configuration_checking_conflict_interval, UINT64_C(1) << exponent));
  return OK;
}

/* ------------------------------------------------------------------------ */
/* Search.                                                                   */

int ds_search(strategy *t) {
  const settings *c = &t->set;
  for (;;) {
    if (t->unported) return 0;
    const int propagated = ds_propagate(t);
    if (propagated == UNSAT) return UNSAT;
    if (propagated == CONFLICT) {
      if (t->affine.live && (observe(t, TACTIX_OBSERVE_AFFINE_SESSION) & 8))
        return ds_unported(t, "affine reason debt");
      if (analyze_conflict(t) == UNSAT) return UNSAT;
      continue;
    }
    /* The retained matrix observes a completed propagation prefix; a
     * published conditional reason re-arms ordinary propagation. */
    if (ds_affine_search_step(t)) continue;
    if (t->affine.live && (observe(t, TACTIX_OBSERVE_AFFINE_SESSION) & 8))
      return ds_unported(t, "affine reason debt");
    if (!observe(t, TACTIX_OBSERVE_UNASSIGNED_ACTIVE)) return SAT;
    if (t->cc_deferred && t->cc_applications && *t->conflicts >= t->cc_release_conflicts)
      t->cc_deferred = 0;
    if (t->cc_walk && *t->conflicts >= t->cc_next_conflicts) {
      ds_end_search_interval(t);
      if (configuration_checking_walk(t) == UNSAT) return UNSAT;
      continue;
    }
    if (*t->conflicts >= t->reduce_next_conflicts &&
        tactix_rule_applicable(t->s, t->reduce_rule) == 1) {
      ds_end_search_interval(t);
      if (reduce(t) == UNSAT) return UNSAT;
      continue;
    }
    const int pinned = c->mode.force == 1 || (c->mode.force == 2 && t->stable);
    if (!pinned && want_switch_mode(t) && tactix_rule_applicable(t->s, t->switchmode) == 1) {
      ds_end_search_interval(t);
      switch_mode(t);
      continue;
    }
    if (want_restart(t) && stat(t, TACTIX_STAT_LEVEL) &&
        tactix_rule_applicable(t->s, t->restart_rule) == 1) {
      const int restarted = restart(t);
      if (restarted == UNSAT || restarted == SAT) return restarted;
      continue;
    }
    if (want_reorder(t) && tactix_rule_applicable(t->s, t->reorder_rule) == 1) {
      ds_end_search_interval(t);
      reorder(t);
      continue;
    }
    if (want_rephase(t) && tactix_rule_applicable(t->s, t->rephase_rule) == 1) {
      ds_end_search_interval(t);
      if (rephase(t) == UNSAT) return UNSAT;
      continue;
    }
    int verdict;
    if (!t->cc_deferred && ds_want_phase(t, PROBE)) {
      ds_end_search_interval(t);
      if ((verdict = ds_run_phase(t, PROBE))) return verdict;
      ds_update_formula_classification(t);
      continue;
    }
    if (!t->cc_deferred && ds_want_phase(t, ELIMINATE)) {
      ds_end_search_interval(t);
      if ((verdict = ds_run_phase(t, ELIMINATE))) return verdict;
      ds_update_formula_classification(t);
      continue;
    }
    if (t->stop && *t->stop) return 0;
    decide(t);
  }
}
