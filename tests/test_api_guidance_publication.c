/* Public guidance publication (gap plan Step 2, finding 2). A BVE / Factor /
 * Substitute sequence, each step followed by Propagate, runs on three solvers:
 * native (tactix_apply_instance under TACTIX_GUIDANCE_NATIVE), reference (the
 * same applications as online steps, published by the shared executor with
 * DefaultPolicy's HeuristicRulePublisher) and discard (the v1 default). The
 * formula census stays identical; native candidates equal the reference's in
 * focused (queue) and stable (score) order, while the discarded solver keeps
 * eliminated/substituted variables queued and the fresh Factor variable at
 * its activation score. */
#undef NDEBUG
#include "tactix.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define VARIABLES 17
#define CAP 64

static int rule(const char *name) {
  for (int id = 0; id < tactix_rule_count(); ++id)
    if (!strcmp(tactix_rule_name(id), name))
      return id;
  assert(0);
  return -1;
}

static void clause(tactix *s, const int *literals) {
  while (*literals)
    tactix_add(s, *literals++);
  tactix_add(s, 0);
}

/* 1<->2 is a Substitute equivalence; 5 and 16 are BVE candidates (16's two
 * occurrences resolve to the root unit 17); 8..13 form a 3x3 Factor grid. */
static tactix *load(void) {
  tactix *s = tactix_init();
  clause(s, (const int[]){-1, 2, 0});
  clause(s, (const int[]){1, -2, 0});
  clause(s, (const int[]){1, 3, 4, 0});
  clause(s, (const int[]){-2, -3, 4, 0});
  clause(s, (const int[]){5, 6, 0});
  clause(s, (const int[]){-5, 7, 0});
  clause(s, (const int[]){6, 7, 14, 0});
  clause(s, (const int[]){16, 17, 0});
  clause(s, (const int[]){-16, 17, 0});
  clause(s, (const int[]){-17, 15, 3, 0});
  for (int a = 8; a <= 10; ++a)
    for (int b = 11; b <= 13; ++b)
      clause(s, (const int[]){a, b, 0});
  clause(s, (const int[]){-8, -11, 14, 0});
  assert(tactix_status(s) == 0);
  return s;
}

typedef struct {
  uint64_t irredundant, root_fixed, unassigned, eliminated, factorings,
      substituted;
} Census;

static Census census(tactix *s) {
  Census c;
  c.irredundant = tactix_stat(s, TACTIX_STAT_IRREDUNDANT_CLAUSES);
  c.root_fixed = tactix_stat(s, TACTIX_STAT_ROOT_FIXED);
  c.unassigned = tactix_stat(s, TACTIX_STAT_UNASSIGNED);
  c.eliminated = tactix_stat(s, TACTIX_STAT_BVE_ELIMINATED);
  c.factorings = tactix_stat(s, TACTIX_STAT_FACTORINGS);
  c.substituted = tactix_stat(s, TACTIX_STAT_SUBSTITUTE_VARS);
  return c;
}

static int same_census(Census a, Census b) {
  return a.irredundant == b.irredundant && a.root_fixed == b.root_fixed &&
         a.unassigned == b.unassigned && a.eliminated == b.eliminated &&
         a.factorings == b.factorings && a.substituted == b.substituted;
}

typedef struct {
  int count;
  int literals[CAP];
} Candidates;

static Candidates candidates(tactix *s) {
  Candidates c;
  c.count = tactix_decide_candidates(s, c.literals, CAP);
  assert(c.count >= 0 && c.count < CAP);
  return c;
}

static int same_candidates(const Candidates *a, const Candidates *b) {
  return a->count == b->count &&
         !memcmp(a->literals, b->literals, sizeof(int) * (size_t)a->count);
}

static int position(const Candidates *c, int variable) {
  for (int i = 0; i < c->count; ++i)
    if (c->literals[i] == variable || c->literals[i] == -variable)
      return i;
  assert(0);
  return -1;
}

static void print(const char *label, const Candidates *c) {
  printf("%s:", label);
  for (int i = 0; i < c->count; ++i)
    printf(" %d", c->literals[i]);
  printf("\n");
}

static void public_step(tactix *s, int rule_id, uint64_t instance) {
  assert(tactix_apply_instance(s, rule_id, instance) == 0);
  const int propagated = tactix_apply(s, rule("Propagate"));
  assert(propagated == -1 || propagated == 0 || propagated == 1);
}

static void online_step(tactix *s, int rule_id, uint64_t instance,
                        uint64_t revision) {
  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = 1;
  step.rule_id = rule_id;
  step.granularity = instance == TACTIX_INSTANCE_CANONICAL
                         ? TACTIX_ONLINE_GRANULARITY_CANONICAL
                         : TACTIX_ONLINE_GRANULARITY_ONE;
  step.instances = instance == TACTIX_INSTANCE_CANONICAL ? NULL : &instance;
  step.instance_count = instance == TACTIX_INSTANCE_CANONICAL ? 0 : 1;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = rule_id == rule("BVE") ? TACTIX_ONLINE_WORK_BVE_TICKS
                                          : TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  step.post_application = TACTIX_ONLINE_POST_PROPAGATE;
  const tactix_online_program program = {&step, 1, TACTIX_ONLINE_REPEAT_ONCE,
                                         1};
  assert(tactix_online_configure(s, &program, revision,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  tactix_online_result result;
  assert(tactix_online_advance(s, &result) == 0);
  assert(result.made_rule_progress);
}

static void mode_surface(void) {
  tactix *s = tactix_init();
  assert(tactix_config(s, TACTIX_CFG_GUIDANCE_PUBLICATION) ==
         TACTIX_GUIDANCE_DISCARD);
  assert(tactix_set_guidance_publication(s, 2) == -1);
  assert(tactix_set_guidance_publication(s, -1) == -1);
  assert(tactix_config(s, TACTIX_CFG_GUIDANCE_PUBLICATION) ==
         TACTIX_GUIDANCE_DISCARD);
  assert(tactix_set_guidance_publication(s, TACTIX_GUIDANCE_NATIVE) == 0);
  assert(tactix_config(s, TACTIX_CFG_GUIDANCE_PUBLICATION) ==
         TACTIX_GUIDANCE_NATIVE);
  assert(tactix_set_guidance_publication(s, TACTIX_GUIDANCE_DISCARD) == 0);
  assert(tactix_config(s, TACTIX_CFG_GUIDANCE_PUBLICATION) ==
         TACTIX_GUIDANCE_DISCARD);
  tactix_release(s);
}

int main(void) {
  mode_surface();
  tactix *native = load(), *reference = load(), *discard = load();
  assert(tactix_set_guidance_publication(native, TACTIX_GUIDANCE_NATIVE) == 0);
  /* Save negative phases on 10 and 3 so candidates carry nontrivial phases. */
  tactix *solvers[] = {native, reference, discard};
  for (int i = 0; i < 3; ++i) {
    assert(tactix_decide(solvers[i], -10) == 0);
    assert(tactix_decide(solvers[i], -3) == 0);
    assert(tactix_apply(solvers[i], rule("Propagate")) == 0);
    assert(tactix_apply_instance(solvers[i], rule("Restart"), 0) == 0);
  }
  {
    const Candidates n = candidates(native), r = candidates(reference),
                     d = candidates(discard);
    assert(same_candidates(&n, &r) && same_candidates(&n, &d));
    assert(n.literals[position(&n, 10)] == -10);
    assert(n.literals[position(&n, 3)] == -3);
  }
  /* Candidates list the queue (most preferred first), then every other
   * unassigned variable unranked in index order; N1 keeps retired variables
   * decidable. Publication moves a retired variable into that unranked tail. */
  const struct {
    const char *rule;
    uint64_t instance;
    int retired; /* the variable this step retires, or 0 */
  } steps[] = {
      {"BVE", TACTIX_LIT_INSTANCE(5), 5},
      {"BVE", TACTIX_LIT_INSTANCE(16), 16},
      {"Factor", TACTIX_LIT_INSTANCE(8), 0},
      {"Substitute", TACTIX_INSTANCE_CANONICAL, 0},
  };
  int retired = 0;
  for (size_t i = 0; i < sizeof steps / sizeof steps[0]; ++i) {
    const int id = rule(steps[i].rule);
    public_step(native, id, steps[i].instance);
    public_step(discard, id, steps[i].instance);
    online_step(reference, id, steps[i].instance, i + 1);
    const Census n = census(native);
    assert(same_census(n, census(reference)));
    assert(same_census(n, census(discard)));
    const Candidates nc = candidates(native), rc = candidates(reference),
                     dc = candidates(discard);
    print(steps[i].rule, &nc);
    assert(same_candidates(&nc, &rc));
    if (steps[i].retired) {
      const int unranked = nc.count - ++retired;
      assert(position(&nc, steps[i].retired) >= unranked);
      assert(position(&dc, steps[i].retired) < unranked);
    }
  }
  /* The sequence did what it names: two eliminations (one root unit), one
   * fresh Factor variable, one substitution. */
  const Census done = census(native);
  assert(done.eliminated == 2 && done.factorings == 1 && done.substituted == 1);
  assert(done.root_fixed >= 1);
  {
    /* Factor's fresh variable: demoted to least preferred among the queued
     * variables under NATIVE, most preferred under DISCARD. The substituted
     * variable joins the unranked tail. */
    const Candidates nc = candidates(native), dc = candidates(discard);
    const int fresh = VARIABLES + 1;
    assert(position(&nc, fresh) == nc.count - 4);
    assert(position(&dc, fresh) == 0);
    assert(position(&nc, 1) >= nc.count - 3 || position(&nc, 2) >= nc.count - 3);
  }
  /* Stable mode ranks candidates by score: Factor's demoted fresh variable
   * has score zero under NATIVE and its activation score under DISCARD. */
  const int switch_mode = rule("SwitchMode");
  assert(tactix_apply(native, switch_mode) == 0);
  assert(tactix_apply(reference, switch_mode) == 0);
  assert(tactix_apply(discard, switch_mode) == 0);
  assert(tactix_stat(native, TACTIX_STAT_STABLE) == 1);
  {
    const Candidates nc = candidates(native), rc = candidates(reference),
                     dc = candidates(discard);
    print("stable", &nc);
    print("stable-discard", &dc);
    assert(same_candidates(&nc, &rc));
    assert(!same_candidates(&nc, &dc));
    assert(position(&nc, VARIABLES + 1) == nc.count - 1);
    assert(position(&dc, VARIABLES + 1) == 0);
  }
  tactix_release(native);
  tactix_release(reference);
  tactix_release(discard);
  printf("guidance publication: native matches the shared executor\n");
  return 0;
}
