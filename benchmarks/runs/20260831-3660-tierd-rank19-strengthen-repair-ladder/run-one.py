#!/usr/bin/env python3
"""Run one immutable Rank-19 Strengthen checkpoint with model validation."""

import hashlib
import importlib.util
from pathlib import Path


HERE = Path(__file__).resolve().parent
SOURCE = (HERE.parent /
          "20260827-0100-tierd-ranks1-40-slurm-memory-screen/run-one.py")
SPEC = importlib.util.spec_from_file_location("rank19_strengthen_ladder", SOURCE)
HARNESS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(HARNESS)
HARNESS.CAMPAIGN = HERE
HARNESS.SOLVE_TIMEOUT = 900
HARNESS.CHECK_TIMEOUT = 600

EXPECTED_PINS = {
    "tactix-strengthen3240":
        "e44ee1a64236e748727bc484b3973ea8bf14fbdaa2084f9e474c293327ad2ea3",
    "tactix-observer3260":
        "673760b7dbbf20320a312f2e5d52d36809d1a6f981673a9c1d5d5c6d3a6ace36",
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
