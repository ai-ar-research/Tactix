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
 * from default_policy.hpp), the SEARCH schedule is trajectory-identical.
 * The inprocessing PHASE is reproduced structurally (preprocess at
 * conflicts=0: BVE to its budget + the batch simplifiers once per outer
 * round) but NOT bit-identically: DefaultPolicy's BVE reads an internal
 * dense occurrence index while the API path uses the arena-scan fallback,
 * so the two take equivalent-but-different eliminations. The "within noise"
 * acceptance is therefore the SAME VERDICT plus every scheduled component
 * firing on both runs — not exact counter-equality (N1 requires
 * applicability, which this demo exercises and the fuzzer verifies; the
 * dense optimization gives up bit-exact external reproduction). If
 * DefaultPolicy's schedule changes, update this mirror deliberately. */
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
#define SUBSUME_INTERVAL_INIT 500
#define SUBSUME_INTERVAL 500
#define INPROCESS_PHASE_BUDGET 300
#define INPROCESS_PHASE_ROUNDS 3
/* mirror of DefaultPolicy::SLOT_IN_DEFAULT (the registry-driven
 * inprocessing slot, rev 26): default composition = Strengthen enabled,
 * SubsumeForward disabled (fused round covers it) — the mirror applies
 * exactly the Strengthen canonical instance per slot pass. Deferred
 * with the same s055 margin verdict as DefaultPolicy (see
 * default_policy.hpp); flips together with it. */
#define SLOT_IN_DEFAULT 0

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
  uint64_t inprocess_next;
  int inprocess_ran; /* the initial full pass is never gated */
  uint64_t inprocess_rounds; /* interval growth key (policy-local) */
  uint64_t inprocess_binaries_seen, inprocess_root_fixed_seen;
};

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

static int scripted_solve(tactix *s) {
  struct sched p;
  sched_init(&p);
  /* the first inprocessing limit is size-scaled at solve entry, like
   * DefaultPolicy's lazy init */
  /* preprocessing-only: the first (and only) phase fires at conflicts=0,
     before search state exists (DefaultPolicy update_inprocess_limit) */
  p.inprocess_next = SLOT_IN_DEFAULT ? tactix_stat(s, TACTIX_STAT_CONFLICTS)
                                     : UINT64_MAX;
  const int propagate = rule_id("Propagate");
  const int conflictanalysis = rule_id("ConflictAnalysis");
  const int decide = rule_id("Decide");
  const int reduce = rule_id("Reduce");
  const int restart = rule_id("Restart");
  const int switchmode = rule_id("SwitchMode");
  const int rephase = rule_id("Rephase");
  const int lucky = rule_id("Lucky");
  const int strengthen = rule_id("Strengthen");
  const int bve = rule_id("BVE");
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
        observe_glue(&p, st,
                     (double)tactix_stat(s, TACTIX_STAT_LAST_LEARNED_GLUE));
        if (st)
          tick_reluctant(&p);
      }
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
    if (conflicts >= p.inprocess_next &&
        (!p.inprocess_ran ||
         tactix_stat(s, TACTIX_STAT_BINARY_CLAUSES) >
             p.inprocess_binaries_seen ||
         tactix_stat(s, TACTIX_STAT_ROOT_FIXED) >
             p.inprocess_root_fixed_seen)) {
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
      for (int outer = 0; outer < INPROCESS_PHASE_ROUNDS; outer++) {
        int round_changed = 0;
        while (bve_applied < INPROCESS_PHASE_BUDGET) {
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
      p.inprocess_ran = 1;
      p.inprocess_rounds++;
      p.inprocess_binaries_seen = tactix_stat(s, TACTIX_STAT_BINARY_CLAUSES);
      p.inprocess_root_fixed_seen = tactix_stat(s, TACTIX_STAT_ROOT_FIXED);
      p.inprocess_next = UINT64_MAX; /* preprocessing-only */
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
  /* pinned size: every schedule component must fire on BOTH runs (first mode
   * switch at 1000 conflicts, first reduce at REDUCE_INTERVAL_INIT = 2000,
   * restart floor 50) while the stepped side stays fast under the
   * per-application invariant checker of debug builds. Re-pinned 2026-07-21
   * (was 132/562): that size left the built-in run at ~2.4k conflicts, barely
   * past the reduce threshold, so an ordinary trajectory shift — the rev-53
   * effort ledger changed BVE's elimination ORDER and moved it to ~2.0k —
   * dropped it below and the coverage assertions failed on a change that was
   * neutral on Tier A. The new size clears every threshold by ~3-5x on both
   * runs, so coverage no longer rides on a trajectory coincidence.
   * argv overrides for exploration: demo3 [N M SEED] */
  int N = 152, M = 647; /* m/n = 4.26 */
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
  const int got = scripted_solve(scripted);
  assert(got == expect);

  /* §8.4 "within noise": the scripted policy drives the SAME schedule through
   * the public API and reaches the SAME VERDICT (asserted above). Exact
   * counter-equality no longer holds — DefaultPolicy's inprocessing BVE reads
   * an internal dense occurrence index while this API path uses the arena-scan
   * fallback, so the two take equivalent-but-different elimination trajectories
   * (N1 is preserved: every rule is applicable via the API at any state, which
   * this demo exercises and the fuzzer verifies; bit-exact reproduction was a
   * bonus the dense optimization gives up). Report both trajectories and check
   * every scheduled component fired on BOTH the built-in and the scripted run. */
  printf("demo3: verdict %d — builtin{confl %llu rst %llu red %llu sw %llu rp %llu}"
         "  scripted{confl %llu rst %llu red %llu sw %llu rp %llu}\n",
         expect,
         (unsigned long long)tactix_stat(builtin, TACTIX_STAT_CONFLICTS),
         (unsigned long long)tactix_stat(builtin, TACTIX_STAT_RESTARTS),
         (unsigned long long)tactix_stat(builtin, TACTIX_STAT_REDUCTIONS),
         (unsigned long long)tactix_stat(builtin, TACTIX_STAT_SWITCHES),
         (unsigned long long)tactix_stat(builtin, TACTIX_STAT_REPHASED),
         (unsigned long long)tactix_stat(scripted, TACTIX_STAT_CONFLICTS),
         (unsigned long long)tactix_stat(scripted, TACTIX_STAT_RESTARTS),
         (unsigned long long)tactix_stat(scripted, TACTIX_STAT_REDUCTIONS),
         (unsigned long long)tactix_stat(scripted, TACTIX_STAT_SWITCHES),
         (unsigned long long)tactix_stat(scripted, TACTIX_STAT_REPHASED));
  for (int w = 0; w < 2; w++) {
    tactix *t = w ? scripted : builtin;
    assert(tactix_stat(t, TACTIX_STAT_RESTARTS) > 0);
    assert(tactix_stat(t, TACTIX_STAT_SWITCHES) > 0);
    assert(tactix_stat(t, TACTIX_STAT_REDUCTIONS) > 0);
    assert(tactix_stat(t, TACTIX_STAT_REPHASED) > 0);
  }
  (void)STATS;

  tactix_release(builtin);
  tactix_release(scripted);
  puts("demo3_scripted_default OK");
  return 0;
}
