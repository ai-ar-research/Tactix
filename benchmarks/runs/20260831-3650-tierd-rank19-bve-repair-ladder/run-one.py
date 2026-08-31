#!/usr/bin/env python3
"""Run one immutable Rank-19 BVE-repair ladder arm with model validation."""

import hashlib
import importlib.util
from pathlib import Path


HERE = Path(__file__).resolve().parent
SOURCE = (HERE.parent /
          "20260827-0100-tierd-ranks1-40-slurm-memory-screen/run-one.py")
SPEC = importlib.util.spec_from_file_location("rank19_bve_repair_ladder", SOURCE)
HARNESS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(HARNESS)
HARNESS.CAMPAIGN = HERE
HARNESS.SOLVE_TIMEOUT = 900
HARNESS.CHECK_TIMEOUT = 600

EXPECTED_PINS = {
    "tactix-input3080":
        "1130b4b43191f1fe33b19eb283bb3761fea68761d92d8e053acc9335590a7175",
    "tactix-unit3170":
        "b72eba50f7f4c725d83e4ec13d88b5424df30fae489715c31cb74c16e3b833a3",
    "tactix-round3220":
        "64d6ffcb8e998765df26a24e59e40512eb86949b3fd18413612cdb133f527540",
    "tactix-retry3270":
        "299307269056c9c87eda1b28b98e0acfdefec0ffd2bca231eb2553ddd471f10f",
    "drat-trim":
        "f2cb7d45d6acc05b1295bfa0d27b53d75fa48259c2b1e15f15772f064f99849a",
    "checkmodel":
        "5fde6bbc7be305f6b6166ef75dc4788e2cf40180317ab1ec9a38a3d6071676cf",
    "modelcheck.py":
        "f70f0eeffe30a31e2ad68f696ba280aba8a81cee747e087efca1f04101ad0d98",
}


def verify_pins():
    for name, expected in EXPECTED_PINS.items():
        path = HERE / "pins" / name
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != expected:
            raise SystemExit(f"pin SHA-256 mismatch for {name}: {actual}")


if __name__ == "__main__":
    verify_pins()
    raise SystemExit(HARNESS.main())
