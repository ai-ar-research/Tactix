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
    if key.startswith(('TACTIX_', 'KISSAT_', 'SAT_EVENT_', 'SAT_CORE_', 'SAT_DECISION_', 'SAT_CHOICE_')):
        del env[key]
env.update({prefix + "_CHOICE_STATE_TRACE": str(C/"traces"/(arm+".state.tsv")),
            "SAT_CHOICE_STATE_CONFLICT": "95801", "SAT_CHOICE_STATE_VARIABLE": "5416"})
env.update({prefix + "_DECISION_STREAM_TRACE": str(C/"traces"/(arm+".decisions.tsv")),
            prefix + "_DECISION_PREFIX_TRACE": str(C/"traces"/(arm+".prefix.tsv")),
            "SAT_DECISION_PREFIX_CONFLICT": "95801",
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
