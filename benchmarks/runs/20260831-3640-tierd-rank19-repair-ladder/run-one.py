#!/usr/bin/env python3
"""Run one immutable Rank-19 repair-ladder arm with model validation."""

import hashlib
import importlib.util
from pathlib import Path


HERE = Path(__file__).resolve().parent
SOURCE = (HERE.parent /
          "20260827-0100-tierd-ranks1-40-slurm-memory-screen/run-one.py")
SPEC = importlib.util.spec_from_file_location("rank19_repair_ladder", SOURCE)
HARNESS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(HARNESS)
HARNESS.CAMPAIGN = HERE
HARNESS.SOLVE_TIMEOUT = 900
HARNESS.CHECK_TIMEOUT = 600

EXPECTED_PINS = {
    "tactix-base2710":
        "1d85a694a122e3a466870b598e82159442196ffef7a5d37343912021678cb4f7",
    "tactix-congruence2820":
        "98111160632b4459d461e4aaae02bdea2b6bd0ab9a2db188cf6f26f6aa8e1fad",
    "tactix-factor2980":
        "e6335a990a34fbcd27341a1a11e33e6c9be318f0b9abc5b1a353c2e7ea948f76",
    "tactix-bve_retry3310":
        "77cae09da6a7814e455f8b31017d29d41062b3ee5ccdce31e9519d0a9d2f0250",
    "tactix-bve_inline3520":
        "361a24de5c0cd5b37066a24055bd8e8683753dbbbfb19403c26b57cb4793ce69",
    "tactix-transitive3620":
        "c783146468ccd8e6d87d78efad9439555a6afe4ae503f357f4a9f0c3c60e4066",
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
