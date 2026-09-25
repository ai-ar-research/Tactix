#!/usr/bin/env python3
"""Prepare, but do not submit, the two-arm Tier-C checker-timeout diagnostic.

Both source binaries are the immutable builds used by the terminal AM427 C
array. This is a new diagnostic authority, not a continuation or alteration of
that frozen benchmark run.
"""

import csv
import hashlib
import json
from pathlib import Path
import shutil
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "tools/bench"))
from experimental_tierd import checker_sources  # noqa: E402

BASELINE = ROOT / "benchmarks/baselines/sc24-26-32g-5000s"
ORIGINAL = ROOT / "benchmarks/runs/20260925-141208-C-am427-sweep-lifecycle"
INSTANCE = "tierC/aes_decry_2_rounds.debugged.cnf.xz"
BUILDS = {
    "candidate": (ROOT / "build/am427-sweep-lifecycle-release/tactix",
                  "c3eafb2bd382c2e1171f886f9cd9cf142dd92e3b538ce29e6accdce9597db2b8"),
    "parent": (ROOT / "build/am421-hint-release/tactix",
               "b34f079efc17bd6caf858c364ba19a5971bea4dcf36ffceed8265221e8c56242"),
}


def sha(path):
    h = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def main():
    if (HERE / "manifest.json").exists():
        raise SystemExit("campaign already prepared; refusing to change pins")
    original = json.loads((ORIGINAL / "manifest.json").read_text())
    if original["tasks"][24]["rel"] != INSTANCE or original["tasks"][25]["rel"] != INSTANCE:
        raise SystemExit("source Tier-C task mapping changed")
    if [original["tasks"][i]["solver"] for i in (24, 25)] != [0, 1]:
        raise SystemExit("source Tier-C arm mapping changed")
    baseline = json.loads((BASELINE / "manifest.json").read_text())
    sources = checker_sources(BASELINE, baseline, "auto")
    for name, (binary, expected) in BUILDS.items():
        if not binary.is_file() or sha(binary) != expected:
            raise SystemExit(f"{name} binary pin changed")
    compressed = ROOT / "benchmarks/cache" / INSTANCE
    if not compressed.is_file():
        raise SystemExit("original compressed input missing")

    for name in ("pins", "logs", "claims", "provisional", "results"):
        (HERE / name).mkdir(exist_ok=False)
    for name, source in sources.items():
        shutil.copy2(source, HERE / "pins" / name)
    for name in ("runtime_task.py", "proof_oracle.py", "anonymous_scratch.py"):
        shutil.copy2(ROOT / "tools/bench" / name, HERE / "pins" / name)
    arms = {}
    for name, (source, expected) in BUILDS.items():
        target = HERE / "pins" / f"tactix-{name}"
        shutil.copy2(source, target)
        if sha(target) != expected:
            raise SystemExit(f"{name} copied binary differs")
        arms[name] = {"path": str(target.relative_to(HERE)), "sha256": expected,
                      "arguments": [], "certificate": "auto"}

    rows = [dict(task=i, rank=13, arm=name, tier="C-diagnostic",
                 instance=INSTANCE, expected="UNSAT")
            for i, name in enumerate(("candidate", "parent"))]
    with (HERE / "matrix.tsv").open("x", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=list(rows[0]), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)
    (HERE / "array.sbatch").write_text(f"""#!/usr/bin/env bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --job-name=am434-aes-dsr-extended
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=8
#SBATCH --mem=32G
#SBATCH --time=13:15:00
#SBATCH --array=0-1
#SBATCH --no-requeue
#SBATCH --output={HERE}/logs/task-%a.log
set -euo pipefail
ulimit -c 0
exec python3 {HERE}/pins/runtime_task.py {HERE} "$SLURM_ARRAY_TASK_ID"
""")
    pins = {str(path.relative_to(HERE)): sha(path)
            for path in [*(HERE / "pins").iterdir(), HERE / "matrix.tsv",
                         HERE / "array.sbatch", HERE / "prepare.py"]}
    manifest = {
        "schema": "am434-aes-checker-timeout-diagnostic-v1",
        "purpose": "Recheck both AM427 Tier-C task-24/25 proofs beyond the 4800-second checker limit",
        "source_run": str(ORIGINAL.relative_to(ROOT)),
        "source_manifest_sha256": sha(ORIGINAL / "manifest.json"),
        "source_tasks": [24, 25],
        "arms": arms,
        "input_sha256": {INSTANCE: sha(compressed)},
        "scratch_policy": "anonymous-local-files-v1",
        "resource_contract": {
            "partition": "cpu-dense-hwulab-q", "cpus_per_task": 8,
            "memory_mib": 32768, "solve_timeout": 1200,
            "checker_timeout": 45000, "scheduler_wall_seconds": 47700,
            "array": "0-1", "throttle": None, "node_constraint": None,
        },
        "proof_contract": "Original-CNF content route with frozen no-option AM369 DSR/Trestle pins",
        "pin_sha256": pins,
    }
    with (HERE / "manifest.json").open("x") as target:
        json.dump(manifest, target, indent=2, sort_keys=True)
        target.write("\n")
    for path in [*(HERE / "pins").iterdir(), HERE / "matrix.tsv",
                 HERE / "array.sbatch", HERE / "manifest.json"]:
        path.chmod(path.stat().st_mode & ~0o222)
    print(json.dumps({"campaign": str(HERE), "manifest_sha256": sha(HERE / "manifest.json"),
                      "tasks": len(rows)}))


if __name__ == "__main__":
    main()
