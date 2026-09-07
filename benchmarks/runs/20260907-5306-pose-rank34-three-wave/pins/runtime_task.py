#!/usr/bin/env python3
"""Checked proof-on comparison: one immutable solver per SLURM task."""

from collections import deque
import csv
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time


CAMPAIGN = Path(sys.argv[1]).resolve()
ROOT = CAMPAIGN.parents[2]
SOLVE_TIMEOUT = 1200
CHECK_TIMEOUT = 16000


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def process_group():
    os.setsid()


def sample_vm_peak(root, stop, peak):
    """Sample the process tree; VmPeak is a per-process high-water mark."""
    while not stop.is_set():
        pending, seen = [root], set()
        while pending:
            pid = pending.pop()
            if pid in seen:
                continue
            seen.add(pid)
            try:
                children = Path(f"/proc/{pid}/task/{pid}/children").read_text()
                pending.extend(map(int, children.split()))
                for line in Path(f"/proc/{pid}/status").read_text().splitlines():
                    if line.startswith("VmPeak:"):
                        peak[0] = max(peak[0], int(line.split()[1]))
            except (OSError, ValueError):
                pass
        stop.wait(0.1)


def runtime_context(affinity):
    """Record placement and aggregate occupancy without other jobs' identities."""
    topology = []
    for cpu in affinity:
        base = Path(f"/sys/devices/system/cpu/cpu{cpu}")
        row = {"cpu": cpu}
        for key, relative in (("core", "topology/core_id"),
                              ("socket", "topology/physical_package_id"),
                              ("l3_shared_cpus", "cache/index3/shared_cpu_list")):
            try:
                row[key] = (base / relative).read_text().strip()
            except OSError:
                row[key] = None
        topology.append(row)
    try:
        occupancy = subprocess.run(
            ["squeue", "-h", "-w", os.environ.get("SLURMD_NODENAME", os.uname().nodename),
             "-o", "%P|%T|%C"], capture_output=True, text=True, timeout=15)
        occupancy = dict(exit_code=occupancy.returncode, rows=occupancy.stdout.splitlines(),
                         stderr=occupancy.stderr[-2000:])
    except (OSError, subprocess.TimeoutExpired) as error:
        occupancy = {"error": repr(error)}
    return dict(topology=topology, occupancy=occupancy,
                observed_at=dt.datetime.now(dt.timezone.utc).isoformat())


def measured(command, stdout, stderr, metrics, timeout):
    wrapped = ["/usr/bin/time", "-f", "max_rss_kb=%M", "-o", str(metrics),
               "--", *map(str, command)]
    started = time.monotonic()
    timed_out = False
    vm_peak = [0]
    with open(stdout, "wb") as out, open(stderr, "wb") as err:
        process = subprocess.Popen(wrapped, stdout=out, stderr=err,
                                   preexec_fn=process_group)
        stop = threading.Event()
        sampler = None
        if os.environ.get("TACTIX_RUNTIME_SAMPLE_VM") == "1":
            sampler = threading.Thread(target=sample_vm_peak,
                                       args=(process.pid, stop, vm_peak), daemon=True)
            sampler.start()
        try:
            exit_code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
            exit_code = None
        finally:
            stop.set()
            if sampler:
                sampler.join()
    peak_rss_kb = None
    try:
        match = re.search(r"^max_rss_kb=(\d+)$", metrics.read_text(), re.M)
        peak_rss_kb = int(match.group(1)) if match else None
    except OSError:
        pass
    return {
        "exit_code": exit_code,
        "wall_seconds": round(time.monotonic() - started, 3),
        "peak_rss_kb": peak_rss_kb,
        "sampled_process_vm_peak_kb": vm_peak[0] or None,
        "timed_out": timed_out,
    }


def tail(path, count=40):
    lines = deque(maxlen=count)
    try:
        with open(path, errors="replace") as source:
            for line in source:
                lines.append(line.rstrip("\n")[-4096:])
    except OSError:
        pass
    return list(lines)


def solver_summary(path):
    states = []
    selected = deque(maxlen=260)
    with open(path, errors="replace") as source:
        for raw in source:
            line = raw.rstrip("\n")
            match = re.match(r"^s\s+(SATISFIABLE|UNSATISFIABLE)\s*$", line)
            if match:
                states.append(match.group(1))
            if line.startswith(("c ", "s ")):
                selected.append(line[-4096:])
    verdict = "UNKNOWN"
    if len(states) == 1:
        verdict = "SAT" if states[0] == "SATISFIABLE" else "UNSAT"
    return verdict, list(selected)


def atomic_json(path, value):
    temporary = path.with_suffix(path.suffix + ".tmp")
    with open(temporary, "x") as target:
        json.dump(value, target, indent=2, sort_keys=True)
        target.write("\n")
    os.replace(temporary, path)


def main():
    if len(sys.argv) != 3 or not os.environ.get("SLURM_JOB_ID"):
        raise SystemExit("usage (SLURM only): runtime_task.py CAMPAIGN TASK")
    task = int(sys.argv[2])
    with open(CAMPAIGN / "matrix.tsv", newline="") as source:
        rows = list(csv.DictReader(source, delimiter="\t"))
    if task < 0 or task >= len(rows) or int(rows[task]["task"]) != task:
        raise SystemExit(f"invalid task {task}")
    row = rows[task]
    result_path = CAMPAIGN / "results" / f"{task:03d}.json"
    provisional_path = CAMPAIGN / "provisional" / f"{task:03d}.json"
    claim_path = CAMPAIGN / "claims" / f"{task:03d}.json"
    if result_path.exists() or claim_path.exists():
        raise SystemExit(f"task {task} already claimed or complete")
    atomic_json(claim_path, {
        "task": task,
        "node": os.environ.get("SLURMD_NODENAME", os.uname().nodename),
        "started_at": dt.datetime.now(dt.timezone.utc).isoformat(),
    })

    manifest = json.loads((CAMPAIGN / "manifest.json").read_text())
    compressed = ROOT / "benchmarks/cache" / row["instance"]
    arm = manifest["arms"][row["arm"]]
    binary = CAMPAIGN / arm["path"]
    scratch_root = Path(os.environ.get("SLURM_TMPDIR", tempfile.gettempdir()))
    scratch = Path(tempfile.mkdtemp(prefix=f"tactix-runtime-{task:03d}-",
                                    dir=scratch_root))
    cnf = scratch / "input.cnf"
    proof = scratch / "proof.drat"
    stdout = scratch / "solver.stdout"
    stderr = scratch / "solver.stderr"
    result = None
    try:
        if sha256(compressed) != manifest["input_sha256"][row["instance"]]:
            raise RuntimeError("compressed input hash mismatch")
        with open(cnf, "wb") as target:
            subprocess.run(["xz", "-dc", compressed], stdout=target, check=True)
        for rel, expected in manifest["pin_sha256"].items():
            if sha256(CAMPAIGN / rel) != expected:
                raise RuntimeError(f"pin hash mismatch: {rel}")
        for name in list(os.environ):
            if name.startswith(("TACTIX_", "KISSAT_", "SAT_EVENT_", "SAT_INPROCESS_")):
                del os.environ[name]
        affinity = sorted(os.sched_getaffinity(0))
        command = ["taskset", "-c", str(affinity[0]), binary,
                   *arm["arguments"], cnf, proof]
        context = None
        perf_path = scratch / "solver.perf"
        if manifest.get("collect_runtime_context"):
            context = runtime_context(affinity)
            os.environ["TACTIX_RUNTIME_SAMPLE_VM"] = "1"
            try:
                probe = subprocess.run(["perf", "stat", "-e", "cycles", "--", "true"],
                                       capture_output=True, text=True, timeout=15)
                context["perf_probe"] = dict(exit_code=probe.returncode, stderr=probe.stderr[-2000:])
                if probe.returncode == 0:
                    command = ["perf", "stat", "-x", "\t", "-o", perf_path, "-e",
                               "task-clock,cycles,instructions,context-switches,cpu-migrations,page-faults",
                               "--", *command]
            except (OSError, subprocess.TimeoutExpired) as error:
                context["perf_probe"] = {"error": repr(error)}
        solve = measured(command, stdout, stderr,
                         scratch / "solve.time", SOLVE_TIMEOUT)
        if context is not None:
            context["perf_statistics"] = tail(perf_path, 50)
            context["end"] = runtime_context(affinity)
            del os.environ["TACTIX_RUNTIME_SAMPLE_VM"]
        verdict, statistics = solver_summary(stdout)
        result = {
            "schema": "tactix-matched-slurm-memory-regression-result-v2",
            "task": task,
            "rank": int(row["rank"]),
            "arm": row["arm"],
            "tier": row["tier"],
            "instance": row["instance"],
            "expected": row["expected"],
            "verdict": verdict,
            "expected_match": verdict == row["expected"],
            "solve": solve,
            "cpu_affinity": affinity,
            "runtime_context": context,
            "command": list(map(str, command)),
            "node": os.environ.get("SLURMD_NODENAME", os.uname().nodename),
            "binary_sha256": arm["sha256"],
            "memory_enforcement": "slurm_task_cgroup",
            "address_space_limit": None,
            "solver_statistics": statistics,
            "solver_stderr_tail": tail(stderr),
            "certificate_bytes": proof.stat().st_size if proof.is_file() else None,
            "certificate_sha256": sha256(proof) if proof.is_file() else None,
            "proof_check": None,
            "model_check": None,
            "valid": False,
            "cleanup_pending": True,
        }
        # Publish solver timing and verdict before potentially long proof
        # checking.  This is deliberately separate from the final validity
        # result and never satisfies the cumulative correctness gate.
        atomic_json(provisional_path, {
            "schema": "tactix-matched-slurm-provisional-result-v1",
            "task": task,
            "rank": int(row["rank"]),
            "arm": row["arm"],
            "tier": row["tier"],
            "instance": row["instance"],
            "expected": row["expected"],
            "verdict": verdict,
            "expected_match": verdict == row["expected"],
            "solve": solve,
            "node": result["node"],
            "certificate_bytes": result["certificate_bytes"],
            "certificate_sha256": result["certificate_sha256"],
            "solver_statistics": statistics,
            "validation_pending": True,
            "recorded_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        })
        if verdict == "UNSAT" and proof.is_file():
            checker_out = scratch / "checker.stdout"
            checker_err = scratch / "checker.stderr"
            checked = measured([CAMPAIGN / "pins/drat-trim", cnf, proof],
                               checker_out, checker_err,
                               scratch / "checker.time", CHECK_TIMEOUT)
            output = "\n".join(tail(checker_out, 100) + tail(checker_err, 100))
            checked["result"] = (
                "verified" if checked["exit_code"] == 0 and
                "s VERIFIED" in output and "s NOT VERIFIED" not in output
                else "not-verified")
            checked["output_tail"] = output[-12000:]
            result["proof_check"] = checked
            result["valid"] = checked["result"] == "verified"
        elif verdict == "SAT":
            checker_out = scratch / "model.stdout"
            checker_err = scratch / "model.stderr"
            checked = measured([CAMPAIGN / "pins/checkmodel", cnf, stdout],
                               checker_out, checker_err,
                               scratch / "model.time", 600)
            checked["result"] = (
                "verified" if checked["exit_code"] == 0 else "not-verified")
            checked["stdout_tail"] = tail(checker_out)
            checked["stderr_tail"] = tail(checker_err)
            result["model_check"] = checked
            result["valid"] = checked["result"] == "verified"
        result["valid"] = bool(
            result["valid"] and result["expected_match"] and
            not solve["timed_out"] and
            solve["exit_code"] == (10 if verdict == "SAT" else 20))
    except Exception as error:
        result = {
            "schema": "tactix-matched-slurm-memory-regression-result-v2",
            "task": task,
            "rank": int(row["rank"]),
            "arm": row["arm"],
            "tier": row["tier"],
            "instance": row["instance"],
            "expected": row["expected"],
            "verdict": "INFRA_ERROR",
            "valid": False,
            "error": repr(error),
            "node": os.environ.get("SLURMD_NODENAME", os.uname().nodename),
            "memory_enforcement": "slurm_task_cgroup",
            "address_space_limit": None,
            "cleanup_pending": True,
        }
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    result["artifact_deleted"] = not scratch.exists()
    result["derived_artifacts_deleted"] = not scratch.exists()
    result["cleanup_pending"] = scratch.exists()
    result["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    atomic_json(result_path, result)
    return 0 if result["valid"] and result["artifact_deleted"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
