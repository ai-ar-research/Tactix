/* Public-API policy for symbreak's Chvatal-conjecture SAT subproblems.
 *
 * The benchmark family uses independent totalizers that count the same
 * literals in the same order, so auxiliary counter variables occur in
 * structurally identical copies. Clause learning about one copy does not
 * automatically transfer to the others.
 *
 * The policy finds such "twins" generically and merges them before handing
 * the smaller formula to the shipped DefaultPolicy:
 *
 *  - two variables are twins when the clauses containing their negative
 *    literals are identical after removing that literal and mapping already
 *    merged variables to their representative; repeating the test bottom-up
 *    finds the twins of twins;
 *  - for a twin b of representative a, the policy adds (a | -b) with witness
 *    {a} and (b | -a) with witness {b} through the public
 *    SubstitutionRedundancy rule, which validates every instance itself and
 *    records the witness in the DSR certificate;
 *  - Substitute then collapses the resulting equivalences, and tactix_solve
 *    runs DefaultPolicy on the merged formula.
 */
#include "tactix.h"
#include "tactix_chvatal.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_dimacs(tactix *solver, const char *path, struct chvatal_formula *f) {
  FILE *input = fopen(path, "r");
  static char line[1 << 20];
  size_t literal_capacity = 0, start_capacity = 0;
  int open_clause = 0, saw_header = 0;
  if (!input) {
    fprintf(stderr, "tactix-chvatal-policy: cannot open '%s': %s\n", path,
            strerror(errno));
    return 0;
  }
  memset(f, 0, sizeof *f);
  if (!chvatal_grow((void **)&f->starts, &start_capacity, 0, sizeof(size_t)))
    goto fail;
  f->starts[0] = 0;
  while (fgets(line, sizeof line, input)) {
    char *cursor = line;
    while (isspace((unsigned char)*cursor)) ++cursor;
    if (!*cursor || *cursor == 'c') continue;
    if (*cursor == 'p') {
      char kind[8];
      int variables;
      unsigned long long clauses;
      if (sscanf(cursor, "p %7s %d %llu", kind, &variables, &clauses) != 3 ||
          strcmp(kind, "cnf") || variables < 0)
        goto fail;
      saw_header = 1;
      if (variables > f->variables) f->variables = variables;
      continue;
    }
    while (*cursor) {
      char *end;
      long value;
      while (isspace((unsigned char)*cursor)) ++cursor;
      if (!*cursor || *cursor == 'c') break;
      errno = 0;
      value = strtol(cursor, &end, 10);
      if (end == cursor || errno || value < INT_MIN || value > INT_MAX)
        goto fail;
      if (tactix_add_checked(solver, (int)value)) goto fail;
      if (value) {
        const int variable = value < 0 ? -(int)value : (int)value;
        if (variable > f->variables) f->variables = variable;
        if (!chvatal_grow((void **)&f->literals, &literal_capacity, f->literal_count,
                  sizeof(int)))
          goto fail;
        f->literals[f->literal_count++] = (int)value;
        open_clause = 1;
      } else {
        if (!chvatal_grow((void **)&f->starts, &start_capacity, f->clauses + 1,
                  sizeof(size_t)))
          goto fail;
        f->starts[++f->clauses] = f->literal_count;
        open_clause = 0;
      }
      cursor = end;
    }
  }
  if (ferror(input) || !saw_header || open_clause) goto fail;
  fclose(input);
  return 1;
fail:
  fprintf(stderr, "tactix-chvatal-policy: malformed or rejected DIMACS\n");
  fclose(input);
  return 0;
}

/* Classifies a finished binary DSR certificate the way the checker routing
 * does: an addition (tag 1) whose first literal reappears later on its line
 * carries a substitution witness and needs the witnessed route. */
static int dsr_has_witness(const char *path) {
  FILE *in = fopen(path, "rb");
  int c, witnessed = 0;
  if (!in) return -1;
  while (!witnessed && (c = fgetc(in)) != EOF) {
    const int addition = c == 1;
    uint64_t first = 0;
    int have_first = 0;
    if (c != 1 && c != 2) { witnessed = -1; break; }
    for (;;) {
      uint64_t value = 0;
      int shift = 0, byte;
      do {
        byte = fgetc(in);
        if (byte == EOF) { fclose(in); return witnessed; }
        value |= (uint64_t)(byte & 0x7f) << shift;
        shift += 7;
      } while (byte & 0x80);
      if (!value) break;
      if (!have_first) { first = value; have_first = 1; }
      else if (addition && value == first) witnessed = 1;
    }
  }
  fclose(in);
  return witnessed;
}

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
  int result = 0;
  struct chvatal_formula f;
  tactix *solver;
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "usage: tactix-chvatal-policy CNF [PROOF]\n");
    return 1;
  }
  cnf = argv[1];
  if (argc == 3) proof = argv[2];
  solver = tactix_init();
  if (proof && tactix_proof(solver, proof, TACTIX_PROOF_DSR_BINARY)) {
    fprintf(stderr, "tactix-chvatal-policy: cannot create proof '%s'\n", proof);
    return 1;
  }
  if (!read_dimacs(solver, cnf, &f) ||
      tactix_set_guidance_publication(solver, TACTIX_GUIDANCE_NATIVE)) {
    tactix_release(solver);
    return 1;
  }
  chvatal_program program = {0};
  if (tactix_status(solver) == 20) result = 20;
  if (!result) {
    if (!chvatal_begin(&program, &f, NULL, NULL)) {
      tactix_release(solver);
      free(f.literals);
      free(f.starts);
      return 1;
    }
    result = chvatal_run(&program, solver, NULL, NULL, stdout);
    if (result == CHVATAL_HANDOFF) result = 0;
  }
  chvatal_release(&program);
  if (!result) result = tactix_solve(solver);

  printf("c conflicts %" PRIu64 "\n",
         tactix_stat(solver, TACTIX_STAT_CONFLICTS));
  if (result == 10) {
    puts("s SATISFIABLE");
    print_model(solver, f.variables);
  } else if (result == 20) {
    puts("s UNSATISFIABLE");
  } else {
    puts("s UNKNOWN");
  }
  tactix_release(solver);
  /* Report the checker route from the certificate's own content, after the
   * session has flushed and closed it. */
  if (result == 20 && proof) {
    const int has = dsr_has_witness(proof);
    if (has >= 0) printf("c proof-route %s\n", has ? "dsr-trestle" : "dsr-cake_lpr");
  }
  fflush(stdout);
  free(f.literals);
  free(f.starts);
  return result == 10 || result == 20 ? result : 0;
}
