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
manifest = json.loads((C / "manifest.json").read_text())
for rel, expected in manifest['hashes'].items():
    assert hashlib.sha256((C / rel).read_bytes()).hexdigest() == expected, rel
scratch = Path(tempfile.mkdtemp(prefix="tactix-sweep-audit-",
                              dir=os.environ.get("SLURM_TMPDIR", "/tmp")))
env = dict(os.environ)
for key in list(env):
    if key.startswith(('TACTIX_', 'KISSAT_', 'SAT_EVENT_', 'SAT_CORE_', 'SAT_DECISION_', 'SAT_CHOICE_', 'SAT_BVE_', 'SAT_INPROCESS_', 'SAT_QUEUE_', 'SAT_VIVIFY_', 'SAT_RETRY_')):
        del env[key]
env[prefix + "_INPROCESS_BOUNDARY_TRACE"] = str(C/"traces"/(arm+".boundaries.tsv"))
env["SAT_INPROCESS_BOUNDARY_CONFLICT_FROM"] = "132816"
env[prefix + "_BVE_CANDIDATE_TRACE"] = str(C/"traces"/(arm+".bve.tsv"))
env["SAT_BVE_CANDIDATE_EXTERNAL"] = "10700"
env[prefix + "_CHOICE_STATE_TRACE"] = str(C/"traces"/(arm+".state.tsv"))
env["SAT_CHOICE_STATE_CONFLICT"] = "345034"
env["SAT_CHOICE_STATE_VARIABLE"] = "9959"
env[prefix + "_BVE_SCHEDULE_TRACE"] = str(C/"traces"/(arm+".schedule.tsv"))
env["SAT_BVE_SCHEDULE_CONFLICT"] = "343785"
env["SAT_BVE_SCHEDULE_ORDINAL"] = "0"
env["SAT_BVE_SCHEDULE_MEMBERS"] = "1"
env[prefix + "_RETRY_PRODUCER_TRACE"] = str(C/"traces"/(arm+".retry.tsv"))
env["SAT_RETRY_PRODUCER_FROM"] = "240040"
env["SAT_RETRY_PRODUCER_TO"] = "343785"
env["SAT_RETRY_PRODUCER_EXTERNAL"] = "10700"
env.update({prefix + "_DECISION_STREAM_TRACE": str(C/"traces"/(arm+".decisions.tsv")),
            prefix + "_DECISION_PREFIX_TRACE": str(C/"traces"/(arm+".prefix.tsv")),
            "SAT_DECISION_PREFIX_CONFLICT": "345034",
            "SAT_DECISION_PREFIX_COMMON": "1",
            "SAT_DECISION_PREFIX_STOP": "1"})
try:
    cnf = scratch / "input.cnf"
    compressed = ROOT / 'benchmarks/cache/tierD24-26/0ae917c2e0c1d39cbe411c29b2f5ff08-dislog_a14_x14_n24.cnf.xz'
    assert hashlib.sha256(compressed.read_bytes()).hexdigest() == manifest['input_sha256_compressed']
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
