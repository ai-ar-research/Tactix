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
  int propagate;
  uint64_t definitions, consequences;
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

static int consequence(struct strategy *p, const int *clause, int count,
                       const tactix_rup_premise *premises, int premise_count) {
  const int result = tactix_add_rup(p->solver, clause, count, premises,
                                   premise_count, UINT64_MAX);
  if (result < 0) return -1;
  ++p->consequences;
  return result == 20 ? 20 : propagate(p);
}

static int reduce_php(struct strategy *p, int holes, int *matrix) {
  int outcome = 0;
  /* No elimination, substitution, sweep or default preprocessing is scheduled.
   * Extension variables and their defining clauses remain available throughout
   * the derivation. The algorithm has no branching or general SAT search.
   */
  for (int h = holes; h >= 2 && !outcome; --h) {
    int *next = malloc((size_t)h * (h - 1) * sizeof *next);
    int *terms = malloc((size_t)h * (h - 1) * sizeof *terms);
    if (!next || !terms) { free(next); free(terms); outcome = -1; break; }
    for (int i = 0; i < h && !outcome; ++i)
      for (int j = 0; j < h - 1; ++j) {
        const int term = tactix_define_extension(p->solver, TACTIX_EXTENSION_AND,
                                                 matrix[i*h+h-1], matrix[h*h+j]);
        if (term < 0) { outcome = -1; break; }
        const int q = tactix_define_extension(p->solver, TACTIX_EXTENSION_OR,
                                              matrix[i*h+j], term);
        if (q < 0) { outcome = -1; break; }
        next[i*(h-1)+j] = q;
        terms[i*(h-1)+j] = term;
        p->definitions += 2;
      }
    /* For each hole, split a new collision on the last pigeon's old literal.
     * Both ternaries are RUP; together they make the binary collision RUP.
     */
    for (int j = 0; j < h - 1 && !outcome; ++j)
      for (int i = 0; i < h && !outcome; ++i)
        for (int k = i + 1; k < h && !outcome; ++k) {
          const int a = next[i*(h-1)+j], b = next[k*(h-1)+j];
          const int ti = terms[i*(h-1)+j], tk = terms[k*(h-1)+j];
          const int x = matrix[i*h+j], y = matrix[k*h+j];
          const int u = matrix[i*h+h-1], v = matrix[k*h+h-1];
          const int z = matrix[h*h+j];
          int positive[] = {-a, -b, z}, negative[] = {-a, -b, -z};
          int rows[][3] = {{-ti,z}, {-tk,z}, {-a,x,ti}, {-b,y,tk}, {-x,-y},
                          {-x,-z}, {-y,-z}, {-ti,u}, {-tk,v}, {-u,-v}};
          tactix_rup_premise first[] = {{rows[0],2},{rows[1],2},
            {rows[2],3},{rows[3],3},{rows[4],2}};
          tactix_rup_premise second[] = {{rows[5],2},{rows[6],2},
            {rows[2],3},{rows[3],3},{rows[7],2},{rows[8],2},{rows[9],2}};
          tactix_rup_premise final[] = {{positive,3},{negative,3}};
          outcome = consequence(p, positive, 3, first, 5);
          if (!outcome) outcome = consequence(p, negative, 3, second, 7);
          if (!outcome) outcome = consequence(p, positive, 2, final, 2);
        }
    for (int i = 0; i < h && !outcome; ++i) {
      const int count = 3*(h-1)+3;
      tactix_rup_premise *premises = malloc((size_t)count * sizeof *premises);
      int (*rows)[3] = malloc((size_t)(count-2) * sizeof *rows);
      if (!premises || !rows) { free(premises); free(rows); outcome = -1; break; }
      premises[0] = (tactix_rup_premise){matrix+i*h,h};
      premises[1] = (tactix_rup_premise){matrix+h*h,h};
      rows[0][0] = -matrix[i*h+h-1]; rows[0][1] = -matrix[h*h+h-1];
      premises[2] = (tactix_rup_premise){rows[0],2};
      for (int j = 0; j < h-1; ++j) {
        const int q = next[i*(h-1)+j], t = terms[i*(h-1)+j];
        const int r = 1+3*j;
        rows[r][0] = q; rows[r][1] = -matrix[i*h+j];
        rows[r+1][0] = q; rows[r+1][1] = -t;
        rows[r+2][0] = t; rows[r+2][1] = -matrix[i*h+h-1];
        rows[r+2][2] = -matrix[h*h+j];
        for (int k = 0; k < 3; ++k)
          premises[3+3*j+k] = (tactix_rup_premise){rows[r+k],k == 2 ? 3 : 2};
      }
      outcome = consequence(p, next+i*(h-1), h-1, premises, count);
      free(rows); free(premises);
    }
    free(terms);
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
