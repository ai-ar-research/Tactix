#!/usr/bin/env python3
"""Run one matched shipped/candidate debug-regression task."""

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
import time


ROOT = Path(__file__).resolve().parents[3]
CAMPAIGN = Path(__file__).resolve().parent
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


def measured(command, stdout, stderr, metrics, timeout):
    wrapped = ["/usr/bin/time", "-f", "max_rss_kb=%M", "-o", str(metrics),
               "--", *map(str, command)]
    started = time.monotonic()
    timed_out = False
    with open(stdout, "wb") as out, open(stderr, "wb") as err:
        process = subprocess.Popen(wrapped, stdout=out, stderr=err,
                                   preexec_fn=process_group)
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
    if len(sys.argv) != 2:
        raise SystemExit("usage: run-one.py TASK")
    task = int(sys.argv[1])
    with open(CAMPAIGN / "matrix.tsv", newline="") as source:
        rows = list(csv.DictReader(source, delimiter="\t"))
    if task < 0 or task >= len(rows) or int(rows[task]["task"]) != task:
        raise SystemExit(f"invalid task {task}")
    row = rows[task]
    result_path = CAMPAIGN / "results" / f"{task:03d}.json"
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
    binary = CAMPAIGN / "pins" / f"tactix-{row['arm']}"
    scratch_root = Path(os.environ.get("SLURM_TMPDIR", tempfile.gettempdir()))
    scratch = Path(tempfile.mkdtemp(prefix=f"cong-admit-{task:03d}-",
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
        solve = measured([binary, cnf, proof], stdout, stderr,
                         scratch / "solve.time", SOLVE_TIMEOUT)
        verdict, statistics = solver_summary(stdout)
        result = {
            "schema": "tactix-matched-slurm-memory-regression-result-v2",
            "task": task,
            "arm": row["arm"],
            "tier": row["tier"],
            "instance": row["instance"],
            "expected": row["expected"],
            "verdict": verdict,
            "expected_match": verdict == row["expected"],
            "solve": solve,
            "node": os.environ.get("SLURMD_NODENAME", os.uname().nodename),
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
        result["valid"] = bool(result["valid"] and result["expected_match"])
    except Exception as error:
        result = {
            "schema": "tactix-matched-slurm-memory-regression-result-v2",
            "task": task,
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
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
