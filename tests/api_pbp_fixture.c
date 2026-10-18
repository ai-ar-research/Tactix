#undef NDEBUG
#include "tactix.h"

#include <assert.h>
#include <stdio.h>

static void add_clause(tactix *solver, const int *literals) {
  while (*literals)
    tactix_add(solver, *literals++);
  tactix_add(solver, 0);
}

int main(int argc, char **argv) {
  assert(argc == 3);
  FILE *cnf = fopen(argv[1], "w");
  assert(cnf);
  fputs("p cnf 2 4\n1 2 0\n1 -2 0\n-1 2 0\n-1 -2 0\n", cnf);
  assert(fclose(cnf) == 0);

  tactix *solver = tactix_init();
  assert(solver);
  add_clause(solver, (const int[]){1, 2, 0});
  add_clause(solver, (const int[]){1, -2, 0});
  assert(tactix_proof(solver, argv[2], TACTIX_PROOF_PBP) == 0);
  add_clause(solver, (const int[]){-1, 2, 0});
  add_clause(solver, (const int[]){-1, -2, 0});
  assert(tactix_solve(solver) == 20);
  tactix_release(solver);
  return 0;
}
