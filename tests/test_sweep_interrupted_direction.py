#!/usr/bin/env python3
"""A stopped second Sweep direction must retain the first binary consistently."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "fuzz"))
from modelcheck import check_files  # noqa: E402

cnf = ROOT / "tests" / "fixtures" / "sweep_interrupted_direction.cnf"
env = os.environ.copy()
env.update({
    "TACTIX_STRESS_RULES": "Sweep,Backbone",
    "TACTIX_STRESS_NORMAL_EVERY": "12",
    "TACTIX_STOP_FUZZ": "101",
})
with tempfile.TemporaryDirectory(prefix="tactix-sweep-interrupt-") as tmp:
    proof = Path(tmp) / "proof.lrat"
    output = Path(tmp) / "solver.out"
    run = subprocess.run(
        [sys.argv[1], "--lrat", "--stress-policy=22015793", str(cnf),
         str(proof)],
        env=env, text=True, capture_output=True, timeout=120,
    )
    assert run.returncode == 10, (run.returncode, run.stderr, run.stdout)
    output.write_text(run.stdout)
    ok, message = check_files(cnf, output)
    assert ok, message
print("test_sweep_interrupted_direction OK")
