"""Seeded CNF generators for the fuzz harness (plan §8.2).

Every generator is a pure function of a random.Random instance; the runner
derives one seed per case and records it in every failure artifact, so any
case is reproducible from (master seed, index) alone.

Categories (dispatched by weight in generate()):
  - random k-SAT sweeping the density axis (k in {2,3,4,5})
  - cnfuzz-style structured CNFs (variable-reuse clusters, mixed lengths)
  - mutation of a base instance (flip/drop/duplicate literals and clauses)
  - FuzzSAT-style random Boolean circuits, Tseitin-encoded to CNF
  - adversarial edge cases (fixed list, run every batch at indices 0..9)
"""

import random

# Density sweep for k=3 (plan §8.2); scaled for other k below.
K3_RATIOS = [3.0, 4.0, 4.26, 4.6, 5.5]


def random_ksat(rng: random.Random, n: int, m: int, k: int):
    clauses = []
    for _ in range(m):
        vs = rng.sample(range(1, n + 1), min(k, n))
        clauses.append([v if rng.random() < 0.5 else -v for v in vs])
    return n, clauses


def gen_sweep(rng: random.Random):
    k = rng.choice([2, 3, 3, 3, 4, 5])  # k=3 is the interesting regime
    n = rng.randint(20, 100)
    if k == 3:
        ratio = rng.choice(K3_RATIOS)
    elif k == 2:
        ratio = rng.uniform(0.8, 1.5)  # 2-SAT transition at m/n = 1
    else:
        # transition scales roughly like 2^k * ln 2
        ratio = rng.uniform(0.6, 1.3) * (2 ** k) * 0.693
    m = max(1, int(n * ratio))
    n, clauses = random_ksat(rng, n, m, k)
    return f"ksat-k{k}-n{n}-m{m}", n, clauses


def gen_structured(rng: random.Random):
    """cnfuzz-style: clustered variable reuse + mixed clause lengths.

    Uniform random CNF exercises one code path; real instances have
    community structure and repeated variables, which is where watcher and
    inprocessing bugs hide (cnfuzz's design insight — Brummayer & Biere).
    """
    nclusters = rng.randint(2, 8)
    # cap total input vars at 100 (the random k-SAT problem-size bound):
    # bound the per-cluster size so nclusters * cluster_size <= 100
    cluster_size = rng.randint(5, max(5, 100 // nclusters))
    n = nclusters * cluster_size
    m = rng.randint(n, 5 * n)
    clauses = []
    for _ in range(m):
        c = rng.randrange(nclusters)
        # occasionally bridge two clusters
        pool_clusters = [c] if rng.random() < 0.8 else [c, rng.randrange(nclusters)]
        pool = [
            v
            for pc in pool_clusters
            for v in range(pc * cluster_size + 1, (pc + 1) * cluster_size + 1)
        ]
        length = rng.choice([1, 2, 2, 3, 3, 3, 4, 5, 6])
        vs = rng.sample(pool, min(length, len(pool)))
        clause = [v if rng.random() < 0.5 else -v for v in vs]
        # inject the occasional duplicate literal or tautology (legal DIMACS)
        if clause and rng.random() < 0.05:
            clause.append(clause[0] if rng.random() < 0.5 else -clause[0])
        clauses.append(clause)
    return f"struct-c{nclusters}x{cluster_size}-m{m}", n, clauses


def gen_mutation(rng: random.Random, base=None):
    """Mutate a base instance. Until Tier A is frozen the base is itself a
    generated sweep case; the runner can later supply Tier A/B instances."""
    if base is None:
        _, n, clauses = gen_sweep(rng)
    else:
        n, clauses = base
        clauses = [list(c) for c in clauses]
    for _ in range(rng.randint(1, 6)):
        op = rng.randrange(5)
        if op == 0 and clauses:  # flip a literal sign
            c = rng.choice(clauses)
            if c:
                i = rng.randrange(len(c))
                c[i] = -c[i]
        elif op == 1 and clauses:  # drop a clause
            clauses.pop(rng.randrange(len(clauses)))
        elif op == 2 and clauses:  # duplicate a clause
            clauses.append(list(rng.choice(clauses)))
        elif op == 3 and clauses:  # drop a literal (may create empty clause)
            c = rng.choice(clauses)
            if c:
                c.pop(rng.randrange(len(c)))
        else:  # add a random clause
            length = rng.randint(1, 4)
            vs = rng.sample(range(1, n + 1), min(length, n))
            clauses.append([v if rng.random() < 0.5 else -v for v in vs])
    return f"mut-n{n}-m{len(clauses)}", n, clauses


def gen_fuzzsat(rng: random.Random):
    """FuzzSAT-style: a random Boolean circuit, Tseitin-encoded to CNF.

    Brummayer & Biere's FuzzSAT builds a random circuit over input variables
    and Tseitin-encodes each gate, then asserts the root. This yields
    structured, sharing-heavy formulas distinct from both uniform k-SAT and
    the cnfuzz variable-reuse clusters — the aux gate vars create long
    definitional chains that stress watchers, equivalence reasoning and BVE.
    """
    inputs = rng.randint(5, 30)
    # pool of available literals, seeded with a random polarity of each input
    pool = [v if rng.random() < 0.5 else -v for v in range(1, inputs + 1)]
    # choose gate count so total vars (inputs + gates) stays <= ~120
    hi = min(3 * inputs, 120 - inputs)
    n_gates = max(1, rng.randint(inputs, hi) if hi >= inputs else 1)

    def _rand_input_lit():
        v = rng.randint(1, inputs)
        return v if rng.random() < 0.5 else -v

    clauses = []
    g = inputs  # last-allocated variable index
    for _ in range(n_gates):
        # pop two random literals from the pool (refill from inputs if empty)
        if not pool:
            pool.append(_rand_input_lit())
        a = pool.pop(rng.randrange(len(pool)))
        if not pool:
            pool.append(_rand_input_lit())
        b = pool.pop(rng.randrange(len(pool)))
        # random re-negation
        if rng.random() < 0.5:
            a = -a
        if rng.random() < 0.5:
            b = -b
        g += 1  # allocate the gate variable
        op = rng.choice(["AND", "OR", "XOR"])
        if op == "AND":       # g <-> (a & b)
            clauses += [[-g, a], [-g, b], [g, -a, -b]]
        elif op == "OR":      # g <-> (a | b)
            clauses += [[g, -a], [g, -b], [-g, a, b]]
        else:                 # XOR: g <-> (a ^ b)
            clauses += [[-g, a, b], [-g, -a, -b], [g, -a, b], [g, a, -b]]
        # push the gate output back (random polarity) for reuse/sharing
        pool.append(g if rng.random() < 0.5 else -g)
        # occasionally also re-inject an input literal to raise sharing
        if rng.random() < 0.2:
            pool.append(_rand_input_lit())
    total_nvars = g  # == inputs + n_gates
    # assert the root (last gate); ~50% assert its negation to vary SAT/UNSAT
    clauses.append([g if rng.random() < 0.5 else -g])
    return f"fuzzsat-i{inputs}-g{n_gates}", total_nvars, clauses


EDGE_CASES = [
    ("edge-empty-formula", 0, []),
    ("edge-empty-clause", 1, [[]]),
    ("edge-units-only", 5, [[1], [-2], [3], [-4], [5]]),
    ("edge-contradictory-units", 2, [[1], [2], [-1]]),
    ("edge-tautology", 3, [[1, -1, 2], [2, 3], [-3, 1]]),
    ("edge-duplicate-lits", 3, [[1, 1, 2], [-2, -2], [3, 1]]),
    ("edge-max-var-index", 500000, [[500000, -1], [1]]),
    ("edge-single-huge-clause", 400, [[v for v in range(1, 401)]]),
    ("edge-huge-negative-clause", 300, [[-v for v in range(1, 301)], [7]]),
    ("edge-unit-empty-mix", 4, [[1], [2, 3], []]),
]


def gen_edge(index: int):
    name, n, clauses = EDGE_CASES[index % len(EDGE_CASES)]
    return name, n, clauses


def generate(seed: int, index: int):
    """Dispatch one case. Returns (name, nvars, clauses).

    The 10 fixed adversarial degeneracies always run: indices 0..9 map
    deterministically to EDGE_CASES[0..9], so every batch (both --quick
    n=1000 and --nightly n=10000) exercises all of them. For index >= 10
    we draw from rng with the split: random k-SAT sweep 15% / structured
    (cnfuzz) 30% / mutation 25% / FuzzSAT 30%.
    """
    if index < len(EDGE_CASES):
        return gen_edge(index)
    rng = random.Random(seed)
    r = rng.random()
    if r < 0.15:            # random k-SAT density sweep
        return gen_sweep(rng)
    if r < 0.45:            # cnfuzz-style structured (variable-reuse clusters)
        return gen_structured(rng)
    if r < 0.70:            # mutation of a base instance
        return gen_mutation(rng)
    return gen_fuzzsat(rng)  # FuzzSAT-style Tseitin circuit


def write_dimacs(path: str, nvars: int, clauses) -> None:
    with open(path, "w") as f:
        f.write(f"p cnf {nvars} {len(clauses)}\n")
        for c in clauses:
            f.write(" ".join(str(l) for l in c) + " 0\n" if c else "0\n")


def parse_dimacs(path: str):
    nvars = 0
    clauses = []
    cur = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("c"):
                continue
            if line.startswith("p"):
                parts = line.split()
                nvars = int(parts[2])
                continue
            for tok in line.split():
                lit = int(tok)
                if lit == 0:
                    clauses.append(cur)
                    cur = []
                else:
                    cur.append(lit)
    if cur:
        clauses.append(cur)
    return nvars, clauses
