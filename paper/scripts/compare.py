#!/usr/bin/env python3
"""Put a rerun of the experiments next to the paper's archived logs.

    python3 paper/scripts/compare.py --run paper/run/results --out DIR

For every experiment in the rerun (bench.py's output directory), the paper's
archived results (paper/logs/) are restricted to the same configurations and
instances and cut off at the rerun's limits: an answer that took longer than
the rerun's solving limit counts as a timeout, a certificate check longer than
its checking limit as unchecked, and PAR-2 uses twice the rerun's limit. (The
memory limit cannot be applied to the archived runs; their memory was larger.)
Both are then turned into the paper's tables and figures by the same scripts:

  DIR/artifact/{logs,data,figures}     the rerun
  DIR/paper-logs/{logs,data,figures}   the paper's logs on the same runs
  DIR/comparison.md                    every table, each cell "rerun (paper logs)"
  DIR/per-instance.csv                 each run's outcome next to the archived one

Runtimes depend on the machine, so compare trends (rankings, who solves what,
ratios) rather than seconds.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PAPER = HERE.parent
sys.path.insert(0, str(HERE))
import report  # noqa: E402

EXPERIMENTS = ("tierd", "milestones", "interface", "cruxmiter", "chvatal")


def read(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def write(path: Path, rows: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fields = list(dict.fromkeys(k for row in rows for k in row))
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def key(instance: str) -> str:
    """The formula's name, whatever directory, compression or hash prefix."""
    name = Path(instance).name.removesuffix(".xz")
    return re.sub(r"^[0-9a-f]{32}-", "", name)


def true(value) -> bool:
    return str(value) == "True"


def censor(row: dict, run: dict) -> dict:
    """An archived result as it would have been under the rerun's limits."""
    solve_limit = float(run["solve_limit_seconds"])
    check_limit = float(run["check_limit_seconds"])
    row = dict(row, solve_limit_seconds=run["solve_limit_seconds"],
               check_limit_seconds=run["check_limit_seconds"],
               memory_limit_gib=run["memory_limit_gib"], profile=run.get("profile", ""),
               set=row.get("set") or run.get("set", ""))
    answered = row["verdict"] in ("SAT", "UNSAT")
    seconds = float(row["solve_seconds"])
    if not answered or seconds > solve_limit:
        row.update(verdict="UNKNOWN", solve_seconds=f"{min(seconds, solve_limit):.3f}", status="timeout",
                   timed_out="True", verified="False", counted="False", solved_checked="False",
                   check_seconds="", proof_check_result="", par2_seconds=2 * solve_limit)
        return row
    if row["verdict"] == "UNSAT" and row.get("check_seconds") and float(row["check_seconds"]) > check_limit:
        row.update(verified="False", solved_checked="False", status="check-timeout",
                   proof_check_result="not-verified")
    checked = true(row.get("verified")) or (row.get("proof_mode") == "none" and true(row.get("counted")))
    if "par2_seconds" in row:
        row["par2_seconds"] = row["solve_seconds"] if checked else 2 * solve_limit
    return row


def paper_view(run_logs: Path, out_logs: Path) -> list[dict]:
    """Write the censored archived results for every rerun experiment; return
    the per-instance comparison rows."""
    pairs = []
    for name in EXPERIMENTS:
        run_path = run_logs / name / "results.csv"
        if not run_path.exists():
            continue
        archived = {(r["configuration"], key(r["instance"])): r
                    for r in read(PAPER / "logs" / name / "results.csv")}
        rows = []
        for run in read(run_path):
            paper = archived.get((run["configuration"], key(run["instance"])))
            if paper is None:
                print(f"{name}: no archived result for {run['configuration']} on {run['instance']}",
                      file=sys.stderr)
                continue
            if run.get("proof_mode") == "none" and paper.get("proof_mode") == "proof":
                # the rerun ran this configuration without proofs (milestones' Current)
                paper = dict(paper, proof_mode="none", proof_check_result="",
                             counted=str(paper["verdict"] in ("SAT", "UNSAT")))
            censored = censor(paper, run)
            censored["instance"] = run["instance"]  # the rerun's spelling, so the tables align
            rows.append(censored)
            pairs.append({
                "experiment": name, "set": run.get("set", ""), "configuration": run["configuration"],
                "instance": key(run["instance"]),
                "rerun_verdict": run["verdict"], "rerun_seconds": run["solve_seconds"],
                "rerun_status": run["status"],
                "paper_verdict": paper["verdict"], "paper_seconds": paper["solve_seconds"],
                "paper_status_at_rerun_limits": censored.get("status", censored["verdict"]),
                "same_outcome": (run["verdict"] in ("SAT", "UNSAT")) == (censored["verdict"] in ("SAT", "UNSAT"))
                and (run["verdict"] == censored["verdict"] or censored["verdict"] == "UNKNOWN"),
            })
        if rows:
            write(out_logs / name / "results.csv", rows)
    return pairs


def build(logs: Path, base: Path) -> None:
    """data/ and figures/ from logs/, with the paper's Makefile."""
    subprocess.run(["make", "-s", "-C", str(PAPER), f"LOGS={logs}", f"DATA={base / 'data'}",
                    f"FIGURES={base / 'figures'}", "data", "figures"], check=True,
                   stdout=subprocess.DEVNULL)


def merge(rerun: report.Table, paper: report.Table | None) -> report.Table:
    lookup = {tuple(r[:rerun.keys]): r for r in (paper.rows if paper else [])}
    rows = []
    for row in rerun.rows:
        other = lookup.get(tuple(row[:rerun.keys]))
        rows.append(row[:rerun.keys] + [
            cell if other is None else f"{cell} ({other[i]})"
            for i, cell in enumerate(row) if i >= rerun.keys])
    return report.Table(rerun.name, rerun.title, rerun.header, rows, rerun.keys,
                        rerun.notes + ["Each cell: rerun (paper's logs on the same runs, at the rerun's limits)."])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--run", type=Path, default=PAPER / "run/results")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    run, out = args.run.resolve(), args.out.resolve()
    for side in ("artifact", "paper-logs"):
        shutil.rmtree(out / side, ignore_errors=True)
    for name in EXPERIMENTS:
        if (run / name / "results.csv").exists():
            (out / "artifact/logs" / name).mkdir(parents=True, exist_ok=True)
            shutil.copy(run / name / "results.csv", out / "artifact/logs" / name / "results.csv")
    pairs = paper_view(out / "artifact/logs", out / "paper-logs/logs")
    if not pairs:
        sys.exit(f"no results.csv under {run}; run bench.py first")
    write(out / "per-instance.csv", pairs)
    for side in ("artifact", "paper-logs"):
        build(out / side / "logs", out / side)
    rerun = report.tables(out / "artifact/data")
    paper = {t.name: t for t in report.tables(out / "paper-logs/data")}
    merged = [merge(t, paper.get(t.name)) for t in rerun]
    same = sum(p["same_outcome"] for p in pairs)
    limits = json.loads((out / "artifact/data/limits.json").read_text())
    header = (f"# Rerun next to the paper's logs ({limits.get('profile', 'full')} profile)\n\n"
              f"Each cell shows the rerun's value and, in parentheses, the value computed from the "
              f"paper's archived logs for the same configurations and formulas, cut off at the "
              f"rerun's limits. Same outcome (answered or not) on {same} of {len(pairs)} runs; "
              f"see per-instance.csv. Figures: artifact/figures/ and paper-logs/figures/.\n")
    (out / "comparison.md").write_text(header + "\n" + "\n".join(report.render(t, True) for t in merged))
    print(header)
    for table in merged:
        print(report.render(table))
    print(f"\nwrote {out / 'comparison.md'}, {out / 'per-instance.csv'}, "
          f"{out / 'artifact/figures'}, {out / 'paper-logs/figures'}")


if __name__ == "__main__":
    main()
