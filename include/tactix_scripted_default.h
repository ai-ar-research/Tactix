#ifndef TACTIX_SCRIPTED_DEFAULT_H
#define TACTIX_SCRIPTED_DEFAULT_H
/* Shared search-only scheduler from demo3, not the full native DefaultPolicy.
 * C clients and persistent sessions use the same public-rule implementation. */
#include "tactix.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static inline int tactix_scripted_rule_id(const char *name) {
  for (int i = 0; i < tactix_rule_count(); i++)
    if (!strcmp(tactix_rule_name(i), name))
      return i;
  return -1;
}

/* scheduling constants — mirror of default_policy.hpp (v1 freeze review:
 * these are POLICY state, deliberately not solver API) */
#define TACTIX_SCRIPTED_RESTART_MARGIN 1.10
#define TACTIX_SCRIPTED_RESTART_INTERVAL 1
#define TACTIX_SCRIPTED_EMA_FAST_WINDOW 33.0
#define TACTIX_SCRIPTED_EMA_SLOW_WINDOW 1e5
#define TACTIX_SCRIPTED_REDUCE_INTERVAL_INIT 2000
#define TACTIX_SCRIPTED_REDUCE_INTERVAL_STEP 300
#define TACTIX_SCRIPTED_MODE_INITIAL_CONFLICTS 1000
#define TACTIX_SCRIPTED_MODE_INTERVAL 1000
#define TACTIX_SCRIPTED_RELUCTANT_PERIOD 1024
#define TACTIX_SCRIPTED_RELUCTANT_LIMIT (1 << 20)
#define TACTIX_SCRIPTED_REPHASE_INTERVAL_INIT 1000
#define TACTIX_SCRIPTED_REPHASE_INTERVAL 1000
#define TACTIX_SCRIPTED_CHRONO_LEVELS 100
#define TACTIX_SCRIPTED_SUBSUME_INTERVAL_INIT 500
#define TACTIX_SCRIPTED_SUBSUME_INTERVAL 500
#define TACTIX_SCRIPTED_INPROCESS_PHASE_BUDGET 300
#define TACTIX_SCRIPTED_INPROCESS_PHASE_ROUNDS 3
/* This demo mirrors DefaultPolicy's SEARCH trajectory. It deliberately does
 * NOT replicate the inprocessing slot (DefaultPolicy runs the full S4
 * arrangement — probing family + simplifiers + dosed BVE/Factor), so its
 * stat contract is within-noise on inprocessing-heavy instances (§8.4), not
 * exact. That scope is intentional and permanent: the demo exists to prove
 * the N1 stepping surface drives kissat-shaped SEARCH, and the arrangement's
 * slot is exercised end-to-end by the registry demos + fuzz instead. */

/* bias-corrected EMA (mirror of DefaultPolicy::Smooth / kissat smooth.c) */
struct tactix_scripted_smooth {
  double value, biased, alpha, beta, exp;
};

static inline void tactix_scripted_smooth_init(struct tactix_scripted_smooth *s, double window) {
  s->alpha = 1.0 / window;
  s->beta = 1.0 - s->alpha;
  s->value = s->biased = 0;
  s->exp = 1.0;
}

static inline void tactix_scripted_smooth_update(struct tactix_scripted_smooth *s, double y) {
  s->biased += s->alpha * (y - s->biased);
  if (s->exp) {
    const double old_exp = s->exp;
    s->exp *= s->beta;
    if (s->exp == old_exp) {
      s->exp = 0;
      s->value = s->biased;
    } else {
      s->value = s->biased / (1.0 - s->exp);
    }
  } else {
    s->value = s->biased;
  }
}

struct tactix_scripted_averages {
  int initialized;
  struct tactix_scripted_smooth fast_glue, slow_glue;
};

typedef struct tactix_scripted_default {
  struct tactix_scripted_averages avg[2]; /* per-mode, persist across switches */
  uint64_t restart_next;
  uint64_t reduce_next;
  uint64_t mode_count, mode_conflicts_limit, mode_ticks_limit,
      mode_ticks_at_focus_entry;
  uint64_t rel_u, rel_v, rel_wait;
  int rel_trigger;
  uint64_t rephase_next;
  uint64_t inprocess_next;
  int inprocess_ran; /* the initial full pass is never gated */
  uint64_t inprocess_rounds; /* interval growth key (policy-local) */
  uint64_t inprocess_binaries_seen, inprocess_root_fixed_seen;
  int lucky_tried;
} tactix_scripted_default;

static inline void tactix_scripted_default_begin(tactix_scripted_default *p) {
  memset(p, 0, sizeof *p);
  p->restart_next = TACTIX_SCRIPTED_RESTART_INTERVAL;
  p->reduce_next = TACTIX_SCRIPTED_REDUCE_INTERVAL_INIT;
  p->mode_conflicts_limit = TACTIX_SCRIPTED_MODE_INITIAL_CONFLICTS;
  p->rel_u = p->rel_v = 1;
  p->rel_wait = TACTIX_SCRIPTED_RELUCTANT_PERIOD;
  p->rephase_next = TACTIX_SCRIPTED_REPHASE_INTERVAL_INIT;
  p->inprocess_next = UINT64_MAX; /* Search-only example, unchanged. */
}

static inline void tactix_scripted_observe_glue(tactix_scripted_default *p, int stable, double glue) {
  struct tactix_scripted_averages *avg = &p->avg[stable];
  if (!avg->initialized) {
    tactix_scripted_smooth_init(&avg->fast_glue, TACTIX_SCRIPTED_EMA_FAST_WINDOW);
    tactix_scripted_smooth_init(&avg->slow_glue, TACTIX_SCRIPTED_EMA_SLOW_WINDOW);
    avg->initialized = 1;
  }
  tactix_scripted_smooth_update(&avg->fast_glue, glue);
  tactix_scripted_smooth_update(&avg->slow_glue, glue);
}

static inline void tactix_scripted_tick_reluctant(tactix_scripted_default *p) {
  if (p->rel_trigger)
    return;
  if (--p->rel_wait)
    return;
  uint64_t u = p->rel_u, v = p->rel_v;
  if ((u & -u) == v) {
    u++;
    v = 1;
  } else {
    v *= 2;
  }
  uint64_t wait = v * TACTIX_SCRIPTED_RELUCTANT_PERIOD;
  if (wait > TACTIX_SCRIPTED_RELUCTANT_LIMIT) {
    u = v = 1;
    wait = TACTIX_SCRIPTED_RELUCTANT_PERIOD;
  }
  p->rel_trigger = 1;
  p->rel_wait = wait;
  p->rel_u = u;
  p->rel_v = v;
}

static inline void tactix_scripted_update_restart_limit(tactix_scripted_default *p, tactix *s) {
  const uint64_t restarts = tactix_stat(s, TACTIX_STAT_RESTARTS);
  uint64_t delta = TACTIX_SCRIPTED_RESTART_INTERVAL;
  if (restarts)
    delta += (uint64_t)(log10((double)restarts + 9) - 1.0);
  p->restart_next = tactix_stat(s, TACTIX_STAT_CONFLICTS) + delta;
}

static inline void tactix_scripted_update_mode_limits(tactix_scripted_default *p, tactix *s) {
  const uint64_t ticks = tactix_stat(s, TACTIX_STAT_TICKS);
  if (p->mode_count & 1) { /* just entered stable */
    p->mode_ticks_limit = ticks + (ticks - p->mode_ticks_at_focus_entry);
  } else { /* just entered focused */
    const uint64_t n = (p->mode_count / 2) + 1;
    const double log4 = pow(log10((double)n + 9), 4);
    p->mode_conflicts_limit =
        tactix_stat(s, TACTIX_STAT_CONFLICTS) +
        (uint64_t)(TACTIX_SCRIPTED_MODE_INTERVAL * (double)n * log4);
    p->mode_ticks_at_focus_entry = ticks;
    /* only the focused entry refreshes the restart limit; per-mode
     * averages persist across switches */
    tactix_scripted_update_restart_limit(p, s);
  }
}

/* The hook settles query assumptions/backtracking and returns -1 to permit
 * the next action, 0 to suspend or a terminal 10/20. State contains no borrowed
 * view and survives same-query restoration. NULL preserves the plain schedule. */
static inline int tactix_scripted_default_run(tactix_scripted_default *p,
    tactix *s, int (*hook)(void *), void *context) {
  const int propagate = tactix_scripted_rule_id("Propagate");
  const int conflictanalysis = tactix_scripted_rule_id("ConflictAnalysis");
  const int decide = tactix_scripted_rule_id("Decide");
  const int reduce = tactix_scripted_rule_id("Reduce");
  const int restart = tactix_scripted_rule_id("Restart");
  const int switchmode = tactix_scripted_rule_id("SwitchMode");
  const int rephase = tactix_scripted_rule_id("Rephase");
  const int lucky = tactix_scripted_rule_id("Lucky");
  const int strengthen = tactix_scripted_rule_id("Strengthen");
  const int bve = tactix_scripted_rule_id("BVE");
  const int subsume = tactix_scripted_rule_id("SubsumeForward");
  if (tactix_status(s) == 20)
    return 20;
  for (;;) {
    const int boundary = hook ? hook(context) : -1;
    if (boundary >= 0) return boundary;
    int r = tactix_apply(s, propagate);
    if (r == 20)
      return 20;
    /* S5 Lucky once at search start, mirroring DefaultPolicy */
    if (!p->lucky_tried && r != 2) {
      p->lucky_tried = 1;
      const int lucky_result =
          tactix_apply_instance(s, lucky, TACTIX_INSTANCE_CANONICAL);
      if (lucky_result == 10 || lucky_result == 20)
        return lucky_result;
    }
    if (r == 2) { /* conflict: one atomic ConflictAnalysis (learn+backjump) */
      r = tactix_apply(s, conflictanalysis);
      if (r == 20)
        return 20;
      /* observe glue + tick AFTER the fused step: backjump leaves
       * LAST_LEARNED_GLUE (pending.glue) intact and neither read affects the
       * backjump, so this stays stat-for-stat with DefaultPolicy (whose
       * internal micro-loop observes between analyze and backjump). */
      {
        const int st = tactix_stat(s, TACTIX_STAT_STABLE) != 0;
        tactix_scripted_observe_glue(p, st,
                     (double)tactix_stat(s, TACTIX_STAT_LAST_LEARNED_GLUE));
        if (st)
          tactix_scripted_tick_reluctant(p);
      }
      continue;
    }
    if (hook && r == 0) continue; /* Settle the rest of A before branching. */
    if (tactix_stat(s, TACTIX_STAT_UNASSIGNED) == 0)
      return 10;
    const uint64_t conflicts = tactix_stat(s, TACTIX_STAT_CONFLICTS);
    if (conflicts >= p->reduce_next) {
      if (tactix_apply(s, reduce) != -1) { /* canonical volume */
        p->reduce_next = tactix_stat(s, TACTIX_STAT_CONFLICTS) +
                        TACTIX_SCRIPTED_REDUCE_INTERVAL_INIT +
                        TACTIX_SCRIPTED_REDUCE_INTERVAL_STEP *
                            tactix_stat(s, TACTIX_STAT_REDUCTIONS);
        continue;
      }
    }
    const int stable = tactix_stat(s, TACTIX_STAT_STABLE) != 0;
    const int want_switch =
        (p->mode_count & 1)
            ? tactix_stat(s, TACTIX_STAT_TICKS) >= p->mode_ticks_limit
            : conflicts >= p->mode_conflicts_limit;
    if (want_switch && tactix_apply(s, switchmode) != -1) {
      p->mode_count++;
      if (!stable) { /* just entered stable: fresh reluctant sequence */
        p->rel_u = p->rel_v = 1;
        p->rel_wait = TACTIX_SCRIPTED_RELUCTANT_PERIOD;
        p->rel_trigger = 0;
      }
      tactix_scripted_update_mode_limits(p, s);
      continue;
    }
    const int want_restart =
        conflicts < p->restart_next
            ? 0
            : (stable ? p->rel_trigger
                      : TACTIX_SCRIPTED_RESTART_MARGIN * p->avg[0].slow_glue.value <=
                            p->avg[0].fast_glue.value);
    if (want_restart && tactix_apply(s, restart) != -1) {
      p->rel_trigger = 0;
      if (!stable) /* focused-only limit update (kissat restart.c) */
        tactix_scripted_update_restart_limit(p, s);
      continue;
    }
    if (stable && conflicts > p->rephase_next) {
      /* the composed root restart (payload 0 = keep nothing); -1 when
       * already at the root */
      tactix_apply_instance(s, restart, 0);
      /* B,B,I,B,B,O rotation over the instance payloads (kissat's
       * B,W,I,B,W,O with Best in the Walking slots until phase 4) */
      static const uint64_t rotation[6] = {0, 0, 1, 0, 0, 2};
      const uint64_t n_before = tactix_stat(s, TACTIX_STAT_REPHASED);
      if (tactix_apply_instance(s, rephase, rotation[n_before % 6]) != -1) {
        const uint64_t n = tactix_stat(s, TACTIX_STAT_REPHASED);
        const double lg = log10((double)n + 9);
        p->rephase_next = tactix_stat(s, TACTIX_STAT_CONFLICTS) +
                         (uint64_t)(TACTIX_SCRIPTED_REPHASE_INTERVAL * (double)n * lg * lg *
                                    lg);
        continue;
      }
    }
    /* C1 forward subsumption in the eliminate slot (after rephase,
     * before decide, like DefaultPolicy): new-material gate first (new
     * binaries / new root-fixed vars, kissat_eliminating's variables
     * trigger), then the composed root restart and the canonical round;
     * applies may be -1 (already at root / guard) */
    if (conflicts >= p->inprocess_next &&
        (!p->inprocess_ran ||
         tactix_stat(s, TACTIX_STAT_BINARY_CLAUSES) >
             p->inprocess_binaries_seen ||
         tactix_stat(s, TACTIX_STAT_ROOT_FIXED) >
             p->inprocess_root_fixed_seen)) {
      /* the simplification PHASE (kissat eliminate-round shape): descend to a
       * quiescent root, then a few outer rounds — BVE per-variable to its
       * global budget, the batch rules (Strengthen, SubsumeForward) once each.
       * Driven entirely through the public API (N1). tactix_apply: 0 progress,
       * 1 no-op, 20 UNSAT, -1 guard-off. NOTE: DefaultPolicy's BVE reads an
       * internal dense occurrence index while this API path uses the arena-scan
       * fallback, so the two pick equivalent-but-different eliminations — the
       * mirror reproduces the SCHEDULE, not a bit-identical trajectory. */
      const int batch[2] = {strengthen, subsume};
      if (tactix_stat(s, TACTIX_STAT_LEVEL) > 0)
        tactix_apply_instance(s, restart, 0);
      uint64_t bve_applied = 0;
      for (int outer = 0; outer < TACTIX_SCRIPTED_INPROCESS_PHASE_ROUNDS; outer++) {
        int round_changed = 0;
        while (bve_applied < TACTIX_SCRIPTED_INPROCESS_PHASE_BUDGET) {
          const int ar = tactix_apply(s, bve);
          if (ar == 20)
            return 20;
          if (ar != 0)
            break; /* no-op / guard-off = BVE fixpoint this round */
          bve_applied++;
          round_changed = 1;
          if (tactix_apply(s, propagate) == 20)
            return 20;
        }
        for (int bi = 0; bi < 2; bi++) {
          const int ar = tactix_apply(s, batch[bi]);
          if (ar == 20)
            return 20;
          if (ar == 0) {
            round_changed = 1;
            if (tactix_apply(s, propagate) == 20)
              return 20;
          }
        }
        if (!round_changed)
          break;
      }
      p->inprocess_ran = 1;
      p->inprocess_rounds++;
      p->inprocess_binaries_seen = tactix_stat(s, TACTIX_STAT_BINARY_CLAUSES);
      p->inprocess_root_fixed_seen = tactix_stat(s, TACTIX_STAT_ROOT_FIXED);
      p->inprocess_next = UINT64_MAX; /* preprocessing-only */
      continue;
    }
    r = tactix_apply(s, decide); /* canonical branch */
    assert(r != -1);
  }
}

#ifdef __cplusplus
}
#endif
#endif
