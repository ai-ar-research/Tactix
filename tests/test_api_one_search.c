/* N2: a session's search does not depend on its proof. No proof and every format,
 * attached before or after the input, take the same conflicts, decisions and
 * propagations and give the same answer, with the input added clause by clause or
 * read as DIMACS. Structural provenance is bound, because the affine matrix runs
 * only with it. */
#undef NDEBUG
#include "api_test_support.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Formulas go to the session, or to a DIMACS file through this. */
static FILE *dimacs;
static int dimacs_variables;
static long dimacs_clauses;
static void put(tactix *s, int literal) {
  if (s) {
    tactix_add(s, literal);
    return;
  }
  if (abs(literal) > dimacs_variables)
    dimacs_variables = abs(literal);
  dimacs_clauses += !literal;
  if (dimacs)
    fprintf(dimacs, literal ? "%d " : "%d\n", literal);
}

static uint64_t state;
static unsigned below(unsigned bound) {
  state = state * 6364136223846793005ULL + 1442695040888963407ULL;
  return (unsigned)((state >> 33) % bound);
}

static void add_xor(tactix *s, const int *vars, int size, int rhs) {
  for (int mask = 0; mask < 1 << size; ++mask) {
    if ((__builtin_popcount(mask) & 1) == rhs)
      continue; /* a clause forbids one assignment of the wrong parity */
    for (int i = 0; i < size; ++i)
      put(s, mask >> i & 1 ? -vars[i] : vars[i]);
    put(s, 0);
  }
}

static void add_random_clauses(tactix *s, int nvars, int count) {
  for (int i = 0; i < count; ++i) {
    for (int k = 0; k < 3; ++k) {
      const int v = 1 + (int)below((unsigned)nvars);
      put(s, below(2) ? v : -v);
    }
    put(s, 0);
  }
}

/* Plain CDCL. */
static void plain(tactix *s) {
  state = 11;
  add_random_clauses(s, 140, 600);
}

/* Consistent 4-XORs under random clauses: GF(2) reasons during the search. */
static void parity(tactix *s) {
  enum { variables = 44 };
  state = 5;
  uint8_t solution[variables + 1];
  for (int v = 1; v <= variables; ++v)
    solution[v] = (uint8_t)below(2);
  for (int i = 0; i < 17; ++i) {
    int vars[4], rhs = 0;
    for (int k = 0; k < 4; ++k) {
      vars[k] = 1 + (int)below(variables);
      for (int j = 0; j < k; ++j)
        if (vars[j] == vars[k]) {
          vars[k] = 1 + (int)below(variables);
          j = -1;
        }
      rhs ^= solution[vars[k]];
    }
    add_xor(s, vars, 4, rhs);
  }
  add_random_clauses(s, variables, 96);
}

/* Tseitin on K4 with odd charge: a startup refutation. */
static void tseitin(tactix *s) {
  add_xor(s, (const int[]){1, 2, 3}, 3, 1);
  add_xor(s, (const int[]){1, 4, 5}, 3, 0);
  add_xor(s, (const int[]){2, 4, 6}, 3, 0);
  add_xor(s, (const int[]){3, 5, 6}, 3, 0);
}

enum route { EARLY, LATE, READ };

struct run {
  int answer;
  int64_t conflicts, decisions, propagations;
  long records, matrix_reasons;
};

/* The journal's size, and how many reasons the affine matrix published in it. */
static void read_journal(FILE *records, struct run *r) {
  assert(!fseek(records, 0, SEEK_END));
  r->records = ftell(records);
  rewind(records);
  char *text = malloc((size_t)r->records + 1);
  assert(text && fread(text, 1, (size_t)r->records, records) == (size_t)r->records);
  text[r->records] = 0;
  r->matrix_reasons = 0;
  for (const char *at = text; (at = strstr(at, "\"kind\":\"affine-matrix-reason\"")); ++at)
    ++r->matrix_reasons;
  free(text);
}

static FILE *as_dimacs(void (*formula)(tactix *)) {
  dimacs_variables = 0;
  dimacs_clauses = 0;
  formula(NULL);
  FILE *file = tmpfile();
  assert(file);
  fprintf(file, "p cnf %d %ld\n", dimacs_variables, dimacs_clauses);
  dimacs = file;
  formula(NULL);
  dimacs = NULL;
  rewind(file);
  return file;
}

static struct run solve(void (*formula)(tactix *), int format, enum route route,
                        const char *path) {
  tactix *s = tactix_init();
  FILE *records = tmpfile();
  assert(records && !tactix_bind_structural_provenance(s, records));
  if (format >= 0 && route != LATE)
    assert(!tactix_proof(s, path, format));
  if (route == READ) {
    FILE *input = as_dimacs(formula);
    assert(!tactix_read_dimacs(s, input));
    fclose(input);
  } else {
    formula(s);
  }
  if (format >= 0 && route == LATE)
    assert(!tactix_proof(s, path, format));
  struct run r;
  r.answer = tactix_solve(s);
  r.conflicts = tactix_stat(s, TACTIX_STAT_CONFLICTS);
  r.decisions = tactix_stat(s, TACTIX_STAT_DECISIONS);
  r.propagations = tactix_stat(s, TACTIX_STAT_PROPAGATIONS);
  assert((format < 0) == (tactix_proof_route(s) == NULL));
  /* No proof means the input is settled, so nothing can attach any more. */
  if (format < 0)
    assert(tactix_proof(s, path, TACTIX_PROOF_PBP) == -1);
  tactix_release(s);
  read_journal(records, &r);
  fclose(records);
  return r;
}

static void same(const char *name, int format, enum route route,
                 const struct run *r, const struct run *none) {
  if (r->answer == none->answer && r->conflicts == none->conflicts &&
      r->decisions == none->decisions && r->propagations == none->propagations &&
      r->records == none->records && r->matrix_reasons == none->matrix_reasons)
    return;
  fprintf(stderr,
          "%s format %d route %d: answer %d conflicts %lld decisions %lld "
          "propagations %lld journal %ld; no proof: %d %lld %lld %lld %ld\n",
          name, format, (int)route, r->answer, (long long)r->conflicts,
          (long long)r->decisions, (long long)r->propagations, r->records,
          none->answer, (long long)none->conflicts, (long long)none->decisions,
          (long long)none->propagations, none->records);
  abort();
}

static void check(void (*formula)(tactix *), const char *name, const char *path,
                  int64_t least_conflicts, int64_t most_conflicts, long least_reasons) {
  /* Each way in has its own no-proof search to match. */
  const struct run none_added = solve(formula, -1, EARLY, path);
  const struct run none_read = solve(formula, -1, READ, path);
  assert(none_added.answer == 10 || none_added.answer == 20);
  assert(none_read.answer == none_added.answer);
  assert(none_added.conflicts >= least_conflicts && none_added.conflicts <= most_conflicts);
  assert(none_read.conflicts >= least_conflicts && none_read.conflicts <= most_conflicts);
  assert(none_added.matrix_reasons >= least_reasons);
  assert(none_read.matrix_reasons >= least_reasons);
  static const int formats[] = {TACTIX_PROOF_PBP, TACTIX_PROOF_AUTO,
                                TACTIX_PROOF_DSR_BINARY, TACTIX_PROOF_DSR_ASCII};
  for (size_t f = 0; f < sizeof formats / sizeof *formats; ++f) {
    const struct run early = solve(formula, formats[f], EARLY, path);
    const struct run late = solve(formula, formats[f], LATE, path);
    const struct run read = solve(formula, formats[f], READ, path);
    same(name, formats[f], EARLY, &early, &none_added);
    same(name, formats[f], LATE, &late, &none_added);
    same(name, formats[f], READ, &read, &none_read);
  }
  printf("%s: answer %d, %lld conflicts added and %lld read, in every mode\n", name,
         none_added.answer, (long long)none_added.conflicts,
         (long long)none_read.conflicts);
}

int main(void) {
  char path[] = "/tmp/tactix-one-search-XXXXXX";
  const int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);
  check(plain, "plain", path, 10, INT64_MAX, 0);
  check(parity, "parity", path, 10, INT64_MAX, 1);
  check(tseitin, "tseitin", path, 0, 0, 0); /* a startup refutation */
  unlink(path);
  return 0;
}
