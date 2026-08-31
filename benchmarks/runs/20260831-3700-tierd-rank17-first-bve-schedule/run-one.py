#!/usr/bin/env python3
"""Stop one immutable Rank-17 trace arm at its first bound-0 BVE schedule."""

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
AUTHORITY = (ROOT / "benchmarks/runs/"
             "20260831-3610-tierd-rank34-transitive-unit-causal/pins")
ARMS = {
    0: ("tactix", "TACTIX", AUTHORITY / "tactix-trace",
        "c8b7737054990c4558b5d25f89870e4bfe82ae062ca888fcab901a83df518f11"),
    1: ("kissat", "KISSAT", AUTHORITY / "kissat-trace",
        "b19089eb8ecdfb7709f8f13345bfd0497dcd7a6a356d2cd280454dffd60c5cb5"),
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def tail(text: str, count: int = 20):
    if isinstance(text, bytes):
        text = text.decode("utf-8", errors="replace")
    return text.splitlines()[-count:]


def main() -> int:
    task = int(os.environ["SLURM_ARRAY_TASK_ID"])
    arm, prefix, solver, expected_solver_sha = ARMS[task]
    if sha256(INPUT_XZ) != INPUT_SHA256:
        raise SystemExit("Rank-17 input SHA-256 mismatch")
    if sha256(solver) != expected_solver_sha:
        raise SystemExit(f"{arm} trace-binary SHA-256 mismatch")

    scratch_root = Path(os.environ.get("SLURM_TMPDIR", "/tmp"))
    started_at = time.time()
    result = {
        "schema": "tactix-tierd-rank17-first-bve-schedule-result-v1",
        "task": task,
        "arm": arm,
        "expected_input_sha256": INPUT_SHA256,
        "expected_solver_sha256": expected_solver_sha,
        "scoring": False,
    }
    with tempfile.TemporaryDirectory(
            prefix=f"rank17-first-bve-{os.environ.get('SLURM_JOB_ID', 'local')}_{task}-",
            dir=scratch_root) as scratch_name:
        scratch = Path(scratch_name)
        input_path = scratch / "rank17.cnf"
        trace_path = scratch / "event.tsv"
        with lzma.open(INPUT_XZ, "rb") as source, input_path.open("wb") as target:
            shutil.copyfileobj(source, target)

        env = os.environ.copy()
        env[f"{prefix}_EVENT_TRACE"] = str(trace_path)
        env[f"{prefix}_EVENT_TRACE_LIMIT"] = "16"
        env["SAT_EVENT_TRACE_EXIT_AFTER_MATCH"] = "bve_schedule|bound=0"
        env["SAT_EVENT_TRACE_TARGET_ONLY"] = "1"
        cpu = min(os.sched_getaffinity(0))
        before = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
        wall_start = time.monotonic()
        try:
            process = subprocess.run(
                ["taskset", "-c", str(cpu), str(solver), str(input_path)],
                env=env, capture_output=True, text=True, timeout=900,
                check=False)
            timed_out = False
        except subprocess.TimeoutExpired as error:
            process = error
            timed_out = True
        wall_seconds = time.monotonic() - wall_start
        after = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss

        rows = []
        if trace_path.exists():
            rows = [line for line in trace_path.read_text().splitlines()
                    if line and not line.startswith("#") and
                    not line.startswith("solver\t")]
        schedule_rows = [line for line in rows
                         if len(line.split("\t")) > 2 and
                         line.split("\t")[2] == "bve_schedule"]
        returncode = None if timed_out else process.returncode
        valid = (not timed_out and returncode == 0 and
                 len(schedule_rows) == 1 and "bound=0" in schedule_rows[0])
        retained_trace = HERE / "traces" / f"{arm}.tsv"
        if trace_path.exists():
            shutil.copy2(trace_path, retained_trace)
        result.update({
            "valid": valid,
            "timed_out": timed_out,
            "returncode": returncode,
            "wall_seconds": round(wall_seconds, 3),
            "child_maxrss_kib": max(before, after),
            "schedule_row": schedule_rows[0] if len(schedule_rows) == 1 else None,
            "trace_sha256": sha256(retained_trace) if retained_trace.exists() else None,
            "stdout_tail": tail(process.stdout or ""),
            "stderr_tail": tail(process.stderr or ""),
            "intentional_pre_solve_exit": True,
            "artifact_deleted": True,
            "derived_artifacts_deleted": True,
            "finished_at_epoch": time.time(),
            "elapsed_total_seconds": round(time.time() - started_at, 3),
        })

    result_path = HERE / "results" / f"{task:03d}.json"
    temporary = result_path.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    temporary.replace(result_path)
    return 0 if result["valid"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
