#!/usr/bin/env python3
"""Print the paper's tables and headline numbers from paper/data/.

    python3 paper/scripts/report.py [--data DIR] [--markdown FILE]

Each section names the table or figure of the paper it corresponds to, so the
output can be compared with the paper line by line. compare.py uses the same
tables to put a rerun next to the paper's logs.
"""
from __future__ import annotations

import argparse
import csv
import json
import statistics
from dataclasses import dataclass, field
from pathlib import Path

PAPER = Path(__file__).resolve().parents[1]
TIERD_ORDER = ("Tactix", "Satsuma+Kissat", "Kissat 4.0.4", "LymphoSAT", "Kissat-sc2024", "AE-Kissat-MAB")
CRUX_ORDER = ("Default Tactix", "Public policy", "Kissat 4.0.4")
CHVATAL_ORDER = ("Chvatal Tactix", "Default Tactix", "Satsuma+Kissat", "Kissat 4.0.4")
INTERFACE_SETS = ("Tier A", "B28+C5", "Rank 1-50", "WV100")
MILESTONES = (("Milestone A", "1"), ("Milestone B", "2"), ("Milestone C", "3"),
              ("Milestone D", "4"), ("Milestone E", "5"), ("Current", "Now"))


@dataclass
class Table:
    """A table of the paper: the first `keys` columns identify a row."""
    name: str
    title: str
    header: list[str]
    rows: list[list[str]]
    keys: int = 2
    notes: list[str] = field(default_factory=list)


def read(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def limits(data: Path) -> dict:
    path = data / "limits.json"
    return json.loads(path.read_text()) if path.exists() else {}


def tierd(data: Path) -> list[Table]:
    rows = [[r["benchmark"], r["configuration"], r["instances"], r["solved"], r["accepted"], r["sat"],
             r["unsat"], r["unsat_verified"], f'{float(r["accepted_solve_mean_s"]):.1f}',
             f'{float(r["par2_mean_s"]):.0f}'] for r in read(data / "tierd-table.csv")]
    times = read(data / "tierd-times.csv")
    by = {(r["configuration"], r["instance"]): r for r in times}
    instances = sorted({r["instance"] for r in times})
    common = [i for i in instances
              if by[("Tactix", i)]["accepted"] == "1" and by[("Satsuma+Kissat", i)]["accepted"] == "1"]
    only_t = sum(by[("Tactix", i)]["accepted"] == "1" and by[("Satsuma+Kissat", i)]["accepted"] != "1"
                 for i in instances)
    only_s = sum(by[("Satsuma+Kissat", i)]["accepted"] == "1" and by[("Tactix", i)]["accepted"] != "1"
                 for i in instances)
    portfolio = sum(any(by[(c, i)]["accepted"] == "1" for c in ("Satsuma+Kissat", "LymphoSAT"))
                    for i in instances)
    summary = [["Tactix", "checked", str(sum(by[("Tactix", i)]["accepted"] == "1" for i in instances))],
               ["SK+Lym portfolio", "checked", str(portfolio)],
               ["Tactix only", "vs Satsuma+Kissat", str(only_t)],
               ["Satsuma+Kissat only", "vs Tactix", str(only_s)],
               ["Both", "checked", str(len(common))]]
    return [Table("table3", "Table 3: results on the SC'24-26 benchmark sets (Tier D)",
                  ["Set", "Configuration", "Instances", "Solved", "Solved (checked)", "SAT", "UNSAT",
                   "UNSAT (checked)", "Avg runtime, checked (s)", "Avg PAR-2 (s)"], rows),
            Table("figure4", "Figure 4: Tactix against the virtual portfolio of Satsuma+Kissat and LymphoSAT",
                  ["Count", "", "Formulas"], summary)]


def interface(data: Path) -> list[Table]:
    rows = read(data / "interface-overhead.csv")
    out = []
    for name in INTERFACE_SETS:
        pairs = [r for r in rows if r["set"] == name]
        if not pairs:
            continue
        ratios = [float(r["c_api_seconds"]) / max(float(r["cpp_seconds"]), 1e-3) for r in pairs]
        total = sum(float(r["c_api_seconds"]) for r in pairs) / sum(float(r["cpp_seconds"]) for r in pairs)
        out.append([name, "", str(len(pairs)), f"{100 * (statistics.median(ratios) - 1):+.1f}%",
                    f"{100 * (total - 1):+.1f}%"])
    return [Table("figure5", "Figure 5: overhead of the default strategy through the C interface",
                  ["Set", "", "Solved by both", "Median slowdown", "Total-time slowdown"], out)]


def cruxmiter(data: Path) -> list[Table]:
    rows = read(data / "crux-results.csv")
    by = {(r["configuration"], int(r["seed"])): r for r in rows}
    out = []
    for seed in sorted({int(r["seed"]) for r in rows}):
        line = [str(seed), ""]
        for configuration in CRUX_ORDER:
            r = by.get((configuration, seed))
            if r is None or r["verdict"] != "UNSAT":
                line += ["-", "-", "-"]
                continue
            checked = r["verified"] == "1"
            line += [f'{float(r["solve_seconds"]):.1f}',
                     f'{float(r["check_seconds"]):.1f}' if checked and r["check_seconds"] else "-",
                     f'{int(r["proof_bytes"] or 0) / 1e9:.2f}']
        out.append(line)
    return [Table("table4", "Table 4: cruxmiter (T = Tactix, Tmi = cruxmiter strategy, K = Kissat 4.0.4; "
                  "solve s, check s, proof GB; '-' = no answer or check failed)",
                  ["Seed", "", "T solve", "T check", "T GB", "Tmi solve", "Tmi check", "Tmi GB",
                   "K solve", "K check", "K GB"], out)]


def chvatal(data: Path) -> list[Table]:
    rows = read(data / "chvatal-results.csv")
    limit = float(limits(data).get("chvatal", {}).get("seconds", 5000))
    nodes = sorted({r["node"] for r in rows})
    by = {(r["configuration"], r["node"]): r for r in rows}
    out = []
    for configuration in CHVATAL_ORDER:
        mine = [by[(configuration, n)] for n in nodes if (configuration, n) in by]
        if not mine:
            continue
        answered = [r for r in mine if r["verdict"] in ("SAT", "UNSAT")]
        checked = [r for r in answered if r["verified"] == "1"]
        par2 = (sum(float(r["solve_seconds"]) for r in checked)
                + 2 * limit * (len(nodes) - len(checked))) / len(nodes)
        out.append([configuration, "", f"{len(answered)}/{len(nodes)}",
                    f'{sum(float(r["solve_seconds"]) for r in answered):,.0f}',
                    f"{len(checked)}/{len(nodes)}",
                    f'{sum(float(r["check_seconds"]) for r in checked if r["check_seconds"]):,.0f}',
                    f"{par2:,.1f}"])
    return [Table("table5", "Table 5: Chvatal subproblems",
                  ["Configuration", "", "Solved", "Runtime (s)", "Checked", "Check time (s)", "PAR-2 (s)"], out)]


def milestones(data: Path) -> list[Table]:
    ladder = {r["label"]: r for r in read(data / "milestone-ladder.csv")}
    activity_path = data / "milestone-activity.csv"
    if not activity_path.exists():
        activity_path = PAPER / "data/milestone-activity.csv"  # from git history (analyze_milestone_activity.py)
    activity = read(activity_path)
    categories = read(activity_path.with_name("milestone-activity-categories.csv"))

    def lines(interval, names):
        mine = [r for r in categories if r["interval"] == interval and r["measure"] == "cumulative"
                and r["category"] in names]
        return f'{sum(int(r["added"]) for r in mine):,} / {sum(int(r["deleted"]) for r in mine):,}'

    out = []
    for (label, version), row in zip(MILESTONES, activity):
        solved = ladder.get(label, {}).get("raw_solved", "-")
        out.append([version, row["revision"][:5], row["end_day"], solved, f'{int(row["commits"]):,}',
                    lines(row["interval"], ("solver", "public_api")),
                    lines(row["interval"], ("tests_fuzz_code",))])
    ladder_rows = [[r["label"], r["commit"], r["sat_checked"], r["unsat_verified"], r["unsat_unchecked"],
                    r["raw_solved"]] for r in ladder.values()]
    return [Table("table6", "Table 6: performance and implementation activity of the evaluated versions "
                  "(Solved from the runs; activity columns from the git history)",
                  ["Version", "Commit", "Day", "Solved", "Commits", "Solver/API lines +/-",
                   "Test/fuzzing lines +/-"], out),
            Table("figure7", "Figure 7: SAT and UNSAT answers of each version (milestone ladder)",
                  ["Version", "Commit", "SAT", "UNSAT checked", "UNSAT unchecked", "Solved"], ladder_rows)]


SECTIONS = ((tierd, "tierd-table.csv"), (interface, "interface-overhead.csv"),
            (cruxmiter, "crux-results.csv"), (chvatal, "chvatal-results.csv"),
            (milestones, "milestone-ladder.csv"))


def tables(data: Path) -> list[Table]:
    result = []
    for section, needed in SECTIONS:
        if (data / needed).exists():
            result += section(data)
    return result


def render(table: Table, markdown: bool = False) -> str:
    cells = [table.header] + table.rows
    if markdown:
        text = [f"### {table.title}", ""]
        text += ["| " + " | ".join(row) + " |" for row in cells[:1]]
        text += ["|" + "|".join("---" if i < table.keys else "---:" for i in range(len(table.header))) + "|"]
        text += ["| " + " | ".join(row) + " |" for row in cells[1:]]
        return "\n".join(text + [""] + [f"> {n}" for n in table.notes] + [""])
    widths = [max(len(row[i]) for row in cells) for i in range(len(table.header))]
    text = [f"\n{table.title}\n"]
    for index, row in enumerate(cells):
        text.append("  " + "  ".join(c.ljust(w) if i < table.keys else c.rjust(w)
                                     for i, (c, w) in enumerate(zip(row, widths))))
        if index == 0:
            text.append("  " + "  ".join("-" * w for w in widths))
    return "\n".join(text + [f"  {n}" for n in table.notes])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--data", type=Path, default=PAPER / "data")
    parser.add_argument("--markdown", type=Path, help="also write the tables as Markdown")
    args = parser.parse_args()
    result = tables(args.data)
    for table in result:
        print(render(table))
    if args.markdown:
        args.markdown.write_text("\n".join(render(t, True) for t in result))


if __name__ == "__main__":
    main()
