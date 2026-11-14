/* Public-API policy for symbreak's Chvatal-conjecture SAT subproblems.
 *
 * Independent totalizers count the same literals in the same order, so their
 * auxiliary counters occur in structurally identical copies that clause
 * learning does not relate. The policy merges such twins before handing the
 * smaller formula to DefaultPolicy:
 *
 *  - two variables are twins when the clauses of their negative literals are
 *    identical after removing that literal and mapping merged variables to
 *    their representative; repeating the test bottom-up finds twins of twins;
 *  - for a twin b of representative a, SubstitutionRedundancy adds (a | -b)
 *    with witness {a} and (b | -a) with witness {b}, validating each instance
 *    and recording the witness in the DSR certificate;
 *  - Substitute collapses the equivalences, and tactix_solve runs
 *    DefaultPolicy on the merged formula.
 */
#include "example_support.h"
#include "tactix_chvatal.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

/* The input's clauses as chvatal_find_twins reads them. */
struct formula_reader {
  struct chvatal_formula *f;
  size_t literal_capacity, start_capacity;
};

static int start_clauses(struct formula_reader *r) {
  if (r->f->starts) return 1;
  if (!chvatal_grow((void **)&r->f->starts, &r->start_capacity, 0, sizeof(size_t))) return 0;
  r->f->starts[0] = 0;
  return 1;
}

static int record_literal(void *context, int literal) {
  struct formula_reader *r = context;
  struct chvatal_formula *f = r->f;
  if (literal) {
    if (!chvatal_grow((void **)&f->literals, &r->literal_capacity, f->literal_count, sizeof(int)))
      return 0;
    f->literals[f->literal_count++] = literal;
    return 1;
  }
  if (!start_clauses(r) ||
      !chvatal_grow((void **)&f->starts, &r->start_capacity, f->clauses + 1, sizeof(size_t)))
    return 0;
  f->starts[++f->clauses] = f->literal_count;
  return 1;
}

static int read_dimacs(tactix *solver, const char *path, struct chvatal_formula *f) {
  static const char program[] = "tactix-chvatal-policy";
  static const char malformed[] = "malformed or rejected DIMACS";
  struct formula_reader reader = {f, 0, 0};
  memset(f, 0, sizeof *f);
  if (!example_read_dimacs(solver, path, program, malformed, 1 << 20, &f->variables, record_literal,
                           &reader))
    return 0;
  if (start_clauses(&reader)) return 1;
  fprintf(stderr, "%s: %s\n", program, malformed);
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
  printf("c conflicts %" PRIu64 "\n", tactix_stat(solver, TACTIX_STAT_CONFLICTS));
  example_print_result(solver, result, f.variables);
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
