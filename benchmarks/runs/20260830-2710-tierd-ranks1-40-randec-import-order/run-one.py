#!/usr/bin/env python3
"""Run one checked rank with the Kissat import-order random-index repair."""

import hashlib
import importlib.util
from pathlib import Path


HERE = Path(__file__).resolve().parent
SOURCE = HERE.parent / "20260830-2310-tierd-ranks1-40-kitten-seed/run-one.py"
SPEC = importlib.util.spec_from_file_location("randec_import_order_rank40_run_one", SOURCE)
HARNESS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(HARNESS)
HARNESS.CAMPAIGN = HERE
HARNESS.MODULE.CAMPAIGN = HERE

EXPECTED_PINS = {
    "tactix-tactix": "1d85a694a122e3a466870b598e82159442196ffef7a5d37343912021678cb4f7",
    "drat-trim": "f2cb7d45d6acc05b1295bfa0d27b53d75fa48259c2b1e15f15772f064f99849a",
    "checkmodel": "5fde6bbc7be305f6b6166ef75dc4788e2cf40180317ab1ec9a38a3d6071676cf",
    "modelcheck.py": "f70f0eeffe30a31e2ad68f696ba280aba8a81cee747e087efca1f04101ad0d98",
}


def verify_pins():
    for name, expected in EXPECTED_PINS.items():
        path = HERE / "pins" / name
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != expected:
            raise SystemExit(f"pin SHA-256 mismatch for {name}: {actual}")


if __name__ == "__main__":
    verify_pins()
    raise SystemExit(HARNESS.MODULE.main())
