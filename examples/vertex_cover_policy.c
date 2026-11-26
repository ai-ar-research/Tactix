/* Public-API strategy for vertex-cover cardinality formulas over clique-rich
 * graphs, falling back to the command-line default strategy.
 *
 * The TS family's "mis" variants ask whether a graph has a vertex cover of at
 * most K vertices. Each edge (a, b) is a clause (x_a | x_b), and a sequential
 * (Sinz) counter over x_1..x_n forbids more than K true vertices. The graph
 * is the primal graph of a parity formula, so every XOR constraint's scope is
 * a clique, and K asks for one vertex fewer than any cover has. A clique holds
 * at most one uncovered vertex. Cliques covering every vertex W times
 * therefore leave at most sum(weights) / W vertices uncovered, fewer than the
 * n - K the counter demands. Resolution must rediscover that count, while the
 * counter only allows search to propagate it.
 *
 * The strategy reconstructs this structure from the clauses alone:
 *
 *  - vertices and edges: the variables and clauses of all-positive binaries;
 *  - the counter: each vertex's first register (~x | s), the register chain
 *    (~s_{i-1,j} | s_{i,j}) ordering the vertices, and the carries
 *    (~x_i | ~s_{i-1,j-1} | s_{i,j}) giving the rows and the bound K;
 *  - cliques: the maximal cliques of the counted vertices (Bron-Kerbosch);
 *  - weights: an optimal fractional clique cover from an exact-enough
 *    simplex, scaled to the smallest integer coverage W that keeps it optimal.
 *
 * When the weighted cover is smaller than W(n - K),
 * tactix_apply_startup_counter_cover validates every premise and refutes the
 * input with a pseudo-Boolean certificate. Other inputs get the command-line
 * defaults (FX, automatic conflict ER, startup counts and parity through
 * arity 8, automatic proof) and tactix_solve. */
#include "tactix.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char program[] = "tactix-vertex-cover-policy";

/* ---- Input ---------------------------------------------------------------- */

struct input {
  int variables;
  int *literals; /* zero-terminated clauses */
  size_t *starts;
  size_t literal_count, count, literal_capacity, start_capacity;
};

static int grow(void **array, size_t *capacity, size_t needed, size_t size) {
  size_t next;
  void *grown;
  if (needed < *capacity) return 1;
  next = *capacity ? 2 * *capacity : 1024;
  while (next <= needed) next *= 2;
  grown = realloc(*array, next * size);
  if (!grown) return 0;
  *array = grown;
  *capacity = next;
  return 1;
}

static int push_literal(struct input *in, int literal) {
  if (!grow((void **)&in->literals, &in->literal_capacity, in->literal_count, sizeof(int)))
    return 0;
  in->literals[in->literal_count++] = literal;
  if (literal) return 1;
  if (!grow((void **)&in->starts, &in->start_capacity, in->count + 1, sizeof(size_t))) return 0;
  in->starts[++in->count] = in->literal_count;
  return 1;
}

/* Reads DIMACS into memory, so the proof route can depend on the structure. */
static int read_input(const char *path, struct input *in) {
  static char line[1 << 20];
  FILE *file = fopen(path, "r");
  int header = 0, open = 0;
  if (!file) {
    fprintf(stderr, "%s: cannot open '%s': %s\n", program, path, strerror(errno));
    return 0;
  }
  if (!grow((void **)&in->starts, &in->start_capacity, 0, sizeof(size_t))) return 0;
  in->starts[0] = 0;
  while (fgets(line, sizeof line, file)) {
    char *cursor = line;
    while (isspace((unsigned char)*cursor)) ++cursor;
    if (!*cursor || *cursor == 'c') continue;
    if (*cursor == 'p') {
      unsigned long long clauses;
      if (sscanf(cursor, "p cnf %d %llu", &in->variables, &clauses) != 2 || in->variables < 0)
        break;
      header = 1;
      continue;
    }
    for (;;) {
      char *end;
      long value;
      while (isspace((unsigned char)*cursor)) ++cursor;
      if (!*cursor) break;
      errno = 0;
      value = strtol(cursor, &end, 10);
      if (end == cursor || errno || value <= INT_MIN || value > INT_MAX) {
        fclose(file);
        fprintf(stderr, "%s: malformed DIMACS\n", program);
        return 0;
      }
      if (labs(value) > in->variables) in->variables = (int)labs(value);
      if (!push_literal(in, (int)value)) {
        fclose(file);
        return 0;
      }
      open = value != 0;
      cursor = end;
    }
  }
  fclose(file);
  if (!header || open) {
    fprintf(stderr, "%s: incomplete DIMACS\n", program);
    return 0;
  }
  return 1;
}

static size_t width(const struct input *in, size_t clause) {
  return in->starts[clause + 1] - in->starts[clause] - 1;
}

static const int *clause_literals(const struct input *in, size_t clause) {
  return in->literals + in->starts[clause];
}

/* ---- Counter recognition --------------------------------------------------- */

struct cover {
  int n, bound;          /* counted vertices x_1..x_n and the counter's K */
  int *vertices;         /* x_i, positive variables in counter order */
  int *registers;        /* (n-1) x K, row-major */
  int *index_of;         /* variable -> 1 + position among vertices, 0 if none */
  int *cliques;          /* zero-terminated vertex literals */
  size_t clique_literals, clique_count;
  int *weights;
  int coverage;
  uint64_t total_weight;
};

/* Recognizes the vertices, register rows and bound; 0 if the input has no
 * sequential counter over the vertices of its all-positive binaries. */
static int recognize_counter(const struct input *in, struct cover *c) {
  const size_t vars = (size_t)in->variables + 1;
  unsigned char *vertex = calloc(vars, 1);
  int *first = calloc(vars, sizeof(int)), *owner = calloc(vars, sizeof(int));
  int *next = calloc(vars, sizeof(int)), *previous = calloc(vars, sizeof(int));
  int *carry = calloc(vars, sizeof(int)), *carry_vertex = calloc(vars, sizeof(int));
  int *row1 = NULL, *row2 = NULL, start = 0, v, ok = 0, count = 0, i, j, K = 0;
  size_t clause, rows_capacity;
  if (!vertex || !first || !owner || !next || !previous || !carry || !carry_vertex) goto done;
  for (clause = 0; clause < in->count; ++clause) {
    const int *l = clause_literals(in, clause);
    if (width(in, clause) == 2 && l[0] > 0 && l[1] > 0 && l[0] != l[1])
      vertex[l[0]] = vertex[l[1]] = 1;
  }
  for (clause = 0; clause < in->count; ++clause) {
    const int *l = clause_literals(in, clause);
    const size_t w = width(in, clause);
    if (w == 2 && (l[0] < 0) != (l[1] < 0)) {
      const int from = l[0] < 0 ? -l[0] : -l[1], to = l[0] < 0 ? l[1] : l[0];
      if (vertex[to]) continue;
      if (vertex[from]) {
        if (first[from]) goto done; /* (~x | s): one first register per vertex */
        first[from] = to;
        owner[to] = from;
      } else {
        if (next[from] || previous[to]) goto done; /* (~s_{i-1,j} | s_{i,j}) */
        next[from] = to;
        previous[to] = from;
      }
    } else if (w == 3) {
      int negatives = 0, positive = 0, x = 0, a = 0, k;
      for (k = 0; k < 3; ++k) {
        if (l[k] > 0) positive = l[k];
        else if (vertex[-l[k]]) x = -l[k], ++negatives;
        else a = -l[k], ++negatives;
      }
      if (negatives != 2 || !positive || !x || !a || vertex[positive]) continue;
      if (carry[a]) goto done; /* (~x_i | ~s_{i-1,j-1} | s_{i,j}) */
      carry[a] = positive;
      carry_vertex[a] = x;
    }
  }
  /* x_1: the vertex whose first register has no predecessor. */
  for (v = 1; v <= in->variables; ++v)
    if (vertex[v] && first[v] && !previous[first[v]]) {
      if (start) goto done;
      start = v;
    }
  if (!start) goto done;
  c->index_of = calloc(vars, sizeof(int));
  c->vertices = malloc(vars * sizeof(int));
  if (!c->index_of || !c->vertices) goto done;
  for (v = start;;) {
    const int s = first[v];
    c->vertices[count++] = v;
    c->index_of[v] = count;
    if (!s || !next[s]) break;
    v = owner[next[s]];
    if (!v || c->index_of[v]) goto done;
  }
  /* x_n has no first register: the vertex in the overflow of the last row. */
  if (count < 2) goto done;
  /* Row widths from rows 1 and 2: s_{2,j} = carry(s_{1,j-1}), s_{1,j} = previous. */
  rows_capacity = vars;
  row1 = malloc(rows_capacity * sizeof(int));
  row2 = malloc(rows_capacity * sizeof(int));
  if (!row1 || !row2) goto done;
  row1[0] = first[c->vertices[0]];
  row2[0] = first[c->vertices[1]];
  for (K = 1; K < (int)rows_capacity; ++K) {
    const int b = carry[row1[K - 1]];
    if (!b || carry_vertex[row1[K - 1]] != c->vertices[1] || !previous[b]) break;
    row2[K] = b;
    row1[K] = previous[b];
  }
  /* The last vertex: (~x_n | ~s_{n-1,K}) with x_n outside the chain. */
  {
    const int last_row = count - 1;
    int *rows = malloc((size_t)count * (size_t)K * sizeof(int)), x_n = 0;
    if (!rows) goto done;
    memcpy(rows, row1, (size_t)K * sizeof(int));
    for (i = 1; i < count; ++i) {
      rows[i * K] = first[c->vertices[i]];
      for (j = 1; j < K; ++j) {
        const int a = rows[(i - 1) * K + j - 1];
        if (carry_vertex[a] != c->vertices[i] || !carry[a]) {
          free(rows);
          goto done;
        }
        rows[i * K + j] = carry[a];
      }
    }
    for (clause = 0; clause < in->count && !x_n; ++clause) {
      const int *l = clause_literals(in, clause);
      if (width(in, clause) == 2 && l[0] < 0 && l[1] < 0) {
        const int s = rows[last_row * K + K - 1];
        if (-l[0] == s && vertex[-l[1]] && !c->index_of[-l[1]]) x_n = -l[1];
        if (-l[1] == s && vertex[-l[0]] && !c->index_of[-l[0]]) x_n = -l[0];
      }
    }
    if (!x_n) {
      free(rows);
      goto done;
    }
    c->vertices[count++] = x_n;
    c->index_of[x_n] = count;
    c->n = count;
    c->bound = K;
    c->registers = rows;
  }
  ok = c->bound < c->n;
done:
  free(vertex);
  free(first);
  free(owner);
  free(next);
  free(previous);
  free(carry);
  free(carry_vertex);
  free(row1);
  free(row2);
  return ok;
}

/* ---- Cliques --------------------------------------------------------------- */

struct clique_search {
  struct cover *c;
  size_t words, capacity, limit;
  uint64_t *adjacency; /* n rows of `words` words */
  int *stack, depth;
  int failed;
};

static int add_clique(struct clique_search *s) {
  int k;
  if (s->c->clique_count == s->limit) return 0;
  for (k = 0; k <= s->depth; ++k)
    if (!grow((void **)&s->c->cliques, &s->capacity, s->c->clique_literals, sizeof(int)))
      return 0;
    else
      s->c->cliques[s->c->clique_literals++] = k < s->depth ? s->c->vertices[s->stack[k]] : 0;
  ++s->c->clique_count;
  return 1;
}

/* Bron-Kerbosch with pivoting over bitsets P and X. */
static void bron_kerbosch(struct clique_search *s, uint64_t *P, uint64_t *X) {
  const size_t words = s->words;
  size_t w;
  int pivot = -1, best = -1, u, empty = 1;
  uint64_t *nextP, *nextX;
  if (s->failed) return;
  for (w = 0; w < words; ++w)
    if (P[w] | X[w]) empty = 0;
  if (empty) {
    if (!add_clique(s)) s->failed = 1;
    return;
  }
  for (u = 0; u < s->c->n; ++u) {
    int count = 0;
    if (!((P[u / 64] | X[u / 64]) >> (u % 64) & 1)) continue;
    for (w = 0; w < words; ++w)
      count += __builtin_popcountll(P[w] & s->adjacency[(size_t)u * words + w]);
    if (count > best) best = count, pivot = u;
  }
  nextP = malloc(2 * words * sizeof(uint64_t));
  if (!nextP) {
    s->failed = 1;
    return;
  }
  nextX = nextP + words;
  for (u = 0; u < s->c->n && !s->failed; ++u) {
    const uint64_t *row = s->adjacency + (size_t)u * words;
    if (!(P[u / 64] >> (u % 64) & 1) ||
        (s->adjacency[(size_t)pivot * words + u / 64] >> (u % 64) & 1))
      continue;
    for (w = 0; w < words; ++w) nextP[w] = P[w] & row[w], nextX[w] = X[w] & row[w];
    s->stack[s->depth++] = u;
    bron_kerbosch(s, nextP, nextX);
    --s->depth;
    P[u / 64] &= ~(UINT64_C(1) << (u % 64));
    X[u / 64] |= UINT64_C(1) << (u % 64);
  }
  free(nextP);
}

static int find_cliques(const struct input *in, struct cover *c) {
  struct clique_search s = {c, (size_t)c->n / 64 + 1, 0, 100000, NULL, NULL, 0, 0};
  uint64_t *P, *X;
  size_t clause;
  int u, ok;
  s.adjacency = calloc((size_t)c->n * s.words, sizeof(uint64_t));
  s.stack = malloc((size_t)c->n * sizeof(int));
  P = calloc(2 * s.words, sizeof(uint64_t));
  if (!s.adjacency || !s.stack || !P) {
    free(s.adjacency);
    free(s.stack);
    free(P);
    return 0;
  }
  X = P + s.words;
  for (clause = 0; clause < in->count; ++clause) {
    const int *l = clause_literals(in, clause);
    int a, b;
    if (width(in, clause) != 2 || l[0] <= 0 || l[1] <= 0) continue;
    a = c->index_of[l[0]] - 1, b = c->index_of[l[1]] - 1;
    if (a < 0 || b < 0 || a == b) continue;
    s.adjacency[(size_t)a * s.words + b / 64] |= UINT64_C(1) << (b % 64);
    s.adjacency[(size_t)b * s.words + a / 64] |= UINT64_C(1) << (a % 64);
  }
  for (u = 0; u < c->n; ++u) P[u / 64] |= UINT64_C(1) << (u % 64);
  bron_kerbosch(&s, P, X);
  ok = !s.failed;
  free(s.adjacency);
  free(s.stack);
  free(P);
  return ok;
}

/* ---- Fractional clique cover ---------------------------------------------- */

/* Solves max sum(z) s.t. sum_{v in C} z_v <= 1 for every clique C, z >= 0, by
 * the simplex method from the slack basis. The optimal clique weights y are
 * the slacks' reduced costs; they solve min sum(y) s.t. every vertex is
 * covered at least once. */
static int fractional_cover(const struct cover *c, double *y) {
  const size_t m = c->clique_count, n = (size_t)c->n, columns = n + m + 1;
  double *t = calloc((m + 1) * columns, sizeof(double));
  size_t *basis = malloc(m * sizeof(size_t)), row, col, at = 0, iterations = 0;
  int ok = 0;
  if (!t || !basis) goto done;
  for (row = 0; row < m; ++row) {
    while (c->cliques[at]) {
      t[row * columns + (size_t)c->index_of[c->cliques[at]] - 1] = 1;
      ++at;
    }
    ++at;
    t[row * columns + n + row] = 1;
    t[row * columns + columns - 1] = 1;
    basis[row] = n + row;
  }
  for (col = 0; col < n; ++col) t[m * columns + col] = -1; /* objective row */
  for (;;) {
    size_t enter = SIZE_MAX, leave = SIZE_MAX;
    double best = -1e-9, ratio = INFINITY;
    if (++iterations > 100000) goto done;
    /* Bland's rule: the first improving column, then the smallest basis. */
    for (col = 0; col + 1 < columns && enter == SIZE_MAX; ++col)
      if (t[m * columns + col] < best) enter = col;
    if (enter == SIZE_MAX) break;
    for (row = 0; row < m; ++row) {
      const double a = t[row * columns + enter];
      if (a > 1e-9) {
        const double r = t[row * columns + columns - 1] / a;
        if (leave == SIZE_MAX || r < ratio - 1e-12 ||
            (fabs(r - ratio) <= 1e-12 && basis[row] < basis[leave]))
          ratio = r, leave = row;
      }
    }
    if (leave == SIZE_MAX) goto done;
    {
      const double pivot = t[leave * columns + enter];
      for (col = 0; col < columns; ++col) t[leave * columns + col] /= pivot;
      for (row = 0; row <= m; ++row) {
        const double factor = t[row * columns + enter];
        if (row == leave || fabs(factor) < 1e-15) continue;
        for (col = 0; col < columns; ++col)
          t[row * columns + col] -= factor * t[leave * columns + col];
      }
      basis[leave] = enter;
    }
  }
  for (row = 0; row < m; ++row) y[row] = t[m * columns + n + row];
  ok = 1;
done:
  free(t);
  free(basis);
  return ok;
}

/* Integer weights for the smallest coverage W <= 12 that turns the fractional
 * cover into an exact one below W(n - K); 0 if none does. */
static int integer_cover(struct cover *c, const double *y) {
  int W;
  c->weights = malloc((c->clique_count + 1) * sizeof(int));
  if (!c->weights) return 0;
  for (W = 1; W <= 12; ++W) {
    uint64_t total = 0;
    size_t k, at = 0;
    int v, ok = 1, *covered = calloc((size_t)c->n, sizeof(int));
    if (!covered) return 0;
    for (k = 0; k < c->clique_count; ++k) {
      const double scaled = y[k] * W;
      c->weights[k] = (int)llround(scaled);
      if (fabs(scaled - c->weights[k]) > 1e-6) ok = 0;
      total += (uint64_t)c->weights[k];
      for (; c->cliques[at]; ++at) covered[c->index_of[c->cliques[at]] - 1] += c->weights[k];
      ++at;
    }
    for (v = 0; v < c->n && ok; ++v) ok = covered[v] >= W;
    free(covered);
    if (ok && total < (uint64_t)W * (uint64_t)(c->n - c->bound)) {
      c->coverage = W;
      c->total_weight = total;
      return 1;
    }
  }
  return 0;
}

/* The cliques with positive weight, and their weights, for the action. */
static int positive_cliques(struct cover *c) {
  size_t k, at = 0, out = 0, kept = 0;
  for (k = 0; k < c->clique_count; ++k) {
    const size_t begin = at;
    while (c->cliques[at]) ++at;
    ++at;
    if (!c->weights[k]) continue;
    memmove(c->cliques + out, c->cliques + begin, (at - begin) * sizeof(int));
    out += at - begin;
    c->weights[kept++] = c->weights[k];
  }
  c->clique_literals = out;
  c->clique_count = kept;
  return kept > 0;
}

/* ---- Solving --------------------------------------------------------------- */

static int install_cli_defaults(tactix *s) {
  static const struct { const char *name, *value; } settings[] = {
      {"TACTIX_FACTOR_FX", "1"},
      {"TACTIX_CONFLICT_ER", "auto"},
      {"TACTIX_STARTUP_MATCHING_COUNT", "1"},
      {"TACTIX_STARTUP_AFFINE_QUOTIENT", "1"},
      {"TACTIX_PARITY_REFUTATION_MAXIMUM_ARITY", "8"},
  };
  size_t i;
  for (i = 0; i < sizeof settings / sizeof *settings; ++i)
    if (!getenv(settings[i].name) &&
        tactix_set_default_option(s, settings[i].name, settings[i].value))
      return -1;
  tactix_defer_startup_state(s);
  tactix_set_bve_gates(s, TACTIX_BVE_GATES_DEFAULT);
  return tactix_discard_structural_provenance(s);
}

static void print_result(tactix *s, int result, int variables) {
  int v;
  if (result == 20) {
    puts("s UNSATISFIABLE");
    return;
  }
  if (result != 10) {
    puts("s UNKNOWN");
    return;
  }
  puts("s SATISFIABLE");
  fputs("v", stdout);
  for (v = 1; v <= variables; ++v) {
    printf(" %d", tactix_value(s, v) > 0 ? v : -v);
    if (v % 20 == 0 && v != variables) fputs("\nv", stdout);
  }
  puts(" 0");
}

int main(int argc, char **argv) {
  const char *proof = NULL;
  struct input in = {0};
  struct cover c = {0};
  double *y = NULL;
  tactix *s = NULL;
  int recognized = 0, result = 0;
  size_t k;
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "usage: %s CNF [PROOF]\n", program);
    return 1;
  }
  if (argc == 3) proof = argv[2];
  if (!read_input(argv[1], &in)) return 1;
  if (recognize_counter(&in, &c) && find_cliques(&in, &c) && c.clique_count &&
      (y = malloc(c.clique_count * sizeof(double))) && fractional_cover(&c, y) &&
      integer_cover(&c, y) && positive_cliques(&c))
    recognized = 1;
  if (c.n)
    printf("c vertex-cover-policy vertices=%d bound=%d cliques=%zu\n", c.n, c.bound,
           c.clique_count);
  s = tactix_init();
  if (!s || install_cli_defaults(s)) return 1;
  /* The cover certificate is pseudo-Boolean; other inputs keep the default
   * automatic proof route. */
  if (proof && tactix_proof(s, proof, recognized ? TACTIX_PROOF_PBP : TACTIX_PROOF_AUTO)) {
    fprintf(stderr, "%s: cannot create proof '%s'\n", program, proof);
    tactix_release(s);
    return 1;
  }
  for (k = 0; k < in.literal_count; ++k)
    if (tactix_add_checked(s, in.literals[k])) {
      fprintf(stderr, "%s: rejected DIMACS input\n", program);
      tactix_release(s);
      return 1;
    }
  if (recognized) {
    printf("c vertex-cover-policy cover weight=%" PRIu64 " coverage=%d demand=%d\n",
           c.total_weight, c.coverage, c.n - c.bound);
    fflush(stdout);
    result = tactix_apply_startup_counter_cover(
        s, c.vertices, c.n, c.registers, c.bound, c.cliques, (int)c.clique_literals,
        c.weights, c.coverage);
    printf("c vertex-cover-policy counter-cover %s\n", result == 20 ? "applied" : "declined");
    if (result != 20) result = 0;
  }
  fflush(stdout);
  if (!result) result = tactix_solve(s);
  if (result == 20 || result == 10) result = tactix_status(s);
  printf("c conflicts %" PRIu64 "\n", tactix_stat(s, TACTIX_STAT_CONFLICTS));
  if (proof && result == 20) printf("c proof-route %s\n", tactix_proof_route(s));
  print_result(s, result, in.variables);
  fflush(stdout);
  tactix_release(s);
  free(y);
  free(c.vertices);
  free(c.registers);
  free(c.index_of);
  free(c.cliques);
  free(c.weights);
  free(in.literals);
  free(in.starts);
  return result == 10 || result == 20 ? result : 0;
}
