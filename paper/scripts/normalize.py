#!/usr/bin/env python3
"""Turn the logs in paper/logs/<experiment>/*.tar.xz into one table per experiment.

    python3 paper/scripts/normalize.py [--out DIR]

Each archive holds one configuration's runs (runs/<formula>.json: the run's
record, in the layout of the harness version that ran it) and the metadata of
the campaigns they came from (campaigns/<name>/).

Writes paper/logs/<experiment>/results.csv with one row per (configuration,
instance) and the common columns

  configuration, instance, verdict, solve_seconds, status, verified,
  check_seconds, proof_bytes, par2_seconds

plus experiment-specific columns.
"""
from __future__ import annotations

import argparse
import csv
import json
import tarfile
from pathlib import Path

LOGS = Path(__file__).resolve().parents[1] / "logs"
OUT = LOGS
PENALTY = 10000


def runs(experiment: str, skip: tuple[str, ...] = ()) -> list[dict]:
    """Every run of an experiment's configurations, in recorded order."""
    out = []
    for path in sorted((LOGS / experiment).glob("*.tar.xz")):
        with tarfile.open(path, "r:xz") as archive:
            out += [json.load(archive.extractfile(member)) for member in archive
                    if member.isfile() and "/runs/" in member.name]
    return sorted((entry for entry in out if entry["configuration"] not in skip),
                  key=lambda entry: entry["order"])


def write(experiment: str, rows: list[dict]) -> None:
    fields = list(dict.fromkeys(key for row in rows for key in row))
    (OUT / experiment).mkdir(parents=True, exist_ok=True)
    with (OUT / experiment / "results.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    print(f"{experiment:11} {len(rows):6d} rows")


# --- tierd: SAT Competition 2024-2026 main tracks --------------------------
#
# A SAT answer counts when its model checks (and, for the reference solvers,
# the output follows the competition protocol); an UNSAT answer counts when
# its proof verifies. Unverified rows count 10,000 s.

TIERD_SOLVERS = {
    "tactix-0f7c10174": "Tactix",
    "satsuma-kissat": "Satsuma+Kissat",
    "kissat-sc2024": "Kissat-sc2024",
    "kissat-4.0.4": "Kissat 4.0.4",
    "ae-kissat-mab": "AE-Kissat-MAB",
    "lymphosat": "LymphoSAT",
}


def _answered(record):
    return record.get("verdict") in ("SAT", "UNSAT") and record.get("expected_match", True)


def _verified(record):
    if not _answered(record):
        return False
    if "model_ok" in record and record["verdict"] == "SAT":
        return record["model_ok"] is True
    return bool(record.get("valid"))


def _tierd_row(record):
    solve = record.get("solve") or {}
    model = record.get("model_check") or {}
    check = record.get("proof_check") or {}
    ok = _verified(record)
    if record.get("verdict") in ("SAT", "UNSAT") and not record.get("expected_match", True):
        status = "wrong-answer"
    elif ok:
        status = "verified"
    elif record.get("verdict") == "SAT":
        status = "model-ok-nonprotocol" if model.get("exit_code") == 0 else "model-rejected"
    elif record.get("verdict") == "UNSAT":
        status = "unverified"
    elif solve.get("timed_out"):
        status = "timeout"
    elif solve.get("exit_code") == 137:
        status = "solver-memout"
    else:
        status = f"no-answer (exit {solve.get('exit_code')})"
    wall = solve.get("wall_seconds")
    # The Tactix campaign's check time also covers preparing the checker's input;
    # check_stage_seconds is the checkers' own time, as the other solvers record.
    stages = check.get("stages") or []
    return dict(verdict=record.get("verdict"), solve_seconds=wall, status=status, verified=ok,
                check_seconds=check.get("wall_seconds"),
                proof_bytes=record.get("certificate_bytes"),
                par2_seconds=round(wall, 3) if ok else PENALTY,
                check_stage_seconds=round(sum(s.get("wall_seconds") or 0 for s in stages), 3)
                if stages else "")


def tierd():
    write("tierd", [dict(configuration=entry["configuration"], instance=entry["instance"],
                         competition=entry["competition"], **_tierd_row(entry["record"]))
                    for entry in runs("tierd")])


# --- Milestone ladder ---------------------------------------------------------
#
# The first commit that met each development milestone, run on the tierd formulas, beside
# the current version's tierd run; Table 6 and Figure 7 count the answers.


def milestones():
    rows = []
    for entry in runs("milestones"):
        record = entry["record"]
        rows.append(dict(configuration=entry["configuration"], commit=entry["commit"],
                         instance=entry["instance"], verdict=record.get("verdict"),
                         solve_seconds=(record.get("solve") or {}).get("wall_seconds")))
    write("milestones", rows)


# --- Overhead of the C interface ---------------------------------------------
#
# The default strategy with every operation through tactix.h (tactix-public)
# against the production C++ build, in the same time window. Tier A ran both
# arms in one campaign; each regression set ran one campaign per arm.

def interface():
    rows = []
    for entry in runs("interface"):
        record = entry["record"]
        if entry["set"] == "Tier A":
            rows.append(dict(set="Tier A", configuration=entry["configuration"],
                             instance=entry["instance"], verdict=record["verdict"],
                             solve_seconds=record["time"],
                             timed_out=record["verdict"] not in ("SAT", "UNSAT")))
            continue
        solve = record["solve"]
        rows.append(dict(set=entry["set"], configuration=entry["configuration"],
                         instance=entry["instance"], verdict=record["verdict"],
                         solve_seconds=solve["wall_seconds"], timed_out=bool(solve.get("timed_out"))))
    write("interface", rows)


# --- Case study: cruxmiter ----------------------------------------------------

def cruxmiter():
    rows = []
    for entry in runs("cruxmiter"):
        record = entry["record"]
        rows.append(dict(configuration=entry["configuration"], instance=record["instance"],
                         verdict=record["verdict"], solve_seconds=record["time"],
                         verified=bool(record["proof_ok"]),
                         check_seconds=record["proof_check_time"],
                         proof_bytes=record["proof_bytes"]))
    write("cruxmiter", rows)


# --- Case study: Chvatal's conjecture -------------------------------------------
#
# The 23 formulas of case n = 7 that Kissat 4.0.4 solves in 300-1,800 s
# (selection in paper-hard.json, from the screening run of all subproblems).


def chvatal():
    selection = json.loads((LOGS / "chvatal" / "paper-hard.json").read_text())
    screen = {entry["file"]: entry["kissat_seconds"] for entry in selection["selected"]}
    rows = []
    for entry in runs("chvatal", skip=("Kissat 4.0.4 (screening)",)):
        record, rel = entry["record"], entry["instance"]
        rows.append(dict(configuration=entry["configuration"], instance=rel,
                         verdict=record["verdict"], solve_seconds=record["time"],
                         verified=bool(record.get("proof_ok")),
                         check_seconds=record.get("proof_check_time"),
                         proof_bytes=record.get("proof_bytes"),
                         kissat_screen_seconds=screen[rel]))
    write("chvatal", rows)


def main():
    global OUT
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=LOGS,
                        help="directory for <experiment>/results.csv (default: paper/logs)")
    OUT = parser.parse_args().out
    tierd()
    milestones()
    interface()
    cruxmiter()
    chvatal()


if __name__ == "__main__":
    main()
