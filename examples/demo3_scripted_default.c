/* Demo 3 drives restart, reduction, mode changes, rephasing and Lucky through
 * the shared public C search scheduler; examples/default_strategy/ is the
 * complete default strategy. */
#undef NDEBUG
#include "tactix.h"
#include "tactix_scripted_default.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

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

int main(int argc, char **argv) {
  /* Lucky can refute a formula as well as find a model. */
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
  /* Pinned formula: every schedule component fires on both runs (first mode
   * switch at 1000 conflicts, first reduce at 2000, restart floor 50) within
   * about 4,000 external conflicts, practical under the per-application
   * invariant checker of Debug builds. argv overrides: demo3 [N M SEED] */
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

  /* The verdict and scheduler coverage; trajectory and certificate equality
   * belong to the DefaultPolicy correspondence test. */
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

  tactix_release(builtin);
  tactix_release(scripted);
  puts("demo3_scripted_default OK");
  return 0;
}
