#!/usr/bin/env python3
"""Run the immutable fixed-active Rank-17 Release arm with model checking."""

import hashlib
import importlib.util
from pathlib import Path


HERE = Path(__file__).resolve().parent
SOURCE = (HERE.parent /
          "20260827-0100-tierd-ranks1-40-slurm-memory-screen/run-one.py")
SPEC = importlib.util.spec_from_file_location("rank17_fixed_active_release", SOURCE)
HARNESS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(HARNESS)
HARNESS.CAMPAIGN = HERE
HARNESS.SOLVE_TIMEOUT = 900
HARNESS.CHECK_TIMEOUT = 600

EXPECTED_PINS = {
    "tactix-fixed-active":
        "5743b0a65b76b3cc92d6011b9021744d231c0544cbfc35f14350d63aa5a03698",
    "drat-trim":
        "f2cb7d45d6acc05b1295bfa0d27b53d75fa48259c2b1e15f15772f064f99849a",
    "checkmodel":
        "5fde6bbc7be305f6b6166ef75dc4788e2cf40180317ab1ec9a38a3d6071676cf",
    "modelcheck.py":
        "f70f0eeffe30a31e2ad68f696ba280aba8a81cee747e087efca1f04101ad0d98",
}


def verify_pins():
    for name, expected in EXPECTED_PINS.items():
        actual = hashlib.sha256((HERE / "pins" / name).read_bytes()).hexdigest()
        if actual != expected:
            raise SystemExit(f"pin SHA-256 mismatch for {name}: {actual}")


if __name__ == "__main__":
    verify_pins()
    raise SystemExit(HARNESS.main())
