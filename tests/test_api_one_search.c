/* N2: a session's search does not depend on its proof. No proof and every format,
 * attached before or after the input, take the same conflicts, decisions and
 * propagations and give the same answer. */
#undef NDEBUG
#include "api_test_support.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
      tactix_add(s, mask >> i & 1 ? -vars[i] : vars[i]);
    tactix_add(s, 0);
  }
}

static void add_random_clauses(tactix *s, int nvars, int count) {
  for (int i = 0; i < count; ++i) {
    for (int k = 0; k < 3; ++k) {
      const int v = 1 + (int)below((unsigned)nvars);
      tactix_add(s, below(2) ? v : -v);
    }
    tactix_add(s, 0);
  }
}

/* Plain CDCL. */
static void plain(tactix *s) {
  state = 11;
  add_random_clauses(s, 140, 600);
}

/* Consistent 4-XORs under random clauses: GF(2) reasons during the search. */
static void parity(tactix *s) {
  state = 2;
  uint8_t solution[81];
  for (int v = 1; v <= 80; ++v)
    solution[v] = (uint8_t)below(2);
  for (int i = 0; i < 40; ++i) {
    int vars[4], rhs = 0;
    for (int k = 0; k < 4; ++k) {
      vars[k] = 1 + (int)below(80);
      for (int j = 0; j < k; ++j)
        if (vars[j] == vars[k]) {
          vars[k] = 1 + (int)below(80);
          j = -1;
        }
      rhs ^= solution[vars[k]];
    }
    add_xor(s, vars, 4, rhs);
  }
  add_random_clauses(s, 80, 240);
}

/* Tseitin on K4 with odd charge: a startup refutation. */
static void tseitin(tactix *s) {
  add_xor(s, (const int[]){1, 2, 3}, 3, 1);
  add_xor(s, (const int[]){1, 4, 5}, 3, 0);
  add_xor(s, (const int[]){2, 4, 6}, 3, 0);
  add_xor(s, (const int[]){3, 5, 6}, 3, 0);
}

struct run {
  int answer;
  int64_t conflicts, decisions, propagations;
};

static struct run solve(void (*formula)(tactix *), int format, int late,
                        const char *path) {
  tactix *s = tactix_init();
  if (format >= 0 && !late)
    assert(!tactix_proof(s, path, format));
  formula(s);
  if (format >= 0 && late)
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
  return r;
}

static void check(void (*formula)(tactix *), const char *name, const char *path,
                  int64_t least_conflicts) {
  const struct run none = solve(formula, -1, 0, path);
  assert(none.answer == 10 || none.answer == 20);
  assert(none.conflicts >= least_conflicts);
  static const int formats[] = {TACTIX_PROOF_PBP, TACTIX_PROOF_AUTO,
                                TACTIX_PROOF_DSR_BINARY, TACTIX_PROOF_DSR_ASCII};
  for (size_t f = 0; f < sizeof formats / sizeof *formats; ++f)
    for (int late = 0; late < 2; ++late) {
      const struct run r = solve(formula, formats[f], late, path);
      if (r.answer != none.answer || r.conflicts != none.conflicts ||
          r.decisions != none.decisions || r.propagations != none.propagations) {
        fprintf(stderr,
                "%s format %d late %d: answer %d conflicts %lld decisions %lld "
                "propagations %lld; no proof: %d %lld %lld %lld\n",
                name, formats[f], late, r.answer, (long long)r.conflicts,
                (long long)r.decisions, (long long)r.propagations, none.answer,
                (long long)none.conflicts, (long long)none.decisions,
                (long long)none.propagations);
        abort();
      }
    }
  printf("%s: answer %d, %lld conflicts in every mode\n", name, none.answer,
         (long long)none.conflicts);
}

int main(void) {
  char path[] = "/tmp/tactix-one-search-XXXXXX";
  const int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);
  check(plain, "plain", path, 10);
  check(parity, "parity", path, 10);
  check(tseitin, "tseitin", path, 0);
  unlink(path);
  return 0;
}
