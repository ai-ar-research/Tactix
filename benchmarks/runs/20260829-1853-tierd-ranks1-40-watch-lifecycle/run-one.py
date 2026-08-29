#!/usr/bin/env python3
"""Run one checked Tactix rank and publish its solver verdict immediately."""

import csv
import datetime as dt
import importlib.util
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[3]
CAMPAIGN = Path(__file__).resolve().parent
SOURCE = ROOT / "benchmarks/runs/20260827-0100-tierd-ranks1-40-slurm-memory-screen/run-one.py"
SPEC = importlib.util.spec_from_file_location("rank40_frozen_run_one_v2", SOURCE)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
MODULE.CAMPAIGN = CAMPAIGN
MODULE.SOLVE_TIMEOUT = 1200
MODULE.CHECK_TIMEOUT = 16000

ORIGINAL_MEASURED = MODULE.measured


def measured_with_provisional(command, stdout, stderr, metrics, timeout):
    """Expose solver timing before a possibly long independent proof check."""
    measured = ORIGINAL_MEASURED(command, stdout, stderr, metrics, timeout)
    if Path(command[0]).name != "tactix-tactix":
        return measured

    task = int(sys.argv[1])
    with open(CAMPAIGN / "matrix.tsv", newline="") as source:
        rows = list(csv.DictReader(source, delimiter="\t"))
    row = rows[task]
    verdict, statistics = MODULE.solver_summary(stdout)
    provisional = {
        "schema": "tactix-tierd-ranks1-40-provisional-solver-v1",
        "task": task,
        "rank": int(row["rank"]),
        "arm": row["arm"],
        "instance": row["instance"],
        "expected": row["expected"],
        "verdict": verdict,
        "expected_match": verdict == row["expected"],
        "solve": measured,
        "solver_statistics": statistics,
        "certificate_check": "pending-nonblocking" if verdict == "UNSAT" else None,
        "model_check": "pending" if verdict == "SAT" else None,
        "finished_solver_at": dt.datetime.now(dt.timezone.utc).isoformat(),
    }
    path = CAMPAIGN / "provisional" / f"{task:03d}.json"
    MODULE.atomic_json(path, provisional)
    return measured


MODULE.measured = measured_with_provisional


if __name__ == "__main__":
    raise SystemExit(MODULE.main())
