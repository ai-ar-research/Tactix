/* Internal interface of the C default strategy: the state DefaultPolicy
 * keeps (src/policies/default_policy.hpp) and what its modules share.
 * settings.c reads the configuration, search.c holds the CDCL search and its
 * controllers, inprocess.c the Probe and Eliminate phases, and
 * default_strategy.c startup and the solve entry. */
#ifndef TACTIX_DEFAULT_STRATEGY_H_INCLUDED
#define TACTIX_DEFAULT_STRATEGY_H_INCLUDED

#include "tactix_default.h"

#include <stdint.h>

/* Settings: the configuration keys this strategy reads, `group.key` in read
 * order (`key.enabled` for DS_ENABLED); U an integer, R a real. */

#define DS_BRANCHING(U, R, g) \
  U(g, random_initial_interval) U(g, random_sequence_length) U(g, random_seed) \
  R(g, logarithm_offset) U(g, focused_phase_switch_span)
#define DS_ANALYSIS(U, R, g) \
  U(g, minimize_depth_limit) U(g, bump_reason_limit) R(g, bump_reason_rate) U(g, reason_depth) \
  U(g, chronological_levels) U(g, conflict_reuse) U(g, jump_reasons) U(g, shrink_glue_promote) \
  U(g, bump_extension_reasons) U(g, eager_subsume) U(g, small_clause_limit) \
  U(g, big_binary_fraction_permille)
#define DS_RESTART(U, R, g) \
  R(g, glue_margin) U(g, initial_interval) R(g, fast_ema_window) R(g, slow_ema_window) \
  R(g, logarithm_offset) R(g, logarithm_bias) U(g, reluctant_period) U(g, reluctant_limit)
#define DS_MODE(U, R, g) \
  U(g, initial_conflicts) U(g, interval) R(g, logarithm_offset) U(g, logarithm_power) \
  U(g, force)
#define DS_REDUCTION(U, R, g) \
  U(g, initial_interval) U(g, arithmetic_interval) U(g, arithmetic_step) R(g, low_percent) \
  R(g, high_percent) R(g, logarithm_offset) U(g, compact_inactive_percent) \
  U(g, retier_initial_interval) U(g, retier_max_interval) U(g, retier_growth) \
  U(g, tier1_default) U(g, tier2_default) U(g, tier1_relative_permille) \
  U(g, tier2_relative_permille) U(g, reduce_tier1_mode) U(g, reduce_tier2_mode) \
  U(g, victim_numerator) U(g, victim_denominator) U(g, protect_tiers) \
  U(g, canonical_compact_collection) U(g, bookkeeping) U(g, square_root_cadence) \
  U(g, ramp_fraction) U(g, dynamic_tiers) U(g, compact_variables)
#define DS_REPHASE(U, R, g) \
  U(g, initial_interval) U(g, interval) R(g, logarithm_offset) U(g, logarithm_power) \
  U(g, walk_permille) U(g, walk_inapplicable_fallback) U(g, walk_warmup) \
  U(g, walk_use_branch_seed) U(g, walk_random_seed) U(g, configuration_checking_walk_permille) \
  U(g, configuration_checking_minimum_step_budget) \
  U(g, configuration_checking_conflict_interval) U(g, configuration_checking_minimum_clauses) \
  U(g, complementary_ternary_minimum_permille) U(g, configuration_checking_maximum_tries) \
  U(g, configuration_checking_random_seed) U(g, configuration_checking_flips_per_try) \
  U(g, configuration_checking_weight_threshold) \
  U(g, configuration_checking_smoothing_old_numerator) \
  U(g, configuration_checking_smoothing_denominator) U(g, configuration_checking_aspiration) \
  U(g, stable_only) U(g, late_lucky)
#define DS_REORDER(U, R, g) \
  U(g, initial_interval) U(g, interval) U(g, mode) U(g, target)
#define DS_ENABLED(U, R, g) \
  U(g, parity_model) U(g, onehot_model) U(g, balanced_signature_model) U(g, fermat_model) \
  U(g, cerny_model) U(g, lookup_circuit_model) U(g, simon_model) U(g, stconn_proof) \
  U(g, signed_cardinality_proof) U(g, exact_cover_model) U(g, independent_set_model) \
  U(g, graph_labeling_model) U(g, ramsey_startup) U(g, startup_count) U(g, affine_root_closure) \
  U(g, affine_search)
#define DS_INPROCESS(U, R, g) \
  U(g, preprocess_only) U(g, initial_fast_elimination) U(g, cadical_arena_ordering_enabled) \
  U(g, append_unlisted_rules) U(g, structured_root_before_lucky) \
  U(g, structured_root_residual_search_handoff) U(g, structured_root_backtracking_domain_limit) \
  U(g, structured_root_large_domain_limit) U(g, eliminate_interval) U(g, probe_interval) \
  U(g, minimum_effort_ticks) U(g, preprocess_initial_ticks) U(g, phase_rounds) \
  R(g, logarithm_offset) R(g, scale_quadratic) R(g, scale_constant) R(g, scale_log_shift) \
  U(g, touch_filter) U(g, likely_kept_redundant) U(g, bve_occurrence_limit) \
  U(g, bve_clause_size_limit) U(g, bve_xor_arity_limit) U(g, bve_resolvent_size_limit) \
  U(g, bve_backward_occurrence_limit) U(g, bve_definition_budget) U(g, bve_definition_cores) \
  U(g, bve_definition_core_budget_multiplier) U(g, bve_backward) \
  U(g, bve_on_the_fly_strengthening) U(g, split_clocks) U(g, watermarks) \
  U(g, hold_subsume_until_eliminate) U(g, couple_factor_bound) U(g, vivify_tier_mode) \
  U(g, bve_enabled) U(g, vivify_enabled) U(g, factor_enabled) U(g, sweep_enabled) \
  U(g, factor_repeated_enabled) U(g, factor_active_ceiling) U(g, factor_initial_ticks) \
  U(g, factor_decade_round_cap) U(g, factor_decade_cap) U(g, factor_decade_offset) \
  U(g, factor_decade_base) U(g, initial_fast_elimination_bve_permille) U(g, substitute_rounds) \
  U(g, substitute_search_rounds) U(g, closure_delay) U(g, preprocess_followups) \
  U(g, forward_midround_budget) U(g, vivify_irredundant_productivity_percent) \
  U(g, vivify_redundant_share) U(g, vivify_total_share) U(g, bve_default_ceiling) \
  U(g, initial_fast_elimination_rounds) U(g, structured_root_sr_discovery_unlimited) \
  U(g, structured_root_sr_discovery_ticks) U(g, structured_root_progress_feedback) \
  U(g, structured_root_large_domain_progress_feedback) \
  U(g, structured_root_progress_minimum_ticks) U(g, structured_root_progress_ticks_per_variable) \
  U(g, structured_root_bce_after_unique_literal_amo)

#define DS_U64(g, key) uint64_t key;
#define DS_REAL(g, key) double key;
typedef struct {
  struct {
    DS_BRANCHING(DS_U64, DS_REAL, _)
    int focused_phase[16];
    uint32_t focused_phases;
  } branching;
  struct { DS_ANALYSIS(DS_U64, DS_REAL, _) } analysis;
  struct { DS_RESTART(DS_U64, DS_REAL, _) } restart;
  struct { DS_MODE(DS_U64, DS_REAL, _) } mode;
  struct { DS_REDUCTION(DS_U64, DS_REAL, _) } reduction;
  struct {
    DS_REPHASE(DS_U64, DS_REAL, _)
    uint8_t rotation[16];
    uint32_t rotations;
  } rephase;
  struct { DS_REORDER(DS_U64, DS_REAL, _) } reorder;
  struct { DS_ENABLED(DS_U64, DS_REAL, _) } enabled;
  struct {
    DS_INPROCESS(DS_U64, DS_REAL, _)
    uint64_t vivify_tier_share[3], vivify_tier_scope[3], bve_bound[16];
    uint32_t bve_bounds;
    int program_steps;        /* inprocess.step<i> entries */
    int structured_stages[3]; /* ordinary, bounded-backtracking, large-domain */
  } inprocess;
} settings;
#undef DS_U64
#undef DS_REAL

/* Strategy state: DefaultPolicy's members. */

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
} slot_clock;

enum { PROBE, ELIMINATE };

/* DefaultInprocessScheduler::SlotEntry: one occurrence of the program. */
typedef struct {
  uint64_t limit, cursor;
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
  int propagate, restart_rule, reduce_rule, switchmode, rephase_rule, reorder_rule, walk, lucky;
  int conflict_analysis;
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
  /* The neutral ConflictAnalysis profile and retier schedule installed
   * outside search, and whether a search interval holds the per-conflict
   * payload (with its bump limit) and the policy's schedule instead; only
   * search-time ConflictAnalysis reads either, so they stay installed across
   * the interval's conflicts. */
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

static inline uint64_t max_u64(uint64_t a, uint64_t b) { return a > b ? a : b; }
static inline uint64_t min_u64(uint64_t a, uint64_t b) { return a < b ? a : b; }
static inline uint64_t nonzero_or(uint64_t value, uint64_t fallback) { return value ? value : fallback; }
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
tactix_online_step_v2 ds_online_step(int rule, const tactix_online_parameters *parameters);
int ds_online_run(strategy *, const tactix_online_step_v2 *, tactix_online_result *);
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
