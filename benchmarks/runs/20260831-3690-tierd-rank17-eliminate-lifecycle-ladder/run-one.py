#!/usr/bin/env python3
"""Run one immutable Rank-17 Eliminate-lifecycle arm with model validation."""

import hashlib
import importlib.util
from pathlib import Path


HERE = Path(__file__).resolve().parent
SOURCE = (HERE.parent /
          "20260827-0100-tierd-ranks1-40-slurm-memory-screen/run-one.py")
SPEC = importlib.util.spec_from_file_location("rank17_eliminate_ladder", SOURCE)
HARNESS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(HARNESS)
HARNESS.CAMPAIGN = HERE
HARNESS.SOLVE_TIMEOUT = 900
HARNESS.CHECK_TIMEOUT = 600

EXPECTED_PINS = {
    "tactix-before_resume3320":
        "01ad58cf8c7687c91900d9a79d69070f05111a329872efeadd64caccde8a1f37",
    "tactix-after_resume3330":
        "f7c21a286f8c8d0be7ddddca203b2f413493a827612a707553d3abcbbe778cc3",
    "tactix-before_h3500":
        "96552165f42cc745ca2f974f44e526b641f4eab0a8172deffac6efd8b6b2f21e",
    "tactix-after_h3510":
        "2ce1f7abdc4dbc07c528b6e1f3ddf50dec9a898fc683a1fadd13f88ec7170486",
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
