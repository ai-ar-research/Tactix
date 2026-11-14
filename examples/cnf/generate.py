#!/usr/bin/env python3
"""Write the synthetic example formulas into DIR (default: this directory).

    python3 examples/cnf/generate.py [DIR]

The build runs this into build/<preset>/examples/cnf. Each formula is
deterministic, so every run writes the same bytes.
"""
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent


def write(path, nvars, clauses, comment):
    lines = [f"c {line}" for line in comment] + [f"p cnf {nvars} {len(clauses)}"]
    lines += [" ".join(map(str, clause + [0])) for clause in clauses]
    path.write_text("\n".join(lines) + "\n")


def random3sat(nvars, nclauses, seed):
    rng = random.Random(seed)
    clauses = []
    for _ in range(nclauses):
        variables = rng.sample(range(1, nvars + 1), 3)
        clauses.append([v if rng.random() < 0.5 else -v for v in variables])
    return clauses


def pigeonhole(pigeons, holes):
    var = lambda p, h: p * holes + h + 1
    clauses = [[var(p, h) for h in range(holes)] for p in range(pigeons)]
    for h in range(holes):
        for p in range(pigeons):
            for q in range(p + 1, pigeons):
                clauses.append([-var(p, h), -var(q, h)])
    return pigeons * holes, clauses


def tseitin(vertices, degree, seed):
    """Tseitin formula of a random regular graph with odd total charge."""
    rng = random.Random(seed)
    while True:  # configuration model, retried until the graph is simple
        stubs = [v for v in range(vertices) for _ in range(degree)]
        rng.shuffle(stubs)
        edges = list(zip(stubs[::2], stubs[1::2]))
        if all(a != b for a, b in edges) and len(set(map(frozenset, edges))) == len(edges):
            break
    incident = {v: [] for v in range(vertices)}
    for index, (a, b) in enumerate(edges, start=1):
        incident[a].append(index)
        incident[b].append(index)
    clauses = []
    for v in range(vertices):
        charge = 1 if v == 0 else 0  # one odd vertex makes the total odd
        xs = incident[v]
        for mask in range(1 << len(xs)):
            # forbid every assignment whose parity differs from the charge
            if bin(mask).count("1") % 2 != charge:
                clauses.append([-x if mask >> i & 1 else x for i, x in enumerate(xs)])
    return len(edges), clauses


def main(out):
    out.mkdir(parents=True, exist_ok=True)
    write(out / "random3sat-sat.cnf", 200, random3sat(200, 800, seed=1),
          ["Uniform random 3-SAT, 200 variables, 800 clauses (ratio 4.0), seed 1.",
           "Satisfiable."])
    n, clauses = pigeonhole(9, 8)
    write(out / "pigeonhole-9-8.cnf", n, clauses,
          ["Pigeonhole principle: 9 pigeons, 8 holes. Unsatisfiable.",
           "Exponentially hard for resolution; symmetry breaking makes it easy."])
    n, clauses = tseitin(40, 3, seed=1)
    write(out / "tseitin-40.cnf", n, clauses,
          ["Tseitin parity formula on a random 3-regular graph with 40 vertices,",
           "seed 1, one odd vertex. Unsatisfiable by a parity argument."])


if __name__ == "__main__":
    main(Path(sys.argv[1]) if len(sys.argv) > 1 else HERE)
