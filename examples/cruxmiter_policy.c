/* Public-API strategy used by the paper's cruxmiter experiment.
 *
 * The opening alternates congruence detection and substitution for at most
 * four rounds, applies Backbone, then hands the reduced formula to the
 * built-in default strategy with the exact settings used by the artifact.
 */
#include "tactix.h"
#include "strategy/presets/cruxmiter.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_dimacs(tactix *solver, const char *path,
                       int *maximum_variable) {
  FILE *input = fopen(path, "r");
  char line[1 << 16];
  int open_clause = 0, saw_header = 0;
  if (!input) {
    fprintf(stderr, "tactix-cruxmiter-policy: cannot open '%s': %s\n", path,
            strerror(errno));
    return 0;
  }
  *maximum_variable = 0;
  while (fgets(line, sizeof line, input)) {
    char *cursor = line;
    while (isspace((unsigned char)*cursor)) ++cursor;
    if (!*cursor || *cursor == 'c') continue;
    if (*cursor == 'p') {
      char kind[8];
      int variables;
      unsigned long long clauses;
      if (sscanf(cursor, "p %7s %d %llu", kind, &variables, &clauses) != 3 ||
          strcmp(kind, "cnf") || variables < 0) {
        fprintf(stderr, "tactix-cruxmiter-policy: malformed DIMACS header\n");
        fclose(input);
        return 0;
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
        fprintf(stderr, "tactix-cruxmiter-policy: malformed DIMACS literal\n");
        fclose(input);
        return 0;
      }
      if (value) {
        const int variable = value < 0 ? -(int)value : (int)value;
        if (variable > *maximum_variable) *maximum_variable = variable;
        open_clause = 1;
      } else {
        open_clause = 0;
      }
      if (tactix_add_checked(solver, (int)value)) {
        fprintf(stderr, "tactix-cruxmiter-policy: rejected DIMACS input\n");
        fclose(input);
        return 0;
      }
      cursor = end;
    }
  }
  if (ferror(input) || !saw_header || open_clause) {
    fprintf(stderr, "tactix-cruxmiter-policy: incomplete DIMACS input\n");
    fclose(input);
    return 0;
  }
  fclose(input);
  return 1;
}

static int terminal(int result) { return result == 10 || result == 20; }

static void print_model(tactix *solver, int maximum_variable) {
  int variable;
  fputs("v", stdout);
  for (variable = 1; variable <= maximum_variable; ++variable) {
    const int value = tactix_value(solver, variable);
    printf(" %d", value > 0 ? variable : -variable);
    if (variable % 20 == 0 && variable != maximum_variable) fputs("\nv", stdout);
  }
  puts(" 0");
}

int main(int argc, char **argv) {
  const char *cnf, *proof = NULL;
  int maximum_variable = 0, result;
  tactix *solver;
  cruxmiter_program program;
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "usage: %s CNF [PROOF]\n", argv[0]);
    return 1;
  }
  cnf = argv[1];
  if (argc == 3) proof = argv[2];
  solver = tactix_init();
  if (!solver || !cruxmiter_configure(solver)) {
    tactix_release(solver);
    return 1;
  }
  if (proof && tactix_proof(solver, proof, TACTIX_PROOF_DSR_BINARY)) {
    fprintf(stderr, "tactix-cruxmiter-policy: cannot create proof '%s'\n", proof);
    tactix_release(solver);
    return 1;
  }
  if (!read_dimacs(solver, cnf, &maximum_variable) ||
      tactix_set_guidance_publication(solver, TACTIX_GUIDANCE_NATIVE) ||
      !cruxmiter_begin(&program)) {
    tactix_release(solver);
    return 1;
  }
  puts("c cruxmiter-policy paper-strategy");
  result = cruxmiter_run(&program, solver, NULL, NULL, stdout);
  if (result == CRUXMITER_HANDOFF) result = tactix_solve(solver);
  printf("c conflicts %" PRIu64 "\n",
         tactix_stat(solver, TACTIX_STAT_CONFLICTS));
  printf("c crux-rules congruence=%" PRIu64 " substitute=%" PRIu64
         " backbone=%" PRIu64 "\n",
         tactix_stat(solver, TACTIX_STAT_CONGRUENCE_MERGES),
         tactix_stat(solver, TACTIX_STAT_SUBSTITUTE_VARS),
         tactix_stat(solver, TACTIX_STAT_BACKBONE_FAILED));
  if (result == 10) {
    puts("s SATISFIABLE");
    print_model(solver, maximum_variable);
  } else if (result == 20) {
    puts("s UNSATISFIABLE");
  } else {
    puts("s UNKNOWN");
  }
  tactix_release(solver);
  return terminal(result) ? result : 0;
}
