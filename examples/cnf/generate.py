#!/usr/bin/env python3
"""Regenerate the synthetic example formulas in this directory.

    python3 examples/cnf/generate.py

Each formula is deterministic, so rerunning the script reproduces the files
byte for byte.
"""
import argparse
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent


def write(name, nvars, clauses, comment):
    lines = [f"c {line}" for line in comment] + [f"p cnf {nvars} {len(clauses)}"]
    lines += [" ".join(map(str, clause + [0])) for clause in clauses]
    (HERE / name).write_text("\n".join(lines) + "\n")


def random3sat(nvars, nclauses, seed):
    rng = random.Random(seed)
    clauses = []
    for _ in range(nclauses):
        variables = rng.sample(range(1, nvars + 1), 3)
        clauses.append([v if rng.random() < 0.5 else -v for v in variables])
    return clauses


def at_most_one(literals, next_variable, encoding):
    """Return clauses and next free identity; all variants encode AMO exactly."""
    clauses = []
    def fresh():
        nonlocal next_variable
        v = next_variable
        next_variable += 1
        return v
    def pairwise(xs):
        clauses.extend([-a, -b] for i, a in enumerate(xs) for b in xs[i+1:])
    xs = list(literals)
    if len(xs) < 2:
        return clauses, next_variable
    if encoding == 'pairwise':
        pairwise(xs)
    elif encoding == 'sequential':
        chain = [fresh() for _ in xs[:-1]]
        clauses.append([-xs[0], chain[0]])
        for i in range(1, len(xs)-1):
            clauses.extend(([-xs[i], chain[i]], [-chain[i-1], chain[i]],
                            [-xs[i], -chain[i-1]]))
        clauses.append([-xs[-1], -chain[-1]])
    elif encoding == 'bitwise':
        bits = [fresh() for _ in range((len(xs)-1).bit_length())]
        for i, x in enumerate(xs):
            clauses.extend([-x, bit if i >> j & 1 else -bit]
                           for j, bit in enumerate(bits))
    elif encoding == 'commander':
        while len(xs) > 3:
            commanders = []
            for start in range(0, len(xs), 3):
                group = xs[start:start+3]
                c = fresh()
                commanders.append(c)
                pairwise(group)
                clauses.extend([-x, c] for x in group)
            xs = commanders
        pairwise(xs)
    elif encoding == 'product':
        width = 1
        while width*width < len(xs):
            width += 1
        row = [fresh() for _ in range((len(xs)+width-1)//width)]
        column = [fresh() for _ in range(width)]
        for i, x in enumerate(xs):
            clauses.extend(([-x, row[i//width]], [-x, column[i%width]]))
        pairwise(row)
        pairwise(column)
    else:
        raise ValueError('unknown AMO encoding: '+encoding)
    return clauses, next_variable


def pigeonhole(pigeons, holes, amo='pairwise', functional=False):
    var = lambda p, h: p * holes + h + 1
    clauses = [[var(p, h) for h in range(holes)] for p in range(pigeons)]
    next_variable = pigeons*holes+1
    for h in range(holes):
        rows, next_variable = at_most_one([var(p, h) for p in range(pigeons)],
                                         next_variable, amo)
        clauses.extend(rows)
    if functional:
        for p in range(pigeons):
            rows, next_variable = at_most_one([var(p, h) for h in range(holes)],
                                             next_variable, 'pairwise')
            clauses.extend(rows)
    return next_variable-1, clauses


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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pigeonhole', nargs=2, type=int,
                        metavar=('PIGEONS', 'HOLES'))
    parser.add_argument('--output', type=Path)
    parser.add_argument('--amo', choices=['pairwise', 'sequential', 'bitwise', 'commander', 'product'], default='pairwise')
    parser.add_argument('--functional', action='store_true')
    args = parser.parse_args()
    if args.pigeonhole is not None:
        pigeons, holes = args.pigeonhole
        if pigeons < 1 or holes < 1 or args.output is None:
            parser.error('--pigeonhole needs positive dimensions and --output')
        n, clauses = pigeonhole(pigeons, holes, args.amo, args.functional)
        write(args.output.resolve(), n, clauses,
              [f'Pigeonhole principle: {pigeons} pigeons, {holes} holes.'])
        return
    if args.output is not None:
        parser.error('--output requires --pigeonhole')
    write("random3sat-sat.cnf", 200, random3sat(200, 800, seed=1),
          ["Uniform random 3-SAT, 200 variables, 800 clauses (ratio 4.0), seed 1.",
           "Satisfiable."])
    n, clauses = pigeonhole(9, 8)
    write("pigeonhole-9-8.cnf", n, clauses,
          ["Pigeonhole principle: 9 pigeons, 8 holes. Unsatisfiable.",
           "Exponentially hard for resolution; symmetry breaking makes it easy."])
    n, clauses = tseitin(40, 3, seed=1)
    write("tseitin-40.cnf", n, clauses,
          ["Tseitin parity formula on a random 3-regular graph with 40 vertices,",
           "seed 1, one odd vertex. Unsatisfiable by a parity argument."])


if __name__ == "__main__":
    main()
