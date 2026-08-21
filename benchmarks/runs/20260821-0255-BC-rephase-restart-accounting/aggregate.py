#!/usr/bin/env python3
import importlib.util
from pathlib import Path

CAMPAIGN = Path(__file__).resolve().parent
AGGREGATOR = CAMPAIGN.parent / "20260816-2200-BC-congruence-admission-regression" / "aggregate.py"
if __name__ == "__main__":
    spec = importlib.util.spec_from_file_location("checked_regression_aggregate", AGGREGATOR)
    aggregate = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(aggregate)
    aggregate.CAMPAIGN = CAMPAIGN
    raise SystemExit(aggregate.main())
