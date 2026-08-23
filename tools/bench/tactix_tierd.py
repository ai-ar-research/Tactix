#!/usr/bin/env python3
"""Run one pinned Tactix pass over the frozen SC24--26 Tier-D union.

This is deliberately a separate, mutable comparison campaign.  It references
the once-only baseline dataset by hash but never writes into baseline results.
The task/check/result implementation is shared with ``frozen_baseline.py`` so
the Tactix and reference measurements have the same resource and certificate
contract.
"""

import argparse
import datetime as dt
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import frozen_baseline as frozen  # noqa: E402

REFERENCE = ROOT / "benchmarks/baselines/sc24-26-32g-5000s"
DEFAULT_CAMPAIGN = ROOT / "benchmarks/runs/20260813-tierd-tactix-sc24-26-32g-5000s"
PIN = "8f304f9"


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def atomic_json(path, value, exclusive=False):
    flags = os.O_WRONLY | os.O_CREAT | (os.O_EXCL if exclusive else os.O_TRUNC)
    fd = os.open(path, flags, 0o640)
    with os.fdopen(fd, "w") as out:
        json.dump(value, out, indent=2, sort_keys=True)
        out.write("\n")


def link_or_copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    try:
        os.link(source, target)
    except OSError:
        shutil.copy2(source, target)


def copy_frozen_binary(source, target):
    """Make a campaign-owned executable copy, never a mutable build hardlink."""
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
    target.chmod(target.stat().st_mode & ~0o222)
    if source.stat().st_ino == target.stat().st_ino:
        raise SystemExit("campaign binary pin unexpectedly shares its source inode")


def slurm_array_spec(count, throttle):
    spec = f"0-{count - 1}"
    return spec + (f"%{throttle}" if throttle else "")


def prepare(campaign, binary, source_commit, solve_limit, memory_gib,
            array_throttle, array_throttle_reason):
    if campaign.exists():
        raise SystemExit(f"campaign already exists: {campaign}")
    head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, check=True,
                          capture_output=True, text=True).stdout.strip()
    if head != source_commit:
        raise SystemExit(f"HEAD {head} differs from requested pin {source_commit}")
    if subprocess.run(["git", "diff", "--quiet", source_commit, "--", "src", "include"],
                      cwd=ROOT).returncode:
        raise SystemExit(f"production source differs from pinned commit {source_commit}")
    if not binary.is_file():
        raise SystemExit(f"missing Tactix release binary: {binary}")
    if solve_limit <= 0 or memory_gib <= 0:
        raise SystemExit("solve limit and memory must be positive")
    if array_throttle is not None and array_throttle <= 0:
        raise SystemExit("array throttle must be positive")
    if (array_throttle is None) != (array_throttle_reason is None):
        raise SystemExit("array throttle and its reason must be specified together")

    reference = json.loads((REFERENCE / "manifest.json").read_text())
    if sha256(REFERENCE / "manifest.json") != json.loads(
            (REFERENCE / "launch.json").read_text())["manifest_sha256"]:
        raise SystemExit("frozen baseline manifest no longer matches its launch hash")

    campaign.mkdir(parents=True)
    for directory in ("claims", "logs", "pins", "results", "scratch", "retained"):
        (campaign / directory).mkdir()
    copy_frozen_binary(binary, campaign / "pins/tactix")
    wrapper = campaign / "pins/run-tactix.sh"
    wrapper.write_text(
        "#!/usr/bin/env bash\n"
        "set -eu\n"
        "exec \"$(dirname \"$0\")/tactix\" \"$1\" \"$2/proof.out\"\n")
    wrapper.chmod(0o750)

    needed = ("checkmodel", "drat-trim", "cake-lpr")
    checkers = {name: reference["checkers"][name] for name in needed}
    for rel in checkers.values():
        link_or_copy(REFERENCE / rel, campaign / rel)
    # checkmodel is a pinned CLI plus its adjacent independent parsing module.
    link_or_copy(REFERENCE / "pins/modelcheck.py", campaign / "pins/modelcheck.py")

    instances = reference["instances"]
    tasks = [{"instance": i, "solver": 0} for i in range(len(instances))]
    limits = dict(reference["limits"])
    limits["solve_wall_seconds"] = solve_limit
    limits["process_memory_gib"] = memory_gib
    slurm = dict(reference["slurm"])
    slurm["cpus_per_task"] = math.ceil(memory_gib / slurm["mem_per_cpu_gib"])
    slurm["array_throttle"] = array_throttle
    if array_throttle_reason is None:
        slurm.pop("array_throttle_reason", None)
    else:
        slurm["array_throttle_reason"] = array_throttle_reason
    manifest = {
        "schema": 1,
        "campaign": campaign.name,
        "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "comparison_only": True,
        "baseline_authority": str(REFERENCE.relative_to(ROOT)),
        "baseline_manifest_sha256": sha256(REFERENCE / "manifest.json"),
        "dataset_sha256": reference["dataset_sha256"],
        "instances_list_sha256": reference["instances_list_sha256"],
        "cache_manifest_sha256": reference["cache_manifest_sha256"],
        "certificate_policy": reference["certificate_policy"],
        "limits": limits,
        "slurm": slurm,
        "policy": reference["policy"],
        "instances": instances,
        "tasks": tasks,
        "solvers": [{
            "name": "tactix",
            "kind": "output-directory",
            "executable": "pins/run-tactix.sh",
            "certificate": "drat",
            "checker_pipeline": "drat-cake-lpr",
            "vbs_eligible": "SAT+UNSAT",
        }],
        "checkers": checkers,
        "provenance": {name: reference["provenance"][name] for name in needed},
        "auxiliary_provenance": {
            "modelcheck.py": {
                "path": "pins/modelcheck.py",
                "sha256": sha256(campaign / "pins/modelcheck.py"),
            },
        },
        "build_provenance": {
            "git_commit": source_commit,
            "binary_sha256": sha256(campaign / "pins/tactix"),
            "binary_pin": "independent read-only campaign copy",
            "cmake_build_type": "Release",
        },
        "measurement_runner": {
            "path": str(Path(frozen.__file__).resolve().relative_to(ROOT)),
            "sha256": sha256(Path(frozen.__file__).resolve()),
        },
        "campaign_runner": {
            "path": str(Path(__file__).resolve().relative_to(ROOT)),
            "sha256": sha256(Path(__file__).resolve()),
        },
    }
    atomic_json(campaign / "manifest.json", manifest, exclusive=True)
    print(json.dumps({"campaign": str(campaign), "tasks": len(tasks),
                      "manifest_sha256": sha256(campaign / "manifest.json")}, indent=2))


def validate(campaign):
    return frozen.validate(campaign)


def aggregate(campaign):
    manifest = json.loads((campaign / "manifest.json").read_text())
    results, missing = [], []
    for index in range(len(manifest["tasks"])):
        path = campaign / "results" / f"{index:05d}.json"
        if path.exists():
            results.append(json.loads(path.read_text()))
        else:
            missing.append(index)
    claimed = {int(path.stem) for path in (campaign / "claims").glob("*.json")}
    interrupted = sorted(claimed - {row["task"] for row in results})
    orphan_artifacts = []
    certificate_suffixes = {".drat", ".dsr", ".lrat", ".lsr", ".pbp"}
    for index in interrupted:
        scratch = campaign / "scratch" / f"{index:05d}"
        if scratch.exists():
            orphan_artifacts.extend(
                str(path.relative_to(campaign)) for path in scratch.rglob("*")
                if path.is_file() and ("proof" in path.name or
                                       "certificate" in path.name or
                                       path.suffix in certificate_suffixes))
    retained = campaign / "retained"
    if retained.exists():
        orphan_artifacts.extend(str(path.relative_to(campaign))
                                for path in retained.rglob("*") if path.is_file())
    summary = {
        "schema": 1,
        "manifest_sha256": sha256(campaign / "manifest.json"),
        "complete": not missing,
        "task_count": len(manifest["tasks"]),
        "result_count": len(results),
        "missing_tasks": missing,
        "interrupted_claims": interrupted,
        "cleanup_pending": (sum(bool(row.get("cleanup_pending")) for row in results)
                            + len(orphan_artifacts)),
        "orphan_artifact_paths": orphan_artifacts,
        "invalid": sum(row.get("valid") is False for row in results),
        "results": results,
    }
    atomic_json(campaign / "results.json", summary)
    print(json.dumps({key: value for key, value in summary.items()
                      if key != "results"}, indent=2))
    return 0 if summary["complete"] else 2


def submit(campaign):
    manifest = json.loads((campaign / "manifest.json").read_text())
    runner = Path(frozen.__file__).resolve()
    if sha256(runner) != manifest["measurement_runner"]["sha256"]:
        raise SystemExit("measurement runner changed after validation; reprepare")
    if sha256(Path(__file__).resolve()) != manifest["campaign_runner"]["sha256"]:
        raise SystemExit("campaign runner changed after validation; reprepare")
    validation = campaign / "validation.json"
    if not validation.is_file() or not json.loads(validation.read_text()).get("passed"):
        raise SystemExit("successful compute-node validation is required")
    if (campaign / "launch.json").exists():
        raise SystemExit("Tactix comparison campaign was already submitted")
    count = len(manifest["tasks"])
    slurm = manifest["slurm"]
    array_spec = slurm_array_spec(count, slurm.get("array_throttle"))
    array = campaign / "array.sbatch"
    aggregate = campaign / "aggregate.sbatch"
    array.write_text(f"""#!/usr/bin/env bash
#SBATCH --partition={frozen.PARTITION}
#SBATCH --job-name=tactix-tierd-sc24-26
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task={slurm['cpus_per_task']}
#SBATCH --mem-per-cpu={slurm['mem_per_cpu_gib']}G
#SBATCH --time=14:15:00
#SBATCH --array={array_spec}
#SBATCH --no-requeue
#SBATCH --output={campaign}/logs/task-%a.log
exec {sys.executable} {runner} --campaign {campaign} --run-one "$SLURM_ARRAY_TASK_ID"
""")
    aggregate.write_text(f"""#!/usr/bin/env bash
#SBATCH --partition={frozen.PARTITION}
#SBATCH --job-name=tactix-tierd-aggregate
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=01:00:00
#SBATCH --no-requeue
#SBATCH --output={campaign}/logs/aggregate.log
exec {sys.executable} {Path(__file__).resolve()} --campaign {campaign} --aggregate
""")
    array_id = subprocess.run(["sbatch", "--parsable", array], check=True,
                              capture_output=True, text=True).stdout.strip().split(";")[0]
    aggregate_id = subprocess.run(
        ["sbatch", "--parsable", f"--dependency=afterany:{array_id}", aggregate],
        check=True, capture_output=True, text=True).stdout.strip().split(";")[0]
    launch = {"array_job_id": array_id, "aggregate_job_id": aggregate_id,
              "tasks": count, "manifest_sha256": sha256(campaign / "manifest.json"),
              "submitted_utc": dt.datetime.now(dt.timezone.utc).isoformat()}
    atomic_json(campaign / "launch.json", launch, exclusive=True)
    print(json.dumps(launch, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--campaign", type=Path, default=DEFAULT_CAMPAIGN)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/release/tactix")
    parser.add_argument("--source-commit", default=PIN)
    parser.add_argument("--solve-limit", type=int, default=frozen.SOLVE_LIMIT)
    parser.add_argument("--memory-gib", type=int, default=frozen.MEM_GIB)
    parser.add_argument("--array-throttle", type=int)
    parser.add_argument("--array-throttle-reason")
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--prepare", action="store_true")
    action.add_argument("--validate", action="store_true")
    action.add_argument("--submit", action="store_true")
    action.add_argument("--aggregate", action="store_true")
    args = parser.parse_args()
    if args.prepare:
        return prepare(args.campaign.resolve(), args.binary.resolve(),
                       args.source_commit, args.solve_limit, args.memory_gib,
                       args.array_throttle, args.array_throttle_reason)
    if args.validate:
        return validate(args.campaign.resolve())
    if args.aggregate:
        return aggregate(args.campaign.resolve())
    return submit(args.campaign.resolve())


if __name__ == "__main__":
    sys.exit(main() or 0)
