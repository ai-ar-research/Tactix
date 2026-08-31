#!/usr/bin/env python3
"""Compare the import-order candidate with final, accepted, and Kissat records."""

import importlib.util
from pathlib import Path


HERE = Path(__file__).resolve().parent
SOURCE = HERE.parent / "20260830-2310-tierd-ranks1-40-kitten-seed/aggregate.py"
SPEC = importlib.util.spec_from_file_location("randec_import_order_rank40_aggregate", SOURCE)
REPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REPORT)
REPORT.CAMPAIGN = HERE
REPORT.LATEST = HERE.parent / "20260830-2530-tierd-ranks1-40-bve-physical-definition"


if __name__ == "__main__":
    raise SystemExit(REPORT.main())
