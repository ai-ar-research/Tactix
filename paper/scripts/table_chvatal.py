#!/usr/bin/env python3
"""Small LaTeX summary table for the Chvatal use case.

Per configuration over the fixed set:
  Solved      formulas answered within the 5000 s solving limit;
  Runtime     total solving time over those answers (s);
  Checked     answers whose certificate was accepted within the 2 h limit;
  Check time  total checking time over the checked certificates (s);
  PAR-2       mean of the solving time for a checked answer and twice the
              solving limit for every other formula (s).
With --limits (limits.json from make_data.py) the solving limit is the run's.
"""
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

LIMIT = 5000.0
ORDER = ("Chvatal Tactix", "Default Tactix", "Satsuma+Kissat", "Kissat 4.0.4")
LABEL = {"Chvatal Tactix": "\\tactixch", "Default Tactix": "\\tactix",
         "Satsuma+Kissat": "\\satsumakissat", "Kissat 4.0.4": "\\kissat~4.0.4"}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", default=Path(__file__).resolve().parents[1] / "data/chvatal-results.csv")
    parser.add_argument("--output", default=Path(__file__).resolve().parents[1] / "figures/chvatal-table.tex")
    parser.add_argument("--limits", type=Path, help="limits.json of the run (make_data.py)")
    args = parser.parse_args()
    limit = LIMIT
    if args.limits and args.limits.exists():
        limit = float(json.loads(args.limits.read_text()).get("chvatal", {}).get("seconds", LIMIT))
    rows = list(csv.DictReader(Path(args.data).open(newline="", encoding="utf-8")))
    nodes = sorted({r["node"] for r in rows})
    by = {(r["configuration"], r["node"]): r for r in rows}
    lines = [
        "\\begin{tabular}{@{}lrrrrr@{}}",
        "\\toprule",
        "Configuration & Solved & Runtime (s) & Checked & Check time (s) & PAR-2 (s) \\\\",
        "\\midrule",
    ]
    for config in ORDER:
        mine = [by[(config, n)] for n in nodes if (config, n) in by]
        if not mine:
            continue
        answered = [r for r in mine if r["verdict"] in ("SAT", "UNSAT")]
        checked = [r for r in answered if r["verified"] == "1"]
        runtime = sum(float(r["solve_seconds"]) for r in answered)
        check = sum(float(r["check_seconds"]) for r in checked if r["check_seconds"])
        par2 = (sum(float(r["solve_seconds"]) for r in checked)
                + 2 * limit * (len(nodes) - len(checked))) / len(nodes)
        lines.append(f"{LABEL.get(config, config)} & {len(answered)}/{len(nodes)} & "
                     f"{runtime:,.0f} & {len(checked)}/{len(nodes)} & "
                     f"{check:,.0f} & {par2:,.1f} \\\\")
    lines += ["\\bottomrule", "\\end{tabular}"]
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text("\n".join(lines) + "\n")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
