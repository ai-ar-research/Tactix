/* Polynomial extended-resolution construction for PHP(n+1,n).
 * An external strategy: no kernel headers or built-in PHP refutation action.
 * Usage: tactix-php-er INPUT.cnf [PROOF] [--cook]
 */
#include "tactix.h"
#include "php_input.h"
#include "php_cubic.h"
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
  /* Query only this rule. Enumerating every rule here also runs expensive
   * unrelated guards once per consequence. A zero-capacity query tests the
   * guard without allocating or enumerating a payload. */
  if (tactix_rule_instances(p->solver, p->propagate, NULL, 0) < 0) return 0;
  const int result = tactix_apply(p->solver, p->propagate);
  return result == 20 ? 20 : result < 0 ? -1 : 0;
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

int main(int argc, char **argv) {
  if (argc < 2 || argc > 4) {
    fprintf(stderr, "usage: %s INPUT.cnf [PROOF] [--cook]\n", argv[0]); return 1;
  }
  int cook = 0;
  const char *proof = NULL;
  for (int i = 2; i < argc; ++i) {
    if (!strcmp(argv[i], "--cook") && !cook) cook = 1;
    else if (!proof && strcmp(argv[i], "--cook")) proof = argv[i];
    else { fprintf(stderr, "invalid arguments\n"); return 1; }
  }
  FILE *input = fopen(argv[1], "r");
  if (!input) { perror(argv[1]); return 1; }
  struct php_input *decoded = php_input_read(input);
  if (!decoded || fseek(input, 0, SEEK_SET)) {
    fclose(input);
    php_input_release(decoded);
    fprintf(stderr, "unsupported input: expected authenticated unary PHP with binary AMO implications\n");
    puts("s UNKNOWN"); return 1;
  }
  const int n = php_input_holes(decoded);
  int *matrix = php_input_matrix(decoded);
  if (!matrix) { fclose(input); php_input_release(decoded); return 1; }
  struct strategy p = {0};
  p.solver = tactix_init();
  p.propagate = find_rule("Propagate");
  if (proof && tactix_proof(p.solver, proof, TACTIX_PROOF_PBP)) {
    fclose(input); free(matrix); php_input_release(decoded); tactix_release(p.solver); return 1;
  }
  /* Preserve the original DIMACS header namespace and input proof image. */
  const int read_error = tactix_read_dimacs(p.solver, input);
  fclose(input);
  if (read_error) { free(matrix); php_input_release(decoded); tactix_release(p.solver); return 1; }
  int outcome = propagate(&p);
  uint64_t collisions = 0;
  /* Cubic replay checks the original compact AMO clauses directly. Only
   * the Cook comparison requires materialized pairwise collision clauses. */
  if (!outcome && cook) outcome = php_input_collisions(p.solver, decoded, &collisions);
  printf("c php-er normalized-collisions=%" PRIu64 "\n", collisions);
  php_input_release(decoded);
  if (!outcome && cook) outcome = reduce_php(&p, n, matrix);
  else {
    if (!outcome) {
      uint64_t additions = 0;
      tactix_clausal_effort effort = {0};
      outcome = php_cubic(p.solver, matrix, n, &additions, &effort);
      printf("c php-er construction=cubic additions=%" PRIu64 " validation-work=%" PRIu64
             " rat-resolvents=%" PRIu64 "\n", additions, effort.work, effort.rat_resolvents);
    }
    free(matrix);
  }
  printf("c php-er decisions=%" PRIu64 "\n", tactix_stat(p.solver, TACTIX_STAT_DECISIONS));
  printf("c php-er bve-eliminated=%" PRIu64 " substituted=%" PRIu64 "\n",
         tactix_stat(p.solver, TACTIX_STAT_BVE_ELIMINATED),
         tactix_stat(p.solver, TACTIX_STAT_SUBSTITUTE_VARS));
  puts(outcome == 20 ? "s UNSATISFIABLE" : "s UNKNOWN");
  tactix_release(p.solver);
  return outcome == 20 ? 20 : 1;
}
