#!/usr/bin/env python3
"""Paired pure-constructor calibration, with independent graph validation."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[3]
run = Path(sys.argv[1]).resolve()
case = int(os.environ["SLURM_ARRAY_TASK_ID"])
assert os.environ.get("SLURM_JOB_ID")
manifest = json.loads((run / "manifest.json").read_text())
for name, expected in manifest["bindings"].items():
    assert hashlib.sha256((root / name).read_bytes()).hexdigest() == expected, name
entry = manifest["cases"][case]
graph = run / entry["file"]
tokens = list(map(int, graph.read_text().split()))
n, separation = tokens[:2]
edges = list(zip(tokens[2::2], tokens[3::2]))
order = ["reference", "native"] if case % 2 == 0 else ["native", "reference"]
results = []
for arm in order:
    command = [str(root / manifest["binary"]), str(graph), arm]
    out = run / f"case-{case:02d}-{arm}.stdout"
    err = run / f"case-{case:02d}-{arm}.stderr"
    result = {"case": case, "arm": arm, "command": command,
              "job_id": os.environ["SLURM_JOB_ID"], "validated": False}
    start = time.monotonic()
    with out.open("w") as stdout, err.open("w") as stderr:
        try:
            process = subprocess.run(command, stdout=stdout, stderr=stderr,
                                     timeout=manifest["arm_timeout_seconds"])
            result["returncode"] = process.returncode
            result["timeout"] = False
        except subprocess.TimeoutExpired:
            result["returncode"] = None
            result["timeout"] = True
    result["process_seconds"] = time.monotonic() - start
    if result["returncode"] == 0:
        measured = json.loads(out.read_text())
        labels = measured["labels"]
        assert sorted(labels) == list(range(n)), "not a permutation"
        deficits = [max(0, separation - abs(labels[u] - labels[v])) for u, v in edges]
        bad = sum(d > 0 for d in deficits)
        cost = sum(d * d for d in deficits)
        assert (bad, cost) == (measured["bad"], measured["cost"]), "wrong graph objective"
        assert (measured["vertices"], measured["separation"], measured["edges"]) == (n, separation, len(edges))
        result.update(validated=True, graph_satisfied=bad == 0, measurement=measured)
    results.append(result)
    (run / f"result-{case:02d}.json").write_text(json.dumps({"case": entry,
        "order": order, "results": results, "complete": len(results) == 2}, indent=2) + "\n")
print(json.dumps({"case": case, "complete": True, "results": results}, indent=2))
