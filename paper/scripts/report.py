#!/usr/bin/env python3
"""Print the paper's tables and headline numbers from paper/data/.

    python3 paper/scripts/report.py [--data DIR]

Each section names the table or figure it corresponds to, so the output can
be compared with the paper line by line.
"""
from __future__ import annotations

import argparse
import csv
import statistics
from pathlib import Path

PAPER = Path(__file__).resolve().parents[1]
LIMIT = 5000.0


def read(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def table(title: str, header: list[str], rows: list[list[object]]) -> None:
    cells = [header] + [[str(c) for c in row] for row in rows]
    widths = [max(len(row[i]) for row in cells) for i in range(len(header))]
    print(f"\n{title}\n")
    for index, row in enumerate(cells):
        print("  " + "  ".join(c.ljust(w) if i < 2 else c.rjust(w)
                               for i, (c, w) in enumerate(zip(row, widths))))
        if index == 0:
            print("  " + "  ".join("-" * w for w in widths))


def tierd(data: Path) -> None:
    rows = [[r["benchmark"], r["configuration"], r["solved"], r["accepted"], r["sat"], r["unsat"],
             r["unsat_verified"], f'{float(r["accepted_solve_mean_s"]):.1f}',
             f'{float(r["par2_mean_s"]):.0f}'] for r in read(data / "tierd-table.csv")]
    table("Table: results on the SC'24-26 benchmark sets (Tier D)",
          ["Set", "Configuration", "Solved", "Solved (checked)", "SAT", "UNSAT",
           "UNSAT (checked)", "Avg runtime, checked (s)", "Avg PAR-2 (s)"], rows)

    times = read(data / "tierd-times.csv")
    by = {(r["configuration"], r["instance"]): r for r in times}
    instances = sorted({r["instance"] for r in times})
    tactix_unsat = [r for r in times if r["configuration"] == "Tactix" and r["accepted"] == "1"
                    and r["verdict"] == "UNSAT"]
    ratios = [float(r["check_seconds"]) / float(r["solve_seconds"]) for r in tactix_unsat]
    print(f"\n  Tactix verified UNSAT proofs: {len(tactix_unsat)}, "
          f"{sum(int(r['proof_bytes']) for r in tactix_unsat) / 1e9:.1f} GB, "
          f"{sum(float(r['check_seconds']) for r in tactix_unsat) / 3600:.1f} h of checking; "
          f"checking slower than solving on {sum(x > 1 for x in ratios)}, "
          f"median check/solve ratio {statistics.median(ratios):.2f}")
    common = [i for i in instances
              if by[("Tactix", i)]["accepted"] == "1" and by[("Satsuma+Kissat", i)]["accepted"] == "1"]
    only_t = sum(by[("Tactix", i)]["accepted"] == "1" and by[("Satsuma+Kissat", i)]["accepted"] != "1"
                 for i in instances)
    only_s = sum(by[("Satsuma+Kissat", i)]["accepted"] == "1" and by[("Tactix", i)]["accepted"] != "1"
                 for i in instances)
    time_ratios = [float(by[("Tactix", i)]["solve_seconds"]) / float(by[("Satsuma+Kissat", i)]["solve_seconds"])
                   for i in common]
    par2 = {c: sum(float(by[(c, i)]["par2_seconds"]) for i in instances)
            for c in ("Tactix", "Satsuma+Kissat")}
    print(f"  Tactix vs Satsuma+Kissat: checked by both {len(common)}, Tactix only {only_t}, "
          f"Satsuma+Kissat only {only_s}; PAR-2 ratio {par2['Tactix'] / par2['Satsuma+Kissat']:.4f}; "
          f"time ratio on common instances: geometric mean "
          f"{statistics.geometric_mean(time_ratios):.4f}, median {statistics.median(time_ratios):.4f}")


def interface(data: Path) -> None:
    rows = read(data / "interface-overhead.csv")
    out = []
    for name in ("Tier A", "B28+C5", "Rank 1-50", "WV100"):
        pairs = [r for r in rows if r["set"] == name]
        ratios = [float(r["c_api_seconds"]) / float(r["cpp_seconds"]) for r in pairs]
        total = sum(float(r["c_api_seconds"]) for r in pairs) / sum(float(r["cpp_seconds"]) for r in pairs)
        out.append([name, "", len(pairs), f"{100 * (statistics.median(ratios) - 1):+.1f}%",
                    f"{100 * (total - 1):+.1f}%"])
    table("Figure: overhead of the default strategy through the C interface",
          ["Set", "", "Instances solved by both", "Median slowdown", "Total-time slowdown"], out)


def cruxmiter(data: Path) -> None:
    rows = read(data / "crux-results.csv")
    by = {(r["configuration"], int(r["seed"])): r for r in rows}
    out = []
    for seed in sorted({int(r["seed"]) for r in rows}):
        line = [seed, ""]
        for configuration in ("Default Tactix", "Public policy", "Kissat 4.0.4"):
            r = by.get((configuration, seed))
            if r is None:
                line += ["-", "-", "-"]
                continue
            line += [f'{float(r["solve_seconds"]):.1f}', f'{float(r["check_seconds"]):.1f}',
                     f'{int(r["proof_bytes"]) / 1e9:.2f}']
        out.append(line)
    table("Table: cruxmiter (Tactix | cruxmiter strategy | Kissat 4.0.4: solve s, check s, proof GB)",
          ["Seed", "", "T solve", "T check", "T GB", "S solve", "S check", "S GB",
           "K solve", "K check", "K GB"], out)


def chvatal(data: Path) -> None:
    rows = read(data / "chvatal-results.csv")
    nodes = sorted({r["node"] for r in rows})
    by = {(r["configuration"], r["node"]): r for r in rows}
    out = []
    for configuration in ("Chvatal Tactix", "Default Tactix", "Satsuma+Kissat", "Kissat 4.0.4"):
        mine = [by[(configuration, n)] for n in nodes if (configuration, n) in by]
        answered = [r for r in mine if r["verdict"] in ("SAT", "UNSAT")]
        checked = [r for r in answered if r["verified"] == "1"]
        par2 = (sum(float(r["solve_seconds"]) for r in checked)
                + 2 * LIMIT * (len(nodes) - len(checked))) / len(nodes)
        out.append([configuration, "", f"{len(answered)}/{len(nodes)}",
                    f'{sum(float(r["solve_seconds"]) for r in answered):,.0f}',
                    f"{len(checked)}/{len(nodes)}",
                    f'{sum(float(r["check_seconds"]) for r in checked if r["check_seconds"]):,.0f}',
                    f"{par2:,.1f}"])
    table("Table: Chvatal subproblems", ["Configuration", "", "Solved", "Runtime (s)", "Checked",
                                         "Check time (s)", "PAR-2 (s)"], out)


def milestones(data: Path) -> None:
    rows = read(data / "milestone-ladder.csv")
    table("Figure: milestone ladder on Tier D",
          ["Version", "Commit", "Date", "Proofs", "SAT", "UNSAT checked", "UNSAT unchecked",
           "Solved", "PAR-2 sum (s)"],
          [[r["label"], r["commit"], r["date"], r["proof_mode"], r["sat_checked"],
            r["unsat_verified"], r["unsat_unchecked"], r["raw_solved"], r["par2_sum_s"]]
           for r in rows])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--data", type=Path, default=PAPER / "data")
    data = parser.parse_args().data
    for section, needed in ((tierd, "tierd-table.csv"), (interface, "interface-overhead.csv"),
                            (cruxmiter, "crux-results.csv"), (chvatal, "chvatal-results.csv"),
                            (milestones, "milestone-ladder.csv")):
        if (data / needed).exists():
            section(data)


if __name__ == "__main__":
    main()
