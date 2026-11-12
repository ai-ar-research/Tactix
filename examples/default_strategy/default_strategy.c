/* The default strategy's solve entry and startup (DefaultPolicy::solve up to
 * the first search interval). Each function is the C form of the
 * DefaultPolicy member it names. */
#include "strategy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *last_unported;
const char *tactix_default_unported(void) { return last_unported; }

/* A strategy path not ported yet: stop the solve with an unknown result. */
int ds_unported(strategy *t, const char *what) {
  if (!t->unported) t->unported = what;
  return 0;
}

/* Session access. */

int ds_rule(const char *name) {
  for (int id = 0; id < tactix_rule_count(); ++id)
    if (!strcmp(tactix_rule_name(id), name)) return id;
  fprintf(stderr, "tactix-default: no rule %s\n", name);
  abort();
}

const uint64_t *ds_counter(tactix *s, const char *name) {
  const uint64_t *view = tactix_counter_view(s, tactix_counter_id(name));
  if (!view) {
    fprintf(stderr, "tactix-default: no counter %s\n", name);
    abort();
  }
  return view;
}

/* Startup. */

/* One Lucky opportunity behind its admission predicate: the terminal verdict
 * it proves (10 or 20, as `terminal` allows), else 0. */
static int lucky(strategy *t, uint64_t instance, int terminal) {
  if (tactix_instance_applicable(t->s, t->lucky, instance) != 1) return 0;
  const int code = tactix_apply_instance(t->s, t->lucky, instance);
  return code == terminal ? code : 0;
}

/* The whole-formula Lucky probes (early before preprocessing, late after). */
static int lucky_cascade(strategy *t) {
  if (tactix_rule_applicable(t->s, t->lucky) != 1) return 0;
  const int code = tactix_apply_instance(t->s, t->lucky, TACTIX_INSTANCE_CANONICAL);
  return code == SAT || code == UNSAT ? code : 0;
}

/* One AffineMatrixInitialize operation under `profile`, the installed
 * basis restored afterwards (the operation's profile is scoped to it). */
int ds_affine_operation(strategy *t, tactix_online_affine_matrix_initialize_parameters profile,
                            const tactix_online_parameters *basis, tactix_affine_matrix_result *out) {
  const int matrix = ds_rule("AffineMatrixInitialize");
  tactix_install_affine_matrix_profile(t->s, &profile);
  /* DefaultPolicy runs the matrix service directly: its published root units
   * reach H only through a later consumer. */
  tactix_set_guidance_publication(t->s, TACTIX_GUIDANCE_DISCARD);
  const int code = tactix_apply(t->s, matrix);
  tactix_set_guidance_publication(t->s, TACTIX_GUIDANCE_NATIVE);
  ds_install(t, matrix, basis);
  if (code == REFUSED) return ds_unported(t, "refused AffineMatrixInitialize");
  tactix_read_affine_matrix_result(t->s, out);
  return OK;
}

/* default_affine_root_closure: the startup-only GF(2) closure of a bounded
 * live formula (the checked Rank263 profile): a complete-snapshot matrix,
 * serviced to its fixpoint, then released. */
static int affine_root_closure(strategy *t) {
  if (!t->set.enabled.affine_root_closure || !observe(t, TACTIX_OBSERVE_STRUCTURAL_PROVENANCE))
    return OK;
  const uint64_t clauses = stat(t, TACTIX_STAT_IRREDUNDANT_CLAUSES);
  const uint64_t extent = observe(t, TACTIX_OBSERVE_EXTERNAL_VARIABLES);
#define CLOSURE(key) ds_integer_setting(t->s, "affine_root_closure." #key)
  const uint64_t additions = CLOSURE(maximum_proof_additions);
  if (!clauses || clauses > CLOSURE(maximum_clauses) || clauses > UINT32_MAX ||
      extent > INT32_MAX || additions > (uint64_t)INT32_MAX - extent)
    return OK;
  if (observe(t, TACTIX_OBSERVE_AFFINE_SESSION) & 16)
    return ds_unported(t, "affine root closure over a caller's matrix profile");
  /* Allowances derived from the exact live population: the admitted ternary
   * shape has arity 3, at most 3 literals per clause and output width 4. */
  tactix_online_parameters basis;
  memset(&basis, 0, sizeof basis);
  basis.kind = TACTIX_ONLINE_PARAMETERS_AFFINE_CONSEQUENCE;
  tactix_online_affine_consequence_parameters *b = &basis.value.affine_consequence;
  const uint32_t row_width = (uint32_t)CLOSURE(maximum_row_width);
  b->maximum_clauses = b->maximum_groups = b->maximum_patterns = b->maximum_session_keys = clauses;
  b->maximum_snapshot_literals = 3 * clauses;
  b->maximum_snapshot_work = CLOSURE(maximum_snapshot_work);
  b->maximum_elimination_work = CLOSURE(maximum_recognition_work);
  b->maximum_arity = 3;
  b->maximum_intermediate_width = b->basis_maximum_intermediate_width = row_width;
  b->maximum_sources = b->basis_maximum_sources = b->basis_maximum_premises =
      b->basis_maximum_nodes = (uint32_t)clauses;
  b->maximum_output_width = 4;
  b->maximum_proof_additions = additions;
  b->maximum_proof_deletions = CLOSURE(maximum_proof_deletions);
  b->maximum_journal_bytes = CLOSURE(maximum_journal_bytes);
  b->semantics = TACTIX_ONLINE_AFFINE_CONSEQUENCE_CERTIFIED_BASIS_V2;
  b->basis_maximum_work = CLOSURE(maximum_basis_work);
  b->basis_maximum_direct_width = 3;
  b->maximum_external_extent = (uint32_t)(extent + additions);
  b->maximum_session_work = CLOSURE(maximum_session_work);
#undef CLOSURE
  /* An invalid derived profile poses no closure. */
  if (tactix_install_parameters(t->s, ds_rule("AffineMatrixInitialize"), &basis)) return OK;
  tactix_online_affine_matrix_initialize_parameters profile;
  memset(&profile, 0, sizeof profile);
  profile.basis = *b;
  profile.admission = TACTIX_AFFINE_MATRIX_COMPLETE_SNAPSHOT;
  profile.operation = TACTIX_AFFINE_MATRIX_INITIALIZE;
  tactix_affine_matrix_result result;
  if (ds_affine_operation(t, profile, &basis, &result) || t->unported) return OK;
  if (result.initialization_status != TACTIX_AFFINE_INITIALIZATION_APPLIED) return OK;
  profile.operation = TACTIX_AFFINE_MATRIX_SEARCH_STEP; /* non-persistent: complete snapshot */
  for (;;) {
    ds_affine_operation(t, profile, &basis, &result);
    if (t->unported) return OK;
    if (result.service_status != TACTIX_AFFINE_SERVICE_APPLIED) break;
    if (result.service_result == TACTIX_RULE_RESULT_UNSAT) return UNSAT;
    if (!result.bcp_pending) abort();
    const int propagated = ds_propagate(t);
    if (propagated != OK) return propagated;
  }
  /* No dormant matrix survives the startup schedule. */
  support(t, TACTIX_SUPPORT_RELEASE_AFFINE_MATRIX, TACTIX_AFFINE_MATRIX_RELEASE_DETACHED);
  return OK;
}

/* reset_schedule: every clock from the current conflict and tick counts. */
static void reset_schedule(strategy *t) {
  const settings *c = &t->set;
  const uint64_t conflicts = *t->conflicts;
  memset(t->averages, 0, sizeof t->averages);
  ds_update_restart_limit(t);
  /* Kissat reduceinit, distinct from the post-reduction cadence. */
  t->reduce_next_conflicts = conflicts + c->reduction.initial_interval;
  memset(&t->mode, 0, sizeof t->mode);
  t->mode.conflicts_limit = conflicts + c->mode.initial_conflicts;
  t->mode.ticks_at_focus_entry = *t->search_ticks;
  ds_reset_reluctant(t);
  t->rephase.count = 0;
  t->rephase.next_conflicts = conflicts + c->rephase.initial_interval;
  t->reorder.count = 0;
  t->reorder.next_conflicts = conflicts + c->reorder.initial_interval;
  t->bump_delay.count = t->bump_delay.current = 0;
  t->retier.enabled = (int)c->reduction.dynamic_tiers;
  t->retier.max_interval = c->reduction.retier_max_interval;
  t->retier.growth = (uint32_t)c->reduction.retier_growth;
  t->retier.tier1_default = (uint32_t)c->reduction.tier1_default;
  t->retier.tier2_default = (uint32_t)c->reduction.tier2_default;
  t->retier.tier1_relative_permille = (uint32_t)c->reduction.tier1_relative_permille;
  t->retier.tier2_relative_permille = (uint32_t)c->reduction.tier2_relative_permille;
  t->retier.interval = c->reduction.retier_initial_interval;
  t->retier.next_conflicts =
      c->reduction.dynamic_tiers ? conflicts + t->retier.interval : t->retier.interval;
  /* DefaultBranching::reset */
  t->randec_remaining = 0;
  t->randec_next = conflicts + c->branching.random_initial_interval;
  t->randec_sequences = 0;
  t->randec_rng = c->branching.random_seed;
  t->focused_phase_epoch = UINT64_MAX;
  ds_inprocess_reset(t);
  memset(&t->graph_schedule, 0, sizeof t->graph_schedule);
}

/* The solve-entry profile installations. */
static void install_profiles(strategy *t) {
  const settings *c = &t->set;
  tactix_online_analyze_parameters *a = &t->analysis_profile.value.analyze;
  t->analysis_profile = ds_read_profile(t, t->conflict_analysis, TACTIX_ONLINE_PARAMETERS_ANALYZE,
                                     TACTIX_PARAMETERS_INSTALLED);
  a->chronological_levels = (uint32_t)c->analysis.chronological_levels;
  a->minimize_depth_limit = (uint32_t)c->analysis.minimize_depth_limit;
  a->conflict_reuse = (int)c->analysis.conflict_reuse;
  a->disable_jump_reasons = !c->analysis.jump_reasons;
  a->disable_shrink_glue_promotion = !c->analysis.shrink_glue_promote;
  a->exclude_extension_reason_bumps = !c->analysis.bump_extension_reasons;
  a->eager_subsume = (uint32_t)c->analysis.eager_subsume;
  ds_install(t, t->conflict_analysis, &t->analysis_profile);
  ds_install_configured(t, "Reorder", TACTIX_ONLINE_PARAMETERS_REORDER);
  static const int lucky_families[] = {
      TACTIX_ONLINE_PARAMETERS_PARITY_MODEL,
      TACTIX_ONLINE_PARAMETERS_ONEHOT_MODEL,
      TACTIX_ONLINE_PARAMETERS_AFFINE_MODEL,
      TACTIX_ONLINE_PARAMETERS_BALANCED_SIGNATURE_MODEL,
      TACTIX_ONLINE_PARAMETERS_FERMAT_MODEL,
      TACTIX_ONLINE_PARAMETERS_LOOKUP_CIRCUIT_MODEL,
      TACTIX_ONLINE_PARAMETERS_SIMON_MODEL,
      TACTIX_ONLINE_PARAMETERS_STCONN_PROOF,
      TACTIX_ONLINE_PARAMETERS_SIGNED_CARDINALITY_PROOF,
      TACTIX_ONLINE_PARAMETERS_EXACT_COVER_MODEL,
      TACTIX_ONLINE_PARAMETERS_INDEPENDENT_SET_MODEL,
      TACTIX_ONLINE_PARAMETERS_GRAPH_LABELING,
      TACTIX_ONLINE_PARAMETERS_RAMSEY_STARTUP,
  };
  for (size_t i = 0; i < sizeof lucky_families / sizeof lucky_families[0]; ++i)
    ds_install_configured(t, "Lucky", lucky_families[i]);
  tactix_online_parameters reduce =
      ds_read_profile(t, t->reduce_rule, TACTIX_ONLINE_PARAMETERS_REDUCE, TACTIX_PARAMETERS_INSTALLED);
  reduce.value.reduce.victim_numerator = (uint32_t)c->reduction.victim_numerator;
  reduce.value.reduce.victim_denominator = (uint32_t)c->reduction.victim_denominator;
  reduce.value.reduce.tier_protection_mode = setting_mode(c->reduction.protect_tiers);
  reduce.value.reduce.compact_collection_mode = setting_mode(c->reduction.canonical_compact_collection);
  reduce.value.reduce.reduction_bookkeeping_mode = setting_mode(c->reduction.bookkeeping);
  ds_install(t, t->reduce_rule, &reduce);
  ds_install_configured(t, "Walk", TACTIX_ONLINE_PARAMETERS_WALK);
}

/* The complementary-ternary census that selects configuration-checking
 * Walk; the scan runs only when its upper bound can reach the threshold. */
static void configuration_checking_census(strategy *t) {
  const settings *c = &t->set;
  const uint64_t large = observe(t, TACTIX_OBSERVE_LIVE_LARGE_IRREDUNDANT);
  const uint64_t live = observe(t, TACTIX_OBSERVE_LIVE_BINARY) + large;
  uint64_t bound = live ? PERMILLE * large / live : 0;
  if (bound > PERMILLE) bound = PERMILLE;
  const int census = live >= c->rephase.configuration_checking_minimum_clauses &&
                     bound >= c->rephase.complementary_ternary_minimum_permille;
  t->cc_permille = census ? observe(t, TACTIX_OBSERVE_COMPLEMENTARY_TERNARY_PERMILLE) : 0;
  t->cc_walk = census && t->cc_permille >= c->rephase.complementary_ternary_minimum_permille;
  t->cc_deferred = t->cc_walk;
  t->cc_applications = 0;
  t->cc_release_conflicts = UINT64_MAX;
  t->cc_next_conflicts = t->cc_walk ? *t->conflicts : UINT64_MAX;
}

/* The structured program before early Lucky, and the one BCE pass that
 * retires unique-literal AMO helpers once a later structured consumer used
 * them without fixing a graph root (default_should_cleanup_unique_literal_amo). */
static int structured_root_before_lucky(strategy *t) {
  support(t, TACTIX_SUPPORT_MATERIALIZE_COLD_STATE, 0); /* its stages read it */
  const uint64_t amo = *t->meters.sr_unique_literal_amo_binaries;
  const uint64_t later = *t->meters.sr_post_amo_nonhelper_applied;
  const uint64_t graph_roots = *t->meters.sr_post_amo_graph_root_units;
  const int result = ds_run_structured_root(t);
  if (result == SAT || result == UNSAT) return result;
  if (t->stop && *t->stop) return STOPPED;
  if (t->set.inprocess.structured_root_bce_after_unique_literal_amo &&
      *t->meters.sr_unique_literal_amo_binaries != amo &&
      *t->meters.sr_post_amo_nonhelper_applied != later &&
      *t->meters.sr_post_amo_graph_root_units == graph_roots) {
    if (!observe(t, TACTIX_OBSERVE_PROPAGATION_FIXPOINT))
      return ds_unported(t, "unpropagated root after structured stages");
    const int bce = ds_rule("BCE");
    if (tactix_rule_applicable(t->s, bce) == 1) {
      support(t, TACTIX_SUPPORT_ENTER_CONTEXT, TACTIX_CONTEXT_INPROCESSING);
      const int code = tactix_apply(t->s, bce);
      support(t, TACTIX_SUPPORT_LEAVE_CONTEXT, 0);
      if (code == UNSAT) return UNSAT;
    }
    if (t->stop && *t->stop) return STOPPED;
  }
  return 0;
}

/* Startup through preprocessing: 10/20 for a verdict, STOPPED when
 * interrupted, 0 to search. */
static int startup(strategy *t) {
  const settings *c = &t->set;
  int verdict;
  support(t, TACTIX_SUPPORT_ENTER_CONTEXT, TACTIX_CONTEXT_STARTUP);
  t->contexts++;
  /* The original-input opportunities, then the certificate window closes. */
  if (c->enabled.ramsey_startup && lucky(t, TACTIX_LUCKY_STARTUP_RAMSEY, UNSAT)) return UNSAT;
  if (lucky(t, TACTIX_LUCKY_STARTUP_PARITY, UNSAT)) return UNSAT;
  if (c->enabled.startup_count && lucky(t, TACTIX_LUCKY_STARTUP_COUNT, UNSAT)) return UNSAT;
  support(t, TACTIX_SUPPORT_CLOSE_STARTUP_CERTIFICATE, 0);
  if (ds_propagate(t) != OK) return UNSAT;
  /* Graph labeling authenticates once at the initial root (Lucky 26). */
  if (c->enabled.graph_labeling_model && !stat(t, TACTIX_STAT_LEVEL) &&
      tactix_instance_applicable(t->s, t->lucky, TACTIX_LUCKY_GRAPH_AUTHENTICATE) == 1) {
    (void)tactix_apply_instance(t->s, t->lucky, TACTIX_LUCKY_GRAPH_AUTHENTICATE);
    if (observe(t, TACTIX_OBSERVE_GRAPH_LABELING) & 4) return STOPPED;
  }
  verdict = affine_root_closure(t);
  if (verdict == UNSAT || verdict == CONFLICT) return UNSAT;
  if (t->unported) return 0;
  /* Bounded theory-specific owners at the untouched imported root. */
  if (c->enabled.signed_cardinality_proof && lucky(t, TACTIX_LUCKY_SIGNED_CARDINALITY_PROOF, UNSAT))
    return UNSAT;
  if (c->enabled.exact_cover_model && lucky(t, TACTIX_LUCKY_EXACT_COVER_MODEL, SAT)) return SAT;
  if (c->enabled.independent_set_model && lucky(t, TACTIX_LUCKY_INDEPENDENT_SET_MODEL, SAT))
    return SAT;
  if (c->enabled.stconn_proof && lucky(t, TACTIX_LUCKY_STCONN_PROOF, UNSAT)) return UNSAT;
  if (c->enabled.balanced_signature_model && lucky(t, TACTIX_LUCKY_BALANCED_SIGNATURE_MODEL, SAT))
    return SAT;
  if (c->enabled.fermat_model && lucky(t, TACTIX_LUCKY_FERMAT_MODEL, SAT)) return SAT;
  if (c->enabled.cerny_model && lucky(t, TACTIX_LUCKY_CERNY_MODEL, SAT)) return SAT;
  if (c->enabled.lookup_circuit_model && lucky(t, TACTIX_LUCKY_LOOKUP_CIRCUIT_MODEL, SAT)) return SAT;
  if (c->enabled.simon_model && lucky(t, TACTIX_LUCKY_SIMON_MODEL, SAT)) return SAT;
  if (lucky(t, TACTIX_LUCKY_PARITY_PROOF, UNSAT)) return UNSAT;
  if (c->enabled.parity_model && lucky(t, TACTIX_LUCKY_PARITY_MODEL, SAT)) return SAT;
  /* The exact-one proof reads the parser's binary order: before any
   * structured-root rule adds helpers. */
  if (lucky(t, TACTIX_LUCKY_EXACT_ONE_PROOF, UNSAT)) return UNSAT;
  /* Symmetry detection may depend on adjacency order: the structured program
   * runs on the imported root before speculative Lucky propagation. */
  if (!t->cc_deferred && c->inprocess.structured_root_before_lucky &&
      c->inprocess.structured_stages[0] && (verdict = structured_root_before_lucky(t)))
    return verdict;
  if (t->unported) return 0;
  if (c->enabled.onehot_model && lucky(t, TACTIX_LUCKY_DIRECT_CSP_MODEL, SAT)) return SAT;
  if (observe(t, TACTIX_OBSERVE_INPUT_BINARY_ORDER))
    support(t, TACTIX_SUPPORT_RELEASE_INPUT_ORDER, 0);
  if ((verdict = lucky_cascade(t))) return verdict;
  if (c->enabled.onehot_model && lucky(t, TACTIX_LUCKY_ONEHOT_MODEL, SAT)) return SAT;
  /* A deferred startup (tactix_defer_startup_state) crosses its first consumer
   * here: state neither root propagation nor early Lucky reads materializes,
   * and the branching heuristic attaches in the retained activation order. */
  support(t, TACTIX_SUPPORT_MATERIALIZE_COLD_STATE, 0);
  if (support(t, TACTIX_SUPPORT_ATTACH_HEURISTICS, 0) == 0 &&
      c->inprocess.structured_root_residual_search_handoff)
    return ds_unported(t, "residual search handoff");
  /* Kissat preprocess.c: propagate, then sparse-collect the root units away
   * before the first Probe technique. */
  int collected;
  do {
    collected = support(t, TACTIX_SUPPORT_SPARSE_COLLECT, 0);
    if (collected == UNSAT || (collected == 2 && ds_propagate(t) == UNSAT)) return UNSAT;
  } while (collected == 2);
  /* Kissat's initial compaction of root-fixed variables, for randec. */
  support(t, TACTIX_SUPPORT_COMPACT_RANDOM_ORDER, 0);
  if (!t->cc_deferred && ds_want_phase(t, PROBE) && (verdict = ds_run_phase(t, PROBE))) return verdict;
  if (t->unported) return 0;
  if (!t->cc_deferred && c->inprocess.initial_fast_elimination &&
      (verdict = ds_run_initial_fast_elimination(t)))
    return verdict;
  if (t->unported) return 0;
  if (ds_propagate(t) != OK) return UNSAT;
  /* CryptoMiniSat's startup XOR detection after elimination. */
  if (ds_start_affine_search(t) == UNSAT) return UNSAT;
  if (t->unported) return 0;
  if (c->rephase.late_lucky && (verdict = lucky_cascade(t))) return verdict;
  if (c->enabled.graph_labeling_model && (observe(t, TACTIX_OBSERVE_GRAPH_LABELING) & 1) &&
      ds_run_graph_labeling(t) == SAT)
    return SAT;
  close_retention(t, 1);
  support(t, TACTIX_SUPPORT_LEAVE_CONTEXT, 0);
  t->contexts--;
  /* Preprocessing consumed the root-unit prefix (Kissat clears it). */
  support(t, TACTIX_SUPPORT_CONSUME_ROOT_PREFIX, 0);
  /* Kissat classifies after startup simplification and again entering
   * search; both are reproduced. */
  ds_update_formula_classification(t);
  ds_update_formula_classification(t);
  if (!t->search_limits_initialized) {
    ds_update_phase_limit(t, ELIMINATE); /* kissat_init_limits from start_search */
    t->search_limits_initialized = 1;
  }
  t->mode.ticks_at_focus_entry = *t->search_ticks;
  return 0;
}

int tactix_default_solve(tactix *s, volatile sig_atomic_t *stop) {
  strategy *t = calloc(1, sizeof *t);
  if (!t) return 0;
  t->s = s;
  t->stop = stop;
  tactix_set_guidance_publication(s, TACTIX_GUIDANCE_NATIVE);
  tactix_set_interrupt(s, stop);
  ds_read_settings(s, &t->set);
  t->propagate = ds_rule("Propagate");
  t->conflict_analysis = ds_rule("ConflictAnalysis");
  t->restart_rule = ds_rule("Restart");
  t->reduce_rule = ds_rule("Reduce");
  t->switchmode = ds_rule("SwitchMode");
  t->rephase_rule = ds_rule("Rephase");
  t->reorder_rule = ds_rule("Reorder");
  t->walk = ds_rule("Walk");
  t->lucky = ds_rule("Lucky");
  t->conflicts = ds_counter(s, "conflicts");
  t->decisions = ds_counter(s, "decisions");
  t->search_ticks = ds_counter(s, "search_ticks");
  t->restarts = ds_counter(s, "restarts");
  t->reductions = ds_counter(s, "reductions");
  t->learned = ds_counter(s, "learned");
  t->learned_glue_sum = ds_counter(s, "learned_glue_sum");
  t->backjumps = ds_counter(s, "backjumps");
  t->reason_bump_rounds = ds_counter(s, "reason_bump_rounds");
  t->reason_bump_aborted = ds_counter(s, "reason_bump_aborted");
  t->bve_marks = ds_counter(s, "bve_marks");
  t->used_glue = tactix_counter_id("used_glue");
#define METER(name) t->meters.name = ds_counter(s, #name)
  METER(bve_eliminated);
  METER(vivify_checked);
  METER(vivify_subsumed);
  METER(vivify_shrunken);
  METER(vivify_implied);
  METER(vivify_asymmetric);
  METER(vivify_instantiated);
  METER(sr_applied);
  METER(sr_unique_literal_amo_binaries);
  METER(sr_nonforcing_cuts_declined);
  METER(sr_post_amo_nonhelper_applied);
  METER(sr_post_amo_graph_root_units);
#undef METER
  t->stable = (int)stat(t, TACTIX_STAT_STABLE);
  tactix_read_retier_schedule(s, &t->idle_retier);

  /* The strategy selects an initial subset of substitution/elimination
   * choices; explicit rule applications keep their complete action surface. */
  t->retaining = observe(t, TACTIX_OBSERVE_VARIABLE_RETENTION) != 0;
  t->retention_open = support(t, TACTIX_SUPPORT_STARTUP_RETENTION, 1) == 0;
  int verdict = 0;
  if (t->set.inprocess.cadical_arena_ordering_enabled) {
    verdict = ds_unported(t, "CaDiCaL arena ordering");
  } else if (observe(t, TACTIX_OBSERVE_INCONSISTENT)) {
    verdict = UNSAT;
  } else if ((stat(t, TACTIX_STAT_LEVEL) || observe(t, TACTIX_OBSERVE_CONFLICT)) &&
             support(t, TACTIX_SUPPORT_PREPARATION_RESTART, 0) == UNSAT) {
    verdict = UNSAT; /* solve-entry root normalization retires a recorded conflict */
  } else if (support(t, TACTIX_SUPPORT_RUN_REGIONS, 0) == UNSAT) {
    verdict = UNSAT; /* explicit public root work precedes elimination */
  } else if (observe(t, TACTIX_OBSERVE_REGIONS_PENDING)) {
    verdict = 0; /* interrupted: the program's cursor survives */
  } else {
    configuration_checking_census(t);
    ds_reset_affine_search(t);
    support(t, TACTIX_SUPPORT_RESET_GRAPH_LABELING_SESSION, 0);
    install_profiles(t);
    for (int mode = 0; mode != 2; ++mode) {
      t->retier.tier1_by_mode[mode] = (uint32_t)t->set.reduction.tier1_default;
      t->retier.tier2_by_mode[mode] =
          (uint32_t)max_u64(t->set.reduction.tier2_default, t->set.reduction.tier1_default);
    }
    ds_pose_vivify_tiers(t);
    reset_schedule(t);
    ds_inprocess_build(t);
    verdict = startup(t);
    if (!verdict && !t->unported) verdict = ds_search(t);
    ds_end_search_interval(t);
    if (verdict == STOPPED) verdict = 0;
  }
  close_retention(t, verdict != 0);
  while (t->contexts-- > 0) support(t, TACTIX_SUPPORT_LEAVE_CONTEXT, 0);
  tactix_set_interrupt(s, NULL);
  last_unported = t->unported;
  if (t->unported) verdict = 0;
  free(t->entries);
  free(t->candidate_orders);
  free(t->ownership.factor_ranges);
  free(t);
  return verdict;
}
