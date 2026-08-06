#!/usr/bin/env python3
"""Satisfiability-preserving permutation of a DIMACS CNF.

Three independent renamings, all of which preserve SAT/UNSAT exactly:
  - variable renaming (a bijection on 1..n)
  - literal polarity flip per variable (v -> -v consistently everywhere)
  - clause order shuffle, and literal order shuffle inside each clause

This is the one perturbation knob that is mechanically inert for EVERY solver,
so it measures the instance's intrinsic hardness distribution rather than any
one solver's configuration.  seed 0 is the identity (byte-shape-normalised
original) so it can be used as the control.

Usage: permute_cnf.py <in.cnf> <out.cnf> <seed>
"""
import random, sys

def main():
    src, dst, seed = sys.argv[1], sys.argv[2], int(sys.argv[3])
    clauses = []
    nvars = 0
    with open(src) as fh:
        cur = []
        for line in fh:
            if not line or line[0] in "c%":
                continue
            if line[0] == "p":
                nvars = int(line.split()[2])
                continue
            for tok in line.split():
                v = int(tok)
                if v == 0:
                    if cur:
                        clauses.append(cur)
                    cur = []
                else:
                    cur.append(v)
        if cur:
            clauses.append(cur)

    rng = random.Random(seed)
    if seed == 0:
        perm = list(range(1, nvars + 1))
        flip = [False] * (nvars + 1)
    else:
        perm = list(range(1, nvars + 1))
        rng.shuffle(perm)
        flip = [False] + [rng.random() < 0.5 for _ in range(nvars)]
    # rename[v] = new positive literal for old variable v
    rename = [0] * (nvars + 1)
    for i, v in enumerate(perm):
        rename[v] = i + 1

    out = []
    for cl in clauses:
        nc = []
        for lit in cl:
            v = abs(lit)
            nv = rename[v]
            neg = (lit < 0) != flip[v]
            nc.append(-nv if neg else nv)
        if seed != 0:
            rng.shuffle(nc)
        out.append(nc)
    if seed != 0:
        rng.shuffle(out)

    with open(dst, "w") as fh:
        fh.write("p cnf %d %d\n" % (nvars, len(out)))
        for cl in out:
            fh.write(" ".join(str(x) for x in cl))
            fh.write(" 0\n")

if __name__ == "__main__":
    main()
