#!/usr/bin/env python3
"""Choose the reduced and smoke instance sets from the paper's archived runs.

    python3 paper/run/select_subsets.py [--check]

Writes paper/benchmarks/subsets/<experiment>-{reduced,smoke}.txt (with --check:
compares with the committed files instead). The reduced sets are what an
artifact evaluation can afford on a 4-core, 16 GB virtual machine (about five
hours with three runs at a time); the rules below decide them from the
per-instance results of the paper's runs (paper/data/, paper/logs/), so they
can be audited and changed.

Tier D (Table 3, Figures 3 and 4) and the milestones (Table 6). The pool is
every formula of SC24-26 that
  - is at most 20 MB compressed (paper/benchmarks/tierd-sizes.tsv),
  - every solver solved within 3 GB of resident memory, or did not solve,
  - some solver solved and checked within 300 s in the paper's runs, and
  - not every solver solved within 20 s (those formulas show nothing).
From the pool, PER_YEAR formulas of each competition year are drawn with a
fixed seed many times, and the draw kept is the one whose fraction of checked
solves per solver and year (at the reduced limits: 600 s solving, 600 s
checking), and whose share of SAT formulas, is closest to the pool's, among
the draws whose estimated cost fits the budget. The reduced set thus mirrors
the pool, on which the paper's ranking holds at the reduced limits; it is too
small to be a random sample with the same power. The smoke set is the smallest
SAT and the smallest UNSAT formula that every solver solved within 10 s (and
checked within 20 s) using at most 1 GB.

Interface (Figure 5). All 200 Tier A formulas (each under 30 s), and of each
other set, up to 15 formulas that both builds solved within 120 s in the
paper's runs, spread evenly over that range of solving times.

Cruxmiter (Table 4). The three seeds (of ten) with the smallest total solving
plus checking time over the three configurations in the paper's runs.

Chvatal (Table 5, Figure 6). The two subproblems (of 23) with the smallest
total solving plus checking time over the four configurations.
"""
from __future__ import annotations

import argparse
import collections
import csv
import json
import random
import statistics
import sys
import tarfile
from pathlib import Path

PAPER = Path(__file__).resolve().parents[1]
OUT = PAPER / "benchmarks" / "subsets"
SOLVERS = ("Tactix", "Satsuma+Kissat", "LymphoSAT", "Kissat 4.0.4", "Kissat-sc2024", "AE-Kissat-MAB")
YEARS = ("SC2024", "SC2025", "SC2026")
SOLVE, CHECK = 600.0, 600.0       # the reduced Tier D limits (experiments.py)
PER_YEAR = 5
BUDGET = 26000.0                  # estimated solver + checker seconds for Tier D
SEED, DRAWS = 2027, 20000


def read(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


# --- Tier D ----------------------------------------------------------------------

def solve_memory() -> dict[tuple[str, str], int]:
    """Peak resident memory (MB) of every solver run, from the raw campaigns."""
    names = {"tactix-0f7c10174": "Tactix", "satsuma-kissat": "Satsuma+Kissat",
             "kissat-sc2024": "Kissat-sc2024", "kissat-4.0.4": "Kissat 4.0.4",
             "ae-kissat-mab": "AE-Kissat-MAB", "lymphosat": "LymphoSAT"}
    memory = {}
    for campaign, solver in (("20261010-tierd-fastel", "Tactix"),
                             ("sc24-26-satsuma-fixed64-32g-8h", "Satsuma+Kissat"),
                             ("sc24-26-32g-5000s", None)):
        with tarfile.open(PAPER / "logs/tierd/raw" / f"{campaign}.tar.xz") as archive:
            for member in archive:
                name = member.name.split("/", 1)[-1]
                if not member.isfile() or not (name.startswith("results/") or name == "results.json"):
                    continue
                data = json.loads(archive.extractfile(member).read())
                for record in data["results"] if name == "results.json" else [data]:
                    configuration = solver or names.get(record.get("solver"))
                    if configuration is None or (solver is None and configuration == "Satsuma+Kissat"):
                        continue
                    rss = (record.get("solve") or {}).get("peak_rss_kb")
                    if rss:
                        memory[(configuration, record["instance"])] = rss // 1024
    return memory


def tierd() -> tuple[list[str], list[str]]:
    rows = read(PAPER / "data/tierd-times.csv")
    by = collections.defaultdict(dict)
    for row in rows:
        by[row["instance"]][row["configuration"]] = row
    size = {r["hash"]: int(r["bytes"]) for r in csv.DictReader((PAPER / "benchmarks/tierd-sizes.tsv").open(),
                                                                delimiter="\t")}
    memory = solve_memory()

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
        if not any(checked(arms[s], 300, 300) for s in SOLVERS):
            continue
        if all(checked(arms[s], 20, 20) for s in SOLVERS):
            continue
        pool.append(instance)
    target = {y: [statistics.mean(checked(by[i][s]) for i in pool if year(i) == y) for s in SOLVERS]
              for y in YEARS}
    sat = {y: statistics.mean(is_sat(i) for i in pool if year(i) == y) for y in YEARS}
    rng = random.Random(SEED)
    best = None
    for _ in range(DRAWS):
        draw = [i for y in YEARS for i in rng.sample([i for i in pool if year(i) == y], PER_YEAR)]
        if sum(cost(by[i][s]) for i in draw for s in SOLVERS) > BUDGET:
            continue
        distance = 0.0
        for y in YEARS:
            mine = [i for i in draw if year(i) == y]
            distance += sum((statistics.mean(checked(by[i][s]) for i in mine) - t) ** 2
                            for s, t in zip(SOLVERS, target[y]))
            distance += (statistics.mean(is_sat(i) for i in mine) - sat[y]) ** 2
        if best is None or distance < best[0]:
            best = (distance, draw)
    reduced = sorted(best[1], key=lambda i: (year(i), i))
    smoke = []
    for want_sat in (True, False):
        fast = [i for i in sorted(by, key=lambda i: size[Path(i).name.split("-")[0]])
                if year(i) and is_sat(i) == want_sat and all(checked(by[i][s], 10, 20) for s in SOLVERS)
                and all(memory.get((s, i), 0) <= 1000 for s in SOLVERS)]
        smoke += fast[:1]
    counts = collections.Counter(s for i in reduced for s in SOLVERS if checked(by[i][s]))
    print(f"tierd: pool {len(pool)}, reduced {len(reduced)}, estimated cost "
          f"{sum(cost(by[i][s]) for i in reduced for s in SOLVERS):.0f} s; checked solves at the "
          f"reduced limits in the paper's runs: " + ", ".join(f"{s} {counts[s]}" for s in SOLVERS))
    return [Path(i).name for i in reduced], [Path(i).name for i in smoke]


# --- interface, cruxmiter, Chvatal --------------------------------------------------

def interface() -> tuple[list[str], list[str]]:
    rows = read(PAPER / "data/interface-overhead.csv")
    chosen = [r["instance"] for r in rows if r["set"] == "Tier A"]
    smoke = []
    for name in ("B28+C5", "Rank 1-50", "WV100"):
        candidates = sorted((r for r in rows if r["set"] == name
                             and max(float(r["cpp_seconds"]), float(r["c_api_seconds"])) <= 120),
                            key=lambda r: float(r["cpp_seconds"]))
        count = min(15, len(candidates))
        picks = [candidates[round(k * (len(candidates) - 1) / max(1, count - 1))] for k in range(count)]
        chosen += list(dict.fromkeys(r["instance"] for r in picks))
        smoke.append(candidates[0]["instance"])
    tier_a = sorted((r for r in rows if r["set"] == "Tier A"), key=lambda r: -float(r["cpp_seconds"]))
    return chosen, [tier_a[0]["instance"], *smoke[:1]]


def cruxmiter() -> tuple[list[str], list[str]]:
    total = collections.Counter()
    for row in read(PAPER / "data/crux-results.csv"):
        total[int(row["seed"])] += float(row["solve_seconds"]) + float(row["check_seconds"])
    seeds = sorted(total, key=total.get)[:3]
    return [f"cruxmiter32seed{s}.cnf" for s in sorted(seeds)], [f"cruxmiter32seed{seeds[0]}.cnf"]


def chvatal() -> tuple[list[str], list[str]]:
    total = collections.Counter()
    for row in read(PAPER / "data/chvatal-results.csv"):
        if row["configuration"] in ("Chvatal Tactix", "Default Tactix", "Kissat 4.0.4", "Satsuma+Kissat"):
            total[row["node"]] += float(row["solve_seconds"]) + float(row["check_seconds"] or 0)
    nodes = sorted(total, key=total.get)[:2]
    return [f"n7-{n}.cnf.xz" for n in sorted(nodes)], [f"n7-{nodes[0]}.cnf.xz"]


HEADER = {
    "tierd": "Tier D reduced set (also the milestones'): see paper/run/select_subsets.py",
    "interface": "Interface reduced set: all of Tier A, up to 15 formulas per other set solved within 120 s",
    "cruxmiter": "Cruxmiter reduced set: the three cheapest seeds in the paper's runs",
    "chvatal": "Chvatal reduced set: the two cheapest of the 23 subproblems in the paper's runs",
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
            text = f"# {HEADER[name]}" + (" (smoke: a quick part of it)" if kind == "smoke" else "") + "\n"
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
