#!/usr/bin/env python3
"""Reproduce the Rank147 checker-profile split on one identical certificate."""

import datetime
import hashlib
import json
import lzma
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


ROOT = Path(__file__).resolve().parent
MANIFEST = json.loads((ROOT / "manifest.json").read_text())
RESULT = ROOT / "result.json"
RETAINED_PROOF = ROOT / "rank147-proof.dsr"


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def run(name, command, expected_exit, timeout=60):
    started = time.monotonic()
    completed = subprocess.run(
        [str(arg) for arg in command], stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, timeout=timeout, check=False)
    output = completed.stdout.decode("utf-8", errors="replace")
    return {
        "name": name,
        "command": [str(arg) for arg in command],
        "exit_code": completed.returncode,
        "expected_exit": expected_exit,
        "exit_matches": completed.returncode == expected_exit,
        "wall_seconds": round(time.monotonic() - started, 3),
        "output_tail": output[-12000:],
    }


def verify_bound_inputs():
    observed = {}
    expected = {MANIFEST["input"]["path"]: MANIFEST["input"]["sha256"]}
    for pin in MANIFEST["pins"].values():
        expected[pin["path"]] = pin["sha256"]
    for item in MANIFEST["prior_evidence"].values():
        expected[item["path"]] = item["sha256"]
    for item in MANIFEST["python_fallback_binding"].values():
        if isinstance(item, dict) and "path" in item:
            expected[item["path"]] = item["sha256"]
    for path, wanted in expected.items():
        got = sha256(path)
        observed[path] = got
        if got != wanted:
            raise RuntimeError(f"hash mismatch for {path}: {got} != {wanted}")
    return observed


def main():
    bound_hashes = verify_bound_inputs()
    pin = lambda name: Path(MANIFEST["pins"][name]["path"])
    with tempfile.TemporaryDirectory(prefix="rank147-checker-profile-") as tmp:
        tmp = Path(tmp)
        cnf = tmp / "rank147.cnf"
        proof = tmp / "rank147.dsr"
        long_lsr = tmp / "long64.lsr"
        canonical_binary_lsr = tmp / "canonical32.binary.lsr"
        canonical_text_lsr = tmp / "canonical32.lsr"

        with lzma.open(MANIFEST["input"]["path"], "rb") as source, cnf.open("wb") as target:
            shutil.copyfileobj(source, target)
        cnf_sha = sha256(cnf)
        if cnf_sha != MANIFEST["expected_checker_input_sha256"]:
            raise RuntimeError(f"decompressed input mismatch: {cnf_sha}")

        solver_command = [pin("tactix"), *MANIFEST["solver_arguments"], cnf, proof]
        solver = run("tactix", solver_command, MANIFEST["expected_solver_exit"])
        proof_sha = sha256(proof)
        proof_bytes = proof.stat().st_size
        if proof_sha != MANIFEST["expected_certificate_sha256"]:
            raise RuntimeError(f"certificate hash mismatch: {proof_sha}")
        if proof_bytes != MANIFEST["expected_certificate_bytes"]:
            raise RuntimeError(f"certificate size mismatch: {proof_bytes}")
        if proof_bytes >= 1_000_000:
            raise RuntimeError(f"refusing to retain unexpectedly large proof: {proof_bytes}")
        shutil.copyfile(proof, RETAINED_PROOF)

        stages = [solver]
        stages.append(run(
            "long64-dsr-trim-negative-control",
            [pin("long64_dsr_trim"), cnf, proof, long_lsr], 1))
        long_output = stages[-1]["output_tail"]
        stages[-1]["checker_reached_proof"] = all(marker in long_output for marker in (
            "Parsed the empty clause", "Checking proof...",
            "[line 111] No UP contradiction for RAT clause 731"))

        stages.append(run(
            "canonical32-dsr-trim",
            [pin("canonical32_dsr_trim"), cnf, proof, canonical_binary_lsr], 0))
        stages[-1]["produced_bytes"] = canonical_binary_lsr.stat().st_size
        stages[-1]["produced_sha256"] = sha256(canonical_binary_lsr)
        stages[-1]["verified_marker"] = "s VERIFIED UNSAT" in stages[-1]["output_tail"]

        stages.append(run(
            "canonical32-dsr-compress",
            [pin("canonical32_dsr_compress"), "-d", canonical_binary_lsr,
             canonical_text_lsr], 0))
        stages[-1]["produced_bytes"] = canonical_text_lsr.stat().st_size
        stages[-1]["produced_sha256"] = sha256(canonical_text_lsr)

        stages.append(run(
            "trestle-srcheck",
            [pin("srcheck"), cnf, canonical_text_lsr], 0))
        stages[-1]["verified_marker"] = "s VERIFIED UNSAT" in stages[-1]["output_tail"]

        passed = (
            all(stage["exit_matches"] for stage in stages)
            and stages[1]["checker_reached_proof"]
            and stages[2]["verified_marker"]
            and stages[4]["verified_marker"])
        result = {
            "schema": "tactix-rank147-checker-profile-result-v1",
            "finished_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "job_id": os.environ.get("SLURM_JOB_ID"),
            "node": os.environ.get("SLURMD_NODENAME") or os.uname().nodename,
            "passed": passed,
            "input_sha256": cnf_sha,
            "certificate_sha256": proof_sha,
            "certificate_bytes": proof_bytes,
            "retained_certificate": str(RETAINED_PROOF),
            "retained_certificate_sha256": sha256(RETAINED_PROOF),
            "bound_input_hashes": bound_hashes,
            "stages": stages,
            "disposition": (
                "identical certificate accepted by frozen canonical32 DSR/Trestle route "
                "and rejected only by current long64 dsr-trim"
                if passed else "diagnostic expectation failed"),
        }
        temporary = RESULT.with_suffix(".json.tmp")
        temporary.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
        temporary.replace(RESULT)
        if not passed:
            raise SystemExit(1)


if __name__ == "__main__":
    main()
