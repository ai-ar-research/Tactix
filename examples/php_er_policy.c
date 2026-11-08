/* Polynomial extended-resolution construction for PHP(n+1,n).
 * An external strategy: no kernel headers or built-in PHP refutation action.
 * Usage: tactix-php-er INPUT.cnf [PROOF]
 */
#include "tactix.h"
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct strategy {
  tactix *solver;
  int sr, propagate;
  uint64_t revision, definitions, consequences;
};

static int find_rule(const char *name) {
  for (int i = 0; i < tactix_rule_count(); ++i) {
    const char *a = name, *b = tactix_rule_name(i);
    while (*a && *a == *b) ++a, ++b;
    if (!*a && !*b) return i;
  }
  return -1;
}

static int propagate(struct strategy *p) {
  const int status = tactix_status(p->solver);
  if (status) return status == 20 ? 20 : -1;
  int applicable[64];
  const int count = tactix_rules_applicable(p->solver, applicable, 64);
  for (int i = 0; i < count; ++i)
    if (applicable[i] == p->propagate) {
      const int result = tactix_apply(p->solver, p->propagate);
      return result == 20 ? 20 : result < 0 ? -1 : 0;
    }
  return 0;
}

static int consequence(struct strategy *p, const int *clause, int count) {
  /* RUP consequences are also valid SR instances with the default pivot
   * witness. The existing public validator checks them against the live CNF.
   * This first implementation deliberately reuses that exact public surface.
   */
  tactix_online_sr_instance instance = {clause, count, NULL, 0, NULL, 0};
  tactix_online_step_v2 step = {0};
  step.base.step_id = 1;
  step.base.rule_id = p->sr;
  step.base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  step.base.budget_unlimited = 1;
  step.base.parameters.kind = TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY;
  step.base.parameters.value.substitution_redundancy.maximum_validation_ticks = UINT64_MAX;
  step.sr_instances = &instance;
  step.sr_instance_count = 1;
  tactix_online_program_v2 program = {&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  if (tactix_online_configure_v2(p->solver, &program, ++p->revision,
                               TACTIX_ONLINE_RECONFIGURE_RESET)) return -1;
  tactix_online_result result = {0};
  if (tactix_online_advance(p->solver, &result)) return -1;
  ++p->consequences;
  if (result.rule_result == 20 || result.status == TACTIX_ONLINE_UNSAT) return 20;
  if (!result.made_rule_progress && result.status != TACTIX_ONLINE_NO_CHANGE)
    return -1;
  return propagate(p);
}

static int reduce_php(struct strategy *p, int holes, int *matrix) {
  int outcome = 0;
  /* No elimination, substitution, sweep or default preprocessing is scheduled.
   * Extension variables and their defining clauses remain available throughout
   * the derivation. The algorithm has no branching or general SAT search.
   */
  for (int h = holes; h >= 2 && !outcome; --h) {
    int *next = malloc((size_t)h * (h - 1) * sizeof *next);
    if (!next) { outcome = -1; break; }
    for (int i = 0; i < h && !outcome; ++i)
      for (int j = 0; j < h - 1; ++j) {
        const int term = tactix_define_extension(p->solver, TACTIX_EXTENSION_AND,
                                                 matrix[i*h+h-1], matrix[h*h+j]);
        if (term < 0) { outcome = -1; break; }
        const int q = tactix_define_extension(p->solver, TACTIX_EXTENSION_OR,
                                              matrix[i*h+j], term);
        if (q < 0) { outcome = -1; break; }
        next[i*(h-1)+j] = q;
        p->definitions += 2;
      }
    /* For each hole, split a new collision on the last pigeon's old literal.
     * Both ternaries are RUP; together they make the binary collision RUP.
     */
    for (int j = 0; j < h - 1 && !outcome; ++j)
      for (int i = 0; i < h && !outcome; ++i)
        for (int k = i + 1; k < h && !outcome; ++k) {
          const int a = next[i*(h-1)+j], b = next[k*(h-1)+j];
          int clause[] = {-a, -b, matrix[h*h+j]};
          outcome = consequence(p, clause, 3);
          if (!outcome) { clause[2] = -clause[2]; outcome = consequence(p, clause, 3); }
          if (!outcome) outcome = consequence(p, clause, 2);
        }
    for (int i = 0; i < h && !outcome; ++i)
      outcome = consequence(p, next + i*(h-1), h - 1);
    printf("c php-er level=%d definitions=%" PRIu64 " consequences=%" PRIu64 "\n",
           h, p->definitions, p->consequences);
    free(matrix);
    matrix = next;
  }
  free(matrix);
  return outcome;
}

/* The family format is the existing generator's canonical row-major PHP
 * encoding. Validate every token, not just a header or filename. Unsupported
 * permutations/encodings are rejected; they are never assumed to be PHP.
 */
static int token(FILE *in, char word[64]) {
  for (;;) {
    const int result = fscanf(in, "%63s", word);
    if (result != 1) return 0;
    if (strcmp(word, "c")) return 1;
    int c; do { c = fgetc(in); } while (c != EOF && c != '\n');
  }
}
static int number(FILE *in, int64_t *out) {
  char word[64], *end;
  if (!token(in, word)) return 0;
  errno = 0;
  const long long value = strtoll(word, &end, 10);
  if (errno || !*word || *end) return 0;
  *out = value;
  return 1;
}
static int expect(FILE *in, int64_t wanted) {
  int64_t got;
  return number(in, &got) && got == wanted;
}
static int php_dimension(FILE *in) {
  char word[64];
  int64_t variables, clauses;
  if (!token(in, word) || strcmp(word, "p") ||
      !token(in, word) || strcmp(word, "cnf") ||
      !number(in, &variables) || !number(in, &clauses) ||
      variables < 6 || variables > INT_MAX) return 0;
  int n = 2;
  while ((int64_t)n*(n+1) < variables) ++n;
  if ((int64_t)n*(n+1) != variables ||
      clauses != n+1+(int64_t)n*n*(n+1)/2 ||
      variables + 2*(int64_t)n*(n+1)*(n-1)/3 > INT_MAX) return 0;
  for (int i = 0; i <= n; ++i) {
    for (int j = 0; j < n; ++j)
      if (!expect(in, i*n+j+1)) return 0;
    if (!expect(in, 0)) return 0;
  }
  for (int j = 0; j < n; ++j)
    for (int i = 0; i <= n; ++i)
      for (int k = i+1; k <= n; ++k)
        if (!expect(in, -(i*n+j+1)) || !expect(in, -(k*n+j+1)) ||
            !expect(in, 0)) return 0;
  return token(in, word) || ferror(in) ? 0 : n;
}

int main(int argc, char **argv) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "usage: %s INPUT.cnf [PROOF]\n", argv[0]); return 1;
  }
  FILE *input = fopen(argv[1], "r");
  if (!input) { perror(argv[1]); return 1; }
  const int n = php_dimension(input);
  if (!n || fseek(input, 0, SEEK_SET)) {
    fprintf(stderr, "unsupported input: expected canonical PHP(n+1,n)\n");
    fclose(input); puts("s UNKNOWN"); return 1;
  }
  const size_t variables = (size_t)n*(n+1);
  int *matrix = malloc(variables * sizeof *matrix);
  if (!matrix) { fclose(input); return 1; }
  struct strategy p = {0};
  p.solver = tactix_init();
  p.sr = find_rule("SubstitutionRedundancy");
  p.propagate = find_rule("Propagate");
  if ((argc == 3 && tactix_proof(p.solver, argv[2], TACTIX_PROOF_PBP)) ||
      tactix_read_dimacs(p.solver, input)) {
    fclose(input); free(matrix); tactix_release(p.solver); return 1;
  }
  fclose(input);
  for (size_t i = 0; i < variables; ++i) matrix[i] = (int)i+1;
  int outcome = propagate(&p);
  if (!outcome) outcome = reduce_php(&p, n, matrix);
  else free(matrix);
  printf("c php-er decisions=%" PRIu64 "\n", tactix_stat(p.solver, TACTIX_STAT_DECISIONS));
  printf("c php-er bve-eliminated=%" PRIu64 " substituted=%" PRIu64 "\n",
         tactix_stat(p.solver, TACTIX_STAT_BVE_ELIMINATED),
         tactix_stat(p.solver, TACTIX_STAT_SUBSTITUTE_VARS));
  puts(outcome == 20 ? "s UNSATISFIABLE" : "s UNKNOWN");
  tactix_release(p.solver);
  return outcome == 20 ? 20 : 1;
}
