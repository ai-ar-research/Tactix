/* Internal interface of the C default strategy: the state DefaultPolicy
 * keeps (src/policies/default_policy.hpp) and what its modules share.
 * settings.c reads the configuration, search.c holds the CDCL search and its
 * controllers, inprocess.c the Probe and Eliminate phases, and
 * default_strategy.c startup and the solve entry. */
#ifndef TACTIX_DEFAULT_STRATEGY_H_INCLUDED
#define TACTIX_DEFAULT_STRATEGY_H_INCLUDED

#include "tactix_default.h"

#include <stdint.h>

/* ------------------------------------------------------------------------ */
/* Settings: the configuration snapshot keys this strategy reads.           */

typedef struct {
  struct {
    uint64_t random_initial_interval, random_sequence_length, random_seed;
    double logarithm_offset;
    uint64_t focused_phase_switch_span;
    int focused_phase[16];
    uint32_t focused_phases;
  } branching;
  struct {
    uint64_t minimize_depth_limit, bump_reason_limit, reason_depth;
    uint64_t chronological_levels, conflict_reuse, jump_reasons;
    uint64_t shrink_glue_promote, bump_extension_reasons, eager_subsume;
    uint64_t small_clause_limit, big_binary_fraction_permille;
    double bump_reason_rate;
  } analysis;
  struct {
    double glue_margin, fast_ema_window, slow_ema_window;
    double logarithm_offset, logarithm_bias;
    uint64_t initial_interval, reluctant_period, reluctant_limit;
  } restart;
  struct {
    uint64_t initial_conflicts, interval, logarithm_power, force;
    double logarithm_offset;
  } mode;
  struct {
    uint64_t initial_interval, arithmetic_interval, arithmetic_step;
    uint64_t compact_inactive_percent, retier_initial_interval;
    uint64_t retier_max_interval, retier_growth, tier1_default, tier2_default;
    uint64_t tier1_relative_permille, tier2_relative_permille;
    uint64_t reduce_tier1_mode, reduce_tier2_mode;
    uint64_t victim_numerator, victim_denominator, protect_tiers;
    uint64_t canonical_compact_collection, bookkeeping, square_root_cadence;
    uint64_t ramp_fraction, dynamic_tiers, compact_variables;
    double low_percent, high_percent, logarithm_offset;
  } reduction;
  struct {
    uint64_t initial_interval, interval, logarithm_power, walk_permille;
    uint64_t walk_inapplicable_fallback, walk_warmup, walk_use_branch_seed;
    uint64_t walk_random_seed, stable_only, late_lucky;
    uint64_t configuration_checking_walk_permille;
    uint64_t configuration_checking_minimum_step_budget;
    uint64_t configuration_checking_conflict_interval;
    uint64_t configuration_checking_minimum_clauses;
    uint64_t complementary_ternary_minimum_permille;
    uint64_t configuration_checking_maximum_tries;
    uint64_t configuration_checking_random_seed;
    uint64_t configuration_checking_flips_per_try;
    uint64_t configuration_checking_weight_threshold;
    uint64_t configuration_checking_smoothing_old_numerator;
    uint64_t configuration_checking_smoothing_denominator;
    uint64_t configuration_checking_aspiration;
    double logarithm_offset;
    uint8_t rotation[16];
    uint32_t rotations;
  } rephase;
  struct {
    uint64_t initial_interval, interval, mode, target;
  } reorder;
  struct {
    uint64_t parity_model, onehot_model, balanced_signature_model;
    uint64_t fermat_model, cerny_model, lookup_circuit_model, simon_model;
    uint64_t stconn_proof, signed_cardinality_proof, exact_cover_model;
    uint64_t independent_set_model, graph_labeling_model, ramsey_startup;
    uint64_t startup_count, affine_root_closure, affine_search;
  } enabled;
  struct {
    uint64_t preprocess_only, initial_fast_elimination;
    uint64_t cadical_arena_ordering_enabled, append_unlisted_rules;
    uint64_t structured_root_before_lucky, structured_root_backtracking_domain_limit;
    uint64_t structured_root_residual_search_handoff;
    uint64_t structured_root_large_domain_limit;
    uint64_t eliminate_interval, probe_interval, minimum_effort_ticks;
    uint64_t preprocess_initial_ticks, phase_rounds;
    uint64_t touch_filter, likely_kept_redundant;
    uint64_t bve_occurrence_limit, bve_clause_size_limit, bve_xor_arity_limit;
    uint64_t bve_resolvent_size_limit, bve_backward_occurrence_limit;
    uint64_t bve_definition_budget, bve_definition_cores;
    uint64_t bve_definition_core_budget_multiplier, bve_backward;
    uint64_t bve_on_the_fly_strengthening;
    uint64_t split_clocks, watermarks, hold_subsume_until_eliminate;
    uint64_t couple_factor_bound, vivify_tier_mode;
    uint64_t bve_enabled, vivify_enabled, factor_enabled, sweep_enabled;
    uint64_t factor_repeated_enabled, factor_active_ceiling, factor_initial_ticks;
    uint64_t factor_decade_round_cap, factor_decade_cap, factor_decade_offset;
    uint64_t factor_decade_base, initial_fast_elimination_bve_permille;
    uint64_t substitute_rounds, substitute_search_rounds, closure_delay;
    uint64_t preprocess_followups, forward_midround_budget;
    uint64_t vivify_irredundant_productivity_percent, vivify_redundant_share;
    uint64_t vivify_total_share, vivify_tier_share[3], vivify_tier_scope[3];
    uint64_t bve_default_ceiling, bve_bound[16], initial_fast_elimination_rounds;
    uint32_t bve_bounds;
    double logarithm_offset, scale_quadratic, scale_constant, scale_log_shift;
    uint64_t structured_root_sr_discovery_unlimited, structured_root_sr_discovery_ticks;
    uint64_t structured_root_progress_feedback, structured_root_large_domain_progress_feedback;
    uint64_t structured_root_progress_minimum_ticks, structured_root_progress_ticks_per_variable;
    uint64_t structured_root_bce_after_unique_literal_amo;
    int program_steps;   /* inprocess.step<i> entries */
    int structured_stages[3]; /* ordinary, bounded-backtracking, large-domain */
  } inprocess;
} settings;

/* ------------------------------------------------------------------------ */
/* Strategy state: DefaultPolicy's members.                                 */

typedef struct {
  double value, biased, alpha, beta, exp;
} smooth;

typedef struct {
  int initialized;
  smooth fast_glue, slow_glue, decision_rate;
  uint64_t saved_decisions;
} mode_averages;

/* DefaultRootServiceSchedule: Kissat's root-service occurrences and delay. */
typedef struct {
  uint64_t occurrences, next_conflicts, last_ticks;
  uint32_t delay_current, delay_count;
} root_service_schedule;

/* StructuredFastelOwnership: the external variables the structured-root
 * program and its coupled startup Factor introduced, which FastEl leaves to
 * that representation. External indices are 0-based. */
typedef struct {
  int grew_root_trail, symmetry_applied;
  uint32_t external_begin, external_end;
  uint32_t (*factor_ranges)[2];
  size_t factor_range_count, factor_range_capacity;
} fastel_ownership;

/* SlotClock (default_inprocess.hpp). */
typedef struct {
  uint64_t next_conflicts, rounds, reductions_mark, marks_mark, fixed_mark;
  uint64_t material_mark, anchor;
  int blocked;
} slot_clock;

enum { PROBE, ELIMINATE };

/* DefaultInprocessScheduler::SlotEntry: one occurrence of the program. */
typedef struct {
  uint64_t step_id, limit, cursor;
  int rule, group, enabled, preprocess, defer_preprocess, dosed, budget_aware;
  int instances, cursor_enabled, followup, order, tick_stat;
  int metered, shared_meter, delayed, delay_delta; /* tactix_strategy_read_rule */
  uint32_t permille, delay_permille, max_applies, applies, doses, delay_count, delay_current;
  const char *name;
  tactix_online_parameters parameters; /* the step's own profile, or INHERIT */
} slot_entry;

typedef struct strategy {
  tactix *s;
  volatile sig_atomic_t *stop;
  settings set;
  const char *unported;
  int contexts; /* ENTER_CONTEXT depth this solve owns */
  /* rule ids */
  int propagate, decide, analyze, restart_rule, reduce_rule, switchmode;
  int rephase_rule, reorder_rule, walk, lucky, bce, conflict_analysis;
  /* live counters */
  const uint64_t *conflicts, *decisions, *search_ticks, *restarts, *reductions;
  const uint64_t *learned, *learned_glue_sum, *backjumps;
  const uint64_t *reason_bump_rounds, *reason_bump_aborted, *bve_marks;
  int used_glue;
  int stable;
  /* DefaultRestartControl */
  mode_averages averages[2];
  uint64_t restart_next_conflicts;
  struct {
    uint64_t u, v, wait;
    int trigger;
  } reluctant;
  /* DefaultModeControl, DefaultDatabaseControl, DefaultPhaseControl */
  struct {
    uint64_t count, conflicts_limit, ticks_limit, ticks_at_focus_entry;
  } mode;
  uint64_t reduce_next_conflicts;
  struct {
    uint64_t next_conflicts, count;
  } rephase, reorder;
  /* DefaultBranching */
  uint64_t randec_remaining, randec_next, randec_sequences, randec_rng;
  uint64_t focused_phase_epoch;
  int focused_phase;
  /* ReasonBumpDelay */
  struct {
    uint32_t count, current;
  } bump_delay;
  tactix_retier_schedule retier;
  /* The ConflictAnalysis profile outside search (the neutral payload), the
   * retier schedule installed outside search, and whether a search interval
   * holds the per-conflict payload (with its bump limit) and the policy's
   * retier schedule installed instead. Only search-time ConflictAnalysis
   * reads either, so the interval keeps them installed across conflicts. */
  tactix_online_parameters analysis_profile;
  tactix_retier_schedule idle_retier;
  int in_search_interval;
  uint32_t installed_bump_limit;
  /* Configuration-checking Walk */
  int cc_walk, cc_deferred;
  uint32_t cc_applications;
  uint64_t cc_release_conflicts, cc_next_conflicts, cc_permille;
  /* DefaultInprocessScheduler */
  slot_clock clocks[2];
  int inprocess_initialized, search_limits_initialized, structured_consumed;
  uint64_t last_reduce_conflicts, bve_level;
  slot_entry *entries;
  int entry_count;
  int *candidate_orders;       /* per rule id: the last occurrence's order */
  uint32_t bve_level_index, bve_rounds_run;
  int bve_ran_this_phase, factor_large_formula, retirements_held;
  uint64_t factor_marked_watermark, forward_pair_limit;
  uint64_t order_rng, order_increment; /* the Random candidate order */
  fastel_ownership ownership;
  /* SC2024's bounded startup fast elimination (FastEl). */
  int fastel_active, fastel_budget_exhausted, fastel_admitted;
  uint64_t fastel_discovery_ticks;
  /* The startup retention scope (DefaultPolicy's InitialRetention): open,
   * and whether it holds any variable. */
  int retention_open, retaining;
  /* The retained root services. */
  root_service_schedule graph_schedule, affine_schedule;
  /* The persistent affine search: its usefulness sample (CryptoMiniSat's
   * autodisable), refresh clock, retained-input cursor and work ledgers. */
  struct {
    int live, paused;
    uint64_t calls, yield, checks, refresh_ticks, refresh_work;
    uint64_t initialization_work, service_work;
    tactix_affine_inputs inputs;
  } affine;
  /* The counters the scheduler's own decisions read. */
  struct {
    const uint64_t *bve_eliminated, *vivify_checked, *vivify_subsumed, *vivify_shrunken;
    const uint64_t *vivify_implied, *vivify_asymmetric, *vivify_instantiated;
    const uint64_t *sr_applied, *sr_unique_literal_amo_binaries, *sr_nonforcing_cuts_declined;
    const uint64_t *sr_post_amo_nonhelper_applied, *sr_post_amo_graph_root_units;
  } meters;
} strategy;

/* tactix_apply* result codes, and STOPPED: the strategy ends unknown. */
enum { OK = 0, NOOP = 1, CONFLICT = 2, SAT = 10, UNSAT = 20, REFUSED = -1, STOPPED = -2 };

__extension__ typedef unsigned __int128 wide; /* exact products of counts */

#define PERMILLE 1000
#define PERCENT 100

/* Substitute and BVE keep a variable the startup retention scope holds. */
static inline int retained(strategy *t, int variable) {
  return t->retaining && tactix_observe(t->s, TACTIX_OBSERVE_STARTUP_RETAINED, (uint64_t)variable) == 1;
}

/* Closing the scope releases its variables (a verdict, or entering search),
 * or keeps them posed (an interrupted startup). */
static inline void close_retention(strategy *t, int release) {
  if (!t->retention_open) return;
  tactix_support(t->s, TACTIX_SUPPORT_STARTUP_RETENTION, release ? 0 : 2);
  t->retention_open = t->retaining = 0;
}

static inline uint64_t saturated_sum(uint64_t a, uint64_t b) { return a > UINT64_MAX - b ? UINT64_MAX : a + b; }
static inline uint64_t saturated_product(uint64_t a, uint64_t b) {
  return a && b > UINT64_MAX / a ? UINT64_MAX : a * b;
}

static inline uint64_t scale_permille(uint64_t reference, uint64_t permille) {
  return reference / PERMILLE * permille + reference % PERMILLE * permille / PERMILLE;
}

static inline uint64_t observe(strategy *t, int what) { return tactix_observe(t->s, what, 0); }
static inline uint64_t stat(strategy *t, int id) { return tactix_stat(t->s, id); }
static inline int support(strategy *t, int id, uint64_t argument) {
  return tactix_support(t->s, id, argument);
}
static inline int setting_mode(uint64_t enabled) {
  return enabled ? TACTIX_ONLINE_SETTING_ENABLE : TACTIX_ONLINE_SETTING_DISABLE;
}

/* default_strategy.c */
int ds_unported(strategy *, const char *what);
int ds_rule(const char *name);
const uint64_t *ds_counter(tactix *, const char *name);
int ds_affine_operation(strategy *, tactix_online_affine_matrix_initialize_parameters profile,
                        const tactix_online_parameters *basis, tactix_affine_matrix_result *out);
/* settings.c */
uint64_t ds_integer_setting(tactix *, const char *key);
double ds_real_setting(tactix *, const char *key);
void ds_read_settings(tactix *, settings *);
/* search.c */
uint64_t ds_scale_inprocessing_delta(strategy *, uint64_t delta);
tactix_online_parameters ds_read_profile(strategy *, int rule_id, int kind, int source);
void ds_install(strategy *, int rule_id, const tactix_online_parameters *profile);
void ds_install_configured(strategy *, const char *rule_name, int kind);
void ds_pose_vivify_tiers(strategy *);
void ds_update_formula_classification(strategy *);
int ds_propagate(strategy *);
void ds_reset_reluctant(strategy *);
void ds_update_restart_limit(strategy *);
void ds_end_search_interval(strategy *);
int ds_search(strategy *);
/* inprocess.c */
void ds_inprocess_reset(strategy *);
void ds_inprocess_build(strategy *);
void ds_update_phase_limit(strategy *, int group);
int ds_want_phase(strategy *, int group);
int ds_run_phase(strategy *, int group);
int ds_run_initial_fast_elimination(strategy *);
int ds_run_structured_root(strategy *);
/* services.c */
int ds_graph_labeling_due(strategy *);
int ds_run_graph_labeling(strategy *);
void ds_reset_affine_search(strategy *);
int ds_start_affine_search(strategy *);
int ds_affine_search_step(strategy *);
int ds_affine_restart_service(strategy *);

#endif
