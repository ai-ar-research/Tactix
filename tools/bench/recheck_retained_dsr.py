#!/usr/bin/env python3
"""Try the canonical DSR/Trestle route on one retained failed certificate.

This is a checker-only diagnostic.  It never invokes Tactix and writes a new
sidecar rather than reclassifying the originating runtime result.
"""

import argparse
import csv
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time

import runtime_task as runtime
from proof_oracle import (bridge_trivial_input_binary_lsr,
                          bridge_trivial_input_lsr,
                          canonical_checker_cnf, certificate_route,
                          routed_dsr_stages)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def validate_source_result(campaign, retention, row, task, result):
    require(result.get("task") == task, "source result task does not match matrix")
    require(result.get("rank") == int(row["rank"]),
            "source result rank does not match matrix")
    require(result.get("instance") == row["instance"],
            "source result instance does not match matrix")
    require(result.get("arm") == row["arm"],
            "source result arm does not match matrix")
    require(result.get("verdict") == "UNSAT" and not result.get("valid"),
            "source result is not a failed UNSAT check")
    proof = Path(result.get("retained_artifact", ""))
    expected_proof = campaign / retention["directory"] / f"{task:03d}.dsr"
    require(proof.is_absolute() and proof == expected_proof,
            "retained proof path does not match campaign contract")
    return proof


def validate_checker_spelling(result, spelling):
    source_spelling = (result.get("proof_check") or {}).get(
        "checker_input") or {}
    require(source_spelling.get("original_sha256") ==
            spelling["original_sha256"],
            "source result original-CNF hash mismatch")
    require(source_spelling.get("checker_sha256") ==
            spelling["checker_sha256"],
            "source result canonical checker-CNF hash mismatch")


def run(campaign, task, timeout):
    campaign = campaign.resolve()
    require(os.environ.get("SLURM_JOB_ID"), "rechecking is SLURM-only")
    manifest = json.loads((campaign / "manifest.json").read_text())
    runtime.validate_runtime_pin_contract(campaign, manifest)
    for relative, expected in manifest["pin_sha256"].items():
        require(runtime.sha256(campaign / relative) == expected,
                f"pin hash mismatch: {relative}")
    retention = runtime.failed_proof_retention_contract(manifest)
    require(retention is not None, "campaign has no failed-proof retention contract")
    with (campaign / "matrix.tsv").open(newline="") as source:
        rows = list(csv.DictReader(source, delimiter="\t"))
    require(0 <= task < len(rows) and int(rows[task]["task"]) == task,
            "invalid task")
    row = rows[task]
    result_path = campaign / "results" / f"{task:03d}.json"
    result = json.loads(result_path.read_text())
    proof = validate_source_result(campaign, retention, row, task, result)
    require(proof.is_file(), "retained proof is missing")
    require(proof.stat().st_size == result.get("retained_artifact_bytes"),
            "retained proof size mismatch")
    require(runtime.sha256(proof) == result.get("retained_artifact_sha256"),
            "retained proof hash mismatch")
    route = certificate_route(proof)
    root = campaign.parents[2]
    compressed = root / "benchmarks/cache" / row["instance"]
    require(runtime.sha256(compressed) == manifest["input_sha256"][row["instance"]],
            "compressed input hash mismatch")
    rechecks = campaign / "rechecks"
    rechecks.mkdir(exist_ok=True)
    output_path = rechecks / f"{task:03d}.canonical-dsr.json"
    require(not output_path.exists(), "recheck result already exists")

    scratch = Path(tempfile.mkdtemp(prefix=f"tactix-dsr-recheck-{task:03d}-"))
    record = None
    stages = []
    started = time.monotonic()
    try:
        cnf = scratch / "input.cnf"
        with cnf.open("wb") as target:
            subprocess.run(["xz", "-dc", compressed], stdout=target, check=True)
        require(runtime.sha256(cnf) ==
                (result.get("proof_check") or {}).get("checker_input", {}).get(
                    "original_sha256"), "decompressed input hash mismatch")
        checker_cnf = scratch / "checker.cnf"
        spelling = canonical_checker_cnf(cnf, checker_cnf)
        validate_checker_spelling(result, spelling)
        binary_lsr = scratch / "proof.binary.lsr"
        text_lsr = scratch / "proof.lsr"

        def remaining():
            value = timeout - (time.monotonic() - started)
            if value <= 0:
                raise runtime.ProofPipelineDeadline("checker-only deadline expired")
            return value

        try:
            with runtime.proof_pipeline_deadline(timeout):
                for name, command in routed_dsr_stages(
                        campaign / "pins/dsr-trim",
                        campaign / "pins/dsr-compress",
                        campaign / "pins/srcheck", cnf, proof,
                        binary_lsr, text_lsr, checker_cnf=checker_cnf,
                        witness_free=False):
                    made = command[-1] if name in ("dsr-trim", "dsr-compress") else None
                    stage = runtime.proof_stage(
                        scratch, name, command, remaining(), made)
                    stages.append(stage)
                    if stage["result"] != "verified":
                        break
                    if name == "dsr-trim":
                        (bridge_trivial_input_binary_lsr if made == binary_lsr
                         else bridge_trivial_input_lsr)(cnf, made)
        except runtime.ProofPipelineDeadline:
            stages.append({"name": "pipeline-deadline", "result": "timeout",
                           "exit_code": None, "timed_out": True,
                           "output_tail": "whole checker-only allowance exhausted"})
        verified = (len(stages) >= 3 and
                    all(stage.get("result") == "verified" for stage in stages))
        record = {
            "schema": "tactix-retained-dsr-recheck-v1",
            "task": task,
            "rank": int(row["rank"]),
            "instance": row["instance"],
            "source_result": str(result_path),
            "source_manifest_sha256": runtime.sha256(campaign / "manifest.json"),
            "certificate_bytes": proof.stat().st_size,
            "certificate_sha256": runtime.sha256(proof),
            "audited_original_route": route,
            "diagnostic_route": "canonical-dsr-trim-dsr-compress-trestle",
            "timeout_seconds": timeout,
            "wall_seconds": round(time.monotonic() - started, 3),
            "stages": stages,
            "verified": verified,
            "node": os.environ.get("SLURMD_NODENAME", os.uname().nodename),
        }
    except Exception as error:
        record = {
            "schema": "tactix-retained-dsr-recheck-v1", "task": task,
            "rank": int(row["rank"]), "instance": row["instance"],
            "verified": False, "error": repr(error), "stages": stages,
            "wall_seconds": round(time.monotonic() - started, 3),
            "node": os.environ.get("SLURMD_NODENAME", os.uname().nodename),
        }
    finally:
        import shutil
        shutil.rmtree(scratch, ignore_errors=True)
    runtime.atomic_json(output_path, record)
    return 0 if record["verified"] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign", type=Path)
    parser.add_argument("task", type=int)
    parser.add_argument("--timeout", type=int, default=1800)
    args = parser.parse_args()
    if args.timeout < 1:
        parser.error("--timeout must be positive")
    return run(args.campaign, args.task, args.timeout)


if __name__ == "__main__":
    raise SystemExit(main())
