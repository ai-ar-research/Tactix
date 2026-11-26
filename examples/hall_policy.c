/* Public-API strategy for formulas whose hardness is a hidden pigeonhole.
 *
 * The MI family of the extended-resolution benchmarks is a transversal
 * problem in disguise. Its clauses of three or more literals are disjoint
 * at-least-one demands (one per item), written with scrambled polarities.
 * Nearly all other clauses are binary. Between two demands' options they
 * forbid sharing a resource, and the forbidden pairs form cliques, one per
 * resource. The remaining binaries tie options to a few auxiliary variables.
 * Unit propagation over those binaries refutes some options outright. After
 * that, some set of k items has options in only k-1 resources: a restricted
 * pigeonhole, which resolution cannot refute in polynomial size.
 *
 * The strategy reconstructs this structure from the clauses alone:
 *
 *  - demands: the variable-disjoint clauses of width at least three, whose
 *    literal signs give each option's polarity;
 *  - failed options: options refuted by unit propagation over the input's
 *    unit and binary clauses, repeated until no new option fails, each
 *    check seeing earlier refutations as root facts;
 *  - pools: a clique partition of the surviving options' binary conflicts;
 *  - a Hall violator: a maximum demand-to-pool matching that leaves some
 *    demand unmatched. The demands reached from it along alternating paths
 *    outnumber the pools they can use by one.
 *
 * tactix_apply_startup_matching validates the selection against the original
 * input and refutes it with a pseudo-Boolean counting certificate: summing the
 * k demands and the k-1 pool at-most-one constraints gives 0 >= 1. Without a
 * Hall violator, or if the selection does not validate, the strategy hands
 * the unchanged formula to tactix_solve. */
#include "example_support.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

static const char program[] = "tactix-hall-policy";

/* Input clauses as zero-terminated literals, plus each clause's start. */
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

static size_t width(const struct clauses *c, size_t clause) {
  return c->starts[clause + 1] - c->starts[clause] - 1;
}

/* Literal l as an index: 2v for v, 2v+1 for -v. */
static size_t lit_index(int l) { return 2 * (size_t)abs(l) + (l < 0); }

/* A compressed adjacency list over literal or option indices. */
struct graph {
  size_t *first, *targets;
};

static int build_graph(struct graph *g, size_t nodes, size_t edges, const size_t *from,
                       const size_t *to) {
  size_t e, *fill;
  g->first = calloc(nodes + 1, sizeof(size_t));
  g->targets = malloc((edges ? edges : 1) * sizeof(size_t));
  fill = calloc(nodes + 1, sizeof(size_t));
  if (!g->first || !g->targets || !fill) {
    free(fill);
    return 0;
  }
  for (e = 0; e < edges; ++e) ++g->first[from[e] + 1];
  for (e = 0; e < nodes; ++e) g->first[e + 1] += g->first[e];
  for (e = 0; e < edges; ++e) g->targets[g->first[from[e]] + fill[from[e]]++] = to[e];
  free(fill);
  return 1;
}

static void release_graph(struct graph *g) {
  free(g->first);
  free(g->targets);
}

struct structure {
  int variables;
  struct clauses input;
  /* Binary implications over literal indices and root-true literal indices. */
  struct graph implication;
  unsigned char *root_true;
  /* owner[v]: 1 + the demand containing v, negative when v occurs negated. */
  long *owner;
  size_t *demands; /* clause indices of the demands */
  size_t demand_count;
  /* Failed option literals, in refutation order. */
  int *failed;
  size_t failed_count;
  /* Propagation scratch. */
  unsigned *seen;
  size_t *queue;
  unsigned epoch;
};

/* Does assuming `seed` propagate to a conflict over the input's binary and
 * unit clauses and the refutations recorded so far? Mirrors the check that
 * tactix_apply_startup_matching performs. */
static int fails(struct structure *s, int seed) {
  size_t head = 0, tail = 0;
  if (++s->epoch == 0) {
    memset(s->seen, 0, (2 * (size_t)s->variables + 2) * sizeof(unsigned));
    s->epoch = 1;
  }
  s->queue[tail++] = lit_index(seed);
  s->seen[lit_index(seed)] = s->epoch;
  while (head < tail) {
    const size_t l = s->queue[head++];
    size_t e;
    if (s->root_true[l ^ 1] || s->seen[l ^ 1] == s->epoch) return 1;
    for (e = s->implication.first[l]; e < s->implication.first[l + 1]; ++e) {
      const size_t next = s->implication.targets[e];
      if (s->seen[next] == s->epoch) continue;
      s->seen[next] = s->epoch;
      s->queue[tail++] = next;
    }
  }
  return 0;
}

/* Signed option literal of variable v, or 0 when v is no demand's option. */
static int option(const struct structure *s, int v) {
  return s->owner[v] > 0 ? v : s->owner[v] < 0 ? -v : 0;
}

static size_t demand_of(const struct structure *s, int v) {
  return (size_t)labs(s->owner[v]) - 1;
}

/* Recognizes the demands and binary implications; 0 when the input has no
 * variable-disjoint family of at least two wide clauses. */
static int recognize(struct structure *s) {
  const struct clauses *c = &s->input;
  size_t clause, binaries = 0, e = 0, *from, *to;
  const size_t literals = 2 * (size_t)s->variables + 2;
  int ok;
  s->owner = calloc((size_t)s->variables + 1, sizeof(long));
  s->demands = malloc((c->count ? c->count : 1) * sizeof(size_t));
  s->root_true = calloc(literals, 1);
  s->seen = calloc(literals, sizeof(unsigned));
  s->queue = malloc(literals * sizeof(size_t));
  if (!s->owner || !s->demands || !s->root_true || !s->seen || !s->queue) return 0;
  for (clause = 0; clause < c->count; ++clause) {
    const int *lits = c->literals + c->starts[clause];
    const size_t n = width(c, clause);
    size_t i;
    if (n == 1) s->root_true[lit_index(lits[0])] = 1;
    if (n == 2) ++binaries;
    if (n < 3) continue;
    for (i = 0; i < n; ++i)
      if (s->owner[abs(lits[i])]) return 0;
    s->demands[s->demand_count++] = clause;
    for (i = 0; i < n; ++i)
      s->owner[abs(lits[i])] =
          lits[i] > 0 ? (long)s->demand_count : -(long)s->demand_count;
  }
  if (s->demand_count < 2) return 0;
  from = malloc((2 * binaries + 1) * sizeof(size_t));
  to = malloc((2 * binaries + 1) * sizeof(size_t));
  if (!from || !to) {
    free(from);
    free(to);
    return 0;
  }
  for (clause = 0; clause < c->count; ++clause) {
    const int *lits = c->literals + c->starts[clause];
    if (width(c, clause) != 2) continue;
    from[e] = lit_index(-lits[0]), to[e++] = lit_index(lits[1]);
    from[e] = lit_index(-lits[1]), to[e++] = lit_index(lits[0]);
  }
  ok = build_graph(&s->implication, literals, e, from, to);
  free(from);
  free(to);
  return ok;
}

/* Refutes options by propagation until no further option fails. */
static int find_failed(struct structure *s) {
  int changed = 1, v;
  unsigned char *failed = calloc((size_t)s->variables + 1, 1);
  s->failed = malloc(((size_t)s->variables + 1) * sizeof(int));
  if (!failed || !s->failed) {
    free(failed);
    return 0;
  }
  while (changed) {
    changed = 0;
    for (v = 1; v <= s->variables; ++v) {
      const int o = option(s, v);
      if (!o || failed[v] || s->root_true[lit_index(-o)] || !fails(s, o)) continue;
      failed[v] = 1;
      s->failed[s->failed_count++] = o;
      s->root_true[lit_index(-o)] = 1;
      changed = 1;
    }
  }
  free(failed);
  return 1;
}

/* The surviving options partitioned into pairwise-conflicting pools. */
struct pools {
  size_t *of_variable; /* pool of an option's variable, SIZE_MAX if none */
  size_t count;
};

static int conflicting(const struct graph *conflicts, int a, int b) {
  size_t lo = conflicts->first[a], hi = conflicts->first[a + 1];
  while (lo < hi) {
    const size_t mid = lo + (hi - lo) / 2;
    if (conflicts->targets[mid] == (size_t)b) return 1;
    if (conflicts->targets[mid] < (size_t)b) lo = mid + 1;
    else hi = mid;
  }
  return 0;
}

static int compare_size(const void *a, const void *b) {
  const size_t x = *(const size_t *)a, y = *(const size_t *)b;
  return (x > y) - (x < y);
}

static int partition_pools(const struct structure *s, struct pools *p) {
  const struct clauses *c = &s->input;
  const size_t variables = (size_t)s->variables + 1;
  size_t clause, edges = 0, e = 0, *from, *to, v, *members = NULL, member_count = 0,
                 member_capacity = 0, *member_pool = NULL, member_pool_capacity = 0;
  struct graph conflicts;
  int ok;
  for (clause = 0; clause < c->count; ++clause) edges += 2 * (width(c, clause) == 2);
  from = malloc((edges + 1) * sizeof(size_t));
  to = malloc((edges + 1) * sizeof(size_t));
  p->of_variable = malloc(variables * sizeof(size_t));
  if (!from || !to || !p->of_variable) {
    free(from);
    free(to);
    return 0;
  }
  /* A binary (-a | -b) over two surviving options of different demands. */
  for (clause = 0; clause < c->count; ++clause) {
    const int *lits = c->literals + c->starts[clause];
    int a, b;
    if (width(c, clause) != 2) continue;
    a = -lits[0], b = -lits[1];
    if (option(s, abs(a)) != a || option(s, abs(b)) != b ||
        demand_of(s, abs(a)) == demand_of(s, abs(b)) || s->root_true[lit_index(-a)] ||
        s->root_true[lit_index(-b)])
      continue;
    from[e] = (size_t)abs(a), to[e++] = (size_t)abs(b);
    from[e] = (size_t)abs(b), to[e++] = (size_t)abs(a);
  }
  ok = build_graph(&conflicts, variables, e, from, to);
  free(from);
  free(to);
  if (!ok) return 0;
  for (v = 0; v < variables; ++v)
    qsort(conflicts.targets + conflicts.first[v], conflicts.first[v + 1] - conflicts.first[v],
          sizeof(size_t), compare_size);
  /* Greedy clique partition: join the first neighbouring pool that conflicts
   * with every member, else open a new pool. Members are kept per pool. */
  for (v = 1; v < variables; ++v) {
    const int o = option(s, (int)v);
    size_t chosen = SIZE_MAX, e2;
    p->of_variable[v] = SIZE_MAX;
    if (!o || s->root_true[lit_index(-o)]) continue;
    for (e2 = conflicts.first[v]; e2 < conflicts.first[v + 1] && chosen == SIZE_MAX; ++e2) {
      const size_t u = conflicts.targets[e2];
      size_t m, all = 1;
      if (u >= v || p->of_variable[u] == SIZE_MAX) continue;
      for (m = 0; m < member_count && all; ++m)
        if (member_pool[m] == p->of_variable[u])
          all = conflicting(&conflicts, (int)v, (int)members[m]);
      if (all) chosen = p->of_variable[u];
    }
    if (chosen == SIZE_MAX) chosen = p->count++;
    p->of_variable[v] = chosen;
    if (!grow((void **)&members, &member_capacity, member_count, sizeof(size_t)) ||
        !grow((void **)&member_pool, &member_pool_capacity, member_count, sizeof(size_t))) {
      ok = 0;
      break;
    }
    members[member_count] = v;
    member_pool[member_count++] = chosen;
  }
  free(members);
  free(member_pool);
  release_graph(&conflicts);
  return ok;
}

/* Kuhn's augmenting-path matching of demands to pools. */
struct matching {
  const struct structure *s;
  const struct pools *p;
  size_t *pool_owner;  /* demand matched to a pool, SIZE_MAX if none */
  unsigned *visited;   /* per pool, stamped with the current search */
  unsigned stamp;
};

static int augment(struct matching *m, size_t demand) {
  const struct clauses *c = &m->s->input;
  const size_t clause = m->s->demands[demand];
  const int *lits = c->literals + c->starts[clause];
  size_t i;
  for (i = 0; i < width(c, clause); ++i) {
    const size_t pool = m->p->of_variable[abs(lits[i])];
    if (pool == SIZE_MAX || m->visited[pool] == m->stamp) continue;
    m->visited[pool] = m->stamp;
    if (m->pool_owner[pool] == SIZE_MAX || augment(m, m->pool_owner[pool])) {
      m->pool_owner[pool] = demand;
      return 1;
    }
  }
  return 0;
}

/* The selection passed to tactix_apply_startup_matching. */
struct selection {
  int *demands, *pools;
  size_t demand_literals, pool_literals, demand_count, pool_count;
};

static int append(int **array, size_t *count, size_t *capacity, int literal) {
  if (!grow((void **)array, capacity, *count, sizeof(int))) return 0;
  (*array)[(*count)++] = literal;
  return 1;
}

/* Finds a Hall violator: 1 with `out` filled, 0 without one, -1 on error. */
static int hall_violator(const struct structure *s, const struct pools *p,
                         struct selection *out) {
  const struct clauses *c = &s->input;
  struct matching m = {s, p, NULL, NULL, 0};
  size_t d, free_demand = SIZE_MAX, head = 0, tail = 0, i, pool, demand_capacity = 0,
            pool_capacity = 0;
  size_t *queue = NULL;
  unsigned char *in_s = NULL, *in_t = NULL;
  int result = -1;
  m.pool_owner = malloc((p->count + 1) * sizeof(size_t));
  m.visited = calloc(p->count + 1, sizeof(unsigned));
  if (!m.pool_owner || !m.visited) goto done;
  for (pool = 0; pool < p->count; ++pool) m.pool_owner[pool] = SIZE_MAX;
  for (d = 0; d < s->demand_count; ++d) {
    ++m.stamp;
    if (!augment(&m, d) && free_demand == SIZE_MAX) free_demand = d;
  }
  result = 0;
  if (free_demand == SIZE_MAX) goto done;
  result = -1;
  queue = malloc(s->demand_count * sizeof(size_t));
  in_s = calloc(s->demand_count, 1);
  in_t = calloc(p->count + 1, 1);
  if (!queue || !in_s || !in_t) goto done;
  queue[tail++] = free_demand;
  in_s[free_demand] = 1;
  while (head < tail) {
    const size_t clause = s->demands[queue[head++]];
    const int *lits = c->literals + c->starts[clause];
    for (i = 0; i < width(c, clause); ++i) {
      size_t owner;
      pool = p->of_variable[abs(lits[i])];
      if (pool == SIZE_MAX || in_t[pool]) continue;
      in_t[pool] = 1;
      owner = m.pool_owner[pool];
      /* A maximum matching leaves no alternating path to a free pool. */
      if (owner == SIZE_MAX) goto done;
      if (!in_s[owner]) {
        in_s[owner] = 1;
        queue[tail++] = owner;
      }
    }
  }
  memset(out, 0, sizeof *out);
  for (d = 0; d < s->demand_count; ++d) {
    const size_t clause = s->demands[d];
    const int *lits = c->literals + c->starts[clause];
    if (!in_s[d]) continue;
    ++out->demand_count;
    for (i = 0; i <= width(c, clause); ++i)
      if (!append(&out->demands, &out->demand_literals, &demand_capacity, lits[i])) goto done;
  }
  /* Each pool of T, restricted to the options of demands in S. */
  for (pool = 0; pool < p->count; ++pool) {
    int v;
    if (!in_t[pool]) continue;
    ++out->pool_count;
    for (v = 1; v <= s->variables; ++v)
      if (p->of_variable[v] == pool && in_s[demand_of(s, v)] &&
          !append(&out->pools, &out->pool_literals, &pool_capacity, option(s, v)))
        goto done;
    if (!append(&out->pools, &out->pool_literals, &pool_capacity, 0)) goto done;
  }
  result = 1;
done:
  free(m.pool_owner);
  free(m.visited);
  free(queue);
  free(in_s);
  free(in_t);
  return result;
}

static void release_structure(struct structure *s) {
  free(s->input.literals);
  free(s->input.starts);
  release_graph(&s->implication);
  free(s->root_true);
  free(s->owner);
  free(s->demands);
  free(s->failed);
  free(s->seen);
  free(s->queue);
}

int main(int argc, char **argv) {
  const char *proof = NULL;
  struct structure s;
  struct pools p = {NULL, 0};
  struct selection selection = {NULL, NULL, 0, 0, 0, 0};
  tactix *solver;
  int result = 0, found = 0;
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "usage: %s CNF [PROOF]\n", program);
    return 1;
  }
  if (argc == 3) proof = argv[2];
  memset(&s, 0, sizeof s);
  solver = tactix_init();
  if (!solver) return 1;
  /* The counting certificate is pseudo-Boolean: request whole-run PBP. */
  if (proof && tactix_proof(solver, proof, TACTIX_PROOF_PBP)) {
    fprintf(stderr, "%s: cannot create proof '%s'\n", program, proof);
    tactix_release(solver);
    return 1;
  }
  if (!example_read_dimacs(solver, argv[1], program, NULL, 1 << 20, &s.variables,
                           record_literal, &s.input)) {
    release_structure(&s);
    tactix_release(solver);
    return 1;
  }
  if (recognize(&s) && find_failed(&s) && partition_pools(&s, &p))
    found = hall_violator(&s, &p, &selection);
  printf("c hall-policy demands=%zu failed=%zu pools=%zu\n", s.demand_count, s.failed_count,
         p.count);
  if (found == 1) {
    printf("c hall-policy violator demands=%zu pools=%zu\n", selection.demand_count,
           selection.pool_count);
    result = tactix_apply_startup_matching(
        solver, selection.demands, (int)selection.demand_literals, selection.pools,
        (int)selection.pool_literals, s.failed, (int)s.failed_count);
    printf("c hall-policy matching-refutation %s\n", result == 20 ? "applied" : "declined");
    if (result != 20) result = 0;
  }
  fflush(stdout);
  if (!result) result = tactix_solve(solver);
  printf("c conflicts %" PRIu64 "\n", tactix_stat(solver, TACTIX_STAT_CONFLICTS));
  example_print_result(solver, result, s.variables);
  free(selection.demands);
  free(selection.pools);
  free(p.of_variable);
  release_structure(&s);
  tactix_release(solver);
  return result == 10 || result == 20 ? result : 0;
}
