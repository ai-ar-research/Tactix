/* The retained root services: graph labeling (Lucky 26/27) and the
 * persistent affine search. Both run at the initial root and in the root
 * window of a natural full restart, on Kissat's Eliminate-shaped schedule
 * (DefaultRootServiceSchedule); neither requests a restart. */
#include "strategy.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* kimits.h UPDATE_CONFLICT_LIMIT(eliminate, NLOG2N): eliminateint * n *
 * log10(n + 9)^2, scaled by kissat_scale_delta. */
static void schedule_occurrence(strategy *t, root_service_schedule *s) {
  ++s->occurrences;
  const double n = (double)s->occurrences;
  const double lg = log10(n + t->set.inprocess.logarithm_offset);
  const uint64_t delta = ds_scale_inprocessing_delta(
      t, (uint64_t)((double)t->set.inprocess.eliminate_interval * n * lg * lg));
  s->next_conflicts = saturated_sum(*t->conflicts, delta);
}

/* kimits.c kissat_delaying: a delayed occurrence still advances the count. */
static int delaying(strategy *t, root_service_schedule *s) {
  if (!s->delay_count) return 0;
  --s->delay_count;
  schedule_occurrence(t, s);
  return 1;
}

/* Halve the delay after a fruitful run, double it after a fruitless one. */
static void judge_run(root_service_schedule *s, int fruitful) {
  if (fruitful)
    s->delay_current /= 2;
  else
    s->delay_current = !s->delay_current ? 1
                       : s->delay_current > UINT32_MAX / 2 ? UINT32_MAX
                                                           : 2 * s->delay_current;
  s->delay_count = s->delay_current;
}

/* Graph labeling. */

enum { SLICE_NONE = 1, SLICE_PENDING = 2, SLICE_SAT = 4, SLICE_INTERRUPTED = 5 };

int ds_graph_labeling_due(strategy *t) {
  const uint64_t session = observe(t, TACTIX_OBSERVE_GRAPH_LABELING);
  if (!t->set.enabled.graph_labeling_model || !(session & 1) || (session & 2) ||
      *t->conflicts < t->graph_schedule.next_conflicts)
    return 0;
  return !delaying(t, &t->graph_schedule);
}

/* One Lucky 27 slice within the session's recommended continuation dose and
 * any caller budget: its status (SLICE_NONE when not offered). */
static int graph_labeling_slice(strategy *t) {
  if (!t->set.enabled.graph_labeling_model || !(observe(t, TACTIX_OBSERVE_GRAPH_LABELING) & 1) ||
      stat(t, TACTIX_STAT_LEVEL) ||
      tactix_instance_applicable(t->s, t->lucky, TACTIX_LUCKY_GRAPH_CONTINUE) != 1)
    return SLICE_NONE;
  const uint64_t suggested = observe(t, TACTIX_OBSERVE_GRAPH_LABELING_CONTINUATION);
  tactix_work_settings work;
  tactix_read_work_settings(t->s, &work);
  const int budgeted = work.bounded || work.budget;
  const uint64_t allowance =
      budgeted ? (suggested && suggested < work.budget ? suggested : work.budget) : suggested;
  if (suggested) {
    tactix_work_settings scoped;
    memset(&scoped, 0, sizeof scoped);
    scoped.fields = TACTIX_WORK_BUDGET | TACTIX_WORK_OFFSET | TACTIX_WORK_BOUNDED | TACTIX_WORK_MIDROUND;
    scoped.budget = allowance;
    scoped.bounded = scoped.midround = 1;
    tactix_install_work_settings(t->s, &scoped);
  }
  (void)tactix_apply_instance(t->s, t->lucky, TACTIX_LUCKY_GRAPH_CONTINUE);
  if (suggested) {
    work.fields = TACTIX_WORK_BUDGET | TACTIX_WORK_OFFSET | TACTIX_WORK_BOUNDED | TACTIX_WORK_MIDROUND;
    tactix_install_work_settings(t->s, &work);
  }
  const uint64_t slice = observe(t, TACTIX_OBSERVE_GRAPH_LABELING_SLICE);
  return slice ? (int)slice : SLICE_NONE;
}

/* run_graph_labeling: LymphoSAT's matched solver runs its trajectory to
 * completion, slice by slice: SAT, or 0. */
int ds_run_graph_labeling(strategy *t) {
  int result = SLICE_NONE, sliced = 0;
  for (;;) {
    const int slice = graph_labeling_slice(t);
    if (sliced && slice == SLICE_NONE) break;
    result = slice;
    sliced = 1;
    if (result != SLICE_PENDING) break;
    if (t->stop && *t->stop) {
      result = SLICE_INTERRUPTED;
      break;
    }
  }
  if (sliced) {
    /* A run without a model exhausted its owner dose; an interrupted run
     * defers the verdict. */
    if (result != SLICE_INTERRUPTED) judge_run(&t->graph_schedule, result == SLICE_SAT);
    schedule_occurrence(t, &t->graph_schedule);
    t->graph_schedule.last_ticks = *t->search_ticks;
  }
  return result == SLICE_SAT ? SAT : 0;
}

/* Persistent affine search. */

static tactix_affine_search_state affine_state(strategy *t) {
  tactix_affine_search_state state;
  tactix_read_affine_search(t->s, &state);
  t->affine.live = state.enabled;
  return state;
}

void ds_reset_affine_search(strategy *t) {
  memset(&t->affine, 0, sizeof t->affine);
  t->affine.refresh_ticks = *t->search_ticks;
  memset(&t->affine_schedule, 0, sizeof t->affine_schedule);
  support(t, TACTIX_SUPPORT_AFFINE_CENSUS, 0);
}

/* default_affine_search_parameters: a persistent matrix over the
 * authenticated subimage within the retained-bytes budget; 0 when the
 * formula or the configuration admits none. */
static int affine_search_parameters(strategy *t, tactix_online_parameters *basis,
                                    uint64_t *retained_bytes) {
  const uint64_t variables = observe(t, TACTIX_OBSERVE_VARIABLES);
  if (variables < 2 || observe(t, TACTIX_OBSERVE_EXTERNAL_VARIABLES) >= (uint64_t)INT32_MAX) return 0;
#define SEARCH(key) ds_integer_setting(t->s, "affine_search." #key)
#define CLOSURE(key) ds_integer_setting(t->s, "affine_root_closure." #key)
  const uint64_t bytes = nonzero_or(SEARCH(maximum_retained_bytes), CLOSURE(maximum_journal_bytes));
  const uint64_t initialization =
      nonzero_or(SEARCH(maximum_initialization_work), t->set.inprocess.preprocess_initial_ticks);
  const uint64_t service = nonzero_or(SEARCH(maximum_service_work), t->set.inprocess.minimum_effort_ticks);
  /* Count caps follow the retained storage each source costs. */
  const uint64_t per_source = observe(t, TACTIX_OBSERVE_AFFINE_SOURCE_BYTES);
  const uint64_t clauses = min_u64(bytes / per_source, UINT32_MAX);
  const uint64_t rows = min_u64(SEARCH(maximum_rows), clauses);
  const uint64_t width = min_u64(SEARCH(maximum_row_width), variables);
  if (!clauses || !rows || width < 2 || !initialization || !service || !CLOSURE(maximum_proof_deletions))
    return 0;
  memset(basis, 0, sizeof *basis);
  basis->kind = TACTIX_ONLINE_PARAMETERS_AFFINE_CONSEQUENCE;
  tactix_online_affine_consequence_parameters *b = &basis->value.affine_consequence;
  b->maximum_clauses = b->maximum_groups = b->maximum_patterns = b->maximum_session_keys = clauses;
  b->maximum_snapshot_literals = bytes / sizeof(int32_t);
  b->maximum_snapshot_work = b->maximum_elimination_work = initialization;
  b->maximum_arity = (uint32_t)min_u64(SEARCH(maximum_arity), width);
  b->maximum_intermediate_width = b->basis_maximum_intermediate_width = (uint32_t)width;
  b->maximum_sources = b->basis_maximum_sources = b->basis_maximum_nodes = (uint32_t)rows;
  b->maximum_output_width = 4; /* the V2 typed compatibility field */
  b->maximum_proof_additions = CLOSURE(maximum_proof_additions);
  b->maximum_proof_deletions = CLOSURE(maximum_proof_deletions);
  b->maximum_journal_bytes = CLOSURE(maximum_journal_bytes);
  b->semantics = TACTIX_ONLINE_AFFINE_CONSEQUENCE_CERTIFIED_BASIS_V2;
  b->basis_maximum_work = service;
  b->basis_maximum_premises = (uint32_t)clauses;
  b->basis_maximum_direct_width = b->maximum_arity;
  /* Proof-only helpers: only the formats' signed 32-bit literals bound them. */
  b->maximum_external_extent = INT32_MAX;
  /* A lifetime allowance from the retained-state budget, at least one
   * complete service quantum. */
  b->maximum_session_work = max_u64(service, bytes);
#undef SEARCH
#undef CLOSURE
  *retained_bytes = bytes;
  return 1;
}

/* record_affine_service_result: CryptoMiniSat's usefulness sampling over
 * charged non-cached reductions; a finite optional-service decline releases
 * the session. */
static void record_affine_service(strategy *t, const tactix_affine_service_state *service) {
  const tactix_affine_search_state state = affine_state(t);
  if (t->affine.paused && !stat(t, TACTIX_STAT_LEVEL) && service->status == TACTIX_AFFINE_SERVICE_APPLIED) {
    t->affine.calls = t->affine.yield = t->affine.checks = 0;
    t->affine.paused = 0;
  }
  if (!t->affine.paused && service->synchronized && service->synchronization_work && !service->cached_event) {
    ++t->affine.calls;
    if (service->status == TACTIX_AFFINE_SERVICE_APPLIED) ++t->affine.yield;
    if (++t->affine.checks >= ds_integer_setting(t->s, "affine_search.autodisable_check_every")) {
      t->affine.checks = 0;
      const long double required =
          floorl((long double)t->affine.calls * ds_real_setting(t->s, "affine_search.minimum_usefulness"));
      if (ds_integer_setting(t->s, "affine_search.autodisable") &&
          t->affine.calls > ds_integer_setting(t->s, "affine_search.autodisable_minimum_calls") &&
          (long double)t->affine.yield < required)
        t->affine.paused = 1; /* the bounded owner keeps its leases */
    }
  }
  if (service->terminal_failure && service->status != TACTIX_AFFINE_SERVICE_INTERRUPTED &&
      state.enabled && !state.lazy_reasons) {
    support(t, TACTIX_SUPPORT_RELEASE_AFFINE_MATRIX, 0);
    t->affine.live = 0;
  }
}

/* One AffineMatrixInitialize operation under the session's or the derived
 * profile, the installed basis restored afterwards. */
static int affine_profile_operation(strategy *t, const tactix_online_parameters *basis,
                                    uint32_t admission, uint32_t operation, uint64_t retained,
                                    uint64_t deletions, uint64_t literal_work) {
  if (observe(t, TACTIX_OBSERVE_AFFINE_SESSION) & 16)
    return ds_unported(t, "affine operation over a caller's matrix profile");
  const int matrix = ds_rule("AffineMatrixInitialize");
  const tactix_online_parameters held = ds_read_profile(
      t, matrix, TACTIX_ONLINE_PARAMETERS_AFFINE_CONSEQUENCE, TACTIX_PARAMETERS_INSTALLED);
  tactix_online_affine_matrix_initialize_parameters profile;
  memset(&profile, 0, sizeof profile);
  profile.basis = basis->value.affine_consequence;
  profile.admission = admission;
  profile.operation = operation;
  profile.maximum_retained_bytes = retained;
  profile.maximum_deferred_deletions = deletions;
  profile.maximum_deferred_literal_work = literal_work;
  tactix_affine_matrix_result ignored;
  return ds_affine_operation(t, profile, &held, &ignored);
}

/* initialize_affine_search: startup recognition (refresh 0), or at a restart
 * root additional recognition (recognize) or retained root maintenance. */
static tactix_affine_search_state initialize_affine_search(strategy *t, int refresh, int recognize) {
  tactix_affine_search_state none;
  memset(&none, 0, sizeof none);
  if (observe(t, TACTIX_OBSERVE_AFFINE_REASONS_UNAFFORDABLE) ||
      tactix_rule_applicable(t->s, ds_rule("AffineMatrixInitialize")) != 1 ||
      !observe(t, TACTIX_OBSERVE_PROOF) || observe(t, TACTIX_OBSERVE_MODEL_EXTENDED))
    return none;
  const tactix_affine_search_state before = affine_state(t);
  /* Root maintenance has no owner to maintain after an empty admission. */
  if (refresh && !recognize && (!before.enabled || !before.persistent)) return none;
  /* A completed empty census answers for this exact source state. */
  if (refresh && recognize && observe(t, TACTIX_OBSERVE_AFFINE_CENSUS_UNCHANGED)) {
    none.unchanged = 1;
    return none;
  }
  const int retained = refresh && !recognize && before.enabled && before.persistent;
  tactix_online_parameters basis;
  uint64_t bytes = 0, deletions, literal_work;
  if (retained) {
    memset(&basis, 0, sizeof basis);
    basis.kind = TACTIX_ONLINE_PARAMETERS_AFFINE_CONSEQUENCE;
    basis.value.affine_consequence = before.basis;
    bytes = before.maximum_retained_bytes;
    deletions = before.maximum_deferred_deletions;
    literal_work = before.maximum_deferred_literal_work;
  } else {
    if (!affine_search_parameters(t, &basis, &bytes)) return none;
    deletions = basis.value.affine_consequence.maximum_proof_deletions;
    literal_work = basis.value.affine_consequence.maximum_session_work;
  }
  const int matrix = ds_rule("AffineMatrixInitialize");
  if (tactix_install_parameters(t->s, matrix, &basis)) return none; /* an invalid profile */
  affine_profile_operation(t, &basis, TACTIX_AFFINE_MATRIX_AUTHENTICATED_SUBIMAGE,
                           !refresh   ? TACTIX_AFFINE_MATRIX_INITIALIZE
                           : recognize ? TACTIX_AFFINE_MATRIX_REFRESH
                                       : TACTIX_AFFINE_MATRIX_ROOT_MAINTENANCE,
                           bytes, deletions, literal_work);
  if (t->unported) return none;
  const tactix_affine_search_state after = affine_state(t);
  t->affine.initialization_work =
      saturated_sum(t->affine.initialization_work,
                    saturated_sum(after.recognition_work, after.maintenance_work));
  if (after.has_root_service) {
    t->affine.service_work = saturated_sum(t->affine.service_work, after.maintenance_work);
    record_affine_service(t, &after.root_service);
  }
  const int applied = after.initialization_status == TACTIX_AFFINE_INITIALIZATION_APPLIED ||
                      after.initialization_status == TACTIX_AFFINE_INITIALIZATION_APPLIED_PARTIAL;
  if (!refresh || recognize) {
    /* Only a genuinely empty successful census is remembered. */
    const tactix_affine_search_state session = affine_state(t);
    if (after.initialization_status == TACTIX_AFFINE_INITIALIZATION_NO_STRUCTURE && !session.enabled)
      support(t, TACTIX_SUPPORT_AFFINE_CENSUS, 1);
    else if (applied)
      support(t, TACTIX_SUPPORT_AFFINE_CENSUS, 0);
    t->affine.refresh_ticks = *t->search_ticks;
    t->affine.refresh_work = after.recognition_work;
  }
  /* A newly authenticated system or a new certified root fact starts a new
   * usefulness sample; maintenance completion is no such evidence. */
  const tactix_affine_search_state session = affine_state(t);
  const int admitted_change = applied && !after.unchanged && (!refresh || recognize) &&
                              session.enabled && session.owner_version != before.owner_version;
  if (admitted_change || after.root_unit_additions) {
    t->affine.calls = t->affine.yield = t->affine.checks = 0;
    t->affine.paused = 0;
  }
  return after;
}

/* service_affine_search: one Gaussian step at a restart root (the eager
 * source-support step) or a propagated search fixpoint (the configured reason
 * mode); the service's state, status 0 when not offered. */
static tactix_affine_service_state service_affine_search(strategy *t, int root) {
  tactix_affine_service_state none;
  memset(&none, 0, sizeof none);
  const tactix_affine_search_state before = affine_state(t);
  if (!t->set.enabled.affine_search || !before.enabled || before.deferred_reason) return none;
  static const uint32_t modes[] = {
      TACTIX_AFFINE_MATRIX_SEARCH_STEP, TACTIX_AFFINE_MATRIX_DEFERRED_SEARCH_STEP,
      TACTIX_AFFINE_MATRIX_DETACHED_MINIMAL_SEARCH_STEP, TACTIX_AFFINE_MATRIX_DETACHED_SOURCE_SEARCH_STEP,
      TACTIX_AFFINE_MATRIX_LAZY_SOURCE_SEARCH_STEP, TACTIX_AFFINE_MATRIX_LAZY_MINIMAL_SEARCH_STEP};
  const uint64_t mode = ds_integer_setting(t->s, "affine_search.reason_mode");
  const uint32_t operation = root || mode >= sizeof modes / sizeof modes[0] ? TACTIX_AFFINE_MATRIX_SEARCH_STEP
                                                                            : modes[mode];
  if (operation == TACTIX_AFFINE_MATRIX_DETACHED_MINIMAL_SEARCH_STEP ||
      operation == TACTIX_AFFINE_MATRIX_DETACHED_SOURCE_SEARCH_STEP)
    return ds_unported(t, "detached affine reasons"), none;
  tactix_online_parameters basis;
  memset(&basis, 0, sizeof basis);
  basis.kind = TACTIX_ONLINE_PARAMETERS_AFFINE_CONSEQUENCE;
  basis.value.affine_consequence = before.basis;
  affine_profile_operation(t,
                           &basis,
                           operation == TACTIX_AFFINE_MATRIX_SEARCH_STEP && !before.persistent
                               ? TACTIX_AFFINE_MATRIX_COMPLETE_SNAPSHOT
                               : TACTIX_AFFINE_MATRIX_AUTHENTICATED_SUBIMAGE,
                           operation, before.maximum_retained_bytes, before.maximum_deferred_deletions,
                           before.maximum_deferred_literal_work);
  if (t->unported) return none;
  const tactix_affine_search_state after = affine_state(t);
  const tactix_affine_service_state *service = &after.service;
  t->affine.service_work = saturated_sum(t->affine.service_work, service->work);
  record_affine_service(t, service);
  return *service;
}

/* SET_EFFORT_LIMIT for a root-service run: the per-mille share of the search
 * ticks since its last run, floored at mineffort (the preprocessing reference
 * before the first conflict). */
static uint64_t root_service_effort(strategy *t, const root_service_schedule *s, uint64_t permille) {
  uint64_t reference;
  if (!*t->conflicts) {
    reference = t->set.inprocess.preprocess_initial_ticks;
  } else {
    const uint64_t now = *t->search_ticks;
    reference = max_u64(now >= s->last_ticks ? now - s->last_ticks : 0, t->set.inprocess.minimum_effort_ticks);
  }
  return scale_permille(reference, permille);
}

/* affine_additional_recognition_due: elapsed search work earns another
 * recognition only while no persistent owner is retained. */
static int additional_recognition_due(strategy *t) {
  const tactix_affine_search_state state = affine_state(t);
  if (state.enabled && state.persistent) return 0;
  const uint64_t quantum = nonzero_or(ds_integer_setting(t->s, "affine_search.maximum_initialization_work"),
                                     t->set.inprocess.preprocess_initial_ticks);
  const uint64_t ticks = *t->search_ticks;
  const uint64_t needed = max_u64(quantum, t->affine.refresh_work);
  return ticks >= t->affine.refresh_ticks && ticks - t->affine.refresh_ticks >= needed;
}

/* run_affine_search: one run at the startup root or a natural full-restart
 * root: retained maintenance (restart only), then atomic Gaussian steps,
 * each published consequence followed by root BCP, until a fixpoint, an
 * unfinished slice or the run's effort limit (checked between steps). */
static int run_affine_search(strategy *t, int startup) {
  const uint64_t effort = root_service_effort(t, &t->affine_schedule, ds_integer_setting(t->s, "inprocess.sweep_permille"));
  uint64_t spent = 0, published = 0;
  int more = 1;
  if (!startup) {
    const int recognize = additional_recognition_due(t);
    const uint64_t before = t->affine.initialization_work;
    const tactix_affine_search_state refreshed = initialize_affine_search(t, 1, recognize);
    if (t->unported) return 0;
    spent = saturated_sum(spent, t->affine.initialization_work - before);
    const int output = refreshed.root_unit_additions || refreshed.root_reason_additions;
    published += (uint64_t)output;
    if (output && ds_propagate(t) != OK) return UNSAT;
    if (refreshed.has_root_service)
      more = refreshed.root_service.status == TACTIX_AFFINE_SERVICE_APPLIED ||
             refreshed.root_service.status == TACTIX_AFFINE_SERVICE_PENDING;
  }
  while (more && affine_state(t).enabled && spent < effort) {
    const uint64_t before = t->affine.service_work;
    const tactix_affine_service_state service = service_affine_search(t, 1);
    if (t->unported) return 0;
    spent = saturated_sum(spent, t->affine.service_work - before);
    if (service.result == TACTIX_RULE_RESULT_UNSAT) return UNSAT;
    if (service.status == TACTIX_AFFINE_SERVICE_APPLIED) {
      if (!service.bcp_pending) abort();
      ++published;
      if (ds_propagate(t) != OK) return UNSAT;
      continue;
    }
    more = service.status == TACTIX_AFFINE_SERVICE_PENDING;
  }
  /* Fruitful = the run published output; a cut run defers the verdict. */
  if (published || !more) judge_run(&t->affine_schedule, published != 0);
  schedule_occurrence(t, &t->affine_schedule);
  t->affine_schedule.last_ticks = *t->search_ticks;
  /* A run that reached its fixpoint has consumed its own support facts. */
  if (!more && affine_state(t).enabled) tactix_affine_inputs_consume(t->s, &t->affine.inputs);
  return OK;
}

/* Startup: CryptoMiniSat's XOR detection after elimination, then one run. */
int ds_start_affine_search(strategy *t) {
  if (!t->set.enabled.affine_search) return OK;
  initialize_affine_search(t, 0, 0);
  if (t->unported || !affine_state(t).enabled) return OK;
  return run_affine_search(t, 1);
}

/* A propagated search fixpoint above the root: one service step; 1 when it
 * published a consequence (ordinary BCP consumes it next). */
int ds_affine_search_step(strategy *t) {
  if (!t->set.enabled.affine_search || !t->affine.live || t->affine.paused ||
      !stat(t, TACTIX_STAT_LEVEL))
    return 0;
  if (!(observe(t, TACTIX_OBSERVE_AFFINE_SESSION) & 2)) {
    t->affine.live = 0;
    return 0;
  }
  return service_affine_search(t, 0).status == TACTIX_AFFINE_SERVICE_APPLIED;
}

/* affine_restart_service: a natural full-restart root window, due at the
 * Eliminate-shaped limit, which at expiry waits until the retained inputs
 * changed (Kissat eliminate.c). */
int ds_affine_restart_service(strategy *t) {
  if (stat(t, TACTIX_STAT_LEVEL) || !affine_state(t).enabled ||
      *t->conflicts < t->affine_schedule.next_conflicts)
    return NOOP;
  if (tactix_affine_inputs_changed(t->s, &t->affine.inputs) != 1) return NOOP;
  if (delaying(t, &t->affine_schedule)) return NOOP;
  /* The cursor keeps the run's own support facts visible unless it reaches
   * a fixpoint. */
  tactix_affine_inputs_consume(t->s, &t->affine.inputs);
  if (run_affine_search(t, 0) == UNSAT) return UNSAT;
  if (affine_state(t).enabled) {
    const uint64_t consumed = t->affine.inputs.root_scanned;
    tactix_affine_inputs_consume(t->s, &t->affine.inputs);
    if (t->affine.inputs.root_scanned > consumed) t->affine.inputs.root_scanned = consumed;
  }
  return OK;
}
