#!/usr/bin/env python3
"""Generate the Tier A benchmark set (plan §8.1) and freeze its list.

~200 seeded, deterministic instances into benchmarks/cache/tierA/:
  - random 3-SAT at the phase transition (m/n = 4.26), n = 50..300
  - under-constrained (m/n = 3.0, easy SAT) and over-constrained
    (m/n = 5.5, easy UNSAT) random 3-SAT
  - pigeonhole PHP(5..8) (UNSAT, deep resolution)
  - pyramid pebbling formulas (UNSAT, structured Horn-ish)
  - parity/XOR chains (SAT/UNSAT by chain parity)
  - random graph k-coloring encodings

Status is established once with Kissat (30 s cap) and written to
benchmarks/tiers/A.list (`tierA/<name>.cnf STATUS ref_seconds`), which is
then frozen in git. Instances themselves stay in the untracked cache and are
bit-reproducible from this script (fixed master seed).
"""

import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "tools", "fuzz"))
from generators import random_ksat, write_dimacs  # noqa: E402

import random  # noqa: E402

CACHE = os.path.join(ROOT, "benchmarks", "cache", "tierA")
LIST = os.path.join(ROOT, "benchmarks", "tiers", "A.list")
KISSAT = os.path.join(ROOT, "tools", "solvers", "bin", "kissat")
MASTER_SEED = 20260709
TIMEOUT = 30.0


def php(holes):
    """Pigeonhole: holes+1 pigeons, UNSAT."""
    pigeons = holes + 1
    var = lambda i, j: (i - 1) * holes + j
    clauses = [[var(i, j) for j in range(1, holes + 1)]
               for i in range(1, pigeons + 1)]
    for j in range(1, holes + 1):
        for i1 in range(1, pigeons + 1):
            for i2 in range(i1 + 1, pigeons + 1):
                clauses.append([-var(i1, j), -var(i2, j)])
    return pigeons * holes, clauses


def pebbling(height):
    """Pyramid pebbling with 2-variable nodes (OR-pebbling), UNSAT."""
    # nodes[(r,c)] -> (v1, v2); row 0 = sources, row height = sink
    idx = {}
    n = 0
    for r in range(height + 1):
        for c in range(height - r + 1):
            idx[(r, c)] = (n + 1, n + 2)
            n += 2
    clauses = []
    for c in range(height + 1):        # sources: v1 or v2
        clauses.append(list(idx[(0, c)]))
    for r in range(1, height + 1):     # (a1|a2) & (b1|b2) -> (v1|v2)
        for c in range(height - r + 1):
            a, b, v = idx[(r - 1, c)], idx[(r - 1, c + 1)], idx[(r, c)]
            for x in a:
                for y in b:
                    clauses.append([-x, -y, v[0], v[1]])
    s = idx[(height, 0)]               # sink false
    clauses.append([-s[0]])
    clauses.append([-s[1]])
    return n, clauses


def xor_chain(rng, n, unsat):
    """Cyclic XOR chain x_i ^ x_{i+1} = c_i; UNSAT iff parity of c is odd."""
    cs = [rng.randrange(2) for _ in range(n)]
    if (sum(cs) % 2 == 1) != unsat:
        cs[0] ^= 1
    clauses = []
    for i in range(n):
        a, b, c = i + 1, (i + 1) % n + 1, cs[i]
        if c:   # a ^ b = 1  <=>  (a|b) & (-a|-b)
            clauses += [[a, b], [-a, -b]]
        else:   # a ^ b = 0  <=>  (a|-b) & (-a|b)
            clauses += [[a, -b], [-a, b]]
    return n, clauses


def coloring(rng, n, k, p):
    """Random G(n,p) k-coloring; status established by kissat."""
    var = lambda v, c: (v - 1) * k + c
    clauses = [[var(v, c) for c in range(1, k + 1)] for v in range(1, n + 1)]
    for u in range(1, n + 1):
        for v in range(u + 1, n + 1):
            if rng.random() < p:
                for c in range(1, k + 1):
                    clauses.append([-var(u, c), -var(v, c)])
    return n * k, clauses


def build_instances():
    """Yields (name, nvars, clauses); deterministic in MASTER_SEED."""
    out = []
    # 100 phase-transition 3-SAT
    for i in range(100):
        rng = random.Random(MASTER_SEED + i)
        n = rng.choice(range(50, 301, 10))
        m = round(4.26 * n)
        out.append((f"tr-n{n}-s{i:03d}", *random_ksat(rng, n, m, 3)))
    # 25 under-constrained (SAT), 25 over-constrained (UNSAT)
    for i in range(25):
        rng = random.Random(MASTER_SEED + 1000 + i)
        n = rng.choice(range(50, 301, 10))
        out.append((f"under-n{n}-s{i:03d}", *random_ksat(rng, n, 3 * n, 3)))
    for i in range(25):
        rng = random.Random(MASTER_SEED + 2000 + i)
        n = rng.choice(range(50, 301, 10))
        out.append((f"over-n{n}-s{i:03d}",
                    *random_ksat(rng, n, round(5.5 * n), 3)))
    # crafted families
    for h in range(5, 9):
        out.append((f"php-{h}", *php(h)))
    for h in range(4, 16):
        out.append((f"pebble-{h}", *pebbling(h)))
    for i in range(20):
        rng = random.Random(MASTER_SEED + 3000 + i)
        n = rng.randint(20, 200)
        out.append((f"xor-n{n}-s{i:03d}", *xor_chain(rng, n, i % 2 == 1)))
    for i in range(14):
        rng = random.Random(MASTER_SEED + 4000 + i)
        n = rng.randint(12, 40)
        k = 3 + (i % 2)
        p = rng.uniform(0.3, 0.6)
        out.append((f"color-n{n}-k{k}-s{i:03d}", *coloring(rng, n, k, p)))
    return out


EXPECTED = {"php": "UNSAT", "pebble": "UNSAT", "under": "SAT"}


def main() -> int:
    if not os.path.exists(KISSAT):
        sys.exit("build baselines first: tools/solvers/setup.sh")
    os.makedirs(CACHE, exist_ok=True)
    os.makedirs(os.path.dirname(LIST), exist_ok=True)
    rows = []
    t_total = 0.0
    for name, nvars, clauses in build_instances():
        path = os.path.join(CACHE, f"{name}.cnf")
        write_dimacs(path, nvars, clauses)
        t0 = time.monotonic()
        try:
            r = subprocess.run([KISSAT, "-q", path], capture_output=True,
                               text=True, timeout=TIMEOUT)
            status = {10: "SAT", 20: "UNSAT"}.get(r.returncode, "UNKNOWN")
        except subprocess.TimeoutExpired:
            status = "UNKNOWN"
        dt = time.monotonic() - t0
        t_total += dt
        fam = name.split("-")[0]
        want = EXPECTED.get(fam) or ("SAT" if fam == "xor" and
                                     int(name.split("s")[-1]) % 2 == 0 else
                                     "UNSAT" if fam == "xor" else None)
        if status == "UNKNOWN":
            sys.exit(f"error: kissat did not solve {name} within {TIMEOUT}s "
                     f"— Tier A must be trivial; drop or shrink the instance")
        if want and status != want:
            sys.exit(f"error: encoding bug — {name} expected {want}, "
                     f"kissat says {status}")
        rows.append((f"tierA/{name}.cnf", status, dt))
        print(f"{name:24s} {status:6s} {dt:6.2f}s", flush=True)

    nsat = sum(1 for _, s, _ in rows if s == "SAT")
    with open(LIST, "w") as f:
        f.write(f"# Tier A — frozen {time.strftime('%Y-%m-%d')}; "
                f"master seed {MASTER_SEED}; generated by "
                f"tools/bench/gen_tier_a.py (bit-reproducible)\n"
                f"# {len(rows)} instances: {nsat} SAT / "
                f"{len(rows) - nsat} UNSAT; kissat total "
                f"{t_total:.1f}s\n")
        for rel, status, dt in rows:
            f.write(f"{rel} {status} {dt:.3f}\n")
    print(f"\nTier A: {len(rows)} instances ({nsat} SAT / "
          f"{len(rows) - nsat} UNSAT), kissat total {t_total:.1f}s "
          f"-> {LIST}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
