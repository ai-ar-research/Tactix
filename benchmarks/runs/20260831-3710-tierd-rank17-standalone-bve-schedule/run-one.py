#!/usr/bin/env python3
"""Run one standalone first-BVE-schedule observer on immutable Rank-17 pins."""

import hashlib
import json
import lzma
import os
from pathlib import Path
import resource
import shutil
import subprocess
import tempfile
import time


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
INPUT_XZ = (ROOT / "benchmarks/cache/tierD24-26/"
            "0a8a4c28d27228e954354ea0a6e7f16c-"
            "sum_of_three_cubes_42_known_representation.cnf.xz")
INPUT_SHA256 = "dc26f207a97200e5b52a46a62477381055b7f0ad41b962492f22a901ed1d1049"
ARMS = {
    0: ("tactix", "TACTIX", "141de0045c1acf6b98b3d3993a62b14b2aba705a66cd18a475f4b1ea82d1057d"),
    1: ("kissat", "KISSAT", "5d6a8c0834a167dd4d2087e65e5f7ad98fab7655437b0aa3a6a371d8d427ad37"),
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def text_tail(value, count: int = 20):
    if isinstance(value, bytes):
        value = value.decode("utf-8", errors="replace")
    return (value or "").splitlines()[-count:]


def main() -> int:
    task = int(os.environ["SLURM_ARRAY_TASK_ID"])
    arm, prefix, expected_solver_sha = ARMS[task]
    solver = HERE / "pins" / f"{arm}-trace"
    if sha256(INPUT_XZ) != INPUT_SHA256:
        raise SystemExit("Rank-17 input SHA-256 mismatch")
    if sha256(solver) != expected_solver_sha:
        raise SystemExit(f"{arm} trace-binary SHA-256 mismatch")

    scratch_root = Path(os.environ.get("SLURM_TMPDIR", "/tmp"))
    result = {
        "schema": "tactix-tierd-rank17-standalone-bve-schedule-result-v1",
        "task": task,
        "arm": arm,
        "expected_input_sha256": INPUT_SHA256,
        "expected_solver_sha256": expected_solver_sha,
        "scoring": False,
        "intentional_pre_solve_exit": True,
    }
    with tempfile.TemporaryDirectory(
            prefix=f"rank17-bve-schedule-{os.environ.get('SLURM_JOB_ID', 'local')}_{task}-",
            dir=scratch_root) as scratch_name:
        scratch = Path(scratch_name)
        input_path = scratch / "rank17.cnf"
        observer_path = scratch / "bve-schedule.tsv"
        with lzma.open(INPUT_XZ, "rb") as source, input_path.open("wb") as target:
            shutil.copyfileobj(source, target)

        env = os.environ.copy()
        env[f"{prefix}_BVE_SCHEDULE_TRACE"] = str(observer_path)
        cpu = min(os.sched_getaffinity(0))
        wall_start = time.monotonic()
        try:
            process = subprocess.run(
                ["taskset", "-c", str(cpu), str(solver), str(input_path)],
                env=env, capture_output=True, text=True, timeout=360,
                check=False)
            timed_out = False
        except subprocess.TimeoutExpired as error:
            process = error
            timed_out = True
        wall_seconds = time.monotonic() - wall_start
        maxrss_kib = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss

        rows = []
        if observer_path.exists():
            rows = [line for line in observer_path.read_text().splitlines()
                    if line and not line.startswith("#")]
        schedule_row = rows[0] if len(rows) == 1 else None
        returncode = None if timed_out else process.returncode
        valid = (not timed_out and returncode == 0 and len(rows) == 1 and
                 rows[0].startswith(f"{arm}\tbound=0|scheduled="))
        retained = HERE / "traces" / f"{arm}.tsv"
        if observer_path.exists():
            shutil.copy2(observer_path, retained)
        result.update({
            "valid": valid,
            "timed_out": timed_out,
            "returncode": returncode,
            "wall_seconds": round(wall_seconds, 3),
            "child_maxrss_kib": maxrss_kib,
            "schedule_row": schedule_row,
            "observer_sha256": sha256(retained) if retained.exists() else None,
            "stdout_tail": text_tail(process.stdout),
            "stderr_tail": text_tail(process.stderr),
            "artifact_deleted": True,
            "derived_artifacts_deleted": True,
            "finished_at_epoch": time.time(),
        })

    result_path = HERE / "results" / f"{task:03d}.json"
    temporary = result_path.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    temporary.replace(result_path)
    return 0 if result["valid"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
