#!/usr/bin/env python3
"""Paired meter-only intervention, with graph and seeded-effect checks."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
run = Path(sys.argv[1]).resolve()
index = int(os.environ["SLURM_ARRAY_TASK_ID"])
assert os.environ.get("SLURM_JOB_ID")
manifest = json.loads((run / "manifest.json").read_text())
for name, expected in manifest["bindings"].items():
    assert hashlib.sha256((root / name).read_bytes()).hexdigest() == expected, name
case = manifest["cases"][index]
graph = run / case["file"]
data = list(map(int, graph.read_text().split()))
n, separation = data[:2]
edges = list(zip(data[2::2], data[3::2]))
order = ["simple", "kernel"] if index % 2 == 0 else ["kernel", "simple"]
results = []
env = {k: v for k, v in os.environ.items() if not k.startswith("TACTIX_")}
for meter in order:
    command = [str(root / manifest["binaries"][meter]), str(graph), "native"]
    result = {"meter": meter, "command": command, "validated": False}
    with (run / f"case-{index:02d}-{meter}.stdout").open("w") as out, \
         (run / f"case-{index:02d}-{meter}.stderr").open("w") as err:
        try:
            process = subprocess.run(command, stdout=out, stderr=err, env=env,
                                     timeout=manifest["arm_timeout_seconds"])
            result.update(returncode=process.returncode, timeout=False)
        except subprocess.TimeoutExpired:
            result.update(returncode=None, timeout=True)
    if result["returncode"] == 0:
        measured = json.loads((run / f"case-{index:02d}-{meter}.stdout").read_text())
        labels = measured["labels"]
        assert sorted(labels) == list(range(n))
        deficits = [max(0, separation - abs(labels[u] - labels[v])) for u, v in edges]
        assert (measured["bad"], measured["cost"]) == (sum(d > 0 for d in deficits), sum(d*d for d in deficits))
        result.update(validated=True, measurement=measured)
    results.append(result)
    (run / f"result-{index:02d}.json").write_text(json.dumps({"case": case,
        "job_id": os.environ["SLURM_JOB_ID"], "results": results,
        "complete": len(results) == 2}, indent=2) + "\n")
def effects(measurement):
    return {k: ([{a: b for a, b in stage.items() if a != "seconds"} for stage in value]
                if k == "stages" else value)
            for k, value in measurement.items() if k != "seconds"}
assert all(r["validated"] for r in results), "incomplete meter comparison"
assert effects(results[0]["measurement"]) == effects(results[1]["measurement"]), "meter changed seeded effects"
final = {"case": case, "job_id": os.environ["SLURM_JOB_ID"], "results": results,
         "complete": True, "seeded_effects_equal": True}
(run / f"result-{index:02d}.json").write_text(json.dumps(final, indent=2) + "\n")
print(json.dumps(final, indent=2))
