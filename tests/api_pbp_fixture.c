#undef NDEBUG
#include "api_test_support.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* A unit among the clauses added before the proof is selected: the input
 * replay derives root units under fresh ids, so its clause ids drift from the
 * live kernel's, and later deletions must not name the live ids. Seeded
 * random 3-SAT at clause ratio 5.5 so that search deletes input clauses.
 * Debug builds audit every `del id` against the constraint it names. */
static int divergent(const char *cnf_path, const char *proof_path) {
  enum { VARS = 55, RANDOM = 300, EARLY = 3 };
  static int clauses[RANDOM + 2][4];
  clauses[0][0] = -1, clauses[0][1] = 2, clauses[0][2] = 0;
  clauses[1][0] = 1, clauses[1][1] = 0;
  uint64_t state = 20261102;
  for (int c = 2; c < RANDOM + 2; ++c) {
    for (int i = 0; i < 3; ++i) {
      int literal, repeated;
      do {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        literal = 1 + (int)((state >> 33) % VARS);
        repeated = 0;
        for (int j = 0; j < i; ++j)
          repeated |= clauses[c][j] == literal || clauses[c][j] == -literal;
      } while (repeated);
      clauses[c][i] = (state >> 32) & 1 ? literal : -literal;
    }
    clauses[c][3] = 0;
  }
  FILE *cnf = fopen(cnf_path, "w");
  assert(cnf);
  fprintf(cnf, "p cnf %d %d\n", VARS, RANDOM + 2);
  for (int c = 0; c < RANDOM + 2; ++c) {
    for (const int *l = clauses[c]; *l; ++l)
      fprintf(cnf, "%d ", *l);
    fputs("0\n", cnf);
  }
  assert(fclose(cnf) == 0);
  tactix *solver = tactix_init();
  assert(solver);
  for (int c = 0; c < RANDOM + 2; ++c) {
    if (c == EARLY)
      assert(tactix_proof(solver, proof_path, TACTIX_PROOF_PBP) == 0);
    add_clause(solver, clauses[c]);
  }
  assert(tactix_solve(solver) == 20);
  tactix_release(solver);
  return 0;
}

/* The documented order: select the proof before the first tactix_add, so it
 * attaches directly with no replay. Seeded random 2+3-SAT whose first clause
 * is ternary; root simplification deletes binaries after input. A binary must
 * never be deleted by an id: finish_input discards the binary-id archive, and
 * its sentinel id would name input clause 1. */
static int early(const char *cnf_path, const char *proof_path) {
  enum { VARS = 50, BINARIES = 60, TERNARIES = 170, TOTAL = 1 + 60 + 170 };
  static int clauses[TOTAL][4];
  int width[TOTAL];
  uint64_t state = 2 * 0x9e3779b97f4a7c15ull + 7;
#define NEXT() (state = state * 6364136223846793005ull + 1442695040888963407ull, \
                (uint32_t)(state >> 33))
  width[0] = 3;
  for (int c = 1; c < TOTAL; ++c)
    width[c] = c <= BINARIES ? 2 : 3;
  for (int c = TOTAL - 1; c > 1; --c) { /* shuffle positions 1..TOTAL-1 */
    const int j = 1 + (int)(NEXT() % (uint32_t)c);
    const int w = width[c];
    width[c] = width[j];
    width[j] = w;
  }
  for (int c = 0; c < TOTAL; ++c) {
    for (int i = 0; i < width[c]; ++i) {
      int literal, repeated;
      do {
        literal = 1 + (int)(NEXT() % (uint32_t)VARS);
        repeated = 0;
        for (int j = 0; j < i; ++j)
          repeated |= clauses[c][j] == literal || clauses[c][j] == -literal;
      } while (repeated);
      clauses[c][i] = NEXT() & 1 ? literal : -literal;
    }
    clauses[c][width[c]] = 0;
  }
#undef NEXT
  FILE *cnf = fopen(cnf_path, "w");
  assert(cnf);
  fprintf(cnf, "p cnf %d %d\n", VARS, TOTAL);
  for (int c = 0; c < TOTAL; ++c) {
    for (const int *l = clauses[c]; *l; ++l)
      fprintf(cnf, "%d ", *l);
    fputs("0\n", cnf);
  }
  assert(fclose(cnf) == 0);
  tactix *solver = tactix_init();
  assert(solver);
  assert(tactix_proof(solver, proof_path, TACTIX_PROOF_PBP) == 0);
  for (int c = 0; c < TOTAL; ++c)
    add_clause(solver, clauses[c]);
  assert(tactix_solve(solver) == 20);
  tactix_release(solver);
  return 0;
}

int main(int argc, char **argv) {
  assert(argc == 3 || argc == 4);
  if (argc == 4) {
    if (!strcmp(argv[3], "early"))
      return early(argv[1], argv[2]);
    assert(!strcmp(argv[3], "divergent"));
    return divergent(argv[1], argv[2]);
  }
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
