#!/usr/bin/env python3
"""Run one Rank-17 schedule-member arm through the frozen 3710 harness."""

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import sys


HERE = Path(__file__).resolve().parent
SOURCE = (HERE.parent /
          "20260831-3710-tierd-rank17-standalone-bve-schedule/run-one.py")
EXPECTED_SOURCE_SHA256 = "f8ceb784547e980eb72e5475b60a9ed32b35b881b3311c3c4d24c0e7dfcf6ab7"
if hashlib.sha256(SOURCE.read_bytes()).hexdigest() != EXPECTED_SOURCE_SHA256:
    raise SystemExit("frozen 3710 harness SHA-256 mismatch")

SPEC = importlib.util.spec_from_file_location("rank17_bve_schedule", SOURCE)
HARNESS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(HARNESS)
HARNESS.HERE = HERE
HARNESS.ARMS = {
    0: ("tactix", "TACTIX", "8d18b187d4102a716619236bf2995644a44143637f249be7b67e1cae046b3413"),
    1: ("kissat", "KISSAT", "3d12a6abbeeed3df6d2aab49eecc3b8e9db350545ea84af57c890be8feab9483"),
}


def repair_result(task: int) -> bool:
    arm = HARNESS.ARMS[task][0]
    trace_path = HERE / "traces" / f"{arm}.tsv"
    schedule_rows = [
        line for line in trace_path.read_text().splitlines()
        if line and not line.startswith("#") and
        not line.startswith("member\t")
    ]
    result_path = HERE / "results" / f"{task:03d}.json"
    result = json.loads(result_path.read_text())
    original_valid = result["valid"]
    valid = (not result["timed_out"] and result["returncode"] == 0 and
             len(schedule_rows) == 1 and
             schedule_rows[0].startswith(f"{arm}\tbound=0|scheduled="))
    result.update({
        "valid": valid,
        "schedule_row": schedule_rows[0] if len(schedule_rows) == 1 else None,
        "classification_repaired": True,
        "original_valid": original_valid,
        "classification_note":
            "Frozen 3710 validation counted member rows as schedule rows; "
            "the repaired classifier selects the sole non-comment, "
            "non-member summary row without rerunning either solver.",
    })
    result_path.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    return valid


if __name__ == "__main__":
    if sys.argv[1:] == ["--repair-existing"]:
        raise SystemExit(0 if all(repair_result(task) for task in (0, 1)) else 1)
    os.environ["SAT_BVE_SCHEDULE_MEMBERS"] = "1"
    task = int(os.environ["SLURM_ARRAY_TASK_ID"])
    HARNESS.main()
    raise SystemExit(0 if repair_result(task) else 1)
