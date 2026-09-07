#!/usr/bin/env python3
"""Bounded diagnostic, not a timing or verdict gate. One stopped solver per task."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

C = Path(__file__).resolve().parent
ROOT = C.parents[2]
arm = ("tactix", "kissat")[int(sys.argv[1])]
prefix = arm.upper()
assert os.environ.get("SLURM_JOB_ID")
scratch = Path(tempfile.mkdtemp(prefix="tactix-sweep-audit-",
                              dir=os.environ.get("SLURM_TMPDIR", "/tmp")))
env = dict(os.environ)
env.update({prefix + "_EVENT_TRACE": str(C / "traces" / (arm + ".tsv")),
            prefix + "_EVENT_TRACE_HEAP_POP_ONLY": "1",
            prefix + "_EVENT_TRACE_HEAP_EACH_BUMP": "1",
            prefix + "_EVENT_TRACE_HEAP_FROM_DECISION": "187082",
            prefix + "_EVENT_TRACE_HEAP_POP_DECISION": "187082",
            prefix + "_EVENT_TRACE_HEAP_REMOVE_DECISION": "187082",
            prefix + "_EVENT_TRACE_CONFLICT_FROM": "75451",
            prefix + "_EVENT_TRACE_CONFLICT_TO": "75451",
            prefix + "_EVENT_TRACE_STOP_AFTER_CONFLICT": "75452",
            prefix + "_EVENT_TRACE_LIMIT": "200000"})
try:
    cnf = scratch / "input.cnf"
    with cnf.open("wb") as out:
        subprocess.run(["xz", "-dc", ROOT / "benchmarks/cache/tierD24-26/0ae917c2e0c1d39cbe411c29b2f5ff08-dislog_a14_x14_n24.cnf.xz"], stdout=out, check=True)
    with (C / "logs" / (arm + ".out")).open("wb") as out:
        result = subprocess.run(["timeout", "600", "taskset", "-c",
                                 str(min(os.sched_getaffinity(0))), C / "pins" / arm,
                                 cnf, scratch / "partial.drat"],
                                stdout=out, stderr=subprocess.STDOUT, env=env)
    print(arm, "exit", result.returncode)
finally:
    shutil.rmtree(scratch)
    print("scratch_deleted", not scratch.exists())
