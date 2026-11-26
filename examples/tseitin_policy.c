/* Complete XOR encodings (the RT Tseitin family): an inconsistent subset
 * refuted by parity.
 *
 * A Tseitin formula asks that every vertex's edges have the parity of its
 * charge, encoding each vertex of degree d by the 2^(d-1) clauses of width d
 * that forbid the wrong parity. Every edge lies on two vertices, so the sum of
 * all equations cancels every variable and leaves the total charge, 1.
 * Resolution needs exponential size on expanders. Native startup parity stops
 * at width 8, or 16 on uniform inputs.
 *
 * The strategy groups clauses by variable set. A group of n variables whose
 * 2^(n-1) distinct sign patterns all forbid one parity is an equation.
 * Gaussian elimination over GF(2), tracking which equations each row
 * combines, finds a subset summing to 0 = 1, here the odd-charge component.
 * tactix_apply_startup_parity_selection validates those equations against the
 * input and certifies their sum. Otherwise the input is solved as by
 * `tactix`. */
#include "structure_support.h"

#define MAXIMUM_ARITY 24

static const struct cnf *f;
static int *sorted; /* each clause's variables, sorted, at its literal offset */

static int compare_int(const void *a, const void *b) {
  return (*(const int *)a > *(const int *)b) - (*(const int *)a < *(const int *)b);
}
static int compare_scope(const void *a, const void *b) {
  const size_t x = *(const size_t *)a, y = *(const size_t *)b;
  const int wx = width(f, x), wy = width(f, y);
  return wx != wy ? wx - wy
                  : memcmp(sorted + f->start[x], sorted + f->start[y], (size_t)wx * sizeof(int));
}
static int compare_u64(const void *a, const void *b) {
  return (*(const uint64_t *)a > *(const uint64_t *)b) - (*(const uint64_t *)a < *(const uint64_t *)b);
}

/* Bit k of the mask: the clause negates its k-th smallest variable. */
static uint64_t mask(size_t i) {
  const int *c = clause(f, i), *vars = sorted + f->start[i], n = width(f, i);
  uint64_t m = 0;
  for (; *c; ++c)
    if (*c < 0) m |= UINT64_C(1) << ((int *)bsearch(&(int){-*c}, vars, n, sizeof(int), compare_int) - vars);
  return m;
}

/* Fills `sets` with the variables of an inconsistent subset of equations. */
static int inconsistent(struct ints *sets) {
  size_t i, j, k, E = 0, *order = xalloc(f->clauses, sizeof(size_t)), N = 0, *equation;
  size_t words, row, rank = 0, col;
  uint64_t *masks = xalloc(f->clauses, sizeof(uint64_t)), *bits;
  unsigned char *rhs;
  sorted = xalloc(f->lits.size, sizeof(int));
  for (i = 0; i < f->clauses; ++i) {
    int *vars = sorted + f->start[i], n = width(f, i), ok = n >= 2 && n <= MAXIMUM_ARITY;
    for (k = 0; k < (size_t)n; ++k) vars[k] = abs(clause(f, i)[k]);
    qsort(vars, n, sizeof(int), compare_int);
    for (k = 1; ok && k < (size_t)n; ++k) ok = vars[k] != vars[k - 1];
    if (ok) order[N++] = i;
  }
  qsort(order, N, sizeof(size_t), compare_scope);
  /* Runs of one variable set; equation[e] is a representative clause. */
  equation = xalloc(N + 1, sizeof(size_t)), rhs = xalloc(N + 1, 1);
  for (i = 0; i < N; i = j) {
    size_t distinct = 1;
    int parity, uniform = 1;
    for (j = i; j < N && !compare_scope(order + i, order + j); ++j) masks[j - i] = mask(order[j]);
    qsort(masks, j - i, sizeof(uint64_t), compare_u64);
    parity = __builtin_popcountll(masks[0]) & 1;
    for (k = i + 1; k < j; ++k) {
      distinct += masks[k - i] != masks[k - i - 1];
      uniform &= (__builtin_popcountll(masks[k - i]) & 1) == parity;
    }
    /* A clause forbids the assignment setting exactly its negated variables. */
    if (uniform && distinct == UINT64_C(1) << (width(f, order[i]) - 1))
      equation[E] = order[i], rhs[E++] = (unsigned char)!parity;
  }
  printf("c tseitin-policy xor-equations=%zu\n", E);
  /* Gaussian elimination; a row holds variable bits, then equation bits. */
  words = (size_t)f->variables / 64 + 1 + E / 64 + 1;
  bits = xalloc(E * words, sizeof(uint64_t));
#define BIT(r, b) (bits[(r) * words + (b) / 64] >> ((b) % 64) & 1)
#define SET(r, b) (bits[(r) * words + (b) / 64] |= UINT64_C(1) << ((b) % 64))
  for (row = 0; row < E; ++row) {
    for (k = 0; k < (size_t)width(f, equation[row]); ++k) SET(row, (size_t)sorted[f->start[equation[row]] + k]);
    SET(row, ((size_t)f->variables / 64 + 1) * 64 + row);
  }
  for (col = 1; col <= (size_t)f->variables && rank < E; ++col) {
    for (row = rank; row < E && !BIT(row, col); ++row) {}
    if (row == E) continue;
    for (k = 0; k < words; ++k) {
      const uint64_t t = bits[row * words + k];
      bits[row * words + k] = bits[rank * words + k], bits[rank * words + k] = t;
    }
    k = rhs[row], rhs[row] = rhs[rank], rhs[rank] = (unsigned char)k;
    for (row = rank + 1; row < E; ++row)
      if (BIT(row, col)) {
        for (k = 0; k < words; ++k) bits[row * words + k] ^= bits[rank * words + k];
        rhs[row] ^= rhs[rank];
      }
    ++rank;
  }
  /* A row past the rank has no variables; rhs 1 makes it 0 = 1. */
  for (row = rank; row < E && !rhs[row]; ++row) {}
  if (row == E) return 0;
  for (i = 0; i < E; ++i)
    if (BIT(row, ((size_t)f->variables / 64 + 1) * 64 + i)) {
      for (k = 0; k < (size_t)width(f, equation[i]); ++k) push(sets, sorted[f->start[equation[i]] + k]);
      push(sets, 0);
    }
  return 1;
}

int main(int argc, char **argv) {
  struct cnf input;
  struct ints sets = {0};
  int found, result = 0;
  tactix *s;
  if (argc < 2 || argc > 3 || !read_cnf(argv[1], &input)) {
    fputs("usage: tactix-tseitin-policy CNF [PROOF]\n", stderr);
    return 1;
  }
  f = &input;
  found = inconsistent(&sets);
  s = structure_session(&input, argv[2], found);
  if (found) result = tactix_apply_startup_parity_selection(s, sets.at, (int)sets.size);
  return structure_finish(s, result, input.variables, argc == 3);
}
