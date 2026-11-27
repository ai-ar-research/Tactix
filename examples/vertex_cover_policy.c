/* Vertex-cover cardinality over clique-rich graphs (the TS "mis" family):
 * a fractional clique cover refutes a sequential counter.
 *
 * Edges are clauses (x_a | x_b), and a sequential (Sinz) counter over
 * x_1..x_n allows at most K true vertices. The graph is the primal graph of a
 * parity formula, so each constraint scope is a clique, and K asks for one
 * vertex fewer than any cover has. A clique holds at most one uncovered vertex,
 * so cliques covering every vertex W times leave at most sum(weights) / W
 * uncovered: fewer than the n - K the counter demands. Search has to
 * rediscover this count through the counter.
 *
 * The strategy reads the counter from its clauses: each vertex's first
 * register (~x | s), the register chain (~s_{i-1,j} | s_{i,j}) that orders the
 * vertices, and the carries (~x_i | ~s_{i-1,j-1} | s_{i,j}) that give the
 * rows and K. A vertex is a literal in two or more carries, so counters with
 * K < 3 are not recognized. It lists maximal cliques (Bron-Kerbosch), solves
 * the clique cover LP by the simplex method, and scales its optimum to
 * integer weights.
 * tactix_apply_startup_counter_cover validates every premise and certifies
 * the count. Otherwise the input is solved as by `tactix`.
 *
 * Results (commit 0ddfab1be, 2026-11-26; 5,000 s cutoff, 32 GiB, proofs and
 * models checked independently): 339 of the 340 TS inputs verify, against
 * 331 for `tactix` on main. The counter cover refutes 8 of the 9 inputs
 * main leaves unsolved, in at most 0.12 s each; the rest run as `tactix`
 * does, and one satisfiable input times out on both.
 * Scrambled copies (renamed variables, flipped signs, shuffled clauses and
 * literals) give the same 339. */
#include "structure_support.h"

#include <math.h>

static const struct cnf *f;
static int n, K, *vertex_of, *index_of, *rows; /* vertices x_1..x_n, rows (n-1) x K */
static unsigned char *adjacent;               /* n x n */
static struct ints cliques;                    /* zero-terminated vertex lists */
static size_t clique_count;

#define L(l) (2 * (size_t)abs(l) + ((l) < 0)) /* literal index */

/* Orders the counted vertices and reads the register rows; 0 if absent.
 * Literals, not variables, carry the roles, so renaming and sign flips do not
 * matter. A vertex occurs with one sign in two or more carries; the end
 * vertices, in no carry, through their edges. The counter is self-dual
 * (reverse the vertices, negate s_{i,j} into s'_{n-i,K+1-j}), so either end
 * vertex can start the order. */
static int counter(void) {
  const int V = f->variables;
  int *sign = xalloc(V + 1, sizeof(int)), *carries = xalloc(V + 1, sizeof(int));
  int *vertex = xalloc(V + 1, sizeof(int));                    /* variable -> literal */
  int *partners = xalloc(2 * V + 2, sizeof(int)), *partner = xalloc(2 * V + 2, sizeof(int));
  int *owner = xalloc(2 * V + 2, sizeof(int)), *implied = xalloc(2 * V + 2, sizeof(int));
  int *carry = xalloc(2 * V + 2, sizeof(int)), *by = xalloc(2 * V + 2, sizeof(int));
  int *first, *second, v, i, j, x, r;
  size_t c;
  for (c = 0; c < f->clauses; ++c)
    for (i = 0; width(f, c) == 3 && i < 3; ++i) {
      const int l = clause(f, c)[i];
      ++carries[abs(l)], sign[abs(l)] |= l > 0 ? 1 : 2;
    }
  for (v = 1; v <= V; ++v)
    if (carries[v] >= 2 && sign[v] != 3) vertex[v] = sign[v] == 2 ? v : -v; /* (~x | ...) */
  for (c = 0; c < f->clauses; ++c) /* end vertices: carry-free partners of a vertex */
    for (i = 0; width(f, c) == 2 && i < 2; ++i) {
      const int a = clause(f, c)[i], b = clause(f, c)[1 - i];
      if (vertex[abs(a)] == a && !carries[abs(b)]) vertex[abs(b)] = b;
    }
  for (c = 0; c < f->clauses; ++c) {
    const int *l = clause(f, c);
    if (width(f, c) == 2 && !vertex[abs(l[0])] + !vertex[abs(l[1])] == 1) {
      x = vertex[abs(l[0])] ? -l[0] : -l[1]; /* (~x | r): first register or overflow */
      if (vertex[abs(x)] != x) continue;
      partner[L(x)] = vertex[abs(l[0])] ? l[1] : l[0], ++partners[L(x)];
      owner[L(partner[L(x)])] = x;
    } else if (width(f, c) == 2 && !vertex[abs(l[0])] && !vertex[abs(l[1])]) {
      implied[L(-l[0])] = l[1], implied[L(-l[1])] = l[0]; /* (~s_{i-1,j} | s_{i,j}) */
    } else if (width(f, c) == 3) {
      for (i = 0; i < 3 && vertex[abs(l[i])] != -l[i]; ++i) {}
      if (i == 3) continue;
      x = -l[i]; /* (~x_i | ~s_{i-1,j-1} | s_{i,j}), in either orientation */
      carry[L(-l[(i + 1) % 3])] = l[(i + 2) % 3], by[L(-l[(i + 1) % 3])] = x;
      carry[L(-l[(i + 2) % 3])] = l[(i + 1) % 3], by[L(-l[(i + 2) % 3])] = x;
    }
  }
  vertex_of = xalloc(V + 1, sizeof(int)), index_of = xalloc(V + 1, sizeof(int));
  first = xalloc(V + 1, sizeof(int)), second = xalloc(V + 1, sizeof(int));
  for (v = 1, x = 0; v <= V && !x; ++v) /* x_1: an end vertex, with one partner */
    if (vertex[v] && partners[L(vertex[v])] == 1) x = vertex[v];
  /* s_{1,1} is x_1's partner; s_{i,1} is implied by s_{i-1,1} and owned by x_i. */
  for (r = x ? partner[L(x)] : 0; r && x && !index_of[abs(x)]; r = implied[L(r)], x = owner[L(r)])
    first[n] = r, vertex_of[n] = x, index_of[abs(x)] = ++n;
  if (n < 2) return 0;
  /* Width K from rows 1 and 2: s_{2,j} = carry(s_{1,j-1}), s_{1,j} = ~implied(~s_{2,j}). */
  rows = xalloc((size_t)n * (V + 1), sizeof(int));
  rows[0] = first[0], second[0] = first[1];
  for (K = 1; carry[L(rows[K - 1])] && by[L(rows[K - 1])] == vertex_of[1]; ++K)
    second[K] = carry[L(rows[K - 1])], rows[K] = -implied[L(-second[K])];
  for (i = 1; i < n; ++i)
    for (rows[i * K] = first[i], j = 1; j < K; ++j)
      if (by[L(rows[(i - 1) * K + j - 1])] != vertex_of[i]) return 0;
      else rows[i * K + j] = carry[L(rows[(i - 1) * K + j - 1])];
  /* x_n: the other end vertex, whose partner is ~s_{n-1,K}. */
  for (v = 1; v <= V; ++v)
    if ((x = vertex[v]) && !index_of[v] && partners[L(x)] == 1 &&
        partner[L(x)] == -rows[(n - 1) * K + K - 1]) {
      vertex_of[n] = x, index_of[v] = ++n;
      return K < n;
    }
  return 0;
}

/* Bron-Kerbosch with pivoting; P and X are vertex lists, R the clique. */
static void maximal_cliques(int *R, int r, int *P, int p, int *X, int x) {
  int i, j, pivot = -1, best = -1, *P2, *X2, p2, x2;
  if (!p && !x) {
    for (i = 0; i < r; ++i) push(&cliques, vertex_of[R[i]]);
    push(&cliques, 0), ++clique_count;
    return;
  }
  for (i = 0; i < p + x; ++i) {
    const int u = i < p ? P[i] : X[i - p];
    int count = 0;
    for (j = 0; j < p; ++j) count += adjacent[u * n + P[j]];
    if (count > best) best = count, pivot = u;
  }
  P2 = xalloc(2 * (size_t)n, sizeof(int)), X2 = P2 + n;
  for (i = 0; i < p; ++i) {
    const int u = P[i];
    if (adjacent[pivot * n + u]) continue;
    for (p2 = x2 = j = 0; j < p; ++j)
      if (adjacent[u * n + P[j]]) P2[p2++] = P[j];
    for (j = 0; j < x; ++j)
      if (adjacent[u * n + X[j]]) X2[x2++] = X[j];
    R[r] = u;
    maximal_cliques(R, r + 1, P2, p2, X2, x2);
    P[i--] = P[--p], X[x++] = u; /* move u from P to X */
  }
  free(P2);
}

/* Max sum(z) s.t. sum_{v in C} z_v <= 1 per clique, by simplex from the slack
 * basis (Bland's rule). The cover LP's optimal weights are the slacks'
 * reduced costs. */
static double *cover_weights(void) {
  const size_t m = clique_count, cols = (size_t)n + m + 1;
  double *t = xalloc((m + 1) * cols, sizeof(double)), *y = xalloc(m, sizeof(double));
  size_t *basis = xalloc(m, sizeof(size_t)), r, c, at = 0;
  for (r = 0; r < m; ++r, ++at) {
    for (; cliques.at[at]; ++at) t[r * cols + index_of[abs(cliques.at[at])] - 1] = 1;
    t[r * cols + n + r] = t[r * cols + cols - 1] = 1, basis[r] = n + r;
  }
  for (c = 0; c < (size_t)n; ++c) t[m * cols + c] = -1;
  for (;;) {
    size_t in = 0, out = m;
    double ratio = INFINITY;
    while (in + 1 < cols && t[m * cols + in] > -1e-9) ++in;
    if (in + 1 == cols) break;
    for (r = 0; r < m; ++r)
      if (t[r * cols + in] > 1e-9) {
        const double q = t[r * cols + cols - 1] / t[r * cols + in];
        if (q < ratio - 1e-12 || (q < ratio + 1e-12 && basis[r] < basis[out])) ratio = q, out = r;
      }
    if (out == m) return NULL;
    for (c = 0, ratio = t[out * cols + in]; c < cols; ++c) t[out * cols + c] /= ratio;
    for (r = 0; r <= m; ++r)
      for (c = 0, ratio = t[r * cols + in]; r != out && c < cols; ++c)
        t[r * cols + c] -= ratio * t[out * cols + c];
    basis[out] = in;
  }
  for (r = 0; r < m; ++r) y[r] = t[m * cols + n + r];
  return y;
}

int main(int argc, char **argv) {
  struct cnf input;
  struct ints kept = {0}, weights = {0};
  int found = 0, result = 0, W, *R, *P, *X, v;
  double *y = NULL;
  size_t c;
  tactix *s;
  if (argc < 2 || argc > 3 || !read_cnf(argv[1], &input)) {
    fputs("usage: tactix-vertex-cover-policy CNF [PROOF]\n", stderr);
    return 1;
  }
  f = &input;
  if (counter()) {
    adjacent = xalloc((size_t)n * n, 1), R = xalloc(3 * (size_t)n, sizeof(int));
    P = R + n, X = P + n;
    for (c = 0; c < f->clauses; ++c) {
      const int *l = clause(f, c), a = index_of[abs(l[0])] - 1, b = index_of[abs(l[1])] - 1;
      if (width(f, c) == 2 && a >= 0 && b >= 0 && a != b && vertex_of[a] == l[0] && vertex_of[b] == l[1])
        adjacent[a * n + b] = adjacent[b * n + a] = 1;
    }
    for (v = 0; v < n; ++v) P[v] = v;
    maximal_cliques(R, 0, P, n, X, 0);
    y = cover_weights();
  }
  /* The smallest coverage W making W * y integral and below W (n - K). */
  for (W = 1; y && !found && W <= 12; ++W) {
    long total = 0, *covered = xalloc(n, sizeof(long));
    size_t k, at = 0;
    int exact = 1;
    for (kept.size = weights.size = k = 0; k < clique_count; ++k, ++at) {
      const long w = lround(y[k] * W);
      exact &= fabs(y[k] * W - w) < 1e-6;
      for (; cliques.at[at]; ++at)
        if (w) covered[index_of[abs(cliques.at[at])] - 1] += w, push(&kept, cliques.at[at]);
      if (w) push(&kept, 0), push(&weights, (int)w), total += w;
    }
    for (v = 0; v < n && exact; ++v) exact = covered[v] >= W;
    found = exact && total < (long)W * (n - K);
    free(covered);
  }
  printf("c vertex-cover-policy vertices=%d bound=%d cliques=%zu cover=%d\n", n, K, clique_count,
         found);
  s = structure_session(&input, argv[2], found);
  if (found)
    result = tactix_apply_startup_counter_cover(s, vertex_of, n, rows, K, kept.at, (int)kept.size,
                                                weights.at, W - 1);
  return structure_finish(s, result, input.variables, argc == 3);
}
