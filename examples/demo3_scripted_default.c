/* §8.4 demo policy 3 — a scripted policy reproducing the default
 * schedule "within noise" (the plan's Python variant lands with the
 * phase-4 bindings; the schedule logic is what is under review here).
 *
 * This client re-implements DefaultPolicy::solve outside the library,
 * pull-based (O5), using only the frozen v1 API: canonical rule
 * applications plus tactix_stat for the counters the built-in policy
 * reads (conflicts, ticks, last learned glue, mode). Because the
 * arithmetic is replicated exactly (same EMA
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

#include "tactix_scripted_default.h"

static int scripted_solve(tactix *s) {
  tactix_scripted_default program;
  tactix_scripted_default_begin(&program);
  return tactix_scripted_default_run(&program,s,NULL,NULL);
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
  /* Lucky can refute a formula as well as find a model. Ignoring its UNSAT
   * result used to fall through to the unassigned-variable SAT shortcut. */
  for (int guidance = TACTIX_GUIDANCE_DISCARD;
       guidance <= TACTIX_GUIDANCE_NATIVE; ++guidance) {
    tactix *s = tactix_init();
    assert(tactix_set_guidance_publication(s, guidance) == 0);
    for (int a = -1; a <= 1; a += 2)
      for (int b = -2; b <= 2; b += 4) {
        tactix_add(s, a); tactix_add(s, b); tactix_add(s, 0);
      }
    assert(scripted_solve(s) == 20);
    tactix_release(s);
  }
  /* Pinned formula: every schedule component fires on BOTH runs (first mode
   * switch at 1000 conflicts, first reduce at REDUCE_INTERVAL_INIT = 2000,
   * restart floor 50) while the stepped side remains practical under the
   * per-application invariant checker of Debug builds.  The earlier seed
   * drove the external schedule through about 75,000 conflicts and made this
   * conformance test take tens of minutes without covering another API state.
   * This seed retains every asserted schedule transition with about 4,000
   * external conflicts.  A trajectory that loses one remains a test failure.
   * argv overrides for exploration: demo3 [N M SEED] */
  int N = 140, M = 596; /* m/n = 4.26 */
  uint64_t SEED = 75;
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
