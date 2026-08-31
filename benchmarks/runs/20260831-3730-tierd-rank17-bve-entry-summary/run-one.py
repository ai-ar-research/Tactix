#!/usr/bin/env python3
"""Run one Rank-17 BVE-entry summary arm through the frozen 3710 harness."""

import hashlib
import importlib.util
from pathlib import Path


HERE = Path(__file__).resolve().parent
SOURCE = (HERE.parent /
          "20260831-3710-tierd-rank17-standalone-bve-schedule/run-one.py")
EXPECTED_SOURCE_SHA256 = "f8ceb784547e980eb72e5475b60a9ed32b35b881b3311c3c4d24c0e7dfcf6ab7"
if hashlib.sha256(SOURCE.read_bytes()).hexdigest() != EXPECTED_SOURCE_SHA256:
    raise SystemExit("frozen 3710 harness SHA-256 mismatch")

SPEC = importlib.util.spec_from_file_location("rank17_bve_entry", SOURCE)
HARNESS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(HARNESS)
HARNESS.HERE = HERE
HARNESS.ARMS = {
    0: ("tactix", "TACTIX", "2cd9bc4f5a50526662466cbfb88ade24f16ed88de4b966cd8d6739873e8ab69a"),
    1: ("kissat", "KISSAT", "acc02845ad16d75ffcb64c2df3e251a19da72c4e8e03d3f97379ee4da13470d4"),
}


if __name__ == "__main__":
    raise SystemExit(HARNESS.main())
