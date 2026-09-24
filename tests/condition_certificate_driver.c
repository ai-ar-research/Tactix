/* Fixed public-API X5 proof witness for external original-CNF checkers. */
#undef NDEBUG
#include "tactix.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void add(tactix *solver, const int *clause) {
  for (const int *p = clause; *p; ++p)
    tactix_add(solver, *p);
  tactix_add(solver, 0);
}

static int rule_id(const char *name) {
  for (int id = 0; id < tactix_rule_count(); ++id)
    if (!strcmp(tactix_rule_name(id), name))
      return id;
  return -1;
}

int main(int argc, char **argv) {
  if ((argc != 4 && argc != 5) ||
      (argc == 5 && strcmp(argv[4], "--online-v3")) ||
      (strcmp(argv[3], "binary") && strcmp(argv[3], "large") &&
       strcmp(argv[3], "root-binary") &&
       strcmp(argv[3], "root-large") && strcmp(argv[3], "sat") &&
       strcmp(argv[3], "root-sat")))
    return 2;
  char cnf[4096], proof[4096];
  if (snprintf(cnf, sizeof cnf, "%s.cnf", argv[1]) >= (int)sizeof cnf ||
      snprintf(proof, sizeof proof, "%s.proof", argv[1]) >=
          (int)sizeof proof)
    return 2;
  const int root = !strncmp(argv[3], "root-", 5);
  const int large = !strcmp(argv[3], "large") ||
                    !strcmp(argv[3], "root-large");
  const int sat = !strcmp(argv[3], "sat") ||
                  !strcmp(argv[3], "root-sat");
  static const int binary_candidate[] = {1, 2, 0};
  static const int large_candidate[] = {1, 2, 7, 0};
  static const int d[] = {-1, 3, 0};
  static const int e[] = {-3, 4, 0};
  static const int force_two[][3] = {{-2, 8, 0}, {-2, -8, 0}};
  static const int force_seven[][3] = {{-7, 9, 0}, {-7, -9, 0}};
  static const int core[][3] = {
      {5, 6, 0}, {5, -6, 0}, {-5, 6, 0}, {-5, -6, 0}};
  const int *candidate = large ? large_candidate : binary_candidate;
  FILE *original = fopen(cnf, "w");
  if (!original)
    return 2;
  fprintf(original, "p cnf %d %d\n", root ? (large ? 9 : 8) :
          sat ? 4 : large ? 7 : 6,
          (sat ? 4 : 8) + (root ? (large ? 4 : 2) : 0));
  for (int copy = 0; copy < 2; ++copy) {
    for (const int *p = candidate; *p; ++p)
      fprintf(original, "%d ", *p);
    fputs("0\n", original);
  }
  const int *other_clauses[] = {d, e};
  for (size_t i = 0; i < 2; ++i) {
    const int *clause = other_clauses[i];
    for (const int *p = clause; *p; ++p)
      fprintf(original, "%d ", *p);
    fputs("0\n", original);
  }
  if (root) {
    for (size_t i = 0; i < 2; ++i)
      fprintf(original, "%d %d 0\n", force_two[i][0], force_two[i][1]);
    if (large)
      for (size_t i = 0; i < 2; ++i)
        fprintf(original, "%d %d 0\n", force_seven[i][0],
                force_seven[i][1]);
  }
  if (!sat)
    for (size_t i = 0; i < 4; ++i)
      fprintf(original, "%d %d 0\n", core[i][0], core[i][1]);
  assert(fclose(original) == 0);

  tactix *solver = tactix_init();
  if (!sat)
    assert(tactix_proof(solver, proof, atoi(argv[2])) == 0);
  add(solver, candidate);
  add(solver, candidate);
  add(solver, d);
  add(solver, e);
  if (root) {
    for (size_t i = 0; i < 2; ++i)
      add(solver, force_two[i]);
    if (large)
      for (size_t i = 0; i < 2; ++i)
        add(solver, force_seven[i]);
  }
  if (!sat)
    for (size_t i = 0; i < 4; ++i)
      add(solver, core[i]);
  if (root) {
    const int probe = rule_id("Probe");
    assert(probe >= 0);
    assert(tactix_apply_instance(solver, probe, 2) == 0);
    if (large)
      assert(tactix_apply_instance(solver, probe, 7) == 0);
    const int propagated = tactix_apply(solver, rule_id("Propagate"));
    assert(propagated == 0 || propagated == 1);
  }
  const int wrong[] = {-1};
  const int cube[] = {1, 3, 4};
  const int root_cube[] = {1};
  if (root)
    assert(tactix_apply_condition(solver, candidate, large ? 3 : 2,
                                  wrong, 1) == -1);
  else {
    const int insufficient[] = {1};
    assert(tactix_apply_condition(solver, candidate, large ? 3 : 2,
                                  insufficient, 1) == -1);
  }
  if (argc == 5) {
    const tactix_online_condition_instance exact = {
        candidate, large ? 3 : 2, root ? root_cube : cube, root ? 1 : 3};
    tactix_online_step_v3 step;
    memset(&step, 0, sizeof step);
    step.base.base.step_id = 375;
    step.base.base.rule_id = rule_id("Condition");
    step.base.base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
    step.base.base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
    step.base.base.repeat_limit = 1;
    step.base.base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
    step.base.base.budget_unlimited = 1;
    step.condition_instances = &exact;
    step.condition_instance_count = 1;
    const tactix_online_program_v3 program = {
        &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
    tactix_online_result applied;
    assert(tactix_online_configure_v3(solver, &program, 375,
                                      TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
    assert(tactix_online_advance(solver, &applied) == 0);
    assert(applied.status == TACTIX_ONLINE_COMPLETED &&
           applied.made_rule_progress && applied.applications == 1);
  } else
    assert(tactix_apply_condition(solver, candidate, large ? 3 : 2,
                                  root ? root_cube : cube,
                                  root ? 1 : 3) == 0);
  if (sat) {
    if (!root)
      assert(tactix_decide(solver, -1) == 0);
    assert(tactix_solve(solver) == 10);
    assert(tactix_value(solver, 1) == 1);
    assert(tactix_value(solver, 3) == 3);
    assert(tactix_value(solver, 4) == 4);
    puts("s SATISFIABLE");
    fputs("v", stdout);
    for (int v = 1; v <= (root ? 8 : 4); ++v)
      printf(" %d", tactix_value(solver, v));
    puts(" 0");
  } else {
    assert(tactix_solve(solver) == 20);
    puts("condition certificate driver UNSAT");
  }
  tactix_release(solver);
  return 0;
}
