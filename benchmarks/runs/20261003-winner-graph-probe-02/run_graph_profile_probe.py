#!/usr/bin/env python3
"""One candidate-only SLURM probe with retained output and independent oracles."""
import hashlib
import json
import lzma
import os
from pathlib import Path
import re
import resource
import shutil
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tools/bench"))
import run as bench

run = Path(sys.argv[1]).resolve()
assert os.environ.get("SLURM_JOB_ID"), "real solver execution requires SLURM"
manifest = json.loads((run / "manifest.json").read_text())
for relative, digest in manifest["bindings"].items():
    assert hashlib.sha256((root / relative).read_bytes()).hexdigest() == digest, relative
temporary = run / "tmp"
temporary.mkdir(exist_ok=True)
cnf = temporary / "original.cnf"
with lzma.open(root / manifest["input"], "rb") as source, cnf.open("wb") as out:
    shutil.copyfileobj(source, out)
proof = temporary / "candidate.dsr"
stdout = run / "solver.stdout"
stderr = run / "solver.stderr"
env = {k: v for k, v in os.environ.items() if not k.startswith("TACTIX_")}
env["TACTIX_PROBE_GRAPH_PROFILE"] = str(run / "profile.txt")
command = [str(root / manifest["pin"] / "tactix-graph-profile"), "--dsr", str(cnf), str(proof)]
def limits():
    seconds = manifest["solver_timeout_seconds"]
    resource.setrlimit(resource.RLIMIT_CPU, (seconds + 5, seconds + 7))
start = time.monotonic()
result = {"command": command, "job_id": os.environ["SLURM_JOB_ID"],
          "model_ok": False, "proof_ok": False, "target_closed": False}
with stdout.open("w") as out, stderr.open("w") as err:
    try:
        process = subprocess.run(command, stdout=out, stderr=err, env=env,
                                 timeout=manifest["solver_timeout_seconds"], preexec_fn=limits)
        result["returncode"] = process.returncode
        result["solver_timeout"] = False
    except subprocess.TimeoutExpired:
        result["returncode"] = None
        result["solver_timeout"] = True
result["solver_seconds"] = time.monotonic() - start
output = stdout.read_text(errors="replace")
result["verdict"] = bench.parse_verdict(result["returncode"], output)
result["graph_statistics"] = [line for line in output.splitlines() if line.startswith("c graphlabel")]
result["proof_bytes"] = proof.stat().st_size if proof.exists() else None
start = time.monotonic()
if result["verdict"] == "SAT":
    check_command = [bench.CHECKMODEL, str(cnf), str(stdout)]
    result["checker_command"] = check_command
    try:
        checked = subprocess.run(check_command, capture_output=True, text=True,
                                 timeout=bench.MODEL_CHECK_BUDGET)
        (run / "checker.stdout").write_text(checked.stdout)
        (run / "checker.stderr").write_text(checked.stderr)
        result["checker_returncode"] = checked.returncode
        result["model_ok"] = checked.returncode == 0
        result["target_closed"] = result["model_ok"]
    except subprocess.TimeoutExpired:
        result["checker_timeout"] = True
elif result["verdict"] == "UNSAT":
    route = re.search(r"^c proof-route (\S+)$", output, re.M)
    ok, message, kind, details = bench.check_proof(
        "dsr", str(cnf), str(proof), manifest["checker_timeout_seconds"],
        allow_pbp=False, route=route.group(1) if route else None)
    result.update(proof_ok=ok, checker_message=message, checker_kind=kind,
                  checker_details=details, expected_verdict_mismatch=True)
    # This frozen target already has independently checked SAT models.
    # An apparent UNSAT certificate must be investigated, never closed here.
result["checker_seconds"] = time.monotonic() - start
(run / "result.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result, indent=2))
