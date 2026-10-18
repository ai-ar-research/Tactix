#!/usr/bin/env python3
"""Prepare a pinned, zero-retention CryptoMiniSat no-matrix family probe."""

import argparse
import copy
import datetime as dt
import json
import shutil
from pathlib import Path

from frozen_baseline import atomic_json, sha256

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "benchmarks/runs/20261017-cms-tierd-cpuq-anon"


def prepare(destination, indices):
    base = json.loads((BASE / "manifest.json").read_text())
    if destination.exists():
        raise RuntimeError(f"probe already exists: {destination}")
    if not indices or len(set(indices)) != len(indices):
        raise RuntimeError("indices must be nonempty and unique")
    if any(i < 0 or i >= len(base["instances"]) for i in indices):
        raise RuntimeError("index outside frozen Tier D")
    for i in indices:
        result = json.loads((BASE / "results" / f"{i:05d}.json").read_text())
        if result.get("task") != i or result.get("valid") is not True or \
                result.get("solve", {}).get("wall_seconds", 1e9) > 600:
            raise RuntimeError(f"source row {i} is not a verified <=600 s CMS solve")

    for name in ("results", "pending", "claims", "scratch", "logs"):
        (destination / name).mkdir(parents=True, exist_ok=True)
    shutil.copytree(BASE / "pins", destination / "pins")
    wrapper = destination / "pins/cms-wrapper"
    wrapper.chmod(0o750)
    wrapper.write_text(
        "#!/usr/bin/env bash\nset -euo pipefail\n"
        f"export LD_LIBRARY_PATH='{destination}/pins':${{LD_LIBRARY_PATH:-}}\n"
        f"exec '{destination}/pins/solver' --zero-exit-status --verb 2 "
        "--maxnummatrices 0 \"$1\" \"$2\"\n")
    wrapper.chmod(0o555)

    manifest = copy.deepcopy(base)
    manifest["kind"] = "cryptominisat-family-no-matrix-ablation-v1"
    manifest["campaign"] = destination.name
    manifest["created_utc"] = dt.datetime.now(dt.timezone.utc).isoformat()
    manifest["diagnostic_of"] = {
        "campaign": BASE.name,
        "manifest_sha256": sha256(BASE / "manifest.json"),
        "source_indices": indices,
    }
    manifest["instances"] = [base["instances"][i] for i in indices]
    manifest["tasks"] = [{"instance": i, "solver": 0}
                         for i in range(len(indices))]
    manifest["limits"]["solve_wall_seconds"] = 300
    manifest["limits"]["checker_pipeline_wall_seconds"] = 3600
    manifest["slurm"]["array_throttle"] = 2
    manifest["provenance"]["cryptominisat-5.14.7"] = {
        "path": "pins/cms-wrapper", "sha256": sha256(wrapper),
        "arguments": ["--zero-exit-status", "--verb", "2",
                      "--maxnummatrices", "0", "{cnf}", "{frat}"],
    }
    for entry in manifest["provenance"].values():
        if isinstance(entry, dict) and "path" in entry and \
                entry["path"].startswith("pins/") and \
                entry["path"] != "pins/cms-wrapper":
            pinned = destination / entry["path"]
            if sha256(pinned) != entry["sha256"]:
                raise RuntimeError(f"pin mismatch: {pinned}")
    atomic_json(destination / "manifest.json", manifest, exclusive=True)
    batch = destination / "run.sbatch"
    batch.write_text(f"""#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --job-name=cms-family-no-matrix
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=4
#SBATCH --mem=32G
#SBATCH --time=02:00:00
#SBATCH --array=0-{len(indices) - 1}%2
#SBATCH --no-requeue
#SBATCH --output={destination}/logs/task-%a.log
export TACTIX_REPO_ROOT={ROOT}
export PYTHONDONTWRITEBYTECODE=1
export TMPDIR=/tmp
exec python3 {destination}/pins/frozen_baseline.py --campaign {destination} --run-one "$SLURM_ARRAY_TASK_ID"
""")
    print(destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("indices", type=int, nargs="+")
    args = parser.parse_args()
    prepare(args.destination.resolve(), args.indices)


if __name__ == "__main__":
    main()
