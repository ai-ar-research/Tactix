#!/usr/bin/env python3
"""Choose the reduced and smoke instance sets from the paper's archived runs.

    python3 paper/run/select_subsets.py [--check]

Writes paper/benchmarks/subsets/<experiment>-{reduced,smoke}.txt (with --check:
compares with the committed files instead). The reduced sets fit 4 cores and
16 GB in about four hours with three runs at a time; the rules below decide
them from the per-instance results of the paper's runs (paper/data/,
paper/logs/).

tierd (Table 3, Figures 3 and 4) and the milestones (Table 6). The pool is
every formula of SC24-26 (of one competition year) that
  - is at most 20 MB compressed (paper/benchmarks/gbd-sizes.tsv),
  - every solver solved within 3 GB of resident memory, or did not solve,
  - some solver solved and checked within the reduced limits (600 s solving,
    600 s checking) in the paper's runs, and
  - not every solver solved within 20 s.
From the pool, 20 formulas (7 of SC24, 7 of SC25, 6 of SC26) from 20 different
benchmark families (paper/benchmarks/tierd-families.tsv) are drawn at random
with a fixed seed; the first draw whose estimated cost (the paper's solving and
checking times, cut at the limits) fits two hours with three runs at a time is
kept. Four drawn formulas were then replaced (REPLACE below) after a test run
of the set in the reviewer environment differed from the paper's runs: on two,
the paper's checkers took 1,000 to 4,500 s, beyond the 600 s checking limit; on
two satisfiable ones, solving times differed by a factor of 2 to 20 (how soon a
satisfying assignment is found varies between machines). Each replacement is a
pool formula of the same year and answer, from a family not yet in the set,
that every solver answered within 130 s and checked within 350 s in the paper's
runs, and whose test-run times were 0.67 to 0.95 times the paper's. The smoke set is the smallest SAT and the smallest UNSAT formula that
every solver solved within 10 s (and checked within 20 s) using at most 1 GB.

Interface (Figure 5). The 20 Tier A formulas (of 200, each under 30 s) that
took longest in the paper's runs, and of each other set, 5 formulas of at most
20 MB compressed that both builds solved in 10 to 250 s in the paper's runs,
spread evenly over that range of solving times (most formulas of these sets
take under a second, which measures start-up rather than solving).

Cruxmiter (Table 4). The seed (of ten) with the smallest total solving plus
checking time over the three configurations in the paper's runs (seed 7).

Chvatal (Table 5, Figure 6). The subproblem (of 23) with the smallest total
solving plus checking time over the four configurations.
"""
from __future__ import annotations

import argparse
import collections
import csv
import json
import random
import sys
import tarfile
from pathlib import Path

PAPER = Path(__file__).resolve().parents[1]
OUT = PAPER / "benchmarks" / "subsets"
SOLVERS = ("Tactix", "Satsuma+Kissat", "LymphoSAT", "Kissat 4.0.4", "Kissat-sc2024", "AE-Kissat-MAB")
YEARS = ("SC2024", "SC2025", "SC2026")
SOLVE, CHECK = 600.0, 600.0       # the reduced tierd limits (experiments.py)
PER_YEAR = {"SC2024": 7, "SC2025": 7, "SC2026": 6}   # 20 formulas
BUDGET = 2 * 3600 * 3             # estimated solver + checker seconds: 2 h with 3 runs at a time
SEED = 2027
REPLACE = {  # drawn formula -> replacement (see above)
    "1ea68d0008f01c50a1e4fec77dd0775e-REGRandom-K4-L1-Seed30.sanitized.cnf.xz":
        "bd8bc25be2b36c64b38459c17e815814-pcmax-scheduling-m11-1517-6802-UNSAT.sanitized.cnf.xz",
    "b68ae7866df5e852c3acb97a46da2faf-circuit_48in64out_with_1000gates_4in4out_dist128_seed4.sanitized.cnf.xz":
        "79b9e24dd9af185dbec18c9b0a32b1e2-g2-slp-synthesis-aes-top30.cnf.xz",
    "c033577c6f5f9a1545537518af964305-reconf10_73_queen13_2.cnf.xz":
        "2241a1fb5bdbac0c1a3c1980ed8b332d-crusti_g2io_225_0.1_31_25.normalised.cnf.xz",
    "166e1e5a9f63fcf94ddae8533fa2a090-mp1-squ_any_s09x07_c27_abix_UNS.cnf.xz":
        "ae48df6541d190c096b1e5ba22d1b534-korf-17.cnf.xz",
}


def read(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


# --- tierd ----------------------------------------------------------------------

def solve_memory() -> dict[tuple[str, str], int]:
    """Peak resident memory (MB) of every tierd solver run, from the paper's logs."""
    memory = {}
    for path in sorted((PAPER / "logs/tierd").glob("*.tar.xz")):
        with tarfile.open(path) as archive:
            for member in archive:
                if member.isfile() and "/runs/" in member.name:
                    entry = json.loads(archive.extractfile(member).read())
                    rss = (entry["record"].get("solve") or {}).get("peak_rss_kb")
                    if rss:
                        memory[(entry["configuration"], entry["instance"])] = rss // 1024
    return memory


def tierd() -> tuple[list[str], list[str]]:
    rows = read(PAPER / "data/tierd-times.csv")
    by = collections.defaultdict(dict)
    for row in rows:
        by[row["instance"]][row["configuration"]] = row
    size = {r["hash"]: int(r["bytes"]) for r in csv.DictReader((PAPER / "benchmarks/gbd-sizes.tsv").open(),
                                                                delimiter="\t")}
    memory = solve_memory()
    lines = [line for line in (PAPER / "benchmarks/tierd-families.tsv").open() if not line.startswith("#")]
    family = {r["hash"]: r["family"] for r in csv.DictReader(lines, delimiter="\t")}

    def checked(row, solve=SOLVE, check=CHECK):
        return (row["accepted"] == "1" and float(row["solve_seconds"]) <= solve
                and (row["verdict"] == "SAT" or float(row["check_seconds"] or "inf") <= check))

    def cost(row):
        seconds = min(float(row["solve_seconds"]), SOLVE)
        if row["verdict"] == "UNSAT" and float(row["solve_seconds"]) <= SOLVE:
            seconds += min(float(row["check_seconds"] or CHECK), CHECK)
        return seconds

    def year(instance):
        row = by[instance]["Tactix"]
        member = [y for y in YEARS if row[y] == "1"]
        return member[0] if len(member) == 1 else None  # one year only, so years stay disjoint

    def is_sat(instance):
        return any(by[instance][s]["verdict"] == "SAT" for s in SOLVERS)

    pool = []
    for instance, arms in sorted(by.items()):
        content_hash = Path(instance).name.split("-")[0]
        if size[content_hash] > 20e6 or year(instance) is None:
            continue
        if any(memory.get((s, instance), 0) > 3000 for s in SOLVERS):
            continue
        if not any(checked(arms[s]) for s in SOLVERS):
            continue
        if all(checked(arms[s], 20, 20) for s in SOLVERS):
            continue
        pool.append(instance)
    price = {i: sum(cost(by[i][s]) for s in SOLVERS) for i in pool}
    by_year = {y: [i for i in pool if year(i) == y] for y in YEARS}

    def fam(instance):
        return family[Path(instance).name.split("-")[0]]

    def draw():
        """PER_YEAR formulas of each year, no two from the same family."""
        chosen, used = [], set()
        for y in YEARS:
            order = list(by_year[y])
            rng.shuffle(order)
            picks = []
            for instance in order:
                if fam(instance) not in used and len(picks) < PER_YEAR[y]:
                    picks.append(instance)
                    used.add(fam(instance))
            chosen += picks
        return chosen

    rng = random.Random(SEED)
    reduced = draw()
    while sum(price[i] for i in reduced) > BUDGET:
        reduced = draw()
    by_name = {Path(i).name: i for i in pool}
    reduced = [by_name.get(REPLACE.get(Path(i).name), i) for i in reduced]
    assert all(Path(i).name not in REPLACE for i in reduced), "a replacement is not in the pool"
    assert len({fam(i) for i in reduced}) == len(reduced), "two formulas of one family"
    reduced = sorted(reduced, key=lambda i: (year(i), i))
    smoke = []
    for want_sat in (True, False):
        fast = [i for i in sorted(by, key=lambda i: size[Path(i).name.split("-")[0]])
                if year(i) and is_sat(i) == want_sat and all(checked(by[i][s], 10, 20) for s in SOLVERS)
                and all(memory.get((s, i), 0) <= 1000 for s in SOLVERS)]
        smoke += fast[:1]
    print(f"tierd: pool {len(pool)}, reduced {len(reduced)} from {len({fam(i) for i in reduced})} "
          f"families, estimated cost {sum(price[i] for i in reduced):.0f} s")
    return [Path(i).name for i in reduced], [Path(i).name for i in smoke]


# --- interface, cruxmiter, Chvatal --------------------------------------------------

def interface() -> tuple[list[str], list[str]]:
    rows = read(PAPER / "data/interface-overhead.csv")
    size = {r["hash"]: int(r["bytes"]) for r in csv.DictReader((PAPER / "benchmarks/gbd-sizes.tsv").open(),
                                                                delimiter="\t")}
    content_hash = {}
    for sources in PAPER.glob("benchmarks/interface-*.sources"):
        for line in sources.read_text().splitlines():
            if line.strip() and not line.startswith("#"):
                fields = line.split()
                content_hash[fields[1]] = fields[0]
    seconds = lambda r: max(float(r["cpp_seconds"]), float(r["c_api_seconds"]))
    tier_a = sorted((r for r in rows if r["set"] == "Tier A"), key=lambda r: -float(r["cpp_seconds"]))
    chosen = sorted(r["instance"] for r in tier_a[:20])
    smoke = []
    for name in ("B28+C5", "Rank 1-50", "WV100"):
        small = sorted((r for r in rows if r["set"] == name and size[content_hash[r["instance"]]] <= 20e6),
                       key=lambda r: float(r["cpp_seconds"]))
        candidates = sorted((r for r in small if 10 <= seconds(r) <= 250), key=seconds)
        count = min(5, len(candidates))
        picks = [candidates[round(k * (len(candidates) - 1) / max(1, count - 1))] for k in range(count)]
        chosen += list(dict.fromkeys(r["instance"] for r in picks))
        smoke.append(small[0]["instance"])
    return chosen, [tier_a[0]["instance"], *smoke[:1]]


def cruxmiter() -> tuple[list[str], list[str]]:
    total = collections.Counter()
    for row in read(PAPER / "data/crux-results.csv"):
        total[int(row["seed"])] += float(row["solve_seconds"]) + float(row["check_seconds"])
    seeds = sorted(total, key=total.get)[:1]
    return [f"cruxmiter32seed{s}.cnf" for s in sorted(seeds)], [f"cruxmiter32seed{seeds[0]}.cnf"]


def chvatal() -> tuple[list[str], list[str]]:
    total = collections.Counter()
    for row in read(PAPER / "data/chvatal-results.csv"):
        if row["configuration"] in ("Chvatal Tactix", "Default Tactix", "Kissat 4.0.4", "Satsuma+Kissat"):
            total[row["node"]] += float(row["solve_seconds"]) + float(row["check_seconds"] or 0)
    nodes = sorted(total, key=total.get)[:1]
    return [f"n7-{n}.cnf.xz" for n in sorted(nodes)], [f"n7-{nodes[0]}.cnf.xz"]


HEADER = {
    "tierd": "tierd reduced set (also the milestones'): 20 formulas from different families",
    "interface": "Interface reduced set: the 20 longest Tier A formulas, 5 per other set solved in 10 to 250 s",
    "cruxmiter": "Cruxmiter reduced set: the cheapest seed in the paper's runs",
    "chvatal": "Chvatal reduced set: the cheapest of the 23 subproblems in the paper's runs",
}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="compare with the committed files")
    args = parser.parse_args()
    different = 0
    for name, choose in (("tierd", tierd), ("interface", interface), ("cruxmiter", cruxmiter),
                         ("chvatal", chvatal)):
        reduced, smoke = choose()
        for kind, items in (("reduced", reduced), ("smoke", smoke)):
            text = (f"# {HEADER[name]}" if kind == "reduced"
                    else f"# {name} smoke set: see paper/run/select_subsets.py") + "\n"
            text += "".join(f"{item}\n" for item in items)
            path = OUT / f"{name}-{kind}.txt"
            if args.check:
                same = path.exists() and path.read_text() == text
                different += not same
                print(f"{'identical' if same else 'DIFFERENT'}  {path.name}")
            else:
                path.write_text(text)
                print(f"wrote {path.relative_to(PAPER.parent)} ({len(items)} instances)")
    sys.exit(1 if different else 0)


if __name__ == "__main__":
    main()
