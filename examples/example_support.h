/* Helpers the example strategies share: clause input, the search demos'
 * driver, a line-based DIMACS reader and the competition model printer. */
#ifndef TACTIX_EXAMPLE_SUPPORT_H_INCLUDED
#define TACTIX_EXAMPLE_SUPPORT_H_INCLUDED

#include "tactix.h"
#include "tactix_search_demo.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* EXAMPLE_ADD(s, 1, 2, 0, -1, 0): each listed literal and clause end, in order. */
static inline void example_add(tactix *s, const int *lits, size_t n) {
  for (size_t i = 0; i < n; ++i) tactix_add(s, lits[i]);
}
#define EXAMPLE_ADD(s, ...) \
  example_add((s), (const int[]){__VA_ARGS__}, sizeof((const int[]){__VA_ARGS__}) / sizeof(int))

/* A search demo under its toy-only nontermination guard. */
static inline int example_search_demo(tactix *s, int kind) {
  tactix_search_demo program;
  const int begun = tactix_search_demo_begin(&program, kind);
  assert(begun);
  (void)begun;
  program.step_limit = 2000000;
  return tactix_search_demo_run(&program, s, NULL, NULL);
}

/* Reads DIMACS from `path` into `solver`, `line_size` bytes per fgets, and
 * sets *maximum_variable to the largest header or literal variable. `record`
 * (optional) sees each accepted literal and clause end; 0 rejects the input.
 * Errors go to stderr as "program: message", every one after opening as
 * `uniform` when it is not NULL. 1 on success. */
static inline int example_read_dimacs(tactix *solver, const char *path, const char *program,
                                      const char *uniform, int line_size, int *maximum_variable,
                                      int (*record)(void *, int), void *context) {
  static char line[1 << 20];
  FILE *input = fopen(path, "r");
  const char *error = NULL;
  int open_clause = 0, saw_header = 0;
  if (!input) {
    fprintf(stderr, "%s: cannot open '%s': %s\n", program, path, strerror(errno));
    return 0;
  }
  *maximum_variable = 0;
  while (!error && fgets(line, line_size, input)) {
    char *cursor = line;
    while (isspace((unsigned char)*cursor)) ++cursor;
    if (!*cursor || *cursor == 'c') continue;
    if (*cursor == 'p') {
      char kind[8];
      int variables;
      unsigned long long clauses;
      if (sscanf(cursor, "p %7s %d %llu", kind, &variables, &clauses) != 3 ||
          strcmp(kind, "cnf") || variables < 0) {
        error = "malformed DIMACS header";
        break;
      }
      saw_header = 1;
      if (variables > *maximum_variable) *maximum_variable = variables;
      continue;
    }
    while (*cursor) {
      char *end;
      long value;
      while (isspace((unsigned char)*cursor)) ++cursor;
      if (!*cursor || *cursor == 'c') break;
      errno = 0;
      value = strtol(cursor, &end, 10);
      if (end == cursor || errno || value < INT_MIN || value > INT_MAX) {
        error = "malformed DIMACS literal";
        break;
      }
      if (tactix_add_checked(solver, (int)value) || (record && !record(context, (int)value))) {
        error = "rejected DIMACS input";
        break;
      }
      if (value) {
        const int variable = value < 0 ? -(int)value : (int)value;
        if (variable > *maximum_variable) *maximum_variable = variable;
      }
      open_clause = value != 0;
      cursor = end;
    }
  }
  if (!error && (ferror(input) || !saw_header || open_clause)) error = "incomplete DIMACS input";
  if (error) fprintf(stderr, "%s: %s\n", program, uniform ? uniform : error);
  fclose(input);
  return !error;
}

/* The verdict line, a model on `v` lines of twenty literals after SAT. */
static inline void example_print_result(tactix *solver, int result, int maximum_variable) {
  int variable;
  if (result != 10) {
    puts(result == 20 ? "s UNSATISFIABLE" : "s UNKNOWN");
    return;
  }
  puts("s SATISFIABLE");
  fputs("v", stdout);
  for (variable = 1; variable <= maximum_variable; ++variable) {
    const int value = tactix_value(solver, variable);
    printf(" %d", value > 0 ? variable : -variable);
    if (variable % 20 == 0 && variable != maximum_variable) fputs("\nv", stdout);
  }
  puts(" 0");
}

#endif
