#!/usr/bin/env python3
import importlib.util
from pathlib import Path

CAMPAIGN = Path(__file__).resolve().parent
RUNNER = CAMPAIGN.parent / "20260816-2200-BC-congruence-admission-regression" / "run-one.py"
if __name__ == "__main__":
    spec = importlib.util.spec_from_file_location("checked_regression_runner", RUNNER)
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)
    runner.CAMPAIGN = CAMPAIGN
    raise SystemExit(runner.main())
