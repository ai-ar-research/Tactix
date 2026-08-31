#!/usr/bin/env python3
"""Run the fixed-active Tactix Rank-17 BVE-entry summary."""

import hashlib
import importlib.util
from pathlib import Path


HERE = Path(__file__).resolve().parent
SOURCE = (HERE.parent /
          "20260831-3710-tierd-rank17-standalone-bve-schedule/run-one.py")
EXPECTED_SOURCE_SHA256 = "f8ceb784547e980eb72e5475b60a9ed32b35b881b3311c3c4d24c0e7dfcf6ab7"
if hashlib.sha256(SOURCE.read_bytes()).hexdigest() != EXPECTED_SOURCE_SHA256:
    raise SystemExit("frozen 3710 harness SHA-256 mismatch")

SPEC = importlib.util.spec_from_file_location("rank17_fixed_active", SOURCE)
HARNESS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(HARNESS)
HARNESS.HERE = HERE
HARNESS.ARMS = {
    0: ("tactix", "TACTIX", "968604bdb8937d092b48b962fe380d6c2694d380a09ebfc93e0580f281fc0c4f"),
}


if __name__ == "__main__":
    raise SystemExit(HARNESS.main())
