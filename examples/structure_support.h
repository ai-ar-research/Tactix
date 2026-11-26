/* Shared by the structure strategies (hall, tseitin, vertex cover): the input
 * read into memory first, so the proof route can follow recognition, and a
 * session that otherwise behaves like the `tactix` command line. */
#ifndef TACTIX_STRUCTURE_SUPPORT_H_INCLUDED
#define TACTIX_STRUCTURE_SUPPORT_H_INCLUDED

#include "tactix.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void *xalloc(size_t count, size_t size) {
  void *p = calloc(count ? count : 1, size);
  if (!p) {
    fputs("out of memory\n", stderr);
    exit(1);
  }
  return p;
}

/* A growable int array. */
struct ints {
  int *at;
  size_t size, capacity;
};

static inline void push(struct ints *v, int value) {
  if (v->size == v->capacity) {
    v->capacity = v->capacity ? 2 * v->capacity : 1024;
    v->at = realloc(v->at, v->capacity * sizeof(int));
    if (!v->at) exit(1);
  }
  v->at[v->size++] = value;
}

/* Clause i is lits.at[start[i] .. start[i+1]-1], ending in 0. */
struct cnf {
  int variables;
  struct ints lits;
  size_t *start, clauses;
};

static inline int *clause(const struct cnf *f, size_t i) { return f->lits.at + f->start[i]; }
static inline int width(const struct cnf *f, size_t i) {
  return (int)(f->start[i + 1] - f->start[i] - 1);
}

static inline int read_cnf(const char *path, struct cnf *f) {
  FILE *in = fopen(path, "r");
  int c, header = 0;
  size_t i, at = 0;
  memset(f, 0, sizeof *f);
  if (!in) return 0;
  while ((c = fgetc(in)) != EOF) {
    long value;
    if (c == 'c' || c == 'p') {
      header |= c == 'p' && fscanf(in, " cnf %d %*d", &f->variables) == 1;
      while (c != '\n' && c != EOF) c = fgetc(in);
    } else if (c == '-' || (c >= '0' && c <= '9')) {
      ungetc(c, in);
      if (fscanf(in, "%ld", &value) != 1) break;
      push(&f->lits, (int)value);
      if (labs(value) > f->variables) f->variables = (int)labs(value);
    }
  }
  fclose(in);
  for (i = 0; i < f->lits.size; ++i) f->clauses += !f->lits.at[i];
  f->start = xalloc(f->clauses + 1, sizeof(size_t));
  for (i = 0; i < f->lits.size; ++i)
    if (!f->lits.at[i]) f->start[++at] = i + 1;
  return header && (!f->lits.size || !f->lits.at[f->lits.size - 1]);
}

/* A session with the command-line defaults and the input added. A recognized
 * structure's certificate is pseudo-Boolean (whole-run PBP); otherwise the
 * proof route is automatic, as in `tactix`. */
static inline tactix *structure_session(const struct cnf *f, const char *proof, int recognized) {
  static const char *const settings[][2] = {
      {"TACTIX_FACTOR_FX", "1"},
      {"TACTIX_CONFLICT_ER", "auto"},
      {"TACTIX_STARTUP_MATCHING_COUNT", "1"},
      {"TACTIX_STARTUP_AFFINE_QUOTIENT", "1"},
      {"TACTIX_PARITY_REFUTATION_MAXIMUM_ARITY", "8"},
  };
  tactix *s = tactix_init();
  size_t i;
  for (i = 0; i < sizeof settings / sizeof *settings; ++i)
    if (!getenv(settings[i][0])) tactix_set_default_option(s, settings[i][0], settings[i][1]);
  tactix_defer_startup_state(s);
  tactix_set_bve_gates(s, TACTIX_BVE_GATES_DEFAULT);
  if (tactix_discard_structural_provenance(s) ||
      (proof && tactix_proof(s, proof, recognized ? TACTIX_PROOF_PBP : TACTIX_PROOF_AUTO))) {
    fprintf(stderr, "cannot create proof '%s'\n", proof);
    exit(1);
  }
  for (i = 0; i < f->lits.size; ++i)
    if (tactix_add_checked(s, f->lits.at[i])) exit(1);
  return s;
}

/* Solves unless `result` is already a verdict, prints it, and releases s. */
static inline int structure_finish(tactix *s, int result, int variables, int proof) {
  int v;
  if (result != 10 && result != 20) result = tactix_solve(s);
  if (result == 10 || result == 20) result = tactix_status(s);
  printf("c conflicts %" PRIu64 "\n", tactix_stat(s, TACTIX_STAT_CONFLICTS));
  if (proof && result == 20) printf("c proof-route %s\n", tactix_proof_route(s));
  puts(result == 10 ? "s SATISFIABLE" : result == 20 ? "s UNSATISFIABLE" : "s UNKNOWN");
  if (result == 10) {
    for (v = 1; v <= variables; ++v)
      printf("%s %d%s", v % 20 == 1 ? "v" : "", tactix_value(s, v) > 0 ? v : -v,
             v % 20 == 0 ? "\n" : "");
    puts(variables % 20 ? " 0" : "v 0");
  }
  fflush(stdout);
  tactix_release(s);
  return result == 10 || result == 20 ? result : 0;
}

#endif
