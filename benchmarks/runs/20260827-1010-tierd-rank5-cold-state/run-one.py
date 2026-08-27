#!/usr/bin/env python3
"""Confirm Rank-5 memory with cold post-Lucky state deferred."""

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import sys


CAMPAIGN = Path(__file__).resolve().parent
SHARED = (CAMPAIGN.parent /
          "20260827-0100-tierd-ranks1-40-slurm-memory-screen/run-one.py")
EXPECTED_RUNNER = "10b1605ca06e48641da3c1115ccf2bf5c1cd2846a940525fc994f51c95c59385"
EXPECTED_BINARY = "c28ae9f11b0478eb7b1d1fb102a3b8f7640b10c8ab0e2dd6b3adb7e337328ce6"


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def main():
    if sha256(SHARED) != EXPECTED_RUNNER:
        raise SystemExit("shared runner hash mismatch")
    if sha256(CAMPAIGN / "pins/tactix-tactix") != EXPECTED_BINARY:
        raise SystemExit("trace binary hash mismatch")
    task = int(sys.argv[1])
    stems = {
        "parse": "TACTIX_PARSE_MEMORY_TRACE",
        "congruence": "TACTIX_CONGRUENCE_MEMORY_TRACE",
        "substitute": "TACTIX_SUBSTITUTE_MEMORY_TRACE",
        "lucky": "TACTIX_LUCKY_MEMORY_TRACE",
        "slots": "TACTIX_SLOT_MEMORY_TRACE",
    }
    for stem, variable in stems.items():
        os.environ[variable] = str(CAMPAIGN / "traces" /
                                   f"{task:03d}.{stem}.tsv")

    spec = importlib.util.spec_from_file_location("rank40_runner", SHARED)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.CAMPAIGN = CAMPAIGN
    result = module.main()

    result_path = CAMPAIGN / "results" / f"{task:03d}.json"
    record = json.loads(result_path.read_text())
    record["diagnostic_traces"] = {}
    for stem in stems:
        path = CAMPAIGN / "traces" / f"{task:03d}.{stem}.tsv"
        record["diagnostic_traces"][stem] = {
            "path": str(path.relative_to(CAMPAIGN)),
            "exists": path.is_file(),
            "lines": len(path.read_text().splitlines()) if path.is_file() else 0,
            "sha256": sha256(path) if path.is_file() else None,
        }
    module.atomic_json(result_path, record)
    return result


if __name__ == "__main__":
    raise SystemExit(main())
