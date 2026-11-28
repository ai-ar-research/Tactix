/* tactix_cdcl_*: DefaultPolicy search episodes interleaved with root work. */
#undef NDEBUG
#include "tactix.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { VARIABLES = 80, CLAUSES = 344 };
static int formula[CLAUSES][3];

/* A seeded random 3-SAT formula near the threshold: many conflicts. */
static void generate(uint64_t seed) {
  for (int c = 0; c < CLAUSES; ++c)
    for (int i = 0; i < 3; ++i) {
      seed = seed * 6364136223846793005u + 1442695040888963407u;
      const int v = 1 + (int)((seed >> 33) % VARIABLES);
      formula[c][i] = (seed >> 13) & 1 ? v : -v;
    }
}

static tactix *session(void) {
  tactix *s = tactix_init();
  for (int c = 0; c < CLAUSES; ++c) {
    for (int i = 0; i < 3; ++i) tactix_add(s, formula[c][i]);
    tactix_add(s, 0);
  }
  return s;
}

/* tactix_value returns the literal itself when it is true. */
static int is_true(tactix *s, int literal) { return tactix_value(s, literal) == literal; }

static void check_model(tactix *s) {
  for (int c = 0; c < CLAUSES; ++c)
    assert(is_true(s, formula[c][0]) || is_true(s, formula[c][1]) || is_true(s, formula[c][2]));
}

static int reference(void) {
  tactix *s = session();
  const int r = tactix_solve(s);
  tactix_release(s);
  return r;
}

static int rule(const char *name) {
  for (int i = 0; i < tactix_rule_count(); ++i)
    if (!strcmp(tactix_rule_name(i), name)) return i;
  return -1;
}

static void inprocess(tactix *s) {
  if (tactix_apply(s, rule("Propagate")) == 20) return;
  (void)tactix_apply(s, rule("Probe"));
  (void)tactix_apply(s, rule("Vivify"));
}

/* Settings as NULL-terminated key/value pairs. */
static void configure(tactix_cdcl *c, const char *const *settings) {
  for (; *settings; settings += 2) assert(!tactix_cdcl_set(c, settings[0], settings[1]));
}

/* Runs `settings` in slices of `budget` conflicts, with `root` between them,
 * and checks every return. */
static int run(const char *const *settings, uint64_t budget, void (*root)(tactix *),
               tactix **out) {
  tactix *s = session();
  tactix_cdcl *search = tactix_cdcl_create(s);
  configure(search, settings);
  for (int slices = 0;; ++slices) {
    const int r = tactix_cdcl_run(s, search, budget);
    if (r == 10) check_model(s);
    if (r == 10 || r == 20) {
      tactix_cdcl_release(search);
      *out = s;
      return r;
    }
    assert(r == 0 && tactix_stat(s, TACTIX_STAT_LEVEL) == 0);
    /* Otherwise a run ends at a restart, a due pass, or its budget. */
    if (root) root(s);
    assert(slices < 100000);
  }
}

int main(void) {
  generate(7);
  {
    tactix *s = session(), *other = session();
    tactix_cdcl *c = tactix_cdcl_create(s);
    const char *const bad[][2] = {{"restart.policy", "6"},    {"mode.force", "3"},
                                  {"restart.base_interval", "0"},
                                  {"restart.growth_permille", "1000"},
                                  {"stop.due_pass", "1"},     {"stop.restart", "0"},
                                  {"analysis.conflict_er", "x"},
                                  {"inprocess.probe", "0"},   {"restart.policy", "-1"}};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; ++i)
      assert(tactix_cdcl_set(c, bad[i][0], bad[i][1]) == -1);
    assert(tactix_cdcl_set(NULL, "mode.force", "1") == -1 && !tactix_cdcl_create(NULL));
    assert(tactix_cdcl_run(other, c, 10) == -1 && tactix_cdcl_run(s, NULL, 10) == -1);
    tactix_cdcl_release(c);
    tactix_release(other);
    tactix_release(s);
  }

  static const char *const native[] = {NULL};
  static const char *const focused_glue[] = {"mode.force", "1", "restart.policy", "1", NULL};
  static const char *const stable_luby[] = {"mode.force", "2", "restart.policy", "2",
                                            "restart.base_interval", "32", NULL};
  static const char *const geometric[] = {"restart.policy", "3", "restart.base_interval", "50",
                                          "restart.growth_permille", "1500",
                                          "reduction.tier1_default", "3", NULL};
  static const char *const fixed[] = {"mode.force", "1", "restart.policy", "4",
                                      "restart.base_interval", "10", "reorder.mode", "0", NULL};
  static const char *const none[] = {"mode.force", "2", "restart.policy", "5",
                                     "analysis.conflict_er", "1", NULL};
  const char *const *const flavors[] = {native, focused_glue, stable_luby, geometric, fixed, none};
  enum { FLAVORS = sizeof flavors / sizeof *flavors };
  for (uint64_t seed = 1; seed <= 4; ++seed) {
    generate(seed);
    const int expected = reference();
    for (size_t f = 0; f < FLAVORS; ++f) {
      tactix *s;
      /* One unbounded run, budget slices, and slices with root work. */
      assert(run(flavors[f], 0, NULL, &s) == expected);
      const uint64_t restarts = tactix_stat(s, TACTIX_STAT_RESTARTS);
      const uint64_t conflicts = tactix_stat(s, TACTIX_STAT_CONFLICTS);
      if (flavors[f] == none) assert(!restarts);
      /* At most one per interval; trail reuse can make a scheduled one a no-op. */
      if (flavors[f] == fixed) assert(restarts && restarts <= conflicts / 10);
      if (flavors[f] == focused_glue || flavors[f] == fixed)
        assert(!tactix_stat(s, TACTIX_STAT_STABLE) && !tactix_stat(s, TACTIX_STAT_SWITCHES));
      if (flavors[f] == stable_luby || flavors[f] == none)
        assert(tactix_stat(s, TACTIX_STAT_STABLE) && tactix_stat(s, TACTIX_STAT_SWITCHES) == 1);
      tactix_release(s);
      assert(run(flavors[f], 37, NULL, &s) == expected);
      tactix_release(s);
      assert(run(flavors[f], 100, inprocess, &s) == expected);
      tactix_release(s);
    }
  }

  /* One handle with new settings for each episode. */
  generate(3);
  {
    const int expected = reference();
    tactix *s = session();
    tactix_cdcl *c = tactix_cdcl_create(s);
    int r = 0;
    for (int i = 0; !r; ++i) {
      configure(c, flavors[i % FLAVORS]);
      r = tactix_cdcl_run(s, c, 64);
      if (!r) inprocess(s);
    }
    assert(r == expected);
    if (r == 10) check_model(s);
    tactix_cdcl_release(c);
    tactix_release(s);
  }
  return 0;
}
