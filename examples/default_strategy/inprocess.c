/* DefaultInprocessScheduler: the Probe and Eliminate phases and their slot
 * program. */
#include "strategy.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* DefaultInprocessScheduler: the Probe and Eliminate phases.               */

/* install_parameters: the configured profiles, and BVE's scheduler-owned
 * limits over the installed caller profile. */
static void install_inprocess_profiles(strategy *t) {
  static const struct {
    const char *rule;
    int kind;
  } configured[] = {
      {"Congruence", TACTIX_ONLINE_PARAMETERS_CONGRUENCE},
      {"Instantiate", TACTIX_ONLINE_PARAMETERS_INSTANTIATE},
      {"Backbone", TACTIX_ONLINE_PARAMETERS_BACKBONE},
      {"BCE", TACTIX_ONLINE_PARAMETERS_BCE},
      {"HyperTernaryRes", TACTIX_ONLINE_PARAMETERS_TERNARY},
      {"Factor", TACTIX_ONLINE_PARAMETERS_FACTOR},
      {"TransitiveReduce", TACTIX_ONLINE_PARAMETERS_TRANSITIVE},
      {"Substitute", TACTIX_ONLINE_PARAMETERS_SUBSTITUTE},
      {"Vivify", TACTIX_ONLINE_PARAMETERS_VIVIFY},
  };
  for (size_t i = 0; i < sizeof configured / sizeof configured[0]; ++i)
    ds_install_configured(t, configured[i].rule, configured[i].kind);
  const settings *c = &t->set;
  const int bve_rule = ds_rule("BVE");
  tactix_online_parameters bve =
      ds_read_profile(t, bve_rule, TACTIX_ONLINE_PARAMETERS_BVE, TACTIX_PARAMETERS_INSTALLED);
  tactix_online_bve_parameters *p = &bve.value.bve;
  p->occurrence_limit = (uint32_t)c->inprocess.bve_occurrence_limit;
  p->clause_size_limit = (uint32_t)c->inprocess.bve_clause_size_limit;
  p->resolvent_count_limit = UINT32_MAX; /* unlimited within a scheduled pass */
  p->xor_arity_limit = (uint32_t)c->inprocess.bve_xor_arity_limit;
  p->resolvent_size_limit = (uint32_t)c->inprocess.bve_resolvent_size_limit;
  p->backward_occurrence_limit = (uint32_t)c->inprocess.bve_backward_occurrence_limit;
  p->definition_budget = c->inprocess.bve_definition_budget;
  p->definition_cores = (uint32_t)c->inprocess.bve_definition_cores;
  p->definition_core_budget_multiplier = (uint32_t)c->inprocess.bve_definition_core_budget_multiplier;
  p->backward = (int)c->inprocess.bve_backward;
  p->on_the_fly_strengthening = (int)c->inprocess.bve_on_the_fly_strengthening;
  ds_install(t, bve_rule, &bve);
}

/* ------------------------------------------------------------------------ */
/* Effort meters and productivity delays (DefaultPolicy's own tables).      */

static uint64_t meter(const strategy *t, const slot_entry *e) { return tactix_strategy_meter(t->s, e->rule); }

/* A productivity delay (Kissat's named DELAY): yield per denominator. */
typedef struct {
  uint64_t yield, denominator;
} productivity;

static productivity rule_productivity(strategy *t, const slot_entry *e) {
  productivity p = {0, 0};
  tactix_strategy_productivity(t->s, e->rule, &p.yield, &p.denominator);
  return p;
}

/* update_inprocess_delay: double-up on an unproductive call, halve otherwise. */
static void update_delay(slot_entry *e, int unproductive) {
  if (unproductive)
    e->delay_current += e->delay_current < UINT32_MAX;
  else
    e->delay_current /= 2;
  e->delay_count = e->delay_current;
}

/* The ownership record's predicates (StructuredFastelOwnership). */
static int directly_owns(const fastel_ownership *o, uint32_t external) {
  return external >= o->external_begin && external < o->external_end;
}
static int factor_owns(const fastel_ownership *o, uint32_t external) {
  for (size_t i = 0; i < o->factor_range_count; ++i)
    if (external >= o->factor_ranges[i][0] && external < o->factor_ranges[i][1]) return 1;
  return 0;
}
static int protects_at_application(const fastel_ownership *o, uint32_t external) {
  return o->symmetry_applied &&
         (directly_owns(o, external) || (o->grew_root_trail && factor_owns(o, external)));
}

/* A startup Factor call after a structured run that applied symmetry couples
 * its fresh definitions to that representation. */
static void record_factor_range(strategy *t, uint32_t begin, uint32_t end) {
  fastel_ownership *o = &t->ownership;
  if (end <= begin) return;
  if (o->factor_range_count && o->factor_ranges[o->factor_range_count - 1][1] == begin) {
    o->factor_ranges[o->factor_range_count - 1][1] = end;
    return;
  }
  if (o->factor_range_count == o->factor_range_capacity) {
    o->factor_range_capacity = o->factor_range_capacity ? 2 * o->factor_range_capacity : 4;
    o->factor_ranges = realloc(o->factor_ranges, o->factor_range_capacity * sizeof *o->factor_ranges);
    if (!o->factor_ranges) abort();
  }
  o->factor_ranges[o->factor_range_count][0] = begin;
  o->factor_ranges[o->factor_range_count][1] = end;
  o->factor_range_count++;
}

static uint32_t external_extent(strategy *t) {
  return (uint32_t)observe(t, TACTIX_OBSERVE_EXTERNAL_VARIABLES);
}

/* ------------------------------------------------------------------------ */
/* Scoped session settings around one slot application.                     */

/* A work-settings scope (WorkSettingsScope): install `request`, then restore
 * the named fields, clearing budget/offset/bounded when `clear`. */
typedef struct {
  tactix_work_settings saved;
  uint32_t fields;
  int clear;
} work_scope;

static work_scope work_enter(strategy *t, tactix_work_settings request, int clear) {
  work_scope scope;
  tactix_read_work_settings(t->s, &scope.saved);
  scope.fields = request.fields;
  scope.clear = clear;
  if (request.fields) tactix_install_work_settings(t->s, &request);
  return scope;
}

static void work_leave(strategy *t, const work_scope *scope) {
  tactix_work_settings restore;
  memset(&restore, 0, sizeof restore);
  restore.fields = scope->fields;
  restore.budget = scope->clear ? 0 : scope->saved.budget;
  restore.offset = scope->clear ? 0 : scope->saved.offset;
  restore.bounded = scope->clear ? 0 : scope->saved.bounded;
  restore.midround = scope->saved.midround;
  if (restore.fields) tactix_install_work_settings(t->s, &restore);
}

static tactix_work_settings work_budget(uint64_t budget) {
  tactix_work_settings request;
  memset(&request, 0, sizeof request);
  request.fields = TACTIX_WORK_BUDGET;
  request.budget = budget;
  return request;
}

/* A slot's tick attribution (TickSinkScope) around one application. */
static int apply_with_ticks(strategy *t, const slot_entry *e, uint64_t instance) {
  if (e->tick_stat != TACTIX_TICK_ATTRIBUTION_NONE) tactix_install_tick_attribution(t->s, e->tick_stat);
  const int code = instance == TACTIX_INSTANCE_CANONICAL
                       ? tactix_apply(t->s, e->rule)
                       : tactix_apply_instance(t->s, e->rule, instance);
  if (e->tick_stat != TACTIX_TICK_ATTRIBUTION_NONE) tactix_install_tick_attribution(t->s, TACTIX_TICK_ATTRIBUTION_NONE);
  return code;
}

/* A rule's own result (rule_result) as a tactix_apply code. */
static int apply_code(int rule_result) {
  switch (rule_result) {
  case TACTIX_RULE_RESULT_OK: return OK;
  case TACTIX_RULE_RESULT_CONFLICT: return CONFLICT;
  case TACTIX_RULE_RESULT_SAT: return SAT;
  case TACTIX_RULE_RESULT_UNSAT: return UNSAT;
  default: return NOOP;
  }
}

/* One online step of `rule` under `parameters`, the current work budget and
 * material settings (the batch's exact limits), as one canonical
 * application. `complete` receives whether its selection completed. */
static int online_step(strategy *t, int rule, const tactix_online_parameters *parameters,
                       int *complete) {
  tactix_work_settings work;
  tactix_read_work_settings(t->s, &work);
  const int bounded = work.bounded || work.budget;
  tactix_online_step_v2 step;
  memset(&step, 0, sizeof step);
  step.base.step_id = 1;
  step.base.rule_id = rule;
  step.base.parameters = *parameters;
  step.base.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
  step.base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.base.repeat_limit = 1;
  step.base.rule_budget_mode = bounded ? TACTIX_ONLINE_RULE_BUDGET_BOUNDED
                                       : TACTIX_ONLINE_RULE_BUDGET_UNLIMITED;
  step.base.rule_budget = work.budget;
  step.base.touch_filter_mode = setting_mode(tactix_config(t->s, TACTIX_CFG_TOUCH_FILTER));
  step.base.likely_kept_redundant_mode =
      setting_mode(tactix_config(t->s, TACTIX_CFG_LIKELY_KEPT_REDUNDANT));
  step.base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.base.budget_unlimited = 1;
  const tactix_online_program_v2 program = {&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  tactix_online_result result;
  if (tactix_online_configure_v2(t->s, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) ||
      tactix_online_advance(t->s, &result))
    return ds_unported(t, "refused online slot step");
  if (complete) *complete = result.selection_complete;
  return apply_code(result.rule_result);
}

/* Retirement publication at a slot boundary; a productive Sweep's hold waits
 * for its follow-up Substitute's import-ordered union. */
static void publish_retirements(strategy *t) {
  support(t, TACTIX_SUPPORT_HOLD_RETIREMENTS, TACTIX_RETIREMENTS_RELEASE);
  t->retirements_held = 0;
}

static void release_held_retirements(strategy *t) {
  if (!t->retirements_held) return;
  support(t, TACTIX_SUPPORT_HOLD_RETIREMENTS, TACTIX_RETIREMENTS_RELEASE_ONLY);
  t->retirements_held = 0;
}

/* The Substitute rounds of one call: Kissat's per-call SCC closure cap. */
static int substitute_rounds(strategy *t, const slot_entry *e, uint32_t rounds, int effort_ceiling,
                             int *round_budget_break) {
  tactix_online_parameters profile = ds_read_profile(
      t, e->rule, TACTIX_ONLINE_PARAMETERS_SUBSTITUTE, TACTIX_PARAMETERS_INSTALLED);
  profile.value.substitute.max_rounds = rounds;
  if (effort_ceiling >= 0) profile.value.substitute.round_effort_ceiling = effort_ceiling;
  release_held_retirements(t);
  int complete = 1;
  const int code = online_step(t, e->rule, &profile, &complete);
  if (round_budget_break) *round_budget_break = !complete;
  return code;
}

/* ------------------------------------------------------------------------ */
/* The slot program.                                                         */

static void add_entry(strategy *t, const tactix_strategy_step *step) {
  const settings *c = &t->set;
  slot_entry *e = &t->entries[t->entry_count++];
  memset(e, 0, sizeof *e);
  e->rule = step->rule_id;
  e->name = tactix_rule_name(step->rule_id);
  const char *n = e->name;
  e->enabled = step->enabled && !(!c->inprocess.bve_enabled && !strcmp(n, "BVE")) &&
               !(!c->inprocess.vivify_enabled && !strcmp(n, "Vivify")) &&
               !(!c->inprocess.factor_enabled && !strcmp(n, "Factor")) &&
               !(!c->inprocess.sweep_enabled && !strcmp(n, "Sweep"));
  e->step_id = step->id;
  e->defer_preprocess = !strcmp(n, "BVE");
  /* Kissat transitive.c's activity-sorted stack; BVE's score heap. */
  e->order = !strcmp(n, "TransitiveReduce") || !strcmp(n, "BVE") ? TACTIX_CANDIDATE_ORDER_HEURISTIC
                                                                 : TACTIX_CANDIDATE_ORDER_NATIVE;
  e->group = step->phase ? ELIMINATE : PROBE;
  e->preprocess = step->preprocessing;
  e->followup = step->followup;
  e->parameters = step->parameters;
  e->max_applies = step->max_applications;
  if (step->candidate_order >= 0) e->order = step->candidate_order;
  tactix_strategy_rule r;
  if (tactix_strategy_read_rule(t->s, e->rule, &r)) abort();
  e->instances = r.instances;
  e->metered = r.metered;
  e->dosed = r.dosed;
  e->budget_aware = r.budget_aware;
  e->shared_meter = r.shared_meter;
  e->tick_stat = r.tick_stat;
  e->permille = r.effort_permille;
  if (r.metered && step->effort_permille >= 0) e->permille = (uint32_t)step->effort_permille;
  e->delayed = r.delayed;
  e->delay_delta = r.delay_delta;
  e->delay_permille = r.delay_permille;
  if (e->dosed) e->cursor_enabled = strcmp(n, "BVE") != 0;
  if (step->cursor >= 0) e->cursor_enabled = step->cursor;
}

/* A registry rule the configured program does not list, under the reference
 * program's phase for it. */
static void add_unlisted_entry(strategy *t, int rule, int enabled) {
  tactix_strategy_step step, reference;
  memset(&step, 0, sizeof step);
  step.id = (uint64_t)t->entry_count + 1;
  step.rule_id = rule;
  step.enabled = enabled;
  step.effort_permille = step.cursor = step.candidate_order = -1;
  for (int i = 0; !tactix_strategy_read_reference_step(i, &reference); ++i)
    if (reference.rule_id == rule) {
      step.phase = reference.phase;
      break;
    }
  add_entry(t, &step);
}

/* DefaultInprocessScheduler::build's entries: the configured program, then the
 * registry's unlisted inprocessing rules when the program appends them. */
static void build_entries(strategy *t) {
  const int rules = tactix_rule_count();
  t->entries = calloc((size_t)t->set.inprocess.program_steps + (size_t)rules + 1, sizeof *t->entries);
  t->candidate_orders = calloc((size_t)rules, sizeof *t->candidate_orders);
  int *placed = calloc((size_t)rules, sizeof *placed);
  if (!t->entries || !t->candidate_orders || !placed) abort();
  for (int i = 0; i < t->set.inprocess.program_steps; ++i) {
    tactix_strategy_step step;
    if (tactix_strategy_read_step(t->s, i, &step)) abort();
    placed[step.rule_id] = 1;
    add_entry(t, &step);
  }
  if (t->set.inprocess.append_unlisted_rules)
    for (int id = 0; id < rules; ++id) {
      tactix_strategy_rule r;
      if (!placed[id] && !tactix_strategy_read_rule(t->s, id, &r) && r.appended)
        add_unlisted_entry(t, id, r.appended_enabled);
    }
  free(placed);
  /* The candidate order of a rule is its last occurrence's. */
  for (int i = 0; i < t->entry_count; ++i) t->candidate_orders[t->entries[i].rule] = t->entries[i].order;
}

void ds_inprocess_reset(strategy *t) {
  t->inprocess_initialized = t->search_limits_initialized = t->structured_consumed = 0;
  t->last_reduce_conflicts = UINT64_MAX;
  memset(t->clocks, 0, sizeof t->clocks);
  t->bve_rounds_run = t->bve_level_index = 0;
  t->bve_level = 0;
  t->factor_large_formula = 0;
  t->factor_marked_watermark = 0;
  t->forward_pair_limit = 0;
  for (int i = 0; i < t->entry_count; ++i) {
    slot_entry *e = &t->entries[i];
    e->limit = e->cursor = 0;
    e->applies = e->doses = e->delay_count = e->delay_current = 0;
  }
}

/* The BVE growth-bound ladder (DefaultBveBoundRamp). */
static uint32_t bve_passes(const settings *c) {
  uint32_t n = 0;
  for (uint32_t i = 0; i < c->inprocess.bve_bounds; ++i)
    if (c->inprocess.bve_bound[i] <= c->inprocess.bve_default_ceiling) n = i + 1;
  return n ? n : 1;
}
static uint64_t bve_bound(const settings *c, uint32_t pass) {
  return c->inprocess.bve_bound[pass < c->inprocess.bve_bounds ? pass : c->inprocess.bve_bounds - 1];
}

/* A preprocessing follow-up runs after a changed Congruence, a changed Sweep,
 * or (AfterSweepOrFinal) when neither changed anything. */
static int followup_admissible(strategy *t, const slot_entry *e, int congruence_changed,
                               int sweep_changed) {
  if (*t->conflicts || e->followup == TACTIX_FOLLOWUP_NONE || !t->set.inprocess.preprocess_followups)
    return 1;
  if (e->followup == TACTIX_FOLLOWUP_AFTER_CONGRUENCE) return congruence_changed;
  return sweep_changed || !congruence_changed;
}

static int slot_entry_admissible(strategy *t, slot_entry *e, int phase_unsat) {
  if (!e->enabled || phase_unsat) return 0;
  /* FastEl's BVE runs before the first conflict, where BVE otherwise waits. */
  const int preprocessing = !*t->conflicts && !t->fastel_active;
  if ((preprocessing && !e->preprocess) || (e->max_applies && e->applies >= e->max_applies) ||
      (e->defer_preprocess && preprocessing))
    return 0;
  e->applies++;
  return 1;
}

static uint64_t ledger_remaining(const strategy *t, const slot_entry *e, uint64_t limit) {
  const uint64_t spent = meter(t, e);
  return limit >= spent ? limit - spent : 0;
}

/* Kissat SET_EFFORT_LIMIT: the entry's share of the search ticks since its
 * phase's anchor (the preprocessing reference before the first conflict,
 * floored at mineffort, Substitute unfloored). */
static void set_entry_limit(strategy *t, slot_entry *e) {
  const settings *c = &t->set;
  const uint64_t now = *t->search_ticks, anchor = t->clocks[e->group].anchor;
  const int substitute = !strcmp(e->name, "Substitute");
  uint64_t reference = now >= anchor ? now - anchor : 0;
  if (!*t->conflicts)
    reference = c->inprocess.preprocess_initial_ticks;
  else if (!substitute && reference < c->inprocess.minimum_effort_ticks)
    reference = c->inprocess.minimum_effort_ticks;
  const uint64_t permille =
      t->fastel_active ? c->inprocess.initial_fast_elimination_bve_permille : e->permille;
  e->limit = meter(t, e) + scale_permille(reference, permille);
  /* One forward-steps ceiling per elimination round, shared by C1 and C2. */
  if (e->shared_meter) {
    if (t->forward_pair_limit)
      e->limit = t->forward_pair_limit;
    else
      t->forward_pair_limit = e->limit;
  }
  /* Kissat's factoriniticks dose belongs to the first factorization. */
  if (!e->doses && e->budget_aware && !strcmp(e->name, "Factor"))
    e->limit = meter(t, e) + c->inprocess.factor_initial_ticks;
}

/* Kissat's factordelay: Factor waits for its decade threshold of active
 * variables and for new material since its last completed call. */
static int factor_declines(strategy *t) {
  const settings *c = &t->set;
  if (*t->conflicts && !c->inprocess.factor_repeated_enabled) return 1;
  const uint64_t active = observe(t, TACTIX_OBSERVE_ACTIVE_VARIABLES);
  if (c->inprocess.factor_active_ceiling && active >= c->inprocess.factor_active_ceiling)
    t->factor_large_formula = 1;
  if (t->factor_large_formula) return 1;
  uint64_t threshold = 1;
  const uint64_t decades = t->bve_rounds_run > c->inprocess.factor_decade_round_cap
                               ? c->inprocess.factor_decade_cap
                               : t->bve_rounds_run + c->inprocess.factor_decade_offset;
  for (uint64_t d = 0; d < decades; ++d) threshold *= c->inprocess.factor_decade_base;
  return active >= threshold ||
         t->factor_marked_watermark >= observe(t, TACTIX_OBSERVE_FACTOR_MARKED);
}

static void set_factor_composite(strategy *t, int rule, int composite) {
  tactix_online_parameters factor =
      ds_read_profile(t, rule, TACTIX_ONLINE_PARAMETERS_FACTOR, TACTIX_PARAMETERS_INSTALLED);
  factor.value.factor.composite = composite;
  ds_install(t, rule, &factor);
}

/* Per-occurrence step profiles: posed for exactly this application. */
static int scoped_family(int kind) {
  switch (kind) {
  case TACTIX_ONLINE_PARAMETERS_SUBSUME:
  case TACTIX_ONLINE_PARAMETERS_STRENGTHEN:
  case TACTIX_ONLINE_PARAMETERS_CONGRUENCE:
  case TACTIX_ONLINE_PARAMETERS_INSTANTIATE:
  case TACTIX_ONLINE_PARAMETERS_BACKBONE:
  case TACTIX_ONLINE_PARAMETERS_VIVIFY:
    return 1;
  default:
    return 0;
  }
}

/* run_vivify_dose: Kissat vivify.c's one cumulative tick ceiling across the
 * tier bands, then the irredundant band unless its delay holds. */
static int run_vivify_dose(strategy *t, slot_entry *e, uint64_t limit) {
  const settings *c = &t->set;
  ds_pose_vivify_tiers(t);
  const uint64_t remaining0 = ledger_remaining(t, e, limit);
  const uint64_t remaining = remaining0 ? remaining0 : 1;
  const uint64_t dose_start = meter(t, e);
  const int irredundant_delayed = *t->conflicts && e->delay_count;
  if (irredundant_delayed) e->delay_count--;
  const uint64_t denominator =
      irredundant_delayed ? c->inprocess.vivify_redundant_share : c->inprocess.vivify_total_share;
#define SHARE(weight) (remaining / denominator * (weight) + remaining % denominator * (weight) / denominator)
  tactix_work_settings dose;
  tactix_read_work_settings(t->s, &dose);
  dose.fields = TACTIX_WORK_BUDGET | TACTIX_WORK_OFFSET | TACTIX_WORK_BOUNDED;
  dose.bounded = 1;
  const work_scope scope = work_enter(t, dose, 1);
  uint64_t cumulative = 0;
  int result = NOOP;
  uint64_t shares[3] = {c->inprocess.vivify_tier_share[0], c->inprocess.vivify_tier_share[1],
                        c->inprocess.vivify_tier_share[2]};
  /* Equal focused tiers leave tier 2 empty: Kissat hands its share to tier 1. */
  const uint32_t mode = (uint32_t)c->inprocess.vivify_tier_mode;
  const uint32_t tier1 = t->retier.tier1_by_mode[mode];
  const uint32_t tier2 = t->retier.tier2_by_mode[mode] > tier1 ? t->retier.tier2_by_mode[mode] : tier1;
  if (tier1 == tier2 && shares[0] && shares[1]) {
    shares[0] += shares[1];
    shares[1] = 0;
  }
  for (int band = 0; band <= 3; ++band) {
    if (result == UNSAT || observe(t, TACTIX_OBSERVE_INCONSISTENT)) break;
    uint64_t scope_id;
    if (band < 3) {
      if (!shares[band]) continue;
      cumulative += SHARE(shares[band]);
      scope_id = c->inprocess.vivify_tier_scope[band];
    } else {
      if (irredundant_delayed) break;
      cumulative += SHARE(c->inprocess.vivify_total_share - c->inprocess.vivify_redundant_share);
      scope_id = 2; /* the irredundant band */
    }
    const uint64_t tried = *t->meters.vivify_checked;
    const uint64_t produced = *t->meters.vivify_subsumed + *t->meters.vivify_shrunken +
                              *t->meters.vivify_implied + *t->meters.vivify_asymmetric +
                              *t->meters.vivify_instantiated;
    tactix_set_vivify_scope(t->s, (int)scope_id);
    tactix_work_settings band_work;
    memset(&band_work, 0, sizeof band_work);
    band_work.fields = TACTIX_WORK_BUDGET | TACTIX_WORK_OFFSET | TACTIX_WORK_BOUNDED;
    band_work.budget = cumulative;
    band_work.offset = meter(t, e) - dose_start;
    band_work.bounded = 1;
    tactix_install_work_settings(t->s, &band_work);
    const int code = apply_with_ticks(t, e, TACTIX_INSTANCE_CANONICAL);
    if (band < 3) {
      if (code == UNSAT || (code == OK && result != UNSAT)) result = code;
    } else {
      if (code == UNSAT)
        result = code;
      else if (code == OK && result != OK)
        result = code;
      /* Kissat counts every successful candidate terminal as vivified. */
      if (*t->conflicts) {
        const uint64_t tried_now = *t->meters.vivify_checked - tried;
        const uint64_t produced_now = *t->meters.vivify_subsumed + *t->meters.vivify_shrunken +
                                      *t->meters.vivify_implied + *t->meters.vivify_asymmetric +
                                      *t->meters.vivify_instantiated - produced;
        update_delay(e, !tried_now || (wide)produced_now * PERCENT <
                                          (wide)tried_now *
                                              c->inprocess.vivify_irredundant_productivity_percent);
      }
    }
  }
#undef SHARE
  tactix_set_vivify_scope(t->s, 0);
  work_leave(t, &scope);
  return result;
}

/* run_budgeted_batch: one application handed the entry's remaining ledger. */
static int run_budgeted_batch(strategy *t, slot_entry *e, uint64_t limit, int factor_entry) {
  const settings *c = &t->set;
  const int substitute = !strcmp(e->name, "Substitute");
  const uint64_t remaining0 = ledger_remaining(t, e, limit);
  /* Substitute's first round is mandatory; others keep a one-tick hand-off. */
  const uint64_t handed = substitute ? remaining0 : remaining0 ? remaining0 : 1;
  const uint64_t spent_before = meter(t, e);
  if (factor_entry) set_factor_composite(t, e->rule, 1);
  tactix_work_settings request = work_budget(handed);
  request.fields |= TACTIX_WORK_MIDROUND;
  request.midround = c->inprocess.forward_midround_budget &&
                     (!strcmp(e->name, "SubsumeForward") || !strcmp(e->name, "Strengthen"));
  const work_scope scope = work_enter(t, request, 1);
  int code, round_budget_break = 0;
  if (e->tick_stat != TACTIX_TICK_ATTRIBUTION_NONE) tactix_install_tick_attribution(t->s, e->tick_stat);
  if (!strcmp(e->name, "Sweep")) {
    const tactix_online_parameters limits =
        e->parameters.kind == TACTIX_ONLINE_PARAMETERS_SWEEP
            ? e->parameters
            : ds_read_profile(t, e->rule, TACTIX_ONLINE_PARAMETERS_SWEEP, TACTIX_PARAMETERS_STRATEGY);
    /* A productive Sweep's retirements wait for the follow-up Substitute. */
    if (!t->retirements_held &&
        support(t, TACTIX_SUPPORT_HOLD_RETIREMENTS, TACTIX_RETIREMENTS_HOLD) == 0)
      t->retirements_held = 1;
    code = online_step(t, e->rule, &limits, NULL);
  } else if (substitute) {
    /* kissat substitute.c: round one always; an overrun suppresses the next. */
    code = substitute_rounds(t, e, (uint32_t)c->inprocess.substitute_rounds, *t->conflicts != 0,
                             &round_budget_break);
  } else {
    code = tactix_apply(t->s, e->rule);
  }
  if (e->tick_stat != TACTIX_TICK_ATTRIBUTION_NONE) tactix_install_tick_attribution(t->s, TACTIX_TICK_ATTRIBUTION_NONE);
  if (factor_entry && (observe(t, TACTIX_OBSERVE_FACTOR_STATE) & 1))
    t->factor_marked_watermark = observe(t, TACTIX_OBSERVE_FACTOR_MARKED);
  work_leave(t, &scope);
  /* Every budgeted dose leaves composite Factor matching off. */
  if (observe(t, TACTIX_OBSERVE_FACTOR_STATE) & 2) set_factor_composite(t, ds_rule("Factor"), 0);
  if (meter(t, e) > spent_before) e->doses++;
  return code;
}

typedef struct {
  int group, outer, phase_unsat, round_changed, eliminate_limit_reached;
  int congruence_changed, sweep_changed, last_result;
} phase_context;

/* The batch application and its follow-ups: 1 to stop the phase. */
static int batch_application(strategy *t, slot_entry *e, phase_context *ctx, uint64_t limit,
                             int factor_entry, int delayed) {
  const settings *c = &t->set;
  const int substitute = !strcmp(e->name, "Substitute");
  const productivity before = delayed ? rule_productivity(t, e) : (productivity){0, 0};
  const int budgeted = e->metered && !e->dosed && e->budget_aware;
  const int forward_entry = ctx->group == ELIMINATE && !strcmp(e->name, "Strengthen");
  const uint64_t forward_trail_from = observe(t, TACTIX_OBSERVE_PROPAGATION_HEAD);
  const uint32_t factor_begin = factor_entry ? external_extent(t) : 0;
  int code;
  if (budgeted && !strcmp(e->name, "Vivify")) {
    code = run_vivify_dose(t, e, limit);
  } else if (budgeted) {
    code = run_budgeted_batch(t, e, limit, factor_entry);
  } else if (substitute) {
    /* Kissat's preprocessing follow-ups substitute with complete=true. */
    code = substitute_rounds(t, e,
                             (uint32_t)(*t->conflicts ? c->inprocess.substitute_search_rounds
                                                      : c->inprocess.substitute_rounds),
                             -1, NULL);
  } else {
    code = apply_with_ticks(t, e, TACTIX_INSTANCE_CANONICAL);
  }
  ctx->last_result = code;
  if (factor_entry && !*t->conflicts && t->ownership.symmetry_applied)
    record_factor_range(t, factor_begin, external_extent(t));
  /* Kissat's named productivity delays update after preprocessing calls too. */
  if (c->inprocess.closure_delay && delayed) {
    const productivity after = rule_productivity(t, e);
    const uint64_t yield = after.yield - before.yield;
    const uint64_t denominator =
        e->delay_delta ? after.denominator - before.denominator : after.denominator;
    update_delay(e, !denominator || yield * PERMILLE < (uint64_t)e->delay_permille * denominator);
  }
  if (code == UNSAT || observe(t, TACTIX_OBSERVE_INCONSISTENT)) return ctx->phase_unsat = 1;
  /* C2's forward pass leaves its reconnected suffix's consequences pending:
   * publish its roots, then close that suffix (H publication is ours). */
  if (forward_entry) {
    support(t, TACTIX_SUPPORT_PUBLISH_ROOTS, forward_trail_from);
    const uint64_t closure_trail_from = observe(t, TACTIX_OBSERVE_TRAIL);
    const int closure = support(t, TACTIX_SUPPORT_FORWARD_CLOSURE, forward_trail_from);
    support(t, TACTIX_SUPPORT_PUBLISH_ROOTS, closure_trail_from);
    if (closure == UNSAT) return ctx->phase_unsat = 1;
  }
  if (code != OK) return 0;
  ctx->round_changed = 1;
  if (ds_propagate(t) == UNSAT) return ctx->phase_unsat = 1;
  return 0;
}

/* run_batch_entry: one all-or-nothing or budgeted application, under the
 * step's own profile when it poses one. */
static int run_batch_entry(strategy *t, slot_entry *e, phase_context *ctx) {
  const settings *c = &t->set;
  const uint64_t limit = e->limit;
  ctx->last_result = NOOP;
  /* Substitute has no call-entry effort gate in Kissat. */
  if (strcmp(e->name, "Substitute") && e->metered && meter(t, e) > limit) return 0;
  const int factor_entry = e->budget_aware && !strcmp(e->name, "Factor");
  if (factor_entry && factor_declines(t)) return 0;
  const int delayed = e->delayed;
  if (c->inprocess.closure_delay && delayed && *t->conflicts && e->delay_count) {
    e->delay_count--;
    return 0;
  }
  if (tactix_rule_applicable(t->s, e->rule) != 1) return 0;
  if (!scoped_family(e->parameters.kind))
    return batch_application(t, e, ctx, limit, factor_entry, delayed);
  const tactix_online_parameters saved =
      ds_read_profile(t, e->rule, e->parameters.kind, TACTIX_PARAMETERS_INSTALLED);
  ds_install(t, e->rule, &e->parameters);
  const int stop = batch_application(t, e, ctx, limit, factor_entry, delayed);
  ds_install(t, e->rule, &saved);
  return stop;
}

/* The literal code a candidate cursor compares (instance_key). */
static uint64_t instance_key(uint64_t instance) {
  if (instance > UINT32_MAX) return instance;
  const int32_t literal = (int32_t)(uint32_t)instance;
  if (!literal || literal == INT32_MIN) return instance;
  return 2 * (uint64_t)((literal < 0 ? -(int64_t)literal : literal) - 1) + (literal < 0);
}

static int instance_variable(uint64_t instance) {
  if (instance > UINT32_MAX) return 0;
  const int32_t literal = (int32_t)(uint32_t)instance;
  return literal == INT32_MIN ? 0 : literal < 0 ? -literal : literal;
}

/* sort.h's quicksort with a sentinel-seeded insertion pass (Kissat's order on
 * ties), over compact positions under `less`. */
typedef struct {
  const double *rank;
  int stable;
} transitive_order;

static int transitive_less(const transitive_order *o, uint32_t a, uint32_t b) {
  /* transitive.c's second IDX: adjacent compact positions share an owner. */
  const uint32_t i = a >> 1, j = b >> 1;
  if (o->stable) {
    if (o->rank[i] != o->rank[j]) return o->rank[i] < o->rank[j];
    return i < j;
  }
  return o->rank[i] < o->rank[j];
}

static void kissat_sort(uint32_t *values, size_t n, const transitive_order *o) {
  if (n <= 1) return;
#define LESS(a, b) transitive_less(o, (a), (b))
#define SWAP(p, q) do { const uint32_t swapped = values[p]; values[p] = values[q]; values[q] = swapped; } while (0)
#define GREATER_SWAP(p, q) do { if (LESS(values[q], values[p])) SWAP(p, q); } while (0)
  enum { QUICK_SORT_LIMIT = 10 };
  size_t left = 0, right = n - 1;
  size_t *pending = NULL, pending_count = 0, pending_capacity = 0;
  if (right - left > QUICK_SORT_LIMIT) {
    for (;;) {
      const size_t middle = left + (right - left) / 2;
      SWAP(middle, right - 1);
      GREATER_SWAP(left, right - 1);
      GREATER_SWAP(left, right);
      GREATER_SWAP(right - 1, right);
      size_t i = left, j = right - 1;
      const uint32_t pivot = values[j];
      for (;;) {
        while (LESS(values[++i], pivot))
          ;
        while (LESS(pivot, values[--j]))
          if (j == left) break;
        if (i >= j) break;
        SWAP(i, j);
      }
      SWAP(i, right - 1);
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
      if (right - left > QUICK_SORT_LIMIT) {
        if (pending_count == pending_capacity) {
          pending_capacity = pending_capacity ? 2 * pending_capacity : 16;
          pending = realloc(pending, 2 * pending_capacity * sizeof *pending);
          if (!pending) abort();
        }
        pending[2 * pending_count] = other_left;
        pending[2 * pending_count + 1] = other_right;
        pending_count++;
      } else if (other_right - other_left > QUICK_SORT_LIMIT) {
        left = other_left;
        right = other_right;
      } else if (pending_count) {
        pending_count--;
        left = pending[2 * pending_count];
        right = pending[2 * pending_count + 1];
      } else {
        break;
      }
    }
  }
  free(pending);
  for (size_t i = n - 1; i > 0; i--) GREATER_SWAP(i - 1, i);
  for (size_t i = 2; i < n; i++) {
    const uint32_t pivot = values[i];
    size_t j = i;
    while (LESS(pivot, values[j - 1])) {
      values[j] = values[j - 1];
      j--;
    }
    values[j] = pivot;
  }
#undef LESS
#undef SWAP
#undef GREATER_SWAP
}

/* rank_transitive_reference: Kissat transitive.c's stack, seeded in compact
 * order, activity-sorted and consumed from the back, each variable's
 * positive then negative literal. 0 when the reference order cannot apply. */
static int rank_transitive(strategy *t, uint64_t *instances, uint32_t n) {
  const uint64_t population = observe(t, TACTIX_OBSERVE_BRANCH_POPULATION);
  if (!population) return 0;
  uint64_t *positive = malloc(population * sizeof *positive);
  uint64_t *negative = malloc(population * sizeof *negative);
  uint8_t *seen = calloc(population, 1);
  uint32_t *candidates = malloc((size_t)n * sizeof *candidates);
  double *rank = malloc(population * sizeof *rank);
  if (!positive || !negative || !seen || !candidates || !rank) abort();
  size_t count = 0;
  int applies = 1;
  for (uint32_t i = 0; i < n && applies; ++i) {
    const int variable = instance_variable(instances[i]);
    const uint64_t compact =
        variable ? tactix_observe(t->s, TACTIX_OBSERVE_COMPACT_POSITION, (uint64_t)variable) : UINT32_MAX;
    if (compact >= population) {
      applies = 0;
      break;
    }
    if (!seen[compact]) {
      seen[compact] = 1;
      positive[compact] = negative[compact] = TACTIX_INSTANCE_CANONICAL;
      candidates[count++] = (uint32_t)compact;
    }
    ((int32_t)(uint32_t)instances[i] < 0 ? negative : positive)[compact] = instances[i];
  }
  if (applies) {
    /* Kissat's all_variables seeds the stack in compact-index order. */
    memset(seen, 0, population);
    for (size_t i = 0; i < count; ++i) seen[candidates[i]] = 1;
    count = 0;
    for (uint64_t compact = 0; compact < population; ++compact)
      if (seen[compact]) candidates[count++] = (uint32_t)compact;
    for (uint64_t owner = 0; owner < population; ++owner)
      rank[owner] = tactix_branch_rank(t->s, tactix_branch_population(t->s, (uint32_t)owner));
    const transitive_order order = {rank, t->stable};
    kissat_sort(candidates, count, &order);
    uint32_t out = 0;
    for (size_t i = count; i-- > 0;) {
      if (positive[candidates[i]] != TACTIX_INSTANCE_CANONICAL) instances[out++] = positive[candidates[i]];
      if (negative[candidates[i]] != TACTIX_INSTANCE_CANONICAL) instances[out++] = negative[candidates[i]];
    }
  }
  free(positive);
  free(negative);
  free(seen);
  free(candidates);
  free(rank);
  return applies;
}

/* order_candidates: resume after the cursor, then the entry's ranking. */
static void order_candidates(strategy *t, slot_entry *e, uint64_t *instances, uint32_t n) {
  if (!n) return;
  if (e->cursor_enabled) {
    uint32_t start = 0;
    while (start < n && instance_key(instances[start]) < e->cursor) start++;
    if (start >= n) start = 0; /* the cursor ran past the set: begin again */
    if (start) {
      uint64_t *rotated = malloc((size_t)n * sizeof *rotated);
      if (!rotated) abort();
      memcpy(rotated, instances + start, (size_t)(n - start) * sizeof *rotated);
      memcpy(rotated + (n - start), instances, (size_t)start * sizeof *rotated);
      memcpy(instances, rotated, (size_t)n * sizeof *rotated);
      free(rotated);
    }
  }
  if (e->order == TACTIX_CANDIDATE_ORDER_NATIVE || n < 2) return;
  const int arm = t->candidate_orders[e->rule];
  if (arm == TACTIX_CANDIDATE_ORDER_NATIVE) return;
  if (arm == TACTIX_CANDIDATE_ORDER_REVERSE) {
    for (uint32_t i = 0, j = n - 1; i < j; ++i, --j) {
      const uint64_t swapped = instances[i];
      instances[i] = instances[j];
      instances[j] = swapped;
    }
    return;
  }
  if (arm == TACTIX_CANDIDATE_ORDER_HEURISTIC && !strcmp(e->name, "TransitiveReduce") &&
      rank_transitive(t, instances, n))
    return;
  (void)ds_unported(t, "candidate order arm");
}

/* fastel_flush_occurrences: SC2024 fastel's occurrence flush of `literal` in
 * the ordered session, each untouched large clause checked once for forward
 * subsumption, its inspected prefix compacted; the occurrences counted,
 * occurrence_limit + 1 once a clause is too large. */
static uint64_t fastel_flush(strategy *t, int literal, uint32_t occurrence_limit,
                             uint32_t clause_size_limit) {
  const int n = tactix_occurrences(t->s, literal, NULL, 0);
  tactix_occurrence *entries = malloc((size_t)(n > 0 ? n : 1) * sizeof *entries);
  if (!entries) abort();
  tactix_occurrences(t->s, literal, entries, n);
  const int subsume = ds_rule("SubsumeForward");
  int *literals = NULL;
  int capacity = 0;
  uint64_t count = 0, at = 0;
  for (int i = 0; i < n; ++i) {
    const tactix_occurrence *entry = &entries[i];
    if (entry->binary) {
      const int partner = entry->partner < 0 ? -entry->partner : entry->partner;
      if (tactix_literal_value(t->s, entry->partner) > 0 ||
          tactix_observe(t->s, TACTIX_OBSERVE_VARIABLE_RETIRED, (uint64_t)partner) == 1) {
        ++at;
        continue;
      }
    } else {
      unsigned flags;
      int size = tactix_clause(t->s, entry->clause, literals, capacity, &flags);
      if (size > capacity) {
        capacity = size;
        literals = realloc(literals, (size_t)capacity * sizeof *literals);
        if (!literals) abort();
        size = tactix_clause(t->s, entry->clause, literals, capacity, &flags);
      }
      int contains = 0;
      for (int j = 0; j < size; ++j) contains |= literals[j] == literal;
      if ((flags & TACTIX_CLAUSE_GARBAGE) || !contains) {
        ++at;
        continue;
      }
      if ((uint64_t)size > clause_size_limit) {
        count = (uint64_t)occurrence_limit + 1;
        ++at;
        break;
      }
      if (flags & TACTIX_CLAUSE_SATISFIED) {
        ++at;
        continue;
      }
      const uint64_t instance = TACTIX_SUBSUME_FORWARD_FASTEL |
                                ((uint64_t)(occurrence_limit & 0x7fffffffu)
                                 << TACTIX_SUBSUME_FORWARD_FASTEL_LIMIT_SHIFT) |
                                entry->clause;
      if (tactix_apply_instance(t->s, subsume, instance) == OK) {
        ++at;
        continue;
      }
    }
    ++at;
    if (++count > occurrence_limit) break;
  }
  free(entries);
  free(literals);
  support(t, TACTIX_SUPPORT_COMPACT_FASTEL_PREFIX, (at << 32) | (uint32_t)literal);
  return count;
}

/* A candidate payload with its sort keys (score, import position). */
typedef struct {
  uint64_t instance, score, tie;
} keyed_instance;

static int keyed_less(const keyed_instance *a, const keyed_instance *b) {
  return a->score != b->score ? a->score < b->score : a->tie < b->tie;
}

/* A stable merge sort (std::stable_sort's order on equal keys). */
static void stable_sort_keyed(keyed_instance *values, size_t n) {
  if (n < 2) return;
  keyed_instance *buffer = malloc(n * sizeof *buffer);
  if (!buffer) abort();
  for (size_t width = 1; width < n; width *= 2) {
    for (size_t left = 0; left < n; left += 2 * width) {
      const size_t middle = left + width < n ? left + width : n;
      const size_t right = left + 2 * width < n ? left + 2 * width : n;
      size_t i = left, j = middle, k = left;
      while (i < middle && j < right) buffer[k++] = keyed_less(&values[j], &values[i]) ? values[j++] : values[i++];
      while (i < middle) buffer[k++] = values[i++];
      while (j < right) buffer[k++] = values[j++];
    }
    memcpy(values, buffer, n * sizeof *values);
  }
  free(buffer);
}

/* FastEl's candidate domain: the BVE candidates gathered in the owner's
 * import order (its flush decides which clause of a subsuming pair dies),
 * those within the occurrence limits, admitted when they cover more live
 * variables than they exclude, then ranked by occurrences. */
static uint32_t fastel_candidates(strategy *t, const tactix_online_bve_parameters *fastel,
                                  uint64_t *instances, uint32_t n) {
  keyed_instance *keyed = malloc((size_t)(n ? n : 1) * sizeof *keyed);
  if (!keyed) abort();
  for (uint32_t i = 0; i < n; ++i) {
    const int variable = instance_variable(instances[i]);
    const uint64_t position =
        variable ? tactix_observe(t->s, TACTIX_OBSERVE_IMPORT_POSITION, (uint64_t)variable) : UINT64_MAX;
    keyed[i] = (keyed_instance){instances[i], position, 0};
  }
  stable_sort_keyed(keyed, n);
  uint32_t count = 0;
  for (uint32_t i = 0; i < n && !t->unported; ++i) {
    const int variable = instance_variable(keyed[i].instance);
    if (!variable || tactix_observe(t->s, TACTIX_OBSERVE_BVE_RETRY, (uint64_t)variable) != 1 ||
        tactix_observe(t->s, TACTIX_OBSERVE_VARIABLE_OPEN, (uint64_t)variable) != 1 ||
        retained(t, variable))
      continue;
    const uint64_t positive =
        fastel_flush(t, variable, fastel->occurrence_limit, fastel->clause_size_limit);
    if (positive > fastel->occurrence_limit) continue;
    /* The shipped owner's gather counts LIT(pivot) twice. */
    const uint64_t negative =
        fastel_flush(t, variable, fastel->occurrence_limit, fastel->clause_size_limit);
    if (negative > fastel->occurrence_limit || positive + negative > fastel->occurrence_limit)
      continue;
    keyed[count++] = (keyed_instance){keyed[i].instance, positive + negative, keyed[i].score};
  }
  const uint64_t active = observe(t, TACTIX_OBSERVE_ACTIVE_VARIABLES);
  if (!t->fastel_admitted && !(count && count > active - (count < active ? count : active)))
    count = 0;
  else
    t->fastel_admitted = 1;
  stable_sort_keyed(keyed, count);
  for (uint32_t i = 0; i < count; ++i) instances[i] = keyed[i].instance;
  free(keyed);
  return count;
}

/* run_dosed_entry: the entry's candidates one instance at a time under its
 * ledger; BVE through Kissat's score-heap schedule at the growth bound. */
static int run_dosed_entry(strategy *t, slot_entry *e, phase_context *ctx) {
  ctx->last_result = NOOP;
  if (tactix_rule_applicable(t->s, e->rule) != 1) return 0;
  const int bve = !strcmp(e->name, "BVE");
  const int fastel = bve && t->fastel_active;
  uint64_t limit = e->limit;
  if (bve && !fastel) {
    if (meter(t, e) > limit) goto finished;
    /* The pass's growth bound, which the schedule scores and the pass read. */
    tactix_online_parameters bound =
        ds_read_profile(t, e->rule, TACTIX_ONLINE_PARAMETERS_BVE, TACTIX_PARAMETERS_INSTALLED);
    bound.value.bve.growth_bound = (uint32_t)t->bve_level;
    ds_install(t, e->rule, &bound);
  }
  /* The BVE pass poses its profile for the pass (BveInvocationSession). */
  tactix_online_parameters saved_bve, pass_bve;
  if (bve) {
    saved_bve = ds_read_profile(t, e->rule, TACTIX_ONLINE_PARAMETERS_BVE, TACTIX_PARAMETERS_INSTALLED);
    pass_bve = fastel ? ds_read_profile(t, e->rule, TACTIX_ONLINE_PARAMETERS_BVE,
                                        TACTIX_PARAMETERS_STRATEGY)
               : e->parameters.kind == TACTIX_ONLINE_PARAMETERS_BVE ? e->parameters
                                                                    : saved_bve;
    ds_install(t, e->rule, &pass_bve);
  }
  if (fastel) {
    /* Discovery inspects every live variable before the owner's order is
     * known: charge that lower bound to the dose, or decline before any
     * candidate domain exists. */
    const uint64_t active = observe(t, TACTIX_OBSERVE_ACTIVE_VARIABLES);
    const uint64_t remaining = ledger_remaining(t, e, e->limit);
    const uint64_t discovery_remaining =
        t->fastel_discovery_ticks > remaining ? 0 : remaining - t->fastel_discovery_ticks;
    if (!active || active > discovery_remaining / 2) {
      t->fastel_budget_exhausted = 1;
      ctx->eliminate_limit_reached = 1;
      ds_install(t, e->rule, &saved_bve);
      return 0;
    }
    t->fastel_discovery_ticks += active;
    limit = e->limit - t->fastel_discovery_ticks;
  }
  const int scheduled = bve && !fastel && e->order == TACTIX_CANDIDATE_ORDER_HEURISTIC;
  uint64_t *instances = NULL;
  uint32_t n = 0;
  if (scheduled) {
    if (tactix_bve_schedule_open(t->s)) {
      ds_install(t, e->rule, &saved_bve);
      return ds_unported(t, "refused BVE schedule");
    }
  } else {
    const uint64_t capacity = 2 * observe(t, TACTIX_OBSERVE_VARIABLES) + 1;
    instances = malloc(capacity * sizeof *instances);
    if (!instances) abort();
    n = (uint32_t)tactix_rule_instances(t->s, e->rule, instances, (int)capacity);
    if (fastel)
      n = fastel_candidates(t, &pass_bve.value.bve, instances, n);
    else
      order_candidates(t, e, instances, n);
  }
  const int affine_protects = bve && (observe(t, TACTIX_OBSERVE_AFFINE_SESSION) & 6) == 6;
  for (uint32_t i = 0; !t->unported;) {
    uint64_t instance;
    if (scheduled) {
      instance = tactix_bve_schedule_next(t->s);
      if (instance == TACTIX_INSTANCE_CANONICAL) break;
    } else {
      if (i >= n) break;
      instance = instances[i++];
    }
    const int variable = instance_variable(instance);
    if (bve && variable) {
      if (retained(t, variable) || tactix_observe(t->s, TACTIX_OBSERVE_VARIABLE_OPEN, (uint64_t)variable) != 1)
        continue;
      /* CryptoMiniSat keeps a retained XOR's variables out of BVE. */
      if (affine_protects && tactix_observe(t->s, TACTIX_OBSERVE_AFFINE_SUPPORT, (uint64_t)variable) == 1)
        continue;
    }
    if (fastel) {
      if (!variable || tactix_observe(t->s, TACTIX_OBSERVE_BVE_RETRY, (uint64_t)variable) != 1)
        continue;
      const tactix_online_bve_parameters *p = &pass_bve.value.bve;
      const uint64_t positive = fastel_flush(t, variable, p->occurrence_limit, p->clause_size_limit);
      const uint64_t negative = fastel_flush(t, -variable, p->occurrence_limit, p->clause_size_limit);
      /* A structured helper keeps its definition: FastEl has considered it. */
      if (positive > p->occurrence_limit || negative > p->occurrence_limit ||
          positive + negative > p->occurrence_limit ||
          protects_at_application(&t->ownership, (uint32_t)variable - 1)) {
        support(t, TACTIX_SUPPORT_RETIRE_BVE_CANDIDATE, (uint64_t)variable);
        continue;
      }
    }
    if (meter(t, e) > limit) { /* dose spent */
      if (fastel && !t->fastel_budget_exhausted) {
        t->fastel_budget_exhausted = 1;
        ctx->eliminate_limit_reached = 1;
      }
      break;
    }
    if (tactix_rule_applicable(t->s, e->rule) != 1) break;
    if (bve && variable && tactix_observe(t->s, TACTIX_OBSERVE_BVE_RETRY, (uint64_t)variable) != 1)
      continue;
    if (t->stop && *t->stop) break;
    if (e->cursor_enabled) e->cursor = instance_key(instance) + 1;
    work_scope scope;
    const int residual = e->budget_aware && e->metered;
    if (residual) {
      const uint64_t remaining = ledger_remaining(t, e, limit);
      scope = work_enter(t, work_budget(remaining ? remaining : 1), 1);
    }
    const uint64_t eliminated = *t->meters.bve_eliminated;
    const uint64_t propagation_begin = observe(t, TACTIX_OBSERVE_PROPAGATION_HEAD);
    const int code = apply_with_ticks(t, e, instance);
    if (residual) work_leave(t, &scope);
    ctx->last_result = code;
    /* The FastEl attempt clears `eliminate` after it ran. */
    if (fastel && !observe(t, TACTIX_OBSERVE_INCONSISTENT))
      support(t, TACTIX_SUPPORT_RETIRE_BVE_CANDIDATE, (uint64_t)variable);
    if (code == UNSAT || observe(t, TACTIX_OBSERVE_INCONSISTENT)) {
      ctx->phase_unsat = 1;
      break;
    }
    if (code != OK) continue;
    /* Kissat repeats a round on eliminations, not on a declined unit. */
    if (!bve || *t->meters.bve_eliminated != eliminated) ctx->round_changed = 1;
    /* eliminate.c flushes a candidate's units after every candidate. */
    if (ds_propagate(t) == UNSAT) {
      ctx->phase_unsat = 1;
      break;
    }
    if (bve) support(t, TACTIX_SUPPORT_BVE_FLUSH_UNITS, propagation_begin);
  }
  free(instances);
  if (scheduled) tactix_bve_schedule_close(t->s);
  if (bve) {
    ds_install(t, e->rule, &saved_bve);
    if (!ctx->phase_unsat && !fastel) t->bve_ran_this_phase = 1;
  }
  if (fastel) return 0;
finished:
  if (bve && ctx->outer == 0) t->bve_rounds_run++;
  if (bve && !ctx->phase_unsat) {
    /* Kissat collects after every completed elimination round; the phase's
     * resolution ceiling ends its forward rounds. */
    support(t, TACTIX_SUPPORT_COLLECT_GARBAGE, 0);
    if (ctx->group == ELIMINATE && meter(t, e) > limit) ctx->eliminate_limit_reached = 1;
  }
  return 0;
}

/* One round over the phase's slots: 1 to stop the phase. */
static int run_slots(strategy *t, phase_context *ctx) {
  int sweep_pending = 0;
  for (int slot = 0; slot < t->entry_count && !t->unported; ++slot) {
    slot_entry *e = &t->entries[slot];
    if (e->group != ctx->group) continue;
    if (t->fastel_active && strcmp(e->name, "BVE")) continue;
    if (t->stop && *t->stop) break;
    if (!followup_admissible(t, e, ctx->congruence_changed, ctx->sweep_changed)) continue;
    if (!slot_entry_admissible(t, e, ctx->phase_unsat)) continue;
    const int substitute = !strcmp(e->name, "Substitute");
    if (e->metered &&
        (substitute || ctx->group == PROBE || ctx->outer == 0 || e->shared_meter))
      set_entry_limit(t, e);
    const int stop = e->dosed && e->instances ? run_dosed_entry(t, e, ctx) : run_batch_entry(t, e, ctx);
    if (!strcmp(e->name, "Congruence")) ctx->congruence_changed = ctx->last_result == OK;
    if (!strcmp(e->name, "Sweep")) {
      ctx->sweep_changed = ctx->last_result == OK;
      if (ctx->sweep_changed)
        sweep_pending = 1;
      else if (!sweep_pending)
        publish_retirements(t);
    } else {
      /* Kissat's post-Sweep Substitute scans the union with Sweep's portion. */
      if (substitute) sweep_pending = 0;
      if (!sweep_pending) publish_retirements(t);
    }
    if (stop) return 1;
  }
  return 0;
}

void ds_inprocess_build(strategy *t) {
  if (!t->entries) build_entries(t);
  install_inprocess_profiles(t);
  tactix_install_material_settings(t->s, setting_mode(t->set.inprocess.touch_filter),
                                   setting_mode(t->set.inprocess.likely_kept_redundant));
  if (!t->inprocess_initialized) {
    t->inprocess_initialized = 1;
    /* Probe owns initial preprocessing; Eliminate's first deadline waits for
     * start_search (initialize_search_limits). */
    t->clocks[PROBE].next_conflicts = *t->conflicts;
    t->clocks[ELIMINATE].next_conflicts = UINT64_MAX;
  }
}

/* kimits.h NLOG2N phase intervals, scaled by kissat_scale_delta. */
void ds_update_phase_limit(strategy *t, int group) {
  const settings *c = &t->set;
  const int split = (int)c->inprocess.split_clocks;
  uint64_t n = split ? t->clocks[group].rounds
               : t->clocks[PROBE].rounds > t->clocks[ELIMINATE].rounds ? t->clocks[PROBE].rounds
                                                                      : t->clocks[ELIMINATE].rounds;
  if (!n) n = 1;
  const double lg = log10((double)n + c->inprocess.logarithm_offset);
  const double base = split && group == PROBE
                          ? (double)c->inprocess.probe_interval * (double)n * lg
                          : (double)c->inprocess.eliminate_interval * (double)n * lg * lg;
  t->clocks[group].next_conflicts = *t->conflicts + ds_scale_inprocessing_delta(t, (uint64_t)base);
}

int ds_want_phase(strategy *t, int group) {
  const uint64_t conflicts = *t->conflicts;
  if (!t->inprocess_initialized || (t->set.inprocess.preprocess_only && conflicts) ||
      (!conflicts && group == ELIMINATE) || t->last_reduce_conflicts == conflicts)
    return 0;
  slot_clock *clock = &t->clocks[group];
  /* Kissat's Probe tests limit < conflicts, Eliminate limit <= conflicts; the
   * round-zero Probe deadline is the preprocessing invitation. */
  const int startup_probe = group == PROBE && !clock->rounds && conflicts == clock->next_conflicts;
  const int before = group == PROBE ? conflicts <= clock->next_conflicts
                                    : conflicts < clock->next_conflicts;
  if (!startup_probe && before) return 0;
  const int gated =
      t->set.inprocess.watermarks && clock->rounds &&
      (group == PROBE ? *t->reductions == clock->reductions_mark
                      : *t->bve_marks <= clock->marks_mark &&
                            stat(t, TACTIX_STAT_ROOT_FIXED) <= clock->fixed_mark &&
                            observe(t, TACTIX_OBSERVE_SUBSUME_MATERIAL) <= clock->material_mark);
  if (gated) {
    clock->blocked = 1;
    return 0;
  }
  return 1;
}

/* run_structured_root: the startup SR program, once, before the first
 * conflict, stage by stage under one cumulative SR-discovery allowance that
 * Satsuma's progress window renews; 10/20 for a verdict, else 0. */
static int structured_stages(strategy *t, int list) {
  const settings *c = &t->set;
  const int large = list == TACTIX_STAGE_LIST_LARGE_DOMAIN;
  const int unlimited = (int)c->inprocess.structured_root_sr_discovery_unlimited;
  const uint64_t hard = unlimited ? UINT64_MAX : c->inprocess.structured_root_sr_discovery_ticks;
  uint64_t window = 0;
  if (c->inprocess.structured_root_progress_feedback &&
      (!large || c->inprocess.structured_root_large_domain_progress_feedback)) {
    const uint64_t scale = c->inprocess.structured_root_progress_ticks_per_variable;
    const uint64_t variables = observe(t, TACTIX_OBSERVE_VARIABLES);
    const uint64_t per_variable = scale && variables > UINT64_MAX / scale ? UINT64_MAX : scale * variables;
    window = c->inprocess.structured_root_progress_minimum_ticks > per_variable
                 ? c->inprocess.structured_root_progress_minimum_ticks
                 : per_variable;
  }
  uint64_t granted = hard;
  if (window && (unlimited || hard > window)) granted = window;
  tactix_sr_discovery_begin(t->s, granted);
  int prior_progress = 0;
  const uint64_t amo_before = *t->meters.sr_unique_literal_amo_binaries;
  const uint64_t nonforcing_before = *t->meters.sr_nonforcing_cuts_declined;
  for (int index = 0; index < c->inprocess.structured_stages[list]; ++index) {
    const unsigned feedback =
        (prior_progress ? TACTIX_STAGE_FEEDBACK_PROGRESS : 0u) |
        (*t->meters.sr_unique_literal_amo_binaries > amo_before ? TACTIX_STAGE_FEEDBACK_UNIQUE_LITERAL_AMO
                                                                : 0u) |
        (*t->meters.sr_nonforcing_cuts_declined > nonforcing_before ? TACTIX_STAGE_FEEDBACK_NONFORCING_CUT
                                                                    : 0u);
    int exhaustion;
    if (tactix_stage_configure(t->s, list, index, feedback, &exhaustion)) return 0;
    int effective = 0;
    while (!tactix_stage_complete(t->s)) {
      tactix_stage_result advanced;
      tactix_stage_advance(t->s, window, hard, &advanced);
      effective += advanced.made_rule_progress;
      /* Progress (and a required discovery entry) earns the window again. */
      uint64_t units = advanced.discovery_progress_units > UINT64_MAX - (uint64_t)advanced.made_rule_progress
                           ? UINT64_MAX
                           : advanced.discovery_progress_units + (uint64_t)advanced.made_rule_progress;
      if (advanced.discovery_entry_required && units != UINT64_MAX) ++units;
      if (window && units && !observe(t, TACTIX_OBSERVE_SR_DISCOVERY_OVERFLOW)) {
        const uint64_t consumed = observe(t, TACTIX_OBSERVE_SR_DISCOVERY_CONSUMED);
        const uint64_t base = granted > consumed ? granted : consumed;
        const uint64_t credit = window > UINT64_MAX / units ? UINT64_MAX : window * units;
        uint64_t renewed = credit > UINT64_MAX - base ? UINT64_MAX : base + credit;
        if (!unlimited && renewed > hard) renewed = hard;
        if (renewed > granted) {
          granted = renewed;
          tactix_sr_discovery_limit(t->s, renewed);
        }
      }
      switch (advanced.status) {
      case TACTIX_ONLINE_COMPLETED:
      case TACTIX_ONLINE_NO_CHANGE:
      case TACTIX_ONLINE_SCOPED_FIXPOINT:
        continue;
      case TACTIX_ONLINE_SAT:
        return SAT;
      case TACTIX_ONLINE_UNSAT:
        return UNSAT;
      case TACTIX_ONLINE_BUDGET_EXHAUSTED:
        if (exhaustion == TACTIX_STAGE_EXHAUSTION_CONTINUE ||
            (exhaustion == TACTIX_STAGE_EXHAUSTION_FINISH_ROUND && !tactix_stage_complete(t->s)))
          continue;
        return 0;
      case TACTIX_ONLINE_CONFLICT:
        fprintf(stderr, "tactix-default: root preprocessing returned a conflict\n");
        abort();
      default: /* every committed atomic transition stays; nothing is retried */
        return 0;
      }
    }
    prior_progress |= effective != 0;
  }
  return observe(t, TACTIX_OBSERVE_INCONSISTENT) ? UNSAT : 0;
}

int ds_run_structured_root(strategy *t) {
  if (t->structured_consumed || *t->conflicts) return 0;
  t->structured_consumed = 1;
  fastel_ownership *o = &t->ownership;
  free(o->factor_ranges);
  memset(o, 0, sizeof *o);
  o->external_begin = o->external_end = external_extent(t);
  const uint64_t trail_before = observe(t, TACTIX_OBSERVE_TRAIL);
  const uint64_t applied_before = *t->meters.sr_applied;
  const uint64_t variables = observe(t, TACTIX_OBSERVE_VARIABLES);
  const int list = variables > t->set.inprocess.structured_root_large_domain_limit
                       ? TACTIX_STAGE_LIST_LARGE_DOMAIN
                   : variables >= t->set.inprocess.structured_root_backtracking_domain_limit
                       ? TACTIX_STAGE_LIST_BOUNDED_BACKTRACKING
                       : TACTIX_STAGE_LIST_ORDINARY;
  int result = 0;
  if (t->set.inprocess.structured_stages[list]) {
    support(t, TACTIX_SUPPORT_ENTER_CONTEXT, TACTIX_CONTEXT_INPROCESSING);
    result = structured_stages(t, list);
    support(t, TACTIX_SUPPORT_LEAVE_CONTEXT, 0);
  }
  o->grew_root_trail = observe(t, TACTIX_OBSERVE_TRAIL) > trail_before;
  o->symmetry_applied = *t->meters.sr_applied > applied_before;
  o->external_end = external_extent(t);
  return result;
}

/* initial_fast_elimination_extension_candidates: the structured cohort's
 * live extension variables FastEl would offer within its limits (round one
 * counts the positive population twice, as the owner's gather does). */
static uint64_t fastel_extension_candidates(strategy *t, const tactix_online_bve_parameters *p) {
  const fastel_ownership *o = &t->ownership;
  uint64_t cohort = o->external_end > o->external_begin ? o->external_end - o->external_begin : 0;
  for (size_t i = 0; i < o->factor_range_count; ++i)
    cohort = saturated_sum(cohort, o->factor_ranges[i][1] - o->factor_ranges[i][0]);
  if (!cohort) return 0;
  const uint32_t extent = external_extent(t);
  /* Per external variable: positive, negative, oversized; UINT64_MAX marks
   * a variable outside the extension set. */
  uint64_t (*counts)[3] = calloc((size_t)extent + 1, sizeof *counts);
  if (!counts) abort();
  int any = 0;
  for (uint32_t external = 0; external < extent; ++external) {
    const int variable = (int)external + 1;
    counts[variable][0] = UINT64_MAX;
    if (tactix_observe(t->s, TACTIX_OBSERVE_EXTENSION_VARIABLE, (uint64_t)variable) != 1 ||
        !(directly_owns(o, external) || factor_owns(o, external)) ||
        tactix_observe(t->s, TACTIX_OBSERVE_VARIABLE_OPEN, (uint64_t)variable) != 1 ||
        tactix_observe(t->s, TACTIX_OBSERVE_BVE_RETRY, (uint64_t)variable) != 1)
      continue;
    counts[variable][0] = 0;
    any = 1;
  }
  uint64_t eligible = 0;
  if (any) {
    int *literals = NULL;
    int capacity = 0;
    /* FastEl rejects an oversized clause before testing root satisfaction. */
    for (uint64_t clause = tactix_clause_after(t->s, UINT64_MAX); clause != UINT64_MAX;
         clause = tactix_clause_after(t->s, clause)) {
      unsigned flags;
      int size = tactix_clause(t->s, clause, literals, capacity, &flags);
      if (size > capacity) {
        capacity = size;
        literals = realloc(literals, (size_t)capacity * sizeof *literals);
        if (!literals) abort();
        size = tactix_clause(t->s, clause, literals, capacity, &flags);
      }
      if (flags & (TACTIX_CLAUSE_GARBAGE | TACTIX_CLAUSE_REDUNDANT)) continue;
      const int oversized = (uint64_t)size > p->clause_size_limit;
      if (!oversized && (flags & TACTIX_CLAUSE_SATISFIED)) continue;
      for (int j = 0; j < size; ++j) {
        const int variable = literals[j] < 0 ? -literals[j] : literals[j];
        if ((uint32_t)variable > extent || counts[variable][0] == UINT64_MAX) continue;
        if (oversized)
          counts[variable][2] = 1;
        else
          ++counts[variable][literals[j] < 0 ? 1 : 0];
      }
    }
    /* Binary occurrences: distinct active partners, as FastEl's dense entry
     * keeps one copy of a repeated binary. */
    for (uint32_t variable = 1; variable <= extent; ++variable) {
      if (counts[variable][0] == UINT64_MAX) continue;
      for (int sign = 0; sign < 2; ++sign) {
        const int literal = sign ? -(int)variable : (int)variable;
        const int n = tactix_binary_partners(t->s, literal, NULL, 0);
        if (n > capacity) {
          capacity = n;
          literals = realloc(literals, (size_t)capacity * sizeof *literals);
          if (!literals) abort();
        }
        tactix_binary_partners(t->s, literal, literals, n);
        int kept = 0;
        for (int j = 0; j < n; ++j) {
          const int partner = literals[j] < 0 ? -literals[j] : literals[j];
          if (tactix_literal_value(t->s, literals[j]) > 0 ||
              tactix_observe(t->s, TACTIX_OBSERVE_VARIABLE_RETIRED, (uint64_t)partner) == 1)
            continue;
          int duplicate = 0;
          for (int m = 0; m < kept && !duplicate; ++m) duplicate = literals[m] == literals[j];
          if (!duplicate) literals[kept++] = literals[j];
        }
        counts[variable][sign] += (uint64_t)kept;
      }
      const uint64_t positive = counts[variable][0], negative = counts[variable][1];
      if (counts[variable][2] || positive > p->occurrence_limit / 2 || positive > p->occurrence_limit ||
          negative > p->occurrence_limit)
        continue;
      eligible += saturated_sum(positive, negative) <= p->occurrence_limit;
    }
    free(literals);
  }
  free(counts);
  return eligible;
}

int ds_run_phase(strategy *t, int group) {
  const settings *c = &t->set;
  slot_clock *clock = &t->clocks[group];
  clock->reductions_mark = *t->reductions;
  clock->marks_mark = *t->bve_marks;
  clock->fixed_mark = stat(t, TACTIX_STAT_ROOT_FIXED);
  clock->blocked = 0;
  /* One embedded Kitten per complete eliminate() invocation. */
  if (group == ELIMINATE && !t->fastel_active)
    support(t, TACTIX_SUPPORT_BEGIN_BVE_DEFINITION_SESSION, 0);
  install_inprocess_profiles(t);
  t->forward_pair_limit = 0;
  t->bve_ran_this_phase = 0;
  const uint64_t factor_snapshot = c->inprocess.couple_factor_bound ? t->bve_level : 0;
  /* Probe sets Kissat's probing flag; Eliminate keeps target capture. */
  if (support(t, TACTIX_SUPPORT_ROOT_PREPARATION,
              (group == PROBE ? TACTIX_ROOT_PREPARATION_PROBING
                              : TACTIX_ROOT_PREPARATION_CAPTURE_TARGET) |
                  TACTIX_ROOT_PREPARATION_CONSUME_ROOT_PREFIX) == UNSAT)
    return UNSAT;
  const uint64_t kernel_bound = observe(t, TACTIX_OBSERVE_FACTOR_BOUND);
  const uint64_t factor_bound = kernel_bound > factor_snapshot ? kernel_bound : factor_snapshot;
  support(t, TACTIX_SUPPORT_ENTER_INPROCESS_PHASE,
          (group == ELIMINATE ? TACTIX_PHASE_ELIMINATE : 0) |
              (factor_bound << TACTIX_PHASE_FACTOR_BOUND_SHIFT));
  const int structured = group == PROBE ? ds_run_structured_root(t) : 0;
  if (t->unported) {
    support(t, TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE, 0);
    return 0;
  }
  support(t, TACTIX_SUPPORT_OPEN_OCCURRENCE_SESSION, 0);
  phase_context ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.group = group;
  ctx.phase_unsat = structured == UNSAT;
  ctx.last_result = NOOP;
  uint64_t active_before = observe(t, TACTIX_OBSERVE_ACTIVE_VARIABLES);
  const uint64_t rounds = t->fastel_active  ? c->inprocess.initial_fast_elimination_rounds
                          : *t->conflicts ? c->inprocess.phase_rounds
                                          : 1;
  for (; (uint64_t)ctx.outer < rounds && !ctx.phase_unsat && !(t->stop && *t->stop) && !t->unported;
       ctx.outer++) {
    /* SET_EFFORT_LIMIT(forward_steps) is per elimination round. */
    t->forward_pair_limit = 0;
    ctx.round_changed = 0;
    support(t, TACTIX_SUPPORT_BEGIN_FORWARD_ROUND, 0);
    run_slots(t, &ctx);
    if (ctx.eliminate_limit_reached) break;
    const int subsume_pending = group == ELIMINATE && c->inprocess.hold_subsume_until_eliminate &&
                                observe(t, TACTIX_OBSERVE_FORWARD_COMPLETE) != 3;
    const uint64_t active_now = observe(t, TACTIX_OBSERVE_ACTIVE_VARIABLES);
    if (active_now == active_before && !subsume_pending) break; /* outer fixpoint */
    active_before = active_now;
  }
  release_held_retirements(t);
  int unsat = ctx.phase_unsat;
  if (support(t, TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE, 0) == UNSAT ||
      observe(t, TACTIX_OBSERVE_INCONSISTENT))
    unsat = 1;
  if (t->unported) return 0;
  /* The BVE growth-bound ladder: a completed engaged pass climbs one rung and
   * reschedules every variable; the top rung arms the new-material marks. */
  if (group == ELIMINATE && t->bve_ran_this_phase && !t->fastel_active && !unsat) {
    const uint32_t last = bve_passes(c) - 1;
    if (t->bve_level_index >= last) {
      clock->marks_mark = *t->bve_marks;
      clock->fixed_mark = stat(t, TACTIX_STAT_ROOT_FIXED);
      clock->material_mark = observe(t, TACTIX_OBSERVE_SUBSUME_MATERIAL);
    } else {
      const uint64_t next = bve_bound(c, ++t->bve_level_index), top = bve_bound(c, last);
      t->bve_level = next < top ? next : top;
      support(t, TACTIX_SUPPORT_RESCHEDULE_BVE, 0);
      clock->marks_mark = 0;
      clock->fixed_mark = 0;
    }
  }
  /* kissat_probe_initially does not advance last.ticks.probe. */
  if (!t->fastel_active && (group != PROBE || *t->conflicts)) clock->anchor = *t->search_ticks;
  if (unsat) return UNSAT;
  if (!t->fastel_active) {
    clock->rounds++;
    ds_update_phase_limit(t, group);
  }
  return OK;
}

/* run_initial_fast_elimination: SC2024's bounded startup pass, one Eliminate
 * phase of BVE alone over the owner's candidate domain; fastel.c ends by
 * making every variable a BVE candidate again. */
int ds_run_initial_fast_elimination(strategy *t) {
  /* A structured run that grew the root through symmetry keeps FastEl off
   * its own helper definitions when both meet in the candidate domain. */
  const fastel_ownership *o = &t->ownership;
  const uint64_t allowance = scale_permille(t->set.inprocess.preprocess_initial_ticks,
                                            t->set.inprocess.initial_fast_elimination_bve_permille);
  const uint64_t active = observe(t, TACTIX_OBSERVE_ACTIVE_VARIABLES);
  if (observe(t, TACTIX_OBSERVE_PROPAGATION_FIXPOINT) && o->grew_root_trail && o->symmetry_applied &&
      active && active <= allowance / 2) {
    const int bve = ds_rule("BVE");
    const tactix_online_parameters fastel =
        ds_read_profile(t, bve, TACTIX_ONLINE_PARAMETERS_BVE, TACTIX_PARAMETERS_STRATEGY);
    if (fastel_extension_candidates(t, &fastel.value.bve)) return 0;
  }
  t->fastel_active = 1;
  t->fastel_discovery_ticks = 0;
  t->fastel_budget_exhausted = t->fastel_admitted = 0;
  const int result = ds_run_phase(t, ELIMINATE);
  if (!t->unported) support(t, TACTIX_SUPPORT_RESCHEDULE_BVE, 0);
  t->fastel_active = 0;
  free(t->ownership.factor_ranges);
  memset(&t->ownership, 0, sizeof t->ownership);
  return result;
}
