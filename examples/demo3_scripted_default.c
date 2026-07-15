/* §8.4 demo policy 3 — a scripted policy reproducing the default
 * schedule "within noise" (the plan's Python variant lands with the
 * phase-4 bindings; the schedule logic is what is under review here).
 *
 * This client re-implements DefaultPolicy::solve outside the library,
 * pull-based (O5), using only the frozen v1 API: canonical rule
 * applications plus tactix_stat for the counters the built-in policy
 * reads (conflicts, ticks, last learned glue, mode — the rev-21 review
 * finding). Because the arithmetic is replicated exactly (same EMA
 * updates, same reluctant doubling, same mode-limit formulas — constants
 * from default_policy.hpp), the run is TRAJECTORY-IDENTICAL, so the
 * "within noise" acceptance is checked as exact equality of every
 * counter against a fresh tactix_solve run. If DefaultPolicy's schedule
 * changes, update this mirror deliberately. */
#undef NDEBUG
#include "tactix.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int rule_id(const char *name) {
  for (int i = 0; i < tactix_rule_count(); i++)
    if (!strcmp(tactix_rule_name(i), name))
      return i;
  return -1;
}

/* scheduling constants — mirror of default_policy.hpp (v1 freeze review:
 * these are POLICY state, deliberately not solver API) */
#define RESTART_MARGIN 1.10
#define RESTART_INTERVAL 1
#define EMA_FAST_WINDOW 33.0
#define EMA_SLOW_WINDOW 1e5
#define REDUCE_INTERVAL_INIT 2000
#define REDUCE_INTERVAL_STEP 300
#define MODE_INITIAL_CONFLICTS 1000
#define MODE_INTERVAL 1000
#define RELUCTANT_PERIOD 1024
#define RELUCTANT_LIMIT (1 << 20)
#define REPHASE_INTERVAL_INIT 1000
#define REPHASE_INTERVAL 1000
#define CHRONO_LEVELS 100
#define BLOCK_R 1.4
#define BLOCK_TRAIL_QUEUE 5000
#define BLOCK_LOWER_BOUND 10000
#define SUBSUME_INTERVAL_INIT 500
#define SUBSUME_INTERVAL 500
/* mirror of DefaultPolicy::SUBSUME_IN_DEFAULT (deferred until V1/E1) */
#define SUBSUME_IN_DEFAULT 0

/* bias-corrected EMA (mirror of DefaultPolicy::Smooth / kissat smooth.c) */
struct smooth {
  double value, biased, alpha, beta, exp;
};

static void smooth_init(struct smooth *s, double window) {
  s->alpha = 1.0 / window;
  s->beta = 1.0 - s->alpha;
  s->value = s->biased = 0;
  s->exp = 1.0;
}

static void smooth_update(struct smooth *s, double y) {
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

struct averages {
  int initialized;
  struct smooth fast_glue, slow_glue;
};

struct sched {
  struct averages avg[2]; /* per-mode, persist across switches */
  uint64_t restart_next;
  uint64_t reduce_next;
  uint64_t mode_count, mode_conflicts_limit, mode_ticks_limit,
      mode_ticks_at_focus_entry;
  uint64_t rel_u, rel_v, rel_wait;
  int rel_trigger;
  uint64_t rephase_next;
  uint64_t subsume_next;
  int subsume_ran; /* the initial full round is never gated */
  uint64_t subsume_binaries_seen, subsume_root_fixed_seen;
  /* blocking restarts (glucose trailQueue mirror): trail depth at the
   * observation point is nvars - TACTIX_STAT_UNASSIGNED */
  uint32_t block_ring[BLOCK_TRAIL_QUEUE];
  uint32_t block_pos, block_filled;
  uint64_t block_sum;
};

/* mirror of DefaultPolicy's scale_inprocessing_delta (kissat kimits.c
 * kissat_scale_delta): identical double arithmetic, identical result */
static uint64_t scale_inproc_delta(tactix *s, uint64_t delta) {
  const uint64_t clauses = tactix_stat(s, TACTIX_STAT_IRREDUNDANT_CLAUSES);
  const double f = log10((double)clauses + 1) - 5;
  return (uint64_t)((4.5 * f * f + 25) * (double)delta);
}

static void sched_init(struct sched *p) {
  memset(p, 0, sizeof *p);
  p->restart_next = RESTART_INTERVAL;
  p->reduce_next = REDUCE_INTERVAL_INIT;
  p->mode_conflicts_limit = MODE_INITIAL_CONFLICTS;
  p->rel_u = p->rel_v = 1;
  p->rel_wait = RELUCTANT_PERIOD;
  p->rephase_next = REPHASE_INTERVAL_INIT;
}

static void observe_glue(struct sched *p, int stable, double glue) {
  struct averages *avg = &p->avg[stable];
  if (!avg->initialized) {
    smooth_init(&avg->fast_glue, EMA_FAST_WINDOW);
    smooth_init(&avg->slow_glue, EMA_SLOW_WINDOW);
    avg->initialized = 1;
  }
  smooth_update(&avg->fast_glue, glue);
  smooth_update(&avg->slow_glue, glue);
}

/* mirror of DefaultPolicy::observe_trail (glucose blocking restarts):
 * identical ring/threshold arithmetic, identical fast-EMA reset */
static void observe_trail(struct sched *p, uint32_t depth,
                          uint64_t conflicts) {
  p->block_sum += depth;
  p->block_sum -= p->block_ring[p->block_pos];
  p->block_ring[p->block_pos] = depth;
  p->block_pos = (p->block_pos + 1) % BLOCK_TRAIL_QUEUE;
  if (p->block_filled < BLOCK_TRAIL_QUEUE) {
    p->block_filled++;
    return;
  }
  if (conflicts <= BLOCK_LOWER_BOUND)
    return;
  const double avg_depth = (double)p->block_sum / (double)BLOCK_TRAIL_QUEUE;
  if (depth > BLOCK_R * avg_depth) {
    struct averages *avg = &p->avg[0];
    avg->fast_glue.value = avg->slow_glue.value;
    avg->fast_glue.biased = avg->slow_glue.value * (1.0 - avg->fast_glue.exp);
  }
}

static void tick_reluctant(struct sched *p) {
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
  uint64_t wait = v * RELUCTANT_PERIOD;
  if (wait > RELUCTANT_LIMIT) {
    u = v = 1;
    wait = RELUCTANT_PERIOD;
  }
  p->rel_trigger = 1;
  p->rel_wait = wait;
  p->rel_u = u;
  p->rel_v = v;
}

static void update_restart_limit(struct sched *p, tactix *s) {
  const uint64_t restarts = tactix_stat(s, TACTIX_STAT_RESTARTS);
  uint64_t delta = RESTART_INTERVAL;
  if (restarts)
    delta += (uint64_t)(log10((double)restarts + 9) - 1.0);
  p->restart_next = tactix_stat(s, TACTIX_STAT_CONFLICTS) + delta;
}

static void update_mode_limits(struct sched *p, tactix *s) {
  const uint64_t ticks = tactix_stat(s, TACTIX_STAT_TICKS);
  if (p->mode_count & 1) { /* just entered stable */
    p->mode_ticks_limit = ticks + (ticks - p->mode_ticks_at_focus_entry);
  } else { /* just entered focused */
    const uint64_t n = (p->mode_count / 2) + 1;
    const double log4 = pow(log10((double)n + 9), 4);
    p->mode_conflicts_limit =
        tactix_stat(s, TACTIX_STAT_CONFLICTS) +
        (uint64_t)(MODE_INTERVAL * (double)n * log4);
    p->mode_ticks_at_focus_entry = ticks;
    /* only the focused entry refreshes the restart limit; per-mode
     * averages persist across switches */
    update_restart_limit(p, s);
  }
}

static int scripted_solve(tactix *s, uint64_t nvars) {
  struct sched p;
  sched_init(&p);
  /* the first inprocessing limit is size-scaled at solve entry, like
   * DefaultPolicy's lazy init */
  p.subsume_next = SUBSUME_IN_DEFAULT
                       ? tactix_stat(s, TACTIX_STAT_CONFLICTS) +
                             scale_inproc_delta(s, SUBSUME_INTERVAL_INIT)
                       : UINT64_MAX;
  const int propagate = rule_id("Propagate");
  const int analyze = rule_id("Analyze");
  const int backjump = rule_id("Backjump");
  const int decide = rule_id("Decide");
  const int reduce = rule_id("Reduce");
  const int restart = rule_id("Restart");
  const int switchmode = rule_id("SwitchMode");
  const int rephase = rule_id("Rephase");
  const int lucky = rule_id("Lucky");
  const int subsume = rule_id("SubsumeForward");
  int lucky_tried = 0;
  if (tactix_status(s) == 20)
    return 20;
  for (;;) {
    int r = tactix_apply(s, propagate);
    if (r == 20)
      return 20;
    /* S5 Lucky once at search start, mirroring DefaultPolicy */
    if (!lucky_tried && r != 2) {
      lucky_tried = 1;
      if (tactix_apply_instance(s, lucky, TACTIX_INSTANCE_CANONICAL) == 10)
        return 10;
    }
    if (r == 2) { /* conflict: analyze, observe, backjump */
      r = tactix_apply(s, analyze);
      if (r == 20)
        return 20;
      {
        const int st = tactix_stat(s, TACTIX_STAT_STABLE) != 0;
        observe_glue(&p, st,
                     (double)tactix_stat(s, TACTIX_STAT_LAST_LEARNED_GLUE));
        if (st)
          tick_reluctant(&p);
        else
          observe_trail(&p,
                        (uint32_t)(nvars -
                                   tactix_stat(s, TACTIX_STAT_UNASSIGNED)),
                        tactix_stat(s, TACTIX_STAT_CONFLICTS));
      }
      {
        /* chrono mirror: the jump level is the canonical Backjump
         * instance (first enumerated payload) */
        uint64_t jump;
        const int m = tactix_rule_instances(s, backjump, &jump, 1);
        const uint64_t back = tactix_stat(s, TACTIX_STAT_LEVEL) - 1;
        const uint64_t target =
            (m >= 1 && back - jump > CHRONO_LEVELS) ? back : jump;
        r = tactix_apply_instance(s, backjump, m >= 1 ? target : jump);
      }
      if (r == 20)
        return 20;
      continue;
    }
    if (tactix_stat(s, TACTIX_STAT_UNASSIGNED) == 0)
      return 10;
    const uint64_t conflicts = tactix_stat(s, TACTIX_STAT_CONFLICTS);
    if (conflicts >= p.reduce_next) {
      if (tactix_apply(s, reduce) != -1) { /* canonical volume */
        p.reduce_next = tactix_stat(s, TACTIX_STAT_CONFLICTS) +
                        REDUCE_INTERVAL_INIT +
                        REDUCE_INTERVAL_STEP *
                            tactix_stat(s, TACTIX_STAT_REDUCTIONS);
        continue;
      }
    }
    const int stable = tactix_stat(s, TACTIX_STAT_STABLE) != 0;
    const int want_switch =
        (p.mode_count & 1)
            ? tactix_stat(s, TACTIX_STAT_TICKS) >= p.mode_ticks_limit
            : conflicts >= p.mode_conflicts_limit;
    if (want_switch && tactix_apply(s, switchmode) != -1) {
      p.mode_count++;
      if (!stable) { /* just entered stable: fresh reluctant sequence */
        p.rel_u = p.rel_v = 1;
        p.rel_wait = RELUCTANT_PERIOD;
        p.rel_trigger = 0;
      }
      update_mode_limits(&p, s);
      continue;
    }
    const int want_restart =
        conflicts < p.restart_next
            ? 0
            : (stable ? p.rel_trigger
                      : RESTART_MARGIN * p.avg[0].slow_glue.value <=
                            p.avg[0].fast_glue.value);
    if (want_restart && tactix_apply(s, restart) != -1) {
      p.rel_trigger = 0;
      if (!stable) /* focused-only limit update (kissat restart.c) */
        update_restart_limit(&p, s);
      continue;
    }
    if (stable && conflicts > p.rephase_next) {
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
        p.rephase_next = tactix_stat(s, TACTIX_STAT_CONFLICTS) +
                         (uint64_t)(REPHASE_INTERVAL * (double)n * lg * lg *
                                    lg);
        continue;
      }
    }
    /* C1 forward subsumption in the eliminate slot (after rephase,
     * before decide, like DefaultPolicy): new-material gate first (new
     * binaries / new root-fixed vars, kissat_eliminating's variables
     * trigger), then the composed root restart and the canonical round;
     * applies may be -1 (already at root / guard) */
    if (conflicts >= p.subsume_next &&
        (!p.subsume_ran ||
         tactix_stat(s, TACTIX_STAT_BINARY_CLAUSES) >
             p.subsume_binaries_seen ||
         tactix_stat(s, TACTIX_STAT_ROOT_FIXED) >
             p.subsume_root_fixed_seen)) {
      if (tactix_stat(s, TACTIX_STAT_LEVEL) > 0)
        tactix_apply_instance(s, restart, 0);
      if (tactix_apply(s, subsume) != -1) {
        p.subsume_ran = 1;
        p.subsume_binaries_seen =
            tactix_stat(s, TACTIX_STAT_BINARY_CLAUSES);
        p.subsume_root_fixed_seen = tactix_stat(s, TACTIX_STAT_ROOT_FIXED);
      }
      {
        uint64_t n = tactix_stat(s, TACTIX_STAT_SUBSUMPTIONS);
        if (!n)
          n = 1;
        const double lg = log10((double)n + 9);
        const uint64_t delta =
            (uint64_t)(SUBSUME_INTERVAL * (double)n * lg * lg);
        p.subsume_next = tactix_stat(s, TACTIX_STAT_CONFLICTS) +
                         scale_inproc_delta(s, delta);
      }
      continue;
    }
    r = tactix_apply(s, decide); /* canonical branch */
    assert(r != -1);
  }
}

/* seeded random 3-SAT near the transition (m/n = 4.26): enough conflicts
 * to exercise the full schedule — restarts, reduce, the first mode
 * switches — while staying sub-second */
static uint64_t rng_state;
static uint64_t rnd(void) {
  rng_state ^= rng_state >> 12;
  rng_state ^= rng_state << 25;
  rng_state ^= rng_state >> 27;
  return rng_state * 0x2545F4914F6CDD1DULL;
}

static void add_random3sat(tactix *s, int nvars, int nclauses,
                           uint64_t seed) {
  rng_state = seed;
  for (int c = 0; c < nclauses; c++) {
    int v[3];
    for (int i = 0; i < 3; i++) {
      int fresh;
      do {
        v[i] = (int)(rnd() % nvars) + 1;
        fresh = 1;
        for (int j = 0; j < i; j++)
          if (v[j] == v[i])
            fresh = 0;
      } while (!fresh);
      tactix_add(s, (rnd() & 1) ? v[i] : -v[i]);
    }
    tactix_add(s, 0);
  }
}

static const int STATS[] = {
    TACTIX_STAT_CONFLICTS, TACTIX_STAT_DECISIONS, TACTIX_STAT_PROPAGATIONS,
    TACTIX_STAT_LEARNED,   TACTIX_STAT_RESTARTS,  TACTIX_STAT_REDUCTIONS,
    TACTIX_STAT_SWITCHES,  TACTIX_STAT_TICKS};
#define NSTATS (int)(sizeof(STATS) / sizeof(STATS[0]))

int main(int argc, char **argv) {
  /* pinned size: ~2.5k conflicts — every schedule component fires (first
   * mode switch at 1000 conflicts, first reduce at 2000, restart floor
   * 50) while the stepped side stays fast under the per-application
   * invariant checker of debug builds. argv overrides for exploration:
   * demo3 [N M SEED] */
  int N = 132, M = 562; /* m/n = 4.26 */
  uint64_t SEED = 20260714;
  if (argc == 4) {
    N = atoi(argv[1]);
    M = atoi(argv[2]);
    SEED = strtoull(argv[3], NULL, 10);
  }

  tactix *builtin = tactix_init();
  add_random3sat(builtin, N, M, SEED);
  const int expect = tactix_solve(builtin);
  assert(expect == 10 || expect == 20);

  tactix *scripted = tactix_init();
  add_random3sat(scripted, N, M, SEED);
  const int got = scripted_solve(scripted, (uint64_t)N);
  assert(got == expect);

  /* "within noise" checked in its strongest form: the mirrored schedule
   * is trajectory-identical, so every counter matches exactly */
  for (int i = 0; i < NSTATS; i++) {
    const uint64_t b = tactix_stat(builtin, STATS[i]);
    const uint64_t c = tactix_stat(scripted, STATS[i]);
    if (b != c) {
      fprintf(stderr, "stat %d diverged: builtin %llu scripted %llu\n",
              STATS[i], (unsigned long long)b, (unsigned long long)c);
      assert(0 && "scripted policy diverged from DefaultPolicy");
    }
  }
  printf("demo3: verdict %d, conflicts %llu, restarts %llu, "
         "reductions %llu, switches %llu, rephased %llu — "
         "scripted == builtin on all\n",
         expect,
         (unsigned long long)tactix_stat(builtin, TACTIX_STAT_CONFLICTS),
         (unsigned long long)tactix_stat(builtin, TACTIX_STAT_RESTARTS),
         (unsigned long long)tactix_stat(builtin, TACTIX_STAT_REDUCTIONS),
         (unsigned long long)tactix_stat(builtin, TACTIX_STAT_SWITCHES),
         (unsigned long long)tactix_stat(builtin, TACTIX_STAT_REPHASED));
  /* and every schedule component actually fired */
  assert(tactix_stat(builtin, TACTIX_STAT_RESTARTS) > 0);
  assert(tactix_stat(builtin, TACTIX_STAT_SWITCHES) > 0);
  assert(tactix_stat(builtin, TACTIX_STAT_REDUCTIONS) > 0);
  assert(tactix_stat(builtin, TACTIX_STAT_REPHASED) > 0);

  tactix_release(builtin);
  tactix_release(scripted);
  puts("demo3_scripted_default OK");
  return 0;
}
