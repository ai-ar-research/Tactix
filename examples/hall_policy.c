/* Hidden pigeonholes (the MI family): a Hall violator refuted by counting.
 *
 * Each clause of width >= 3 is an item's demand for one of its options, with
 * scrambled signs; the demands are variable-disjoint. Binary clauses forbid
 * two items' options from sharing a resource, and these conflicts form one
 * clique per resource. A few auxiliary variables let unit propagation refute
 * some options outright. Afterwards k items have options in only k-1
 * resources, a restricted pigeonhole that resolution cannot refute in
 * polynomial size, but whose count is short in pseudo-Boolean reasoning.
 *
 * The strategy refutes options by propagation until none fails. Surviving
 * conflicts join options into pools (connected components). A maximum
 * matching of demands to pools leaves one demand unmatched; the demands
 * reached from it by alternating paths (S) use only |S| - 1 pools.
 * tactix_apply_startup_matching validates that selection against the input
 * and certifies the count. Otherwise the input is solved as by `tactix`.
 *
 * Results (commit 0ddfab1be, 2026-11-26; 5,000 s cutoff, 32 GiB, proofs
 * checked by VeriPB and CakePB against the original CNF): all 23 MI inputs
 * are recognized and verified UNSAT in at most 0.12 s each; `tactix` on main
 * verifies 10. A scrambled copy of each input (renamed variables, flipped
 * signs, shuffled clauses and literals) gives the same 23. */
#include "structure_support.h"

#define L(l) (2 * (size_t)abs(l) + ((l) < 0)) /* literal index */

static const struct cnf *f;
static long *owner;            /* variable -> +-(1 + demand), sign of its option */
static size_t *demand_clause;  /* demand -> clause */
static unsigned char *root;    /* root-true literal indices */
static int *first, *target;    /* binary implications, compressed by literal index */
static int *seen, *queue, epoch;
static int *parent, *matched, *mark, stamp; /* pools, matching */

static int option(int v) { return owner[v] > 0 ? v : owner[v] < 0 ? -v : 0; }
static int alive(int v) { return option(v) && !root[L(-option(v))]; }
static int pool(int v) {
  while (parent[v] != v) v = parent[v] = parent[parent[v]];
  return v;
}

/* Does assuming l propagate to a conflict? The action repeats this check. */
static int fails(int l) {
  int head = 0, tail = 0, e;
  seen[L(l)] = ++epoch, queue[tail++] = (int)L(l);
  while (head < tail) {
    const int x = queue[head++];
    if (root[x ^ 1] || seen[x ^ 1] == epoch) return 1;
    for (e = first[x]; e < first[x + 1]; ++e)
      if (seen[target[e]] != epoch) seen[target[e]] = epoch, queue[tail++] = target[e];
  }
  return 0;
}

static int augment(int d) {
  const int *c = clause(f, demand_clause[d]);
  for (; *c; ++c) {
    const int p = alive(abs(*c)) ? pool(abs(*c)) : 0;
    if (!p || mark[p] == stamp) continue;
    mark[p] = stamp;
    if (!matched[p] || augment(matched[p] - 1)) return matched[p] = d + 1;
  }
  return 0;
}

/* Fills the selection with a Hall violator; 0 when there is none. */
static int hall(struct ints *demands, struct ints *pools, struct ints *failed) {
  const int V = f->variables;
  int D = 0, v, e, free = -1, changed = 1, *in_s, *in_t, *fill, head = 0, tail = 0;
  size_t i;
  owner = xalloc(V + 1, sizeof(long)), demand_clause = xalloc(f->clauses, sizeof(size_t));
  root = xalloc(2 * V + 2, 1), first = xalloc(2 * V + 3, sizeof(int));
  seen = xalloc(2 * V + 2, sizeof(int)), queue = xalloc(2 * V + 2, sizeof(int));
  for (i = 0; i < f->clauses; ++i) {
    const int *c = clause(f, i);
    if (width(f, i) == 1) root[L(c[0])] = 1;
    if (width(f, i) == 2) ++first[L(-c[0]) + 1], ++first[L(-c[1]) + 1];
    if (width(f, i) < 3) continue;
    for (; *c; ++c)
      if (owner[abs(*c)]) return 0;
      else owner[abs(*c)] = *c > 0 ? D + 1 : -(D + 1);
    demand_clause[D++] = i;
  }
  if (D < 2) return 0;
  for (i = 1; i < 2 * (size_t)V + 3; ++i) first[i] += first[i - 1];
  target = xalloc(first[2 * V + 2], sizeof(int)), fill = xalloc(2 * V + 2, sizeof(int));
  for (i = 0; i < f->clauses; ++i)
    if (width(f, i) == 2) {
      const int *c = clause(f, i);
      target[first[L(-c[0])] + fill[L(-c[0])]++] = (int)L(c[1]);
      target[first[L(-c[1])] + fill[L(-c[1])]++] = (int)L(c[0]);
    }
  while (changed)
    for (changed = 0, v = 1; v <= V; ++v)
      if (alive(v) && fails(option(v)))
        push(failed, option(v)), root[L(-option(v))] = 1, changed = 1;
  /* Pools: components of the conflicts (-a | -b) between surviving options. */
  parent = xalloc(V + 1, sizeof(int));
  for (v = 0; v <= V; ++v) parent[v] = v;
  for (i = 0; i < f->clauses; ++i) {
    const int *c = clause(f, i), a = abs(c[0]), b = abs(c[1]);
    if (width(f, i) == 2 && alive(a) && alive(b) && option(a) == -c[0] &&
        option(b) == -c[1] && labs(owner[a]) != labs(owner[b]))
      parent[pool(a)] = pool(b);
  }
  matched = xalloc(V + 1, sizeof(int)), mark = xalloc(V + 1, sizeof(int));
  for (e = 0; e < D; ++e)
    if (++stamp, !augment(e) && free < 0) free = e;
  if (free < 0) return 0;
  /* S: demands reached from the free one by alternating paths; T: their pools. */
  in_s = xalloc(D, sizeof(int)), in_t = xalloc(V + 1, sizeof(int));
  in_s[free] = 1, queue[tail++] = free;
  while (head < tail)
    for (const int *c = clause(f, demand_clause[queue[head++]]); *c; ++c) {
      const int p = alive(abs(*c)) ? pool(abs(*c)) : 0;
      if (!p || in_t[p]) continue;
      in_t[p] = 1;
      if (!matched[p]) return 0;
      if (!in_s[matched[p] - 1]) in_s[matched[p] - 1] = 1, queue[tail++] = matched[p] - 1;
    }
  for (e = 0; e < D; ++e)
    for (const int *c = clause(f, demand_clause[e]); in_s[e]; ++c)
      if (push(demands, *c), !*c) break;
  for (e = 1; e <= V; ++e) {
    if (!in_t[e]) continue;
    for (v = 1; v <= V; ++v)
      if (alive(v) && pool(v) == e && in_s[labs(owner[v]) - 1]) push(pools, option(v));
    push(pools, 0);
  }
  printf("c hall-policy demands=%d failed=%zu violator=%d\n", D, failed->size, tail);
  return 1;
}

int main(int argc, char **argv) {
  struct cnf input;
  struct ints demands = {0}, pools = {0}, failed = {0};
  int found, result = 0;
  tactix *s;
  if (argc < 2 || argc > 3 || !read_cnf(argv[1], &input)) {
    fputs("usage: tactix-hall-policy CNF [PROOF]\n", stderr);
    return 1;
  }
  f = &input;
  found = hall(&demands, &pools, &failed);
  s = structure_session(&input, argv[2], found);
  if (found)
    result = tactix_apply_startup_matching(s, demands.at, (int)demands.size, pools.at,
                                           (int)pools.size, failed.at, (int)failed.size);
  return structure_finish(s, result, input.variables, argc == 3);
}
