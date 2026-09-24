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

int main(int argc, char **argv) {
  if (argc != 4 ||
      (strcmp(argv[3], "binary") && strcmp(argv[3], "large") &&
       strcmp(argv[3], "sat")))
    return 2;
  char cnf[4096], proof[4096];
  if (snprintf(cnf, sizeof cnf, "%s.cnf", argv[1]) >= (int)sizeof cnf ||
      snprintf(proof, sizeof proof, "%s.proof", argv[1]) >=
          (int)sizeof proof)
    return 2;
  const int large = !strcmp(argv[3], "large");
  const int sat = !strcmp(argv[3], "sat");
  static const int binary_candidate[] = {1, 2, 0};
  static const int large_candidate[] = {1, 2, 7, 0};
  static const int d[] = {-1, 3, 0};
  static const int e[] = {-3, 4, 0};
  static const int core[][3] = {
      {5, 6, 0}, {5, -6, 0}, {-5, 6, 0}, {-5, -6, 0}};
  const int *candidate = large ? large_candidate : binary_candidate;
  FILE *original = fopen(cnf, "w");
  if (!original)
    return 2;
  fprintf(original, "p cnf %d %d\n", sat ? 4 : large ? 7 : 6,
          sat ? 4 : 8);
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
  if (!sat)
    for (size_t i = 0; i < 4; ++i)
      add(solver, core[i]);
  const int wrong[] = {1};
  const int cube[] = {1, 3, 4};
  assert(tactix_apply_condition(solver, candidate, large ? 3 : 2,
                                wrong, 1) == -1);
  assert(tactix_apply_condition(solver, candidate, large ? 3 : 2,
                                cube, 3) == 0);
  if (sat) {
    assert(tactix_decide(solver, -1) == 0);
    assert(tactix_solve(solver) == 10);
    assert(tactix_value(solver, 1) == 1);
    assert(tactix_value(solver, 3) == 3);
    assert(tactix_value(solver, 4) == 4);
    puts("s SATISFIABLE");
    fputs("v", stdout);
    for (int v = 1; v <= 4; ++v)
      printf(" %d", tactix_value(solver, v));
    puts(" 0");
  } else {
    assert(tactix_solve(solver) == 20);
    puts("condition certificate driver UNSAT");
  }
  tactix_release(solver);
  return 0;
}
