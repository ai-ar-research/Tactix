#!/usr/bin/env python3
"""Run one Rank-5 deferred-guidance validation task."""

import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "benchmarks/runs/20260827-0100-tierd-ranks1-40-slurm-memory-screen/run-one.py"
SPEC = importlib.util.spec_from_file_location("rank40_frozen_run_one_v2", SOURCE)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
MODULE.CAMPAIGN = Path(__file__).resolve().parent
MODULE.SOLVE_TIMEOUT = 1200
MODULE.CHECK_TIMEOUT = 16000


if __name__ == "__main__":
    raise SystemExit(MODULE.main())
