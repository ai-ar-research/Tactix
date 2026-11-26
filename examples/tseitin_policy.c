/* Public-API strategy for formulas made of complete XOR encodings.
 *
 * The RT family encodes Tseitin formulas on random d-regular graphs: each
 * edge is a variable, and each vertex v asks that the parity of its d edges
 * equals the vertex's charge. The direct encoding spends 2^(d-1) clauses of
 * width d per vertex, one for each assignment of the wrong parity. Every edge
 * lies on two vertices, so summing all vertex equations cancels every
 * variable. The charges sum to one, so that sum reads 0 = 1. Resolution needs
 * exponential size in the graph's expansion, while CDCL sees only wide
 * clauses. Native startup parity recognizes widths up to 8 on any proof route,
 * and up to 16 on uniform inputs with a pseudo-Boolean proof.
 *
 * The strategy recovers the equations from the clauses alone:
 *
 *  - equations: clauses grouped by their variable set; a group of n
 *    variables whose 2^(n-1) distinct clauses forbid exactly the assignments
 *    of one parity encodes an XOR equation;
 *  - an inconsistent subset: Gaussian elimination over GF(2) tracks which
 *    equations combine into each row and stops at a row 0 = 1. For a
 *    Tseitin formula this is the odd-charge connected component.
 *
 * tactix_apply_startup_parity_selection validates the selected sets against
 * the original input and refutes them with a pseudo-Boolean certificate whose
 * size is linear in their clauses. Without an inconsistent subset the
 * strategy hands the unchanged formula to tactix_solve. */
#include "example_support.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

#define MAXIMUM_ARITY 24

static const char program[] = "tactix-tseitin-policy";

struct clauses {
  int *literals;
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

static int record_literal(void *context, int literal) {
  struct clauses *c = context;
  if (!grow((void **)&c->literals, &c->literal_capacity, c->literal_count, sizeof(int)))
    return 0;
  if (!c->starts) {
    if (!grow((void **)&c->starts, &c->start_capacity, 0, sizeof(size_t))) return 0;
    c->starts[0] = 0;
  }
  c->literals[c->literal_count++] = literal;
  if (literal) return 1;
  if (!grow((void **)&c->starts, &c->start_capacity, c->count + 1, sizeof(size_t))) return 0;
  c->starts[++c->count] = c->literal_count;
  return 1;
}

static int compare_int(const void *a, const void *b) {
  const int x = *(const int *)a, y = *(const int *)b;
  return (x > y) - (x < y);
}

/* A clause's sorted variables; 0 for widths outside 2..MAXIMUM_ARITY or a
 * repeated variable. */
static int scope(const struct clauses *c, size_t clause, int *variables) {
  const int *lits = c->literals + c->starts[clause];
  const size_t n = c->starts[clause + 1] - c->starts[clause] - 1;
  size_t i;
  if (n < 2 || n > MAXIMUM_ARITY) return 0;
  for (i = 0; i < n; ++i) variables[i] = abs(lits[i]);
  qsort(variables, n, sizeof(int), compare_int);
  for (i = 1; i < n; ++i)
    if (variables[i] == variables[i - 1]) return 0;
  return (int)n;
}

static uint64_t hash_scope(const int *variables, int n) {
  uint64_t h = (uint64_t)n;
  int i;
  for (i = 0; i < n; ++i) h = h * UINT64_C(0x9e3779b185ebca87) + (uint32_t)variables[i];
  return h ^ (h >> 29);
}

/* Clauses grouped by variable set in an open-addressing table. */
struct group {
  size_t first_clause; /* a clause holding the group's variables */
  uint64_t clauses;    /* clauses with this variable set */
  uint64_t *masks;     /* distinct sign patterns, for complete candidates */
  uint64_t distinct;
  int arity, parity;   /* parity of the forbidden assignments, -1 mixed */
};

struct groups {
  struct group *slots;
  size_t capacity, count;
};

static struct group *find_group(struct groups *g, const struct clauses *c, const int *variables,
                                int n, int create) {
  int other[MAXIMUM_ARITY];
  size_t at = (size_t)(hash_scope(variables, n) & (g->capacity - 1));
  for (;; at = (at + 1) & (g->capacity - 1)) {
    struct group *slot = g->slots + at;
    if (!slot->arity) {
      if (!create) return NULL;
      slot->arity = n;
      slot->first_clause = SIZE_MAX;
      slot->parity = -2;
      ++g->count;
      return slot;
    }
    if (slot->arity == n && slot->first_clause != SIZE_MAX &&
        scope(c, slot->first_clause, other) == n &&
        !memcmp(other, variables, (size_t)n * sizeof(int)))
      return slot;
  }
}

/* One equation as a GF(2) row over variables, tagged with the equations it
 * combines. */
struct rows {
  size_t equations, variable_words, equation_words;
  uint64_t *bits; /* per row: variable words, then equation words */
  unsigned char *rhs;
};

static uint64_t *row_bits(struct rows *r, size_t row) {
  return r->bits + row * (r->variable_words + r->equation_words);
}

int main(int argc, char **argv) {
  const char *proof = NULL;
  struct clauses input = {0};
  struct groups table = {NULL, 0, 0};
  struct group **equations = NULL;
  size_t equation_count = 0, clause, i;
  struct rows rows = {0};
  int *selected = NULL, maximum_variable = 0, result = 0, scratch[MAXIMUM_ARITY];
  size_t selected_count = 0, selected_capacity = 0, inconsistent = SIZE_MAX;
  tactix *solver;
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "usage: %s CNF [PROOF]\n", program);
    return 1;
  }
  if (argc == 3) proof = argv[2];
  solver = tactix_init();
  if (!solver) return 1;
  /* The parity certificate is pseudo-Boolean: request whole-run PBP. */
  if (proof && tactix_proof(solver, proof, TACTIX_PROOF_PBP)) {
    fprintf(stderr, "%s: cannot create proof '%s'\n", program, proof);
    tactix_release(solver);
    return 1;
  }
  if (!example_read_dimacs(solver, argv[1], program, NULL, 1 << 20, &maximum_variable,
                           record_literal, &input))
    goto finish;

  /* Count clauses per variable set. */
  table.capacity = 1024;
  while (table.capacity < 2 * input.count + 2) table.capacity *= 2;
  table.slots = calloc(table.capacity, sizeof *table.slots);
  if (!table.slots) goto finish;
  for (clause = 0; clause < input.count; ++clause) {
    const int n = scope(&input, clause, scratch);
    struct group *g;
    if (!n) continue;
    g = find_group(&table, &input, scratch, n, 1);
    if (g->first_clause == SIZE_MAX) g->first_clause = clause;
    ++g->clauses;
  }
  /* Record sign patterns of the groups that could be complete encodings. */
  for (i = 0; i < table.capacity; ++i) {
    struct group *g = table.slots + i;
    if (!g->arity || g->clauses < (UINT64_C(1) << (g->arity - 1))) continue;
    g->masks = calloc(((size_t)1 << g->arity) / 64 + 1, sizeof(uint64_t));
    if (!g->masks) goto finish;
  }
  for (clause = 0; clause < input.count; ++clause) {
    const int *lits = input.literals + input.starts[clause];
    const int n = scope(&input, clause, scratch);
    struct group *g;
    uint64_t mask = 0;
    int j;
    if (!n || !(g = find_group(&table, &input, scratch, n, 0)) || !g->masks) continue;
    /* Bit k is set when the clause negates the k-th smallest variable. */
    for (j = 0; j < n; ++j) {
      const int *at = bsearch(&(int){abs(lits[j])}, scratch, (size_t)n, sizeof(int), compare_int);
      if (lits[j] < 0) mask |= UINT64_C(1) << (at - scratch);
    }
    if (g->masks[mask / 64] & (UINT64_C(1) << (mask % 64))) continue;
    g->masks[mask / 64] |= UINT64_C(1) << (mask % 64);
    ++g->distinct;
    /* A clause forbids the assignment setting exactly its negated variables. */
    j = __builtin_popcountll(mask) & 1;
    g->parity = g->parity == -2 ? j : g->parity == j ? j : -1;
  }
  equations = malloc((table.count + 1) * sizeof *equations);
  if (!equations) goto finish;
  for (i = 0; i < table.capacity; ++i) {
    struct group *g = table.slots + i;
    if (g->masks && g->parity >= 0 && g->distinct == (UINT64_C(1) << (g->arity - 1)))
      equations[equation_count++] = g;
  }
  printf("c tseitin-policy xor-equations=%zu\n", equation_count);

  /* Gaussian elimination with combination tracking. */
  if (equation_count) {
    size_t row, column, rank = 0, width;
    rows.equations = equation_count;
    rows.variable_words = (size_t)maximum_variable / 64 + 1;
    rows.equation_words = equation_count / 64 + 1;
    width = rows.variable_words + rows.equation_words;
    rows.bits = calloc(equation_count * width, sizeof(uint64_t));
    rows.rhs = calloc(equation_count, 1);
    if (!rows.bits || !rows.rhs) goto finish;
    for (row = 0; row < equation_count; ++row) {
      uint64_t *bits = row_bits(&rows, row);
      const int n = scope(&input, equations[row]->first_clause, scratch);
      int j;
      for (j = 0; j < n; ++j) bits[scratch[j] / 64] |= UINT64_C(1) << (scratch[j] % 64);
      bits[rows.variable_words + row / 64] |= UINT64_C(1) << (row % 64);
      /* The allowed parity is the opposite of the forbidden one. */
      rows.rhs[row] = (unsigned char)!equations[row]->parity;
    }
    for (column = 1; column <= (size_t)maximum_variable && rank < equation_count; ++column) {
      const uint64_t bit = UINT64_C(1) << (column % 64);
      size_t pivot = SIZE_MAX;
      for (row = rank; row < equation_count && pivot == SIZE_MAX; ++row)
        if (row_bits(&rows, row)[column / 64] & bit) pivot = row;
      if (pivot == SIZE_MAX) continue;
      if (pivot != rank) {
        uint64_t *a = row_bits(&rows, pivot), *b = row_bits(&rows, rank);
        unsigned char t = rows.rhs[pivot];
        size_t w;
        for (w = 0; w < width; ++w) {
          const uint64_t x = a[w];
          a[w] = b[w];
          b[w] = x;
        }
        rows.rhs[pivot] = rows.rhs[rank];
        rows.rhs[rank] = t;
      }
      for (row = rank + 1; row < equation_count; ++row) {
        uint64_t *target = row_bits(&rows, row);
        const uint64_t *source = row_bits(&rows, rank);
        size_t w;
        if (!(target[column / 64] & bit)) continue;
        for (w = 0; w < width; ++w) target[w] ^= source[w];
        rows.rhs[row] ^= rows.rhs[rank];
      }
      ++rank;
    }
    /* Rows below the rank have no variables left; one with rhs 1 is 0 = 1. */
    for (row = rank; row < equation_count && inconsistent == SIZE_MAX; ++row)
      if (rows.rhs[row]) inconsistent = row;
  }

  if (inconsistent != SIZE_MAX) {
    const uint64_t *combination = row_bits(&rows, inconsistent) + rows.variable_words;
    size_t equations_used = 0, row;
    for (row = 0; row < equation_count; ++row) {
      int n, j;
      if (!(combination[row / 64] & (UINT64_C(1) << (row % 64)))) continue;
      ++equations_used;
      n = scope(&input, equations[row]->first_clause, scratch);
      for (j = 0; j <= n; ++j)
        if (!grow((void **)&selected, &selected_capacity, selected_count, sizeof(int))) goto finish;
        else selected[selected_count++] = j < n ? scratch[j] : 0;
    }
    printf("c tseitin-policy inconsistent-subset equations=%zu\n", equations_used);
    fflush(stdout);
    result = tactix_apply_startup_parity_selection(solver, selected, (int)selected_count);
    printf("c tseitin-policy parity-refutation %s\n", result == 20 ? "applied" : "declined");
    if (result != 20) result = 0;
  }
  fflush(stdout);
  if (!result) result = tactix_solve(solver);
  printf("c conflicts %" PRIu64 "\n", tactix_stat(solver, TACTIX_STAT_CONFLICTS));
  example_print_result(solver, result, maximum_variable);
finish:
  if (table.slots)
    for (i = 0; i < table.capacity; ++i) free(table.slots[i].masks);
  free(table.slots);
  free(equations);
  free(rows.bits);
  free(rows.rhs);
  free(selected);
  free(input.literals);
  free(input.starts);
  tactix_release(solver);
  return result == 10 || result == 20 ? result : 0;
}
