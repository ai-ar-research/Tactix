#!/usr/bin/env python3
"""Table 5 (Chvatal subproblems) as LaTeX.

Per configuration:
  Solved      formulas answered within the solving limit;
  Runtime     total solving time over those answers (s);
  Checked     answers whose certificate was accepted within the checking limit;
  Check time  total checking time over the checked certificates (s).
"""
from __future__ import annotations

import argparse
import csv
from pathlib import Path

ORDER = ("Chvatal Tactix", "Default Tactix", "Satsuma+Kissat", "Kissat 4.0.4")
LABEL = {"Chvatal Tactix": "\\tactixch", "Default Tactix": "\\tactix",
         "Satsuma+Kissat": "\\satsumakissat", "Kissat 4.0.4": "\\kissat~4.0.4"}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", default=Path(__file__).resolve().parents[1] / "data/chvatal-results.csv")
    parser.add_argument("--output", default=Path(__file__).resolve().parents[1] / "figures/chvatal-table.tex")
    args = parser.parse_args()
    rows = list(csv.DictReader(Path(args.data).open(newline="", encoding="utf-8")))
    nodes = sorted({r["node"] for r in rows})
    by = {(r["configuration"], r["node"]): r for r in rows}
    lines = [
        "\\begin{tabular}{@{}lrrrr@{}}",
        "\\toprule",
        "Configuration & Solved & Runtime (s) & Checked & Check time (s)\\\\",
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
        lines.append(f"{LABEL.get(config, config)} & {len(answered)} & {runtime:,.0f} & "
                     f"{len(checked)} & {check:,.0f} \\\\")
    lines += ["\\bottomrule", "\\end{tabular}"]
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text("\n".join(lines) + "\n")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
